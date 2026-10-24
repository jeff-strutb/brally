/* thread.c: the game's threads, message queues, events, timers and clock
 * (tgr_os.h), on the host's threads and the host's clock.
 *
 * Every game thread is a host thread of its own and runs whenever it can,
 * as many at once as the host has cores; priorities are kept but schedule
 * nothing.  The clock is real time: tgr_clock counts the N64's 46.875 MHz
 * ticks since the start.  One clock thread wakes for what falls due: the
 * retrace, 60 times a second (the controllers sampled, the finished frame
 * shown, the retrace queue and event posted), and the timers the game sets.
 * Message queues keep the layout the game gave them, in game memory; one
 * lock guards them, and a thread waiting on one sleeps until it changes.
 * While the window is in the background the clock stands still. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "host.h"
#include "plat.h"
#include "checkrun.h"

#define MAXT 16
#define EV_MAX 16
#define MAXTIMERS 16
#define TICKS_HZ 46875000ull

typedef struct TThread {
    TgrThread *os;
    int id, pri;
    void (*entry)(void *);
    void *arg;
    host_thread *ht;
    int started;
} TThread;

typedef struct TTimer {
    int live;
    uint64_t due, interval;         /* ticks */
    TgrMsgQueue *mq;
    TgrMsg msg;
} TTimer;

static host_mutex *G;               /* the queues, events, timers, threads */
static host_cond *s_changed;        /* a queue changed, or the run ended */
static host_cond *s_wake;           /* the clock thread: a timer was set */
static TThread s_threads[MAXT];
static int s_nthreads;
static TTimer s_timers[MAXTIMERS];
static struct { TgrMsgQueue *mq; TgrMsg msg; } s_events[EV_MAX];
static TgrMsgQueue *s_vi_mq;
static TgrMsg s_vi_msg;
static uint32_t s_vi_every;
static volatile uint32_t s_frame;
static uint64_t s_t0;               /* host ns at tick 0, moved on while paused */
static int s_finished;

int tgr_check;                     /* TGR_CHECK=1: one thread at a time on a counted clock (checkrun.c) */
volatile int tgr_paused;            /* set by the host loop while the window is not in front */

static uint64_t now_ticks(void)
{
    uint64_t ns = host_ticks_ns() - s_t0;
    return ns / 1000000000ull * TICKS_HZ + ns % 1000000000ull * TICKS_HZ / 1000000000ull;
}

uint64_t tgr_count(void) { return tgr_check ? chk_count() : now_ticks(); }
uint32_t tgr_frame(void) { return tgr_check ? chk_frame() : s_frame; }

/* TGR_STATS: once a second of retraces, how they kept time and what reached
 * the window: retraces and frames finished, the worst gap between retraces,
 * the time spent presenting, and the audio the host has buffered */
extern uint32_t tgr_rcp_frames;
extern uint64_t tgr_rcp_end_ns, tgr_rcp_end_max_ns;
/* frames the screen actually showed, counted by a renderer that is told
 * (render/metal: the drawable's presented time); -1 by one that is not */
volatile int tgr_frames_shown = -1;
int tgr_audio_buffered_ms(void);

static void stats(void)
{
    static int on = -1;
    static uint64_t t0, last, worst;
    static uint32_t f0, r0;
    static int s0;
    uint64_t now;
    if (on < 0)
        on = getenv("TGR_STATS") != NULL;
    if (!on)
        return;
    now = host_ticks_ns();
    if (last && now - last > worst)
        worst = now - last;
    last = now;
    if (!t0) {
        t0 = now;
        f0 = s_frame;
        r0 = tgr_rcp_frames;
        s0 = tgr_frames_shown;
        return;
    }
    if (s_frame - f0 >= 60) {
        uint32_t frames = tgr_rcp_frames - r0;
        int shown = tgr_frames_shown;
        char sh[32] = "";
        if (shown >= 0)
            snprintf(sh, sizeof sh, " (%d shown)", shown - s0);
        fprintf(stderr, "stats: %u retraces in %.3f s, %u frames%s, worst gap %.1f ms, present %.2f ms avg %.1f max, audio %d ms\n",
                s_frame - f0, (now - t0) / 1e9, frames, sh, worst / 1e6,
                frames ? tgr_rcp_end_ns / 1e6 / frames : 0.0, tgr_rcp_end_max_ns / 1e6, tgr_audio_buffered_ms());
        s0 = shown;
        t0 = now;
        f0 = s_frame;
        r0 = tgr_rcp_frames;
        worst = 0;
        tgr_rcp_end_ns = tgr_rcp_end_max_ns = 0;
    }
}

void tgr_os_lock(void)   { if (tgr_check) chk_lock(); else host_mutex_lock(G); }
void tgr_os_unlock(void) { if (tgr_check) chk_unlock(); else host_mutex_unlock(G); }

static TThread *find(TgrThread *t)
{
    int i;
    for (i = 0; i < s_nthreads; i++)
        if (s_threads[i].os == t)
            return &s_threads[i];
    return NULL;
}

/* ---- message queues: the game's layout, in game memory (the caller holds G) ---- */
static int mq_send(TgrMsgQueue *mq, TgrMsg msg, int jam)
{
    TgrMsg *buf = mq->msg;
    if (mq->validCount >= mq->msgCount)
        return 0;
    if (jam) {
        mq->first = (mq->first + mq->msgCount - 1) % mq->msgCount;
        buf[mq->first] = msg;
    } else {
        buf[(mq->first + mq->validCount) % mq->msgCount] = msg;
    }
    mq->validCount++;
    host_cond_broadcast(s_changed);
    return 1;
}

static int mq_recv(TgrMsgQueue *mq, TgrMsg *out)
{
    TgrMsg *buf = mq->msg;
    if (mq->validCount == 0)
        return 0;
    if (out)
        *out = buf[mq->first];
    mq->first = (mq->first + 1) % mq->msgCount;
    mq->validCount--;
    host_cond_broadcast(s_changed);
    return 1;
}

/* an event happening now: its queue gets its message (a full queue drops
 * it, as the N64's did); the caller holds G */
void tgr_event(int ev)
{
    if (tgr_check) {
        chk_event(ev);
        return;
    }
    if (ev >= 0 && ev < EV_MAX && s_events[ev].mq)
        mq_send(s_events[ev].mq, s_events[ev].msg, 0);
}

/* what the platform delivers when it has done the work: now, in order */
void tgr_post_event(int event)
{
    if (tgr_check) {
        chk_post_event(event);
        return;
    }
    host_mutex_lock(G);
    tgr_event(event);
    host_mutex_unlock(G);
}

void tgr_post_mesg(TgrMsgQueue *mq, TgrMsg msg)
{
    if (tgr_check) {
        chk_post_mesg(mq, msg);
        return;
    }
    host_mutex_lock(G);
    mq_send(mq, msg, 0);
    host_mutex_unlock(G);
}

/* ---- the clock thread: retraces and timers ----------------------------------- */
static void retrace(void)
{
    s_frame++;
    tgr_vi_retrace();                           /* the controllers sampled, the frame shown */
    stats();
    host_mutex_lock(G);
    if (g_tgr.frames && s_frame >= (uint32_t)g_tgr.frames) {
        s_finished = 1;
        host_cond_broadcast(s_changed);
        host_mutex_unlock(G);
        return;
    }
    if (s_vi_mq && s_frame % (s_vi_every ? s_vi_every : 1) == 0)
        mq_send(s_vi_mq, s_vi_msg, 0);
    tgr_event(TGR_EVENT_VI);
    host_mutex_unlock(G);
}

static void *clock_main(void *v)
{
    (void)v;
    host_thread_interactive();
    host_mutex_lock(G);
    while (!s_finished) {
        uint64_t next_vi = (uint64_t)(s_frame + 1) * TICKS_HZ / 60, next = next_vi, now;
        int i;
        if (tgr_paused) {
            /* the window is in the background: the N64's time stands still */
            uint64_t at = host_ticks_ns();
            host_mutex_unlock(G);
            while (tgr_paused && !s_finished)
                host_sleep_ms(10);
            host_mutex_lock(G);
            s_t0 += host_ticks_ns() - at;
            continue;
        }
        for (i = 0; i < MAXTIMERS; i++)
            if (s_timers[i].live && s_timers[i].due < next)
                next = s_timers[i].due;
        now = now_ticks();
        if (now > next_vi + TICKS_HZ / 5) {
            /* fell behind by more than 200 ms (the host stalled): the N64's
               time stood still meanwhile, rather than racing to catch up */
            s_t0 += (now - next_vi) * 1000000000ull / TICKS_HZ;
            continue;
        }
        if (now < next) {
            uint32_t ms = (uint32_t)((next - now) * 1000ull / TICKS_HZ);
            if (ms > 1)
                host_cond_wait(s_wake, G, ms - 1);       /* to within a millisecond, */
            else {
                host_mutex_unlock(G);
                while (now_ticks() < next)               /* then waited out exactly */
                    ;
                host_mutex_lock(G);
            }
            continue;
        }
        for (i = 0; i < MAXTIMERS; i++) {
            TTimer *t = &s_timers[i];
            if (t->live && t->due <= now) {
                mq_send(t->mq, t->msg, 0);
                if (t->interval)
                    t->due += t->interval;
                else
                    t->live = 0;
            }
        }
        if (now >= next_vi) {
            host_mutex_unlock(G);
            retrace();
            host_mutex_lock(G);
        }
    }
    host_mutex_unlock(G);
    return NULL;
}

/* ---- the calls ---------------------------------------------------------------- */
/* the only part of libultra's start-up the game can see: the clock's rate is
 * the count's, 3/4 of the CPU's (initialize.c) */
void tgr_os_init(void)
{
    extern uint64_t tgr_clock_rate;
    tgr_clock_rate = tgr_clock_rate * 3 / 4;
}

static void *thread_main(void *v)
{
    TThread *t = (TThread *)v;
    host_thread_interactive();
    t->entry(t->arg);
    fprintf(stderr, "tgr: thread %d returned from its entry function\n", t->id);
    abort();
}

void tgr_thread_new(TgrThread *t, TgrId id, void (*entry)(void *), void *arg, void *sp, TgrPri pri)
{
    TThread *th;
    (void)sp;                       /* (the N64's stack: the host thread brings its own) */
    if (tgr_check) {
        chk_thread_new(t, id, entry, arg, pri);
        return;
    }
    host_mutex_lock(G);
    th = find(t);
    if (!th) {
        if (s_nthreads == MAXT) {
            fprintf(stderr, "tgr: too many threads\n");
            abort();
        }
        th = &s_threads[s_nthreads++];
        memset(th, 0, sizeof *th);
    }
    th->os = t;
    th->id = id;
    th->pri = pri;
    th->entry = entry;
    th->arg = arg;
    host_mutex_unlock(G);
}

void tgr_thread_start(TgrThread *t)
{
    TThread *th;
    if (tgr_check) {
        chk_thread_start(t);
        return;
    }
    host_mutex_lock(G);
    th = find(t);
    if (th && !th->started) {
        th->started = 1;
        th->ht = host_thread_start(thread_main, th);
    }
    host_mutex_unlock(G);
}

void tgr_thread_pri(TgrThread *t, TgrPri pri)
{
    TThread *th;
    if (tgr_check) {
        chk_thread_pri(t, pri);
        return;
    }
    host_mutex_lock(G);
    th = t ? find(t) : NULL;
    if (th)
        th->pri = pri;
    host_mutex_unlock(G);
}

/* the idle thread's rest: the N64's spun when nothing else could run */
void tgr_thread_idle(void)
{
    for (;;)
        host_sleep_ms(1000000);
}

void tgr_mq_init(TgrMsgQueue *mq, TgrMsg *msg, int32_t count)
{
    mq->mtqueue = NULL;
    mq->fullqueue = NULL;
    mq->validCount = 0;
    mq->first = 0;
    mq->msgCount = count;
    mq->msg = msg;
}

static int32_t send(TgrMsgQueue *mq, TgrMsg msg, int32_t flag, int jam)
{
    host_mutex_lock(G);
    while (!mq_send(mq, msg, jam)) {
        if (flag != TGR_BLOCK || s_finished) {
            host_mutex_unlock(G);
            return -1;
        }
        host_cond_wait(s_changed, G, 0xFFFFFFFFu);
    }
    host_mutex_unlock(G);
    return 0;
}

int32_t tgr_mq_send(TgrMsgQueue *mq, TgrMsg msg, int32_t flag) { return tgr_check ? chk_mq_send(mq, msg, flag) : send(mq, msg, flag, 0); }
int32_t tgr_mq_jam(TgrMsgQueue *mq, TgrMsg msg, int32_t flag)  { return tgr_check ? chk_mq_jam(mq, msg, flag) : send(mq, msg, flag, 1); }

int32_t tgr_mq_recv(TgrMsgQueue *mq, TgrMsg *msg, int32_t flag)
{
    if (tgr_check)
        return chk_mq_recv(mq, msg, flag);
    host_mutex_lock(G);
    while (!mq_recv(mq, msg)) {
        if (flag != TGR_BLOCK) {
            host_mutex_unlock(G);
            return -1;
        }
        while (s_finished)          /* the run is over: the game's threads stop here */
            host_cond_wait(s_changed, G, 0xFFFFFFFFu);
        host_cond_wait(s_changed, G, 0xFFFFFFFFu);
    }
    host_mutex_unlock(G);
    return 0;
}

void tgr_event_mq(TgrEvent e, TgrMsgQueue *mq, TgrMsg msg)
{
    if (tgr_check) {
        chk_event_mq(e, mq, msg);
        return;
    }
    host_mutex_lock(G);
    if (e < EV_MAX) {
        s_events[e].mq = mq;
        s_events[e].msg = msg;
    }
    host_mutex_unlock(G);
}

void tgr_retrace_mq(TgrMsgQueue *mq, TgrMsg msg, uint32_t retraceCount)
{
    if (tgr_check) {
        chk_retrace_mq(mq, msg, retraceCount);
        return;
    }
    host_mutex_lock(G);
    s_vi_mq = mq;
    s_vi_msg = msg;
    s_vi_every = retraceCount;
    host_mutex_unlock(G);
}

uint32_t tgr_clock(void) { return (uint32_t)tgr_count(); }

int32_t tgr_timer(TgrTimer *t, TgrTime countdown, TgrTime interval, TgrMsgQueue *mq, TgrMsg msg)
{
    int i;
    (void)t;
    if (tgr_check) {
        chk_timer(countdown, interval, mq, msg);
        return 0;
    }
    host_mutex_lock(G);
    for (i = 0; i < MAXTIMERS; i++)
        if (!s_timers[i].live)
            break;
    if (i == MAXTIMERS) {
        fprintf(stderr, "tgr: too many timers\n");
        abort();
    }
    s_timers[i].live = 1;
    s_timers[i].due = now_ticks() + (countdown > 0 ? (uint64_t)countdown : 1);
    s_timers[i].interval = (uint64_t)interval;
    s_timers[i].mq = mq;
    s_timers[i].msg = msg;
    host_cond_broadcast(s_wake);
    host_mutex_unlock(G);
    return 0;
}

TgrAddr tgr_faulted_thread(void) { return 0; }

/* ---- the run ------------------------------------------------------------------- */
static void (*s_boot)(void);

static void *boot_main(void *v)
{
    (void)v;
    s_boot();                       /* starts the idle thread, which starts the rest */
    return NULL;
}

void tgr_os_start(void (*boot)(void))
{
    const char *c = getenv("TGR_CHECK");
    if (c && atoi(c)) {
        tgr_check = 1;
        chk_start(boot);
        return;
    }
    G = host_mutex_new();
    s_changed = host_cond_new();
    s_wake = host_cond_new();
    s_boot = boot;
    s_t0 = host_ticks_ns();
    host_thread_start(clock_main, NULL);
    host_thread_start(boot_main, NULL);
}

/* 1 once the run is over (--frames reached) */
int tgr_os_finished(void)
{
    return tgr_check ? chk_finished() : s_finished;
}

void tgr_os_wait(void)
{
    if (tgr_check) {
        chk_wait();
        return;
    }
    host_mutex_lock(G);
    while (!s_finished)
        host_cond_wait(s_changed, G, 0xFFFFFFFFu);
    host_mutex_unlock(G);
}
