/* br_uiimg.h -- THE UI IMAGE REGISTRY: the 145-entry table at Glide
 * 0x10AC53E8 that names every BMP the front end can blit, and the counter
 * that says how many of them are currently loaded.
 *
 * RESPONSIBILITY: drawing/ -- "turn geometry and images into pixels ...
 * textures, surfaces".  This is the SURFACE REGISTRY the menu blitter indexes.
 * It is filed next to br_surf.c because br_surf.c's chain is its only writer
 * of the +0x00 slot: 0x100583C0 walks this table, hands each entry's path to
 * 0x10001290 -> BrSurfFromBitmap, and stores the BrSurf back into +0x00.
 *
 * ==========================================================================
 * WHERE IT COMES FROM
 * ==========================================================================
 *
 * Glide 0x10056260 (D3D 0x1005D440), the 8,349-byte pre-loop gate RallyMain
 * calls at 0x1001CD25, builds this table.  br_uiboot.h owns that function in
 * full; this module owns the three things it leaves behind that outlive it --
 * the table, the loaded count, and two 16-bit slots beside them -- because
 * they are read by the drawing chain and not by the boot sequence.
 *
 * The record is EIGHT BYTES in the Glide build:
 *
 *     +0x00  BrSurf *  the decoded 16-bit surface, NULL until loaded
 *     +0x04  char   *  the 0x104-byte path buffer, "images\<name>.bmp"
 *
 * and the count and stride are pinned four independent ways:
 *
 *   1. 0x10056279  `mov ecx,0x122 / rep stosd` at 0x10AC53E8 -- 290 DWORDS,
 *      i.e. 1160 bytes.  (`rep stosd` counts DWORDS.  A constant read as
 *      bytes has already made a buffer a quarter of its size in this tree.)
 *   2. the 145 stores that follow run 0x10AC53EC, 0x10AC53F4, ... 0x10AC586C
 *      -- first at base+4, stride 8, last at base + 144*8 + 4, so
 *      base + 145*8 == 0x10AC5870 is exactly the end of the cleared block.
 *   3. 0x100583C0 walks "+0x04 to 0x10AC5874", and
 *      (0x10AC5874 - 0x10AC53EC) / 8 == 145 (br_surf.h says so independently).
 *   4. the sprite table br_uispr.h owns has 145 entries, one per image.
 *
 * THE D3D BUILD USES A DIFFERENT RECORD, and that is worth stating because
 * the two functions are otherwise the same 8,349 bytes instruction for
 * instruction.  D3D 0x1005D440 clears 0x106D dwords (16,820 bytes) at
 * 0x10A9E360 and puts the first path pointer at 0x10A9E3D0 == base + 0x70:
 * a 0x74-byte record with the path at +0x70, and 145 * 0x74 == 16,820 to the
 * byte.  Same 145 images, same order, same strings; 0x6C more bytes of
 * per-image state that the Glide build keeps inside glide2x instead.  Any
 * claim that "the record is 0x74 bytes" is therefore true of D3D and false
 * here -- see slice1_06.h, which is the D3D reading of the same function.
 *
 * ==========================================================================
 * NO NEW STORAGE FOR THE SAVE-FILE PATHS -- DELIBERATELY
 * ==========================================================================
 *
 * 0x10056260 also seeds two fixed buffers with "c:\RallySeason.dat" and
 * "c:\RallyGhost.dat" (Glide 0x117A6030 / 0x117A5F28, D3D 0x11782CD0 /
 * 0x11782BC8).  br_save.h records a live aliased-storage hazard on exactly
 * those two addresses -- slice1_06.c models each as a `const char *const` to
 * the .rdata LITERAL while slice2_24.c and slice2_25.c model them as the
 * mutable 0x104-byte buffers they really are -- and ends "the fix is to merge
 * the two models, not to add a third".
 *
 * So BrUiImgSavePathsInit takes the CALLER's buffers.  This module declares no
 * storage for either address, and the literals it copies are slice1_06.h's
 * g_pszBrRallySeasonDat / g_pszBrRallyGhostDat, which name the .rdata source
 * correctly.  Identifying the initialiser is the piece br_save.h's note was
 * missing: these buffers are not "initialised data", they are written here,
 * every time the gate runs.
 */
#ifndef BR_UIIMG_H
#define BR_UIIMG_H

#include <stddef.h>
#include <stdint.h>

#include "br_surf.h"     /* BrSurf -- what 0x100583C0 stores at +0x00 */

/* ==========================================================================
 * The table
 * ========================================================================== */

#define BR_UIIMG_COUNT        145     /* images, and sprite-table entries   */
#define BR_UIIMG_ORIG_STRIDE  8u      /* bytes per record IN THE ORIGINAL   */
#define BR_UIIMG_PATH_MAX     0x104u  /* the size passed to operator new    */

/* The Glide record.  Written with real pointers rather than at byte offsets:
 * on LP64 this struct is 16 bytes, not 8, which is exactly why nothing here
 * may use BR_UIIMG_ORIG_STRIDE for anything but arithmetic ABOUT the
 * original.  (CONVENTIONS.md, "Byte offsets are 32-bit-only".) */
typedef struct BrUiImg {
    BrSurf *pSurf;     /* +0x00  NULL until 0x100583C0 loads it            */
    char   *pszPath;   /* +0x04  points at BR_UIIMG_PATH_MAX bytes          */
} BrUiImg;

extern BrUiImg g_aBrUiImg[BR_UIIMG_COUNT];   /* 0x10AC53E8 */

/* 0x10AC5C2C -- HOW MANY IMAGES ARE LOADED, and the only slot in this module
 * that anything outside 0x10056260 writes.
 *
 * The teardown at 0x10058300 is where its type is settled and it is not
 * tidy: the guard reads it as a WORD (`cmp word ptr [0x10AC5C2C], di` with
 * di == 0, and the branch is `jbe`, i.e. UNSIGNED), while the loop bound
 * reads the same address as a DWORD and masks -- `mov eax,[0x10AC5C2C] /
 * and eax,0xFFFF`.  The mask is what makes uint16_t the right model: the
 * upper half is deliberately discarded by the only reader of the dword.
 * 0x100584E0 is the increment, `inc word ptr`. */
extern uint16_t g_cBrUiImgLoaded;    /* 0x10AC5C2C */

/* 0x10AC5D50 and 0x10AC5D54 -- two 16-bit slots zeroed in the same three
 * instructions as the count.  NOT A GUESS THAT THEY BELONG HERE and not a
 * claim that they are understood: a range scan of every constant-address
 * operand in BRGlide.dll's .text finds NO other instruction that touches
 * either.  They are written once, by 0x10056260, and never read.  They are
 * declared because the gate writes them and a transcription that dropped
 * them would be silently short; they are not given names beyond their
 * addresses because nothing in the image says what they mean. */
extern uint16_t g_wBrUiImgAC5D50;    /* 0x10AC5D50 */
extern uint16_t g_wBrUiImgAC5D54;    /* 0x10AC5D54 */

/* ==========================================================================
 * What 0x10056260 does to them
 * ========================================================================== */

/* 0x10056279 .. 0x1005629D -- the `rep stosd` of 0x122 dwords over the whole
 * table, then the three 16-bit stores.  Drops every pSurf AND every pszPath
 * without freeing either: re-running the gate (which RallyMain's state 4 does
 * on a mode change, 0x1001CE88) LEAKS the previous 145 path buffers and every
 * loaded surface.  Faithful; see br_uiboot.h. */
void BrUiImgTableClear(void);

/* The allocator seam.  0x10056260 reaches operator new through the import
 * thunk at 0x10074572 (`jmp [MSVCRT!??2@YAPAXI@Z]`), which does NOT zero --
 * see the DEVIATION note in br_uiimg.c, which contradicts slice1_06.c on
 * this point and shows its working. */
typedef struct BrUiImgAlloc {
    void *(*pfnAlloc)(void *pUser, uint32_t cb);   /* 0x10074572 */
    void  (*pfnFree)(void *pUser, void *p);        /* HOST-ONLY, see the .c */
    void  *pUser;
} BrUiImgAlloc;

/* 0x100562A3 .. 0x100581CC -- 145 repetitions of
 *   `push 0x104 / call 0x10074572 / mov [table + 8i + 4], eax / <inline
 *    strcpy of the literal>`.
 * The path strings come from slice1_06.h's g_apszBrUiAssets, which is the
 * same 145 strings in the same order read off the D3D build; this module
 * re-derived them from BRGlide.dll and the two lists agree exactly, as does
 * br_uispr.c's g_aBrUiSpriteName modulo the "images\" prefix.
 *
 * Returns the number of records left with a NULL path -- 0 when every
 * allocation succeeded.  The ORIGINAL DOES NOT CHECK; see the DEVIATION. */
int BrUiImgPathsInit(const BrUiImgAlloc *pAlloc);

/* Not in the original, which never frees these.  Present so a test can run
 * the gate more than once without leaking; see br_uiboot.h's note that the
 * leak is the original's behaviour and is preserved in the gate itself. */
void BrUiImgPathsFree(const BrUiImgAlloc *pAlloc);

/* 0x10058299 .. 0x100582E0 -- the two inlined strcpy's into the caller's
 * buffers.  `cbSeason` / `cbGhost` bound a copy the original does not bound;
 * pass BR_UIIMG_PATH_MAX for the real 0x104-byte buffers. */
void BrUiImgSavePathsInit(char *pszSeason, size_t cbSeason,
                          char *pszGhost,  size_t cbGhost);

/* Reset every global this module owns to its load-time value, so a test can
 * run the sequence more than once.  Not in the original -- the original gets
 * fresh .data from the loader.  Does NOT free; use BrUiImgPathsFree first. */
void BrUiImgResetForTest(void);

#endif /* BR_UIIMG_H */
