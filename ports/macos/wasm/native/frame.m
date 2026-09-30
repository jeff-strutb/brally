/* frame.m -- the port-owned frame loop (ports/macos/NATIVE_RENDERER.md 4.1).
 *
 * The game presents every frame through one swap and times its race through
 * one millisecond clock.  Both are replaced here:
 *
 *   swap   after the frame is submitted, wait until `lead` before the
 *          compositor's latch for the next refresh this loop aims at, then
 *          return: the game reads input, runs the sim ticks that are due,
 *          renders and swaps again, all before that latch.  A frame
 *          submitted between refreshes R0 and R1 is shown at R2 (measured
 *          2026-09-29), so input-to-screen is lead + one refresh.
 *   clock  the presentation time of the frame being built (latch plus the
 *          pipeline depth the loop has learned).  The race
 *          runs its 30 Hz ticks and blends its snapshots (BrSnapInterpDraw,
 *          t = ms * 0.03) by this clock, so each picture shows the game
 *          exactly at the moment it reaches the screen.
 *
 * Refresh times come from a display link on its own thread.  Presented
 * times (exact, but delivered late on the main run loop) only teach the
 * loop.  A frame shown later than aimed is one of two things:
 *   - one more frame queued: skip one refresh to drain it and widen the
 *     lead by 1 ms; after a long clean stretch narrow it again, never
 *     below 3 ms;
 *   - WindowServer compositing with a deeper pipeline (measured 2026-09-29
 *     while a video call was on screen: every frame, submitted anywhere
 *     from 0 to 13 ms before its latch, was shown 2 refreshes after it,
 *     skips or not).  Skipping cannot help there; if frames are still late
 *     after a skip, the loop takes the new depth as the target instead,
 *     and returns to the shallower one once frames arrive early again.
 *
 * Frame rate: the race is drawn at the display's full refresh rate (120 Hz
 * on a ProMotion screen), its 30 Hz simulation untouched: every frame
 * blends the two newest snapshots at the moment it will be shown
 * (BrSnapInterpDraw, wrapped here so the blend has the time to a fraction of
 * a millisecond, not the game clock's whole milliseconds).  Menus, which
 * count frames, stay at 60.  With vertical sync off (View > Vertical Sync,
 * remembered; BR_VSYNC=0) the race draws as fast as the GPU allows, each
 * frame shown at once.
 *
 * Off (the swap and the clock behave exactly as the game's own) when
 * headless, under BR_VCLOCK (deterministic runs), or with BR_PACE=0.
 *
 * BR_FRAMELOG=path: one line per presented frame, for framelog.py (section 6
 * of the spec): seq, frame start, submit, GPU complete, presented, latch,
 * target, lead (ms since the log opened), the game clock at frame start and
 * flags (o = the frame started late for its refresh, s = a refresh skipped
 * to drain a miss before it).
 */
#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>
#include <mach/mach_time.h>
#include <os/lock.h>
#include <pthread.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "w2c_native.h"

u32 h_grBufferNumPending(void);
void h_grBufferSwap(u32 interval);
void happ_pump(int block_ms);
NSWindow *happ_window(void);
CAMetalLayer *happ_metal_layer(void);
u64 hwin_us_at_mach(u64 m);

void hglide_mailbox_present(void);
int hframe_mailbox(void);

#define LEAD_START   0.0055
#define LEAD_MIN     0.003
#define LEAD_MAX     0.012
#define CLEAN_NARROW 1800              /* clean frames before the lead narrows */
#define RING         256

/* the snapshot blend's state (br_snapinterp.c) */
#define SNAP_ORIGIN  0x10396F24u        /* float: the blend fraction at the last reset */
#define SNAP_T0      0x104AB4F4u        /* int: the clock at the last reset */

/* the game clock's globals (br_timer.c, 0x10075020) */
#define G_QPC_FREQ   0x118EE238u        /* __int64 frequency */
#define G_QPC_OK     0x118EE240u        /* QueryPerformanceFrequency result */
#define G_QPC_BASE   0x118EE248u        /* ms at the first read */

typedef struct {
    u64 seq;
    double start, submit, gpu, presented, latch, target, lead;
    s32 clock;
    char flags[4];
    int open;                           /* started, not yet written */
} frec;

static struct {
    int init, active, vsync;
    int race;                           /* a race frame was drawn since the last swap */
    double race_until;                  /* ... or this recently: the race's odd frame
                                           that is not a blend (a pause, the start)
                                           must not fall back to the menus' 60 */
    double last_start;                  /* unsynced: when the last frame started */
    double clock_ms;                    /* the game clock's last value, to the fraction */
    double lead, latch, target;
    u64 seq, recover_after;
    unsigned clean, early;
    int skip, tried_skip;
    int depth;                          /* refreshes from latch to shown */
    u64 present_us;                     /* target presentation, host clock */
    FILE *log;
    double log0;
    unsigned misses, overruns;
    frec ring[RING];
} F;

/* ------------------------------------------------ refresh times (link) -- */
static os_unfair_lock g_vl = OS_UNFAIR_LOCK_INIT;
static double g_vts, g_vperiod;         /* last refresh, refresh period (s) */

@interface BRFrameLink : NSObject
- (void)tick:(CADisplayLink *)l;
@end
@implementation BRFrameLink
- (void)tick:(CADisplayLink *)l
{
    double ts = l.timestamp, p = l.targetTimestamp - l.timestamp;
    os_unfair_lock_lock(&g_vl);
    g_vts = ts;
    if (p > 0.002 && p < 0.1) g_vperiod = p;
    os_unfair_lock_unlock(&g_vl);
    if (hframe_mailbox()) hglide_mailbox_present();   /* vertical sync off: the newest picture */
}
@end

static int grid(double *ts, double *p)
{
    os_unfair_lock_lock(&g_vl);
    *ts = g_vts;
    *p = g_vperiod;
    os_unfair_lock_unlock(&g_vl);
    return *p > 0;
}

static void start_link(void)
{
    static BRFrameLink *target;
    static CADisplayLink *link;
    NSWindow *w = happ_window();
    NSThread *t;
    if (!w) return;
    target = [BRFrameLink new];
    link = [w displayLinkWithTarget:target selector:@selector(tick:)];
    {   /* every refresh the display has: 120 a second on ProMotion */
        float mx = (float)(w.screen ? w.screen.maximumFramesPerSecond : 60);
        if (mx < 60) mx = 60;
        link.preferredFrameRateRange = CAFrameRateRangeMake(60, mx, mx);
    }
    t = [[NSThread alloc] initWithBlock:^{
        [link addToRunLoop:NSRunLoop.currentRunLoop forMode:NSRunLoopCommonModes];
        for (;;) [NSRunLoop.currentRunLoop run];
    }];
    t.name = @"frame link";
    t.qualityOfService = NSQualityOfServiceUserInteractive;
    [t start];
}

/* ------------------------------------------------------------- time -- */
static mach_timebase_info_data_t g_tb;
static u64 mach_at(double s)            /* CACurrentMediaTime domain -> mach */
{
    if (!g_tb.denom) mach_timebase_info(&g_tb);
    return (u64)(s * 1e9 * g_tb.denom / g_tb.numer);
}

/* the game clock's value at host-clock microsecond us: the same arithmetic
 * br_timer.c does on QueryPerformanceCounter, which host_win.c answers with
 * us + 1 s */
static int game_ms_at(u64 us, s32 *out)
{
    s64 freq = (s64)W_LD(u64, G_QPC_FREQ, 0);
    if (!W_LD(s32, G_QPC_OK, 0) || freq <= 0) return 0;
    *out = (s32)(((s64)(us + 1000000ULL) * 1000 + 500) / freq) - W_LD(s32, G_QPC_BASE, 0);
    return 1;
}

/* the same, to the fraction of a millisecond */
static double game_ms_exact(u64 us)
{
    s64 freq = (s64)W_LD(u64, G_QPC_FREQ, 0);
    if (!W_LD(s32, G_QPC_OK, 0) || freq <= 0) return 0;
    return ((double)(us + 1000000ULL) * 1000.0 + 500.0) / (double)freq - W_LD(s32, G_QPC_BASE, 0);
}

/* --------------------------------------------------------------- log -- */
static void log_write(frec *r)
{
    double z = F.log0;
#define MS(x) ((x) > 0 ? ((x) - z) * 1000.0 : -1.0)
    if (F.log)
        fprintf(F.log, "%llu %.3f %.3f %.3f %.3f %.3f %.3f %.3f %d %s\n",
                (unsigned long long)r->seq, MS(r->start), MS(r->submit), MS(r->gpu),
                MS(r->presented), MS(r->latch), MS(r->target), r->lead * 1000.0,
                r->clock, r->flags[0] ? r->flags : "-");
#undef MS
    r->open = 0;
}

static void frame_presented(u64 seq, double t)
{
    frec *r = &F.ring[seq % RING];
    double ts, p;
    if (r->seq != seq || !r->open) return;
    r->presented = t;
    log_write(r);
    if (!F.active || seq <= F.recover_after || r->target <= 0 || !grid(&ts, &p)) return;
    /* a window nobody can see (behind a full-screen app, on another Space,
     * minimised) is not composited, and its frames report no presentation:
     * that is not a missed refresh, and skipping for it halved the rate */
    if (t <= 0 && !(happ_window().occlusionState & NSWindowOcclusionStateVisible)) return;
    if (t <= 0 || t > r->target + p / 2) {
        F.misses++;
        F.clean = F.early = 0;
        F.recover_after = F.seq;
        if (t > 0 && F.tried_skip) {
            /* still late after draining: the compositor's pipeline is deeper */
            F.depth = (int)lround((t - r->latch) / p);
            F.tried_skip = 0;
            F.lead = fmax(F.lead - 0.001, LEAD_MIN);   /* the skip's widening did not help */
        } else {
            /* one more frame queued than aimed: drain it */
            F.skip = 1;
            F.tried_skip = 1;
            F.lead = fmin(F.lead + 0.001, LEAD_MAX);
        }
    } else if (t < r->target - p / 2) {
        /* earlier than aimed: the deeper pipeline is gone */
        F.clean = 0;
        if (++F.early >= 30) {
            F.depth = (int)fmax(1, lround((t - r->latch) / p));
            F.early = 0;
            F.recover_after = F.seq;
        }
    } else {
        F.early = 0;
        F.tried_skip = 0;
        if (++F.clean >= CLEAN_NARROW) {
            F.lead = fmax(F.lead - 0.0005, LEAD_MIN);
            F.clean = 0;
        }
    }
}

/* called by host_glide.m's swap, just before it commits the frame */
void hframe_present(id<MTLCommandBuffer> cb, id<CAMetalDrawable> d)
{
    u64 seq = F.seq;
    frec *r = &F.ring[seq % RING];
    if (F.log && r->seq == seq && r->open) {
        r->submit = CACurrentMediaTime();
        [cb addCompletedHandler:^(id<MTLCommandBuffer> b) {
            (void)b;
            frec *q = &F.ring[seq % RING];
            if (q->seq == seq) q->gpu = CACurrentMediaTime();
        }];
    }
    if (r->seq == seq && r->open)
        [d addPresentedHandler:^(id<MTLDrawable> dd) { frame_presented(seq, dd.presentedTime); }];
    [cb presentDrawable:d];
}

/* ---------------------------------------------------------- the loop -- */
static void frame_init(void)
{
    const char *e = getenv("BR_FRAMELOG");
    CAMetalLayer *l = happ_metal_layer();
    F.init = 1;
    F.lead = LEAD_START;
    F.depth = 1;
    if (!l) return;                      /* headless: nothing is shown */
    if (e && (F.log = fopen(e, "w"))) {
        setvbuf(F.log, NULL, _IOLBF, 0);
        F.log0 = CACurrentMediaTime();
        fprintf(F.log, "# seq start submit gpu presented latch target lead clock flags\n");
    }
    F.active = !getenv("BR_VCLOCK") && !(getenv("BR_PACE") && !atoi(getenv("BR_PACE")));
    if (!F.active) return;
    {
        NSNumber *v = [[NSUserDefaults standardUserDefaults] objectForKey:@"VSync"];
        const char *e2 = getenv("BR_VSYNC");
        F.vsync = e2 ? atoi(e2) != 0 : v ? v.boolValue : 1;
        l.displaySyncEnabled = F.vsync ? YES : NO;
    }
    /* the loop, not the drawable pool, limits the queue: with two
     * drawables nextDrawable waits for the frame on screen to be replaced */
    l.maximumDrawableCount = 3;
    start_link();
}

static void frame_open(double start, double latch, double target, const char *flags)
{
    frec *r;
    F.seq++;
    r = &F.ring[F.seq % RING];
    if (r->open) log_write(r);           /* never reported: written as -1 */
    memset(r, 0, sizeof *r);
    r->seq = F.seq;
    r->open = 1;
    r->start = start;
    r->latch = latch;
    r->target = target;
    r->lead = F.active ? F.lead : 0;
    snprintf(r->flags, sizeof r->flags, "%s", flags);
    if (!game_ms_at(hwin_us_at_mach(mach_at(target > 0 ? target : start)), &r->clock))
        r->clock = -1;
}

/* After a frame is submitted: wait until it is time to start the next. */
static void frame_next(void)
{
    double now = CACurrentMediaTime(), ts, p, L;
    char flags[4] = "";
    int n, k = 0;
    int race = F.race || now < F.race_until;
    if (!F.init) frame_init();
    if (F.race) F.race_until = now + 0.25;
    F.race = 0;
    if (F.active && !F.vsync) {
        /* no vertical sync: the race at once, menus at 60 */
        if (!race && F.last_start > 0 && now < F.last_start + 1.0 / 60.0) {
            mach_wait_until(mach_at(F.last_start + 1.0 / 60.0));
            now = CACurrentMediaTime();
        }
        F.last_start = now;
        F.latch = 0;
        F.present_us = hwin_us_at_mach(mach_at(now));
        if (F.log) frame_open(now, 0, 0, "");
        happ_pump(0);
        return;
    }
    if (!F.active || !grid(&ts, &p)) {
        F.present_us = 0;
        if (F.log) frame_open(now, 0, 0, "");
        return;
    }
    /* menus at most 60 frames a second: the original's menus count frames,
     * and a faster display must not run them faster.  The race blends its
     * snapshots by the clock: every refresh */
    n = race ? 1 : (int)lround((1.0 / 60.0) / p);
    if (n < 1) n = 1;
    if (F.latch > 0) {
        L = F.latch + n * p;
        L = ts + round((L - ts) / p) * p;    /* stay on the display's grid */
    } else {
        L = ts + ceil((now + F.lead - ts) / p) * p;
    }
    if (F.skip) { L += p; F.skip = 0; flags[k++] = 's'; }
    if (L - F.lead < now) {
        /* the frame ran past its start: the next refresh it can make */
        L = ts + ceil((now + F.lead - ts) / p) * p;
        F.overruns++;
        flags[k++] = 'o';
    }
    mach_wait_until(mach_at(L - F.lead));
    F.latch = L;
    F.target = L + F.depth * p;
    F.present_us = hwin_us_at_mach(mach_at(F.target));
    frame_open(CACurrentMediaTime(), F.latch, F.target, flags);
    happ_pump(0);                        /* the freshest input for this frame */
}

/* ------------------------------------------------------- overrides -- */

/* WHY: every presented frame goes through here, so frame pacing is decided
 * here.  The game's own body (br_dlglide.c) waits for Glide's pending
 * buffers and swaps; this does the same, then holds the game until the
 * next frame's start. */
/* @replaces 0x1001DD50 BrGlideFlipWait */
void n_BrGlideFlipWait(void)
{
    W_TRACE("n_BrGlideFlipWait");
    while ((s32)h_grBufferNumPending() > 0)
        ;
    h_grBufferSwap(1);
    frame_next();
}

/* WHY: the race's time-sync loop (BrRaceStep 0x1001C5EE) runs its ticks and
 * BrSnapInterpDraw blends its snapshots by this clock.  While a frame is
 * being built it reads that frame's target presentation time; the game's
 * own clock otherwise, and whenever it is later (a frame that overran). */
/* @replaces 0x1006E280 BrSub10075020 */
u32 n_BrSub10075020(void)
{
    s32 v, p;
    W_TRACE("n_BrSub10075020");
    v = (s32)W_ORIG_BrSub10075020();
    F.clock_ms = v;
    if (F.present_us && game_ms_at(F.present_us, &p) && p > v) {
        v = p;
        F.clock_ms = game_ms_exact(F.present_us);
    }
    return (u32)v;
}

/* native/aspect.m: the race drew this frame (BrFrameDraw) */
void hframe_race_frame(void) { F.race = 1; }

/* The game clock's last reading, to the fraction of a millisecond: the
 * time of the frame being built (host_fx.m's effects run on it). */
double hframe_game_ms(void) { return F.clock_ms; }

int hframe_vsync(void) { return F.active ? F.vsync : 1; }
int hframe_mailbox(void) { return F.active && !F.vsync; }
/* View > Vertical Sync */
void hframe_set_vsync(int on)
{
    CAMetalLayer *l = happ_metal_layer();
    if (!F.active) return;
    F.vsync = on != 0;
    if (l) l.displaySyncEnabled = F.vsync ? YES : NO;
    F.latch = 0;
    [[NSUserDefaults standardUserDefaults] setBool:F.vsync ? YES : NO forKey:@"VSync"];
    fprintf(stderr, "vertical sync: %s\n", F.vsync ? "on" : "off");
}

/* WHY: the race blends its two newest snapshots by t = (now - t0) * 0.03 +
 * origin, with `now` the game clock in whole milliseconds.  Drawn at 60 Hz
 * that rounding moves each frame's picture by up to 3% of a tick; at 120 Hz
 * or unsynced, more, and the motion judders.  The frame's presentation time
 * is known to the microsecond (the clock above): the part of it below the
 * millisecond, since the blend's last reset, goes into origin for this
 * call, and stays there only when the call resets (draws). */
/* @replaces 0x100131E0 BrSnapInterpDraw */
u32 n_BrSnapInterpDraw(u32 force)
{
    static double fr0;                  /* the fraction at the last reset */
    double fr = 0;
    float add = 0, org = 0;
    s32 p;
    u32 r;
    W_TRACE("n_BrSnapInterpDraw");
    if (F.present_us && game_ms_at(F.present_us, &p))
        fr = game_ms_exact(F.present_us) - p;
    if (fr != fr0 && W_LD(s32, SNAP_T0, 0) != 0) {
        add = (float)((fr - fr0) * 0.03);
        org = W_LD(f32, SNAP_ORIGIN, 0);
        W_ST(f32, SNAP_ORIGIN, 0, org + add);
    }
    r = W_ORIG_BrSnapInterpDraw(force);
    if (r) { fr0 = fr; F.race = 1; }
    else if (add != 0) W_ST(f32, SNAP_ORIGIN, 0, org);
    return r;
}
