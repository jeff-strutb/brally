/* br_textbox.c -- menus: the text box, one line of on-screen text with its own
 * position and size. Constructor, deleting destructor, both measurers (font A
 * and font B), and the two tiny stubs that sit with them.
 *
 * Filed out of slice3_39.c, whose preamble it keeps verbatim below so the
 * compiler's view of these bodies is unchanged.  BrTextBoxMeasureB is the
 * font-B twin of BrTextBoxMeasureA and is filed to menus/ with it, so the
 * BrGlyphMetric12 view and the BrGlyphClassify file-static they share are
 * defined here once and used by both.
 *
 * The original banner follows.
 *
 * slice3_39.c -- Boss Rally (BRD3D.dll) slice 3, a later pass.
 *
 * Packet 0x1005AE70 - 0x100607B0.  See slice3_39.h for the layout notes and
 * the list of functions that were deliberately left out.
 *
 * Every arithmetic width here is deliberate: the original accumulates string
 * widths in 16 bits and stores 16 bits, and the range tests on a character
 * are done on the SIGN-EXTENDED byte.  Both are reproduced literally.
 */

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "br_vtables.h"
#include "br_coretypes.h"   /* br_globals: its objects */
#include <string.h>

#include "slice1_07.h"   /* BrDevSlot -- see the note in slice3_39.h */
/* Header prototype is cdecl; matching needs thiscall.  Rename the cdecl
 * declaration so the definition below can wear a different convention. */
#define BrTextBoxDeleteDtor BrTextBoxDeleteDtor_cdecl
#define BrTextBoxMeasureA  BrTextBoxMeasureA_cdecl
#define BrTextBoxMeasureB  BrTextBoxMeasureB_cdecl
#define BrTextBoxInit BrTextBoxInit_port
#include "slice3_39.h"
#undef BrTextBoxInit
#undef BrTextBoxDeleteDtor
#undef BrTextBoxMeasureA
#undef BrTextBoxMeasureB

/* =====================================================================
 * 0x1005B050 -- BrTextBox constructor (thiscall)
 * ===================================================================== */

/* WHAT IT DOES: sets up a fresh text box -- one line of on-screen text with
 * its own position and size. It clears the string and the measurements but
 * deliberately leaves the box's left and right edges as it found them, and
 * since the allocator does not zero either, a brand-new box has junk in
 * those fields until something fills them in. */
/* @implements 0x1005B050 d3d BrTextBoxInit */
/* thiscall ctor: vtbl immediate, memset of the 0x400 buffer at +9, field
 * zeroes through the memset's zero register, returns this. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

BrTextBox *__fastcall BrTextBoxInit(BrTextBox *pBox)
{
    char *p = (char *)pBox;

    *(void **)p = (void *)&PTR_FUN_100776f0;
    memset(p + 9, 0, 0x400);
    *(int *)(p + 0x418) = 0;
    *(int *)(p + 0x414) = 0;
    *(int *)(p + 0x410) = 0;
    *(short *)(p + 0x40C) = 0;
    *(short *)(p + 0x40A) = 0;
    *(short *)(p + 0x41C) = 0;
    *(int *)(p + 0x420) = 0;
    *(int *)(p + 4) = 0;
    *(unsigned char *)(p + 8) = 1;
    return pBox;
}

/* =====================================================================
 * 0x1005B0A0 -- scalar deleting destructor
 * ===================================================================== */

/* WHAT IT DOES: destroys a text box and, if asked, frees it too. It hands
 * the pointer back even when it has just freed it, which is what the
 * compiler's standard destructor does and is kept. */
/* @implements 0x1005B0A0 d3d BrTextBoxDeleteDtor */
/* Original is 2-arg thiscall: `this` in ecx, flags on the stack, `ret 4`.
 * BR_THISCALL1 (= __fastcall) would put flags in edx; a struct is never
 * register-eligible, so it is forced back onto the stack. */
typedef struct { uint32_t v; } BrTextBoxDeleteFlags;
BrTextBox *BR_THISCALL1 BrTextBoxDeleteDtor(BrTextBox *pBox, BrTextBoxDeleteFlags flags)
{
    BrVtInit53EE0(pBox);
    if (flags.v & 1u) {
        BrOperatorDelete(pBox);
    }
    return pBox;
}

/* =====================================================================
 * 0x1005B0D0 / 0x1005B160 -- measure sz[]
 * ===================================================================== */

/* GLIDE's font table records are 12 bytes (`lea eax,[eax+eax*2]; shl eax,2`
 * -- index * 12), not the 8-byte BrGlyphMetric the port carries.  Only the
 * first two words are read here.  Table base 0x100ABE84 in BRGlide.dll. */
/* BrGlyphMetric12: br_coretypes.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x100ABE84 (glide) */

/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x100AC2FC (glide) */

/* WHAT IT DOES: walks sz[] adding up glyph advances from font A, growing the
 * seeded height to the tallest glyph seen; a control byte stops the walk.
 * The original is ONE loop -- the port's BrGlyphClassify split below is not
 * a matching twin, so the matching build carries the inlined shape. */
/* @implements 0x10053EF0 glide BrTextBoxMeasureA */
void BR_THISCALL1 BrTextBoxMeasureA(BrTextBox *pBox)
{
    char    c;
    int16_t h;
    int16_t k;
    int16_t adv;
    int16_t width;
    int16_t maxH;
    int16_t i;

    c     = pBox->sz[0];
    maxH  = pBox->height;
    width = 0;
    i     = 0;
    for (;;) {
        if (c == '\0' ||
            (((k = (int16_t)((int16_t)c - 0x20)), k < 0 || k > 0x7F) &&
             c != ' ')) {
            pBox->height = maxH;
            pBox->width  = width;
            return;
        }
        if (c < '!' || c > '~') {
LAB_spaceA:
            if (c == ' ') {
                width = width + BR_GLYPH_SPACE_ADVANCE;
            }
        } else {
            adv = (int16_t)(*(BrGlyphMetric12 (*)[])&g_BrGlyphFontA12)[k].advance;
            if (adv == -1 ||
                ((h = (int16_t)(*(BrGlyphMetric12 (*)[])&g_BrGlyphFontA12)[k].height),
                 (uint16_t)h == BR_GLYPH_NONE)) goto LAB_spaceA;
            width = width + adv;
            if (maxH < h) {
                maxH = h;
            }
        }
        i = i + 1;
        c = pBox->sz[i];
    }
}

/* WHAT IT DOES: like A but font B supplies the metrics (advance - 4) while
 * font A's sentinels still gate the glyph path -- the same cross-font gate
 * the port documents.  Both sentinel tests compare memory directly against
 * -1, which VC5 registerises (`or ebp,-1; cmp [mem],bp`). */
/* @implements 0x10053F80 glide BrTextBoxMeasureB */
void BR_THISCALL1 BrTextBoxMeasureB(BrTextBox *pBox)
{
    char    c;
    int16_t k;
    int16_t width;
    int16_t maxH;
    int16_t i;

    width = 0;
    i     = 0;
    maxH  = pBox->height;
    c     = pBox->sz[0];
    for (;;) {
        if (c == '\0' ||
            (((k = (int16_t)((int16_t)c - 0x20)), k < 0 || k > 0x7F) &&
             c != ' ')) {
            pBox->height = maxH;
            pBox->width  = width;
            return;
        }
        if (c < '!' || c > '~' ||
            (int16_t)(*(BrGlyphMetric12 (*)[])&g_BrGlyphFontA12)[k].advance == -1 ||
            (int16_t)(*(BrGlyphMetric12 (*)[])&g_BrGlyphFontA12)[k].height == -1) {
            if (c == ' ') {
                width = width + BR_GLYPH_SPACE_ADVANCE;
            }
        } else {
            width = width + (int16_t)((*(BrGlyphMetric12 (*)[])&g_BrGlyphFontB12)[k].advance - 4);
            if (maxH < (int16_t)(*(BrGlyphMetric12 (*)[])&g_BrGlyphFontB12)[k].height) {
                maxH = (int16_t)(*(BrGlyphMetric12 (*)[])&g_BrGlyphFontB12)[k].height;
            }
        }
        i = i + 1;
        c = pBox->sz[i];
    }
}

/* -- Ghidra-matched functions --------------------------- */
typedef int (*funcptr)();
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: stub that always returns 0. */
/* @implements 0x10053E60 glide BrStubFalse */

int BrStubFalse(void)

{
  return 0;
}

/* WHAT IT DOES: vtable constructor: install the function-pointer table at PTR_FUN_100776F0 (fastcall). */
/* @implements 0x10053EE0 glide BrVtInit53EE0 */

int __fastcall BrVtInit53EE0(const void **param_1)

{
  /* the text box's destructor: its vtable back to its own class's */
  *param_1 = g_brVtbl_100776F0;
  return 0;
}


/* =====================================================================
 * 0x1005B200 -- centre horizontally
 * ===================================================================== */

/* WHAT IT DOES: centres a line of text horizontally between the box's two
 * edges, storing the resulting left position and also handing it back. */
/* @implements 0x1005B200 d3d BrTextBoxCentreX */
float BR_THISCALL1 BrTextBoxCentreX(BrTextBox *pBox)
{
    float fLeft  = (float)pBox->left;
    float fWidth = (float)(int32_t)pBox->width;          /* movsx from +0x40A */
    float fSpan  = (float)(pBox->right - pBox->left);    /* the sub is 32-bit */
    float v;

    /* 0x1008F678 == 0.5f */
    /* THREE STEPS, EACH ITS OWN STATEMENT, and that is the whole match: the
     * original keeps the span-minus-width difference as a value of its own,
     * scales it, and only then adds `left` (`fsubp`, two dead `fxch`, `fmul
     * 0.5`, `fxch`, `faddp`).  Folded into one expression VC5 schedules the
     * x87 stack differently and drops those two shuffles -- 76 bytes against
     * the original's 80.  The dead fxch pair is not noise to be optimised
     * away; it is what the original's own source shape produced. */
    float t = fSpan - fWidth;

    v = t * 0.5f;
    v = v + fLeft;

    pBox->x = v;
    return v;
}
