/* host.h: what each operating system supplies under the platform layer.
 *
 * The game core is written against Win32, winmm, DirectX and Glide
 * (platform/include). platform/common implements that surface once, for
 * every OS, on top of this small interface; each OS implements only this:
 *
 *   host/posix    time, threads, files, network, processes (macOS, Linux)
 *   host/win32    the same on Windows
 *   host/null     a headless window, input and audio (tests, lockstep)
 *   host/macos    a Cocoa window and input, Core Audio
 *   host/windows  a Win32 window and input, WASAPI, Media Foundation, XInput
 *
 * and a renderer backend behind platform/render/brr.h. On Windows the
 * game's Win32 emulation takes private names (platform/include/br_winemu.h)
 * so it never collides with the system the host calls.
 */
#ifndef BR_HOST_H
#define BR_HOST_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- time ----------------------------------------------------------------- */
uint64_t host_ticks_ns(void);                 /* monotonic, arbitrary origin */
void     host_sleep_ms(uint32_t ms);

/* ---- threads and synchronisation ------------------------------------------ */
typedef struct host_mutex  host_mutex;        /* recursive */
typedef struct host_cond   host_cond;
typedef struct host_thread host_thread;

host_mutex  *host_mutex_new(void);
void         host_mutex_lock(host_mutex *m);
void         host_mutex_unlock(host_mutex *m);
void         host_mutex_free(host_mutex *m);
host_cond   *host_cond_new(void);
/* 0 when signalled, 1 on timeout; timeout_ms 0xFFFFFFFF waits forever */
int          host_cond_wait(host_cond *c, host_mutex *m, uint32_t timeout_ms);
void         host_cond_broadcast(host_cond *c);
void         host_cond_free(host_cond *c);
host_thread *host_thread_start(void *(*fn)(void *), void *arg);
void         host_thread_exit(void);
uintptr_t    host_thread_self(void);
/* the calling thread keeps time for the game (paces frames, feeds audio):
   ask the OS to wake it on time (no timer coalescing) */
void         host_thread_interactive(void);

/* ---- files ------------------------------------------------------------------ */
typedef struct host_dir host_dir;
host_dir   *host_dir_open(const char *path);
/* the next entry's name, NULL at the end; *is_dir and *size describe it */
const char *host_dir_next(host_dir *d, int *is_dir, uint64_t *size);
void        host_dir_close(host_dir *d);
int         host_mkdir(const char *path);
/* where the game's files are: the install (C:\BOSSRALLY), the CD (D:\)
 * and the player's saves and settings */
const char *host_game_dir(void);
const char *host_cd_dir(void);
const char *host_save_dir(void);
/* where the program's own files are: a macOS app's Resources, else the
 * executable's folder (BR_RESDIR overrides) */
const char *host_resource_dir(void);

/* ---- window and input --------------------------------------------------------- */
enum {
    HOST_EV_NONE,
    HOST_EV_KEY,          /* vk (Win32 virtual key), down, scan (DirectInput DIK) */
    HOST_EV_CHAR,         /* ch */
    HOST_EV_MOUSE,        /* x, y, buttons */
    HOST_EV_FOCUS,        /* down: 1 gained, 0 lost */
    HOST_EV_CLOSE
};
typedef struct host_event {
    int type;
    int vk, scan, down, ch;
    int x, y, buttons;
} host_event;

int  host_window_open(int width, int height, const char *title);
void host_window_close(void);
/* 1 while any of the window can be seen (not minimised, hidden or covered) */
int  host_window_visible(void);
/* 1: the window keeps the game's shape as it is resized (the original's
 * 4:3); 0: any shape (BR_FLAG_ANY_ASPECT) */
void host_window_lock_aspect(int lock);
/* the next event, waiting up to wait_ms for one; 1 if *ev was filled */
int  host_poll_event(host_event *ev, uint32_t wait_ms);
void host_message_box(const char *text, const char *caption);
/* a finished frame of ARGB pixels (0xAARRGGBB), top row first, for the window */
void host_present(const uint32_t *argb, int w, int h);

/* ---- audio out ------------------------------------------------------------------ */
/* the mixer callback fills frames of interleaved stereo float */
typedef void (*host_audio_fn)(float *lr, int frames, void *user);
int  host_audio_open(int rate, host_audio_fn fn, void *user);
void host_audio_close(void);

/* ---- decoded audio files (the CD's music) ------------------------------------------ */
/* where the CD audio tracks are, as track02.flac .. trackNN.* (NULL: none) */
const char *host_music_dir(void);
/* a file decoded to interleaved stereo float at rate; NULL when this host
 * cannot decode it */
typedef struct host_stream host_stream;
host_stream *host_stream_open(const char *path, int rate);
/* up to frames frames into lr; the number read, 0 at the end */
int          host_stream_read(host_stream *s, float *lr, int frames);
void         host_stream_close(host_stream *s);

/* ---- game controllers ------------------------------------------------------------- */
/* the first game controller, in the layout DirectInput gives an XInput pad:
 *   x, y     left stick, -1..1, y positive downward
 *   rx, ry   right stick, likewise
 *   z        right trigger minus left trigger
 *   buttons  0 A  1 B  2 X  3 Y  4 LB  5 RB  6 view  7 menu  8 L3  9 R3
 *            10 LT  11 RT (past half)  12-15 d-pad up/right/down/left
 *   pov      the d-pad in hundredths of a degree clockwise from up, -1 centred
 * 0 when none is connected (the state then reads centred) */
typedef struct host_pad {
    float    x, y, z;
    float    rx, ry;
    unsigned buttons;
    int      pov;
} host_pad;
int host_pad_read(host_pad *p);

/* ---- network: IPv4 UDP ---------------------------------------------------------- */
typedef struct host_addr {
    uint32_t ip;          /* host byte order */
    uint16_t port;
} host_addr;
typedef struct host_sock host_sock;
/* a datagram socket on port (0: any free one); group != 0 also joins that
 * multicast group, with the port shared among the processes on this machine */
host_sock *host_udp_open(uint16_t port, uint32_t group);
/* the port it was given */
uint16_t   host_udp_port(host_sock *s);
int        host_udp_send(host_sock *s, const host_addr *to, const void *p, int n);
/* one datagram into p: its size, or -1 when none arrives within wait_ms */
int        host_udp_recv(host_sock *s, host_addr *from, void *p, int cap, uint32_t wait_ms);
void       host_udp_close(host_sock *s);

/* ---- processes ------------------------------------------------------------------- */
/* run this program again with the environment changed by env ("NAME=value"
 * entries, NULL-terminated) and its output to log; an id for host_kill, 0
 * when it cannot */
intptr_t host_spawn_self(const char *const *env, const char *log);
/* stop it: let it finish on its own for up to grace_ms, then end it */
void     host_kill(intptr_t id, uint32_t grace_ms);

/* ---- process ---------------------------------------------------------------------- */
/* the program's name, before host_init: dir names the save folder (default
 * "Boss Rally 64"), title the menus and dialogs (default "Boss Rally") */
void host_set_app_name(const char *dir, const char *title);
void host_init(int argc, char **argv);
void host_shutdown(void);

#ifdef __cplusplus
}
#endif
#endif
