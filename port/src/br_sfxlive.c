/* br_sfxlive.c -- a continuously running AudioQueue over BrMixRender.
 * See br_sfxlive.h.  This file mixes nothing and decides nothing.
 */
#include "br_sfxlive.h"

#include <AudioToolbox/AudioToolbox.h>

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

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

/* Same mechanism as br_sfxaq.c. */
__asm__(".linker_option \"-framework\", \"AudioToolbox\"");

static AudioQueueRef       s_queue;
static AudioQueueBufferRef s_aBuf[BR_SFXLIVE_BUFFERS];
static int                 s_cBuf;
static BrMix              *s_pMix;
static pthread_mutex_t     s_lock = PTHREAD_MUTEX_INITIALIZER;
static volatile uint64_t   s_frames;
static volatile int        s_running;

static int16_t *s_pTap;
static int      s_cTapCap;
static int      s_cTap;

/* Copy a rendered block into the tap.  Called with s_lock held. */
static void tap_append(const int16_t *pPcm, int cFrames)
{
    int room;

    if (s_pTap == NULL || cFrames <= 0)
        return;
    room = s_cTapCap - s_cTap;
    if (room <= 0)
        return;
    if (cFrames > room)
        cFrames = room;
    memcpy(s_pTap + (size_t)s_cTap * BR_MIX_CHANNELS, pPcm,
           (size_t)cFrames * BR_MIX_CHANNELS * sizeof(int16_t));
    s_cTap += cFrames;
}

static void live_render(int16_t *pDst, int cFrames)
{
    BrMixRender(s_pMix, pDst, cFrames);
    s_frames += (uint64_t)cFrames;
    tap_append(pDst, cFrames);
}

static void live_callback(void *pUser, AudioQueueRef q, AudioQueueBufferRef buf)
{
    (void)pUser;

    pthread_mutex_lock(&s_lock);
    if (!s_running) {
        pthread_mutex_unlock(&s_lock);
        return;
    }
    live_render((int16_t *)buf->mAudioData, BR_SFXLIVE_FRAMES);
    buf->mAudioDataByteSize =
        BR_SFXLIVE_FRAMES * BR_MIX_CHANNELS * (UInt32)sizeof(int16_t);
    pthread_mutex_unlock(&s_lock);

    /* Enqueue outside the lock: AudioQueueEnqueueBuffer may re-enter the
     * callback on some paths, and re-entering a non-recursive mutex from the
     * same thread deadlocks. */
    (void)AudioQueueEnqueueBuffer(q, buf, 0, NULL);
}

static void live_format(AudioStreamBasicDescription *pFmt)
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

int BrSfxLiveStart(BrMix *pMix)
{
    AudioStreamBasicDescription fmt;
    UInt32 cb = BR_SFXLIVE_FRAMES * BR_MIX_CHANNELS * (UInt32)sizeof(int16_t);
    int    i;

    if (s_running)
        return 1;

    s_pMix   = pMix;
    s_frames = 0;
    live_format(&fmt);

    if (br_aq_new_output(&fmt, live_callback, NULL, NULL, NULL, 0, &s_queue)
        != noErr || s_queue == NULL) {
        s_queue = NULL;
        return 0;
    }

    s_running = 1;
    for (i = 0; i < BR_SFXLIVE_BUFFERS; ++i) {
        if (AudioQueueAllocateBuffer(s_queue, cb, &s_aBuf[i]) != noErr)
            break;
        s_cBuf++;
        pthread_mutex_lock(&s_lock);
        live_render((int16_t *)s_aBuf[i]->mAudioData, BR_SFXLIVE_FRAMES);
        s_aBuf[i]->mAudioDataByteSize = cb;
        pthread_mutex_unlock(&s_lock);
        (void)AudioQueueEnqueueBuffer(s_queue, s_aBuf[i], 0, NULL);
    }
    if (s_cBuf == 0 || AudioQueueStart(s_queue, NULL) != noErr) {
        s_running = 0;
        AudioQueueDispose(s_queue, true);
        s_queue = NULL;
        s_cBuf  = 0;
        return 0;
    }
    return 1;
}

void BrSfxLiveStop(void)
{
    if (s_queue == NULL) {
        s_running = 0;
        return;
    }
    pthread_mutex_lock(&s_lock);
    s_running = 0;
    pthread_mutex_unlock(&s_lock);

    AudioQueueStop(s_queue, true);
    AudioQueueDispose(s_queue, true);
    s_queue = NULL;
    s_cBuf  = 0;
    s_pMix  = NULL;
}

int      BrSfxLiveRunning(void)        { return s_running; }
uint64_t BrSfxLiveFramesRendered(void) { return s_frames; }

void BrSfxLiveLock(void)   { pthread_mutex_lock(&s_lock); }
void BrSfxLiveUnlock(void) { pthread_mutex_unlock(&s_lock); }

/* ---------------------------------------------------------------- the tap */

int BrSfxLiveTapBegin(int cFrames)
{
    int16_t *p;

    if (cFrames <= 0)
        return 0;
    p = (int16_t *)calloc((size_t)cFrames * BR_MIX_CHANNELS, sizeof(int16_t));
    if (p == NULL)
        return 0;

    pthread_mutex_lock(&s_lock);
    free(s_pTap);
    s_pTap    = p;
    s_cTapCap = cFrames;
    s_cTap    = 0;
    pthread_mutex_unlock(&s_lock);
    return 1;
}

int BrSfxLiveTapFrames(void)
{
    int n;

    pthread_mutex_lock(&s_lock);
    n = s_cTap;
    pthread_mutex_unlock(&s_lock);
    return n;
}

int BrSfxLiveTapPeak(void)
{
    int i, n, peak = 0;

    pthread_mutex_lock(&s_lock);
    n = s_cTap * BR_MIX_CHANNELS;
    for (i = 0; i < n; ++i) {
        int v = s_pTap[i];
        if (v < 0)
            v = -v;
        if (v > peak)
            peak = v;
    }
    pthread_mutex_unlock(&s_lock);
    return peak;
}

int BrSfxLiveTapWrite(const char *pszPath)
{
    BrMixWaveFormat fmt;
    int             rc;

    pthread_mutex_lock(&s_lock);
    if (s_pTap == NULL || s_cTap <= 0) {
        pthread_mutex_unlock(&s_lock);
        return -1;
    }
    BrMixOutputFormat(&fmt);
    rc = BrMixWavWrite(pszPath, &fmt, s_pTap,
                       (uint32_t)s_cTap * BR_MIX_CHANNELS
                           * (uint32_t)sizeof(int16_t));
    pthread_mutex_unlock(&s_lock);
    return rc;
}

void BrSfxLiveTapEnd(void)
{
    pthread_mutex_lock(&s_lock);
    free(s_pTap);
    s_pTap    = NULL;
    s_cTapCap = 0;
    s_cTap    = 0;
    pthread_mutex_unlock(&s_lock);
}

/* --------------------------------------------------------------- offline */

int BrSfxLivePumpOffline(int cFrames)
{
    int16_t buf[BR_SFXLIVE_FRAMES * BR_MIX_CHANNELS];
    int     done = 0;

    if (cFrames <= 0)
        return 0;
    while (done < cFrames) {
        int n = cFrames - done;
        if (n > BR_SFXLIVE_FRAMES)
            n = BR_SFXLIVE_FRAMES;
        pthread_mutex_lock(&s_lock);
        live_render(buf, n);
        pthread_mutex_unlock(&s_lock);
        done += n;
    }
    return done;
}

void BrSfxLiveSetMix(BrMix *pMix)
{
    pthread_mutex_lock(&s_lock);
    s_pMix = pMix;
    pthread_mutex_unlock(&s_lock);
}
