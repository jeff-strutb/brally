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

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <string.h>

#include "slice1_07.h"   /* BrDevSlot -- see the note in slice3_39.h */
#ifdef BR_MATCHING_BUILD
/* Header prototype is cdecl; matching needs thiscall.  Rename the cdecl
 * declaration so the definition below can wear a different convention. */
#define BrTextBoxDeleteDtor BrTextBoxDeleteDtor_cdecl
#define BrTextBoxMeasureA  BrTextBoxMeasureA_cdecl
#define BrTextBoxMeasureB  BrTextBoxMeasureB_cdecl
#endif
#ifdef BR_MATCHING_BUILD
#define BrTextBoxInit BrTextBoxInit_port
#include "slice3_39.h"
#undef BrTextBoxInit
#else
#include "slice3_39.h"
#endif
#ifdef BR_MATCHING_BUILD
#undef BrTextBoxDeleteDtor
#undef BrTextBoxMeasureA
#undef BrTextBoxMeasureB
#endif

/* =====================================================================
 * 0x1005B050 -- BrTextBox constructor (thiscall)
 * ===================================================================== */

/* WHAT IT DOES: sets up a fresh text box -- one line of on-screen text with
 * its own position and size. It clears the string and the measurements but
 * deliberately leaves the box's left and right edges as it found them, and
 * since the allocator does not zero either, a brand-new box has junk in
 * those fields until something fills them in. */
/* @implements 0x1005B050 d3d BrTextBoxInit */
#ifdef BR_MATCHING_BUILD
/* thiscall ctor: vtbl immediate, memset of the 0x400 buffer at +9, field
 * zeroes through the memset's zero register, returns this. */
extern int DAT_100776f0;

BrTextBox *__fastcall BrTextBoxInit(BrTextBox *pBox)
{
    char *p = (char *)pBox;

    *(void **)p = (void *)&DAT_100776f0;
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
#else
/* WHAT IT DOES: sets up a fresh text box -- one line of on-screen text with
 * its own position and size. It clears the string and the measurements but
 * deliberately leaves the box's left and right edges as it found them, and
 * since the allocator does not zero either, a brand-new box has junk in
 * those fields until something fills them in. */
/* @implements 0x1005B050 d3d BrTextBoxInit */
BrTextBox *BrTextBoxInit(BrTextBox *pBox)
{
    /* The vtable store comes FIRST, before the buffer clear -- the clear
     * starts at this+9 and so never touches it either way. */
    pBox->pVtbl = g_pBrTextBoxVtbl;

    memset(pBox->sz, 0, sizeof pBox->sz);

    pBox->f418  = 0;
    pBox->y     = 0.0f;
    pBox->x     = 0.0f;
    pBox->height = 0;
    pBox->width  = 0;
    pBox->f41C  = 0;
    pBox->f420  = 0;
    pBox->f04   = 0;
    pBox->f08   = 1;

    /* left / f428 / right / f430 / f434 are deliberately NOT touched. */
    return pBox;
}
#endif

/* =====================================================================
 * 0x1005B0A0 -- scalar deleting destructor
 * ===================================================================== */

/* WHAT IT DOES: destroys a text box and, if asked, frees it too. It hands
 * the pointer back even when it has just freed it, which is what the
 * compiler's standard destructor does and is kept. */
/* @implements 0x1005B0A0 d3d BrTextBoxDeleteDtor */
#ifdef BR_MATCHING_BUILD
/* Original is 2-arg thiscall: `this` in ecx, flags on the stack, `ret 4`.
 * BR_THISCALL1 (= __fastcall) would put flags in edx; a struct is never
 * register-eligible, so it is forced back onto the stack. */
typedef struct { uint32_t v; } BrTextBoxDeleteFlags;
BrTextBox *BR_THISCALL1 BrTextBoxDeleteDtor(BrTextBox *pBox, BrTextBoxDeleteFlags flags)
{
    BrTextBoxDtor(pBox);
    if (flags.v & 1u) {
        BrOperatorDelete(pBox);
    }
    return pBox;
}
#else
BrTextBox *BrTextBoxDeleteDtor(BrTextBox *pBox, uint32_t flags)
{
    BrTextBoxDtor(pBox);
    if (flags & 1u) {
        BrOperatorDelete(pBox);
    }
    /* Returns the (possibly freed) pointer, exactly as the original does. */
    return pBox;
}
#endif

/* =====================================================================
 * 0x1005B0D0 / 0x1005B160 -- measure sz[]
 * ===================================================================== */

#ifdef BR_MATCHING_BUILD
/* GLIDE's font table records are 12 bytes (`lea eax,[eax+eax*2]; shl eax,2`
 * -- index * 12), not the 8-byte BrGlyphMetric the port carries.  Only the
 * first two words are read here.  Table base 0x100ABE84 in BRGlide.dll. */
typedef struct BrGlyphMetric12 {
    uint16_t advance;   /* +0x00 */
    uint16_t height;    /* +0x02 */
    uint16_t s4, s6, s8, sA;
} BrGlyphMetric12;
extern BrGlyphMetric12 g_BrGlyphFontA12[];   /* 0x100ABE84 (glide) */

extern BrGlyphMetric12 g_BrGlyphFontB12[];   /* 0x100AC2FC (glide) */

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
            adv = (int16_t)g_BrGlyphFontA12[k].advance;
            if (adv == -1 ||
                ((h = (int16_t)g_BrGlyphFontA12[k].height),
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
            (int16_t)g_BrGlyphFontA12[k].advance == -1 ||
            (int16_t)g_BrGlyphFontA12[k].height == -1) {
            if (c == ' ') {
                width = width + BR_GLYPH_SPACE_ADVANCE;
            }
        } else {
            width = width + (int16_t)(g_BrGlyphFontB12[k].advance - 4);
            if (maxH < (int16_t)g_BrGlyphFontB12[k].height) {
                maxH = (int16_t)g_BrGlyphFontB12[k].height;
            }
        }
        i = i + 1;
        c = pBox->sz[i];
    }
}
#else
/* The shared prologue of both measurers: decide what kind of character this
 * is.  Returns 1 for "in the glyph range", 0 for "space-or-nothing", -1 for
 * "stop the walk entirely". */
static int BrGlyphClassify(char c)
{
    /* movsx ax, dl ; sub eax, 0x20 ; test ax,ax / cmp ax,0x7f  -- a SIGNED
     * 16-bit test on the sign-extended byte, so 0x80..0xFF land negative. */
    int16_t k = (int16_t)((int16_t)(signed char)c - 0x20);

    if (k < 0 || k > 0x7F) {
        if (c != 0x20) {
            return -1;      /* < 0x20 or >= 0x80: stop measuring here */
        }
    }
    /* cmp dl,0x21 / jl ; cmp dl,0x7e / jg  -- signed byte compares */
    if ((signed char)c < 0x21 || (signed char)c > 0x7E) {
        return 0;
    }
    return 1;
}

void BrTextBoxMeasureA(BrTextBox *pBox)
{
    uint16_t width = 0;
    /* Seeded from the field, never reset. */
    uint16_t maxH  = (uint16_t)pBox->height;
    int      i     = 0;
    char     c     = pBox->sz[0];

    while (c != '\0') {
        int cls = BrGlyphClassify(c);
        int hit = 0;

        if (cls < 0) {
            break;
        }
        if (cls > 0) {
            const BrGlyphMetric *pG = &g_BrGlyphFontA[(unsigned char)c - BR_GLYPH_FIRST];

            if (pG->advance != BR_GLYPH_NONE && pG->height != BR_GLYPH_NONE) {
                width = (uint16_t)(width + pG->advance);
                if ((int16_t)maxH < (int16_t)pG->height) {
                    maxH = pG->height;
                }
                hit = 1;
            }
        }
        if (!hit && c == 0x20) {
            width = (uint16_t)(width + BR_GLYPH_SPACE_ADVANCE);
        }

        ++i;
        /* movsx eax, di -- the index is truncated to 16 bits and
         * sign-extended.  Harmless for a 0x400-byte buffer. */
        c = pBox->sz[(int16_t)i];
    }

    pBox->height = (int16_t)maxH;
    pBox->width  = (int16_t)width;
}

void BrTextBoxMeasureB(BrTextBox *pBox)
{
    uint16_t width = 0;
    uint16_t maxH  = (uint16_t)pBox->height;
    int      i     = 0;
    char     c     = pBox->sz[0];

    while (c != '\0') {
        int cls = BrGlyphClassify(c);
        int hit = 0;

        if (cls < 0) {
            break;
        }
        if (cls > 0) {
            unsigned idx = (unsigned)((unsigned char)c - BR_GLYPH_FIRST);
            /* GOTCHA: the sentinel test reads FONT A, the metrics read
             * FONT B.  Not a transcription slip -- see the header. */
            const BrGlyphMetric *pGate = &g_BrGlyphFontA[idx];
            const BrGlyphMetric *pG    = &g_BrGlyphFontB[idx];

            if (pGate->advance != BR_GLYPH_NONE && pGate->height != BR_GLYPH_NONE) {
                width = (uint16_t)(width + (uint16_t)(pG->advance - 4u));
                if ((int16_t)maxH < (int16_t)pG->height) {
                    maxH = pG->height;
                }
                hit = 1;
            }
        }
        if (!hit && c == 0x20) {
            width = (uint16_t)(width + BR_GLYPH_SPACE_ADVANCE);
        }

        ++i;
        c = pBox->sz[(int16_t)i];
    }

    pBox->height = (int16_t)maxH;
    pBox->width  = (int16_t)width;
}
#endif /* BR_MATCHING_BUILD */

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
typedef int (*funcptr)();
extern funcptr PTR_FUN_100776F0;
extern funcptr PTR_FUN_100776f0;

/* WHAT IT DOES: stub that always returns 0. */
/* @implements 0x10053E60 glide BrStubFalse */

int BrStubFalse(void)

{
  return 0;
}

/* WHAT IT DOES: vtable constructor: install the function-pointer table at PTR_FUN_100776F0 (fastcall). */
/* @implements 0x10053EE0 glide BrVtInit53EE0 */

int __fastcall BrVtInit53EE0(int *param_1)

{
  *param_1 = &PTR_FUN_100776f0;
  return;
}

#endif /* BR_MATCHING_BUILD */
