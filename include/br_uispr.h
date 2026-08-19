/* br_uispr.h -- THE UI SPRITE TABLE (0x100AB568 D3D / 0x100AAD08 Glide) and
 * the control's CHROME DISPATCH: what the game actually draws behind a menu
 * control, and how "selected" changes it.
 *
 * WHY THIS FILE EXISTS
 *
 * port/host/brally.c used to fill every control's hit rectangle with a flat
 * white quad and call that the control background.  It is not.  The game
 * BLITS A BITMAP, and which bitmap it blits is a per-control number that the
 * ported navigation code already computes and the host was throwing away.
 *
 * HOW THAT WAS ESTABLISHED, in the Glide build, from the four routines that
 * stand between "this control is on screen" and a pixel:
 *
 *   0x10048010  (control vtable +0x08, Glide 0x10041460) -- the draw dispatch
 *
 *       if ((flags28 & 1) == 0)          nothing
 *       else if (flags1C & 0x100000)     aText[0].pVtbl->pfn10(&aText[0])
 *                                        i.e. the control is a LABEL and its
 *                                        chrome is its text, nothing else
 *       else if (flags1C & 0x200000)     nothing
 *       else                             vtable +0x10
 *
 *     That first arm is why a text row must NOT get a background quad: the
 *     game never draws one for it.  Half of what the flat-quad harness drew
 *     was invented.
 *
 *   0x10047A10  (control vtable +0x10, Glide 0x10040E60) -- which sprite path
 *
 *       if (f296C == 0)  vtable +0x1C (draw at the control's own position)
 *       else             k = wStep;  w1E20C = aStepId[k];
 *                        vtable +0x18 (draw with the rect at p1E210 + k*0x10)
 *
 *     So +0x1E210 really is "the base of a stride-0x10 array" (br_ui.h
 *     ADJ-11): it is an array of RECTANGLES, one per step.  No builder in the
 *     corpus sets f296C, so this arm is dormant today; it is transcribed
 *     anyway because it is the only reader that pins the array's element type.
 *
 *   0x10047930  (control vtable +0x1C, Glide 0x10040D80) -- 79 bytes:
 *
 *       i = (int16)w1E20C;  if (i < 0) return 1;      // SIGNED -- 0xFFFF is
 *       e = SPRITE_TABLE + i*24;                      // "no sprite"
 *       blit(__ftol(x), __ftol(y), e->iImage, &e->rect, e->fBlit);
 *
 *   0x10047980  (control vtable +0x18, Glide 0x10040DD0) -- 65 bytes: the
 *       same call with the CALLER's rectangle in place of e->rect.
 *
 * and the blit itself, 0x10058380 (Glide, 53 bytes) -> 0x10001320 (206):
 * a 16-bit software blit of `rect` out of image[iImage] to (x, y) on the
 * surface at 0x10AC5D84, colour-keyed when the entry's +0x14 has bit 0.
 *
 * ==========================================================================
 * THE ANSWER TO "what is a selected control background"
 * ==========================================================================
 *
 * 0x10048180 (the control frame, Glide 0x100415D0) writes w1E20C once per
 * frame, and it writes one of TWO values, both gated on flags1C & 0x400000:
 *
 *     vtable+0x20 returned nonzero (the control is CURRENT), 0x10041670:
 *         w1E20C = aStepId[1]            <- the +0x2A42 four headers call a
 *                                           scalar; br_ui.h ADJ-4 already
 *                                           showed it is element 1
 *     vtable+0x20 returned zero,          0x1004182D:
 *         w1E20C = aStepId[0]            <- the a7 that 0x10047FB0 stored
 *
 * So a control carries a PAIR of sprites and the frame picks between them.
 * The pair is not an abstraction imposed here -- the shipped art is named for
 * it.  BrExt_10054B50 places three 127x33 rectangles with
 *
 *     a7 = 0x78, aStepId[1] = 0x79      sprites 120 / 121
 *     a7 = 0x52, aStepId[1] = 0x53      sprites  82 /  83
 *     a7 = 0x54, aStepId[1] = 0x55      sprites  84 /  85
 *
 * and those six sprites are, in the loader's own order,
 *
 *     but-sav.bmp / but-savd.bmp,  but-main.bmp / but-maind.bmp,
 *     but-op.bmp  / but-opd.bmp
 *
 * -- a button and its "d" (down) variant, three times, each exactly 127x33,
 * which is exactly the +0x7F / +0x21 rectangle every builder computes for
 * those controls.  Three independent facts landing on the same six numbers.
 *
 * ==========================================================================
 * THE STYLE POOL IS NOT THIS
 * ==========================================================================
 *
 * slice3_39.h's g_aBrUiStyle (0x100AB418, 21 x 16 bytes) sits IMMEDIATELY
 * BELOW this table -- 0x100AB418 + 21*16 == 0x100AB568 -- and is easy to
 * mistake for the thing that gets drawn.  It is not drawn at all.  Its
 * rectangles are consumed by 0x10047EB0 (which copies [0] and [2] into the
 * text box's left/right, i.e. the ALIGNMENT box), by 0x1005B910 (the list's
 * scroll geometry) and by 0x10047A60 (three of them as cursor hit rects).
 * Two adjacent stride-16 and stride-24 tables of rectangles, one for layout
 * and one for art.
 *
 * ==========================================================================
 * WHAT THIS MODULE DOES NOT HAVE: THE PIXELS
 * ==========================================================================
 *
 * The images are BMP files -- images\but-main.bmp and 144 others, loaded by
 * 0x10056260 into the stride-8 table at 0x10AC53E8 -- and this tree has no
 * copy of them.  Unlike the font, which lives in .data and can be recovered
 * from the DLL, the UI art is on the disc.  So this module supplies the
 * GEOMETRY, the SELECTION, the transparency flag and the file name for every
 * sprite, and a caller with no art draws a placeholder of the right size in
 * the right place.  That is a real limitation and it is named, not hidden.
 *
 * ==========================================================================
 * WHAT ALREADY EXISTED, AND WHY THIS IS NOT A SIXTH MODEL
 * ==========================================================================
 *
 * slice3_32.h reached the same table from the other end: its BrScrRectEnt is
 * this struct, derived independently and described in the same words ("entry
 * i is { i, {0,0,w,h}, flag }"), and slice3_32.c already carries byte-image
 * ports of 0x10047930, 0x10047980 and 0x100479D0 that index it. What it does
 * NOT have is STORAGE -- it reaches the table through a context pointer
 * (BrScrGlobals::aAB568) that no wiring layer had ever filled in, so those
 * three functions were unreachable.
 *
 * This module supplies the storage; port/host/brally.c points aAB568 at it.
 * There is one object and one owner. The two transcriptions of the draw slots
 * were made from the disassembly independently and agree, including on the
 * asymmetry noted at BrUiCtlChrome's second arm, which is the strongest thing
 * that can be said for either of them.
 *
 * ==========================================================================
 * ALIASED STORAGE -- one known overlap, not yet resolved
 * ==========================================================================
 *
 * slice3_39.c defines `int32_t g_BrSprRect46[4]` at 0x100AB9BC and
 * `g_BrSprRect48[4]` at 0x100AB9EC.  Those are entries 46 and 48 of THIS
 * table: 0x100AB568 + 46*24 + 4 == 0x100AB9BC and + 48*24 + 4 == 0x100AB9EC.
 * One original object, two host objects -- exactly the hazard CONVENTIONS.md
 * warns about.  Neither side writes them today, so they cannot have drifted,
 * and port/tests/test_uispr.c ASSERTS they still agree so that the day one of
 * them changes is the day a test fails rather than the day a scrollbar moves.
 * The fix is for slice3_39.c to alias into this table; that edit belongs to
 * whoever owns slice3_39.
 */
#ifndef BR_UISPR_H
#define BR_UISPR_H

#include <stdint.h>

#include "br_ui.h"      /* BrUiCtl_ -- the canonical control */

/* ==========================================================================
 * The table
 * ========================================================================== */

/* The base in the D3D build; the Glide build puts the same 3480 bytes at
 * 0x100AAD08.  Checked byte for byte: the two images are IDENTICAL over the
 * whole table, so nothing here depends on which build it was read from. */
#define BR_UI_SPR_BASE    0x100AB568u   /* D3D; Glide 0x100AAD08 */
#define BR_UI_SPR_STRIDE  24u           /* `lea eax,[eax+eax*2] / shl eax,3` */
#define BR_UI_SPR_COUNT   145

/* The 24 bytes the two draw slots read, in the original's field order:
 * +0x00 is loaded as a WORD (`mov ax,[eax+base]`) but the storage is a dword
 * -- the four bytes above it are zero in every entry. */
typedef struct BrUiSprite {
    int32_t iImage;      /* +0x00  index into the image table at 0x10AC53E8 */
    int32_t rect[4];     /* +0x04  left, top, right, bottom -- SOURCE rect  */
    int32_t fBlit;       /* +0x14  bit 0 -> colour-keyed blit (0x10001320)  */
} BrUiSprite;

extern const BrUiSprite g_aBrUiSprite[BR_UI_SPR_COUNT];   /* 0x100AB568 */

/* The BMP each entry names, recovered by walking 0x10056260 and pairing its
 * string loads with its stores into the image table.
 *
 * THE SEVEN GAPS ARE CLOSED.  This comment used to end "Seven entries could
 * not be paired and are NULL"; the array has been complete for a while and
 * the note was stale.  WHY there were exactly seven is worth keeping, because
 * it is a property of the compiler's output and not of the data: MSVC emits
 * `mov edi,<literal>` BEFORE the table store in 138 of the 145 blocks and
 * AFTER it in the other seven, so a pass that pairs each store with the
 * nearest EARLIER literal duplicates the previous name in exactly those
 * seven -- 16, 46, 63, 80, 97, 127 and 144.  Grouping on the
 * `call <operator new>` that opens each block instead recovers all 145.
 *
 * Independently re-derived from BRGlide.dll for port/src/drawing/br_uiimg.c;
 * that list, this one and slice1_06.c's g_apszBrUiAssets (read off BRD3D.dll)
 * agree on all 145, which port/tests/test_br_uiimg.c asserts entry by entry.
 * The geometry above is read straight out of the image and never depended on
 * the pairing either way. */
extern const char *const g_aBrUiSpriteName[BR_UI_SPR_COUNT];

/* Index by the ORIGINAL address, the way slice3_39.h's BR_UI_STYLE does, so a
 * wiring site reads like the disassembly: BR_UI_SPR(0x100AB9AC) is entry 46. */
#define BR_UI_SPR(addr) \
    (&g_aBrUiSprite[((unsigned)(addr) - BR_UI_SPR_BASE) / BR_UI_SPR_STRIDE])

/* Entry `i`, or NULL when `i` is the "no sprite" sentinel or out of range.
 * The sentinel test is 0x10040D8D's `test ax,ax / jl`: SIGNED and 16-bit, so
 * the 0xFFFF that BrUiCtlCtor fills aStepId with is negative and draws
 * nothing -- which is why a control whose builder passed a7 = -1 has no
 * background at all. */
const BrUiSprite *BrUiSpriteAt(int32_t i);

/* ==========================================================================
 * The chrome dispatch
 * ========================================================================== */

typedef enum BrUiChromeKind {
    BR_UI_CHROME_NONE   = 0,   /* the dispatch draws nothing               */
    BR_UI_CHROME_TEXT   = 1,   /* flags1C & 0x100000: the text box draws   */
    BR_UI_CHROME_SPRITE = 2    /* a blit from the table                    */
} BrUiChromeKind;

typedef struct BrUiChrome {
    BrUiChromeKind    kind;
    int32_t           iSprite;    /* w1E20C as the frame left it           */
    const BrUiSprite *pSpr;       /* NULL on the +0x18 arm's rect override */
    int32_t           iImage;     /* what 0x10058380 receives as its image */
    const int32_t    *pRect;      /* the four int32 the blit actually reads*/
    int32_t           x, y;       /* __ftol(ctl->x), __ftol(ctl->y)        */
    int32_t           w, h;       /* the blit's size AFTER its own clip    */
    int32_t           fKeyed;     /* the entry's +0x14 bit 0               */
    int32_t           fDown;      /* iSprite == aStepId[1] != 0xFFFF, i.e.
                                   * 0x10048180 swapped in the DOWN art    */
} BrUiChrome;

/* Resolve what the frame draws for this control against a `cx` x `cy`
 * surface: 0x10048530's two flag-only skips, then 0x10048010's three arms,
 * then 0x10047A10's two paths and the blit's clip.  Returns non-zero when
 * `pOut->kind` is a drawing kind.
 *
 * `cx`/`cy` are the destination surface's dimensions, which 0x10001320 reads
 * out of the surface at 0x10AC5D84 and clips against; pass 0 for either to
 * skip that clip and get the sprite's full size. */
int BrUiCtlChrome(const BrUiCtl_ *pCtl, int32_t cx, int32_t cy,
                  BrUiChrome *pOut);

/* 0x10001320's clip, on its own.  Returns 0 when nothing survives.
 *
 * TRANSCRIBED WITH THE ORIGINAL'S UNSIGNED COMPARES (`jb`, not `jl`), which
 * matter: the routine clips the RIGHT and BOTTOM edges only, never the left
 * or the top, and a negative x compares as a huge unsigned so it takes the
 * "off the surface entirely" arm rather than being clamped to zero. */
int BrUiSprClip(int32_t x, int32_t y, const int32_t *pRect,
                int32_t cx, int32_t cy, int32_t *pw, int32_t *ph);

/* The two halves of the pair, as 0x10048180 sees them; -1 for "none".
 * Reading them through these rather than through aStepId[0] / aStepId[1] at a
 * call site keeps the sign-extension in ONE place. */
int32_t BrUiCtlSpriteUp(const BrUiCtl_ *pCtl);
int32_t BrUiCtlSpriteDown(const BrUiCtl_ *pCtl);

#endif /* BR_UISPR_H */
