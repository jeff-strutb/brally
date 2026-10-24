/* br_os.c: what the game asks of the system (br_os.h) -- its threads,
 * events and mutexes, its locks, the clock, sleeping, debug output and the
 * few facts it reads about the machine.
 *
 * Every waitable object shares one lock and one condition: a state change
 * wakes every waiter, and each re-checks what it waits for. The game waits
 * on a handful of objects at a time, so this is simple and enough. */
#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "plat.h"
#include "br_os.h"

int g_plat_log;

/* ---- waitable objects ---------------------------------------------------------- */
enum { K_EVENT = 0x4B45, K_MUTEX, K_THREAD };

typedef struct kobj {
    int        type;
    int        signaled;       /* event: set; thread: finished */
    int        manual;         /* event: manual reset */
    uintptr_t  owner;          /* mutex: owning thread, 0 = free */
    int        depth;          /* mutex: recursion */
    uint32_t (*fn)(void *);    /* thread */
    void      *arg;
    uint32_t   code;
} kobj;

static host_mutex *s_k;
static host_cond  *s_kc;

static void kinit(void)
{
    if (!s_k) {
        s_k = host_mutex_new();
        s_kc = host_cond_new();
    }
}

/* Handles are small numbers, as on Windows (whose handles always fit in 32
 * bits): the game keeps them in 32-bit fields of its records. A handle is
 * (slot + 1) * 4 in a table of objects. */
#define KMAX 4096
static kobj *s_ktab[KMAX];
static int   s_kn;

static kobj *knew(int type)
{
    kobj *o = (kobj *)calloc(1, sizeof *o);
    kinit();
    o->type = type;
    return o;
}

static void *khandle(kobj *o)
{
    int i;
    host_mutex_lock(s_k);
    i = s_kn < KMAX ? s_kn++ : -1;
    if (i >= 0)
        s_ktab[i] = o;
    host_mutex_unlock(s_k);
    return i < 0 ? NULL : (void *)(uintptr_t)((i + 1) * 4);
}

static kobj *kget(void *h)
{
    uint32_t v = (uint32_t)(uintptr_t)h;
    if (v == 0 || (v & 3) != 0 || v / 4 > (uint32_t)s_kn)
        return NULL;
    return s_ktab[v / 4 - 1];
}

void *br_os_event(int manual, int initial)
{
    kobj *o = knew(K_EVENT);
    o->manual = manual;
    o->signaled = initial;
    return khandle(o);
}

void *br_os_mutex(int owned)
{
    kobj *o = knew(K_MUTEX);
    if (owned) {
        o->owner = host_thread_self();
        o->depth = 1;
    }
    return khandle(o);
}

int br_os_set(void *h)
{
    kobj *o;
    plat_vclock_import();
    o = kget(h);
    if (!o || o->type != K_EVENT)
        return 0;
    host_mutex_lock(s_k);
    o->signaled = 1;
    host_cond_broadcast(s_kc);
    host_mutex_unlock(s_k);
    return 1;
}

int br_os_release(void *h)
{
    kobj *o;
    int ok = 0;
    plat_vclock_import();
    o = kget(h);
    if (!o || o->type != K_MUTEX)
        return 0;
    host_mutex_lock(s_k);
    if (o->owner == host_thread_self() && o->depth > 0) {
        if (--o->depth == 0)
            o->owner = 0;
        host_cond_broadcast(s_kc);
        ok = 1;
    }
    host_mutex_unlock(s_k);
    return ok;
}

/* can this thread take o now (and if so, take it) -- under s_k */
static int ktake(kobj *o, int take)
{
    uintptr_t self = host_thread_self();
    switch (o->type) {
    case K_EVENT:
        if (!o->signaled)
            return 0;
        if (take && !o->manual)
            o->signaled = 0;
        return 1;
    case K_MUTEX:
        if (o->owner != 0 && o->owner != self)
            return 0;
        if (take) {
            o->owner = self;
            o->depth++;
        }
        return 1;
    case K_THREAD:
        return o->signaled;
    }
    return 0;
}

uint32_t br_os_wait_many(uint32_t n, void *const *h, int all, uint32_t ms)
{
    uint64_t end;
    uint32_t i, r = BR_OS_WAIT_TIMEOUT;
    plat_vclock_import();
    end = ms == BR_OS_FOREVER ? 0 : host_ticks_ns() + (uint64_t)ms * 1000000u;
    kinit();
    /* an invalid handle fails the wait at once, as on Windows */
    for (i = 0; i < n; i++) {
        kobj *o = kget(h[i]);
        if (!o || (o->type != K_EVENT && o->type != K_MUTEX && o->type != K_THREAD))
            return BR_OS_WAIT_FAILED;
    }
    host_mutex_lock(s_k);
    for (;;) {
        if (all) {
            for (i = 0; i < n && ktake(kget(h[i]), 0); i++)
                ;
            if (i == n) {
                for (i = 0; i < n; i++)
                    ktake(kget(h[i]), 1);
                r = BR_OS_WAIT_OBJECT_0;
                break;
            }
        } else {
            for (i = 0; i < n; i++)
                if (ktake(kget(h[i]), 1))
                    break;
            if (i < n) {
                r = BR_OS_WAIT_OBJECT_0 + i;
                break;
            }
        }
        if (ms == 0)
            break;
        if (ms == BR_OS_FOREVER) {
            host_cond_wait(s_kc, s_k, BR_OS_FOREVER);
        } else {
            uint64_t now = host_ticks_ns();
            if (now >= end)
                break;
            host_cond_wait(s_kc, s_k, (uint32_t)((end - now) / 1000000u) + 1);
        }
    }
    host_mutex_unlock(s_k);
    return r;
}

uint32_t br_os_wait(void *h, uint32_t ms)
{
    plat_vclock_import();
    if (!h)
        return BR_OS_WAIT_FAILED;
    return br_os_wait_many(1, &h, 0, ms);
}

/* the thread object of the calling thread, for ExitThread */
static _Thread_local kobj *s_kself;

/* a thread finishing, by returning or by ExitThread: its handle signals */
static void kthread_done(kobj *o, uint32_t code)
{
    host_mutex_lock(s_k);
    o->code = code;
    o->signaled = 1;
    host_cond_broadcast(s_kc);
    host_mutex_unlock(s_k);
}

static void *kthread_main(void *p)
{
    kobj *o = (kobj *)p;
    s_kself = o;
    kthread_done(o, o->fn(o->arg));
    return NULL;
}

void *br_os_thread(uint32_t (*fn)(void *), void *arg, uint32_t *id)
{
    kobj *o = knew(K_THREAD);
    o->fn = fn;
    o->arg = arg;
    if (!host_thread_start(kthread_main, o)) {
        free(o);
        return NULL;
    }
    {
        void *hT = khandle(o);
        if (id)
            *id = (uint32_t)(uintptr_t)hT;
        return hT;
    }
}

void br_os_thread_exit(uint32_t code)
{
    if (s_kself)
        kthread_done(s_kself, code);
    host_thread_exit();
}

int br_os_close(void *h)
{
    /* Objects stay allocated: a thread may still signal one that the game
     * has closed, and the game creates few of them. */
    return kget(h) != NULL;
}

void br_os_sleep(uint32_t ms)
{
    plat_pump(0);
    if (plat_vclock_main())
        plat_vclock_advance((uint64_t)ms * 1000u);
    else
        host_sleep_ms(ms);
}

/* ---- critical sections ------------------------------------------------------------ */
/* a lock the game keeps in a block of its own (its first pointer holds it) */
void br_os_lock_init(void *cs) { *(host_mutex **)cs = host_mutex_new(); }
void br_os_lock_free(void *cs)
{
    host_mutex_free(*(host_mutex **)cs);
    *(host_mutex **)cs = NULL;
}
void br_os_lock(void *cs) { host_mutex_lock(*(host_mutex **)cs); }
void br_os_unlock(void *cs) { host_mutex_unlock(*(host_mutex **)cs); }

/* ---- time ------------------------------------------------------------------------------ */
/* BR_VCLOCK=ms: virtual time, kept the way the wasm lane keeps it
 * (ports/brally-wasm/wasm/host/host_win.c) so the two lanes run a script frame for
 * frame: a clock read on the main thread costs ms, br_os_sleep(n) there costs n
 * instead of sleeping, a message wait costs its wait; other threads read the
 * clock without moving it. The counter then runs at the wasm lane's 1 MHz
 * from its base, so a dump or a Glide log compares exactly. */
static _Thread_local int s_main_thread;
static int64_t  s_vtick = -1;
static uint64_t s_vus;

void plat_mark_main_thread(void) { s_main_thread = 1; }

int plat_vclock(void)
{
    if (s_vtick < 0) {
        const char *e = getenv("BR_VCLOCK");
        s_vtick = e ? (int64_t)(atof(e) * 1000.0) : 0;
    }
    return s_vtick != 0;
}

int plat_vclock_main(void) { return plat_vclock() && s_main_thread; }

void plat_vclock_advance(uint64_t us)
{
    if (plat_vclock_main()) {
        s_vus += us;
        plat_peersync(s_vus);
    }
}

/* BR_VCLOCK_IMPORTS: every call into the platform costs a tick too, as in
 * the brbox oracle (each import advances its clock 0.25 ms). Scripts
 * written against it -- the multiplayer pairs -- are timed by that clock; a
 * `peer` pair turns it on (script.c). Off, time moves only when read. */
static int s_vimports = -1;

void plat_vclock_imports(int on) { s_vimports = on; }

void plat_vclock_import(void)
{
    if (s_vimports < 0)
        s_vimports = getenv("BR_VCLOCK_IMPORTS") != NULL;
    if (s_vimports && plat_vclock_main())
        plat_vclock_advance((uint64_t)s_vtick);
}

/* ... and, as there, each frame costs 1/30 s on top: a machine that presents
 * exactly 30 frames a second (brbox_drive.py FRAME_MS) */
void plat_vclock_frame(void)
{
    static uint64_t frames;
    if (s_vimports < 0)
        s_vimports = getenv("BR_VCLOCK_IMPORTS") != NULL;
    if (s_vimports && plat_vclock_main()) {
        frames++;
        plat_vclock_advance(frames * 1000000u / 30u - (frames - 1) * 1000000u / 30u);
    }
}

static uint64_t vclock_read_us(void)
{
    plat_vclock_advance((uint64_t)s_vtick);
    return s_vus;
}

static uint64_t s_t0;
static uint64_t since_start(void)
{
    if (!s_t0)
        s_t0 = host_ticks_ns();
    return host_ticks_ns() - s_t0;
}

/* the performance counter at a PC-typical 1.193182 MHz, so values the game
 * keeps in 32 bits wrap no sooner than they did on the original */
#define PERF_HZ 1193182u
int br_os_perf_hz(int64_t *f)
{
    *f = plat_vclock() ? 1000000 : PERF_HZ;
    return 1;
}

int br_os_perf(int64_t *c)
{
    uint64_t ns;
    if (plat_vclock()) {
        *c = (int64_t)(vclock_read_us() + 1000000u);
        return 1;
    }
    ns = since_start();
    *c = (int64_t)(ns / 1000000000u * PERF_HZ + ns % 1000000000u * PERF_HZ / 1000000000u);
    return 1;
}

/* the clock as br_os_ms reads it, without spending a virtual tick: the
 * platform's own bookkeeping (timers, message stamps) is not a game read */
uint32_t plat_time_ms(void)
{
    if (plat_vclock())
        return (uint32_t)(s_vus / 1000u) + 1000u;
    return (uint32_t)(since_start() / 1000000u);
}

/* milliseconds since the start (the original's timeGetTime) */
uint32_t br_os_ms(void)
{
    if (plat_vclock())
        return (uint32_t)(vclock_read_us() / 1000u) + 1000u;
    return (uint32_t)(since_start() / 1000000u);
}
/* what the drive the path names is: 5 the CD, 3 the hard disk, 1 none */
uint32_t br_os_drive_type(const char *root)
{
    int d = root && root[0] && root[1] == ':' ? toupper((unsigned char)root[0]) - 'A' + 1 : plat_drive();
    if (d == PLAT_CD_DRIVE)
        return 5;
    if (d == 3)
        return 3;
    return 1;
}

/* the drive's label, serial, name length, flags and file system */
int br_os_volume_info(const char *root, char *label, uint32_t nlabel, uint32_t *serial,
                      uint32_t *maxlen, uint32_t *flags, char *fs, uint32_t nfs)
{
    int d = root && root[0] && root[1] == ':' ? toupper((unsigned char)root[0]) - 'A' + 1 : plat_drive();
    if (label && nlabel)
        snprintf(label, nlabel, "%s", d == PLAT_CD_DRIVE ? PLAT_CD_LABEL : "");
    if (serial)
        *serial = 0x19980000u;
    if (maxlen)
        *maxlen = 255;
    if (flags)
        *flags = 0;
    if (fs && nfs)
        snprintf(fs, nfs, "%s", d == PLAT_CD_DRIVE ? "CDFS" : "FAT32");
    return d == PLAT_CD_DRIVE || d == 3;
}

/* the player's name: the host user's */
int br_os_user_name(char *out, uint32_t *n)
{
    const char *u = getenv("USER");
    if (!u)
        u = "Player";
    if (!out || !n || *n <= strlen(u))
        return 0;
    strcpy(out, u);
    *n = (uint32_t)strlen(u) + 1;
    return 1;
}


void br_os_debug(const char *s) { PLOG("%s", s ? s : ""); }
int  br_os_strlen(const char *s) { return s ? (int)strlen(s) : 0; }
