/* host.h -- the macOS platform under the translated game (port code).
 *
 * Every function here implements one import of the original BRGlide.dll
 * (Win32, the MSVC 5 CRT it links as msvcrt.dll, Glide 2, DirectX COM,
 * winmm), with the C signature w2c.py derived from the game's own
 * declarations: every pointer is a u32 GAME address, reached through W_P().
 * tools/brbox_imports.py is the behavioural reference -- the live oracle the
 * T3 build was certified under answers each import the same way.
 */
#ifndef HOST_H
#define HOST_H
#include "w2c_rt.h"
#include <stdio.h>

#define HS(a) ((char *)W_P(a))                   /* game C string */
#define H32(a) (*(w_u32 *)W_P(a))
#define HW32(a, v) (*(w_u32 *)W_P(a) = (u32)(v))
#define HW16(a, v) (*(w_u16 *)W_P(a) = (u16)(v))

const w_fentry *w_lookup_quiet(u32 a);

/* heap in game memory */
u32 hmem_alloc(u32 n, int zero);
void hmem_free(u32 a);
u32 hmem_size(u32 a);
u32 hmem_realloc(u32 a, u32 n);
u32 hstrdup(const char *s);

/* virtual file system: C:\BOSSRALLY\ and D:\ (the CD) both read the disc
 * tree; writes go to an overlay directory. */
void vfs_init(const char *disc, const char *save);
int vfs_resolve(const char *gamepath, char *host, size_t n, int forwrite);
const char *vfs_cwd(void);
int vfs_chdir(const char *p);

/* printf-family over a wasm varargs block */
int hfmt(char *out, size_t n, const char *fmt, u32 va);

/* COM objects the host implements */
typedef struct { const char *name; int nargs; void *fn; } hcom_method;
u32 hcom_new(const char *iface, const hcom_method *m, int n, void *state);
void *hcom_state(u32 obj);

/* logging */
extern int g_hlog;
#define HLOG(...) do { if (g_hlog) fprintf(stderr, __VA_ARGS__); } while (0)

/* window/event side, implemented in host_app.m */
void happ_init(void);
void happ_pump(int block_ms);
int happ_key_down(int vk);
void happ_dik_state(u8 *out256);
extern volatile int g_happ_quit;

/* windows message queue (host_win.c) */
void hwin_post(u32 hwnd, u32 msg, u32 wp, u32 lp);
u32 hwin_main_hwnd(void);

#endif
