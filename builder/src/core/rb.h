/* rb.h: the release builder's core, shared by the macOS and Windows front ends.
 *
 * The builder turns the player's own copy of a game into a native build of
 * it: the game's code ships inside the builder (built from ports/brally and
 * ports/tgrally with none of the original's data in it), and the data comes
 * from the disc image or ROM the player provides, checked by MD5.
 *
 *   Boss Rally      the BIN/CUE of the retail PC CD: the data track's files
 *                   (BRGlide.dll's data is read from there at start-up) and
 *                   the twelve CD audio tracks, encoded as FLAC
 *   Top Gear Rally  the N64 cartridge ROM (USA): its data region, from ROM
 *                   0x70AB0 to the end, as romdata.bin
 *
 * Neither result needs the disc image or ROM once built.
 *
 * Paths are UTF-8 everywhere. Every long call reports progress through an
 * rb_progress and stops early (with an error) when *cancel becomes nonzero.
 */
#ifndef RB_H
#define RB_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { RB_BOSS_RALLY, RB_TOP_GEAR_RALLY };

typedef void (*rb_progress)(void *ctx, double fraction, const char *status);

/* the dumps the builder accepts */
#define RB_BR_BIN_MD5  "31c64f9b1e09788c2dfc384b44af8f6c"   /* BossRally.BIN */
#define RB_BR_CUE_MD5  "a48a4a5860558177c3041afee57e03c9"   /* BossRally.cue */
#define RB_TGR_ROM_MD5 "6f7030284b6bc84a49e07da864526b52"   /* Top Gear Rally (USA), .z64 order */

const char *rb_game_name(int game);           /* "Boss Rally" / "Top Gear Rally" */
/* what the build is called in the destination folder: "Boss Rally.app" on
 * macOS, the folder "Boss Rally" on Windows */
const char *rb_output_name(int game);

/* ---- checking what the player provides ---------------------------------------- */
typedef struct rb_check {
    int  ok;                  /* the file is the dump the builder reads */
    char md5[33];             /* the file's MD5, as it is */
    char note[256];           /* why it is not, or how it was taken (byte order) */
} rb_check;

/* the BIN image (MD5 over the whole file) */
int rb_check_bin(const char *path, rb_check *out, rb_progress cb, void *ctx, volatile int *cancel);
/* the cue sheet: its MD5, or else a track layout the same as the retail
 * disc's (a cue written by another ripper names the BIN differently) */
int rb_check_cue(const char *path, rb_check *out);
/* the ROM in any of the three byte orders (.z64, .v64, .n64): its MD5 as it
 * is, and in .z64 order against RB_TGR_ROM_MD5 */
int rb_check_rom(const char *path, rb_check *out, rb_progress cb, void *ctx, volatile int *cancel);
/* the BIN a cue sheet names, beside it: 1 and the path in out */
int rb_cue_bin_path(const char *cue, char *out, size_t n);

/* ---- building ---------------------------------------------------------------------- */
typedef struct rb_job {
    int         game;
    const char *bin, *cue;    /* Boss Rally */
    const char *rom;          /* Top Gear Rally */
    const char *dest_dir;     /* the folder the build goes into */
} rb_job;

/* 1 when dest_dir already holds this game's output; *ours is 1 when this
 * builder made it (it then may be replaced) */
int rb_output_exists(const rb_job *job, int *ours);
/* the whole build; 1 on success, else 0 with the reason in err */
int rb_build(const rb_job *job, rb_progress cb, void *ctx, volatile int *cancel, char *err, size_t errlen);
/* where the build went (dest_dir/rb_output_name) */
void rb_output_path(const rb_job *job, char *out, size_t n);

/* ---- supplied by each front end -------------------------------------------------- */
/* write the game executable shipped inside the builder (name: "brally64" or
 * "tgrally") to path; 1 on success */
int rb_host_payload(const char *name, const char *path, char *err, size_t errlen);

/* ---- utilities the core shares (util.c, md5.c) ---------------------------------- */
typedef struct rb_md5 { uint32_t a, b, c, d; uint64_t len; uint8_t buf[64]; } rb_md5;
void rb_md5_init(rb_md5 *m);
void rb_md5_update(rb_md5 *m, const void *p, size_t n);
void rb_md5_final(rb_md5 *m, uint8_t out[16]);
void rb_md5_hex(const uint8_t d[16], char out[33]);

#ifdef __cplusplus
}
#endif
#endif
