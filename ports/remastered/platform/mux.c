/* mux.c: one host for two games.  Top Gear Rally runs the menus and Boss
 * Rally's engine runs the races (main.c); each game asks the host for its
 * window, input, sound and folders through its own thunks (thunks.c), and
 * this gives both the one real host underneath (host/macos, host/windows,
 * host/null, or the iPhone app's Swift: ios/Sources/Host.swift):
 *
 *   front     one game is in front at a time (mux_front): the keys, the
 *             mouse and the pad are its, and only it sees its window
 *   events    Top Gear Rally's window loop (its main thread, os/main.c) is
 *             the only one that drains the real host's queue; what it takes
 *             goes to the game in front (focus changes and the close to
 *             both), and Boss Rally's thread waits on a queue of its own.
 *             Keys held when a game leaves the front go up for it
 *   window    opened by Top Gear Rally, 16:9 when its Remastered profile
 *             fills any shape (TGR_FLAG_ANY_ASPECT; the window's shape is
 *             the front game's to lock, applied on the window's thread).
 *             On Windows Boss Rally draws into a child window over the
 *             client area, shown while it is in front (a Vulkan swapchain
 *             owns the window it presents to)
 *   sound     one output at the first game's rate (Top Gear Rally's), each
 *             game's mixer added in, the other rate's resampled; Boss
 *             Rally's only while it is in front.  The music under a race is
 *             still Top Gear Rally's (src/racing/handoff.c), so Boss Rally
 *             is given no CD music
 *   folders   the app's saves (main.c names the app), Boss Rally's in a
 *             folder of its own inside them */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "host.h"
#include "mux.h"

int tgr_flag(int flag);                 /* the Top Gear Rally object's (os/view.c) */
#define TGR_FLAG_ANY_ASPECT 0           /* tgr_view.h */

static host_mutex *s_m;
static host_cond *s_c;
static volatile int s_front = MUX_TGR;

static void init_once(void)
{
    if (!s_m) {
        s_m = host_mutex_new();
        s_c = host_cond_new();
    }
}

/* ---- the event queues and the keys each game holds ------------------------------ */
#define QMAX 256
static struct {
    host_event e[QMAX];
    int h, t;
    unsigned char held[256];            /* by Win32 virtual key: down */
    unsigned char scan[256];            /* its scan code */
} s_q[MUX_COUNT];

static void qpush(int g, const host_event *e)
{
    int n = (s_q[g].t + 1) % QMAX;
    if (e->type == HOST_EV_KEY && e->vk >= 0 && e->vk < 256) {
        s_q[g].held[e->vk] = (unsigned char)e->down;
        s_q[g].scan[e->vk] = (unsigned char)e->scan;
    }
    if (n != s_q[g].h) {
        s_q[g].e[s_q[g].t] = *e;
        s_q[g].t = n;
    }
}

static int qpop(int g, host_event *e)
{
    if (s_q[g].h == s_q[g].t)
        return 0;
    *e = s_q[g].e[s_q[g].h];
    s_q[g].h = (s_q[g].h + 1) % QMAX;
    return 1;
}

void mux_front(int g)
{
    int i, was;
    init_once();
    host_mutex_lock(s_m);
    was = s_front;
    if (was != g) {
        for (i = 0; i < 256; i++)       /* what the one leaving holds goes up for it */
            if (s_q[was].held[i]) {
                host_event k;
                memset(&k, 0, sizeof k);
                k.type = HOST_EV_KEY;
                k.vk = i;
                k.scan = s_q[was].scan[i];
                k.down = 0;
                qpush(was, &k);
            }
        s_front = g;
    }
    host_cond_broadcast(s_c);
    host_mutex_unlock(s_m);
}

int mux_front_game(void) { return s_front; }

/* ---- the window ----------------------------------------------------------------- */
static int s_open;                      /* Top Gear Rally opened the real window */
static volatile int s_visible;          /* the real window can be seen (read on its thread) */
static int s_want_lock[MUX_COUNT] = { -1, -1 };
static int s_lock = -1;                 /* what the real window was last told */

#ifdef _WIN32
/* Boss Rally's child window: the size of the client area, shown while Boss
 * Rally is in front and has asked for it (a GPU renderer draws into it; the
 * software one presents through the real window) */
static HWND s_child;
static volatile int s_child_used;
static int s_child_shown = -1;

static LRESULT CALLBACK child_proc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    if (m == WM_NCHITTEST)
        return HTTRANSPARENT;           /* the mouse goes to the real window */
    if (m == WM_ERASEBKGND)
        return 1;
    return DefWindowProcA(h, m, wp, lp);
}

static void child_open(void)
{
    WNDCLASSA wc;
    HWND parent = (HWND)host_window_handle();
    RECT r;
    if (s_child || !parent)
        return;
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = child_proc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = "RemasteredRace";
    RegisterClassA(&wc);
    SetWindowLongPtrA(parent, GWL_STYLE, GetWindowLongPtrA(parent, GWL_STYLE) | WS_CLIPCHILDREN);
    GetClientRect(parent, &r);
    s_child = CreateWindowExA(WS_EX_NOPARENTNOTIFY, "RemasteredRace", "", WS_CHILD | WS_CLIPSIBLINGS,
                              0, 0, r.right, r.bottom, parent, NULL, wc.hInstance, NULL);
}

static void child_keep(void)
{
    RECT r, c;
    int show;
    if (!s_child)
        return;
    GetClientRect((HWND)host_window_handle(), &r);
    GetClientRect(s_child, &c);
    if (c.right != r.right || c.bottom != r.bottom)
        MoveWindow(s_child, 0, 0, r.right, r.bottom, FALSE);
    show = s_front == MUX_BR && s_child_used;
    if (show != s_child_shown) {
        ShowWindow(s_child, show ? SW_SHOWNA : SW_HIDE);
        s_child_shown = show;
    }
}
#endif

/* on the window's thread, each time its loop polls: the front game's shape */
static void window_keep(void)
{
    int want = s_want_lock[s_front];
    if (want >= 0 && want != s_lock) {
        host_window_lock_aspect(want);
        s_lock = want;
    }
    s_visible = host_window_visible();
#ifdef _WIN32
    child_keep();
#endif
}

int mux_tgr_host_window_open(int width, int height, const char *title)
{
    int r;
    /* the Remastered profile's window is 16:9 (its race fills it; the menus
       keep the N64's 4:3 inside) */
    if (tgr_flag(TGR_FLAG_ANY_ASPECT))
        width = (height * 16 + 4) / 9;
    (void)title;                        /* the app's (main.c) */
    r = host_window_open(width, height, NULL);
    s_open = r;
    s_visible = host_window_visible();
#ifdef _WIN32
    if (r)
        child_open();
#endif
    return r;
}

/* Top Gear Rally opened it first (headless, the null host answers) */
int mux_br_host_window_open(int width, int height, const char *title)
{
    return s_open || host_window_open(width, height, title);
}

void mux_tgr_host_window_close(void) { host_window_close(); }
void mux_br_host_window_close(void) {}

int mux_tgr_host_window_visible(void) { return s_front == MUX_TGR && s_visible; }
int mux_br_host_window_visible(void)  { return s_front == MUX_BR && s_visible; }

void mux_tgr_host_window_lock_aspect(int lock) { s_want_lock[MUX_TGR] = lock != 0; }
void mux_br_host_window_lock_aspect(int lock)  { s_want_lock[MUX_BR] = lock != 0; }

void *mux_tgr_host_window_handle(void) { return host_window_handle(); }
void *mux_br_host_window_handle(void)
{
#ifdef _WIN32
    if (s_child) {
        s_child_used = 1;
        return s_child;
    }
#endif
    return host_window_handle();
}

void mux_tgr_host_window_pixels(int *w, int *h) { host_window_pixels(w, h); }
void mux_br_host_window_pixels(int *w, int *h)  { host_window_pixels(w, h); }

void mux_tgr_host_present(const uint32_t *argb, int w, int h)
{
    if (s_front == MUX_TGR)
        host_present(argb, w, h);
}
void mux_br_host_present(const uint32_t *argb, int w, int h)
{
    if (s_front == MUX_BR)
        host_present(argb, w, h);
}

void mux_tgr_host_message_box(const char *text, const char *caption) { host_message_box(text, caption); }
void mux_br_host_message_box(const char *text, const char *caption)
{
#ifdef __APPLE__
    /* a Cocoa alert belongs to the window's thread, which is Top Gear Rally's */
    fprintf(stderr, "%s: %s\n", caption ? caption : "Boss Rally", text ? text : "");
#else
    host_message_box(text, caption);
#endif
}

#if defined(__APPLE__) && defined(MUX_METAL_LAYER)
void *host_macos_metal_layer(void);
void host_macos_layer_fit(int *w, int *h);
void *mux_tgr_host_macos_metal_layer(void) { return host_macos_metal_layer(); }
void *mux_br_host_macos_metal_layer(void)  { return host_macos_metal_layer(); }
void mux_tgr_host_macos_layer_fit(int *w, int *h) { host_macos_layer_fit(w, h); }
void mux_br_host_macos_layer_fit(int *w, int *h)  { host_macos_layer_fit(w, h); }
#endif
#ifdef _WIN32
void *host_win32_instance(void);
void *host_win32_window(void);
void *mux_tgr_host_win32_instance(void) { return host_win32_instance(); }
void *mux_br_host_win32_instance(void)  { return host_win32_instance(); }
void *mux_tgr_host_win32_window(void)   { return host_win32_window(); }
void *mux_br_host_win32_window(void)    { return mux_br_host_window_handle(); }
#endif

/* ---- events ----------------------------------------------------------------------- */
static void route(const host_event *e)
{
    host_mutex_lock(s_m);
    switch (e->type) {
    case HOST_EV_FOCUS:                 /* both pause in the background */
        qpush(MUX_TGR, e);
        qpush(MUX_BR, e);
        break;
    case HOST_EV_CLOSE:                 /* the app ends with Top Gear Rally */
        qpush(MUX_TGR, e);
        break;
    default:
        qpush(s_front, e);
        break;
    }
    host_cond_broadcast(s_c);
    host_mutex_unlock(s_m);
}

/* Top Gear Rally's window loop: the one that drains the real queue */
int mux_tgr_host_poll_event(host_event *ev, uint32_t wait_ms)
{
    host_event e;
    int r;
    init_once();
    window_keep();
    host_mutex_lock(s_m);
    r = qpop(MUX_TGR, ev);
    host_mutex_unlock(s_m);
    if (r)
        return 1;
    while (host_poll_event(&e, wait_ms)) {
        route(&e);
        wait_ms = 0;                    /* the rest of what is queued, no more waiting */
    }
    window_keep();
    host_mutex_lock(s_m);
    r = qpop(MUX_TGR, ev);
    host_mutex_unlock(s_m);
    return r;
}

/* Boss Rally's thread: its own queue */
int mux_br_host_poll_event(host_event *ev, uint32_t wait_ms)
{
    int r;
    init_once();
    host_mutex_lock(s_m);
    if (s_q[MUX_BR].h == s_q[MUX_BR].t && wait_ms > 0)
        host_cond_wait(s_c, s_m, wait_ms);
    r = qpop(MUX_BR, ev);
    host_mutex_unlock(s_m);
    return r;
}

/* ---- the pad: the front game's ----------------------------------------------------- */
static int pad_read(int g, host_pad *p)
{
    if (s_front == g)
        return host_pad_read(p);
    memset(p, 0, sizeof *p);
    p->pov = -1;
    return 0;
}
int mux_tgr_host_pad_read(host_pad *p) { return pad_read(MUX_TGR, p); }
int mux_br_host_pad_read(host_pad *p)  { return pad_read(MUX_BR, p); }

/* ---- sound: one output, both mixers ----------------------------------------------- */
#define CHUNK 1024                      /* frames mixed at a time */
static struct {
    host_audio_fn fn;
    void *user;
    int rate;
    double pos;                         /* resampling: where between its samples */
    float last[2];                      /* its last sample of the chunk before */
} s_a[MUX_COUNT];
static int s_rate;                      /* the output's */
static float s_scratch[2 * (4 * CHUNK + 2)];

/* one game's mixer added into lr: at the output's rate as it is, else read
 * between its samples along a straight line */
static void add_game(int g, float *lr, int n)
{
    int i;
    if (s_a[g].rate == s_rate) {
        s_a[g].fn(s_scratch, n, s_a[g].user);
        for (i = 0; i < 2 * n; i++)
            lr[i] += s_scratch[i];
    } else {
        double ratio = (double)s_a[g].rate / s_rate, end;
        int need = (int)(s_a[g].pos + (n - 1) * ratio) + 1, used;
        if (need + 1 > 4 * CHUNK)
            return;                     /* a rate more than four times the output's */
        s_scratch[0] = s_a[g].last[0];
        s_scratch[1] = s_a[g].last[1];
        s_a[g].fn(s_scratch + 2, need, s_a[g].user);
        for (i = 0; i < n; i++) {
            double x = s_a[g].pos + i * ratio;
            int k = (int)x;
            float f = (float)(x - k);
            lr[2 * i] += s_scratch[2 * k] * (1 - f) + s_scratch[2 * k + 2] * f;
            lr[2 * i + 1] += s_scratch[2 * k + 1] * (1 - f) + s_scratch[2 * k + 3] * f;
        }
        end = s_a[g].pos + n * ratio;
        used = (int)end;
        s_a[g].last[0] = s_scratch[2 * used];
        s_a[g].last[1] = s_scratch[2 * used + 1];
        s_a[g].pos = end - used;
    }
}

static void mix(float *lr, int frames, void *user)
{
    (void)user;
    memset(lr, 0, sizeof(float) * 2 * (size_t)frames);
    while (frames > 0) {
        int n = frames < CHUNK ? frames : CHUNK, g;
        for (g = 0; g < MUX_COUNT; g++) {
            if (!s_a[g].fn || (g == MUX_BR && s_front != MUX_BR))
                continue;               /* Boss Rally is silent between races */
            add_game(g, lr, n);
        }
        lr += 2 * n;
        frames -= n;
    }
}

static int audio_open(int g, int rate, host_audio_fn fn, void *user)
{
    s_a[g].user = user;
    s_a[g].rate = rate;
    s_a[g].pos = 0;
    s_a[g].last[0] = s_a[g].last[1] = 0;
    s_a[g].fn = fn;
    if (s_rate)
        return 1;
    s_rate = rate;
    if (!host_audio_open(rate, mix, NULL)) {
        s_rate = 0;
        s_a[g].fn = NULL;
        return 0;
    }
    return 1;
}
int mux_tgr_host_audio_open(int rate, host_audio_fn fn, void *user) { return audio_open(MUX_TGR, rate, fn, user); }
int mux_br_host_audio_open(int rate, host_audio_fn fn, void *user)  { return audio_open(MUX_BR, rate, fn, user); }

/* the output ends with Top Gear Rally; Boss Rally's mixer is only let go */
void mux_tgr_host_audio_close(void)
{
    host_audio_close();
    s_rate = 0;
    s_a[MUX_TGR].fn = NULL;
}
void mux_br_host_audio_close(void) { s_a[MUX_BR].fn = NULL; }

host_stream *mux_tgr_host_stream_open(const char *path, int rate) { return host_stream_open(path, rate); }
host_stream *mux_br_host_stream_open(const char *path, int rate)  { return host_stream_open(path, rate); }
int mux_tgr_host_stream_read(host_stream *s, float *lr, int frames) { return host_stream_read(s, lr, frames); }
int mux_br_host_stream_read(host_stream *s, float *lr, int frames)  { return host_stream_read(s, lr, frames); }
void mux_tgr_host_stream_close(host_stream *s) { host_stream_close(s); }
void mux_br_host_stream_close(host_stream *s)  { host_stream_close(s); }

/* ---- process and folders ------------------------------------------------------------ */
static char s_br_save[1024];

/* the app is named by main.c, not by either game */
void mux_tgr_host_set_app_name(const char *dir, const char *title) { (void)dir; (void)title; }
void mux_br_host_set_app_name(const char *dir, const char *title)  { (void)dir; (void)title; }

void mux_tgr_host_init(int argc, char **argv)
{
    init_once();
    host_init(argc, argv);
}

/* the real host is up (Top Gear Rally started it): Boss Rally's saves */
void mux_br_host_init(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    init_once();
    snprintf(s_br_save, sizeof s_br_save, "%s/Boss Rally", host_save_dir());
    host_mkdir(s_br_save);
}

void mux_tgr_host_shutdown(void) { host_shutdown(); }
void mux_br_host_shutdown(void) {}

const char *mux_tgr_host_save_dir(void) { return host_save_dir(); }
const char *mux_br_host_save_dir(void)  { return s_br_save; }
const char *mux_tgr_host_game_dir(void) { return host_game_dir(); }
const char *mux_br_host_game_dir(void)  { return host_game_dir(); }
const char *mux_tgr_host_cd_dir(void)   { return host_cd_dir(); }
const char *mux_br_host_cd_dir(void)    { return host_cd_dir(); }
const char *mux_tgr_host_resource_dir(void) { return host_resource_dir(); }
const char *mux_br_host_resource_dir(void)  { return host_resource_dir(); }
const char *mux_tgr_host_music_dir(void) { return host_music_dir(); }
const char *mux_br_host_music_dir(void)  { return NULL; }
