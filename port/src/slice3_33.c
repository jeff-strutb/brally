/* slice3_33.c -- BRD3D.dll 0x1004A580-0x1004D1E9, agent 33. See slice3_33.h.
 *
 * Five menu-screen builders. Each is a transcription; the interesting content
 * is the coordinates, ids and the places the pattern breaks, all of which are
 * listed in the header's GOTCHAS block.
 *
 * Float literals below are the exact values of the 32-bit patterns the
 * original pushes (10.0f == 0x41200000, 195.0f == 0x43430000, ...); the row
 * offsets -19/-38/-57/-76/-95/-114/-133/-33 are the .rdata constants at
 * 0x1008F680..0x1008F69C read out of the image with tools/pe.py.
 */

#include "slice3_33.h"

/* ==========================================================================
 * The two allocation steps, which are byte-identical everywhere they appear
 * ========================================================================== */

/* The screen prologue: 0x1004A595..0x1004A627 and its four twins.
 *
 * DEVIATION (memory safety, two places): the array writes are bounded. The
 * original has the same implicit bounds -- apScreen ends where aF6C begins
 * and aF6C ends at the object's 0xC8 bytes -- but does not check them. No
 * caller in this build gets anywhere near 22 screens.
 *
 * DEVIATION (memory safety): on allocation failure the original reports error
 * index 4 and then dereferences NULL. Index 4 is FATAL in g_aBrErrTable
 * (slice1_06.c), so BrErrShow does not return there in practice; the port
 * returns instead of faulting. */
static BrUiScreen *BrUiScreenNew(BrUiBuildCtx *pCtx, BrUiPhase *pPhase,
                                 float fY)
{
    BrUiScreen *pScr;
    uint16_t    i;

    i = pPhase->cScreen;
    pPhase->f12 = 0;
    if (i < BR_UI_PHASE_SCREEN_MAX) {
        pPhase->aF6C[i] = 1;
    }

    pScr = (BrUiScreen *)BrOperatorNew(BR_ALLOC_SIZE(BrUiScreen, BR_UI_SCREEN_ORIG_SIZE));
    pScr = (pScr != NULL) ? BrUiScreenCtor(pScr) : NULL;

    /* The original re-reads the counter here rather than reusing `i`. */
    i = pPhase->cScreen;
    if (i < BR_UI_PHASE_SCREEN_MAX) {
        pPhase->apScreen[i] = pScr;
    }
    if (pScr == NULL) {
        BrErrShow(pCtx->pErrHost, 4);
    }
    pPhase->cScreen++;

    if (pScr == NULL) {
        return NULL;            /* DEVIATION: see above */
    }

    pScr->pOwner = pPhase;
    pScr->f10    = 0;
    pScr->fX     = 195.0f;      /* 0x43430000 -- the same in all five */
    pScr->fY     = fY;
    return pScr;
}

/* The control prologue, ~60 occurrences. Same two DEVIATIONs as above. */
static BrUiCtl *BrUiCtlNew(BrUiBuildCtx *pCtx, BrUiScreen *pScr)
{
    BrUiCtl *pCtl;

    pCtl = (BrUiCtl *)BrOperatorNew(BR_ALLOC_SIZE(BrUiCtl, BR_UI_CTL_ORIG_SIZE));
    pCtl = (pCtl != NULL) ? BrUiCtlCtor(pCtl) : NULL;

    /* Stored BEFORE the null test, exactly as the original does. */
    if (pScr->cCtl < BR_UI_SCREEN_CTL_MAX) {
        pScr->apCtl[pScr->cCtl] = pCtl;
    }
    if (pCtl == NULL) {
        BrErrShow(pCtx->pErrHost, 4);
    }
    return pCtl;
}

/* Shorthand so the transcription below stays readable. Relies on the local
 * names pCtx / pScr / pCtl, which every function here declares. */
#define BR_NEW_CTL()                                    \
    do {                                                \
        pCtl = BrUiCtlNew(pCtx, pScr);                  \
        if (pCtl == NULL) { return; }                   \
    } while (0)

/* The unnamed first control of every screen: 0x1004A670 and its four twins.
 * Note the owner argument is the PHASE, not the screen, at every f38 site. */
static void BrUiCtlPlaceRoot(BrUiCtl *pCtl, BrUiPhase *pPhase)
{
    pCtl->pVtbl->f38(pCtl, pPhase, 0.0f, 0.0f, 9, 2, 5, 0, 0);
}

/* ==========================================================================
 * 0x1004A580
 * ========================================================================== */

void BrExt_1004A580(BrUiBuildCtx *pCtx, BrUiPhase *pPhase)
{
    const BrUiBuildHooks *pH = pCtx->pHooks;
    BrUiScreen *pScr;
    BrUiCtl    *pCtl;
    float       fA;         /* [esp+0x10] */
    float       fB;         /* [esp+0x2C] -- also the incoming argument slot */
    int32_t     iA  = 0;    /* ebx, live across two blocks                   */
    int32_t     iB;
    int32_t     iL14;       /* [esp+0x14] */
    int32_t     iL18 = 0;   /* [esp+0x18], live across two blocks            */

    pScr = BrUiScreenNew(pCtx, pPhase, 111.0f);   /* 0x42DE0000 */
    if (pScr == NULL) {
        return;
    }

    /* 0x1004A670 */
    BR_NEW_CTL();
    BrUiCtlPlaceRoot(pCtl, pPhase);
    pScr->cCtl++;

    /* 0x1004A6D8 -- the title */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, 10.0f, 0x100009, 2, 5, 1, -1);
    pCtl->f1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x13), 1, 1, pCtx->p0AB508);
    pScr->cCtl++;

    /* 0x1004A771 */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY, 0x102001, 2, 5, 1, -1);
    pCtl->pfn0C  = pH->p10047360;
    pCtl->pfn08  = pH->p10042B30;
    pCtl->f1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x14), 1, 1, pCtx->p0AB448);
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x1004A821 -- fY - (-19) */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY - (-19.0f),
                     0x102001, 2, 5, 1, -1);
    pCtl->pfn0C  = pH->p10047360;
    pCtl->pfn08  = pH->p10042DC0;
    pCtl->f1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x15), 1, 1, pCtx->p0AB448);
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x1004A8DA -- fY - (-38) */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY - (-38.0f),
                     0x102001, 2, 5, 1, -1);
    pCtl->pfn0C  = pH->p10047360;
    pCtl->pfn08  = pH->p10042E20;
    pCtl->f1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x16), 1, 1, pCtx->p0AB448);
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x1004A993 -- fY - (-57) */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY - (-57.0f),
                     0x102001, 2, 5, 1, -1);
    pCtl->pfn0C  = pH->p10047360;
    pCtl->pfn08  = pH->p10042C80;
    pCtl->f1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x17), 1, 1, pCtx->p0AB448);
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x1004AA4C -- fY - (-76) */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY - (-76.0f),
                     0x102001, 2, 5, 1, -1);
    pCtl->pfn0C  = pH->p10047360;
    pCtl->pfn08  = pH->p10042E80;
    pCtl->f1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x18), 1, 1, pCtx->p0AB448);
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x1004AB05 -- fY - (-114). GOTCHA: -95 (0x1008F690) is skipped. */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY - (-114.0f),
                     0x102001, 2, 5, 1, -1);
    pCtl->pfn0C  = pH->p10047360;
    pCtl->pfn08  = pH->p10043760;
    pCtl->f1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x19), 1, 1, pCtx->p0AB448);
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x1004ABBE -- fY - (-133) */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY - (-133.0f),
                     0x102001, 2, 5, 1, -1);
    pCtl->pfn0C  = pH->p10047360;
    pCtl->pfn08  = pH->p100464E0;
    pCtl->f1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x0C), 1, 1, pCtx->p0AB448);
    pCtx->pAA29B4 = pCtl;
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x1004AC2C. Both globals are read with `fild`, i.e. as INTEGERS. */
    fA = (float)pCtx->nAB428;
    fB = (float)pCtx->nAB42C;

    if (pCtx->n0AA010 == 0) {
        /* 0x1004AC9E -- the first of three rect controls */
        BR_NEW_CTL();
        pCtl->pVtbl->f38(pCtl, pPhase, fA, fB, 0x402001, 2, 5, 1, 0x78);
        pCtl->pfn0C = pH->p10047360;
        pCtl->pfn08 = pH->p100458E0;
        iB = (int32_t)fB;               /* __ftol, truncates toward zero */
        pCtl->f54 = iB;
        iA = (int32_t)fA;
        pCtl->f50 = iA;
        pCtl->f58 = iA + 0x7F;
        pCtl->f5C = iB + 0x21;
        pCtl->f2968 = 0;
        fB = fB - (-33.0f);
        pCtl->f2A42 = 0x79;
        pScr->cCtl++;
    }

    /* 0x1004AD68 */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, fA, fB, 0x402001, 2, 5, 1, 0x52);
    pCtl->pfn0C = pH->p10047360;
    pCtl->pfn08 = pH->p10043FA0;
    iL14 = (int32_t)fB;
    pCtl->f54 = iL14;
    iA = (int32_t)fA;
    pCtl->f2968 = 0;
    pCtl->f50 = iA;
    pCtl->f2A42 = 0x53;
    iL18 = iA + 0x7F;
    pCtl->f58 = iL18;
    fB = fB - (-33.0f);
    pCtl->f5C = iL14 + 0x21;
    pScr->cCtl++;

    /* 0x1004AE3E. GOTCHA: reuses iA and iL18 from the block above and does
     * NOT advance fB, so this control shares its left and right edges with
     * its predecessor and its top edge with wherever fB was left. */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, fA, fB, 0x402001, 2, 5, 1, 0x54);
    pCtl->pfn0C = pH->p10047360;
    pCtl->pfn08 = pH->p100458C0;
    iB = (int32_t)fB;
    pCtl->f54 = iB;
    pCtl->f50 = iA;
    pCtl->f58 = iL18;
    pCtl->f5C = iB + 0x21;
    pCtl->f2968 = 0;
    pCtl->f2A42 = 0x55;
    pScr->cCtl++;

    /* 0x1004AEEF */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 61.0f, 244.0f, 9, 2, 5, 1, 0x36);
    pScr->cCtl++;

    /* 0x1004AF5D */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 73.0f, 214.0f, 1, 2, 5, 1, 0x35);
    pCtl->pfn04 = pH->p1003E920;
    pScr->cCtl++;

    /* 0x1004AFD2 */
    BR_NEW_CTL();
    /* GOTCHA: x is the LARGER value here (0x43A98000 pushed after
     * 0x428C0000), unlike every other hardcoded pair in the packet. */
    pCtl->pVtbl->f38(pCtl, pPhase, 339.0f, 70.0f, 1, 2, 5, 1, 0x0B);
    pCtl->pfn04 = pH->p1003F720;
    pCtl->f2AB4++;
    pCtl->f2AB6 = (uint16_t)(pScr->cCtl + 1);
    pScr->cCtl++;

    /* 0x1004B05B */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, 137.0f, 0x101001, 2, 5, 1, -1);
    pCtl->pfn04  = pH->p1003F760;
    pCtl->f1E20C = 3;
    pCtl->pVtbl->f34(pCtl, pCtx->p0AD300, 1, 1, pCtx->p0AB468);
    pScr->cCtl++;

    /* 0x1004B0F4 */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 108.0f, 66.0f, 1, 2, 5, 1, 0x19);
    pCtl->pfn04 = pH->p10040890;
    pCtl->f2AB4++;
    pCtl->f2AB6 = (uint16_t)(pScr->cCtl + 1);
    pScr->cCtl++;

    /* 0x1004B17D */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, 123.0f, 0x101001, 2, 5, 1, -1);
    pCtl->pfn04  = pH->p1003F7F0;
    pCtl->f1E20C = 3;
    pCtl->pVtbl->f34(pCtl, pCtx->p0AD300, 1, 1, pCtx->p0AB498);
    pScr->cCtl++;

    /* 0x1004B216 */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 476.0f, 129.0f, 1, 2, 5, 1, 0x0E);
    pCtl->pfn04 = pH->p100408B0;
    pCtl->f2AB4++;
    pCtl->f2AB6 = (uint16_t)(pScr->cCtl + 1);
    pScr->cCtl++;

    /* 0x1004B29F */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, 181.0f, 0x101001, 2, 5, 1, -1);
    pCtl->pfn04  = pH->p1003F990;
    pCtl->f1E20C = 3;
    pCtl->pVtbl->f34(pCtl, pCtx->p0AD300, 1, 1, pCtx->p0AB488);
    pScr->cCtl++;

    /* 0x1004B338 */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 505.0f, 200.0f, 1, 2, 5, 1, 0x0C);
    pCtl->pfn04 = pH->p10040870;
    pCtl->f2AB4++;
    pCtl->f2AB6 = (uint16_t)(pScr->cCtl + 1);
    pScr->cCtl++;

    /* 0x1004B3C1 */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, 262.0f, 0x101001, 2, 5, 1, -1);
    pCtl->pfn04  = pH->p1003F860;
    pCtl->f1E20C = 3;
    pCtl->pVtbl->f34(pCtl, pCtx->p0AD300, 1, 1, pCtx->p0AB478);
    pScr->cCtl++;
}

/* ==========================================================================
 * 0x1004B430
 * ========================================================================== */

void BrExt_1004B430(BrUiBuildCtx *pCtx, BrUiPhase *pPhase)
{
    const BrUiBuildHooks *pH = pCtx->pHooks;
    BrUiScreen *pScr;
    BrUiCtl    *pCtl;
    /* [esp+0x10]: zeroed by the prologue, set to 19.0f ONLY inside the
     * n0AC304 block. GOTCHA: when n0AC304 is zero the first four rows below
     * all land on pScr->fY. */
    float       fRow = 0.0f;

    pScr = BrUiScreenNew(pCtx, pPhase, 130.0f);   /* 0x43020000 */
    if (pScr == NULL) {
        return;
    }

    /* 0x1004B526 */
    BR_NEW_CTL();
    BrUiCtlPlaceRoot(pCtl, pPhase);
    pScr->cCtl++;

    /* 0x1004B58E */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, 10.0f, 0x100009, 2, 5, 1, -1);
    pCtl->f1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x1A), 1, 1, pCtx->p0AB508);
    pScr->cCtl++;

    if (pCtx->n0AC304 != 0) {
        /* 0x1004B634 */
        BR_NEW_CTL();
        pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY,
                         0x102001, 2, 5, 1, -1);
        pCtl->pfn0C  = pH->p100474B0;
        pCtl->pfn08  = pH->p10042EE0;
        pCtl->f1E20C = 3;
        pCtl->pVtbl->f34(pCtl, BrStrGet(0x1B), 1, 1, pCtx->p0AB448);
        pScr->cCtl++;
        pScr->cSel++;
        fRow = 19.0f;                          /* 0x41980000 */
    }

    /* 0x1004B6EC */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, fRow + pScr->fY,
                     0x102001, 2, 5, 1, -1);
    pCtl->pfn0C  = pH->p10047360;
    pCtl->pfn08  = pH->p10043180;
    pCtl->f1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x1C), 1, 1, pCtx->p0AB448);
    fRow = fRow - (-19.0f);
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x1004B7B1 */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, fRow + pScr->fY,
                     0x102001, 2, 5, 1, -1);
    pCtl->pfn0C  = pH->p10047360;
    pCtl->pfn08  = pH->p100430B0;
    pCtl->f1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x1D), 1, 1, pCtx->p0AB448);
    fRow = fRow - (-57.0f);       /* GOTCHA: +57, not +19, at this step */
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x1004B876 */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, fRow + pScr->fY,
                     0x102001, 2, 5, 1, -1);
    pCtl->pfn0C  = pH->p100474B0;
    pCtl->pfn08  = pH->p10045110;
    pCtl->f1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x1E), 1, 1, pCtx->p0AB448);
    fRow = fRow - (-19.0f);
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x1004B93A. GOTCHA: placed and then abandoned -- no f34, no hooks, no
     * cCtl increment, so the NEXT control overwrites it in apCtl. */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, fRow + pScr->fY,
                     0x102001, 2, 5, 1, -1);

    /* 0x1004B9B1 -- fY - (-114); note it uses the SCREEN's fY, not fRow. */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY - (-114.0f),
                     0x102001, 2, 5, 1, -1);
    pCtl->pfn0C  = pH->p10047360;
    pCtl->pfn08  = pH->p10046450;
    pCtl->f1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x0C), 1, 1, pCtx->p0AB448);
    pCtx->pAA29AC = pCtl;
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x1004BA6F */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 73.0f, 212.0f, 1, 2, 5, 1, 0x16);
    pCtl->pfn04 = pH->p100407E0;
    pCtl->f2AB4++;
    pCtl->f2AB6 = (uint16_t)(pScr->cCtl + 1);
    pScr->cCtl++;

    /* 0x1004BAF8 */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, 275.0f, 0x101001, 2, 5, 1, -1);
    pCtl->pfn04  = pH->p1003FE80;
    pCtl->f1E20C = 3;
    pCtl->pVtbl->f34(pCtl, pCtx->p0AD300, 1, 1, pCtx->p0AB4B8);
    pScr->cCtl++;

    if (pCtx->n0AC304 != 0) {
        /* 0x1004BB9E */
        BR_NEW_CTL();
        pCtl->pVtbl->f38(pCtl, pPhase, 325.0f, 72.0f, 1, 2, 5, 1, 0x11);
        pCtl->pfn04 = pH->p10040730;
        pCtl->f2AB4++;
        pCtl->f2AB6 = (uint16_t)(pScr->cCtl + 1);
        pScr->cCtl++;

        /* 0x1004BC27 */
        BR_NEW_CTL();
        pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, 152.0f,
                         0x101001, 2, 5, 1, -1);
        pCtl->pfn04  = pH->p1003FA00;
        pCtl->f1E20C = 3;
        pCtl->pVtbl->f34(pCtl, pCtx->p0AD300, 1, 1, pCtx->p0AB4A8);
        pScr->cCtl++;
    }

    /* 0x1004BCC1. The only site in the packet with f1E20C == 5 and with
     * f34's third argument == 3. */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 106.0f, 68.0f, 0x5001, 2, 5, 1, -1);
    pCtl->pfn04  = pH->p100408D0;
    pCtl->f1E20C = 5;
    pCtl->pVtbl->f34(pCtl, pCtx->p0AD274, 1, 3, pCtx->p0AB458);
    pScr->cCtl++;

    /* 0x1004BD59 */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 106.0f, 115.0f, 0x100001, 2, 5, 1, -1);
    pCtl->f1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x1D), 1, 1, pCtx->p0AB458);
    pScr->cCtl++;
}

/* ==========================================================================
 * 0x1004BDC0
 * ========================================================================== */

void BrExt_1004BDC0(BrUiBuildCtx *pCtx, BrUiPhase *pPhase)
{
    const BrUiBuildHooks *pH = pCtx->pHooks;
    BrUiScreen *pScr;
    BrUiCtl    *pCtl;

    pScr = BrUiScreenNew(pCtx, pPhase, 130.0f);
    if (pScr == NULL) {
        return;
    }

    /* 0x1004BEAD */
    BR_NEW_CTL();
    BrUiCtlPlaceRoot(pCtl, pPhase);
    pScr->cCtl++;

    /* 0x1004BF14 */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, 10.0f, 0x100009, 2, 5, 1, -1);
    pCtl->f1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x06), 1, 1, pCtx->p0AB508);
    pScr->cCtl++;

    /* 0x1004BFAC */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY, 0x102001, 2, 5, 1, -1);
    pCtl->pfn0C  = pH->p10047360;
    pCtl->pfn08  = pH->p100452C0;
    pCtl->f1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x1F), 1, 1, pCtx->p0AB448);
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x1004C05B -- fY - (-19) */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY - (-19.0f),
                     0x102001, 2, 5, 1, -1);
    pCtl->pfn0C  = pH->p10047360;
    pCtl->pfn08  = pH->p10045390;
    pCtl->f1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x20), 1, 1, pCtx->p0AB448);
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x1004C113 -- fY - (-38) */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY - (-38.0f),
                     0x102001, 2, 5, 1, -1);
    pCtl->pfn0C  = pH->p10047360;
    pCtl->pfn08  = pH->p100455E0;
    pCtl->f1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x21), 1, 1, pCtx->p0AB448);
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x1004C1CB -- fY - (-57) */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY - (-57.0f),
                     0x102001, 2, 5, 1, -1);
    pCtl->pfn0C  = pH->p10047360;
    pCtl->pfn08  = pH->p100456B0;
    pCtl->f1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x22), 1, 1, pCtx->p0AB448);
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x1004C283 -- fY - (-114); -76 and -95 are both skipped here */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY - (-114.0f),
                     0x102001, 2, 5, 1, -1);
    pCtl->pfn0C  = pH->p10047360;
    pCtl->pfn08  = pH->p10046520;
    pCtl->f1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x0C), 1, 1, pCtx->p0AB448);
    pCtx->pAA29C8 = pCtl;
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x1004C340 */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 80.0f, 46.0f, 9, 2, 5, 0, 9);
    pScr->cCtl++;

    /* 0x1004C3AD */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 324.0f, 46.0f, 1, 2, 5, 1, 0x16);
    pCtl->pfn04 = pH->p10041870;
    pCtl->f2AB4++;
    pCtl->f2AB6 = (uint16_t)(pScr->cCtl + 1);
    pScr->cCtl++;

    /* 0x1004C435 */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, 155.0f, 0x101001, 2, 5, 1, -1);
    pCtl->pfn04  = pH->p1003FFD0;
    pCtl->f1E20C = 3;
    pCtl->pVtbl->f34(pCtl, pCtx->p0AD300, 1, 1, pCtx->p0AB4C8);
    pScr->cCtl++;
}

/* ==========================================================================
 * 0x1004C4A0
 * ========================================================================== */

void BrExt_1004C4A0(BrUiBuildCtx *pCtx, BrUiPhase *pPhase)
{
    const BrUiBuildHooks *pH = pCtx->pHooks;
    BrUiScreen *pScr;
    BrUiCtl    *pCtl;

    pScr = BrUiScreenNew(pCtx, pPhase, 130.0f);
    if (pScr == NULL) {
        return;
    }

    /* 0x1004C58D */
    BR_NEW_CTL();
    BrUiCtlPlaceRoot(pCtl, pPhase);
    pScr->cCtl++;

    /* 0x1004C5F4 */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, 10.0f, 0x100009, 2, 5, 1, -1);
    pCtl->f1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x1F), 1, 1, pCtx->p0AB508);
    pScr->cCtl++;

    /* 0x1004C68C */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY, 0x102001, 2, 5, 1, -1);
    pCtl->pfn0C  = pH->p10047360;
    pCtl->pfn08  = pH->p10043400;
    pCtl->f1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x23), 1, 1, pCtx->p0AB448);
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x1004C73B -- fY - (-19) */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY - (-19.0f),
                     0x102001, 2, 5, 1, -1);
    pCtl->pfn0C  = pH->p10047360;
    pCtl->pfn08  = pH->p100434C0;
    pCtl->f1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x24), 1, 1, pCtx->p0AB448);
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x1004C7F3 -- fY - (-38) */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY - (-38.0f),
                     0x102001, 2, 5, 1, -1);
    pCtl->pfn0C  = pH->p10047360;
    pCtl->pfn08  = pH->p100406C0;
    pCtl->f1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x25), 1, 1, pCtx->p0AB448);
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x1004C8AB -- fY - (-114) */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY - (-114.0f),
                     0x102001, 2, 5, 1, -1);
    pCtl->pfn0C  = pH->p10047360;
    pCtl->pfn08  = pH->p100465A0;
    pCtl->f1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x0C), 1, 1, pCtx->p0AB448);
    pCtx->pAA29C8 = pCtl;
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x1004C968 */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 80.0f, 46.0f, 9, 2, 5, 0, 9);
    pScr->cCtl++;

    /* 0x1004C9D5 */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 324.0f, 46.0f, 1, 2, 5, 1, 0x16);
    pCtl->pfn04 = pH->p10041870;
    pCtl->f2AB4++;
    pCtl->f2AB6 = (uint16_t)(pScr->cCtl + 1);
    pScr->cCtl++;

    /* 0x1004CA5D */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, 155.0f, 0x101001, 2, 5, 1, -1);
    pCtl->pfn04  = pH->p1003FFD0;
    pCtl->f1E20C = 3;
    pCtl->pVtbl->f34(pCtl, pCtx->p0AD300, 1, 1, pCtx->p0AB4C8);
    pScr->cCtl++;
}

/* ==========================================================================
 * 0x1004CAC0
 * ========================================================================== */

void BrExt_1004CAC0(BrUiBuildCtx *pCtx, BrUiPhase *pPhase)
{
    const BrUiBuildHooks *pH = pCtx->pHooks;
    BrUiScreen *pScr;
    BrUiCtl    *pCtl;

    pScr = BrUiScreenNew(pCtx, pPhase, 130.0f);
    if (pScr == NULL) {
        return;
    }

    /* 0x1004CBAE */
    BR_NEW_CTL();
    BrUiCtlPlaceRoot(pCtl, pPhase);
    pScr->cCtl++;

    /* 0x1004CC15 */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, 10.0f, 0x100009, 2, 5, 1, -1);
    pCtl->f1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x26), 1, 1, pCtx->p0AB508);
    pScr->cCtl++;

    /* 0x1004CCAD -- the list control */
    {
        BrUiCtlSub           *pSub;
        const BrUiCtlSubVtbl *pSubVtbl;
        int32_t               i;

        BR_NEW_CTL();
        pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY,
                         0x3001, 2, 5, 1, -1);

        /* The vtable is read ONCE, before the loop, and reused. */
        pSub     = &pCtl->f3838;
        pSubVtbl = pSub->pVtbl;

        pCtl->pfn04  = pH->p1003EC80;
        pCtl->f1E1F4 = 1;
        pSubVtbl->f14(pSub, 0x40001, pCtx->p0AB4D8, 5, 0, -1);

        for (i = 0; i < BR_UI_AB330_COUNT; ++i) {
            int32_t nA2 = 0;

            /* nAA2A0C is re-read every iteration; the flag applies to the
             * first two records only, and writes nAA2840 once per record. */
            if (pCtx->nAA2A0C == 3 && (i == 0 || i == 1)) {
                nA2 = 0x10;
                pCtx->nAA2840 = 2;
            }
            /* GOTCHA: looked up twice -- once to test, once for the value. */
            if (BrStrGet(pCtx->aAB330[i].idText) != NULL) {
                pSubVtbl->f10(pSub, BrStrGet(pCtx->aAB330[i].idText),
                              nA2, 1, pCtx->p0AB4D8, 0);
            }
        }
        pScr->cCtl++;
        pScr->cSel++;
    }

    /* 0x1004CDCF -- fY - (-95). The only use of 0x1008F690 in the packet. */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY - (-95.0f),
                     0x102001, 2, 5, 1, -1);
    pCtl->pfn0C  = pH->p10047360;
    pCtl->pfn08  = pH->p10040680;
    pCtl->pfn18  = pH->p10040450;      /* the only +0x18 store in the packet */
    pCtl->f1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x27), 1, 1, pCtx->p0AB448);
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x1004CE8E -- fY - (-114) */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY - (-114.0f),
                     0x102001, 2, 5, 1, -1);
    pCtl->pfn0C  = pH->p10047360;
    pCtl->pfn08  = pH->p10046560;
    pCtl->f1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x0C), 1, 1, pCtx->p0AB448);
    pCtx->pAA29C8 = pCtl;
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x1004CF4B */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 80.0f, 46.0f, 9, 2, 5, 0, 9);
    pScr->cCtl++;

    /* 0x1004CFB8 */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, 135.0f, 0x101001, 2, 5, 1, -1);
    pCtl->pfn04  = pH->p1003F8D0;
    pCtl->f1E20C = 3;
    pCtl->pVtbl->f34(pCtl, pCtx->p0AD300, 1, 1, pCtx->p0AB488);
    pScr->cCtl++;

    /* 0x1004D050 */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 324.0f, 46.0f, 1, 2, 5, 1, 0x16);
    pCtl->pfn04 = pH->p10041870;
    pCtl->f2AB4++;
    pCtl->f2AB6 = (uint16_t)(pScr->cCtl + 1);
    pScr->cCtl++;

    /* 0x1004D0D8 -- flags 0x100001 here, 0x101001 at the twin sites */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, 155.0f, 0x100001, 2, 5, 1, -1);
    pCtl->f1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x28), 1, 1, pCtx->p0AB4C8);
    pScr->cCtl++;

    /* 0x1004D170 */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, 100.0f, 0x101001, 2, 5, 1, -1);
    pCtl->pfn04  = pH->p100400E0;
    pCtl->f1E20C = 3;
    pCtl->pVtbl->f34(pCtl, pCtx->p0AD300, 1, 1, pCtx->p0AB4C8);
    pScr->cCtl++;

    /* 0x1004D1B8 -- the only tail work in the packet. */
    pCtx->nAA2850 = pCtx->pfn10040330(pCtx->aAC520[pCtx->nAA2A0C]);
}
