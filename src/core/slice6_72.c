/* slice6_72.c -- BRD3D.dll, packet 72.  See slice6_72.h.
 *
 * Float literals below are the exact values of the 32-bit patterns the
 * original pushes or loads.  The ones read as memory operands were taken out
 * of orig/BRD3D.dll's .rdata with tools/pe.py, not assumed:
 *
 *   0x1008F410 =    0.0     0x1008F514 =    2.0
 *   0x1008F3BC =  255.0     0x1008F3C0 =    1/255 (0x3B808081)
 *   0x1008F680 =  -19.0     0x1008F684 =  -38.0     0x1008F688 =  -57.0
 *   0x1008F68C =  -76.0     0x1008F690 =  -95.0     0x1008F694 = -114.0
 *   0x1008F698 = -133.0     0x1008F69C =  -33.0     0x1008F6A0 =  +19.0
 *
 * `fsub m32` is the non-reversed form, st(0) = st(0) - m32, so every row
 * constant except 0x1008F6A0 -- which is POSITIVE -- is an addition.
 */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <string.h>

#include "slice6_72.h"

Br72Env *g_pBr72Env = NULL;

/* The render-state ids the apply loop of 0x1001BE90 pushes, in index order.
 * Index i pairs 0x10277378+4i (wanted) with 0x102773F8+4i (applied) and bit i
 * of 0x10277370. */
const int32_t g_aBr72RsId[BR72_RS_COUNT] = {
    0x0E, 0x0F, 0x13, 0x14, 0x16, 0x17, 0x18, 0x19, 0x2C, 0x2D, 0x1B
};

/* ==========================================================================
 * 0x1003E3A0
 * ========================================================================== */
/* WHAT IT DOES: makes a set of options actually take effect. It picks the data
 * table that goes with the selected entry, copies its name into the buffer the
 * menus display, notes four on/off states, and then republishes a saved block
 * of a dozen settings out into the individual globals the rest of the game
 * reads. One setting cannot hold the value 1 and is quietly promoted to 2 on
 * the way through. */
/* @implements 0x1003E3A0 d3d BrSub1003E3A0 */
void BrSub1003E3A0(void)
{
    Br72Env *pE = g_pBr72Env;
    int32_t  n;

    /* DEVIATION (memory safety): the original indexes 0x100AC520 with
     * 0x10AA2A0C and does not range-check it.  The port does; the shipped
     * selector never leaves 0..3. */
    n = 0;
    if (pE->aAC520 != NULL &&
        pE->nAA2A0C >= 0 && pE->nAA2A0C < pE->cAC520) {
        n = pE->aAC520[pE->nAA2A0C];
    }
    pE->nB4E1D0 = n;

    /* 0x1003E3B3 -- a dec-chain, i.e. 1/2/3 with everything else defaulting. */
    if (n == 1) {
        pE->pB4E1D4 = pE->pB4DFD8;
    } else if (n == 2) {
        pE->pB4E1D4 = pE->pB4E080;
    } else if (n == 3) {
        pE->pB4E1D4 = pE->pB4E128;
    } else {
        pE->pB4E1D4 = pE->pB4DF30;
    }

    /* 0x1003E3EA -- four "is zero" flags.  The original loads all four values
     * first and interleaves the tests; the results are independent. */
    pE->nAA2A1C = (pE->nB4E1E0 == 0);
    pE->nAA2A20 = (pE->nB4E1D8 == 0);
    pE->nAA2A24 = (pE->nB4E1DC == 0);
    pE->nAA2A28 = (pE->nB4E7A0 == 0);

    /* 0x1003E435 -- strlen + rep movs of len+1 bytes: a plain strcpy, and as
     * unbounded as the original.
     * DEVIATION (memory safety): bounded to the destination's room. */
    if (pE->pszB4E1E4 != NULL) {
        size_t cb = strlen(pE->pszB4E1E4) + 1u;
        if (cb > sizeof(pE->szA9CDF0)) {
            cb = sizeof(pE->szA9CDF0);
        }
        memcpy(pE->szA9CDF0, pE->pszB4E1E4, cb);
        pE->szA9CDF0[sizeof(pE->szA9CDF0) - 1u] = '\0';
    }

    if (pE->pfn1003E2C0 != NULL) {
        pE->pfn1003E2C0();
    }

    /* 0x1003E45F -- republish the twelve-dword config block. */
    pE->n0AC648 = pE->cfgB4E710.nB4E710;
    pE->nAA2A00 = pE->cfgB4E710.nB4E714;
    pE->nAA2A08 = pE->cfgB4E710.nB4E718;
    pE->n0AC64C = pE->cfgB4E710.nB4E71C;
    pE->n0AC650 = pE->cfgB4E710.nB4E720;
    pE->n0AC654 = pE->cfgB4E710.nB4E724;

    pE->nAA2A0C = pE->cfgB4E710.nB4E728;
    if (pE->cfgB4E710.nB4E728 == 1) {
        /* GOTCHA: 1 is not representable -- it is promoted to 2 here. */
        pE->nAA2A0C = 2;
    }

    pE->n0AC658 = pE->cfgB4E710.nB4E72C;
    pE->n0AC65C = pE->cfgB4E710.nB4E738;
    /* GOTCHA: a WORD-wide OR of the LOW HALF of a dword. */
    pE->w0AB3E4 = (uint16_t)(pE->w0AB3E4 | (uint16_t)pE->cfgB4E710.nB4E730);
    pE->nAA2A10 = pE->cfgB4E710.nB4E730;
    pE->n0AB3EC = pE->n0AB3EC | pE->cfgB4E710.nB4E734;
    pE->nAA2A14 = pE->cfgB4E710.nB4E734;
    pE->nAA2A18 = pE->cfgB4E710.nB4E73C;
}

/* ==========================================================================
 * 0x10035FC0 -- thiscall, two dwords
 * ========================================================================== */
void BrEnt35FC0(void *pThis)
{
    uint32_t *p = (uint32_t *)pThis;
    uint32_t  a = p[0];
    uint32_t  b = p[1];

    p[0] = a & ~b;      /* "newly set"  */
    p[1] = a & b;       /* "still set"  */
}

/* ==========================================================================
 * 0x1005B0C0 -- thiscall
 * ========================================================================== */
/* WHAT IT DOES: the tidy-up step for a piece of on-screen text. All it does is
 * point the object back at its base set of behaviours, which is what a C++
 * destructor for a class with no owned memory compiles to -- there is nothing
 * to release. */
/* @d3donly 0x1005B0C0 BrTextBoxDtor -- exists in BRGlide only as folded/duplicated stubs; no unique twin locatable by bytes */
/* @n64 0x80225ED8 located */
void BR_THISCALL1 BrTextBoxDtor(BrTextBox *pBox)
{
    /* The original is `mov dword ptr [ecx], 0x1008F728; ret` -- seven bytes.
     * Two things have to line up.  `this` arrives in ecx, which BR_THISCALL1
     * supplies; and the vtable is planted as an immediate ADDRESS, so this
     * has to be an address-of, not a load through the g_pBrTextBoxVtbl hook
     * (that would emit an extra `mov eax,[mem]` and miss by an instruction). */
#ifdef BR_MATCHING_BUILD
    pBox->pVtbl = &g_BrTextBoxVtbl;
#else
    pBox->pVtbl = g_pBrTextBoxVtbl;
#endif
}

/* ==========================================================================
 * 0x1002B2A0
 * ========================================================================== */
int BrSub_1002B2A0(void)
{
    Br72Env *pE = g_pBr72Env;

    /* `fld 0x10575514 / fcomp 0x1008F410 (0.0f) / fnstsw / test ah,1` --
     * ah bit 0 is C0, which x87 also sets for UNORDERED, so a NaN takes the
     * "return 1" side.  Written as the negated comparison. */
    if (!(pE->f575514 >= 0.0f)) {
        return 1;
    }
    if (pE->n575530 != 0) {
        return 1;
    }
    return 0;
}

/* ==========================================================================
 * 0x1003407D
 * ========================================================================== */
void BrSub_1003407D(float a, float b)
{
    Br72Env    *pE = g_pBr72Env;
    float      *m  = pE->aMtx6C29A8;
    Br72GfxCmd *pCmd;
    int         i;

    /* 0x1008F514 == 2.0f.  GOTCHA: a or b == 0 divides by zero in x87; the
     * original's control word has the exception masked, so it produces an
     * infinity rather than trapping.  Reproduced by not checking. */
    for (i = 0; i < 16; ++i) {
        m[i] = 0.0f;
    }
    m[0]  =  2.0f / a;     /* 0x106C29A8 */
    m[5]  =  2.0f / b;     /* 0x106C29BC */
    m[12] = -1.0f;         /* 0x106C29D8 (0xBF800000) */
    m[13] = -1.0f;         /* 0x106C29DC */
    m[14] =  0.0f;         /* 0x106C29E0 */
    m[15] =  1.0f;         /* 0x106C29E4 (0x3F800000) */

    /* 0x10034139 -- bump the cursor by one 8-byte command, then fill it. */
    pCmd = pE->pDlCursor;
    pE->pDlCursor = pCmd + 1;
    pCmd->w0 = 0xBC00000Eu;
    pCmd->w1 = pE->w6C067C;            /* zero-extended word */
    pCmd->p1 = NULL;

    pE->p6C32D0 = pE->pfn10069490();
    pE->pfn100307A0(m, pE->p6C32D0);

    pCmd = pE->pDlCursor;
    pE->pDlCursor = pCmd + 1;
    pCmd->w0 = 0x01030040u;
    /* DEVIATION: the original stores the POINTER 0x106C32D0 holds into the
     * command's second dword.  A host pointer does not fit in 32 bits, so it
     * goes in the parallel slot and w1 is left zero.  See Br72GfxCmd. */
    pCmd->w1 = 0u;
    pCmd->p1 = pE->p6C32D0;
}

/* ==========================================================================
 * 0x1001BE90 -- fill a rectangle
 * ==========================================================================
 *
 * The vertex is D3DTLVERTEX: the `rep movsd` count is 8 and the primitive is
 * drawn with dwVertexTypeDesc == 3.  Two triangles make the quad; y is
 * flipped about 0x100A81C4.
 */
/* WHAT IT DOES: fills in one corner of a flat coloured rectangle for the
 * graphics card -- its position, its colour and which bit of the texture it
 * takes. Six of these make the two triangles that a filled box is drawn as. */
/* NOT A CLAIM.  This is a HELPER of BrSub_1001BE90 below, which is where
 * 0x1001BE90's @implements line lives.  It used to be here, and it was the
 * br_ftol64 shape exactly: 1,934 bytes of original attached to eight field
 * writes, while the real transcription sat forty lines further down with no
 * claim on it at all.  The original has no such helper -- the eight stores
 * are written out six times inline at 0x1001C2D3 and 0x1001C3E7. */
static void Br72FillVert(BrD3DTLVertex *pV, float x, float y,
                         uint32_t color, float tu, float tv)
{
    pV->x        = x;
    pV->y        = y;
    pV->z        = 0.0f;
    pV->rhw      = 1.0f;
    pV->diffuse  = color;
    pV->specular = 0xFF0000FFu;
    pV->tu       = tu;
    pV->tv       = tv;
}

/* RENDERER SLOT -- THIS IS THE D3D IMPLEMENTATION, AND THE GLIDE ONE IS NOT
 * TRANSCRIBED.  See "Renderer slots" in CONVENTIONS.md for why that is stated
 * rather than fixed.
 *
 *     slot           the rectangle filler, 2 aligned callsites
 *     D3D    0x1001BE90   1,934 bytes   534 instructions   <-- this body
 *     Glide  0x1001E380     914 bytes   228 instructions   NOT PORTED
 *     config/shared.csv: class `renderer`, matched_by `slot`, similarity 0.118
 *
 * The two are not variants of one routine.  This body builds BrD3DTLVertex
 * records -- rhw, specular, a D3D device -- and the Glide body reaches the
 * hardware through glide2x.dll directly: its calls resolve to _grDrawTriangle,
 * _grClipWindow, _grAlphaCombine, _grAlphaBlendFunction, _grAlphaTestFunction,
 * _grAlphaTestReferenceValue, _grCullMode, _grDepthMask and
 * _grDepthBufferFunction.  There is no constant to swap and no gate to
 * restore; there are two implementations and this tree contains one.
 *
 * The `d3d` tag below is therefore ACCURATE, not a defect -- it is the reason
 * a census of d3d-tagged claims surfaces this address, and the reason the
 * right response is a label rather than an edit.
 *
 * That label is now applied.  Left as @implements, the sweep mapped this body
 * onto glide 0x1001E380 through config/shared.csv and scored it against 914
 * bytes it shares no code with, which put a FINISHED address (BrGlRectFill in
 * src/core/drawing/br_dlglide.c, byte-exact) at the top of the lane ranking
 * as the single best open target -- 914 B with a 315-instruction structural
 * gap that no amount of work could ever close.  @d3donly says the same thing
 * the paragraph above says, in the form the tooling reads. */
/* WHAT IT DOES: paints a flat coloured rectangle over part of the screen. It
 * first pulls the corners back inside the visible area, works out the colour
 * from whichever of the two colour sources the renderer currently has live,
 * and then draws the box -- either as two triangles through the graphics card,
 * or, on a machine not running the Direct3D path, by handing the rectangle to
 * the software surface instead. */
/* @d3donly 0x1001BE90 BrSub_1001BE90 -- glide 0x1001E380 is a DIFFERENT
 * implementation (glide2x direct, 914 B, similarity 0.118), matched as
 * BrGlRectFill in src/core/drawing/br_dlglide.c */
void BrSub_1001BE90(int32_t x1, int32_t y1, int32_t x2, int32_t y2)
{
    Br72Env      *pE = g_pBr72Env;
    BrD3DDev     *pDev;
    uint8_t       bR, bG, bB, bA;
    uint32_t      color;
    int32_t       h;
    BrD3DTLVertex aTri[3];
    int           i;

    /* 0x1001BE9F -- the four clamps.  Each writes the argument slot; the
     * y1 clamp is the only one that does not also update its register, and
     * every later use re-reads the slot, so it makes no difference. */
    if (x1 < pE->n4C516C) { x1 = pE->n4C516C; }
    if (y1 < pE->n4C5170) { y1 = pE->n4C5170; }
    if (x2 > pE->n4C5164) { x2 = pE->n4C5164; }
    if (y2 > pE->n4C01A0) { y2 = pE->n4C01A0; }

    /* 0x1001BEFC -- two magic numbers select the float colour source. */
    if (pE->n4C5158 == 0xFCFFFFFFu && pE->n4C515C == 0xFFFDF6FBu) {
        /* __ftol: truncate toward zero, low dword of a 64-bit fistp. */
        bR = (uint8_t)BrFtolTrunc(pE->f4C5154 * 255.0f);
        bG = (uint8_t)BrFtolTrunc(pE->f4C5160 * 255.0f);
        bB = (uint8_t)BrFtolTrunc(pE->f4C1690 * 255.0f);
        bA = (uint8_t)BrFtolTrunc(pE->f4C0BA8 * 255.0f);
    } else {
        bR = pE->b4BBF00;
        bG = pE->b4BC194;
        bB = pE->b4C5150;
        bA = pE->b4C15CC;
    }

    if (pE->n4C1694 != 0x504340) {
        /* ------------------------------------------------------------------
         * 0x1001C50D -- the non-Direct3D arm.
         * ------------------------------------------------------------------ */
        BrGfxCtx    *pG;
        BrGfxSurf   *pSurf;
        BrGfxTarget *pTarget;
        int32_t      aRect[4];

        if (pE->n4C518C != 0) {
            pDev = pE->pDev277368;
            pDev->pVtbl->DrawIndexedPrimitive(pDev, 4, 3,
                                              pE->p4BC1A0, pE->n4C5188,
                                              pE->p4C4D50, pE->n4C518C, 0x0C);
            pE->pfn1001C640();
        }

        h  = pE->n0A81C4;
        pG = pE->pCtx27736C;

        /* The rect is assembled at [esp+0x40] as {x1, h-y2, x2, h-y1}. */
        aRect[0] = x1;
        aRect[1] = h - y2;
        aRect[2] = x2;
        aRect[3] = h - y1;

        pSurf = pG->pSurf;
        /* 0x1008F3C0 == 1/255.  The three bytes go through fild, i.e. as
         * SIGNED ints, but they were masked to 0..255 first. */
        pSurf->f04 = (float)(int32_t)bR * (1.0f / 255.0f);
        pSurf->f58 = 1;
        pSurf->f08 = (float)(int32_t)bG * (1.0f / 255.0f);
        pSurf->f0C = (float)(int32_t)bB * (1.0f / 255.0f);

        pG      = pE->pCtx27736C;      /* the original re-reads it */
        pSurf   = pG->pSurf;
        pTarget = pG->pTarget;
        pSurf->f4C = 1;
        pSurf->f58 = 1;
        pE->pfn1001C620(pSurf);
        pTarget->pVtbl->f20(pTarget, pSurf->f50);

        pG      = pE->pCtx27736C;      /* and again */
        pTarget = pG->pTarget;
        pTarget->pVtbl->f30(pTarget, 1, aRect, 3);
        return;
    }

    /* ----------------------------------------------------------------------
     * 0x1001BF9C -- the Direct3D arm.
     * -------------------------------------------------------------------- */

    /* 0x100A79EC gets one of two code addresses depending on 0x106C6618. */
    pE->pfn0A79EC = (pE->n6C6618 != 0) ? pE->pfn1001C690 : pE->pfn1001BC90;

    /* 0x1001BFBB -- publish the wanted render states and mark the ones that
     * differ from what is currently applied.  Indices 4, 8 and 9 are left
     * alone here but their bits are still honoured by the apply loop. */
    {
        uint32_t dirty = pE->nDirty277370;

        pE->aWant277378[2] = 5;
        dirty = (pE->aHave2773F8[2] == 5) ? (dirty & ~0x004u) : (dirty | 0x004u);

        pE->aWant277378[3] = 6;
        dirty = (pE->aHave2773F8[3] == 6) ? (dirty & ~0x008u) : (dirty | 0x008u);

        pE->aWant277378[6] = 0;
        dirty = (pE->aHave2773F8[6] == 0) ? (dirty & ~0x040u) : (dirty | 0x040u);

        pE->n4BBE28        = 8;
        pE->aWant277378[7] = 8;
        dirty = (pE->aHave2773F8[7] == 8) ? (dirty & ~0x080u) : (dirty | 0x080u);

        pE->aWant277378[1] = 0;
        dirty = (pE->aHave2773F8[1] == 0) ? (dirty & ~0x002u) : (dirty | 0x002u);

        pE->aWant277378[10] = 1;
        dirty = (pE->aHave2773F8[10] == 1) ? (dirty & ~0x400u) : (dirty | 0x400u);

        pE->n4C16A0        = 8;
        pE->aWant277378[5] = 8;
        dirty = (pE->aHave2773F8[5] == 8) ? (dirty & ~0x020u) : (dirty | 0x020u);

        pE->aWant277378[0] = 0;
        dirty = (pE->aHave2773F8[0] == 0) ? (dirty & ~0x001u) : (dirty | 0x001u);

        pE->nDirty277370 = dirty;

        if (dirty != 0u) {
            if (pE->n4C518C != 0) {
                pDev = pE->pDev277368;
                pDev->pVtbl->DrawIndexedPrimitive(pDev, 4, 3,
                                                  pE->p4BC1A0, pE->n4C5188,
                                                  pE->p4C4D50, pE->n4C518C,
                                                  0x0C);
                pE->pfn1001C640();
            }
            for (i = 0; i < BR72_RS_COUNT; ++i) {
                /* The original re-reads 0x10277370 between arms; nothing in
                 * the loop writes it, so one snapshot is equivalent. */
                if ((pE->nDirty277370 & (1u << i)) != 0u) {
                    pDev = pE->pDev277368;
                    pDev->pVtbl->SetRenderState(pDev, g_aBr72RsId[i],
                                                pE->aWant277378[i]);
                    pE->aHave2773F8[i] = pE->aWant277378[i];
                }
            }
            pE->nDirty277370 = 0u;
        }
    }

    /* 0x1001C28D -- pack A/R/G/B.  The bytes are masked to 8 bits first. */
    color = ((uint32_t)bA << 24) | ((uint32_t)bR << 16) |
            ((uint32_t)bG <<  8) |  (uint32_t)bB;

    pDev = pE->pDev277368;
    pDev->pVtbl->SetRenderState(pDev, 1, 0);
    pDev = pE->pDev277368;
    pDev->pVtbl->SetRenderState(pDev, 0x16, 1);

    h = pE->n0A81C4;

    /* 0x1001C2D3 -- triangle 1 */
    Br72FillVert(&aTri[0], (float)x1, (float)(h - y2), color, 0.0f, 0.0f);
    Br72FillVert(&aTri[1], (float)x2, (float)(h - y1), color, 1.0f, 1.0f);
    Br72FillVert(&aTri[2], (float)x1, (float)(h - y1), color, 0.0f, 1.0f);
    pDev = pE->pDev277368;
    pDev->pVtbl->DrawPrimitive(pDev, 4, 3, aTri, 3, 9);

    /* 0x1001C3E7 -- triangle 2 */
    Br72FillVert(&aTri[0], (float)x1, (float)(h - y2), color, 0.0f, 0.0f);
    Br72FillVert(&aTri[1], (float)x2, (float)(h - y2), color, 1.0f, 0.0f);
    Br72FillVert(&aTri[2], (float)x2, (float)(h - y1), color, 1.0f, 1.0f);
    pDev = pE->pDev277368;
    pDev->pVtbl->DrawPrimitive(pDev, 4, 3, aTri, 3, 9);

    /* 0x1001C4EE -- restore render state 0x16 from the applied cache. */
    pDev = pE->pDev277368;
    pDev->pVtbl->SetRenderState(pDev, 0x16, pE->aHave2773F8[4]);
}

/* ==========================================================================
 * 0x10005B10
 * ========================================================================== */
/* WHAT IT DOES: creates the locks that keep the game's threads out of each
 * other's way -- sixteen for one bank of shared slots and ten more for
 * individual pieces of shared state -- clears two counters, and starts the two
 * subsystems that then rely on them. The argument its callers pass is never
 * read. */
void BrSub10005B10(int32_t a)
{
    Br72Env *pE = g_pBr72Env;
    int      i;

    /* GOTCHA: the original takes no argument at all; the caller's
     * declaration passes 1 and it is dropped on the floor. */
    (void)a;

    /* 0x10005B1D -- 16 slots of stride 0x978 from 0x10221328, the loop
     * bound being 0x1022AAA8. */
    for (i = 0; i < BR72_MUTEX_BANK; ++i) {
        pE->aMutexBank[i] = pE->pfnCreateMutex();
    }

    /* 0x10005B35 -- 0x1022AF24, 0x1022AF28, 0x1022AF2C, 0x1022AF30 */
    for (i = 0; i < 4; ++i) {
        pE->aMutexExtra[i] = pE->pfnCreateMutex();
    }

    pE->n221310 = 0;      /* 0x10221310 */
    pE->n220DD8 = 0;      /* 0x10220DD8 */
    pE->pfn10075100();

    /* 0x10005B82 -- 0x1022AF34, 0x10221324, 0x1022AF04, 0x10220DDC,
     *               0x1022131C, 0x10220CEC */
    for (i = 4; i < BR72_MUTEX_EXTRA; ++i) {
        pE->aMutexExtra[i] = pE->pfnCreateMutex();
    }

    pE->pfn10005960();
    /* The original returns 1; the caller's declaration discards it. */
}

/* ==========================================================================
 * 0x100440D0 -- a THUNK
 * ==========================================================================
 *
 * Byte-for-byte the body slice2_25.c already ports as BrOptOpen294C:
 * `if (g_brPAA294C) { g_brPAA2904 = it; return 1; }` then operator new(0xC8),
 * the 0x10048710 constructor, pfn04 = 0x100575F0, call it, f0C = 1, f68 = 1.
 * slice5_61.h records the equality.  Forwarded rather than re-transcribed so
 * the two can never drift.
 */
void BrExt_100440D0(int32_t a)
{
    (void)a;                    /* the original reads no argument */
    (void)BrOptOpen294C(NULL);  /* the original returns 1 / 0; discarded */
}

/* ==========================================================================
 * 0x1003CDA0
 * ========================================================================== */
static void Br72GlobalRelease(Br72Env *pE, void *pv)
{
    /* GlobalHandle is called TWICE on the same pointer, once for the unlock
     * and once for the free, exactly as the original does. */
    pE->pfnGlobalUnlock(pE->pfnGlobalHandle(pv));
    pE->pfnGlobalFree(pE->pfnGlobalHandle(pv));
}

void BrExt_1003CDA0(void)
{
    Br72Env         *pE    = g_pBr72Env;
    BrDPSessionUser *pDesc = NULL;
    void            *pDP;
    int32_t          hr;

    pDP = pE->pDPlay277B40;
    if (pDP == NULL) {
        /* 0x88770082 -- the original's early-out hresult, discarded here. */
        return;
    }

    hr = pE->pfn1003D0B0(pDP, &pDesc);
    if (hr >= 0) {
        pDesc->dwUser1 = pE->n0B380C;
        pDesc->dwUser2 = pE->n22B350;
        pDesc->dwUser3 = pE->nAA2A18;
        pDesc->dwUser4 = pE->n0AC658;

        BrSub10044540();

        pDP = pE->pDPlay277B40;         /* the original re-reads it */
        hr  = pE->pfnDPSetSessionDesc(pDP, pDesc, 0u);
    }

    if (hr < 0) {
        /* 0x1003CE20 -- the failure path null-checks the descriptor. */
        if (pDesc != NULL) {
            Br72GlobalRelease(pE, pDesc);
        }
        return;
    }
    /* 0x1003CE4C -- the success path does NOT null-check it.  Reproduced;
     * hr >= 0 means 0x1003D0B0 produced one. */
    Br72GlobalRelease(pE, pDesc);
}

/* ==========================================================================
 * The six menu-screen builders
 * ==========================================================================
 *
 * The two allocation steps are byte-identical everywhere they appear, in this
 * packet and in slice3_33.c's five twins.
 *
 * DEVIATION (memory safety, both helpers): the array writes are bounded.  The
 * original has the same implicit bounds -- aPages ends where aFlags begins,
 * apCtl ends at the first float -- but does not check them.
 *
 * DEVIATION (memory safety, both helpers): on allocation failure the original
 * reports error index 4 and then dereferences NULL.  Index 4 is FATAL in
 * g_aBrErrTable (slice1_06.c), so BrErrShow does not return there in
 * practice; the port returns instead of faulting.
 */
static BrUiPage_ *Br72ScreenNew(BrPhase_ *pPhase, float fX, float fY)
{
    Br72Env   *pE = g_pBr72Env;
    BrUiPage_ *pScr;
    uint16_t   i;

    i = pPhase->nPages;
    pPhase->iPage = 0;
    if (i < BR_PHASE_PAGES) {
        pPhase->aFlags[i] = 1;
    }

    pScr = (BrUiPage_ *)BrOperatorNew(BR72_ALLOC(BrUiPage_,
                                                 BR72_PAGE_ORIG_SIZE));
    pScr = (pScr != NULL) ? pE->pfnPageCtor(pScr) : NULL;

    /* The original re-reads the counter here rather than reusing `i`. */
    i = pPhase->nPages;
    if (i < BR_PHASE_PAGES) {
        pPhase->aPages[i] = pScr;
    }
    if (pScr == NULL) {
        BrErrShow(pE->pErrHost, 4);
    }
    pPhase->nPages++;

    if (pScr == NULL) {
        return NULL;                    /* DEVIATION: see above */
    }

    pScr->pOwner = pPhase;
    pScr->f10    = 0;
    pScr->fX     = fX;
    pScr->fY     = fY;
    return pScr;
}

static BrUiCtl_ *Br72CtlNew(BrUiPage_ *pScr)
{
    Br72Env  *pE = g_pBr72Env;
    BrUiCtl_ *pCtl;

    pCtl = (BrUiCtl_ *)BrOperatorNew(BR72_ALLOC(BrUiCtl_,
                                                BR72_CTL_ORIG_SIZE));
    pCtl = (pCtl != NULL) ? pE->pfnCtlCtor(pCtl) : NULL;

    /* Stored BEFORE the null test, exactly as the original does. */
    if (pScr->cCtl < BR72_PAGE_CTL_MAX) {
        pScr->apCtl[pScr->cCtl] = pCtl;
    }
    if (pCtl == NULL) {
        BrErrShow(pE->pErrHost, 4);
    }
    return pCtl;
}

/* Shorthand so the transcriptions stay readable.  Relies on the local names
 * pScr / pCtl, which every builder below declares. */
#define BR_NEW_CTL()                                    \
    do {                                                \
        pCtl = Br72CtlNew(pScr);                        \
        if (pCtl == NULL) { return; }                   \
    } while (0)

/* ==========================================================================
 * 0x10056A10 -- BrOptFn10056A10.  Eight controls.
 * ==========================================================================
 *
 * GOTCHA: this is the ONLY builder in the family whose screen fX is 190.0f
 * (0x433E0000).  Every other one uses 195.0f.
 */
/* WHAT IT DOES: builds the "modem dial-up" screen, where the player types the
 * phone number to call for a modem game. It puts up the multiplayer banner, the
 * heading, a "Phone Number" caption, a typing field on its name-bar graphic
 * pre-filled with the number last used, and Continue and Back rows. It is the
 * only screen in the game laid out five pixels to the left of all the others. */
/* port-only body; Glide match is src/core/cpp/0x1004F8C0.cpp */
void BrOptFn10056A10(BrPhase_ *pPhase)
{
    Br72Env           *pE = g_pBr72Env;
    const BrUi72Hooks *pH = pE->pHooks;
    BrUiPage_         *pScr;
    BrUiCtl_          *pCtl;

    /* 0x10056A2A..0x10056AB6 -- fX = 190.0f, fY = 130.0f (0x43020000). */
    pScr = Br72ScreenNew(pPhase, 190.0f, 130.0f);
    if (pScr == NULL) {
        return;
    }

    /* 0x10056AB6 -- the unnamed root control.  The owner argument is the
     * PHASE, not the screen, at every f38 site in the family. */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 0.0f, 0.0f, 9, 2, 5, 0, 0);
    pScr->cCtl++;

    /* 0x10056B14 -- the title, at (fX, 10.0f == 0x41200000). */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, 10.0f, 0x100009, 2, 5, 1, -1);
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x5D), 1, 1, pE->p0AB508);
    pScr->cCtl++;

    /* 0x10056BAC -- straight on the screen's own fY, no row offset.
     * GOTCHA: f1E20C is 0x34 and f34's third argument is 4. */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY, 0x100009, 2, 5, 1, -1);
    pCtl->w1E20C = 0x34;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x5E), 1, 4, pE->p0AB448);
    pScr->cCtl++;

    /* 0x10056C46 -- absolute (156.0f == 0x431C0000, 172.0f == 0x432C0000),
     * flags 9, a6 = 0, a7 = 0x39.  No label, no hooks, no cSel. */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 156.0f, 172.0f, 9, 2, 5, 0, 0x39);
    pScr->cCtl++;

    /* 0x10056CB6 -- the one control in the packet that drives the item block
     * at +0x2B5C.  y is 174.0f == 0x432E0000; flags 0x200001. */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, 174.0f, 0x200001, 2, 5, 1, -1);
    pCtl->pfn08  = pH->p10042AC0;
    pCtl->pfn04  = pH->p1003EF90;
    pCtl->pfn10  = pH->p1003F020;       /* +0x10 -- only this builder */
    pCtl->w1E20C = 3;
    /* The text is the ADDRESS 0x1039B720, not a string-table id. */
    pCtl->pVtbl->f34(pCtl, pE->p39B720, 1, 1, pE->p0AB448);

    /* 0x10056D55 -- strlen + rep movs from 0x10A9CDF0 into the item's text,
     * then the item's own vtable +0x04.
     * DEVIATION (memory safety): bounded to the item's room; the original's
     * rep movs is not. */
    {
        size_t cb = strlen(pE->szA9CDF0) + 1u;
        if (cb > sizeof(pCtl->aText[0].sz)) {
            cb = sizeof(pCtl->aText[0].sz);
        }
        memcpy(pCtl->aText[0].sz, pE->szA9CDF0, cb);
        pCtl->aText[0].sz[sizeof(pCtl->aText[0].sz) - 1u] = '\0';
    }
    if (pCtl->aText[0].pVtbl != NULL) {
        pCtl->aText[0].pVtbl->pfn04(&pCtl->aText[0]);
    }
    /* Two DIFFERENT original addresses on each line -- the control's own
     * rectangle at +0x50..+0x5C and the text box's at +0x2F80..+0x2F8C. Under
     * br_ui.h the second is aText[0].left / f428 / right / f430 (ADJ-2), so
     * the box's fields are reached through the box and not mirrored as
     * control fields, which is the aliasing bug slice6_73.h already fixed. */
    pCtl->rcLeft         = 0x9B;
    pCtl->aText[0].left  = 0x9B;
    pCtl->rcRight        = 0x15B;
    pCtl->aText[0].right = 0x15B;
    pCtl->rcTop          = 0xAC;
    pCtl->aText[0].f428  = 0xAC;
    pCtl->rcBottom       = 0xBC;
    pCtl->aText[0].f430  = 0xBC;
    /* GOTCHA: the width is computed 16-BIT WIDE from two dwords -- the
     * original's `mov ax,[..2F88] / sub ax,[..2F80] / sub eax,0x10`, whose
     * upper half is then dropped by the word store.  +0x2F78 is item +0x41C,
     * which slice3_39.h types int16_t; the value is the same 16 bits. */
    pCtl->aText[0].f41C = (int16_t)(uint16_t)((uint16_t)pCtl->aText[0].right
                                              - (uint16_t)pCtl->aText[0].left
                                              - 0x10u);
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x10056D91 -- fY - (-95), 0x1008F690.
     * GOTCHA: flags are 0x102011, not the family's usual 0x102001, and
     * f1E20C is 2 with f34's third argument 0. */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY - (-95.0f),
                     0x102011, 2, 5, 1, -1);
    pCtl->pfn0C  = pH->p10047360;
    pCtl->pfn08  = pH->p10043F50;
    pCtl->w1E20C = 2;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x1E), 1, 0, pE->p0AB448);
    pE->pAA29E8 = pCtl;                 /* 0x10AA29E8 */
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x10056EA9 -- fY - (-114), 0x1008F694. */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY - (-114.0f),
                     0x102001, 2, 5, 1, -1);
    pCtl->pfn0C  = pH->p10047360;
    pCtl->pfn08  = pH->p10044B40;
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x0C), 1, 1, pE->p0AB448);
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x10056F61 -- absolute (80.0f, 46.0f), a7 = 7.  Placed only. */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 80.0f, 46.0f, 9, 2, 5, 0, 7);
    pScr->cCtl++;
}

/* ==========================================================================
 * 0x10057C10 -- BrOptFn10057C10.  Twelve or sixteen controls.
 * ==========================================================================
 *
 * GOTCHA: four whole controls are skipped unless 0x1022AF18 == 2, and it is
 * an EQUALITY test, not "non-zero".
 */
/* WHAT IT DOES: builds the multiplayer game-setup screen -- the lobby everyone
 * waits in before a network race. Only the host gets the four rows that decide
 * the race: track, weather, laps and car group; a joining player sees the
 * screen without them. The two action rows change their wording to match who
 * you are, reading "Create Game" and "Quit to Lobby" for the host and
 * "Continue" and "Back" otherwise. Alongside the rows it places the track and
 * weather thumbnails, the lap count, a car picture and the chat readout. */
/* port-only body; Glide match is src/core/cpp/0x10050AC0.cpp */
void BrOptFn10057C10(BrPhase_ *pPhase)
{
    Br72Env           *pE = g_pBr72Env;
    const BrUi72Hooks *pH = pE->pHooks;
    BrUiPage_         *pScr;
    BrUiCtl_          *pCtl;

    /* 0x10057C29..0x10057CB5 -- fX = 195.0f, fY = 130.0f. */
    pScr = Br72ScreenNew(pPhase, 195.0f, 130.0f);
    if (pScr == NULL) {
        return;
    }

    /* 0x10057CB5 -- the root */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 0.0f, 0.0f, 9, 2, 5, 0, 0);
    pScr->cCtl++;

    /* 0x10057D14 -- the title */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, 10.0f, 0x100009, 2, 5, 1, -1);
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x64), 1, 1, pE->p0AB508);
    pScr->cCtl++;

    if (pE->n22AF18 == 2) {
        /* 0x10057DBB -- row 0, on the screen's own fY. */
        BR_NEW_CTL();
        pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY,
                         0x102001, 2, 5, 1, -1);
        pCtl->pfn0C  = pH->p100474B0;   /* the one row that is not 0x10047360 */
        pCtl->pfn08  = pH->p10042EE0;
        pCtl->w1E20C = 3;
        pCtl->pVtbl->f34(pCtl, BrStrGet(0x1B), 1, 1, pE->p0AB448);
        pScr->cCtl++;
        pScr->cSel++;

        /* 0x10057E6B -- fY - (-19) */
        BR_NEW_CTL();
        pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY - (-19.0f),
                         0x102001, 2, 5, 1, -1);
        pCtl->pfn0C  = pH->p10047360;
        pCtl->pfn08  = pH->p10043180;
        pCtl->w1E20C = 3;
        pCtl->pVtbl->f34(pCtl, BrStrGet(0x1C), 1, 1, pE->p0AB448);
        pScr->cCtl++;
        pScr->cSel++;

        /* 0x10057F24 -- fY - (-38) */
        BR_NEW_CTL();
        pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY - (-38.0f),
                         0x102001, 2, 5, 1, -1);
        pCtl->pfn0C  = pH->p10047360;
        pCtl->pfn08  = pH->p100430B0;
        pCtl->w1E20C = 3;
        pCtl->pVtbl->f34(pCtl, BrStrGet(0x1D), 1, 1, pE->p0AB448);
        pScr->cCtl++;
        pScr->cSel++;

        /* 0x10057FDD -- fY - (-57) */
        BR_NEW_CTL();
        pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY - (-57.0f),
                         0x102001, 2, 5, 1, -1);
        pCtl->pfn0C  = pH->p10047360;
        pCtl->pfn08  = pH->p10044600;
        pCtl->w1E20C = 3;
        pCtl->pVtbl->f34(pCtl, BrStrGet(0x65), 1, 1, pE->p0AB448);
        pScr->cCtl++;
        pScr->cSel++;
    }

    /* 0x10058096 -- fY - (-95).  The row constants therefore run
     * -19, -38, -57, -95 with -76 skipped, and the first three vanish
     * entirely when 0x1022AF18 is not 2. */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY - (-95.0f),
                     0x102001, 2, 5, 1, -1);
    pCtl->pfn0C  = pH->p10047360;
    pCtl->pfn08  = pH->p100446D0;
    pCtl->pfn18  = pH->p100437D0;       /* +0x18 -- only this control */
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(pE->nAA2884 != 0 ? 0x66 : 0x1E),
                     1, 1, pE->p0AB448);
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x10058163 -- fY - (-114).  Same id-swap trick on a different global. */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY - (-114.0f),
                     0x102001, 2, 5, 1, -1);
    pCtl->pfn0C  = pH->p10047360;
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(pE->nA9D000 != 0 ? 0x67 : 0x0C),
                     1, 1, pE->p0AB448);
    pE->pAA29B8 = pCtl;                 /* 0x10AA29B8 */
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x10058228 -- (73.0f, 212.0f), flags 1.  f2AB4/f2AB6 instead of a
     * label; f2AB6 gets cCtl + 1, read BEFORE the increment. */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 73.0f, 212.0f, 1, 2, 5, 1, 0x16);
    pCtl->pfn04 = pH->p100407E0;
    pCtl->cChild++;
    pCtl->aChild[0] = (uint16_t)(pScr->cCtl + 1);
    pScr->cCtl++;

    /* 0x100582B4 -- (fX, 275.0f).  Text by ADDRESS. */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, 275.0f, 0x101001, 2, 5, 1, -1);
    pCtl->pfn04  = pH->p1003FE80;
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, pE->p0AD300, 1, 1, pE->p0AB4B8);
    pScr->cCtl++;

    /* 0x100583D6 -- (325.0f, 72.0f) */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 325.0f, 72.0f, 1, 2, 5, 1, 0x11);
    pCtl->pfn04 = pH->p10040730;
    pCtl->cChild++;
    pCtl->aChild[0] = (uint16_t)(pScr->cCtl + 1);
    pScr->cCtl++;

    /* 0x1005846C -- (fX, 152.0f) */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, 152.0f, 0x101001, 2, 5, 1, -1);
    pCtl->pfn04  = pH->p1003FA00;
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, pE->p0AD300, 1, 1, pE->p0AB4A8);
    pScr->cCtl++;

    /* 0x10058504 -- (106.0f, 68.0f), flags 0x5001, f1E20C 5, a3 = 3, and the
     * text block is 0x100AD274 rather than 0x100AD300. */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 106.0f, 68.0f, 0x5001, 2, 5, 1, -1);
    pCtl->pfn04  = pH->p100408D0;
    pCtl->w1E20C = 5;
    pCtl->pVtbl->f34(pCtl, pE->p0AD274, 1, 3, pE->p0AB458);
    pScr->cCtl++;

    /* 0x1005859B -- (106.0f, 115.0f) */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 106.0f, 115.0f, 0x100001, 2, 5, 1, -1);
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x1D), 1, 1, pE->p0AB458);
    pScr->cCtl++;

    /* 0x10058612 -- (437.0f, 141.0f), a7 = 0x56.  Placed, hooked, no label. */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 437.0f, 141.0f, 1, 2, 5, 1, 0x56);
    pCtl->pfn04 = pH->p1003F5E0;
    pScr->cCtl++;

    /* 0x1005866C -- (476.0f, 224.0f).  The last control. */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 476.0f, 224.0f, 1, 2, 5, 1, -1);
    pCtl->pfn04 = pH->p1003F680;
    pScr->cCtl++;
}

/* ==========================================================================
 * 0x10052030 -- BrExt_10052030.  Twenty-three controls.
 * ========================================================================== */
/* WHAT IT DOES: builds the season-progress screen the player sees between
 * championship rounds. It carries the heading, a Reset Round row, Continue and
 * Back, the standings readouts, and down the right-hand side three picture
 * buttons -- save, main menu and options -- each of which has a second
 * "pressed" picture it swaps to. */
/* @implements 0x10052030 d3d BrExt_10052030 */
void BrExt_10052030(BrPhase_ *pPhase)
{
    Br72Env           *pE = g_pBr72Env;
    const BrUi72Hooks *pH = pE->pHooks;
    BrUiPage_         *pScr;
    BrUiCtl_          *pCtl;
    float              fA;          /* [esp+0x10] -- rect x                  */
    float              fB;          /* [esp+0x2C] -- rect y cursor           */
    int32_t            iA     = 0;  /* ebx, live across the three rects      */
    int32_t            iB;
    int32_t            iRight = 0;  /* [esp+0x14], live across the three     */

    pScr = Br72ScreenNew(pPhase, 195.0f, 130.0f);
    if (pScr == NULL) {
        return;
    }

    /* 0x100520B6 -- the root */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 0.0f, 0.0f, 9, 2, 5, 0, 0);
    pScr->cCtl++;

    /* 0x10052137 -- the title */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, 10.0f, 0x100009, 2, 5, 1, -1);
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x44), 1, 1, pE->p0AB508);
    pScr->cCtl++;

    /* 0x100521D0 -- fY - (-19) */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY - (-19.0f),
                     0x102001, 2, 5, 1, -1);
    pCtl->pfn0C  = pH->p10047360;
    pCtl->pfn08  = pH->p10047340;
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x45), 1, 1, pE->p0AB448);
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x10052289 -- fY - (-95).
     * GOTCHA: -38, -57 and -76 are all skipped.
     * GOTCHA: f1E20C is 2 and f34's third argument is 0 -- the only such
     * pair in this function. */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY - (-95.0f),
                     0x102001, 2, 5, 1, -1);
    pCtl->pfn0C  = pH->p10047360;
    pCtl->pfn08  = pH->p10045050;
    pCtl->w1E20C = 2;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x1E), 1, 0, pE->p0AB448);
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x10052342 -- fY - (-114) */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY - (-114.0f),
                     0x102001, 2, 5, 1, -1);
    pCtl->pfn0C  = pH->p10047360;
    pCtl->pfn08  = pH->p10047060;
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x0C), 1, 1, pE->p0AB448);
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x100523FB.  Both globals are read with `fild`, i.e. as INTEGERS. */
    fA = (float)pE->nAB428;
    fB = (float)pE->nAB42C;

    /* 0x10052401 -- rect 1.  GOTCHA: none of the three rects bumps cSel. */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, fA, fB, 0x402001, 2, 5, 1, 0x78);
    pCtl->pfn0C = pH->p10047360;
    pCtl->pfn08 = pH->p100457E0;
    iB          = BrFtolTrunc(fB);
    pCtl->rcTop   = iB;
    iA          = BrFtolTrunc(fA);
    fB          = fB - (-33.0f);        /* 0x1008F69C, advanced in FLOAT ... */
    iRight      = iA + 0x7F;
    pCtl->rcLeft   = iA;
    pCtl->rcRight   = iRight;
    pCtl->rcBottom   = iB + 0x21;            /* ... while the rect uses +0x21 int */
    pCtl->f2968 = 0;
    pCtl->aStepId[1] = 0x79;
    pScr->cCtl++;

    /* 0x100524E1 -- rect 2.
     * GOTCHA: reuses iA and iRight instead of re-truncating fA, so it shares
     * both vertical edges with rect 1. */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, fA, fB, 0x402001, 2, 5, 1, 0x52);
    pCtl->pfn0C = pH->p10047360;
    pCtl->pfn08 = pH->p10043FA0;
    iB          = BrFtolTrunc(fB);
    fB          = fB - (-33.0f);
    pCtl->rcTop   = iB;
    pCtl->rcLeft   = iA;
    pCtl->rcRight   = iRight;
    pCtl->rcBottom   = iB + 0x21;
    pCtl->f2968 = 0;
    pCtl->aStepId[1] = 0x53;
    pScr->cCtl++;

    /* 0x100525A5 -- rect 3.
     * GOTCHA: reuses iA/iRight, and it is the one rect that does NOT advance
     * the y cursor afterwards -- there is no fsub/fstp pair.  Unobservable
     * here only because nothing after it reads fB. */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, fA, fB, 0x402001, 2, 5, 1, 0x54);
    pCtl->pfn0C = pH->p10047360;
    pCtl->pfn08 = pH->p100457C0;
    iB          = BrFtolTrunc(fB);
    pCtl->rcTop   = iB;
    pCtl->rcLeft   = iA;
    pCtl->rcRight   = iRight;
    pCtl->rcBottom   = iB + 0x21;
    pCtl->f2968 = 0;
    pCtl->aStepId[1] = 0x55;
    pScr->cCtl++;

    /* 0x10052657 -- (73.0f, 212.0f), flags 1, f2AB4/f2AB6 pair. */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 73.0f, 212.0f, 1, 2, 5, 1, 0x16);
    pCtl->pfn04 = pH->p100407E0;
    pCtl->cChild++;
    pCtl->aChild[0] = (uint16_t)(pScr->cCtl + 1);
    pScr->cCtl++;

    /* 0x100526E3 -- (fX, 275.0f).  Text by ADDRESS. */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, 275.0f, 0x101001, 2, 5, 1, -1);
    pCtl->pfn04  = pH->p1003FE80;
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, pE->p0AD300, 1, 1, pE->p0AB4B8);
    pScr->cCtl++;

    /* 0x10052779 -- (325.0f, 72.0f) */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 325.0f, 72.0f, 1, 2, 5, 1, 0x11);
    pCtl->pfn04 = pH->p10040730;
    pCtl->cChild++;
    pCtl->aChild[0] = (uint16_t)(pScr->cCtl + 1);
    pScr->cCtl++;

    /* 0x10052805 -- (fX, 152.0f) */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, 152.0f, 0x101001, 2, 5, 1, -1);
    pCtl->pfn04  = pH->p1003FA00;
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, pE->p0AD300, 1, 1, pE->p0AB4A8);
    pScr->cCtl++;

    /* 0x1005289B -- (450.0f, 125.0f) */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 450.0f, 125.0f, 0x100009, 2, 5, 1, -1);
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x40), 1, 1, pE->p0AB4F8);
    pScr->cCtl++;

    /* 0x10052932 -- (450.0f, 185.0f) */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 450.0f, 185.0f, 0x100009, 2, 5, 1, -1);
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x46), 1, 1, pE->p0AB4F8);
    pScr->cCtl++;

    /* 0x100529C9 -- (450.0f, 141.0f).  Text is the writable buffer
     * 0x1039B720, a3 = 3, f1E20C = 5. */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 450.0f, 141.0f, 0x5001, 2, 5, 1, -1);
    pCtl->pfn04  = pH->p100415A0;
    pCtl->w1E20C = 5;
    pCtl->pVtbl->f34(pCtl, pE->p39B720, 1, 3, pE->p0AB4F8);
    pScr->cCtl++;

    /* 0x10052A61 -- (fX, 203.0f) */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, 203.0f, 0x100009, 2, 5, 1, -1);
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x40), 1, 1, pE->p0AB478);
    pScr->cCtl++;

    /* 0x10052AFA -- (fX, 265.0f) */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, 265.0f, 0x100009, 2, 5, 1, -1);
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x41), 1, 1, pE->p0AB478);
    pScr->cCtl++;

    /* 0x10052B93 -- (450.0f, 217.0f) */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 450.0f, 217.0f, 0x5001, 2, 5, 1, -1);
    pCtl->pfn04  = pH->p100414B0;
    pCtl->w1E20C = 5;
    pCtl->pVtbl->f34(pCtl, pE->p39B720, 1, 3, pE->p0AB478);
    pScr->cCtl++;

    /* 0x10052C2B -- (106.0f, 85.0f) */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 106.0f, 85.0f, 0x100001, 2, 5, 1, -1);
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x38), 1, 1, pE->p0AB458);
    pScr->cCtl++;

    /* 0x10052CC2 -- (440.0f, 66.0f).  a3 = 4 and f1E20C = 0x34. */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 440.0f, 66.0f, 0x5001, 2, 5, 1, -1);
    pCtl->pfn04  = pH->p10041300;
    pCtl->w1E20C = 0x34;
    pCtl->pVtbl->f34(pCtl, pE->p39B720, 1, 4, pE->p0AB458);
    pScr->cCtl++;

    /* 0x10052D5A -- (106.0f, 123.0f) */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 106.0f, 123.0f, 0x100001, 2, 5, 1, -1);
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x36), 1, 1, pE->p0AB458);
    pScr->cCtl++;

    /* 0x10052DF1 -- (440.0f, 104.0f) */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 440.0f, 104.0f, 0x5001, 2, 5, 1, -1);
    pCtl->pfn04  = pH->p100413B0;
    pCtl->w1E20C = 0x34;
    pCtl->pVtbl->f34(pCtl, pE->p39B720, 1, 4, pE->p0AB458);
    pScr->cCtl++;

    /* 0x10052E89 -- the last control.
     * GOTCHA: the subtrahend is 0x1008F6A0 == +19.0f, NOT one of the negative
     * row constants, so this is a genuine SUBTRACTION -- the only row step in
     * the family that moves a control UP. */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY - 19.0f,
                     0x5001, 2, 5, 1, -1);
    pCtl->pfn04  = pH->p10040B30;
    pCtl->w1E20C = 0x34;
    pCtl->pVtbl->f34(pCtl, pE->p39B720, 1, 4, pE->p0AB448);
    pScr->cCtl++;
}

/* ==========================================================================
 * 0x10059760 -- BrExt_10059760.  Six controls.
 * ========================================================================== */
/* WHAT IT DOES: builds the time-attack menu -- the time-attack banner, then
 * New Race, Load Race and Back. The gap between Load Race and Back is five rows
 * wide, so the Back row sits well clear of the other two. */
/* port-only body; Glide match is src/core/cpp/0x10052610.cpp */
void BrExt_10059760(BrPhase_ *pPhase)
{
    Br72Env           *pE = g_pBr72Env;
    const BrUi72Hooks *pH = pE->pHooks;
    BrUiPage_         *pScr;
    BrUiCtl_          *pCtl;

    pScr = Br72ScreenNew(pPhase, 195.0f, 130.0f);
    if (pScr == NULL) {
        return;
    }

    /* 0x100597E3 -- the root */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 0.0f, 0.0f, 9, 2, 5, 0, 0);
    pScr->cCtl++;

    /* 0x10059863 -- the title */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, 10.0f, 0x100009, 2, 5, 1, -1);
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x6A), 1, 1, pE->p0AB508);
    pScr->cCtl++;

    /* 0x100598FB -- row 0, on the screen's own fY (no row offset). */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY, 0x102001, 2, 5, 1, -1);
    pCtl->pfn0C  = pH->p10047360;
    pCtl->pfn08  = pH->p10046260;
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x6B), 1, 1, pE->p0AB448);
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x100599AA -- fY - (-19) */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY - (-19.0f),
                     0x102001, 2, 5, 1, -1);
    pCtl->pfn0C  = pH->p10047360;
    pCtl->pfn08  = pH->p10044D00;
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x6C), 1, 1, pE->p0AB448);
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x10059A62 -- fY - (-114).
     * GOTCHA: the row cursor jumps from -19 straight to -114; four constants
     * are skipped, so this row sits a five-row gap below its predecessor. */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY - (-114.0f),
                     0x102001, 2, 5, 1, -1);
    pCtl->pfn0C  = pH->p10047360;
    pCtl->pfn08  = pH->p10044C70;
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x0C), 1, 1, pE->p0AB448);
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x10059B1A -- absolute (80.0f, 46.0f), flags 9, a6 = 0, a7 = 8.
     * No hooks, no label, no f1E20C, no cSel. */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 80.0f, 46.0f, 9, 2, 5, 0, 8);
    pScr->cCtl++;
}

/* ==========================================================================
 * 0x1005A6E0 -- BrExt_1005A6E0.  TWO screens; ten controls then one.
 * ========================================================================== */
void BrExt_1005A6E0(BrPhase_ *pPhase)
{
    Br72Env           *pE = g_pBr72Env;
    const BrUi72Hooks *pH = pE->pHooks;
    BrUiPage_         *pScr;
    BrUiCtl_          *pCtl;
    BrTextList        *pSub;
    BrGrfList         *pList;
    int32_t            iOff;
    uint16_t           iPageSlot;

    /* ------------------------------------------------------------------
     * 0x1005A6EE -- the extra prologue.  The receiver is the GLOBAL phase
     * at 0x10AA2908, NOT the argument.
     * ------------------------------------------------------------------ */
    pList = (BrGrfList *)pE->pAA2908->fC4;
    pList->pVtbl->f04(pList, pE->pszTimeAttackMask);   /* "TimeAttack*.GRF" */

    /* 0x1005A719.  The original's order is iPage=0, 0AB3F4=-1, AA28E8=0,
     * aFlags[i]=1, operator new.  Br72ScreenNew folds three of those into one
     * call; all four stores are independent so the visible order is the
     * same. */
    pE->n0AB3F4 = -1;
    pE->nAA28E8 = 0;

    pScr = Br72ScreenNew(pPhase, 195.0f, 130.0f);
    if (pScr == NULL) {
        return;
    }

    /* 0x1005A782 -- the root */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 0.0f, 0.0f, 9, 2, 5, 0, 0);
    pScr->cCtl++;

    /* 0x1005A807 -- the title */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, 10.0f, 0x100009, 2, 5, 1, -1);
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0xC2), 1, 1, pE->p0AB508);
    pScr->cCtl++;

    /* 0x1005A8A3 -- the ghost/replay list.  The one control in the packet
     * that drives the sub-object at +0x3838. */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY, 0x3001, 2, 5, 1, -1);
    pSub = &pCtl->list;                /* kept live past pCtl -- see below */
    pCtl->pfn04  = pH->p1003EC30;
    pCtl->list.f1A99C[8].i = 1;      /* control +0x1E1F4 (ADJ-6) */
    pSub->pVtbl->f14(pSub, 0x40001, pE->p0AB4D8, 5, 0, -1);
    pSub->f04 = pH->p10042740;        /* control +0x383C */
    pSub->f14 = pH->p10042560;        /* control +0x384C */

    /* 0x1005A95E -- feed the 100 enumerated names into the list.
     *
     * GOTCHA (register aliasing): the original reuses the register that held
     * the control as the byte cursor, so the control pointer is dead from
     * here on; everything below reaches the object through pSub.
     *
     * GOTCHA: the guard tests the computed ADDRESS `fC4 + i + 4`, not the
     * string, and that is non-zero for any base.  All 100 slots are appended,
     * empty ones included.  Preserved -- almost certainly a dropped `[0]` in
     * the original source. */
    for (iOff = 0; iOff < (int32_t)(BR72_GRF_COUNT * BR72_GRF_STRIDE);
         iOff += BR72_GRF_STRIDE) {
        const char *pszName;

        pList   = (BrGrfList *)pE->pAA2908->fC4;   /* re-read every pass */
        pszName = (const char *)pList->aName[0] + iOff;
        if (pszName != NULL) {
            pSub->pVtbl->f10(pSub, pszName, 0, 1, pE->p0AB4D8, 0);
        }
    }
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x1005A9A0 -- fY - (-95).  The only row constant this builder uses. */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY - (-95.0f),
                     0x102001, 2, 5, 1, -1);
    pCtl->pfn0C  = pH->p10047360;
    pCtl->pfn08  = pH->p10044F00;
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x1E), 1, 1, pE->p0AB448);
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x1005AA59 -- the right-hand readout column: x = 440.0f throughout,
     * y walking 208 / 224 / 240 / 256 as absolute literals. */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 440.0f, 208.0f, 0x101001, 2, 5, 1, -1);
    pCtl->pfn04  = pH->p10041040;
    pCtl->w1E20C = 0x34;
    pCtl->pVtbl->f34(pCtl, pE->p0AD300, 1, 4, pE->p0AB478);
    pScr->cCtl++;

    /* 0x1005AAF1 */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 440.0f, 224.0f, 0x100001, 2, 5, 1, -1);
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x6F), 1, 1, pE->p0AB478);
    pScr->cCtl++;

    /* 0x1005AB88 */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 440.0f, 240.0f, 0x101001, 2, 5, 1, -1);
    pCtl->pfn04  = pH->p10041180;
    pCtl->w1E20C = 0x34;
    pCtl->pVtbl->f34(pCtl, pE->p0AD300, 1, 4, pE->p0AB478);
    pScr->cCtl++;

    /* 0x1005AC20 */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 440.0f, 256.0f, 0x100001, 2, 5, 1, -1);
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x3F), 1, 1, pE->p0AB478);
    pScr->cCtl++;

    /* 0x1005ACB7 -- absolute (80.0f, 46.0f), a7 = 6.  No label. */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 80.0f, 46.0f, 9, 2, 5, 0, 6);
    pScr->cCtl++;

    /* ------------------------------------------------------------------
     * 0x1005AD2A -- SCREEN 2.
     *
     * GOTCHA: this prologue writes aFlags[nPages] = ZERO where every other
     * screen in the game writes 1, and it does not re-zero iPage (nothing has
     * moved it since the top of the function, so the net state is the same).
     * The store happens before `operator new` in the original; writing it
     * immediately after the helper -- and before the NULL check -- reproduces
     * the original's state on both paths.
     * ------------------------------------------------------------------ */
    iPageSlot = pPhase->nPages;
    pScr = Br72ScreenNew(pPhase, 195.0f, 130.0f);
    if (iPageSlot < BR_PHASE_PAGES) {
        pPhase->aFlags[iPageSlot] = 0;
    }
    if (pScr == NULL) {
        return;
    }

    /* 0x1005AD89 -- screen 2's only control.
     * GOTCHA: x is the literal 0.0f, not pScr->fX, even though fX was just
     * set to 195.0f and is never read. */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 0.0f, 232.0f, 0x100009, 2, 5, 1, -1);
    pCtl->pfn0C  = pH->p10047360;
    pCtl->pfn04  = pH->p10047250;
    pCtl->pfn14  = pH->p1003E7A0;       /* +0x14 -- only this control */
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x70), 1, 1, pE->p0AB438);
    pE->pAA29C4 = pCtl;                 /* 0x10AA29C4 */
    pScr->cCtl++;
}

/* ==========================================================================
 * 0x1004E830 -- BrExt_1004E830.  Sixteen controls.
 * ========================================================================== */
/* WHAT IT DOES: builds the game-options screen: force feedback, skid marks,
 * specular lighting and car shadow, each with a little picture showing what the
 * setting looks like, and a Back row. It re-checks for a force-feedback device
 * on the spot, right after placing the heading, and greys the force-feedback
 * row out if there is not one. */
/* port-only body; Glide match is src/core/cpp/0x100476E0.cpp */
void BrExt_1004E830(BrPhase_ *pPhase)
{
    Br72Env           *pE = g_pBr72Env;
    const BrUi72Hooks *pH = pE->pHooks;
    BrUiPage_         *pScr;
    BrUiCtl_          *pCtl;
    int32_t            nFfb;

    pScr = Br72ScreenNew(pPhase, 195.0f, 130.0f);
    if (pScr == NULL) {
        return;
    }

    /* 0x1004E8D5 -- the root */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 0.0f, 0.0f, 9, 2, 5, 0, 0);
    pScr->cCtl++;

    /* 0x1004E934 -- the title.
     * GOTCHA: after the cCtl++ this block makes a bare no-argument call to
     * 0x100795D0, which re-probes the force-feedback subsystem.  Nothing
     * else in the family calls anything like it. */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, 10.0f, 0x100009, 2, 5, 1, -1);
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x22), 1, 1, pE->p0AB508);
    pScr->cCtl++;
    pE->pfn100795D0();

    /* 0x1004E9D2 -- row 0, on the screen's own fY.  The only conditional
     * control in the function.
     * GOTCHA: 0x118ABDBC is read TWICE, once before f38 for the flags and
     * once after it for the label.  Both reads are kept.
     * GOTCHA: the flags come from `neg/sbb/and 0xFFFFFFF0/add 0x102011`,
     * i.e. 0x102001 when set and 0x102011 when clear. */
    BR_NEW_CTL();
    nFfb = pE->n18ABDBC;
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY,
                     (nFfb != 0) ? 0x102001 : 0x102011, 2, 5, 1, -1);
    pCtl->pfn0C = pH->p10047360;
    pCtl->pfn08 = pH->p10043590;
    nFfb = pE->n18ABDBC;                /* re-read at 0x1004EA64 */
    if (nFfb != 0) {
        pCtl->w1E20C = 3;
        pCtl->pVtbl->f34(pCtl, BrStrGet(0x30), 1, 1, pE->p0AB448);
    } else {
        pCtl->w1E20C = 2;
        pCtl->pVtbl->f34(pCtl, BrStrGet(0x30), 1, 0, pE->p0AB448);
    }
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x1004EAAC -- fY - (-19) */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY - (-19.0f),
                     0x102001, 2, 5, 1, -1);
    pCtl->pfn0C  = pH->p10047360;
    pCtl->pfn08  = pH->p100435F0;
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x31), 1, 1, pE->p0AB448);
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x1004EB65 -- fY - (-38) */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY - (-38.0f),
                     0x102001, 2, 5, 1, -1);
    pCtl->pfn0C  = pH->p10047360;
    pCtl->pfn08  = pH->p100436B0;
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x32), 1, 1, pE->p0AB448);
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x1004EC1E -- fY - (-57) */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY - (-57.0f),
                     0x102001, 2, 5, 1, -1);
    pCtl->pfn0C  = pH->p10047360;
    pCtl->pfn08  = pH->p10043650;
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x33), 1, 1, pE->p0AB448);
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x1004ECD7 -- fY - (-114).
     * GOTCHA: -76 and -95 are BOTH skipped, so this row sits three rows below
     * its predecessor.
     * GOTCHA: the only control here published to a global, and the store is
     * after f34 and before cCtl++. */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY - (-114.0f),
                     0x102001, 2, 5, 1, -1);
    pCtl->pfn0C  = pH->p10047360;
    pCtl->pfn08  = pH->p10046710;
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x0C), 1, 1, pE->p0AB448);
    pE->pAA29C8 = pCtl;                 /* 0x10AA29C8 */
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x1004ED96 -- absolute (80.0f, 46.0f), flags 9, a6 = 0, a7 = 9. */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 80.0f, 46.0f, 9, 2, 5, 0, 9);
    pScr->cCtl++;

    /* 0x1004EE07 -- (336.0f, 48.0f).  The first of four widget controls. */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 336.0f, 48.0f, 1, 2, 5, 1, 0x61);
    pCtl->pfn04 = pH->p10040950;
    pCtl->cChild++;
    pCtl->aChild[0] = (uint16_t)(pScr->cCtl + 1);
    pScr->cCtl++;

    /* 0x1004EE90 -- (fX, 166.0f).  Text by ADDRESS.
     * GOTCHA: the four labels' y values are not monotonic -- 166, 279, 196,
     * 273 -- so they interleave with the widgets rather than stacking. */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, 166.0f, 0x101001, 2, 5, 1, -1);
    pCtl->pfn04  = pH->p1003FCB0;
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, pE->p0AD300, 1, 1, pE->p0AB4C8);
    pScr->cCtl++;

    /* 0x1004EF26 -- (67.0f, 199.0f) */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 67.0f, 199.0f, 1, 2, 5, 1, 0x8C);
    pCtl->pfn04 = pH->p10040990;
    pCtl->cChild++;
    pCtl->aChild[0] = (uint16_t)(pScr->cCtl + 1);
    pScr->cCtl++;

    /* 0x1004EFB5 -- (fX, 279.0f) */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, 279.0f, 0x101001, 2, 5, 1, -1);
    pCtl->pfn04  = pH->p1003FD30;
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, pE->p0AD300, 1, 1, pE->p0AB4B8);
    pScr->cCtl++;

    /* 0x1004F04B -- (440.0f, 114.0f) */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 440.0f, 114.0f, 1, 2, 5, 1, 0x65);
    pCtl->pfn04 = pH->p100409D0;
    pCtl->cChild++;
    pCtl->aChild[0] = (uint16_t)(pScr->cCtl + 1);
    pScr->cCtl++;

    /* 0x1004F0D7 -- (fX, 196.0f) */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, 196.0f, 0x101001, 2, 5, 1, -1);
    pCtl->pfn04  = pH->p1003FE10;
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, pE->p0AD300, 1, 1, pE->p0AB488);
    pScr->cCtl++;

    /* 0x1004F16D -- (484.0f, 214.0f) */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 484.0f, 214.0f, 1, 2, 5, 1, 0x63);
    pCtl->pfn04 = pH->p100409B0;
    pCtl->cChild++;
    pCtl->aChild[0] = (uint16_t)(pScr->cCtl + 1);
    pScr->cCtl++;

    /* 0x1004F1F9 -- (fX, 273.0f).  The last control. */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, 273.0f, 0x101001, 2, 5, 1, -1);
    pCtl->pfn04  = pH->p1003FDA0;
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, pE->p0AD300, 1, 1, pE->p0AB478);
    pScr->cCtl++;
}

#ifdef BR_MATCHING_BUILD
/* 0x10074AE6 FUN_10074ae6 now lives in src/core/startup/br_stubs.c. */


#endif /* BR_MATCHING_BUILD */
