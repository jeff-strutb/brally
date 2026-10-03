/* plat.h: shared internals of the platform layer (platform/common). */
#ifndef BR_PLAT_H
#define BR_PLAT_H

#include <stdio.h>
#include "win32.h"
#include "host.h"

/* ---- paths -------------------------------------------------------------- */
/* A game path (C:\BOSSRALLY\..., D:\..., or relative to the current
 * directory) to a host path. Name lookup is case-insensitive, as on the
 * original's file system. 1 when the file or directory exists. */
int  plat_path(const char *game, char *host, size_t n);
/* the same for a file about to be created: its directory must exist */
void plat_path_new(const char *game, char *host, size_t n);
int  plat_drive(void);                  /* current drive, 3 = C: */
int  plat_chdrive(int drive);
int  plat_chdir(const char *game);
void plat_getcwd(char *out, size_t n);  /* as the game sees it, "C:\..." */
#define PLAT_CD_DRIVE 4                 /* D: */
#define PLAT_CD_LABEL "Boss Rally"

/* ---- logging -------------------------------------------------------------- */
extern int g_plat_log;
#define PLOG(...) do { if (g_plat_log) fprintf(stderr, __VA_ARGS__); } while (0)

/* ---- windows and input (win_user.c) --------------------------------------- */
void  plat_pump(uint32_t wait_ms);       /* host events -> window messages */
void  plat_mark_main_thread(void);      /* win_kernel.c: BR_VCLOCK's clock moves on this thread */
int   plat_vclock(void);                /* BR_VCLOCK set */
int   plat_vclock_main(void);           /* ... and this is the main thread */
void  plat_vclock_advance(uint64_t us);
void  plat_vclock_import(void);         /* BR_VCLOCK_IMPORTS: an import costs a tick */
void  plat_vclock_imports(int on);
void  plat_vclock_frame(void);          /* ... and a frame costs 1/30 s */
uint16_t plat_peersync_listen(void);    /* peersync.c: a port for a scripted peer */
void  plat_peersync(uint64_t us);       /* ... keep the two virtual clocks together */
void  plat_peersync_end(void);          /* ... and let the other run on alone */
DWORD plat_time_ms(void);              /* timeGetTime's value, not spending a BR_VCLOCK tick */
int   plat_audio_start(void);           /* audio.c: the output; 1 when a device mixes */
void  plat_cd_tracks(int *first, int *last);
int   plat_cd_play(int from, int to, void (*end_fn)(void *), void *user);
void  plat_cd_stop(void);
void  plat_cd_pause(int on);
void  plat_cd_volume(float v);
int   plat_cd_current(void);
int   plat_cd_playing(void);
void  plat_cd_poll(void);
void  plat_deliver(const host_event *ev); /* one host event -> window messages, key state */
void  plat_mouse_move(int dx, int dy);    /* dx.c: DirectInput mouse */
void  plat_mouse_button(int down);
/* the window's pointer at x,y (the game's 640x480): the next polls steer the
 * game's own cursor there (dx.c) */
void  plat_mouse_abs(int x, int y);
/* glide.c: a pointer position the host reports stretched over the window,
 * to the game's pixels through the picture as it is drawn there */
void  plat_pointer_to_game(int *x, int *y);
/* script_game.c: where the game's menu cursor is (0 before it exists) */
int   plat_game_cursor(int *x, int *y);
void  plat_app_frame(void);               /* script.c: BrAppFrame's entry */
HWND  plat_main_window(void);

/* ---- modules (win_kernel.c) ------------------------------------------------- */
/* a module the platform provides by name (ddraw.dll, dsound.dll ...) */
typedef struct { const char *name; void *fn; } plat_export;
void plat_register_module(const char *dll, const plat_export *ex, int n);

/* ---- resources (win_rsrc.c) ------------------------------------------------- */
/* a PE file loaded for its resources; LoadStringA reads its string table */
void *plat_pe_open(const char *hostpath);
int   plat_pe_string(void *pe, unsigned id, char *out, int n);
void  plat_pe_close(void *pe);

#endif
