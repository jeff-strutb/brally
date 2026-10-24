/* br_os_window.c: the game's window, its message queue and its timers
 * (br_os.h).
 *
 * Host events (platform/host) become the messages the game's window
 * procedure handles -- keys, characters, focus, close -- with the numbers
 * the game was written for (BR_MSG_*), queued with what the game posts
 * itself and the timers it sets. */
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "plat.h"
#include "br_os.h"

/* ---- classes and windows ------------------------------------------------------- */
typedef struct pwnd {
    int        magic;
    br_os_proc proc;
    int        visible;
} pwnd;
#define WND_MAGIC 0x57494E44

static pwnd  *s_main;
static int    s_active;

static pwnd *W(void *h)
{
    pwnd *w = (pwnd *)h;
    return (w && w->magic == WND_MAGIC) ? w : NULL;
}

void *plat_main_window(void) { return s_main; }

static intptr_t send(pwnd *w, uint32_t m, uintptr_t wp, intptr_t lp)
{
    return (w && w->proc) ? w->proc(w, m, wp, lp) : 0;
}

/* the host's mouse positions come in the units of the size its window was
 * opened at (host.h: the whole window that size), whatever the game's
 * resolution is now (plat_pointer_to_game) */
static int s_unit_w = 640, s_unit_h = 480;

void plat_pointer_units(int *w, int *h)
{
    *w = s_unit_w;
    *h = s_unit_h;
}

/* the game's window: w x h, its messages to proc, told it exists
 * (BR_MSG_CREATE) before this returns; NULL when proc refuses it */
void *br_os_window_open(br_os_proc proc, int cx, int cy, const char *title)
{
    pwnd *w = (pwnd *)calloc(1, sizeof *w);
    w->magic = WND_MAGIC;
    w->proc = proc;
    if (!s_main) {
        s_main = w;
        s_unit_w = cx > 0 ? cx : 640;
        s_unit_h = cy > 0 ? cy : 480;
        host_window_open(s_unit_w, s_unit_h, title);
        host_window_lock_aspect(1);
    }
    if (send(w, BR_MSG_CREATE, 0, 0) == -1) {
        free(w);
        if (s_main == w)
            s_main = NULL;
        return NULL;
    }
    return w;
}

/* what the game does when it does not handle a message: a close asks to quit */
intptr_t br_os_default_proc(void *h, uint32_t m, uintptr_t wp, intptr_t lp)
{
    (void)h;
    (void)wp;
    (void)lp;
    if (m == BR_MSG_CLOSE)
        br_os_quit(0);
    return 0;
}

/* shown (cmd 0 hides, which the game never does): the window is told it is
 * shown, the active one, and has the keyboard */
int br_os_window_show(void *h, int cmd)
{
    pwnd *w = W(h);
    if (!w)
        return 0;
    if (cmd != 0 && !w->visible) {
        w->visible = 1;
        send(w, BR_MSG_SHOWWINDOW, 1, 0);
        s_active = 1;
        send(w, BR_MSG_ACTIVATEAPP, 1, 0);
        send(w, BR_MSG_ACTIVATE, 1, 0);
        send(w, BR_MSG_SETFOCUS, 0, 0);
    }
    return 1;
}

int   br_os_window_valid(void *h) { return W(h) != NULL; }
void *br_os_active_window(void) { return s_active ? s_main : NULL; }

/* ---- the message queue ------------------------------------------------------------- */
#define QN 512
static BrOsMsg s_q[QN];
static int s_qh, s_qt;
static int s_quit = -1;

static int qempty(void) { return s_qh == s_qt; }

static void qpush(void *h, uint32_t m, uintptr_t wp, intptr_t lp)
{
    int n = (s_qt + 1) % QN;
    if (n == s_qh)
        return;              /* full: the message is lost, as on Windows */
    s_q[s_qt].hwnd = h;
    s_q[s_qt].message = m;
    s_q[s_qt].wParam = wp;
    s_q[s_qt].lParam = lp;
    s_q[s_qt].time = plat_time_ms();
    s_qt = n;
}

int br_os_post(void *h, uint32_t m, uintptr_t wp, intptr_t lp)
{
    qpush(h, m, wp, lp);
    return 1;
}

void br_os_quit(int code) { s_quit = code; }

/* ---- timers -------------------------------------------------------------------------- */
/* a timer posts BR_MSG_TIMER (wParam its id) every ms milliseconds */
typedef struct ptimer { void *h; uintptr_t id; uint32_t ms; uint32_t due; int live; } ptimer;
static ptimer s_timer[16];

uintptr_t br_os_timer(void *h, uintptr_t id, uint32_t ms)
{
    int i, free_ = -1;
    for (i = 0; i < 16; i++) {
        if (s_timer[i].live && s_timer[i].h == h && s_timer[i].id == id)
            break;
        if (!s_timer[i].live && free_ < 0)
            free_ = i;
    }
    if (i == 16)
        i = free_;
    if (i < 0)
        return 0;
    s_timer[i].h = h;
    s_timer[i].id = id ? id : (uintptr_t)(i + 1);
    s_timer[i].ms = ms;
    s_timer[i].due = plat_time_ms() + ms;
    s_timer[i].live = 1;
    return s_timer[i].id;
}

int br_os_timer_kill(void *h, uintptr_t id)
{
    int i;
    for (i = 0; i < 16; i++)
        if (s_timer[i].live && s_timer[i].h == h && s_timer[i].id == id) {
            s_timer[i].live = 0;
            return 1;
        }
    return 0;
}

/* A scripted run owns the input: the desktop pointer passing over the window
 * would steer the game's cursor away from the script's, and another window
 * taking the focus would park the game in br_os_wait_message. */
static int scripted(void)
{
    static int on = -1;
    if (on < 0)
        on = getenv("BR_SCRIPT") != NULL;
    return on;
}

static void timers(void)
{
    uint32_t now = plat_time_ms();
    int i;
    for (i = 0; i < 16; i++) {
        ptimer *t = &s_timer[i];
        if (t->live && (int32_t)(now - t->due) >= 0) {
            t->due = now + t->ms;
            qpush(t->h, BR_MSG_TIMER, t->id, 0);
        }
    }
}

/* ---- keys ----------------------------------------------------------------------------- */
static uint8_t s_key[256];        /* bit 7 down, bit 0 pressed since the last query */

/* a key by the game's key number: 0x8000 held, 1 pressed since the last ask */
int16_t br_os_key(int vk)
{
    int16_t r;
    vk &= 0xFF;
    r = (int16_t)((s_key[vk] & 0x80 ? 0x8000 : 0) | (s_key[vk] & 1));
    s_key[vk] &= 0x80;
    return r;
}

/* the keyboard by scan code (br_in.c reads it) */
uint8_t g_plat_dik[256];

/* ---- host events -> messages ------------------------------------------------------------ */
void plat_deliver(const host_event *e)
{
    host_event ev = *e;
    {
        switch (ev.type) {
        case HOST_EV_KEY:
            if (ev.vk > 0 && ev.vk < 256)
                s_key[ev.vk] = ev.down ? (uint8_t)(0x80 | 1 | s_key[ev.vk]) : 0;
            if (ev.scan > 0 && ev.scan < 256)
                g_plat_dik[ev.scan] = ev.down ? 0x80 : 0;
            if (s_main)
                qpush(s_main, ev.down ? BR_MSG_KEYDOWN : BR_MSG_KEYUP, (uintptr_t)ev.vk,
                      (intptr_t)(1 | ((ev.scan & 0xFF) << 16) | (ev.down ? 0 : (3u << 30))));
            break;
        case HOST_EV_CHAR:
            if (s_main)
                qpush(s_main, BR_MSG_CHAR, (uintptr_t)ev.ch, 1);
            break;
        case HOST_EV_MOUSE: {
            if (scripted())
                break;
            /* the game reads the mouse as a device: its cursor is steered to
             * the pointer (plat_mouse_abs), and the left button */
            plat_pointer_to_game(&ev.x, &ev.y);
            plat_mouse_abs(ev.x, ev.y);
            plat_mouse_button(ev.buttons & 1);
            if (s_main)
                qpush(s_main, BR_MSG_MOUSEMOVE, (uintptr_t)ev.buttons, (intptr_t)(uint32_t)((ev.x & 0xFFFF) | (ev.y & 0xFFFF) << 16));
            break;
        }
        case HOST_EV_FOCUS:
            if (scripted())
                break;
            s_active = ev.down;
            if (s_main) {
                qpush(s_main, BR_MSG_ACTIVATEAPP, (uintptr_t)ev.down, 0);
                qpush(s_main, BR_MSG_ACTIVATE, (uintptr_t)(ev.down ? 1 : 0), 0);
            }
            break;
        case HOST_EV_CLOSE:
            if (s_main)
                qpush(s_main, BR_MSG_CLOSE, 0, 0);
            break;
        }
    }
}

void plat_pump(uint32_t wait_ms)
{
    host_event ev;
    uint32_t w = wait_ms;
    if (w && plat_vclock_main()) {       /* virtual time: the wait costs its length */
        plat_vclock_advance((uint64_t)w * 1000u);
        w = 0;
    }
    while (host_poll_event(&ev, w)) {
        w = 0;
        plat_deliver(&ev);
    }
    timers();
    plat_cd_poll();
}

static int take(BrOsMsg *m, int remove)
{
    if (qempty())
        return 0;
    *m = s_q[s_qh];
    if (remove)
        s_qh = (s_qh + 1) % QN;
    return 1;
}

static int quit_msg(BrOsMsg *m)
{
    memset(m, 0, sizeof *m);
    m->message = BR_MSG_QUIT;
    m->wParam = (uintptr_t)s_quit;
    return 1;
}

/* the next message, left queued; 0 when there is none */
int br_os_peek(BrOsMsg *m)
{
    plat_pump(0);
    if (take(m, 0))
        return 1;
    if (s_quit >= 0) {
        quit_msg(m);
        return 1;
    }
    return 0;
}

/* the next message, waited for; 0 for the quit message */
int br_os_get(BrOsMsg *m)
{
    for (;;) {
        plat_pump(0);
        if (take(m, 1))
            return m->message != BR_MSG_QUIT;
        if (s_quit >= 0) {
            quit_msg(m);
            s_quit = -1;
            return 0;
        }
        plat_pump(5);
    }
}

void br_os_wait_message(void)
{
    while (qempty() && s_quit < 0)
        plat_pump(5);
}

intptr_t br_os_dispatch(const BrOsMsg *m)
{
    return send(W(m->hwnd), m->message, m->wParam, m->lParam);
}

/* a number of the game's own for a message named name, the same each time */
uint32_t br_os_message_id(const char *name)
{
    static char names[32][64];
    static int n;
    int i;
    for (i = 0; i < n; i++)
        if (!strcmp(names[i], name))
            return 0xC100 + i;
    if (n == 32)
        return 0;
    snprintf(names[n], sizeof names[0], "%s", name);
    return 0xC100 + n++;
}

/* ---- message boxes ---------------------------------------------------------------------------- */
int br_os_message_box(const char *text, const char *caption)
{
    host_message_box(text, caption);
    return 1;
}
