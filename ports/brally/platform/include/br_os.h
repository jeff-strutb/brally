/* br_os.h: what the game asks of the system (platform/common/br_os.c).
 *
 * The game runs a few threads (the network's, the sound tick's) and guards
 * its shared records with mutexes and events, waiting on one or two at a
 * time. An object is a small number, as the game keeps it in a 32-bit field
 * of its records; a wait answers which object it got (BR_OS_WAIT_OBJECT_0
 * plus its index), BR_OS_WAIT_TIMEOUT or BR_OS_WAIT_FAILED.
 *
 * The clock is milliseconds and a counter since the start; under BR_VCLOCK
 * both are virtual and a read on the main thread moves them (br_os.c).
 *
 * Force-included into the core. */
#ifndef BR_OS_H
#define BR_OS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BR_OS_FOREVER        0xFFFFFFFFu
#define BR_OS_WAIT_OBJECT_0  0u
#define BR_OS_WAIT_TIMEOUT   0x102u
#define BR_OS_WAIT_FAILED    0xFFFFFFFFu

/* objects */
void    *br_os_mutex(int owned);
void    *br_os_event(int manual, int initial);
void    *br_os_thread(uint32_t (*fn)(void *), void *arg, uint32_t *id);
int      br_os_set(void *event);
int      br_os_release(void *mutex);
uint32_t br_os_wait(void *h, uint32_t ms);
uint32_t br_os_wait_many(uint32_t n, void *const *h, int all, uint32_t ms);
int      br_os_close(void *h);
void     br_os_thread_exit(uint32_t code);

/* a lock kept in a block of the game's own (at least a pointer's size) */
void br_os_lock_init(void *cs);
void br_os_lock(void *cs);
void br_os_unlock(void *cs);
void br_os_lock_free(void *cs);

/* the clock */
uint32_t br_os_ms(void);
int      br_os_perf(int64_t *count);
int      br_os_perf_hz(int64_t *hz);
void     br_os_sleep(uint32_t ms);

/* the window and its messages: the numbers the game's window procedure
 * was written for (it handles these and passes the rest back to
 * br_os_default_proc) */
#define BR_MSG_CREATE       0x001u
#define BR_MSG_DESTROY      0x002u
#define BR_MSG_ACTIVATE     0x006u
#define BR_MSG_SETFOCUS     0x007u
#define BR_MSG_CLOSE        0x010u
#define BR_MSG_QUIT         0x012u
#define BR_MSG_SHOWWINDOW   0x018u
#define BR_MSG_ACTIVATEAPP  0x01Cu
#define BR_MSG_KEYDOWN      0x100u
#define BR_MSG_KEYUP        0x101u
#define BR_MSG_CHAR         0x102u
#define BR_MSG_SYSCOMMAND   0x112u
#define BR_MSG_TIMER        0x113u
#define BR_MSG_MOUSEMOVE    0x200u

typedef struct BrOsMsg {
    void     *hwnd;
    uint32_t  message;
    uintptr_t wParam;
    intptr_t  lParam;
    uint32_t  time;
    int32_t   x, y;
} BrOsMsg;
typedef intptr_t (*br_os_proc)(void *hwnd, uint32_t msg, uintptr_t wParam, intptr_t lParam);

void     *br_os_window_open(br_os_proc proc, int cx, int cy, const char *title);
int       br_os_window_show(void *hwnd, int cmd);
int       br_os_window_valid(void *hwnd);
void     *br_os_active_window(void);
intptr_t  br_os_default_proc(void *hwnd, uint32_t msg, uintptr_t wParam, intptr_t lParam);
int       br_os_post(void *hwnd, uint32_t msg, uintptr_t wParam, intptr_t lParam);
void      br_os_quit(int code);
int       br_os_peek(BrOsMsg *m);
int       br_os_get(BrOsMsg *m);
intptr_t  br_os_dispatch(const BrOsMsg *m);
void      br_os_wait_message(void);
uintptr_t br_os_timer(void *hwnd, uintptr_t id, uint32_t ms);
int       br_os_timer_kill(void *hwnd, uintptr_t id);
int16_t   br_os_key(int vk);
uint32_t  br_os_message_id(const char *name);
int       br_os_message_box(const char *text, const char *caption);

/* the game's text: the string table of the resource DLL on its CD */
void *br_os_strings_open(const char *path);
int   br_os_string(void *strings, unsigned id, char *out, int n);
void  br_os_strings_close(void *strings);

/* the machine */
void     br_os_debug(const char *s);
int      br_os_strlen(const char *s);
int      br_os_user_name(char *out, uint32_t *n);
uint32_t br_os_drive_type(const char *root);      /* 5 the CD, 3 the hard disk, 1 none */
int      br_os_volume_info(const char *root, char *label, uint32_t nlabel, uint32_t *serial,
                           uint32_t *maxlen, uint32_t *flags, char *fs, uint32_t nfs);

#ifdef __cplusplus
}
#endif
#endif
