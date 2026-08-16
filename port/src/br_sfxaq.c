/* br_sfxaq.c -- AudioQueue output.  See br_sfxaq.h.
 *
 * This is the only place in the port that opens a sound device, and it is
 * deliberately the dumbest file in the subsystem: it copies finished samples
 * into system buffers.  Anything cleverer belongs upstream, in br_mix.c or
 * further up still in the ported logic that drives it.
 */
#include "br_sfxaq.h"
#include "br_mix.h"

#include <AudioToolbox/AudioToolbox.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* THE ONE PLACE A SPEAKER CAN BE REACHED.
 *
 * Every AudioQueueNewOutput in this file goes through here. The gate started
 * out at the caller level and that was not good enough: the harness kept
 * emitting beeps from headless runs, because a caller-level flag only covers
 * the callers you remembered. There are three files that can open a device and
 * several entry points across them, and the user heard the ones that were
 * missed before anyone found them in the code.
 *
 * So the check sits at the syscall, not at the policy. Audible output is
 * opt-in via BR_AUDIO_LIVE; everything else mixes exactly as before and the
 * .wav evidence files are byte-identical, because the mixing was never the
 * part that made noise.
 */
static int br_aq_live_allowed(void)
{
    const char *p = getenv("BR_AUDIO_LIVE");
    return (p != NULL && *p != '\0' && *p != '0');
}

static OSStatus br_aq_new_output(const AudioStreamBasicDescription *pFmt,
                                 AudioQueueOutputCallback pfnCb,
                                 void *pUser, CFRunLoopRef rl,
                                 CFStringRef mode, UInt32 flags,
                                 AudioQueueRef *pOut)
{
    if (!br_aq_live_allowed()) {
        if (pOut) *pOut = NULL;
        return (OSStatus)-1;          /* same shape as a device failure */
    }
    return AudioQueueNewOutput(pFmt, pfnCb, pUser, rl, mode, flags, pOut);
}

/* Link AudioToolbox without a command-line flag.  build.sh discovers modules
 * and must not have to know that one of them wants a framework; ld64 honours
 * LC_LINKER_OPTION records, and this is how you emit one from C. */
__asm__(".linker_option \"-framework\", \"AudioToolbox\"");

#define BR_AQ_BUFFERS   3
#define BR_AQ_FRAMES    4096

typedef struct BrAqState {
    const int16_t *pPcm;
    int            cFrames;
    volatile int   iRead;        /* frames handed to the queue so far      */
    volatile int   cQueued;      /* buffers still in flight                */
} BrAqState;

/* Called on the queue's own thread.  Refills one buffer, or enqueues a silent
 * short buffer once the source is exhausted so the queue drains cleanly. */
static void aq_callback(void *pUser, AudioQueueRef q, AudioQueueBufferRef buf)
{
    BrAqState *pS   = (BrAqState *)pUser;
    int        want = BR_AQ_FRAMES;
    int        left = pS->cFrames - pS->iRead;
    UInt32     cb;

    if (left <= 0) {
        pS->cQueued--;
        return;                 /* not re-enqueued: the queue runs dry     */
    }
    if (want > left)
        want = left;

    cb = (UInt32)want * BR_MIX_CHANNELS * (UInt32)sizeof(int16_t);
    memcpy(buf->mAudioData,
           pS->pPcm + (size_t)pS->iRead * BR_MIX_CHANNELS, cb);
    buf->mAudioDataByteSize = cb;
    pS->iRead += want;

    if (AudioQueueEnqueueBuffer(q, buf, 0, NULL) != noErr)
        pS->cQueued--;
}

static void aq_format(AudioStreamBasicDescription *pFmt)
{
    memset(pFmt, 0, sizeof(*pFmt));
    pFmt->mSampleRate       = (Float64)BR_MIX_RATE;
    pFmt->mFormatID         = kAudioFormatLinearPCM;
    pFmt->mFormatFlags      = kAudioFormatFlagIsSignedInteger
                            | kAudioFormatFlagIsPacked;
    pFmt->mBitsPerChannel   = BR_MIX_BITS;
    pFmt->mChannelsPerFrame = BR_MIX_CHANNELS;
    pFmt->mFramesPerPacket  = 1;
    pFmt->mBytesPerFrame    = BR_MIX_CHANNELS * (BR_MIX_BITS / 8);
    pFmt->mBytesPerPacket   = pFmt->mBytesPerFrame;
}

int BrSfxAqAvailable(void)
{
    AudioStreamBasicDescription fmt;
    AudioQueueRef               q = NULL;
    BrAqState                   st;

    memset(&st, 0, sizeof(st));
    aq_format(&fmt);
    if (br_aq_new_output(&fmt, aq_callback, &st, NULL, NULL, 0, &q) != noErr
        || q == NULL)
        return 0;
    AudioQueueDispose(q, true);
    return 1;
}

int BrSfxAqPlay(void *pUser, const int16_t *pPcm, int cFrames)
{
    AudioStreamBasicDescription fmt;
    AudioQueueRef               q = NULL;
    AudioQueueBufferRef         aBuf[BR_AQ_BUFFERS];
    BrAqState                   st;
    int                         i, spins;

    (void)pUser;
    if (pPcm == NULL || cFrames <= 0)
        return 0;

    memset(&st, 0, sizeof(st));
    st.pPcm    = pPcm;
    st.cFrames = cFrames;

    aq_format(&fmt);
    if (br_aq_new_output(&fmt, aq_callback, &st, NULL, NULL, 0, &q) != noErr
        || q == NULL) {
        fprintf(stderr, "sfx: no audio output device -- rendering only\n");
        return 0;
    }

    /* Prime: the callback both fills and enqueues, so priming is just calling
     * it once per buffer. */
    for (i = 0; i < BR_AQ_BUFFERS; ++i) {
        UInt32 cb = BR_AQ_FRAMES * BR_MIX_CHANNELS * (UInt32)sizeof(int16_t);
        aBuf[i] = NULL;
        if (AudioQueueAllocateBuffer(q, cb, &aBuf[i]) != noErr || aBuf[i] == NULL)
            continue;
        st.cQueued++;
        aq_callback(&st, q, aBuf[i]);
    }
    if (st.cQueued == 0) {
        AudioQueueDispose(q, true);
        return 0;
    }

    if (AudioQueueStart(q, NULL) != noErr) {
        AudioQueueDispose(q, true);
        return 0;
    }

    /* Wait for the source to drain, with a ceiling of twice its own duration
     * so a wedged device cannot hang the harness. */
    spins = (cFrames / (BR_MIX_RATE / 100)) * 2 + 200;
    while (st.cQueued > 0 && spins-- > 0)
        usleep(10000);

    AudioQueueStop(q, true);
    AudioQueueDispose(q, true);
    return 1;
}
