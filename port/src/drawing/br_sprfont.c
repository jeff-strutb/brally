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
 * All five are `shared` in config/shared.csv, so the two builds agree.
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
#include "br_sprfont.h"

#include <stddef.h>

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

int32_t g_aBrSprRectA[BR_SPRFONT_RECT_A][4];   /* 0x10AC4208  16x16, 8 wide  */
int32_t g_aBrSprRectB[BR_SPRFONT_RECT_B][4];   /* 0x10AC46C0  39x44, 5 wide  */
int32_t g_aBrSprRectC[BR_SPRFONT_RECT_C][4];   /* 0x10AC4AD8 128x128,5 wide  */
int32_t g_aBrSprRectD[BR_SPRFONT_RECT_D][4];   /* 0x10AC4BC8 128x128,3 wide  */

static void BrSprGrid(int32_t (*pTab)[4], int n, int cols, int cw, int ch)
{
    int i;

    for (i = 0; i < n; ++i) {
        int32_t l = (int32_t)(i % cols) * cw;
        int32_t t = (int32_t)(i / cols) * ch;

        pTab[i][0] = l;
        pTab[i][1] = t;
        pTab[i][2] = l + cw;
        pTab[i][3] = t + ch;
    }
}

/* WHAT IT DOES: works out, once at start-up, where every picture sits inside
 * four different sprite sheets -- the small letters, the large letters, and
 * two sheets of big square pictures -- by laying each sheet out as a fixed
 * grid. Everything that later draws a letter or a picture just asks for cell
 * number N and gets the rectangle from here. */
/* @implements 0x1005F800 d3d BrSprFontRectInit_1005F800 */
void BrSprFontRectInit_1005F800(void)
{
    BrSprGrid(g_aBrSprRectA, BR_SPRFONT_RECT_A, 8,  16,  16);
    BrSprGrid(g_aBrSprRectB, BR_SPRFONT_RECT_B, 5,  39,  44);
    BrSprGrid(g_aBrSprRectC, BR_SPRFONT_RECT_C, 5, 128, 128);
    BrSprGrid(g_aBrSprRectD, BR_SPRFONT_RECT_D, 3, 128, 128);
}

/* ==========================================================================
 * 2. 0x1005B730's leading chain -- kind to sheet
 * ========================================================================== */

/* WHAT IT DOES: turns a piece of text's style setting into the lettering sheet
 * it should be drawn from -- grey, white, mid-grey or yellow. Any style it does
 * not recognise falls back to sheet zero, which is not a lettering sheet at all
 * but the general artwork sheet, so an unexpected style draws garbage rather
 * than plain text. */
/* @implements 0x1005B730 d3d BrSprFontSheet_1005B730 */
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

void BrSprFontGlyphA_1005B730(const BrTextBox *pBox, int32_t iGlyph,
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
void BrSprFontGlyphB_1005B7A0(int32_t iGlyph, float x, float y,
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
static int BrSprGlyphClassify(char c)
{
    /* `movsx cx, al / sub ecx,0x20 / test cx,cx / jl / cmp cx,0x7f / jle` --
     * a SIGNED 16-bit test on the sign-extended byte, so 0x80..0xFF land
     * negative. */
    int16_t k = (int16_t)((int16_t)(signed char)c - 0x20);

    if (k < 0 || k > 0x7F) {
        if (c != 0x20) {
            return -1;
        }
    }
    /* `cmp al,0x21 / jl` and `cmp al,0x7e / jg` -- signed BYTE compares. */
    if ((signed char)c < 0x21 || (signed char)c > 0x7E) {
        return 0;
    }
    return 1;
}

/* WHAT IT DOES: decides where the first letter of a line of text goes. Normally
 * that is just the text box's own left edge, but a box marked as centred is
 * asked to work out its centred starting point instead. */
/* @implements 0x1005B2B0 d3d BrSprFontPenStart_1005B2B0 */
float BrSprFontPenStart_1005B2B0(BrTextBox *pBox)
{
    if (pBox == NULL) {
        return 0.0f;                /* DEVIATION: the original faults. */
    }
    /* `test byte [edi+4],1` -- re-centre, and take the float the centring
     * method RETURNS rather than re-reading +0x410.  BrTextBoxCentreX stores
     * the same value into +0x410 on its way out, so the two agree; the
     * original reads the return value and so does this. */
    if ((pBox->f04 & 1u) != 0 && pBox->pVtbl != NULL
        && pBox->pVtbl->pfn28 != NULL) {
        return pBox->pVtbl->pfn28(pBox);
    }
    return pBox->x;
}

float BrSprFontDraw_1005B2B0(BrTextBox *pBox,
                             BrSprFontBlitFn pfnBlit, void *pCtx)
{
    float x;
    int   i;
    char  c;

    if (pBox == NULL) {
        return 0.0f;                /* DEVIATION: the original faults. */
    }

    x = BrSprFontPenStart_1005B2B0(pBox);
    i = 0;
    c = pBox->sz[0];

    while (c != '\0') {
        int cls = BrSprGlyphClassify(c);

        if (cls < 0) {
            break;
        }
        if (cls > 0) {
            const BrGlyphMetric *pG =
                &g_BrGlyphFontA[(unsigned char)c - BR_GLYPH_FIRST];

            /* The DRAW gates on `sprite`; the two MEASURERS gate on
             * `advance` and `height`.  Preserved -- see the header. */
            if (pG->sprite != BR_GLYPH_NONE) {
                BrSprFontGlyphA_1005B730(pBox, (int32_t)(int16_t)pG->sprite,
                                         x, pBox->y,
                                         (int32_t)(int8_t)pBox->f08,
                                         pfnBlit, pCtx);
                /* `movsx edx, word [esi+0x100ABE84] / fild / fadd` -- the
                 * advance is read SIGNED and added as an integer. */
                x += (float)(int32_t)(int16_t)pG->advance;
            }
        } else if (c == 0x20) {
            /* `fsub [0x10077674]`, and that dword is -6.0f. */
            x -= -6.0f;
        }

        ++i;
        /* `movsx eax, bx` -- the index is truncated to 16 bits and
         * sign-extended, exactly as the measurers do it. */
        c = pBox->sz[(int16_t)i];
    }

    /* The +0x420 tail.  The predicate is reproduced; the call is not, because
     * slice3_39.h's type for the vtable slot it dispatches through disagrees
     * with the disassembly about the arity.  See the header.  Nothing in the
     * tree fills the slot, so the call is unreachable either way. */
    if (pBox->f420 != 0) {
        /* pBox->pVtbl->pfn24(pBox, x, pBox->y); */
    }

    return x;
}

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

int32_t BrSprFontKindHook_10047360(BrUiCtl_ *pCtl)
{
    uint32_t uFlags;
    int32_t  iCase;
    uint16_t uCount;

    if (pCtl == NULL) {
        return 0;                   /* DEVIATION: the original faults. */
    }

    uFlags = (uint32_t)pCtl->flags1C;
    if ((uFlags & 0x00000010u) != 0) {
        return 0;                   /* disabled */
    }
    if ((uFlags & 0x01000000u) != 0) {
        return 0;
    }
    /* `test dword [eax+0x3850], 0x1000000`.  br_ui.h's ADJ-6 puts +0x3850
     * inside the embedded list, at list +0x18. */
    if ((pCtl->list.f18 & 0x01000000u) != 0) {
        return 0;
    }

    /* The MOUSE arm.  0x10AA284C was written by 0x10047A60 for THIS control,
     * moments ago, in the same pass of 0x10048180. */
    if (g_pBrUiNav != NULL && g_pBrUiNav->nAA284C != 0
        && g_pBrUiNav->pG != NULL) {
        const BrObjAA2E80 *q = g_pBrUiNav->pG->pAA2E80;

        /* No NULL guard in the original; one here, because a host that has
         * not wired the input object would otherwise fault on a path the
         * original can never reach with it unset. */
        if (q != NULL && (q->f2C != 0 || q->f30 != 0
                          || q->f34 != 0 || q->f38 != 0)) {
            pCtl->aText[0].f08 = 4;         /* images\type_yel.bmp */
            /* and +0x1C is NOT stored back on this arm */
            return 1;
        }
    }

    /* `test dh,1` -- bit 0x100, which the step timer 0x100480A0 raises once
     * every 60 ms.  Without it the kind byte is left exactly as it was, which
     * is what makes the pulse a pulse and not a per-frame flicker. */
    if ((uFlags & 0x00000100u) == 0) {
        return 1;
    }

    /* `inc word [eax+0x1e20c]` then `movsx ecx, word [eax+0x1e20c]`. */
    uCount = (uint16_t)(pCtl->w1E20C + 1u);
    pCtl->w1E20C = uCount;

    /* -2, then an UNSIGNED compare against 0x32: counts of 0 and 1 wrap
     * negative and land in the default. */
    iCase = (int32_t)(int16_t)uCount - 2;

    uFlags &= ~0x00000100u;

    if ((uint32_t)iCase <= 0x32u) {
        switch (g_aBrSprKindArm[(uint32_t)iCase]) {
        case 0:  pCtl->aText[0].f08 = 0; break;     /* type_gry */
        case 1:  pCtl->aText[0].f08 = 1; break;     /* type_wit */
        case 2:  pCtl->aText[0].f08 = 2; break;     /* type_mid */
        case 3:  pCtl->aText[0].f08 = 4; break;     /* type_yel */
        default: pCtl->w1E20C = 2;       break;
        }
    } else {
        pCtl->w1E20C = 2;
    }

    pCtl->flags1C = (int32_t)uFlags;
    return 1;
}
