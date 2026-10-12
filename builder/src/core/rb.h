/* rb.h: the release builder's core, shared by the macOS and Windows front ends.
 *
 * The builder turns the player's own copies of the two games into a native
 * build of Top Gear Rally Remastered (ports/remastered: Top Gear Rally's menus,
 * Boss Rally's engine running the races). The code ships inside the builder,
 * built with none of the originals' data in it; the data comes from the dumps
 * the player provides, checked by MD5:
 *
 *   Top Gear Rally  the N64 cartridge ROM (USA): its data region, from ROM
 *                   0x70AB0 to the end, as romdata.bin
 *   Boss Rally      the BIN/CUE of the retail PC CD: the data track's files
 *                   (BRGlide.dll's data is read from there at start-up)
 *
 * The result needs neither dump once built. It is for the system the builder
 * runs on (Windows: Windows; macOS: the Mac, or the iPhone as an .ipa).
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

/* what the build is for */
enum { RB_MACOS, RB_IOS, RB_WINDOWS };

typedef void (*rb_progress)(void *ctx, double fraction, const char *status);

/* the dumps the builder accepts */
#define RB_BR_BIN_MD5  "31c64f9b1e09788c2dfc384b44af8f6c"   /* BossRally.BIN */
#define RB_BR_CUE_MD5  "a48a4a5860558177c3041afee57e03c9"   /* BossRally.cue */
#define RB_TGR_ROM_MD5 "6f7030284b6bc84a49e07da864526b52"   /* Top Gear Rally (USA), .z64 order */

#define RB_PRODUCT "Top Gear Rally Remastered"
const char *rb_target_name(int target);       /* "Mac" / "iPhone" / "Windows" */
/* what the build is called in the destination folder: "Top Gear Rally
 * Remastered.app" (Mac), ".ipa" (iPhone), the folder (Windows) */
const char *rb_output_name(int target);

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
    int         target;       /* RB_MACOS / RB_IOS / RB_WINDOWS */
    const char *bin, *cue;    /* Boss Rally */
    const char *rom;          /* Top Gear Rally */
    const char *dest_dir;     /* the folder the build goes into */
} rb_job;

/* 1 when dest_dir already holds this target's output; *ours is 1 when this
 * builder made it (it then may be replaced) */
int rb_output_exists(const rb_job *job, int *ours);
/* the whole build; 1 on success, else 0 with the reason in err */
int rb_build(const rb_job *job, rb_progress cb, void *ctx, volatile int *cancel, char *err, size_t errlen);
/* where the build went (dest_dir/rb_output_name) */
void rb_output_path(const rb_job *job, char *out, size_t n);

/* the iPhone build's signing: the player's own Xcode signing when this Mac
 * has it (a certificate with a provisioning profile for it): 1 and its
 * description ("Apple Development: Name (ID)") in desc; 0 when the build will
 * be an unsigned .ipa. macOS only. */
int rb_ios_signing(char *desc, size_t n);

/* ---- supplied by each front end -------------------------------------------------- */
/* where a piece shipped inside the builder is (a file, or the iPhone app's
 * folder): "remastered" (the game for this system), "icon" (its icon:
 * AppIcon.icns or icon.ico), "ios-app", "ios-sign"; 1 on success */
int rb_host_payload(const char *name, char *path, size_t n, char *err, size_t errlen);

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
