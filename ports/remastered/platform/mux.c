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
 *             owns the window it presents to, an OpenGL context the pixel
 *             format it draws with)
 *   sound     one output at the first game's rate (Top Gear Rally's), each
 *             game's mixer added in, the other rate's resampled, a limiter
 *             over the sum.  The music is the Remaster Options' (options.c,
 *             mux_music): the N64's is Top Gear Rally's (src/racing/
 *             handoff.c plays its course's piece under a race), the PC's
 *             Boss Rally's CD (its front end's track under Top Gear Rally's
 *             menus, a race's own pick under a race).  Boss Rally is heard
 *             while it is in front, and its CD alone behind the menus when
 *             the PC's music is chosen
 *   folders   the app's saves (main.c names the app), Boss Rally's in a
 *             folder of its own inside them */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "host.h"
#include "mux.h"
#include "options.h"
#include "controls.h"

int tgr_flag(int flag);                 /* the Top Gear Rally object's (os/view.c) */
#define TGR_FLAG_ANY_ASPECT 0           /* tgr_view.h */

/* the two objects' music (link.sh exports them) */
void  tgr_music_mute(int on);           /* platform/audio/mixer.c */
void  br_music_gain(float g);           /* platform/common/audio.c */
void  br_effects_quiet(int on);
float br_music_volume(void);
static void music_targets(int snap);

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
        music_targets(1);               /* the other game's music at once, not faded */
    }
    host_cond_broadcast(s_c);
    host_mutex_unlock(s_m);
}

int mux_front_game(void) { return s_front; }

/* ---- the window ----------------------------------------------------------------- */
static int s_open;                      /* Top Gear Rally opened the real window */
static int s_win_w = 1, s_win_h = 1;    /* the size it was opened at: the mouse's units */
static volatile int s_visible;          /* the real window can be seen (read on its thread) */
static int s_freed;                     /* its shape left to the player */

#ifdef _WIN32
/* Boss Rally's child window: the size of the client area, shown while Boss
 * Rally is in front and has asked for it (its renderer draws into it) */
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
    wc.style = CS_OWNDC;              /* one DC for the window's life (an OpenGL context keeps it) */
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

/* on the window's thread, each time its loop polls.  The window is any shape
 * the player makes it: either game's picture is fitted inside it (the races
 * filling it with Widescreen on), so neither game's wish to lock its shape
 * is passed on */
static void window_keep(void)
{
    if (s_open && !s_freed) {
        host_window_lock_aspect(0);
        s_freed = 1;
    }
    s_visible = host_window_visible();
#ifdef _WIN32
    child_keep();
#endif
}

void rm_br_start(void);                 /* main.c */

int mux_tgr_host_window_open(int width, int height, const char *title)
{
    int r;
    /* 16:9 with Widescreen on (a race fills it; the menus keep the N64's 4:3
       inside), else the games' 4:3 */
    if (tgr_flag(TGR_FLAG_ANY_ASPECT))
        width = (height * 16 + 4) / 9;
    (void)title;                        /* the app's (main.c) */
    r = host_window_open(width, height, NULL);
    s_open = r;
    s_win_w = width > 0 ? width : 1;
    s_win_h = height > 0 ? height : 1;
    s_visible = host_window_visible();
#ifdef _WIN32
    if (r)
        child_open();
#endif
    /* Boss Rally started behind the menus (the PC's music there is its front
       end's), now that the window is the app's: its own opening of one finds
       it there (headless, it starts at the first race) */
    if (r)
        rm_br_start();
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

void mux_tgr_host_window_lock_aspect(int lock) { (void)lock; }
void mux_br_host_window_lock_aspect(int lock)  { (void)lock; }

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
#elif defined(__APPLE__)
/* headless (HOST=null) with Metal: no window, no layer; the renderers draw
   offscreen at the game's size */
void *host_macos_metal_layer(void) { return 0; }
void host_macos_layer_fit(int *w, int *h) { *w = *h = 0; }
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
void br_race_click(float nx, float ny);  /* race_handoff.c: Boss Rally's pause menu */

static void route(const host_event *e)
{
    static int down;
    if (e->type == HOST_EV_MOUSE) {     /* a click on Boss Rally's pause menu chooses */
        if (e->buttons & 1 && !down && s_front == MUX_BR)
            br_race_click((float)e->x / s_win_w, (float)e->y / s_win_h);
        down = e->buttons & 1;
    }
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

/* the second player's pad (controls.h): the host's i-th, likewise the front game's */
static int pad_read_n(int g, int i, host_pad *p)
{
    if (s_front == g)
        return i == 0 ? host_pad_read(p) : host_pad_read_n(i, p);
    memset(p, 0, sizeof *p);
    p->pov = -1;
    return 0;
}
int mux_tgr_host_pad_read_n(int i, host_pad *p) { return pad_read_n(MUX_TGR, i, p); }
int mux_br_host_pad_read_n(int i, host_pad *p)  { return pad_read_n(MUX_BR, i, p); }

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

/* ---- the music ---------------------------------------------------------------------- */
/* The N64's music up to the PC's: Top Gear Rally mixes its music at its music
 * table's top step (48 of 255, its effects 255) far under its effects.  Its
 * five race pieces, as this mixer gets them under a race, measured -31.2,
 * -32.8, -33.0, -29.8 and -28.3 LUFS integrated (EBU R128, 600 s each, peaks
 * -17.2 dBFS at most); Boss Rally's eleven race tracks (CD 3..13) -15.52 LUFS
 * by their energy's mean, the N64's -30.64: 15.12 dB, x5.70.  Top Gear Rally's
 * whole output takes it, its menus' effects with its music, so the N64's own
 * balance holds; its loudest piece then peaks at -2.1 dBFS. */
#define N64_GAIN 5.70f
#define FADE_S 0.5f                     /* a change of music crossfades over this */

static volatile int s_music = RM_MUSIC_N64;
static volatile float s_tgr_want = N64_GAIN;
static float s_tgr_gain = N64_GAIN;     /* Top Gear Rally's output, moving to s_tgr_want */
static float s_tgr_to = -1, s_tgr_step; /* where it is moving, and by how much a frame: there in FADE_S */

/* who sounds the music now: Top Gear Rally's mixer and gain, Boss Rally's
 * CD; snap: at once (a change of game in front), else crossfaded */
static void music_targets(int snap)
{
    int race = s_front == MUX_BR, n64 = s_music == RM_MUSIC_N64;
    /* the menus' N64 music, or nothing; under a race Top Gear Rally plays its
       piece and the gain decides */
    tgr_music_mute(!race && !n64);
    br_music_gain(n64 ? 0.0f : 1.0f);
    br_effects_quiet(!race);
    s_tgr_want = race ? (n64 ? N64_GAIN : 0.0f) : N64_GAIN;
    if (snap)
        s_tgr_gain = s_tgr_want * (race ? br_music_volume() : 1.0f);
}

void mux_music(int which)
{
    s_music = which;
    music_targets(0);
}

/* the sum's peaks held under full scale: unchanged to 0.9, then a smooth
 * knee to 1 */
static float limit(float x)
{
    float a = fabsf(x);
    if (a <= 0.9f)
        return x;
    a = 0.9f + 0.1f * tanhf((a - 0.9f) / 0.1f);
    return x < 0 ? -a : a;
}

static void mix(float *lr, int frames, void *user)
{
    float *out = lr;
    int all = frames, i;
    (void)user;
    memset(lr, 0, sizeof(float) * 2 * (size_t)frames);
    while (frames > 0) {
        int n = frames < CHUNK ? frames : CHUNK, g;
        for (g = 0; g < MUX_COUNT; g++) {
            if (!s_a[g].fn)
                continue;
            if (g == MUX_BR && s_front != MUX_BR && s_music != RM_MUSIC_PC)
                continue;               /* Boss Rally is silent between races, but for its CD */
            if (g == MUX_TGR) {         /* Top Gear Rally at its gain */
                static float tmp[2 * CHUNK];
                /* under a race the N64's music follows Boss Rally's music volume, as its CD does */
                float want = s_tgr_want * (s_front == MUX_BR ? br_music_volume() : 1.0f), step;
                if (want != s_tgr_to) {
                    s_tgr_to = want;
                    s_tgr_step = fabsf(want - s_tgr_gain) / (FADE_S * (float)s_rate);
                }
                step = s_tgr_step;
                memset(tmp, 0, sizeof(float) * 2 * (size_t)n);
                add_game(g, tmp, n);
                for (i = 0; i < n; i++) {
                    s_tgr_gain += s_tgr_gain < want ? fminf(step, want - s_tgr_gain) : -fminf(step, s_tgr_gain - want);
                    lr[2 * i] += tmp[2 * i] * s_tgr_gain;
                    lr[2 * i + 1] += tmp[2 * i + 1] * s_tgr_gain;
                }
                continue;
            }
            add_game(g, lr, n);
        }
        lr += 2 * n;
        frames -= n;
    }
    for (i = 0; i < 2 * all; i++)
        out[i] = limit(out[i]);
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

/* the real host up: the kept Remaster Options applied */
void mux_tgr_host_init(int argc, char **argv)
{
    init_once();
    host_init(argc, argv);
    rm_options_load();
    rm_controls_load();
    music_targets(1);
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
const char *mux_br_host_music_dir(void)  { return host_music_dir(); }   /* the PC's music */
