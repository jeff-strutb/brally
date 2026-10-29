/* checkrun.c: the check mode's scheduler (TGR_CHECK=1, tools/tgrcheck.sh).
 *
 * Play runs the game's threads alongside each other on the real clock
 * (thread.c), so no two runs are alike frame for frame.  A check compares
 * two builds frame for frame, so here the same calls run in one order every
 * time: exactly one game thread runs, the one holding the baton (s_cur),
 * which moves only inside an OS call to the highest-priority runnable thread
 * (ties to the thread created first).  When nothing but the idle thread
 * could run, a counted clock moves on to the next thing due: a delivery
 * (a load or a drawing task done, a timer, a controller read) or the next
 * retrace, 1/60 s of the count after the last.  Nothing waits for real
 * time, so a check runs as fast as the host can.  Never used in play. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "host.h"
#include "plat.h"
#include "checkrun.h"

#define MAXT 16
#define EV_MAX 16
#if TGR_REGION_EUR
#define TICKS_PER_FRAME 937500u     /* the count at 46.875 MHz, 1/50 s: Europe's PAL console */
#else
#define TICKS_PER_FRAME 781250u     /* the count at 46.875 MHz, 1/60 s */
#endif

typedef struct TThread {
    TgrThread *os;
    int id, pri;
    void (*entry)(void *);
    void *arg;
    enum { T_STOPPED, T_RUNNABLE, T_BLOCKED } state;
    TgrMsgQueue *wait_mq;
    int wait_full;
    host_cond *cv;
    host_thread *ht;
    int started;
} TThread;

typedef struct Pending {
    uint64_t when;
    int kind;               /* 0 event, 1 message, 2 timer */
    int event;
    TgrMsgQueue *mq;
    TgrMsg msg;
    uint64_t interval;
    uint32_t seq;
} Pending;

static host_mutex *G;
static TThread s_threads[MAXT];
static int s_nthreads;
static TThread *s_cur;
static int s_started;
static uint64_t s_count;
static uint32_t s_frame;
static struct { TgrMsgQueue *mq; TgrMsg msg; } s_events[EV_MAX];
static TgrMsgQueue *s_vi_mq;
static TgrMsg s_vi_msg;
static uint32_t s_vi_every;
static Pending s_pending[256];
static int s_npending;
static uint32_t s_seq;
static uint64_t s_vi_due = TICKS_PER_FRAME;
static host_cond *s_done;
static int s_finished;

uint64_t chk_count(void) { return s_count; }
uint32_t chk_frame(void) { return s_frame; }
void chk_lock(void)   { host_mutex_lock(G); }
void chk_unlock(void) { host_mutex_unlock(G); }

static TThread *find(TgrThread *t)
{
    int i;
    for (i = 0; i < s_nthreads; i++)
        if (s_threads[i].os == t)
            return &s_threads[i];
    return NULL;
}

static TThread *pick(void)
{
    TThread *best = NULL;
    int i;
    for (i = 0; i < s_nthreads; i++) {
        TThread *t = &s_threads[i];
        if (t->state != T_RUNNABLE)
            continue;
        if (!best || t->pri > best->pri || (t->pri == best->pri && t->id < best->id))
            best = t;
    }
    return best;
}

/* ---- message queues: the game's layout, in game memory (the caller holds G) ---- */
static int mq_send(TgrMsgQueue *mq, TgrMsg msg, int jam)
{
    TgrMsg *buf = TGR_PTR(TgrMsg *, mq->msg);
    int i;
    if (mq->validCount >= mq->msgCount)
        return 0;
    if (jam) {
        mq->first = (mq->first + mq->msgCount - 1) % mq->msgCount;
        buf[mq->first] = msg;
    } else {
        buf[(mq->first + mq->validCount) % mq->msgCount] = msg;
    }
    mq->validCount++;
    for (i = 0; i < s_nthreads; i++) {
        TThread *t = &s_threads[i];
        if (t->state == T_BLOCKED && t->wait_mq == mq && !t->wait_full) {
            t->state = T_RUNNABLE;
            t->wait_mq = NULL;
        }
    }
    return 1;
}

static int mq_recv(TgrMsgQueue *mq, TgrMsg *out)
{
    TgrMsg *buf = TGR_PTR(TgrMsg *, mq->msg);
    int i;
    if (mq->validCount == 0)
        return 0;
    if (out)
        *out = buf[mq->first];
    mq->first = (mq->first + 1) % mq->msgCount;
    mq->validCount--;
    for (i = 0; i < s_nthreads; i++) {
        TThread *t = &s_threads[i];
        if (t->state == T_BLOCKED && t->wait_mq == mq && t->wait_full) {
            t->state = T_RUNNABLE;
            t->wait_mq = NULL;
        }
    }
    return 1;
}

void chk_event(int ev)
{
    if (ev >= 0 && ev < EV_MAX && s_events[ev].mq)
        mq_send(s_events[ev].mq, s_events[ev].msg, 0);
}

/* ---- deliveries, in count order (equal counts in arrival order) ----------------- */
static void pend(Pending p)
{
    int i;
    if (s_npending == (int)(sizeof s_pending / sizeof *s_pending)) {
        fprintf(stderr, "tgr: too many pending deliveries\n");
        abort();
    }
    p.seq = s_seq++;
    i = s_npending++;
    while (i > 0 && (s_pending[i - 1].when > p.when)) {
        s_pending[i] = s_pending[i - 1];
        i--;
    }
    s_pending[i] = p;
}

/* a drawing task's two events come 1 and 2 counts on, the rest at once */
void chk_post_event(int event)
{
    Pending p;
    memset(&p, 0, sizeof p);
    host_mutex_lock(G);
    p.when = s_count + (event == TGR_EVENT_SP ? 1 : event == TGR_EVENT_DP ? 2 : 0);
    p.kind = 0;
    p.event = event;
    pend(p);
    host_mutex_unlock(G);
}

void chk_post_mesg(TgrMsgQueue *mq, TgrMsg msg)
{
    Pending p;
    memset(&p, 0, sizeof p);
    host_mutex_lock(G);
    p.when = s_count;
    p.kind = 1;
    p.mq = mq;
    p.msg = msg;
    pend(p);
    host_mutex_unlock(G);
}

/* nothing can run: deliver the next thing due (a delivery due on the
 * retrace's count goes first, then the retrace) */
static void advance_time(void)
{
    uint64_t next_vi = s_vi_due;
    if (s_npending && s_pending[0].when <= next_vi) {
        Pending p = s_pending[0];
        memmove(s_pending, s_pending + 1, (size_t)(--s_npending) * sizeof *s_pending);
        if (p.when > s_count)
            s_count = p.when;
        if (p.kind == 0) {
            chk_event(p.event);
        } else if (p.kind == 1) {
            mq_send(p.mq, p.msg, 0);
        } else {
            mq_send(p.mq, p.msg, 0);
            if (p.interval) {
                p.when = s_count + p.interval;
                pend(p);
            }
        }
        return;
    }
    s_count = next_vi;
    s_vi_due = next_vi + TICKS_PER_FRAME;
    s_frame++;
    tgr_vi_retrace();
    if (g_tgr.frames && s_frame >= (uint32_t)g_tgr.frames) {
        s_finished = 1;
        host_cond_broadcast(s_done);
        return;
    }
    if (s_vi_mq && s_frame % (s_vi_every ? s_vi_every : 1) == 0)
        mq_send(s_vi_mq, s_vi_msg, 0);
    chk_event(TGR_EVENT_VI);
}

/* hand the baton to the best runnable thread; the caller holds G and goes on
 * (from where it called) once it has the baton back */
static void reschedule(void)
{
    TThread *self = s_cur, *next;
    for (;;) {
        next = pick();
        if (next && next->pri > 0)
            break;
        if (s_finished)
            break;
        advance_time();
    }
    while (s_finished)              /* the run is over: every game thread parks */
        host_cond_wait(self ? self->cv : s_done, G, 0xFFFFFFFFu);
    if (next == self)
        return;
    s_cur = next;
    host_cond_broadcast(next->cv);
    if (!self)
        return;
    while (s_cur != self)
        host_cond_wait(self->cv, G, 0xFFFFFFFFu);
}

static void *thread_main(void *v)
{
    TThread *t = (TThread *)v;
    host_mutex_lock(G);
    while (s_cur != t)
        host_cond_wait(t->cv, G, 0xFFFFFFFFu);
    host_mutex_unlock(G);
    t->entry(t->arg);
    fprintf(stderr, "tgr: thread %d returned from its entry function\n", t->id);
    abort();
}

void chk_thread_new(TgrThread *t, TgrId id, void (*entry)(void *), void *arg, TgrPri pri)
{
    TThread *th;
    host_mutex_lock(G);
    th = find(t);
    if (!th) {
        if (s_nthreads == MAXT) {
            fprintf(stderr, "tgr: too many threads\n");
            abort();
        }
        th = &s_threads[s_nthreads++];
        memset(th, 0, sizeof *th);
        th->cv = host_cond_new();
    }
    th->os = t;
    th->id = id;
    th->pri = pri;
    th->entry = entry;
    th->arg = arg;
    th->state = T_STOPPED;
    host_mutex_unlock(G);
}

void chk_thread_start(TgrThread *t)
{
    TThread *th;
    host_mutex_lock(G);
    th = find(t);
    th->state = T_RUNNABLE;
    if (!th->started) {
        th->started = 1;
        th->ht = host_thread_start(thread_main, th);
    }
    if (!s_started) {
        /* from boot: this thread takes over, and the boot code never resumes */
        s_started = 1;
        s_cur = NULL;
        reschedule();
        host_mutex_unlock(G);
        for (;;)
            host_sleep_ms(1000000);
    }
    if (th->pri > s_cur->pri)
        reschedule();
    host_mutex_unlock(G);
}

void chk_thread_pri(TgrThread *t, TgrPri pri)
{
    TThread *th, *best;
    host_mutex_lock(G);
    th = t ? find(t) : s_cur;
    th->pri = pri;
    best = pick();
    if ((best && best != s_cur && best->pri > s_cur->pri) || s_cur->pri == 0)
        reschedule();
    host_mutex_unlock(G);
}

static int32_t send(TgrMsgQueue *mq, TgrMsg msg, int32_t flag, int jam)
{
    TThread *woke;
    host_mutex_lock(G);
    for (;;) {
        if (mq_send(mq, msg, jam)) {
            woke = pick();
            if (woke && woke != s_cur && woke->pri > s_cur->pri)
                reschedule();
            host_mutex_unlock(G);
            return 0;
        }
        if (flag != TGR_BLOCK) {
            host_mutex_unlock(G);
            return -1;
        }
        s_cur->state = T_BLOCKED;
        s_cur->wait_mq = mq;
        s_cur->wait_full = 1;
        reschedule();
    }
}

int32_t chk_mq_send(TgrMsgQueue *mq, TgrMsg msg, int32_t flag) { return send(mq, msg, flag, 0); }
int32_t chk_mq_jam(TgrMsgQueue *mq, TgrMsg msg, int32_t flag)  { return send(mq, msg, flag, 1); }

int32_t chk_mq_recv(TgrMsgQueue *mq, TgrMsg *msg, int32_t flag)
{
    host_mutex_lock(G);
    for (;;) {
        if (mq_recv(mq, msg)) {
            host_mutex_unlock(G);
            return 0;
        }
        if (flag != TGR_BLOCK) {
            host_mutex_unlock(G);
            return -1;
        }
        s_cur->state = T_BLOCKED;
        s_cur->wait_mq = mq;
        s_cur->wait_full = 0;
        reschedule();
    }
}

void chk_event_mq(TgrEvent e, TgrMsgQueue *mq, TgrMsg msg)
{
    if (e < EV_MAX) {
        s_events[e].mq = mq;
        s_events[e].msg = msg;
    }
}

void chk_retrace_mq(TgrMsgQueue *mq, TgrMsg msg, uint32_t retraceCount)
{
    s_vi_mq = mq;
    s_vi_msg = msg;
    s_vi_every = retraceCount;
}

void chk_timer(TgrTime countdown, TgrTime interval, TgrMsgQueue *mq, TgrMsg msg)
{
    Pending p;
    memset(&p, 0, sizeof p);
    host_mutex_lock(G);
    p.when = s_count + (countdown > 0 ? (uint64_t)countdown : 1);
    p.kind = 2;
    p.mq = mq;
    p.msg = msg;
    p.interval = (uint64_t)interval;
    pend(p);
    host_mutex_unlock(G);
}

/* ---- the run ------------------------------------------------------------------- */
static void (*s_boot)(void);

static void *boot_main(void *v)
{
    (void)v;
    s_boot();
    return NULL;
}

void chk_start(void (*boot)(void))
{
    G = host_mutex_new();
    s_done = host_cond_new();
    s_boot = boot;
    host_thread_start(boot_main, NULL);
}

int chk_finished(void) { return s_finished; }

void chk_wait(void)
{
    host_mutex_lock(G);
    while (!s_finished)
        host_cond_wait(s_done, G, 0xFFFFFFFFu);
    host_mutex_unlock(G);
}
