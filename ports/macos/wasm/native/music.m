/* music.m -- the soundtrack, played natively (ports/MUSIC-DECISION-PENDING.md,
 * decided 2026-09-29).
 *
 * The game treats its music as a CD player: track 2 on the front end
 * (BrBootInit), a random track from 3 up in each race (BrCdTrackRandom),
 * a jukebox row in Options, next/previous keys, pause while the window is
 * inactive, and a new track when one ends. It reaches the player through
 * one of two Windows backends, chosen by PlayMusic= (g_brCdEnabled): 1 is
 * MCI Redbook audio, 2 (the shipped default) the EAR engine's CD channel.
 *
 * Every function of that module that talks to either backend is replaced
 * here; the ones that are pure game logic (which track, clamping, the
 * random pick, next/previous/wrap, the dispatch on the mode) still run as
 * the game's own code, so the game asks for exactly what it asked for on
 * Windows. What answers is AVAudioEngine:
 *
 *   PC   the disc's CD audio, track NN.flac, streamed into an AVAudioPlayerNode
 *   N64  Top Gear Rally's music, looping forever as on the N64. The N64
 *        game's own cues pick the piece (extract_modules.py): its title
 *        piece for track 2, and for a race track (3 up) one of the five
 *        per-track race pieces, (track - 3) mod 5. With Barry Leitch's own
 *        recordings of the six pieces in the app (ost_loops.py, which also
 *        clears them of the PAL hardware's hum), those play: [0, loop_end)
 *        and then [loop_start, loop_end) over and over, streamed back to
 *        back, so the loop is a hard, gapless, sample-exact jump at the
 *        point where the recording's two passes match, and the fade at its
 *        end is never reached. Without them the ROM's modules play live through
 *        libopenmpt in an AVAudioSourceNode.
 *
 * Which one is heard is the version the port is being, PC or N64
 * (native/version.m, Tab). Both play every cue at once, each through its
 * own submix, the one not heard at zero gain, so a switch crossfades
 * (FADE_S, equal power) to the other where it has got to: switching back
 * and forth never restarts either. The CD tracks end and the game moves on
 * to the next one whichever version is heard; the N64 piece loops on under
 * that, as on the N64, and changes only when the game asks for a new cue.
 * A version whose files are missing is silence. Levels: the modules are rendered 4.0 dB down, which puts their mean
 * integrated loudness (-16.2 LUFS) within 0.6 dB of the CD tracks' (-15.6)
 * and keeps the loudest module's +4.0 dBFS peak below full scale
 * (measured 2026-09-29, ffmpeg ebur128). The recordings get the gain
 * ost_loops.py measured for the same match (gain_db).
 *
 * When a track ends: under EAR the engine told the window and the game
 * advanced with BrCdTrackNextWrap; under MCI the drive played on through
 * the disc and at the end notified the window, whose handler
 * (0x10002830) started again from the requested track. Both are done the
 * same way here, from happ_pump, which is where the window's messages
 * would have been handled.
 *
 * Silent (the game's state still moves, nothing is heard and no track
 * ever ends) when headless or with BR_MUSIC=0. Data: the app's
 * Resources/music, else BR_MUSIC_DIR, else build/app/extract/music.
 * BR_MUSICWAV=path records the music output to a file (a script's
 * `version` command switches, as Tab does); BR_MUSICLOOPTEST=s starts a recording s seconds before its loop end
 * (checks the loop without waiting minutes for it).
 */
#import <AVFoundation/AVFoundation.h>
#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#include <libopenmpt/libopenmpt.h>
#include <dispatch/dispatch.h>
#include <math.h>
#include <os/lock.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include "w2c_native.h"
#include "host.h"

/* the module's globals (symmap.csv) */
#define G_ENABLED  0x1007B074u   /* g_brCdEnabled: 0 off, 1 MCI, 2 EAR */
#define G_MEDIAOK  0x1021C76Cu   /* g_brCdMediaOk */
#define G_LAST     0x1021C768u   /* g_brCdTrackLast */
#define G_FIRST    0x1021C774u   /* g_brCdTrackFirst */
#define G_EARSTATE 0x1021C778u   /* EAR channel block, +0: channel open */
#define G_HWND     0x1021C77Cu
#define G_PLAYING  0x1021C800u   /* g_brCdPlaying: open count */
#define G_CUR      0x1021C804u   /* g_brCdTrackCur */
#define G_PENDING  0x1021C808u   /* a track has been asked for */
#define G_STEP     0x106E79F4u   /* g_pfnStep: the current activity (br_gamestep.c) */
/* game functions called back */
#define F_TRACKRESUME   0x10002E80u   /* BrCdTrackResume */
#define F_NEXTWRAP      0x10002CF0u   /* BrCdTrackNextWrap */
#define F_MCINOTIFY     0x10002830u   /* the MM_MCINOTIFY handler's play */
#define F_FADERELEASE   0x10017F10u   /* BrFadeRelease */
#define F_CLOCK         0x1006E280u   /* BrSub10075020, the ms clock */
#define F_TRACKPLAY     0x10002AF0u   /* BrCdTrackPlay, the mode dispatch */
#define F_STARTUP       0x10002910u   /* BrCdStartup, the EAR route's open */
#define F_RACESTEP      0x10019A70u   /* BrRaceStep, the race activity */

#define GI(a) ((s32)W_LD(u32, (a), 0))
#define GS(a, v) W_ST(u32, (a), 0, (u32)(v))

void h_srand(u32 s);
NSWindow *happ_window(void);

#define N64_GAIN_MB (-400)            /* millibel, see the header */

#define FADE_S 0.5                    /* the version crossfade, seconds */

enum { ST_PC = HV_PC, ST_N64 = HV_N64, ST_REM, NST };
int hfx_on(void);

static struct {
    int init, silent;
    int have[NST];
    int cd_last;                      /* highest CD track number on disc */
    NSString *dir[NST];
    NSString *title;
    NSArray<NSString *> *race;
    NSDictionary *rec_title;          /* the recordings, when all are present */
    NSArray<NSDictionary *> *rec_race;
    float rec_gain_db;
    NSDictionary *rem_title;          /* the remastered soundtrack (ports/common/music) */
    NSArray<NSDictionary *> *rem_race;
    int remastered;                   /* heard over either version while Remastered is on (~) */
    int soundtrack;                   /* the version heard: native/version.m's */
    int track;                        /* the CD track number playing, 0 none */
    int paused;
    int advancing;                    /* the PC track ended: only it moves on */
    float volume;
    AVAudioEngine *engine;
    AVAudioMixerNode *mix[NST];       /* each source's submix, crossfaded */
    AVAudioPlayerNode *cd;
    AVAudioSourceNode *mod_node;
    AVAudioUnitEffect *limiter;
    AVAudioPlayerNode *rec;
    AVAudioUnitEQ *rec_eq;
    AVAudioPlayerNode *rem;
    AVAudioUnitEQ *rem_eq;
    AVAudioFormat *fmt;               /* the output device's rate: every source's */
    atomic_uint gen[NST], ended;      /* per source: a stale callback is ignored */
    os_unfair_lock lock;              /* guards mod against the render thread */
    openmpt_module *mod;
} M = { .lock = OS_UNFAIR_LOCK_INIT, .volume = 1.0f };

/* The crossfade: x runs from 0 (the PC version alone) to 1 (the N64 one
 * alone) at 1/FADE_S per second, and the two submixes get equal-power
 * gains cos and sin of x * pi/2, so the sum stays as loud through the fade.
 * It runs on its own queue at 4 ms steps (a fifth of a render buffer or
 * less), so the game's frame loop can neither stall nor step it; a Tab in
 * the middle of a fade just turns it round from where it is. */
/* The remastered soundtrack fades the same way on its own axis y: 0 is
 * the version Tab picked, 1 the remastered one, whichever version is under
 * it; the three gains keep the sum equal-power throughout. */
static struct {
    dispatch_queue_t q;
    dispatch_source_t timer;
    double x, y, last;
    int target, ytarget;
} X;

static void fade_apply(void)
{
    double under = cos(X.y * M_PI_2);
    M.mix[ST_PC].outputVolume = (float)(cos(X.x * M_PI_2) * under);
    M.mix[ST_N64].outputVolume = (float)(sin(X.x * M_PI_2) * under);
    if (M.mix[ST_REM]) M.mix[ST_REM].outputVolume = (float)sin(X.y * M_PI_2);
}

static double fade_step(double v, int target, double d)
{
    return target ? fmin(v + d, 1) : fmax(v - d, 0);
}

static void fade_to(int v, int y, int now)
{
    dispatch_async(X.q, ^{
        X.target = v; X.ytarget = y;
        if (now) { X.x = v; X.y = y; }
        fade_apply();
        if (now || X.timer || (X.x == v && X.y == y)) return;
        X.last = CACurrentMediaTime();
        X.timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, X.q);
        dispatch_source_set_timer(X.timer, DISPATCH_TIME_NOW, 4 * NSEC_PER_MSEC, NSEC_PER_MSEC);
        dispatch_source_set_event_handler(X.timer, ^{
            double t = CACurrentMediaTime(), d = (t - X.last) / FADE_S;
            X.last = t;
            X.x = fade_step(X.x, X.target, d);
            X.y = fade_step(X.y, X.ytarget, d);
            fade_apply();
            if (X.x == X.target && X.y == X.ytarget) {
                dispatch_source_cancel(X.timer);
                X.timer = nil;
            }
        });
        dispatch_resume(X.timer);
    });
}

static void fade(int v, int now)
{
    fade_to(v, M.remastered && M.have[ST_REM], now);
}

static void play(int track);
u32 n_FUN_10002980(u32 hwnd);
u32 n_FUN_100027e0(void);

static NSString *music_root(void)
{
    NSString *res = [[NSBundle mainBundle] resourcePath];
    NSString *p = [res stringByAppendingPathComponent:@"music"];
    const char *e = getenv("BR_MUSIC_DIR");
    if (res && [[NSFileManager defaultManager] fileExistsAtPath:p]) return p;
    if (e) return @(e);
    return [@(getenv("BR_ROOT") ? getenv("BR_ROOT") : ".") stringByAppendingPathComponent:@"build/app/extract/music"];
}

static void music_init(void)
{
    NSFileManager *fm = [NSFileManager defaultManager];
    NSString *root;
    const char *e = getenv("BR_MUSIC");
    int t;
    if (M.init) return;
    M.init = 1;
    root = music_root();
    M.dir[ST_PC] = [root stringByAppendingPathComponent:@"cd"];
    M.dir[ST_N64] = [root stringByAppendingPathComponent:@"n64"];
    for (t = 2; t < 100; t++)
        if (![fm fileExistsAtPath:[M.dir[ST_PC] stringByAppendingPathComponent:
                                   [NSString stringWithFormat:@"track%02d.flac", t]]]) break;
    M.cd_last = t - 1;
    M.have[ST_PC] = M.cd_last >= 2;
    {
        NSData *j = [NSData dataWithContentsOfFile:[M.dir[ST_N64] stringByAppendingPathComponent:@"modules.json"]];
        NSDictionary *d = j ? [NSJSONSerialization JSONObjectWithData:j options:0 error:nil] : nil;
        M.title = d[@"title"];
        M.race = d[@"race"];
        M.have[ST_N64] = M.title && M.race.count > 0;
        {
            NSDictionary *r = d[@"recordings"];
            int ok = r && [r[@"race"] count] == M.race.count;
            for (NSDictionary *x in ok ? [@[r[@"title"]] arrayByAddingObjectsFromArray:r[@"race"]] : @[])
                ok = ok && [fm fileExistsAtPath:[M.dir[ST_N64] stringByAppendingPathComponent:x[@"file"]]];
            if (ok) {
                M.rec_title = r[@"title"];
                M.rec_race = r[@"race"];
                M.rec_gain_db = [r[@"gain_db"] floatValue];
            }
        }
    }
    {
        /* the remastered soundtrack: the app's music/remastered, or the tree's ports/common/music */
        NSString *d = [root stringByAppendingPathComponent:@"remastered"];
        NSDictionary *r;
        NSData *j;
        if (![fm fileExistsAtPath:[d stringByAppendingPathComponent:@"remastered.json"]])
            d = [@(getenv("BR_ROOT") ? getenv("BR_ROOT") : ".") stringByAppendingPathComponent:@"ports/common/music"];
        M.dir[ST_REM] = d;
        j = [NSData dataWithContentsOfFile:[d stringByAppendingPathComponent:@"remastered.json"]];
        r = j ? [NSJSONSerialization JSONObjectWithData:j options:0 error:nil] : nil;
        if (r && [r[@"race"] count] > 0) {
            int ok = 1;
            for (NSDictionary *x in [@[r[@"title"]] arrayByAddingObjectsFromArray:r[@"race"]])
                ok = ok && [fm fileExistsAtPath:[d stringByAppendingPathComponent:x[@"file"]]];
            if (ok) { M.rem_title = r[@"title"]; M.rem_race = r[@"race"]; M.have[ST_REM] = 1; }
        }
    }
    M.soundtrack = hversion();
    M.remastered = hfx_on();
    fprintf(stderr, "music: %s; PC %s (tracks 2-%d), N64 %s, remastered %s; playing %s\n", root.UTF8String,
            M.have[ST_PC] ? "yes" : "no", M.cd_last,
            !M.have[ST_N64] ? "no" : M.rec_title ? "yes (recordings)" : "yes (modules)",
            M.have[ST_REM] ? "yes" : "no",
            M.remastered && M.have[ST_REM] ? "remastered" : M.soundtrack == ST_PC ? "PC" : "N64");

    M.silent = !happ_window() || (e && !atoi(e)) || !(M.have[0] || M.have[1] || M.have[ST_REM]);
    if (M.silent) return;
    M.engine = [AVAudioEngine new];
    /* Everything runs at the output device's rate, so the engine converts
     * nothing: its own converter (the mixers') is a cheap one, measured
     * 2026-09-29 at 20 dB signal to error on a CD track taken to 48 kHz,
     * heard as fuzz and crackle. Each file is converted as it streams, by
     * feed() below; the modules render at this rate directly. */
    M.fmt = [[AVAudioFormat alloc] initStandardFormatWithSampleRate:
               [M.engine.outputNode outputFormatForBus:0].sampleRate channels:2];
    for (t = 0; t < NST; t++) {
        M.mix[t] = [AVAudioMixerNode new];
        [M.engine attachNode:M.mix[t]];
        [M.engine connect:M.mix[t] to:M.engine.mainMixerNode format:M.fmt];
    }
    M.cd = [AVAudioPlayerNode new];
    [M.engine attachNode:M.cd];
    [M.engine connect:M.cd to:M.mix[ST_PC] format:M.fmt];
    M.mod_node = [[AVAudioSourceNode alloc] initWithFormat:M.fmt
        renderBlock:^OSStatus(BOOL *silence, const AudioTimeStamp *ts, AVAudioFrameCount n, AudioBufferList *abl) {
            float *l = abl->mBuffers[0].mData, *r = abl->mNumberBuffers > 1 ? abl->mBuffers[1].mData : NULL;
            size_t got = 0;
            (void)ts;
            if (os_unfair_lock_trylock(&M.lock)) {
                if (M.mod && !M.paused && r)
                    got = openmpt_module_read_float_stereo(M.mod, (int32_t)M.fmt.sampleRate, n, l, r);
                os_unfair_lock_unlock(&M.lock);
            }
            if (got < n) {
                memset(l + got, 0, (n - got) * sizeof *l);
                if (r) memset(r + got, 0, (n - got) * sizeof *r);
            }
            if (!got) *silence = YES;
            return noErr;
        }];
    [M.engine attachNode:M.mod_node];
    [M.engine connect:M.mod_node to:M.mix[ST_N64] format:M.fmt];
    if (M.rec_title) {
        M.rec = [AVAudioPlayerNode new];
        M.rec_eq = [[AVAudioUnitEQ alloc] initWithNumberOfBands:0];
        M.rec_eq.globalGain = M.rec_gain_db;
        [M.engine attachNode:M.rec];
        [M.engine attachNode:M.rec_eq];
        [M.engine connect:M.rec to:M.rec_eq format:M.fmt];
        [M.engine connect:M.rec_eq to:M.mix[ST_N64] format:M.fmt];
    }
    if (M.have[ST_REM]) {
        M.rem = [AVAudioPlayerNode new];
        M.rem_eq = [[AVAudioUnitEQ alloc] initWithNumberOfBands:0];
        [M.engine attachNode:M.rem];
        [M.engine attachNode:M.rem_eq];
        [M.engine connect:M.rem to:M.rem_eq format:M.fmt];
        [M.engine connect:M.rem_eq to:M.mix[ST_REM] format:M.fmt];
    }
    M.engine.mainMixerNode.outputVolume = M.volume;
    X.q = dispatch_queue_create("music.fade", DISPATCH_QUEUE_SERIAL);
    fade(M.soundtrack, 1);
    {
        /* The disc is mastered to full scale, and resampling it to the
         * device rate overshoots (+0.9 dBFS on track 2 at 48 kHz, measured
         * 2026-09-29): the limiter catches those overs instead of the
         * output clipping them. Below full scale it leaves the signal as
         * it is. */
        AudioComponentDescription d = { kAudioUnitType_Effect, kAudioUnitSubType_PeakLimiter,
                                        kAudioUnitManufacturer_Apple, 0, 0 };
        AVAudioFormat *out = M.fmt;
        M.limiter = [[AVAudioUnitEffect alloc] initWithAudioComponentDescription:d];
        [M.engine attachNode:M.limiter];
        [M.engine disconnectNodeOutput:M.engine.mainMixerNode];
        [M.engine connect:M.engine.mainMixerNode to:M.limiter format:out];
        [M.engine connect:M.limiter to:M.engine.outputNode format:out];
    }
    if (getenv("BR_MUSICWAV")) {
        /* what the player sends to the output, recorded (checks without
         * listening) */
        static AVAudioFile *rec;
        [M.limiter installTapOnBus:0 bufferSize:4096 format:nil
                                          block:^(AVAudioPCMBuffer *b, AVAudioTime *t) {
            NSError *err = nil;
            (void)t;
            if (!rec)       /* the format the engine settled on, not the one it started with */
                rec = [[AVAudioFile alloc] initForWriting:[NSURL fileURLWithPath:@(getenv("BR_MUSICWAV"))]
                                                 settings:b.format.settings commonFormat:b.format.commonFormat
                                              interleaved:b.format.interleaved error:&err];
            if (rec && ![rec writeFromBuffer:b error:&err])
                fprintf(stderr, "music: record: %s\n", err.localizedDescription.UTF8String);
        }];
    }
    {
        NSError *err = nil;
        if (![M.engine startAndReturnError:&err]) {
            fprintf(stderr, "music: no audio output (%s)\n", err.localizedDescription.UTF8String);
            M.silent = 1;
        }
    }
}

static void mod_set(openmpt_module *m)
{
    openmpt_module *old;
    os_unfair_lock_lock(&M.lock);
    old = M.mod;
    M.mod = m;
    os_unfair_lock_unlock(&M.lock);
    if (old) openmpt_module_destroy(old);
}

static void stop_cd(void)
{
    atomic_fetch_add(&M.gen[ST_PC], 1);
    if (!M.silent) [M.cd stop];
}

static void stop_n64(void)
{
    atomic_fetch_add(&M.gen[ST_N64], 1);
    if (M.silent) return;
    [M.rec stop];
    mod_set(NULL);
}

static void stop_rem(void)
{
    atomic_fetch_add(&M.gen[ST_REM], 1);
    if (!M.silent) [M.rem stop];
}

static void stop(void)
{
    if (M.track) HLOG("music: stop %d\n", M.track);
    stop_cd();
    stop_n64();
    stop_rem();
    M.track = 0;
    M.paused = 0;
}

/* A file streamed into a player, converted to the output rate on the way
 * by AVAudioConverter at its best (the mastering algorithm, maximum
 * quality): 0.25 s buffers, three queued ahead, each one converted on the
 * feed's own queue as the player takes the one before. With a loop it
 * reads [from, end) and then [loop, end) over and over: the converter sees
 * one continuous stream in which frame end-1 is followed by frame loop, so
 * the jump stays hard and sample-exact at the recording's own rate. Without
 * one, when the file runs out the last buffer reports it (ended). */
@interface BRFeed : NSObject {
@public
    AVAudioFile *file;
    AVAudioConverter *cv;
    AVAudioPCMBuffer *in;
    AVAudioPlayerNode *player;
    AVAudioFramePosition pos, end, loop;          /* loop < 0: none */
    atomic_uint *genp;
    unsigned gen;
    int done;
    void (^ended)(void);
    dispatch_queue_t q;
}
@end
@implementation BRFeed
@end

#define FEED_S 0.25
#define FEED_AHEAD 3

static void feed_next(BRFeed *F)
{
    AVAudioPCMBuffer *out;
    NSError *err = nil;
    AVAudioConverterOutputStatus st;
    __block int eof = 0;
    if (F->done || atomic_load(F->genp) != F->gen) return;
    out = [[AVAudioPCMBuffer alloc] initWithPCMFormat:M.fmt
                                        frameCapacity:(AVAudioFrameCount)(M.fmt.sampleRate * FEED_S)];
    st = [F->cv convertToBuffer:out error:&err withInputFromBlock:
          ^AVAudioBuffer *(AVAudioPacketCount n, AVAudioConverterInputStatus *s) {
        AVAudioFrameCount k;
        NSError *e = nil;
        if (F->pos >= F->end) {
            if (F->loop < 0) { eof = 1; *s = AVAudioConverterInputStatus_EndOfStream; return nil; }
            F->pos = F->loop;
        }
        k = (AVAudioFrameCount)MIN((AVAudioFramePosition)MIN(n, F->in.frameCapacity), F->end - F->pos);
        if (F->file.framePosition != F->pos) F->file.framePosition = F->pos;
        if (![F->file readIntoBuffer:F->in frameCount:k error:&e] || !F->in.frameLength) {
            fprintf(stderr, "music: read %s: %s\n", F->file.url.path.UTF8String, e.localizedDescription.UTF8String);
            eof = 1; *s = AVAudioConverterInputStatus_EndOfStream; return nil;
        }
        F->pos += F->in.frameLength;
        *s = AVAudioConverterInputStatus_HaveData;
        return F->in;
    }];
    if (st == AVAudioConverterOutputStatus_Error) {
        fprintf(stderr, "music: convert: %s\n", err.localizedDescription.UTF8String);
        eof = 1;
    }
    F->done = eof || st == AVAudioConverterOutputStatus_EndOfStream;
    if (!F->done) {
        [F->player scheduleBuffer:out atTime:nil options:0
           completionCallbackType:AVAudioPlayerNodeCompletionDataConsumed
                completionHandler:^(AVAudioPlayerNodeCompletionCallbackType t) {
            (void)t;
            dispatch_async(F->q, ^{ feed_next(F); });
        }];
        return;
    }
    [F->player scheduleBuffer:out atTime:nil options:0
       completionCallbackType:AVAudioPlayerNodeCompletionDataPlayedBack
            completionHandler:^(AVAudioPlayerNodeCompletionCallbackType t) {
        (void)t;
        if (F->ended && atomic_load(F->genp) == F->gen) F->ended();
    }];
}

/* start streaming path into player from frame `from` (see BRFeed); 0 when
 * the file cannot be read */
static int feed(AVAudioPlayerNode *player, int st, NSString *path, AVAudioFramePosition from,
                AVAudioFramePosition end, AVAudioFramePosition loop, void (^ended)(void))
{
    NSError *err = nil;
    BRFeed *F = [BRFeed new];
    F->file = [[AVAudioFile alloc] initForReading:[NSURL fileURLWithPath:path] error:&err];
    if (!F->file) { fprintf(stderr, "music: %s: %s\n", path.UTF8String, err.localizedDescription.UTF8String); return 0; }
    F->cv = [[AVAudioConverter alloc] initFromFormat:F->file.processingFormat toFormat:M.fmt];
    F->cv.sampleRateConverterAlgorithm = AVSampleRateConverterAlgorithm_Mastering;
    F->cv.sampleRateConverterQuality = AVAudioQualityMax;
    F->in = [[AVAudioPCMBuffer alloc] initWithPCMFormat:F->file.processingFormat frameCapacity:16384];
    F->player = player;
    F->pos = from;
    F->end = end > 0 ? end : F->file.length;
    F->loop = loop;
    F->genp = &M.gen[st];
    F->gen = atomic_load(&M.gen[st]);
    F->ended = ended;
    F->q = dispatch_queue_create("music.feed", DISPATCH_QUEUE_SERIAL);
    dispatch_sync(F->q, ^{ for (int k = 0; k < FEED_AHEAD; k++) feed_next(F); });
    [player play];
    return 1;
}

static void play_cd(int track)
{
    NSString *p = [M.dir[ST_PC] stringByAppendingPathComponent:
                   [NSString stringWithFormat:@"track%02d.flac", track]];
    unsigned g = atomic_load(&M.gen[ST_PC]);
    feed(M.cd, ST_PC, p, 0, 0, -1, ^{ atomic_store(&M.ended, g); });
}

static void play_mod(int track)
{
    NSString *name = track <= 2 ? M.title : M.race[(NSUInteger)(track - 3) % M.race.count];
    NSData *d = [NSData dataWithContentsOfFile:[M.dir[ST_N64] stringByAppendingPathComponent:name]];
    openmpt_module *m = d ? openmpt_module_create_from_memory2(d.bytes, d.length, NULL, NULL, NULL, NULL,
                                                                NULL, NULL, NULL) : NULL;
    if (!m) { fprintf(stderr, "music: cannot load %s\n", name.UTF8String); return; }
    openmpt_module_set_repeat_count(m, -1);
    openmpt_module_set_render_param(m, OPENMPT_MODULE_RENDER_MASTERGAIN_MILLIBEL, N64_GAIN_MB);
    mod_set(m);
}

static void play_rec(int track)
{
    NSDictionary *r = track <= 2 ? M.rec_title : M.rec_race[(NSUInteger)(track - 3) % M.rec_race.count];
    NSString *p = [M.dir[ST_N64] stringByAppendingPathComponent:r[@"file"]];
    AVAudioFramePosition s = [r[@"loop_start"] longLongValue], e = [r[@"loop_end"] longLongValue], from = 0;
    const char *t = getenv("BR_MUSICLOOPTEST");
    if (t) from = MAX(0, e - (AVAudioFramePosition)(atof(t) * [r[@"rate"] doubleValue]));
    feed(M.rec, ST_N64, p, from, e, s, nil);
}

/* The remastered piece for a cue, looped like the recordings; its gain
 * levels it with the CD tracks. */
static void play_rem(int track)
{
    NSDictionary *r = track <= 2 ? M.rem_title : M.rem_race[(NSUInteger)(track - 3) % M.rem_race.count];
    M.rem_eq.globalGain = [r[@"gain_db"] floatValue];
    feed(M.rem, ST_REM, [M.dir[ST_REM] stringByAppendingPathComponent:r[@"file"]], 0,
         [r[@"loop_end"] longLongValue], [r[@"loop_start"] longLongValue], nil);
}

/* A cue starts both versions of it: the one not heard plays on at zero
 * gain, so a switch finds it where it would be. When the PC track has
 * ended and the game moves on (M.advancing), only the PC version changes
 * track; the N64 piece loops on regardless, as it does on the N64. */
static void play(int track)
{
    if (M.advancing) {
        stop_cd();
        M.track = track;
        HLOG("music: play %d (PC moves on)\n", track);
        if (M.silent) return;
        if (M.have[ST_PC]) play_cd(track);
        if (M.paused) {                        /* the game asked for music: none is paused */
            M.paused = 0;
            [M.rec play];
            [M.rem play];
        }
        return;
    }
    stop();
    M.track = track;
    HLOG("music: play %d (heard: %s)\n", track, M.soundtrack == ST_PC ? "PC" : "N64");
    if (M.silent) return;
    if (M.have[ST_PC]) play_cd(track);
    if (M.have[ST_N64]) {
        if (M.rec_title) play_rec(track);
        else play_mod(track);
    }
    if (M.have[ST_REM]) play_rem(track);
}

static void pause_(void)
{
    if (!M.track || M.paused) return;
    M.paused = 1;
    HLOG("music: pause %d\n", M.track);
    if (!M.silent) { [M.cd pause]; [M.rec pause]; [M.rem pause]; }
}

static void resume(void)
{
    if (!M.track || !M.paused) return;
    M.paused = 0;
    HLOG("music: resume %d\n", M.track);
    if (M.silent) return;
    [M.cd play];
    [M.rec play];
    [M.rem play];
}

/* native/version.m: Tab. The other version fades in where it has got to. */
void nmusic_version(int v)
{
    M.soundtrack = v;
    if (!M.init || M.silent) return;
    if (!M.have[v]) fprintf(stderr, "music: no %s soundtrack in %s\n", v == ST_PC ? "PC" : "N64",
                            M.dir[v].UTF8String);
    if (g_hlog) {
        AVAudioTime *pc = [M.cd playerTimeForNodeTime:M.cd.lastRenderTime];
        AVAudioTime *n = [M.rec playerTimeForNodeTime:M.rec.lastRenderTime];
        double mod;
        os_unfair_lock_lock(&M.lock);
        mod = M.mod ? openmpt_module_get_position_seconds(M.mod) : -1;
        os_unfair_lock_unlock(&M.lock);
        HLOG("music: to %s; PC at %.2f s, N64 at %.2f s\n", v == ST_PC ? "PC" : "N64",
             pc ? pc.sampleTime / pc.sampleRate : -1,
             n ? n.sampleTime / n.sampleRate : mod);
    }
    fade(v, 0);
}

/* host_fx.m: ~. While Remastered is on the remastered soundtrack is heard
 * whichever version Tab has picked; off, that version is heard as before. */
void nmusic_remastered(int on)
{
    M.remastered = on;
    if (!M.init || M.silent) return;
    if (on && !M.have[ST_REM]) fprintf(stderr, "music: no remastered soundtrack in %s\n", M.dir[ST_REM].UTF8String);
    HLOG("music: remastered %s\n", on ? "on" : "off");
    fade(M.soundtrack, 0);
}

static int active(void)
{
    return GI(G_ENABLED) && GI(G_PLAYING) && GI(G_MEDIAOK);
}

/* The disc as the game will see it: the PC soundtrack's track numbers, or
 * with only the N64 soundtrack present, the title cue and one per race
 * module. */
static void media_open(void)
{
    music_init();
    GS(G_FIRST, 2);
    GS(G_LAST, M.have[ST_PC] ? M.cd_last : 2 + (int)M.race.count);
    GS(G_MEDIAOK, 1);
}

/* happ_pump: a track that ended is handled here, where the window's
 * messages would have been. Only the PC version's tracks end (the N64
 * pieces loop), and they end whichever version is heard. */
/* The game makes no music call when a race ends (no caller of the CD
 * module in the original runs on the way out), so on Windows the race's
 * CD track played on under the menus, and the N64 and remastered pieces,
 * which loop, would never end. Leaving the race activity cues the title
 * piece, as Top Gear Rally does on the N64: through BrCdTrackPlay(2), the
 * game's own call at boot (BrBootInit), so its track state follows. */
static void race_left(void)
{
    static u32 last;
    u32 s = (u32)GI(G_STEP);
    if (s == last) return;
    HLOG("music: activity %08X -> %08X\n", last, s);
    if (last == F_RACESTEP && GI(G_ENABLED) && GI(G_PLAYING)) w_icall_i_i(F_TRACKPLAY, 2);
    last = s;
}

void nmusic_poll(void)
{
    unsigned e;
    race_left();
    e = atomic_exchange(&M.ended, 0);
    if (!e || e != atomic_load(&M.gen[ST_PC]) || !M.track) return;
    HLOG("music: track %d ended\n", M.track);
    M.advancing = 1;
    if (GI(G_ENABLED) == 1) {
        /* MCI: the drive plays on through the disc, then notifies */
        if (M.track < GI(G_LAST)) play(M.track + 1);
        else { M.track = 0; w_icall__i(F_MCINOTIFY); }
    } else {
        M.track = 0;
        w_icall__i(F_NEXTWRAP);
    }
    M.advancing = 0;
}

/* ------------------------------------------------------- overrides -- */

/* WHY: the start of the music, from BrBootColdInitRun: MCI open
 * (0x10002980) under PlayMusic=1, else BrCdStartup (0x10002910, the EAR
 * route). The translated body reaches neither: br_musiccmd.c defines its
 * two callees as empty local stand-ins, so the music never opened. */
/* @replaces 0x100028E0 BrDispatch_100025C0 */
void n_BrDispatch_100025C0(u32 hwnd)
{
    W_TRACE("n_BrDispatch_100025C0");
    if (GI(G_ENABLED) == 1) n_FUN_10002980(hwnd);
    else w_icall_i_i(F_STARTUP, hwnd);
}

/* WHY: the current track, which next/previous/wrap step from. The source
 * (br_cd.c) ends each arm in a bare `return;` after the call, relying on
 * MSVC leaving the callee's eax as the result; the translation returns
 * nothing, so every step started from 0 and clamped back to track 2. */
/* @replaces 0x10002C50 BrCdTrackGet */
u32 n_BrCdTrackGet(void)
{
    W_TRACE("n_BrCdTrackGet");
    if (GI(G_ENABLED) == 1) return n_FUN_100027e0();
    return (GI(G_ENABLED) && GI(G_PLAYING) && GI(G_MEDIAOK)) ? (u32)GI(G_CUR) : 0;   /* BrCdTrackGetEar */
}

/* WHY: the three steps below are the game's logic, but the translation
 * inlines its broken BrCdTrackGet into each (same file), so they are
 * restated here around the fixed one. */
static void step(int d, int wrap)
{
    s32 t;
    if (!GI(G_ENABLED) || !GI(G_PLAYING)) return;
    t = (s32)n_BrCdTrackGet() + d;
    if (d < 0 && t < GI(G_FIRST)) t = GI(G_FIRST);
    if (d > 0 && t > GI(G_LAST)) t = wrap ? GI(G_FIRST) : GI(G_LAST);
    GS(G_CUR, t);
    w_icall_i_i(F_TRACKPLAY, (u32)t);
}

/* @replaces 0x10002C70 BrCdTrackPrev */
u32 n_BrCdTrackPrev(void) { W_TRACE("n_BrCdTrackPrev"); step(-1, 0); return 1; }
/* @replaces 0x10002CB0 BrCdTrackNext */
u32 n_BrCdTrackNext(void) { W_TRACE("n_BrCdTrackNext"); step(1, 0); return 1; }
/* @replaces 0x10002CF0 BrCdTrackNextWrap */
u32 n_BrCdTrackNextWrap(void) { W_TRACE("n_BrCdTrackNextWrap"); step(1, 1); return 1; }

/* WHY: the EAR backend's channel open (RegisterChannel, the channel state
 * block, track range queries). Here the range comes from the soundtrack. */
/* @replaces 0x10002580 BrCdEarChannelOpen */
u32 n_BrCdEarChannelOpen(void)
{
    W_TRACE("n_BrCdEarChannelOpen");
    if (GI(G_ENABLED) && GI(G_PLAYING) && !GI(G_MEDIAOK)) {
        GS(G_EARSTATE, 1);
        media_open();
    }
    return 1;
}

/* WHY: EAR ShutDownChannel. */
/* @replaces 0x10002760 BrCdMaybeClose */
u32 n_BrCdMaybeClose(void)
{
    W_TRACE("n_BrCdMaybeClose");
    if (active()) {
        GS(G_MEDIAOK, 0);
        stop();
    }
    return 1;
}

/* WHY: MCI_STATUS, the drive's current track. */
/* @replaces 0x100027E0 FUN_100027e0 */
u32 n_FUN_100027e0(void)
{
    W_TRACE("n_FUN_100027e0");
    return active() ? (u32)M.track : 0;
}

/* WHY: MCI_PLAY from a track to the end of the disc. */
/* @replaces 0x10002870 FUN_10002870 */
u32 n_FUN_10002870(u32 hwnd, u32 track)
{
    W_TRACE("n_FUN_10002870");
    (void)hwnd;
    play((int)(track & 0xFF));
    return 0;
}

/* WHY: MCI_OPEN of the cdaudio device, time format, track count. The
 * open count, the clock-seeded srand and the track state are the game's
 * and are kept. */
/* @replaces 0x10002980 FUN_10002980 */
u32 n_FUN_10002980(u32 hwnd)
{
    W_TRACE("n_FUN_10002980");
    if (GI(G_ENABLED)) {
        GS(G_PLAYING, GI(G_PLAYING) + 1);
        if (GI(G_PLAYING) == 1) {
            GS(G_HWND, hwnd);
            h_srand(w_icall__i(F_CLOCK));
            GS(G_CUR, 2);
            GS(G_PENDING, 0);
            media_open();
        }
    }
    return 1;
}

/* WHY: the EAR route's MixEvent on the CD channel. The clamp and the
 * recorded track are the game's. */
/* @replaces 0x10002B20 BrCdPlayClamped */
u32 n_BrCdPlayClamped(u32 track)
{
    s32 t = (s32)track;
    W_TRACE("n_BrCdPlayClamped");
    if (GI(G_ENABLED) && GI(G_PLAYING)) {
        if (t < GI(G_FIRST)) t = GI(G_FIRST);
        if (t > GI(G_LAST)) t = GI(G_LAST);
        GS(G_CUR, t);
        if (GI(G_MEDIAOK)) play(t);
    }
    return 1;
}

/* WHY: the EAR channel volume (0..255 to the engine's 0..10000; only the
 * low byte counts, as in the original). */
/* @replaces 0x10002D60 BrCdVolumeScale */
u32 n_BrCdVolumeScale(u32 vol)
{
    W_TRACE("n_BrCdVolumeScale");
    if (active()) {
        M.volume = (float)(vol & 0xFF) / 255.0f;
        if (!M.silent) M.engine.mainMixerNode.outputVolume = M.volume;
    }
    return 1;
}

/* WHY: MCI_PAUSE. */
/* @replaces 0x10002E20 BrCdMciPause */
u32 n_BrCdMciPause(void)
{
    W_TRACE("n_BrCdMciPause");
    if (GI(G_ENABLED) && GI(G_PLAYING)) {
        GS(G_PENDING, 1);
        if (GI(G_MEDIAOK)) pause_();
    }
    return 1;
}

/* WHY: pause; the EAR arm was ChangeChannelControl(4). */
/* @replaces 0x10002EB0 BrCdPause */
u32 n_BrCdPause(void)
{
    W_TRACE("n_BrCdPause");
    if (GI(G_ENABLED) == 1) return n_BrCdMciPause();
    if (active()) pause_();
    return 1;
}

/* WHY: resume; the EAR arm was ChangeChannelControl(0xC). Under MCI the
 * game replays the current track itself (BrCdTrackResume), as it did; that
 * replays the CD track only, and the N64 piece resumes where it paused. */
/* @replaces 0x10002F10 BrCdResume */
u32 n_BrCdResume(void)
{
    W_TRACE("n_BrCdResume");
    if (GI(G_ENABLED) == 1) {
        u32 r;
        M.advancing = 1;
        r = w_icall__i(F_TRACKRESUME);
        M.advancing = 0;
        return r;
    }
    if (active()) resume();
    return 1;
}

/* WHY: MCI_STOP / EAR ClearChannel. */
/* @replaces 0x10002F70 BrCdStop */
u32 n_BrCdStop(void)
{
    W_TRACE("n_BrCdStop");
    if (active()) stop();
    return 1;
}

/* WHY: MCI_STOP + MCI_CLOSE / EAR ClearChannel on the last release. The
 * open count and the EAR route's fade release are the game's. */
/* @replaces 0x10003030 BrCdStopRelease */
u32 n_BrCdStopRelease(void)
{
    int mode = GI(G_ENABLED);
    W_TRACE("n_BrCdStopRelease");
    if (!mode) return 1;
    GS(G_PLAYING, GI(G_PLAYING) - 1);
    if (mode == 1 || GI(G_MEDIAOK)) stop();
    if (mode != 1 && GI(G_PLAYING) == 0) w_icall__i(F_FADERELEASE);
    return 1;
}
