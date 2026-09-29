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
 *   PC   the disc's CD audio, track NN.flac, streamed by an AVAudioPlayerNode
 *   N64  Top Gear Rally's modules, played live by libopenmpt in an
 *        AVAudioSourceNode, looping forever as on the N64. The N64 game's
 *        own cues pick the module (extract_modules.py): its title module
 *        for track 2, and for a race track (3 up) one of the five per-track
 *        race modules, (track - 3) mod 5.
 *
 * The player chooses the soundtrack in the Music menu; the choice is
 * remembered, a soundtrack whose files are missing is disabled there, and
 * switching mid-track restarts the current cue in the other soundtrack.
 * Levels: the modules are rendered 4.0 dB down, which puts their mean
 * integrated loudness (-16.2 LUFS) within 0.6 dB of the CD tracks' (-15.6)
 * and keeps the loudest module's +4.0 dBFS peak below full scale
 * (measured 2026-09-29, ffmpeg ebur128).
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
 * BR_MUSICWAV=path records the music output to a file; BR_MUSICSWAP=s
 * picks the other soundtrack through the Music menu's action s seconds
 * after the first track starts (checks the switch without a click).
 */
#import <AVFoundation/AVFoundation.h>
#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#include <libopenmpt/libopenmpt.h>
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
/* game functions called back */
#define F_TRACKRESUME   0x10002E80u   /* BrCdTrackResume */
#define F_NEXTWRAP      0x10002CF0u   /* BrCdTrackNextWrap */
#define F_MCINOTIFY     0x10002830u   /* the MM_MCINOTIFY handler's play */
#define F_FADERELEASE   0x10017F10u   /* BrFadeRelease */
#define F_CLOCK         0x1006E280u   /* BrSub10075020, the ms clock */
#define F_TRACKPLAY     0x10002AF0u   /* BrCdTrackPlay, the mode dispatch */
#define F_STARTUP       0x10002910u   /* BrCdStartup, the EAR route's open */

#define GI(a) ((s32)W_LD(u32, (a), 0))
#define GS(a, v) W_ST(u32, (a), 0, (u32)(v))

void h_srand(u32 s);
NSWindow *happ_window(void);

#define RATE 44100
#define N64_GAIN_MB (-400)            /* millibel, see the header */

enum { ST_PC, ST_N64 };

static struct {
    int init, silent;
    int have[2];
    int cd_last;                      /* highest CD track number on disc */
    NSString *dir[2];
    NSString *title;
    NSArray<NSString *> *race;
    int soundtrack;
    int track;                        /* the CD track number playing, 0 none */
    int paused;
    float volume;
    AVAudioEngine *engine;
    AVAudioPlayerNode *cd;
    AVAudioSourceNode *mod_node;
    AVAudioUnitEffect *limiter;
    atomic_uint gen, ended;
    os_unfair_lock lock;              /* guards mod against the render thread */
    openmpt_module *mod;
    NSMenuItem *item[2];
} M = { .lock = OS_UNFAIR_LOCK_INIT, .volume = 1.0f };

static void play(int track);
u32 n_FUN_10002980(u32 hwnd);
u32 n_FUN_100027e0(void);

@interface BRMusicMenu : NSObject
@end
@implementation BRMusicMenu
- (void)choose:(NSMenuItem *)it
{
    int st = (int)it.tag;
    if (st == M.soundtrack || !M.have[st]) return;
    M.soundtrack = st;
    [[NSUserDefaults standardUserDefaults] setObject:st == ST_N64 ? @"n64" : @"pc" forKey:@"Soundtrack"];
    M.item[ST_PC].state = st == ST_PC ? NSControlStateValueOn : NSControlStateValueOff;
    M.item[ST_N64].state = st == ST_N64 ? NSControlStateValueOn : NSControlStateValueOff;
    if (M.track) {
        int was = M.paused;
        play(M.track);
        if (was) { M.paused = 1; [M.cd pause]; }
    }
}
- (BOOL)validateMenuItem:(NSMenuItem *)it { return M.have[it.tag]; }
@end

static NSString *music_root(void)
{
    NSString *res = [[NSBundle mainBundle] resourcePath];
    NSString *p = [res stringByAppendingPathComponent:@"music"];
    const char *e = getenv("BR_MUSIC_DIR");
    if (res && [[NSFileManager defaultManager] fileExistsAtPath:p]) return p;
    if (e) return @(e);
    return [@(getenv("BR_ROOT") ? getenv("BR_ROOT") : ".") stringByAppendingPathComponent:@"build/app/extract/music"];
}

static void menu_init(void)
{
    static BRMusicMenu *target;
    NSMenu *bar = NSApp.mainMenu, *m;
    NSMenuItem *top;
    int i;
    if (!bar) return;
    target = [BRMusicMenu new];
    m = [[NSMenu alloc] initWithTitle:@"Music"];
    M.item[ST_PC] = [m addItemWithTitle:@"PC Soundtrack (CD Audio)" action:@selector(choose:) keyEquivalent:@""];
    M.item[ST_N64] = [m addItemWithTitle:@"N64 Soundtrack (Top Gear Rally)" action:@selector(choose:) keyEquivalent:@""];
    for (i = 0; i < 2; i++) {
        M.item[i].tag = i;
        M.item[i].target = target;
        M.item[i].state = i == M.soundtrack ? NSControlStateValueOn : NSControlStateValueOff;
    }
    top = [bar addItemWithTitle:@"Music" action:nil keyEquivalent:@""];
    top.submenu = m;
    HLOG("music: menu bar:");
    for (NSMenuItem *t in bar.itemArray)
        HLOG(" [%s]", t.submenu.title.UTF8String);
    HLOG("\n");
}

static void music_init(void)
{
    NSFileManager *fm = [NSFileManager defaultManager];
    NSString *root, *pref;
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
    }
    pref = [[NSUserDefaults standardUserDefaults] stringForKey:@"Soundtrack"];
    M.soundtrack = [pref isEqualToString:@"n64"] ? ST_N64 : ST_PC;
    if (!M.have[M.soundtrack]) M.soundtrack = !M.soundtrack;
    fprintf(stderr, "music: %s; PC %s (tracks 2-%d), N64 %s; playing %s\n", root.UTF8String,
            M.have[ST_PC] ? "yes" : "no", M.cd_last, M.have[ST_N64] ? "yes" : "no",
            M.soundtrack == ST_PC ? "PC" : "N64");

    M.silent = !happ_window() || (e && !atoi(e)) || !(M.have[0] || M.have[1]);
    if (M.silent) return;
    menu_init();
    M.engine = [AVAudioEngine new];
    M.cd = [AVAudioPlayerNode new];
    [M.engine attachNode:M.cd];
    [M.engine connect:M.cd to:M.engine.mainMixerNode
               format:[[AVAudioFormat alloc] initStandardFormatWithSampleRate:RATE channels:2]];
    M.mod_node = [[AVAudioSourceNode alloc] initWithFormat:
                    [[AVAudioFormat alloc] initStandardFormatWithSampleRate:RATE channels:2]
        renderBlock:^OSStatus(BOOL *silence, const AudioTimeStamp *ts, AVAudioFrameCount n, AudioBufferList *abl) {
            float *l = abl->mBuffers[0].mData, *r = abl->mNumberBuffers > 1 ? abl->mBuffers[1].mData : NULL;
            size_t got = 0;
            (void)ts;
            if (os_unfair_lock_trylock(&M.lock)) {
                if (M.mod && !M.paused && r)
                    got = openmpt_module_read_float_stereo(M.mod, RATE, n, l, r);
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
    [M.engine connect:M.mod_node to:M.engine.mainMixerNode
               format:[[AVAudioFormat alloc] initStandardFormatWithSampleRate:RATE channels:2]];
    M.engine.mainMixerNode.outputVolume = M.volume;
    {
        /* The disc is mastered to full scale, and resampling it to the
         * device rate overshoots (+0.9 dBFS on track 2 at 48 kHz, measured
         * 2026-09-29): the limiter catches those overs instead of the
         * output clipping them. Below full scale it leaves the signal as
         * it is. */
        AudioComponentDescription d = { kAudioUnitType_Effect, kAudioUnitSubType_PeakLimiter,
                                        kAudioUnitManufacturer_Apple, 0, 0 };
        /* at the device's rate, so the conversion happens before the limit */
        AVAudioFormat *out = [[AVAudioFormat alloc] initStandardFormatWithSampleRate:
                                [M.engine.outputNode outputFormatForBus:0].sampleRate channels:2];
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

static void stop(void)
{
    atomic_fetch_add(&M.gen, 1);
    M.track = 0;
    M.paused = 0;
    if (M.silent) return;
    [M.cd stop];
    mod_set(NULL);
}

static void play_cd(int track)
{
    NSString *p = [M.dir[ST_PC] stringByAppendingPathComponent:
                   [NSString stringWithFormat:@"track%02d.flac", track]];
    NSError *err = nil;
    AVAudioFile *f = [[AVAudioFile alloc] initForReading:[NSURL fileURLWithPath:p] error:&err];
    unsigned g = atomic_load(&M.gen);
    if (!f) { fprintf(stderr, "music: %s: %s\n", p.UTF8String, err.localizedDescription.UTF8String); return; }
    [M.cd scheduleFile:f atTime:nil completionCallbackType:AVAudioPlayerNodeCompletionDataPlayedBack
     completionHandler:^(AVAudioPlayerNodeCompletionCallbackType t) {
        (void)t;
        if (atomic_load(&M.gen) == g) atomic_store(&M.ended, g);
    }];
    [M.cd play];
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

static void play(int track)
{
    stop();
    M.track = track;
    HLOG("music: play %d (%s)\n", track, M.soundtrack == ST_PC ? "PC" : "N64");
    if (M.silent) return;
    if (M.soundtrack == ST_PC) play_cd(track);
    else play_mod(track);
}

static void pause_(void)
{
    if (!M.track || M.paused) return;
    M.paused = 1;
    if (!M.silent) [M.cd pause];
}

static void resume(void)
{
    if (!M.track || !M.paused) return;
    M.paused = 0;
    if (!M.silent && M.soundtrack == ST_PC) [M.cd play];
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
 * messages would have been. */
void nmusic_poll(void)
{
    static double swap_at = -1;
    unsigned e;
    if (swap_at < 0 && M.track && M.item[0])
        swap_at = getenv("BR_MUSICSWAP") ? CACurrentMediaTime() + atof(getenv("BR_MUSICSWAP")) : 0;
    if (swap_at > 0 && CACurrentMediaTime() >= swap_at) {
        NSMenuItem *it = M.item[!M.soundtrack];
        swap_at = 0;
        HLOG("music: menu picks %s\n", it.title.UTF8String);
        [NSApp sendAction:it.action to:it.target from:it];
    }
    e = atomic_exchange(&M.ended, 0);
    if (!e || e != atomic_load(&M.gen) || !M.track) return;
    HLOG("music: track %d ended\n", M.track);
    if (GI(G_ENABLED) == 1) {
        /* MCI: the drive plays on through the disc, then notifies */
        if (M.track < GI(G_LAST)) play(M.track + 1);
        else { M.track = 0; w_icall__i(F_MCINOTIFY); }
    } else {
        M.track = 0;
        w_icall__i(F_NEXTWRAP);
    }
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
 * game replays the current track itself (BrCdTrackResume), as it did. */
/* @replaces 0x10002F10 BrCdResume */
u32 n_BrCdResume(void)
{
    W_TRACE("n_BrCdResume");
    if (GI(G_ENABLED) == 1) return w_icall__i(F_TRACKRESUME);
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
