/* host.h: what each operating system supplies under the platform layer.
 *
 * The game core is written against Win32, winmm, DirectX and Glide
 * (platform/include). platform/common implements that surface once, for
 * every OS, on top of this small interface; each OS implements only this:
 *
 *   host/posix    time, threads, files     (macOS, Linux)
 *   host/null     a headless window, input and audio (tests, lockstep)
 *   host/macos    a Cocoa window and input, Core Audio
 *
 * and a renderer backend behind platform/render/brr.h. A Windows build
 * implements the same interface (or hands win32.h to the real system).
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

/* ---- process ---------------------------------------------------------------------- */
void host_init(int argc, char **argv);
void host_shutdown(void);

#ifdef __cplusplus
}
#endif
#endif
