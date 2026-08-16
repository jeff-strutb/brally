/* br_musicaq.c -- BrAudioBackend over ExtAudioFile + AudioQueue.
 * See br_musicaq.h for why this file exists and what it deliberately leaves
 * to br_audio.c.
 */
#include "br_musicaq.h"

#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CoreFoundation.h>

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

/* Same mechanism as br_sfxaq.c: only a binary that links this object acquires
 * the framework dependency, and build.sh never learns about it. */
__asm__(".linker_option \"-framework\", \"AudioToolbox\"");
__asm__(".linker_option \"-framework\", \"CoreFoundation\"");

#define BR_MQ_BUFFERS   3
#define BR_MQ_FRAMES    8192

typedef struct BrMqTrack {
    int                 used;
    ExtAudioFileRef     file;
    AudioQueueRef       queue;
    AudioQueueBufferRef aBuf[BR_MQ_BUFFERS];
    int                 cBuf;

    double              rate;        /* the FILE's rate, not BR_MIX_RATE   */
    int                 channels;    /* always 2 after the client format   */
    int                 loop;
    int                 state;       /* BrAudioState                       */

    volatile int        eof;         /* set by the queue thread            */
    volatile int        finished;    /* raised once, cleared by the pump   */
    volatile int        inFlight;    /* buffers the device still holds     */
    volatile uint64_t   framesOut;   /* frames handed to the device        */
} BrMqTrack;

static BrMqTrack       s_aTrack[BR_MUSICAQ_HANDLES];
static pthread_mutex_t s_lock = PTHREAD_MUTEX_INITIALIZER;
static uint64_t        s_framesTotal;
static OSStatus        s_lastErr;
static const char     *s_lastWhere;

static void mq_fail(const char *pszWhere, OSStatus st)
{
    if (st != noErr) {
        s_lastErr   = st;
        s_lastWhere = pszWhere;
    }
}

/* handle 0 is BR_AUDIO_HANDLE_NONE, so handles are 1-based. */
static BrMqTrack *mq_track(int handle)
{
    if (handle < 1 || handle > BR_MUSICAQ_HANDLES)
        return NULL;
    if (!s_aTrack[handle - 1].used)
        return NULL;
    return &s_aTrack[handle - 1];
}

/* ------------------------------------------------------------------------
 * the queue thread
 * --------------------------------------------------------------------- */

/* Fill one buffer from the file.  Returns the number of frames written, and
 * 0 at end of stream (after a rewind if the track loops).
 *
 * Called ONLY from the queue callback and from priming, both of which hold
 * s_lock, so ExtAudioFileRead is never re-entered. */
static UInt32 mq_fill(BrMqTrack *pT, AudioQueueBufferRef buf)
{
    AudioBufferList abl;
    UInt32          want = BR_MQ_FRAMES;
    OSStatus        st;

    abl.mNumberBuffers              = 1;
    abl.mBuffers[0].mNumberChannels = (UInt32)pT->channels;
    abl.mBuffers[0].mDataByteSize   = buf->mAudioDataBytesCapacity;
    abl.mBuffers[0].mData           = buf->mAudioData;

    st = ExtAudioFileRead(pT->file, &want, &abl);
    if (st != noErr) {
        mq_fail("ExtAudioFileRead", st);
        return 0;
    }
    if (want == 0 && pT->loop) {
        /* Seek and try once more.  Once, not in a loop: a zero-length file
         * would otherwise spin the audio thread for ever. */
        if (ExtAudioFileSeek(pT->file, 0) == noErr) {
            want                          = BR_MQ_FRAMES;
            abl.mBuffers[0].mDataByteSize = buf->mAudioDataBytesCapacity;
            if (ExtAudioFileRead(pT->file, &want, &abl) != noErr)
                want = 0;
        }
    }
    buf->mAudioDataByteSize =
        want * (UInt32)pT->channels * (UInt32)sizeof(int16_t);
    return want;
}

static void mq_callback(void *pUser, AudioQueueRef q, AudioQueueBufferRef buf)
{
    BrMqTrack *pT = (BrMqTrack *)pUser;
    UInt32     got;

    pthread_mutex_lock(&s_lock);
    if (!pT->used || pT->eof) {
        pT->inFlight--;
        pthread_mutex_unlock(&s_lock);
        return;
    }
    got = mq_fill(pT, buf);
    if (got == 0) {
        /* End of stream on a non-looping track.  Do NOT call into br_audio.c
         * here -- raise the flag and let BrMusicAqPump do it on the game
         * thread.  See br_musicaq.h. */
        pT->eof      = 1;
        pT->finished = 1;
        pT->inFlight--;
        pthread_mutex_unlock(&s_lock);
        return;
    }
    pT->framesOut += got;
    s_framesTotal += got;
    if (AudioQueueEnqueueBuffer(q, buf, 0, NULL) != noErr)
        pT->inFlight--;
    pthread_mutex_unlock(&s_lock);
}

/* ------------------------------------------------------------------------
 * the backend
 * --------------------------------------------------------------------- */

static void mq_teardown(BrMqTrack *pT)
{
    int i;

    if (pT->queue != NULL) {
        AudioQueueStop(pT->queue, true);
        for (i = 0; i < pT->cBuf; ++i)
            if (pT->aBuf[i] != NULL)
                AudioQueueFreeBuffer(pT->queue, pT->aBuf[i]);
        AudioQueueDispose(pT->queue, true);
        pT->queue = NULL;
    }
    pT->cBuf = 0;
    if (pT->file != NULL) {
        ExtAudioFileDispose(pT->file);
        pT->file = NULL;
    }
}

static int mq_open(void *pUser, const char *pszPath)
{
    AudioStreamBasicDescription fileFmt, cliFmt;
    CFStringRef                 cs;
    CFURLRef                    url;
    ExtAudioFileRef             f  = NULL;
    BrMqTrack                  *pT = NULL;
    UInt32                      cb;
    OSStatus                    st;
    int                         i, handle = BR_AUDIO_HANDLE_NONE;

    (void)pUser;
    if (pszPath == NULL || pszPath[0] == '\0')
        return BR_AUDIO_HANDLE_NONE;

    cs = CFStringCreateWithCString(NULL, pszPath, kCFStringEncodingUTF8);
    if (cs == NULL)
        return BR_AUDIO_HANDLE_NONE;
    url = CFURLCreateWithFileSystemPath(NULL, cs, kCFURLPOSIXPathStyle, false);
    CFRelease(cs);
    if (url == NULL)
        return BR_AUDIO_HANDLE_NONE;

    st = ExtAudioFileOpenURL(url, &f);
    CFRelease(url);
    if (st != noErr || f == NULL) {
        mq_fail("ExtAudioFileOpenURL", st);
        return BR_AUDIO_HANDLE_NONE;
    }

    cb = (UInt32)sizeof(fileFmt);
    memset(&fileFmt, 0, sizeof(fileFmt));
    st = ExtAudioFileGetProperty(f, kExtAudioFileProperty_FileDataFormat,
                                 &cb, &fileFmt);
    if (st != noErr || fileFmt.mSampleRate <= 0.0) {
        mq_fail("kExtAudioFileProperty_FileDataFormat", st);
        ExtAudioFileDispose(f);
        return BR_AUDIO_HANDLE_NONE;
    }

    /* The client format: 16-bit interleaved stereo at the FILE's own rate.
     * Asking for a fixed rate here would resample every track, and a
     * 44100 Hz CD rip does not want that. */
    memset(&cliFmt, 0, sizeof(cliFmt));
    cliFmt.mSampleRate       = fileFmt.mSampleRate;
    cliFmt.mFormatID         = kAudioFormatLinearPCM;
    cliFmt.mFormatFlags      = kAudioFormatFlagIsSignedInteger
                             | kAudioFormatFlagIsPacked;
    cliFmt.mBitsPerChannel   = 16;
    cliFmt.mChannelsPerFrame = 2;
    cliFmt.mFramesPerPacket  = 1;
    cliFmt.mBytesPerFrame    = 2 * 2;
    cliFmt.mBytesPerPacket   = cliFmt.mBytesPerFrame;

    st = ExtAudioFileSetProperty(f, kExtAudioFileProperty_ClientDataFormat,
                                 (UInt32)sizeof(cliFmt), &cliFmt);
    if (st != noErr) {
        mq_fail("kExtAudioFileProperty_ClientDataFormat", st);
        ExtAudioFileDispose(f);
        return BR_AUDIO_HANDLE_NONE;
    }

    pthread_mutex_lock(&s_lock);
    for (i = 0; i < BR_MUSICAQ_HANDLES; ++i) {
        if (!s_aTrack[i].used) {
            pT     = &s_aTrack[i];
            handle = i + 1;
            break;
        }
    }
    if (pT == NULL) {
        pthread_mutex_unlock(&s_lock);
        ExtAudioFileDispose(f);
        return BR_AUDIO_HANDLE_NONE;
    }

    memset(pT, 0, sizeof(*pT));
    pT->used     = 1;
    pT->file     = f;
    pT->rate     = cliFmt.mSampleRate;
    pT->channels = 2;
    pT->state    = BR_AUDIO_STOPPED;
    pthread_mutex_unlock(&s_lock);

    return handle;
}

static void mq_close(void *pUser, int handle)
{
    BrMqTrack *pT;

    (void)pUser;
    pthread_mutex_lock(&s_lock);
    pT = mq_track(handle);
    if (pT != NULL) {
        mq_teardown(pT);
        pT->used = 0;
    }
    pthread_mutex_unlock(&s_lock);
}

static int mq_start(void *pUser, int handle)
{
    AudioStreamBasicDescription fmt;
    BrMqTrack                  *pT;
    OSStatus                    st;
    int                         i, primed = 0;

    (void)pUser;
    pthread_mutex_lock(&s_lock);
    pT = mq_track(handle);
    if (pT == NULL) {
        pthread_mutex_unlock(&s_lock);
        return 0;
    }

    /* Restart from the beginning: BrAudioPlayTrack's contract. */
    if (pT->queue != NULL) {
        AudioQueueStop(pT->queue, true);
        AudioQueueDispose(pT->queue, true);
        pT->queue = NULL;
        pT->cBuf  = 0;
    }
    (void)ExtAudioFileSeek(pT->file, 0);
    pT->eof       = 0;
    pT->finished  = 0;
    pT->inFlight  = 0;

    memset(&fmt, 0, sizeof(fmt));
    fmt.mSampleRate       = pT->rate;
    fmt.mFormatID         = kAudioFormatLinearPCM;
    fmt.mFormatFlags      = kAudioFormatFlagIsSignedInteger
                          | kAudioFormatFlagIsPacked;
    fmt.mBitsPerChannel   = 16;
    fmt.mChannelsPerFrame = (UInt32)pT->channels;
    fmt.mFramesPerPacket  = 1;
    fmt.mBytesPerFrame    = (UInt32)pT->channels * 2;
    fmt.mBytesPerPacket   = fmt.mBytesPerFrame;

    st = br_aq_new_output(&fmt, mq_callback, pT, NULL, NULL, 0, &pT->queue);
    if (st != noErr || pT->queue == NULL) {
        mq_fail("AudioQueueNewOutput", st);
        pT->queue = NULL;
        pthread_mutex_unlock(&s_lock);
        return 0;
    }

    for (i = 0; i < BR_MQ_BUFFERS; ++i) {
        UInt32 cb = BR_MQ_FRAMES * (UInt32)pT->channels * (UInt32)sizeof(int16_t);
        UInt32 got;

        if (AudioQueueAllocateBuffer(pT->queue, cb, &pT->aBuf[i]) != noErr)
            break;
        pT->cBuf++;
        got = mq_fill(pT, pT->aBuf[i]);
        if (got == 0)
            break;
        pT->framesOut += got;
        s_framesTotal += got;
        if (AudioQueueEnqueueBuffer(pT->queue, pT->aBuf[i], 0, NULL) != noErr)
            break;
        pT->inFlight++;
        primed++;
    }
    if (primed == 0) {
        mq_teardown(pT);
        pthread_mutex_unlock(&s_lock);
        return 0;
    }

    st = AudioQueueStart(pT->queue, NULL);
    if (st != noErr) {
        mq_fail("AudioQueueStart", st);
        mq_teardown(pT);
        pthread_mutex_unlock(&s_lock);
        return 0;
    }
    pT->state = BR_AUDIO_PLAYING;
    pthread_mutex_unlock(&s_lock);
    return 1;
}

static void mq_stop(void *pUser, int handle)
{
    BrMqTrack *pT;

    (void)pUser;
    pthread_mutex_lock(&s_lock);
    pT = mq_track(handle);
    if (pT != NULL && pT->queue != NULL) {
        AudioQueueStop(pT->queue, true);
        pT->state    = BR_AUDIO_STOPPED;
        pT->inFlight = 0;
    }
    pthread_mutex_unlock(&s_lock);
}

static void mq_pause(void *pUser, int handle)
{
    BrMqTrack *pT;

    (void)pUser;
    pthread_mutex_lock(&s_lock);
    pT = mq_track(handle);
    if (pT != NULL && pT->queue != NULL && pT->state == BR_AUDIO_PLAYING) {
        AudioQueuePause(pT->queue);
        pT->state = BR_AUDIO_PAUSED;
    }
    pthread_mutex_unlock(&s_lock);
}

static void mq_resume(void *pUser, int handle)
{
    BrMqTrack *pT;

    (void)pUser;
    pthread_mutex_lock(&s_lock);
    pT = mq_track(handle);
    if (pT != NULL && pT->queue != NULL && pT->state == BR_AUDIO_PAUSED) {
        if (AudioQueueStart(pT->queue, NULL) == noErr)
            pT->state = BR_AUDIO_PLAYING;
    }
    pthread_mutex_unlock(&s_lock);
}

static void mq_set_volume(void *pUser, int handle, int volume)
{
    BrMqTrack *pT;

    (void)pUser;
    if (volume < 0)                   volume = 0;
    if (volume > BR_AUDIO_VOLUME_MAX) volume = BR_AUDIO_VOLUME_MAX;

    pthread_mutex_lock(&s_lock);
    pT = mq_track(handle);
    if (pT != NULL && pT->queue != NULL)
        AudioQueueSetParameter(pT->queue, kAudioQueueParam_Volume,
                               (AudioQueueParameterValue)volume
                               / (AudioQueueParameterValue)BR_AUDIO_VOLUME_MAX);
    pthread_mutex_unlock(&s_lock);
}

static void mq_set_pan(void *pUser, int handle, int pan)
{
    BrMqTrack *pT;

    (void)pUser;
    if (pan < 0)   pan = 0;
    if (pan > 255) pan = 255;

    pthread_mutex_lock(&s_lock);
    pT = mq_track(handle);
    if (pT != NULL && pT->queue != NULL)
        AudioQueueSetParameter(pT->queue, kAudioQueueParam_Pan,
                               (AudioQueueParameterValue)(pan - BR_AUDIO_PAN_CENTRE)
                               / (AudioQueueParameterValue)BR_AUDIO_PAN_CENTRE);
    pthread_mutex_unlock(&s_lock);
}

static void mq_set_loop(void *pUser, int handle, int loop)
{
    BrMqTrack *pT;

    (void)pUser;
    pthread_mutex_lock(&s_lock);
    pT = mq_track(handle);
    if (pT != NULL)
        pT->loop = (loop != 0);
    pthread_mutex_unlock(&s_lock);
}

static int mq_get_state(void *pUser, int handle)
{
    BrMqTrack *pT;
    int        state = BR_AUDIO_STOPPED;

    (void)pUser;
    pthread_mutex_lock(&s_lock);
    pT = mq_track(handle);
    if (pT != NULL)
        state = pT->eof ? BR_AUDIO_STOPPED : pT->state;
    pthread_mutex_unlock(&s_lock);
    return state;
}

static int mq_get_position_ms(void *pUser, int handle, uint32_t *pMs)
{
    BrMqTrack     *pT;
    AudioTimeStamp ts;
    int            ok = 0;

    (void)pUser;
    if (pMs == NULL)
        return 0;
    pthread_mutex_lock(&s_lock);
    pT = mq_track(handle);
    if (pT != NULL && pT->queue != NULL) {
        memset(&ts, 0, sizeof(ts));
        if (AudioQueueGetCurrentTime(pT->queue, NULL, &ts, NULL) == noErr
            && (ts.mFlags & kAudioTimeStampSampleTimeValid) != 0
            && pT->rate > 0.0) {
            *pMs = (uint32_t)(ts.mSampleTime * 1000.0 / pT->rate);
            ok   = 1;
        }
    }
    pthread_mutex_unlock(&s_lock);
    return ok;
}

static const BrAudioBackend s_backend = {
    NULL,
    mq_open, mq_close,
    mq_start, mq_stop,
    mq_pause, mq_resume,
    mq_set_volume, mq_set_pan, mq_set_loop,
    mq_get_state, mq_get_position_ms
};

const BrAudioBackend *BrMusicAqBackend(void)
{
    return &s_backend;
}

int BrMusicAqAvailable(void)
{
    AudioStreamBasicDescription fmt;
    AudioQueueRef               q = NULL;

    memset(&fmt, 0, sizeof(fmt));
    fmt.mSampleRate       = 44100.0;
    fmt.mFormatID         = kAudioFormatLinearPCM;
    fmt.mFormatFlags      = kAudioFormatFlagIsSignedInteger
                          | kAudioFormatFlagIsPacked;
    fmt.mBitsPerChannel   = 16;
    fmt.mChannelsPerFrame = 2;
    fmt.mFramesPerPacket  = 1;
    fmt.mBytesPerFrame    = 4;
    fmt.mBytesPerPacket   = 4;

    if (br_aq_new_output(&fmt, mq_callback, &s_aTrack[0], NULL, NULL, 0, &q)
        != noErr || q == NULL)
        return 0;
    AudioQueueDispose(q, true);
    return 1;
}

void BrMusicAqPump(BrAudio *pAudio)
{
    int i, nDone = 0;

    pthread_mutex_lock(&s_lock);
    for (i = 0; i < BR_MUSICAQ_HANDLES; ++i) {
        if (s_aTrack[i].used && s_aTrack[i].finished) {
            s_aTrack[i].finished = 0;
            s_aTrack[i].state    = BR_AUDIO_STOPPED;
            nDone++;
        }
    }
    pthread_mutex_unlock(&s_lock);

    /* Outside the lock: BrAudioOnTrackFinished calls back into pfnStart. */
    while (nDone-- > 0 && pAudio != NULL)
        BrAudioOnTrackFinished(pAudio);
}

void BrMusicAqShutdown(void)
{
    int i;

    pthread_mutex_lock(&s_lock);
    for (i = 0; i < BR_MUSICAQ_HANDLES; ++i) {
        if (s_aTrack[i].used) {
            mq_teardown(&s_aTrack[i]);
            s_aTrack[i].used = 0;
        }
    }
    pthread_mutex_unlock(&s_lock);
}

uint64_t BrMusicAqFramesPlayed(void)
{
    uint64_t n;

    pthread_mutex_lock(&s_lock);
    n = s_framesTotal;
    pthread_mutex_unlock(&s_lock);
    return n;
}

int         BrMusicAqLastError(void)      { return (int)s_lastErr; }
const char *BrMusicAqLastErrorWhere(void) { return s_lastWhere; }

void BrMusicAqClearError(void)
{
    s_lastErr   = noErr;
    s_lastWhere = NULL;
}
