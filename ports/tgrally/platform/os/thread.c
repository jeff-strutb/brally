/* thread.c: libultra's threads, message queues, events, timers and clock,
 * over the model n64/tools/n64box.py runs the original ROM in.
 *
 * Every game thread is a host thread, but exactly one runs at a time: the
 * one holding the baton (s_cur).  The baton moves only inside an OS call,
 * to the highest-priority runnable thread (ties to the thread created
 * first), as libultra's scheduler does on the VR4300.  When only the idle
 * thread (priority 0) could run, virtual time moves to the next thing the
 * hardware would do: a pending delivery (DMA done, RCP task done, timer,
 * controller read), or the next vertical retrace.  The same calls in the
 * same order give the same run, call for call, as the original's under
 * n64box. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "host.h"
#include "plat.h"

#define MAXT 16
#define EV_MAX 16

typedef struct TThread {
    OSThread *os;
    int id, pri, order;
    void (*entry)(void *);
    void *arg;
    enum { T_STOPPED, T_RUNNABLE, T_BLOCKED } state;
    OSMesgQueue *wait_mq;
    int wait_full;
    host_cond *cv;
    host_thread *ht;
    int started;
} TThread;

typedef struct Pending {
    uint64_t when;
    int kind;               /* 0 event, 1 message, 2 timer */
    int event;
    OSMesgQueue *mq;
    OSMesg msg;
    OSTimer *timer;
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
static struct { OSMesgQueue *mq; OSMesg msg; } s_events[EV_MAX];
static OSMesgQueue *s_vi_mq;
static OSMesg s_vi_msg;
static uint32_t s_vi_every;
static Pending s_pending[256];
static int s_npending;
static uint32_t s_seq;
static uint64_t s_wall0;
static host_cond *s_done;
static int s_finished;

uint64_t tgr_count(void) { return s_count; }
uint32_t tgr_frame(void) { return s_frame; }
void tgr_os_lock(void)   { host_mutex_lock(G); }
void tgr_os_unlock(void) { host_mutex_unlock(G); }

static TThread *find(OSThread *t)
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

/* ---- message queues: libultra's layout, in game memory ----------------------- */
static int mq_send(OSMesgQueue *mq, OSMesg msg, int jam)
{
    OSMesg *buf = TGR_PTR(OSMesg *, mq->msg);
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

static int mq_recv(OSMesgQueue *mq, OSMesg *out)
{
    OSMesg *buf = TGR_PTR(OSMesg *, mq->msg);
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

void tgr_event(int ev)
{
    if (ev >= 0 && ev < EV_MAX && s_events[ev].mq)
        mq_send(s_events[ev].mq, s_events[ev].msg, 0);
}

/* ---- pending deliveries, in time order (equal times in arrival order) -------- */
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

void tgr_post_event_at(uint64_t when, int event)
{
    Pending p;
    memset(&p, 0, sizeof p);
    p.when = when;
    p.kind = 0;
    p.event = event;
    pend(p);
}

void tgr_post_mesg_at(uint64_t when, OSMesgQueue *mq, OSMesg msg)
{
    Pending p;
    memset(&p, 0, sizeof p);
    p.when = when;
    p.kind = 1;
    p.mq = mq;
    p.msg = msg;
    pend(p);
}

/* nothing can run: deliver the next thing the hardware would do */
static void advance_time(void)
{
    uint64_t next_vi = (s_count / TGR_TICKS_PER_FRAME + 1) * TGR_TICKS_PER_FRAME;
    if (s_npending && s_pending[0].when <= next_vi) {
        Pending p = s_pending[0];
        memmove(s_pending, s_pending + 1, (size_t)(--s_npending) * sizeof *s_pending);
        if (p.when > s_count)
            s_count = p.when;
        if (p.kind == 0) {
            tgr_event(p.event);
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
    /* the next vertical retrace */
    s_count = next_vi;
    s_frame++;
    if (!g_tgr.headless) {
        /* keep pace with the wall clock: 60 retraces a second */
        uint64_t due = s_wall0 + (uint64_t)s_frame * 1000000000ull / 60;
        uint64_t now = host_ticks_ns();
        if (now < due) {
            host_mutex_unlock(G);
            host_sleep_ms((uint32_t)((due - now) / 1000000));
            host_mutex_lock(G);
        } else if (now - due > 200000000ull) {
            s_wall0 = now - (uint64_t)s_frame * 1000000000ull / 60;     /* fell behind: do not race */
        }
    }
    tgr_vi_retrace();
    if (g_tgr.frames && s_frame >= (uint32_t)g_tgr.frames) {
        s_finished = 1;
        host_cond_broadcast(s_done);
        return;
    }
    if (s_vi_mq && s_frame % (s_vi_every ? s_vi_every : 1) == 0)
        mq_send(s_vi_mq, s_vi_msg, 0);
    tgr_event(OS_EVENT_VI);
}

/* hand the baton to the best runnable thread; the caller holds G and keeps
 * running (from where it called) once it has the baton back */
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
    while (s_finished) {
        /* the run is over: park every game thread */
        host_cond_wait(self ? self->cv : s_done, G, 0xFFFFFFFFu);
    }
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

static void spawn(TThread *t)
{
    if (!t->started) {
        t->started = 1;
        t->ht = host_thread_start(thread_main, t);
    }
}

/* ---- the OS calls ------------------------------------------------------------ */
void osInitialize(void) {}

void osCreateThread(OSThread *t, OSId id, void (*entry)(void *), void *arg, void *sp, OSPri pri)
{
    TThread *th;
    (void)sp;
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

void osStartThread(OSThread *t)
{
    TThread *th;
    host_mutex_lock(G);
    th = find(t);
    th->state = T_RUNNABLE;
    spawn(th);
    if (!s_started) {
        /* from boot: this thread takes over, and the boot code never resumes */
        s_started = 1;
        s_wall0 = host_ticks_ns();
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

void osSetThreadPri(OSThread *t, OSPri pri)
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

void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, int32_t count)
{
    mq->mtqueue = 0x802A6230;       /* libultra's empty thread queue, as n64box sets it */
    mq->fullqueue = 0x802A6230;
    mq->validCount = 0;
    mq->first = 0;
    mq->msgCount = count;
    mq->msg = tgr_addr32(msg);
}

static int32_t send(OSMesgQueue *mq, OSMesg msg, int32_t flag, int jam)
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
        if (flag != OS_MESG_BLOCK) {
            host_mutex_unlock(G);
            return -1;
        }
        s_cur->state = T_BLOCKED;
        s_cur->wait_mq = mq;
        s_cur->wait_full = 1;
        reschedule();
    }
}

int32_t osSendMesg(OSMesgQueue *mq, OSMesg msg, int32_t flag) { return send(mq, msg, flag, 0); }
int32_t osJamMesg(OSMesgQueue *mq, OSMesg msg, int32_t flag)  { return send(mq, msg, flag, 1); }

int32_t osRecvMesg(OSMesgQueue *mq, OSMesg *msg, int32_t flag)
{
    host_mutex_lock(G);
    for (;;) {
        if (mq_recv(mq, msg)) {
            host_mutex_unlock(G);
            return 0;
        }
        if (flag != OS_MESG_BLOCK) {
            host_mutex_unlock(G);
            return -1;
        }
        s_cur->state = T_BLOCKED;
        s_cur->wait_mq = mq;
        s_cur->wait_full = 0;
        reschedule();
    }
}

void osSetEventMesg(OSEvent e, OSMesgQueue *mq, OSMesg msg)
{
    if (e < EV_MAX) {
        s_events[e].mq = mq;
        s_events[e].msg = msg;
    }
}

void osViSetEvent(OSMesgQueue *mq, OSMesg msg, uint32_t retraceCount)
{
    s_vi_mq = mq;
    s_vi_msg = msg;
    s_vi_every = retraceCount;
}

uint32_t osGetCount(void) { return (uint32_t)s_count; }

int32_t osSetTimer(OSTimer *t, OSTime countdown, OSTime interval, OSMesgQueue *mq, OSMesg msg)
{
    Pending p;
    memset(&p, 0, sizeof p);
    p.when = s_count + (countdown > 0 ? (uint64_t)countdown : 1);
    p.kind = 2;
    p.mq = mq;
    p.msg = msg;
    p.timer = t;
    p.interval = (uint64_t)interval;
    host_mutex_lock(G);
    pend(p);
    host_mutex_unlock(G);
    return 0;
}

void *__tgr_unused;
TgrAddr __osGetCurrFaultedThread(void) { return 0; }

/* ---- the run ----------------------------------------------------------------- */
static void (*s_boot)(void);

static void *boot_main(void *v)
{
    (void)v;
    s_boot();
    return NULL;
}

void tgr_os_start(void (*boot)(void))
{
    G = host_mutex_new();
    s_done = host_cond_new();
    s_boot = boot;
    host_thread_start(boot_main, NULL);
}

void tgr_os_wait(void)
{
    host_mutex_lock(G);
    while (!s_finished)
        host_cond_wait(s_done, G, 0xFFFFFFFFu);
    host_mutex_unlock(G);
}
