/* br_sprfont.h -- the MENU font, which is a sprite sheet and not br_font.c's
 * display-list font.
 *
 * ==========================================================================
 * WHAT THE MENU FONT IS
 * ==========================================================================
 *
 * br_font.c recovers the DISPLAY-LIST font out of .data and feeds it to the
 * software RSP.  The menus never touch it.  A menu caption is drawn one
 * SPRITE PER CHARACTER, blitted out of a 128x144 BMP on the disc, and which
 * of four such BMPs is a property of the text box's own "kind" byte:
 *
 *      kind 0 -> sprite 2    images\type_gry.bmp    (112,136,150)
 *      kind 1 -> sprite 3    images\type_wit.bmp    (255,255,255)
 *      kind 2 -> sprite 4    images\type_mid.bmp    (191,201,208)
 *      kind 4 -> sprite 0x34 images\type_yel.bmp    (208,209, 84)
 *
 * So a caption's COLOUR is a choice of sheet, not a tint, and a menu row is
 * recoloured by writing one byte.  The four RGB triples above were measured
 * out of the shipped files; they are the second-commonest colour in each,
 * behind the colour key.
 *
 * The pieces, and the original behind each:
 *
 *   0x1005F800 / Glide 0x10058540   builds four rectangle tables at run time.
 *                                   Two of them are this font's glyph cells.
 *   0x1005B730 / Glide 0x10054550   kind -> sheet, then blit one glyph.
 *   0x1005B7A0 / Glide 0x100545C0   the same for bignums.bmp (sprite 5).
 *   0x1005B2B0 / Glide 0x100540D0   walk the string, one glyph at a time.
 *   0x10047360 / Glide 0x100407B0   the +0x0C hook that PICKS the kind byte.
 *
 * The last one is in this module rather than with the rest of the control
 * frame because what it does is choose a font sheet: it is the other half of
 * the recolour mechanism, and separating it from the sheet table it indexes
 * would leave neither half readable.
 *
 * ==========================================================================
 * WHERE A CAPTION GOES
 * ==========================================================================
 *
 * 0x1005B2B0 hands EVERY glyph the box's +0x414 unchanged as the destination
 * top-left.  There is no baseline, no ascent and no per-glyph bearing: the
 * cell top IS the control's y.  The pen starts at the box's +0x410, which
 * BrTextBoxCentreX has already centred in the style rectangle -- and the walk
 * re-centres it when the box's f04 bit 0 is set, which every menu builder
 * arranges by passing a2 = 1 to 0x10047EB0.
 *
 * ==========================================================================
 * A SIGNATURE CONFLICT, REPORTED RATHER THAN PAPERED OVER
 * ==========================================================================
 *
 * slice3_39.h types the text box's vtable slot +0x24 as
 * `void (*)(struct BrTextBox *)`.  0x100540D0 pushes TWO stack arguments
 * before calling it (`push eax` = the box's +0x414, `push ecx` = the running
 * pen x), so the slot is `void (*)(BrTextBox *, float x, float y)`.  Nothing
 * in the tree fills the slot -- 0x1005B250 is unported -- so this module
 * reproduces the PREDICATE that guards the call and does not make the call.
 * Calling through the header's type would be a two-argument mismatch; calling
 * through a cast would assert a signature the header denies.  See
 * BrSprFontDraw_1005B2B0.
 */
#ifndef BR_SPRFONT_H
#define BR_SPRFONT_H

#include <stdint.h>

#include "br_ui.h"        /* BrUiCtl_ */
#include "slice3_39.h"    /* BrTextBox, BrGlyphMetric, g_BrGlyphFontA */

/* ==========================================================================
 * 1. The rectangle tables -- 0x1005F800 (Glide 0x10058540)
 *
 * Four loops, four tables, all of them {left, top, right, bottom} int32
 * quadruples on a 16-byte stride.  Every extent is pinned by the loop's own
 * terminating address, which is a literal in the instruction stream:
 *
 *   A  0x10AC4208  68 entries  16 x 16   8 per row   the small font's glyphs
 *   B  0x10AC46C0  20 entries  39 x 44   5 per row   bignums.bmp's digits
 *   C  0x10AC4AD8  15 entries  128 x 128 5 per row   not a font
 *   D  0x10AC4BC8   9 entries  128 x 128 3 per row   not a font
 *
 * A's count is corroborated from the other end: the largest `sprite` in the
 * font-A metric table (slice3_39.h's g_BrGlyphFontA) is 67, and 67 is the
 * last index this table has.  B's is likewise: bignums.bmp is 204x93 and
 * 5 * 39 == 195 <= 204, 2 * 44 == 88 <= 93 -- ten digits in two rows, with
 * entries 10..19 addressing rows that do not exist in the file.
 *
 * C and D are here because they are the same function, not because this
 * module uses them.  Nothing else in the port models either address (grepped
 * before adding, per CONVENTIONS' aliased-storage rule), so this file owns
 * the storage; a later pass that needs them must alias, not redeclare.
 * ========================================================================== */

#define BR_SPRFONT_RECT_A  68
#define BR_SPRFONT_RECT_B  20
#define BR_SPRFONT_RECT_C  15
#define BR_SPRFONT_RECT_D   9

extern int32_t g_aBrSprRectA[BR_SPRFONT_RECT_A][4];   /* 0x10AC4208 */
extern int32_t g_aBrSprRectB[BR_SPRFONT_RECT_B][4];   /* 0x10AC46C0 */
extern int32_t g_aBrSprRectC[BR_SPRFONT_RECT_C][4];   /* 0x10AC4AD8 */
extern int32_t g_aBrSprRectD[BR_SPRFONT_RECT_D][4];   /* 0x10AC4BC8 */

/* 0x1005F800 -- fill all four.  The original runs it once during start-up;
 * it is idempotent, so a caller may run it again. */
void BrSprFontRectInit_1005F800(void);

/* ==========================================================================
 * 2. kind -> sheet
 * ========================================================================== */

/* 0x1005B730's leading chain, split out because 0x1005B7A0's caller needs the
 * same answer and because the fall-through is worth naming.
 *
 * GOTCHA, reproduced: the chain tests 0, 1, 2 and 4 and has NO default arm --
 * edx is zeroed before it, so kind 3 (and 5, 6, ...) selects SPRITE 0, which
 * is images\work1a.bmp, the 640x480 backdrop.  A kind-3 label would blit
 * 16x16 chunks of the backdrop where its glyphs should be.  Kind 3 is the
 * value 0x10047EB0 routes to the LARGE-digit measurer, so the pairing is not
 * accidental -- but this function is still the small-glyph path, and it still
 * answers 0. */
int32_t BrSprFontSheet_1005B730(uint8_t bKind);

/* 0x1005B7A0 pushes a literal 5 and indexes the sprite table's entry 5. */
#define BR_SPRFONT_SHEET_B  5

/* ==========================================================================
 * 3. The blit seam
 *
 * The original ends every glyph at 0x10058380, which looks the image up in
 * the stride-8 table at 0x10AC53E8 and hands 0x10001320 a 16-bit software
 * blit against the 640x480 surface at 0x10AC5D84.  Neither the table nor the
 * surface exists in this port, so the glyph is handed to a CALLBACK with
 * exactly the arguments 0x10058380 receives, in its order:
 *
 *     0x10058380(x, y, iSprite, pRect, fBlit)
 *
 * x and y have already been through __ftol (truncation toward zero), which is
 * what the two glyph drawers do to them; iSprite is the SHEET, not the glyph;
 * pRect is the glyph's cell inside that sheet; fBlit is the sprite table
 * entry's +0x14, whose bit 0 selects the colour-keyed copy.
 * ========================================================================== */

typedef void (*BrSprFontBlitFn)(void *pCtx, int32_t x, int32_t y,
                                int32_t iSprite, const int32_t *pRect,
                                int32_t fBlit);

/* 0x1005B730 -- one glyph of the small font.  `iGlyph` is a metric table
 * `sprite` field; the sheet comes from the BOX, not from the caller.
 *
 * GOTCHA, reproduced: the original takes FOUR stack arguments and reads the
 * kind out of `pBox->f08` anyway -- the caller pushes the same byte as the
 * fourth argument and the callee ignores it.  The parameter is kept so the
 * arity matches `ret 0x10`; it is unused here for the same reason it is
 * unused there. */
void BrSprFontGlyphA_1005B730(const BrTextBox *pBox, int32_t iGlyph,
                              float x, float y, int32_t bKindUnused,
                              BrSprFontBlitFn pfnBlit, void *pCtx);

/* 0x1005B7A0 -- one glyph of the large font.  Same shape, fixed sheet 5. */
void BrSprFontGlyphB_1005B7A0(int32_t iGlyph, float x, float y,
                              int32_t bKindUnused,
                              BrSprFontBlitFn pfnBlit, void *pCtx);

/* ==========================================================================
 * 4. The string walk -- 0x1005B2B0
 * ========================================================================== */

/* The pen's starting x, exactly as the walk resolves it: the box's +0x410, or
 * the float the box's vtable +0x28 returns when f04 bit 0 is set.
 *
 * Split out of the walk -- the split is this port's, the dispatch is the
 * original's -- because a caller rasterising into a buffer has to know where
 * the string BEGINS in order to place that buffer, and running the walk twice
 * to find out would call the centring method twice. */
float BrSprFontPenStart_1005B2B0(BrTextBox *pBox);

/* 0x1005B2B0 -- walk sz[] and blit one glyph per character.  Returns the pen
 * x the original leaves on the x87 stack for its +0x24 tail.
 *
 * The classification is the measurers' (slice3_39.c's BrGlyphClassify), and
 * the two disagree about one thing which is preserved here: the MEASURERS
 * gate on the metric's `advance` and `height` being != 0xFFFF, the DRAW gates
 * on its `sprite`.  A glyph with a sprite and no advance would be drawn and
 * not measured, and vice versa.  No shipped entry is in either state.
 *
 * A space with no glyph advances by 6.  The original spells that as
 * `fsub [0x10077674]` with the constant read out of the image as -6.0f, i.e.
 * it SUBTRACTS a negative -- the same 6 slice3_39.h calls
 * BR_GLYPH_SPACE_ADVANCE, arrived at from the other direction. */
float BrSprFontDraw_1005B2B0(BrTextBox *pBox,
                             BrSprFontBlitFn pfnBlit, void *pCtx);

/* ==========================================================================
 * 5. The hook that picks the kind -- 0x10047360 (Glide 0x100407B0)
 *
 * A control's +0x0C hook, cdecl, one argument, and the thing that makes a
 * SELECTED row look different from an unselected one.  The mechanism is worth
 * stating in full because "the game draws a highlight" is the wrong model:
 *
 *   - 0x10048180's NOT-CURRENT tail forces a label's kind byte to 1
 *     (type_wit) and its +0x1E20C to 3, every frame, for every label that
 *     carries a +0x0C hook.  So an unselected row is pinned WHITE.
 *   - The CURRENT control instead has its +0x0C hook called, which is this
 *     function.  When the control's +0x1C carries bit 0x100 it advances
 *     +0x1E20C by one and maps the result through a table:
 *
 *         +0x1E20C == 2      -> kind 0   type_gry
 *         +0x1E20C == 3      -> kind 1   type_wit
 *         +0x1E20C == 4      -> kind 2   type_mid
 *         +0x1E20C == 0x34   -> kind 4   type_yel
 *         5 .. 0x33          -> +0x1E20C = 2, kind unchanged
 *
 *     and clears 0x100 again.  So the selected row PULSES between white and
 *     type_mid, and the unselected rows do not.
 *   - Bit 0x100 is raised by the step timer 0x100480A0 (control vtable
 *     +0x04), which 0x10048180 calls immediately before all of this, once
 *     every 60 ms.  That is the clock the pulse runs on.
 *   - The other arm, ahead of all of it, is the MOUSE one: when the cursor is
 *     inside one of the three hot rectangles (0x10AA284C, which 0x10047A60
 *     has just written) and any of the input object's +0x2C..+0x38 is set,
 *     the kind goes straight to 4 -- yellow under the pointer.
 *
 * Returns 1 unless one of the three refusals fires, in which case 0.  0 from
 * a +0x0C hook is discarded by 0x10048180, which does not test it.
 * ========================================================================== */

int32_t BrSprFontKindHook_10047360(BrUiCtl_ *pCtl);

#endif /* BR_SPRFONT_H */
