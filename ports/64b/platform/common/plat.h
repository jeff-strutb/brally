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
