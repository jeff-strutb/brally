/* sound.m -- the game's sound effects, played natively.
 *
 * The game plays its effects through DirectSound: one static buffer per
 * sample (the front end's clicks and speech, the cars' engines, the
 * crashes, the co-driver), driven with volume, pan, pitch, play and stop.
 * host_dx.c keeps those buffers and mixes the ones playing
 * (hdx_sfx_render); this file sends the mix to the output device through
 * AVAudioEngine, rendered at the device's rate so nothing downstream
 * converts it, into Apple's peak limiter (the sum of a race's voices can
 * pass full scale, where DirectSound would have clipped). The music plays
 * beside it on its own engine (music.m).
 *
 * As on Windows, where the game's buffers do not ask for global focus
 * (DSBCAPS_GLOBALFOCUS), the effects are muted while the app is not the
 * active one; they play on underneath, so they come back where they have
 * got to.
 *
 * Silent (the buffers keep the clock's time, as the oracle models them)
 * when headless or with BR_SFX=0. BR_SFXWAV=path records the effects to a
 * WAV file: what goes to the output when there is a window; headless, the
 * same graph rendered offline (AVAudioEngine's manual rendering) in step
 * with the game's clock, which checks the effects without a window and
 * without a sound. BR_SFXQUIET records a windowed run without playing it.
 *
 * The samples load through the game's own WAV reader; BrWaveSeekData is
 * replaced below (it relied on MSVC's eax).
 */
#import <AVFoundation/AVFoundation.h>
#import <Cocoa/Cocoa.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include "w2c_native.h"
#include "host.h"

u32 h_mmioSeek(u32 h, u32 off, u32 org);
u32 h_mmioDescend(u32 h, u32 ck, u32 parent, u32 fl);
u32 h_timeGetTime(void);
NSWindow *happ_window(void);

#define OFFLINE_RATE 48000.0
#define OFFLINE_BLOCK 512u

static struct {
    int init, on, offline;
    double rate;
    atomic_int muted;
    AVAudioEngine *engine;
    AVAudioSourceNode *src;
    AVAudioUnitEffect *limiter;
    AVAudioPCMBuffer *block;          /* offline: one rendered block */
    FILE *wav;                        /* offline: the recording */
    u32 t0;
    u64 frames;
} S;

/* a float32 stereo WAV, its sizes written as it grows */
static void wav_header(FILE *f, u64 frames)
{
    u32 data = (u32)(frames * 8), v;
    u8 h[44];
    memcpy(h, "RIFF", 4); v = 36 + data; memcpy(h + 4, &v, 4);
    memcpy(h + 8, "WAVEfmt ", 8); v = 16; memcpy(h + 16, &v, 4);
    h[20] = 3; h[21] = 0; h[22] = 2; h[23] = 0;                /* IEEE float, 2 channels */
    v = (u32)OFFLINE_RATE; memcpy(h + 24, &v, 4);
    v = (u32)OFFLINE_RATE * 8; memcpy(h + 28, &v, 4);
    h[32] = 8; h[33] = 0; h[34] = 32; h[35] = 0;
    memcpy(h + 36, "data", 4); memcpy(h + 40, &data, 4);
    fseek(f, 0, SEEK_SET);
    fwrite(h, 1, 44, f);
    fseek(f, 0, SEEK_END);
}

static void wav_close(void)
{
    if (!S.wav) return;
    wav_header(S.wav, S.frames);
    fclose(S.wav);
    S.wav = NULL;
}

/* the graph, the same for the device and for a recording: the mix, the
 * limiter, the engine's output */
static void graph(AVAudioFormat *fmt)
{
    AudioComponentDescription d = { kAudioUnitType_Effect, kAudioUnitSubType_PeakLimiter,
                                    kAudioUnitManufacturer_Apple, 0, 0 };
    S.src = [[AVAudioSourceNode alloc] initWithFormat:fmt
        renderBlock:^OSStatus(BOOL *silence, const AudioTimeStamp *ts, AVAudioFrameCount n, AudioBufferList *abl) {
            float *l = abl->mBuffers[0].mData, *r = abl->mNumberBuffers > 1 ? abl->mBuffers[1].mData : NULL;
            (void)ts;
            if (!r) {                                  /* not the standard format: never so far */
                memset(l, 0, n * sizeof *l);
                *silence = YES;
                return noErr;
            }
            hdx_sfx_render(l, r, n, S.rate);
            if (atomic_load(&S.muted)) {
                memset(l, 0, n * sizeof *l);
                memset(r, 0, n * sizeof *r);
                *silence = YES;
            }
            return noErr;
        }];
    S.limiter = [[AVAudioUnitEffect alloc] initWithAudioComponentDescription:d];
    [S.engine attachNode:S.src];
    [S.engine attachNode:S.limiter];
    [S.engine connect:S.src to:S.limiter format:fmt];
    [S.engine connect:S.limiter to:S.engine.mainMixerNode format:fmt];
}

int nsound_open(void)
{
    const char *e = getenv("BR_SFX"), *w = getenv("BR_SFXWAV");
    AVAudioFormat *fmt;
    NSError *err = nil;
    if (S.init) return S.on;
    S.init = 1;
    if (e && !atoi(e)) return 0;
    S.offline = !happ_window();
    if (S.offline && !w) return 0;
    S.engine = [AVAudioEngine new];
    S.rate = S.offline ? OFFLINE_RATE : [S.engine.outputNode outputFormatForBus:0].sampleRate;
    if (S.rate <= 0) S.rate = OFFLINE_RATE;
    fmt = [[AVAudioFormat alloc] initStandardFormatWithSampleRate:S.rate channels:2];
    if (S.offline) {
        if (![S.engine enableManualRenderingMode:AVAudioEngineManualRenderingModeOffline format:fmt
                               maximumFrameCount:OFFLINE_BLOCK error:&err] || !(S.wav = fopen(w, "wb"))) {
            fprintf(stderr, "sound: cannot record to %s (%s)\n", w, err.localizedDescription.UTF8String);
            S.engine = nil;
            return 0;
        }
        S.block = [[AVAudioPCMBuffer alloc] initWithPCMFormat:fmt frameCapacity:OFFLINE_BLOCK];
        wav_header(S.wav, 0);
        atexit(wav_close);
    }
    graph(fmt);
    if (!S.offline && w) {
        static AVAudioFile *rec;
        [S.limiter installTapOnBus:0 bufferSize:4096 format:nil
                             block:^(AVAudioPCMBuffer *b, AVAudioTime *t) {
            NSError *er = nil;
            (void)t;
            if (!rec)
                rec = [[AVAudioFile alloc] initForWriting:[NSURL fileURLWithPath:@(w)]
                                                 settings:b.format.settings commonFormat:b.format.commonFormat
                                              interleaved:b.format.interleaved error:&er];
            if (rec && ![rec writeFromBuffer:b error:&er])
                fprintf(stderr, "sound: record: %s\n", er.localizedDescription.UTF8String);
        }];
    }
    if (!S.offline && getenv("BR_SFXQUIET"))
        S.engine.mainMixerNode.outputVolume = 0;        /* recorded, not heard */
    if (![S.engine startAndReturnError:&err]) {
        fprintf(stderr, "sound: no audio output (%s)\n", err.localizedDescription.UTF8String);
        S.engine = nil;
        return 0;
    }
    S.t0 = h_timeGetTime();
    S.on = 1;
    fprintf(stderr, "sound: effects mixing at %.0f Hz%s%s\n", S.rate, S.offline ? ", recorded offline to " : "",
            S.offline ? w : "");
    return 1;
}

/* happ_pump: the focus mute, or the offline recording caught up with the
 * game's clock */
void nsound_poll(void)
{
    u64 want;
    if (!S.on) return;
    if (!S.offline) {
        atomic_store(&S.muted, !NSApp.isActive);
        return;
    }
    want = (u64)((double)(u32)(h_timeGetTime() - S.t0) * OFFLINE_RATE / 1000.0);
    while (S.frames + OFFLINE_BLOCK <= want) {
        NSError *err = nil;
        float io[OFFLINE_BLOCK * 2];
        const float *l, *r;
        u32 i;
        if ([S.engine renderOffline:OFFLINE_BLOCK toBuffer:S.block error:&err] !=
                AVAudioEngineManualRenderingStatusSuccess || S.block.frameLength != OFFLINE_BLOCK) {
            fprintf(stderr, "sound: offline render failed (%s)\n", err.localizedDescription.UTF8String);
            S.on = 0;
            return;
        }
        l = S.block.floatChannelData[0];
        r = S.block.floatChannelData[1];
        for (i = 0; i < OFFLINE_BLOCK; i++) { io[2 * i] = l[i]; io[2 * i + 1] = r[i]; }
        fwrite(io, sizeof io, 1, S.wav);
        S.frames += OFFLINE_BLOCK;
    }
}

/* ------------------------------------------------------- overrides -- */

/* WHY (the three below): br_musiccmd.c calls the real functions through
 * local empty stand-ins (BrExt_10072A90 for BrSndVoiceConfigure at
 * 0x1006BA00, BrExt_10072B30 for FUN_1006baa0 at 0x1006BAA0). The MSVC
 * build is byte-exact because a call's target is a relocation, but the
 * translation inlines the empty stand-ins, so every sound the game plays
 * through these wrappers (the menu clicks, the start of a race and the
 * race's one-shots) never started. Each forwards as the original does. */
#define F_VOICECONFIGURE 0x1006BA00u     /* BrSndVoiceConfigure(group, slot, packed, loop) */
#define F_SLOTLEVELS     0x1006BAA0u     /* FUN_1006baa0(group, slot, packed) */

/* @replaces 0x1006B9E0 BrWrap_10072A70 */
void n_BrWrap_10072A70(u32 group, u32 packed, u32 loop)
{
    W_TRACE("n_BrWrap_10072A70");
    w_icall_iiii_i(F_VOICECONFIGURE, group, 1, packed, loop);
}

/* @replaces 0x1006BA80 BrWrap_10072B10 */
void n_BrWrap_10072B10(u32 group, u32 slot, u32 packed)
{
    W_TRACE("n_BrWrap_10072B10");
    w_icall_iiii_i(F_VOICECONFIGURE, group, slot + slot, packed, 1);
}

/* @replaces 0x1006BAF0 BrWrap_10072B80 */
void n_BrWrap_10072B80(u32 group, u32 slot, u32 packed)
{
    W_TRACE("n_BrWrap_10072B80");
    w_icall_iii_i(F_SLOTLEVELS, group, slot + slot, packed);
}

/* WHY: BrSndBufSetPan ends in a bare `return;` after BrSndVoiceBufStart,
 * relying on MSVC leaving that call's eax -- the last DirectSound call's
 * result -- as its own.  The translation returns whatever the register
 * held, so BrSfxChanStart took every one-shot start as a failure: the
 * channel never got its rate and was never polled or retuned.  Here those
 * calls (GetStatus, Play, SetCurrentPosition) always return S_OK. */
/* @replaces 0x1006B950 BrSndBufSetPan */
u32 n_BrSndBufSetPan(u32 voice, u32 loop)
{
    W_TRACE("n_BrSndBufSetPan");
    W_ORIG_BrSndBufSetPan(voice, loop);
    return 0;
}

/* WHY: seek a WAV to its sample data and descend into the "data" chunk.
 * The source (br_input.c) ends in a bare `return;` after mmioDescend,
 * relying on MSVC leaving that call's eax as the result; the translation
 * returns nothing, so every sample load failed and no sound was made. */
/* @replaces 0x10070170 BrWaveSeekData */
u32 n_BrWaveSeekData(u32 phmmio, u32 ck, u32 parent)
{
    W_TRACE("n_BrWaveSeekData");
    h_mmioSeek(W_LD(u32, phmmio, 0), W_LD(u32, parent, 12) + 4, 0);
    W_ST(u32, ck, 0, 0x61746164u);                       /* 'data' */
    return h_mmioDescend(W_LD(u32, phmmio, 0), ck, parent, 0x10);
}
