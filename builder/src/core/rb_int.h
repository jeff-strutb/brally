/* rb_int.h: what the core's files share. */
#ifndef RB_INT_H
#define RB_INT_H

#include <stdio.h>
#include "rb.h"

#ifdef _WIN32
#define RB_SEP '\\'
#else
#define RB_SEP '/'
#endif

/* ---- files (util.c): UTF-8 paths on every system ---------------------------------- */
FILE    *rb_fopen(const char *path, const char *mode);
int      rb_seek(FILE *f, uint64_t off);              /* 1 on success */
uint64_t rb_file_size(const char *path);              /* 0 when missing */
int      rb_is_dir(const char *path);
int      rb_exists(const char *path);
int      rb_mkdirs(const char *path);                 /* mkdir -p; 1 on success */
int      rb_rmtree(const char *path);                 /* rm -rf; 1 on success */
int      rb_rename(const char *from, const char *to);
int      rb_write_file(const char *path, const void *p, size_t n);
void    *rb_read_file(const char *path, size_t *n);   /* malloc'd; NULL when unreadable */
void     rb_join(char *out, size_t n, const char *a, const char *b);
void     rb_dirname(char *out, size_t n, const char *path);
int      rb_set_exec(const char *path);               /* chmod 755 (POSIX) */

/* ---- the disc (disc.c) --------------------------------------------------------------- */
typedef struct rb_track { int number, audio; uint32_t start, count; } rb_track;   /* sectors */
typedef struct rb_cue { char file[1024]; int ntracks; rb_track t[99]; } rb_cue;
int rb_cue_parse(const char *path, rb_cue *cue, uint64_t bin_size, char *err, size_t errlen);
/* the data track's files into dir (directx/ left out) */
int rb_extract_data_track(const char *bin, const char *dir, rb_progress cb, void *ctx, double lo, double hi,
                          volatile int *cancel, char *err, size_t errlen);

/* ---- FLAC (flac.c) ------------------------------------------------------------------- */
/* stereo 16-bit 44.1 kHz little-endian PCM of `frames` frames, read from f at
 * its position, to a FLAC file; 1 on success */
int rb_flac_encode(FILE *f, uint64_t frames, const char *path, rb_progress cb, void *ctx, double lo, double hi,
                   volatile int *cancel, char *err, size_t errlen);

/* ---- icons (icon.c) ------------------------------------------------------------------ */
/* an .ico's largest image as an .icns (PNG entries, nearest-neighbour scaled) */
int rb_ico_to_icns(const char *ico, const char *icns, char *err, size_t errlen);
#ifdef _WIN32
/* the .ico's images as the exe's icon resource */
int rb_set_exe_icon(const char *exe, const char *ico, char *err, size_t errlen);
#endif

void rb_err(char *err, size_t errlen, const char *fmt, ...);

#endif
