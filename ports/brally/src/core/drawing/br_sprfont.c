/* br_sprfont.c -- the menu's sprite font.  See br_sprfont.h for the
 * derivation; this file is the transcription.
 *
 * Everything here was read off BRGlide.dll, which is the reference build:
 *
 *   0x1005F800 -> Glide 0x10058540   the four rectangle tables
 *   0x1005B730 -> Glide 0x10054550   kind -> sheet, blit one small glyph
 *   0x1005B7A0 -> Glide 0x100545C0   blit one large glyph
 *   0x1005B2B0 -> Glide 0x100540D0   walk the string
 *   0x10047360 -> Glide 0x100407B0   choose the kind byte
 *
 * All five are `shared` in config/brally/shared.csv, so the two builds agree.
 *
 * ==========================================================================
 * 0x10047360 EXISTS TWICE IN THIS TREE, AND THAT IS DELIBERATE
 * ==========================================================================
 *
 * slice3_31.c already ports it, as `BrSub10047360`, over slice2_25.h's
 * BrGameObj -- a BYTE IMAGE indexed at the ORIGINAL offsets (0x1C, 0x2B64,
 * 0x1E20C, 0x3850) through memcpy helpers.  That transcription cannot be used
 * here: br_ui.h's BrUiCtl_ is a host struct whose members are NOT at the
 * original byte offsets (br_ui.h says so at its head), so handing a BrUiCtl_
 * to a byte-image function writes into the wrong members.
 *
 * This is the same split br_uinav.h already documents for 0x10048180 and
 * friends -- slice3_32.c has the byte-image transcription, br_uinav.c the
 * struct one -- and it is handled the same way: two transcriptions of one
 * original, each over the model its callers use, cross-referenced so neither
 * looks like a duplicate nobody noticed.  The two were derived independently
 * and agree, including the 51-byte index table, which was re-read out of
 * BRGlide.dll at 0x100408C0 for this file and matches slice3_31.c's copy of
 * the D3D table at 0x10047470 byte for byte.
 *
 * ALIAS, stated: `nAA284C` (original 0x10AA284C) is modelled twice as well --
 * by slice3_31's context and by BrUiNav.  Only one of them is ever written in
 * a given host, by whichever transcription of 0x10047A60 is running.  This
 * file reads BrUiNav's, because the control it is handed is a struct control
 * and therefore came through the struct frame.
 */
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#define BrSprFontGlyphA_1005B730 BrSprFontGlyphA_1005B730_port
#define BrSprFontGlyphB_1005B7A0 BrSprFontGlyphB_1005B7A0_port
#include "slice2_25.h"   /* br_globals: its objects */
#include "br_sprfont.h"
#undef BrSprFontGlyphA_1005B730
#undef BrSprFontGlyphB_1005B7A0
#include "br_match.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "br_crt.h"        /* BrFtolTrunc -- 0x1007C8A0 */
#include "br_uispr.h"      /* g_aBrUiSprite -- the entry's +0x14 is fBlit */
#include "br_uinav.h"      /* g_pBrUiNav -- 0x10AA284C and 0x10AA2E80     */

/* ==========================================================================
 * 1. 0x1005F800 -- the four rectangle tables
 *
 * The original is four copies of one loop shape.  Each writes
 * {left, top, right, bottom} at base + i*16 and terminates on a LITERAL
 * ADDRESS compared against the running pointer, which is what pins every
 * count without any guessing:
 *
 *   ecx starts at base+0x14 and is bumped by 0x10 at the top of the body, so
 *   the body that runs with ecx == LIMIT is the LAST one -- the compare is at
 *   the bottom.  base + n*16 then lands exactly on the next referenced
 *   global in every case, which is the second, independent check:
 *
 *     A  0x10AC4208 + 68*16 == 0x10AC4648   (read by 0x10038E10, 0x10039870)
 *     B  0x10AC46C0 + 20*16 == 0x10AC4800   (read by 0x10056260)
 *     C  0x10AC4AD8 + 15*16 == 0x10AC4BC8   (== D's base; they abut)
 *     D  0x10AC4BC8 +  9*16 == 0x10AC4C58   (read by 0x1003AF30, 0x100425E0)
 *
 * The divisions are signed in the original (`cdq`/`idiv`, plus the
 * bias-and-mask idiom for the /8 case).  i is never negative here, so the
 * sign handling cannot be observed; it is written as plain C division rather
 * than reproduced, and this comment is the record of what was dropped.
 * ========================================================================== */

/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x10AC4208  16x16, 8 wide  */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x10AC46C0  39x44, 5 wide  */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x10AC4AD8 128x128,5 wide  */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x10AC4BC8 128x128,3 wide  */

/* (port-only BrSprGrid removed) */


/* The sprite-sheet table entry i, or NULL outside it: the bounds check the
 * original makes at each reader, gathered (the table is g_aBrUiSprite). */
const BrUiSprite *BrUiSpriteAt(int32_t i)
{
    if (i < 0 || i >= BR_UI_SPR_COUNT)
        return NULL;
    return &g_aBrUiSprite[i];
}

/* WHAT IT DOES: works out, once at start-up, where every picture sits inside
 * four different sprite sheets -- the small letters, the large letters, and
 * two sheets of big square pictures -- by laying each sheet out as a fixed
 * grid. Everything that later draws a letter or a picture just asks for cell
 * number N and gets the rectangle from here. */
/* @t4-pass 0x10058540 1 2026-09-07 probes 56 bytes 309 insns 112 regions 4 rows 0 census yes  (tools/brally/crank.py) */
/* @t4-pass 0x10058540 2 2026-09-07 probes 56 bytes 309 insns 112 regions 4 rows 0 census yes  (tools/brally/crank.py) */
/* @implements 0x10058540 glide BrSprFontRectInit_1005F800 */
void BrSprFontRectInit_1005F800(void)
{
    /* Byte-exact 2026-09-12 in INDEX FORM: VC5 strength-reduces each
     * `tab[i][k]` walk into the biased cursor the original has (`add ecx,
     * 0x10` at the top, stores at [ecx-0x14..-8], `cmp ecx, limit` on the
     * three pointer-bound loops) and keeps `inc edi; cmp; jl` AFTER the
     * last two stores.  The pointer-walk spelling this replaced (a
     * `volatile int *p` stepped first, `i++` as the last statement) was
     * size- and multiset-exact but let the scheduler interleave the `inc`
     * and the `cmp` with the last two stores -- and without the volatile
     * the stores reorder by register chain.  Two details carry: `top` is
     * computed BEFORE `left` (the div's result takes esi, the mod's eax),
     * and the fourth loop's bound is `i + 15 < 24` -- the original's test
     * (`lea edx,[edi+0xf]; cmp edx,0x18`), i.e. the D sheet is the tail of
     * the C sheet's 24-record table indexed from 15 with a rebased counter;
     * `i < 9` lands the `inc` before the last two stores.
     * Dead: `while (++i, p < limit)` on the pointer walk; index form with
     * left before top (+14 insns, the `% 8` CSEs differently). */
    {
        int i;
        int left, top;

        for (i = 0; i < BR_SPRFONT_RECT_A; i++) {
            top  = (i / 8) * 16;
            left = (i % 8) * 16;
            g_aBrSprRectA[i][0] = left;
            g_aBrSprRectA[i][1] = top;
            g_aBrSprRectA[i][2] = left + 16;
            g_aBrSprRectA[i][3] = top + 16;
        }
        for (i = 0; i < BR_SPRFONT_RECT_B; i++) {
            top  = (i / 5) * 44;
            left = (i % 5) * 39;
            g_aBrSprRectB[i][0] = left;
            g_aBrSprRectB[i][1] = top;
            g_aBrSprRectB[i][2] = left + 39;
            g_aBrSprRectB[i][3] = top + 44;
        }
        for (i = 0; i < BR_SPRFONT_RECT_C; i++) {
            top  = (i / 5) * 128;
            left = (i % 5) * 128;
            g_aBrSprRectC[i][0] = left;
            g_aBrSprRectC[i][1] = top;
            g_aBrSprRectC[i][2] = left + 128;
            g_aBrSprRectC[i][3] = top + 128;
        }
        for (i = 0; i + 15 < 24; i++) {
            top  = (i / 3) * 128;
            left = (i % 3) * 128;
            g_aBrSprRectD[i][0] = left;
            g_aBrSprRectD[i][1] = top;
            g_aBrSprRectD[i][2] = left + 128;
            g_aBrSprRectD[i][3] = top + 128;
        }
    }
}

/* ==========================================================================
 * 2. 0x1005B730's leading chain -- kind to sheet
 * ========================================================================== */

/* WHAT IT DOES: turns a piece of text's style setting into the lettering sheet
 * it should be drawn from -- grey, white, mid-grey or yellow. Any style it does
 * not recognise falls back to sheet zero, which is not a lettering sheet at all
 * but the general artwork sheet, so an unexpected style draws garbage rather
 * than plain text. */
/* NOT A CLAIM.  This is 0x1005B730..0x1005B75F, the `cmp/jne` ladder that
 * opens it, split out because 0x1005B7A0's caller needs it on its own.
 * 0x1005B730's @implements line lives on BrSprFontGlyphA_1005B730 below,
 * which is the whole 112 bytes: this ladder, the sheet lookup, the two
 * __ftol conversions and the blit at 0x1005B790. */
int32_t BrSprFontSheet_1005B730(uint8_t bKind)
{
    /* `xor edx,edx` before the chain is the whole default arm. */
    if (bKind == 0) return 2;      /* images\type_gry.bmp */
    if (bKind == 1) return 3;      /* images\type_wit.bmp */
    if (bKind == 2) return 4;      /* images\type_mid.bmp */
    if (bKind == 4) return 0x34;   /* images\type_yel.bmp */
    return 0;                      /* GOTCHA -- images\work1a.bmp */
}

/* The sprite table entry's +0x14, which is the only thing either glyph drawer
 * takes out of the table.  0x10054550 reads `[iSheet*24 + 0x100AAD1C]`, and
 * 0x100AAD08 + 0x14 == 0x100AAD1C, so the field is the entry's sixth int32 --
 * br_uispr.h's `fBlit`. */
static int32_t BrSprSheetBlitFlags(int32_t iSheet)
{
    const BrUiSprite *pS = BrUiSpriteAt(iSheet);

    /* DEVIATION (memory safety): the original indexes unchecked.  Every value
     * either drawer can produce -- 0, 2, 3, 4, 5, 0x34 -- is in range, so the
     * guard cannot be taken with shipped data. */
    return (pS != NULL) ? pS->fBlit : 0;
}

/* WHAT IT DOES: draws one character of the small lettering at the given place
 * on screen. It picks the lettering sheet from the text's style, rounds the
 * position down to whole pixels, and hands the sheet, the character's
 * rectangle and the sheet's transparency setting to whatever does the actual
 * drawing. */
/* arg3 declared short: the original's caller pushes the sheet's home
 * register raw (upper bits unspecified), which VC5 only emits for a
 * prototyped short -- an int arg forces a movsx that pins eax and pushes
 * the glyph-rect add into the 6-byte non-eax encoding (113B > 112B slot). */
/* FUN_10058380: prototype in br_funcs.h */

/* thiscall + 4 stack args (`ret 0x10`).  Struct-typed extras so edx stays
 * free for the kind chain (dummy-edx fastcall stole it and `add ecx,imm`
 * cost the extra byte). */
typedef struct { short v; } BrGlyphI16;
typedef struct { float v; } BrGlyphF32;
typedef struct { int v; }   BrGlyphI32;
/* @t4-pass 0x10054550 1 2026-09-07 probes 42 bytes 113 insns 36 regions 1 rows 0 census yes  (tools/brally/crank.py) */
/* @t4-pass 0x10054550 2 2026-09-07 probes 42 bytes 113 insns 36 regions 1 rows 0 census yes  (tools/brally/crank.py) */
/* WHAT IT DOES: draw one character of the FIRST sprite font at the given
 * screen position; the text box's kind picks which sprite sheet the glyph
 * comes from. Always reports success. */
/* @implements 0x10054550 glide BrSprFontGlyphA_1005B730 */
int __fastcall BrSprFontGlyphA_1005B730(BrTextBox *pBox, BrGlyphI16 iGlyph,
    BrGlyphF32 x, BrGlyphF32 y, BrGlyphI32 unused)
{short sheet;unsigned char k;sheet = 0;k = pBox->f08;if (k == 0) {
        sheet = 2;
    } else if (k == 1) {
        sheet = 3;
    } else if (k == 2) {
        sheet = 4;
    } else if (k == 4) {
        sheet = 0x34;
    }BrSprFontDraw((int)x.v, (int)y.v, sheet, g_aBrSprRectA[iGlyph.v],
                 g_aBrUiSprite[sheet].fBlit);return 1;}

/* WHAT IT DOES: draw one character of the SECOND sprite font at the given
 * screen position, looking its rectangle up in that font's table. The A/B
 * pair are two different typefaces, not two sizes of one. Always reports
 * success. */
/* @implements 0x100545C0 glide BrSprFontGlyphB_1005B7A0 */
int BR_STDCALL BrSprFontGlyphB_1005B7A0(short iGlyph, float x, float y,
                                       int unused)
{
    BrSprFontDraw((int)x, (int)y, 5, g_aBrSprRectB[iGlyph],
                 g_aBrUiSprite[5].fBlit);
    return 1;
}

/* WHAT IT DOES: the D3D build's draw of one FIRST-font character: picks the
 * sprite sheet from the text box's kind and hands the glyph's rectangle to
 * the caller's blit function. */
/* @implements 0x1005B730 d3d BrSprFontGlyphA_1005B730 */
void BrSprFontGlyphA_1005B730_port(const BrTextBox *pBox, int32_t iGlyph,
                              float x, float y, int32_t bKindUnused,
                              BrSprFontBlitFn pfnBlit, void *pCtx)
{
    int32_t iSheet, ix, iy;

    /* The fourth argument really is dead -- see the header. */
    (void)bKindUnused;

    if (pBox == NULL || pfnBlit == NULL) {
        return;                     /* DEVIATION: the original faults. */
    }
    /* DEVIATION (memory safety): 0x10054550 does `movsx eax,[esp+4] / shl
     * eax,4 / add eax,0x10AC4208` with no bound at either end.  The metric
     * table's largest `sprite` is 67 and the table has 68 entries, so no
     * shipped character can take this guard. */
    if (iGlyph < 0 || iGlyph >= BR_SPRFONT_RECT_A) {
        return;
    }

    iSheet = BrSprFontSheet_1005B730(pBox->f08);

    /* The original converts y FIRST (the `fld [esp+0xC]` is the function's
     * second instruction and the first __ftol consumes it), then x. */
    iy = BrFtolTrunc(y);
    ix = BrFtolTrunc(x);

    pfnBlit(pCtx, ix, iy, iSheet, g_aBrSprRectA[iGlyph],
            BrSprSheetBlitFlags(iSheet));
}

/* WHAT IT DOES: draws one character of the big lettering at the given place on
 * screen. Unlike the small lettering there is no choice of colour here -- the
 * large characters always come from one fixed sheet. The style argument it is
 * handed is ignored. */
/* @implements 0x1005B7A0 d3d BrSprFontGlyphB_1005B7A0 */
void BrSprFontGlyphB_1005B7A0_port(int32_t iGlyph, float x, float y,
                              int32_t bKindUnused,
                              BrSprFontBlitFn pfnBlit, void *pCtx)
{
    int32_t ix, iy;

    (void)bKindUnused;

    if (pfnBlit == NULL) {
        return;                     /* DEVIATION: the original faults. */
    }
    if (iGlyph < 0 || iGlyph >= BR_SPRFONT_RECT_B) {
        return;                     /* DEVIATION, as above. */
    }

    iy = BrFtolTrunc(y);
    ix = BrFtolTrunc(x);

    pfnBlit(pCtx, ix, iy, BR_SPRFONT_SHEET_B, g_aBrSprRectB[iGlyph],
            BrSprSheetBlitFlags(BR_SPRFONT_SHEET_B));
}

/* ==========================================================================
 * 3. 0x1005B2B0 -- the string walk
 * ========================================================================== */

/* The same three-way classification the two MEASURERS open with
 * (slice3_39.c's BrGlyphClassify, from 0x1005B0D0 / 0x1005B160).  It is
 * transcribed again rather than shared because it is `static` there and
 * because the draw and the measure are separate functions in the original
 * that happen to agree; making one call the other would assert an identity
 * the disassembly does not.
 *
 *   -1  stop the walk
 *    0  not a drawable character; only 0x20 does anything
 *    1  look the character up in the metric table
 */
/* (port-only BrSprGlyphClassify removed) */


/* WHAT IT DOES: decides where the first letter of a line of text goes. Normally
 * that is just the text box's own left edge, but a box marked as centred is
 * asked to work out its centred starting point instead. */
/* THE TAG IS ON THE WRONG BODY -- read this before touching 0x1005B2B0.
 *
 * 0x1005B2B0 (glide 0x100540D0) is 212 bytes, and config/brally/shared.csv pairs the
 * two by BODY, not by slot, so both really are that size.  What lives there is
 * the whole DRAW routine: the pen-start below is its first thirty bytes,
 * inlined, and the rest is the character loop.  The twelve-line function this
 * tag sits on is a fragment of it, which is why the row reads 64 bytes against
 * 212.  BrSprFontDraw_1005B2B0 further down is the body that belongs here, and
 * it is untagged.
 *
 * What the bytes say about the real body, so the next pass does not re-derive
 * it:
 *   - Thiscall, `this` in ecx, NO stack arguments, returns void.  The blit
 *     function and context BrSprFontDraw_1005B2B0 takes are port inventions;
 *     the original dispatches through the box's own vtable.
 *   - Two frame dwords (`sub esp, 8`): the running pen x at [esp+0xc] after
 *     the three pushes, and an int temp at [esp+0x10] that only exists to
 *     carry the advance into `fild`.
 *   - The glyph draw is `call dword ptr [edx+0x18]` with FOUR stack arguments
 *     (glyph, x, y, (int8)f08) and this in ecx, and the +0x420 tail is
 *     `call dword ptr [edx+0x24]` with two (x, y).  slice3_39.h types both
 *     slots `void (*)(BrTextBox *)` because their arity was unknown; a LOCAL
 *     __fastcall view of the vtable with struct-typed stack arguments is how
 *     to reach them without touching that shared header (see the thiscall
 *     entry in docs/brally/VC5-IDIOMS.md -- this is now known to work through a
 *     function pointer).
 *   - BrSprGlyphClassify is INLINED (VC5 inlines no static helper), and the
 *     index arithmetic is 16-bit: `movsx cx, al` then `sub ecx, 0x20`.
 *   - THE METRIC TABLE IS NOT g_BrGlyphFontA.  It is at 0x100ABE84 with a
 *     TWELVE-byte stride (`lea esi,[eax+eax*2]; shl esi,2`), advance at +0
 *     and sprite at +4 -- while slice3_39.h's BrGlyphMetric is eight bytes at
 *     0x100AC6E4.  A third table, or a wider one; it needs its own type
 *     before this body can be written. */
/* declared only (the Mac port keeps its own body in ports/brally-wasm/patch/); Glide match is src/brally/core/cpp/0x100540D0.cpp */
float BrSprFontPenStart_1005B2B0(BrTextBox *pBox);

/* BrSprFontDraw_1005B2B0: the placed body is BrSprFontDraw_1005B2B0_100540D0.cpp */

/* ==========================================================================
 * 4. 0x10047360 -- the hook that picks the kind byte
 * ========================================================================== */

/* The index table at Glide 0x100408C0 (D3D 0x10047470), 0x33 bytes, read out
 * of the image.  It maps (+0x1E20C - 2) onto one of the five arms of the jump
 * table at Glide 0x100408AC.  Arm 4 is the same code the range check's
 * default reaches. */
static const unsigned char g_aBrSprKindArm[0x33] = {
    0, 1, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3
};

/* (port-only BrSprFontKindHook_10047360 removed) */


/* BrSprFontDraw (0x10058380) lives in ghidra_batch.c: the original binary
 * calls BrUiSprClip with 6 args, but the port header declares 7. */

typedef int (__fastcall *VT1)(void *this);

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* FUN_10054360: prototype in br_funcs.h */
/* FUN_10037720: prototype in br_funcs.h */
/* FUN_10037040: prototype in br_funcs.h */
/* FUN_1006ba60: prototype in br_funcs.h */

/* WHAT IT DOES: the text box's key handler -- vtable slot +0x14 of the
 * BrTextBox in slice3_39.h (D3D 0x1005B570), with the box as `this`.  In a
 * network session it first sends whichever of the four pending lobby
 * messages (4..7) is flagged and records it.  Then it consumes the character
 * 0x10AA33E4 holds from WM_CHAR: the flag at 0x10AC5DE4 returns -1; a
 * non-empty box returns 0 while 0x10AC5E50 or 0x10AC6050 is set, or
 * 0x10037720 reports true outside a session; code 8 (backspace) drops the last
 * character; any other code goes through BrCharMapLookup and, if the box
 * (re-measured through vtable +0x04) is still under its width limit at
 * +0x41C, is appended to the text at +0x09.  An unmapped code returns 1 and
 * is left pending; every other path clears it. */
/* @implements 0x10054390 glide FUN_10054390 */
char __fastcall FUN_10054390(BrTextBox *param_1)

{
  char cVar1;
  
  if (g_5BB4 != 0) {
    if ((*(int *)&g_BrDikEdge[59]) != 0) {
      BrSub1003D9A0(g_brPA9D008, 4);
      BrSub10072AF0(4, 0x200020);
      g_track = 4;
    }
    else if ((*(int *)&g_BrDikEdge[60]) != 0) {
      BrSub1003D9A0(g_brPA9D008, 5);
      BrSub10072AF0(5, 0x200020);
      g_track = 5;
    }
    else if ((*(int *)&g_BrDikEdge[61]) != 0) {
      BrSub1003D9A0(g_brPA9D008, 6);
      BrSub10072AF0(6, 0x200020);
      g_track = 6;
    }
    else if ((*(int *)&g_BrDikEdge[62]) != 0) {
      BrSub1003D9A0(g_brPA9D008, 7);
      BrSub10072AF0(7, 0x200020);
      g_track = 7;
    }
  }
  if ((*(int *)&g_BrDikEdge[1]) != 0) {
    DAT_10ac6744 = 0;
    return (char)0xff;
  }
  if (((*(int *)&g_BrDikEdge[28]) != 0) || ((*(int *)&g_BrDikEdge[156]) != 0) ||
      (BrInputAnyActive() != 0 && g_5BB4 == 0)) {
    if (strlen(param_1->sz) != 0) {
      DAT_10ac6744 = 0;
      return 0;
    }
  }
  if (DAT_10ac6744 != 0) {
    if (DAT_10ac6744 == 8) {
      if (strlen(param_1->sz) != 0) {
        param_1->sz[strlen(param_1->sz) - 1] = 0;   /* box +8 + strlen(box +9) */
        DAT_10ac6744 = 0;
        return 1;
      }
    }
    else {
      cVar1 = BrCharMapLookup(DAT_10ac6744);
      if (cVar1 == 0) {
        return 1;
      }
      ((void (*)(BrTextBox *))((void *const *)param_1->pVtbl)[1])(param_1);   /* vtbl +4 */
      if (param_1->width < param_1->f41C) {
        sprintf(param_1->sz, DAT_100acb44, param_1->sz, (int)cVar1);
      }
    }
  }
  DAT_10ac6744 = 0;
  return 1;
}
