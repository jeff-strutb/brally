/* host_ps1.c: the host interface (ports/brally/platform/host/host.h) on the
 * PlayStation, as much of it as the game's platform layer uses.
 *
 * Threads are coroutines.  The platform's scheduler (ports/tgrally's
 * os/thread.c) already lets exactly one game thread run at a time and moves
 * between them only by waiting on a condition another one signals, so a
 * wait here is a switch to the next thread that can go on, and a broadcast
 * makes its waiters able to.  No locks are needed: nothing preempts. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "host.h"
#include "ps1.h"

uint64_t host_ticks_ns(void)
{
    return ps1_time_ns();
}

typedef struct Ps1Ctx { uint32_t r[12]; } Ps1Ctx;       /* s0-s7 fp gp sp ra (hw/ctx.s) */
void ps1_ctx_switch(Ps1Ctx *from, Ps1Ctx *to);
void ps1_ctx_entry(void);

enum { HT_FREE, HT_READY, HT_WAIT, HT_DEAD };
struct host_thread {
    Ps1Ctx ctx;
    int state;
    host_cond *wait;
    void *stack;
};
struct host_cond { int unused; };
struct host_mutex { int unused; };

#define MAXHT 20
#define STACK 0x4000
static struct host_thread s_ht[MAXHT];
static int s_cur;                       /* thread 0 is main() */

int ps1_hostlog;                        /* TGR_HOSTLOG: each switch on the TTY */

static void schedule(void)
{
    int i, n;
    for (i = 1; i <= MAXHT; i++) {
        n = (s_cur + i) % MAXHT;
        if (s_ht[n].state == HT_READY) {
            int from = s_cur;
            if (n == from)
                return;
            s_cur = n;
            if (ps1_hostlog) {
                char b[32] = "host: switch x -> y\n";
                b[13] = (char)('A' + from);
                b[18] = (char)('A' + n);
                ps1_tty(b);
            }
            ps1_ctx_switch(&s_ht[from].ctx, &s_ht[n].ctx);
            return;
        }
    }
    ps1_fatal("host: every thread is waiting");
}

host_thread *host_thread_start(void *(*fn)(void *), void *arg)
{
    int i;
    uint8_t *sp;
    if (s_ht[0].state == HT_FREE)
        s_ht[0].state = HT_READY;
    for (i = 1; i < MAXHT && s_ht[i].state != HT_FREE && s_ht[i].state != HT_DEAD; i++)
        ;
    if (i == MAXHT)
        ps1_fatal("host: too many threads");
    if (!s_ht[i].stack)
        s_ht[i].stack = malloc(STACK);
    memset(&s_ht[i].ctx, 0, sizeof s_ht[i].ctx);
    sp = (uint8_t *)s_ht[i].stack + STACK - 32;
    s_ht[i].ctx.r[0] = (uint32_t)fn;
    s_ht[i].ctx.r[1] = (uint32_t)arg;
    s_ht[i].ctx.r[9] = ps1_gp();
    s_ht[i].ctx.r[10] = (uint32_t)sp;
    s_ht[i].ctx.r[11] = (uint32_t)ps1_ctx_entry;
    s_ht[i].state = HT_READY;
    s_ht[i].wait = NULL;
    return &s_ht[i];
}

void host_thread_exit(void)
{
    s_ht[s_cur].state = HT_DEAD;
    schedule();
    for (;;)
        ;
}

uintptr_t host_thread_self(void) { return (uintptr_t)s_cur; }
void host_thread_interactive(void) {}

host_mutex *host_mutex_new(void) { static host_mutex m; return &m; }
void host_mutex_lock(host_mutex *m) { (void)m; }
void host_mutex_unlock(host_mutex *m) { (void)m; }
void host_mutex_free(host_mutex *m) { (void)m; }

host_cond *host_cond_new(void) { return (host_cond *)malloc(sizeof(host_cond)); }
void host_cond_free(host_cond *c) { (void)c; }

int host_cond_wait(host_cond *c, host_mutex *m, uint32_t timeout_ms)
{
    (void)m;
    (void)timeout_ms;
    if (s_ht[0].state == HT_FREE)
        s_ht[0].state = HT_READY;
    s_ht[s_cur].state = HT_WAIT;
    s_ht[s_cur].wait = c;
    schedule();
    return 0;
}

void host_cond_broadcast(host_cond *c)
{
    int i;
    for (i = 0; i < MAXHT; i++)
        if (s_ht[i].state == HT_WAIT && s_ht[i].wait == c) {
            s_ht[i].state = HT_READY;
            s_ht[i].wait = NULL;
        }
}

/* a sleeping thread lets the others run, and runs again once its time is up */
void host_sleep_ms(uint32_t ms)
{
    uint64_t due = ps1_time_ns() + (uint64_t)ms * 1000000u;
    if (s_ht[0].state == HT_FREE)
        s_ht[0].state = HT_READY;
    do
        schedule();
    while (ps1_time_ns() < due);
}

/* no window; files over PCDRV, relative to its root */
const char *host_resource_dir(void) { return "."; }
const char *host_save_dir(void) { return "."; }
const char *host_game_dir(void) { return "."; }
const char *host_cd_dir(void) { return "."; }
int  host_mkdir(const char *path) { (void)path; return 0; }
void host_window_pixels(int *w, int *h) { *w = 320; *h = 240; }
void host_present(const uint32_t *argb, int w, int h) { (void)argb; (void)w; (void)h; }
void host_message_box(const char *text, const char *caption) { (void)caption; ps1_fatal(text); }
void host_set_app_name(const char *dir, const char *title) { (void)dir; (void)title; }
void host_init(int argc, char **argv) { (void)argc; (void)argv; }
void host_shutdown(void) {}

int host_pad_read(host_pad *p)
{
    return ps1_pad_read(p);
}
