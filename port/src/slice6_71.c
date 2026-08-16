/* slice6_71.c -- packet 71. See slice6_71.h.
 *
 * Float literals below are the exact values of the 32-bit patterns the
 * original pushes (195.0f == 0x43430000, 130.0f == 0x43020000,
 * 10.0f == 0x41200000, ...). The `fsub` constants are the .rdata words at
 * 0x1008F660 (8.0), 0x1008F680..0x1008F69C (-19, -38, -57, -76, -95, -114,
 * -133, -33) and 0x1008F6A8 (12.0), read out of orig/BRD3D.dll with
 * tools/pe.py rather than assumed. Note the sign split: the 0x1008F68x block
 * is NEGATIVE, so `fsub` there is an ADDITION of a row offset, while
 * 0x1008F660 / 0x1008F6A8 are POSITIVE and genuinely subtract.
 *
 * There is not one `fxch` in this packet: every float instruction is
 * `fld m32 / fsub m32 / fstp m32`, the non-reversed form st(0) = st(0) - m32,
 * so no operand order is ambiguous.
 */

#include <string.h>

#include "slice6_71.h"

/* --- DUPLICATE OWNERSHIP (host link only) -------------------------------
 * slice6_71 and slice6_70 each independently ported 0x1003BF60. Both bodies
 * are faithful; the duplication is a coordination artefact of parallel
 * passes, not a disagreement about behaviour.
 *
 * slice6_70 is the OWNER. Under BR_HOST_LINK this module's copies are renamed
 * so the full-program link has exactly one definition of each. The renaming
 * covers this file's internal calls too, so the module stays self-consistent
 * and its own test binary -- which links this .o alone -- is unaffected.
 *
 * This is a stopgap. The duplicate bodies should be deleted and the callers
 * pointed at slice6_70's, once someone has diffed the two transcriptions
 * line-by-line and confirmed they agree. Until that diff is done, deleting the
 * wrong one would silently discard the better transcription.
 * ------------------------------------------------------------------------ */
#ifdef BR_HOST_LINK
#define BrSub1003BF60 BrSub1003BF60_dup_71
#define BrExt_1003BF60 BrExt_1003BF60_dup_71
#endif



BrS71Globals    g_brS71;
const BrS71Env *g_brS71Env;

/* ==========================================================================
 * 0x1003F2B0 -- adapter over slice1_06.c's BrOptAvailA
 * ========================================================================== */

/* The original's index is a plain `mov ecx,[esp+4]` used as a shift count and
 * compared against 12/13/14, i.e. it is consumed unsigned. BrOptAvailA takes
 * it as uint32_t and applies the x86 `shl cl` 5-bit mask itself. */
int BrSub1003F2B0(int index)
{
    return (int)BrOptAvailA(g_brS71.pOptCaps, (uint32_t)index);
}

/* ==========================================================================
 * 0x1003BF60 -- session teardown
 * ========================================================================== */

void BrSub1003BF60(void)
{
    const BrS71Env *pEnv = g_brS71Env;

    pEnv->pfn100586A0();

    /* KillTimer(g_680584, g_A9BFDC). The original ignores the result. */
    pEnv->pfnKillTimer(g_brS71.hWnd680584, g_brS71.nA9BFDC);

    if (g_brS71.nAA2884 != 0) {
        pEnv->pfn10072270();
    }
    pEnv->pfn1003C550();

    /* GOTCHA: modes 2 and 3 skip the entity reset entirely; every other
     * value falls through to it. The null test is on 0x10AA29D8 and both
     * writes then go through a FRESH load of the same global -- the original
     * reloads it rather than reusing the register. Nothing can change it in
     * between, so the port reads it once. */
    if (g_brS71.nAA287C != 2 && g_brS71.nAA287C != 3) {
        if (g_brS71.pAA29D8_b2B64 != NULL) {
            *g_brS71.pAA29D8_b2B64 = 0;
            /* `and dword [eax+0x1C], 0xFFFFFFEF` -- slice2_26.h names this
             * bit BR_ENTITY_FLAG_1C_10; that header carries its own partial
             * model of the phase and cannot be included here, so the value is
             * spelled out rather than a second name coined for it. */
            *g_brS71.pAA29D8_f1C &= ~(int32_t)0x10;
        }
    }

    g_brS71.nA9CFFC = 0;
    g_brS71.nAA2884 = 0;
    g_brS71.n22AF18 = 0;
    g_brS71.nAA2888 = 0;
}

/* Second wanted name for the same address -- see slice6_71.h. */
void BrExt_1003BF60(void)
{
    BrSub1003BF60();
}

/* ==========================================================================
 * 0x10038F30 -- the shutdown sequence
 * ========================================================================== */

/* A straight line of calls. The value is in the ORDER and in the four
 * conditionals, so every call is kept even where the callee is known to be a
 * stub (0x10008B80 -- see the contract). */
void BrExt_10038F30(int32_t a)
{
    const BrS71Env *pEnv = g_brS71Env;

    /* Both the phase and 0x100AC300 must be non-zero. The phase's +0x68 is
     * cleared BEFORE the vtable call, and the global is re-read for the
     * call's `this` exactly as the original does. */
    if (g_brS71.pAA2904 != NULL && g_brS71.n0AC300 != 0) {
        g_brS71.pAA2904->f68 = 0;
        g_brS71.pAA2904->pVtbl->f18(g_brS71.pAA2904, NULL);
    }

    pEnv->pfn1002C4A0();
    pEnv->pfn10016990();

    if (g_brS71.pfnB501CC != NULL) {
        g_brS71.pfnB501CC();
    }

    pEnv->pfn10079550();
    pEnv->pfn10078BC0();
    pEnv->pfn10078DB0();
    pEnv->pfn10073730();

    if (g_brS71.n22AF18 != 0) {
        pEnv->pfn10005BE0(1);
    }

    pEnv->pfn1003BFD0();
    BrSub1003BF60();

    if (g_brS71.n0940A4 != 0) {
        pEnv->pfn10002CF0();
    }

    pEnv->pfn10008B80();

    if (g_brS71.pfn18AA0D0 != NULL) {
        g_brS71.pfn18AA0D0();
    }
    if (g_brS71.pfn690A28 != NULL) {
        g_brS71.pfn690A28();
    }

    pEnv->pfn10061620();
    pEnv->pfn10008970(g_brS71.pA99780);   /* __thiscall, ecx = 0x10A99780 */
    pEnv->pfn1002AEA0();
    pEnv->pfn10074050();
    pEnv->pfnCoUninitialize();

    /* 0x1007CC00 with the incoming argument. The original does
     * `push ecx / call / add esp,4 / ret`, i.e. it is modelled as a call that
     * RETURNS -- slice2_16.h makes the same choice for this address. */
    pEnv->pfnExit(a);
}

/* ==========================================================================
 * The two allocation steps, byte-identical in all four builders
 * ========================================================================== */

/* The page prologue: 0x10049F5E..0x10049FE4 and its three twins.
 *
 * The page is br_ui.h's BrUiPage_ and the constructor is 0x10048470 under the
 * name the full model uses. slice3_33.h's BrUiScreen / BrUiScreenCtor pair is
 * gone from this module: that model begins at +0x10, so a page built through
 * it and read through a model that begins at +0x00 is three fields out at
 * every access -- which is exactly what the host harness measured.
 *
 * DEVIATION (memory safety, two places): the array writes are bounded. The
 * original has the same implicit bounds -- aPages ends where aFlags begins --
 * but does not check them.
 *
 * DEVIATION (memory safety): on allocation failure the original reports error
 * index 4 and then dereferences NULL. Index 4 is FATAL in g_aBrErrTable
 * (slice1_06.c), so BrErrShow does not return there in practice; the port
 * returns NULL instead of faulting. */
static BrUiPage_ *Br71PageNew(BrPhase_ *pPhase, float fY)
{
    BrUiPage_ *pScr;
    uint16_t   i;

    i = pPhase->nPages;
    pPhase->iPage = 0;
    if (i < BR_PHASE_PAGES) {
        pPhase->aFlags[i] = 1;
    }

    pScr = (BrUiPage_ *)BrOperatorNew(BR_UI_PAGE_ALLOC_SIZE);
    pScr = (pScr != NULL) ? BrUiPageCtor_10048470(pScr) : NULL;

    /* The original re-reads the counter here rather than reusing `i`. */
    i = pPhase->nPages;
    if (i < BR_PHASE_PAGES) {
        pPhase->aPages[i] = pScr;
    }
    if (pScr == NULL) {
        BrErrShow(g_brS71.pErrHost, 4);
    }
    pPhase->nPages++;

    if (pScr == NULL) {
        return NULL;                  /* DEVIATION: see above */
    }

    pScr->pOwner = pPhase;
    pScr->f10    = 0;
    pScr->fX     = 195.0f;            /* 0x43430000 -- the same in all four */
    pScr->fY     = fY;
    return pScr;
}

/* The control prologue. Same two DEVIATIONs. The store into apCtl happens
 * BEFORE the null test, exactly as the original does. */
static BrUiCtl_ *Br71CtlNew(BrUiPage_ *pScr)
{
    BrUiCtl_ *pCtl;

    pCtl = (BrUiCtl_ *)BrOperatorNew(BR_UI_CTL_ALLOC_SIZE);
    pCtl = (pCtl != NULL) ? BrUiCtlCtor(pCtl) : NULL;

    if (pScr->cCtl < BR_UI_PAGE_CTL_MAX) {
        pScr->apCtl[pScr->cCtl] = pCtl;
    }
    if (pCtl == NULL) {
        BrErrShow(g_brS71.pErrHost, 4);
    }
    return pCtl;
}

/* Relies on the local names pScr / pCtl, which every builder here declares. */
#define BR71_NEW_CTL()                                  \
    do {                                                \
        pCtl = Br71CtlNew(pScr);                        \
        if (pCtl == NULL) { return; }                   \
    } while (0)

/* The unnamed first control of every screen. Note the owner argument is the
 * PHASE, not the page, at every f38 site in the packet. The original pushes
 * the zero register for x and y, i.e. four zero bytes == 0.0f. */
static void Br71PlaceRoot(BrUiCtl_ *pCtl, BrPhase_ *pPhase)
{
    pCtl->pVtbl->f38(pCtl, pPhase, 0.0f, 0.0f, 9, 2, 5, 0, 0);
}

/* ==========================================================================
 * 0x10049F40
 * ========================================================================== */

void BrExt_10049F40(BrPhase_ *pSelf)
{
    const BrS71Hooks *pH = g_brS71.pHooks;
    BrUiPage_        *pScr;
    BrUiCtl_         *pCtl;

    pScr = Br71PageNew(pSelf, 130.0f);          /* 0x43020000 */
    if (pScr == NULL) {
        return;
    }

    /* 0x1004A02D */
    BR71_NEW_CTL();
    Br71PlaceRoot(pCtl, pSelf);
    pScr->cCtl++;

    /* 0x1004A094 -- the title */
    BR71_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pSelf, pScr->fX, 10.0f, 0x100009, 2, 5, 1, -1);
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x10), 1, 1, g_brS71.p0AB438);
    pScr->cCtl++;

    /* 0x1004A12C */
    BR71_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pSelf, pScr->fX, pScr->fY, 0x102001, 2, 5, 1, -1);
    pCtl->pfn0C  = pH->p10047360;
    pCtl->pfn08  = pH->p10046F60;
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x11), 1, 1, g_brS71.p0AB448);
    g_brS71.pAA29B0 = pCtl;
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x1004A1E1 -- fY - (-19) */
    BR71_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pSelf, pScr->fX, pScr->fY - (-19.0f),
                     0x102001, 2, 5, 1, -1);
    pCtl->pfn0C  = pH->p10047360;
    pCtl->pfn08  = pH->p10046FC0;
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x12), 1, 1, g_brS71.p0AB448);
    pScr->cCtl++;
    pScr->cSel++;
}

/* ==========================================================================
 * 0x10051D30
 * ========================================================================== */

void BrOptFn10051D30(BrPhase_ *pThis)
{
    const BrS71Hooks *pH = g_brS71.pHooks;
    BrUiPage_        *pScr;
    BrUiCtl_         *pCtl;
    int               k;

    pScr = Br71PageNew(pThis, 130.0f);
    if (pScr == NULL) {
        return;
    }

    /* 0x10051E1D */
    BR71_NEW_CTL();
    Br71PlaceRoot(pCtl, pThis);
    pScr->cCtl++;

    /* 0x10051E84 -- the title */
    BR71_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pThis, pScr->fX, 10.0f, 0x100009, 2, 5, 1, -1);
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x43), 1, 1, g_brS71.p0AB438);
    pScr->cCtl++;

    /* 0x10051F1C -- the one control that breaks the pattern: no f34 at all,
     * a step table, and an integer rectangle computed from the page's own
     * float origin. Both `fsub` constants here are POSITIVE (8.0 at
     * 0x1008F660, 12.0 at 0x1008F6A8), so these two genuinely subtract. */
    BR71_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pThis, pScr->fX - 8.0f, pScr->fY - 12.0f,
                     0x22001, 2, 5, 0, 0x50);
    pCtl->p1E210 = g_brS71.pA9DA50;
    pCtl->f2968  = 1;
    pCtl->f296C  = 1;

    /* Two loops in the original -- 15 iterations then 9 -- but +0x29B4 is
     * exactly +0x2978 + 15*4 and +0x2A5E exactly +0x2A40 + 15*2, so the two
     * loops walk ONE pair of parallel arrays whose tail carries a different
     * code. The ARRAYS hold 50 entries each (br_ui.h ADJ-3, from the
     * constructor's `mov ecx,0x32 / lea edi,[esi+0x2978] / rep stosd`); the
     * 24 this module used to declare was the high-water mark of this loop,
     * which is a floor, not a bound. Entries 24..49 stay at the
     * constructor's 0 / -1 -- and now really do exist to stay there. */
    for (k = 0; k < 15; ++k) {
        pCtl->aStepMs[k] = 0x3C;
        pCtl->aStepId[k] = 0x50;
    }
    for (k = 15; k < 24; ++k) {
        pCtl->aStepMs[k] = 0x3C;
        pCtl->aStepId[k] = 0x51;
    }

    pCtl->pfn08  = pH->p100471B0;
    pCtl->w1E20C = 0x50;

    /* The four are read back from the PAGE's floats, not from the values
     * just handed to f38, and the two offsets are +0x80 -- not the +0x7F /
     * +0x21 pair the other builders use. __ftol truncates toward zero. */
    pCtl->rcTop    = BrFtolTrunc(pScr->fY);
    pCtl->rcLeft   = BrFtolTrunc(pScr->fX);
    pCtl->rcRight  = BrFtolTrunc(pScr->fX) + 0x80;
    pCtl->rcBottom = BrFtolTrunc(pScr->fY) + 0x80;

    pScr->cCtl++;
    pScr->cSel++;
}

/* ==========================================================================
 * 0x1004F700
 * ========================================================================== */

void BrExt_1004F700(BrPhase_ *pSelf)
{
    const BrS71Hooks *pH   = g_brS71.pHooks;
    const BrS71Env   *pEnv = g_brS71Env;
    BrS71FileList    *pFiles;
    BrUiPage_        *pScr;
    BrUiCtl_         *pCtl;
    BrTextList       *pList;
    void             *pFile;
    int32_t           fAutoSave;
    int32_t           flags;
    int32_t           k;

    pSelf->iPage = 0;

    /* 0x1004F72C: rescan the season files with 0x10AA2848 raised. */
    g_brS71.n0AB3F4 = -1;
    g_brS71.nAA2848 = 1;
    pFiles = (BrS71FileList *)g_brS71.pAA2908->fC0;
    pFiles->pVtbl->f04(pFiles, g_brS71.pszRallySeasonBrf);
    g_brS71.nAA2848 = 0;

    /* The page prologue runs AFTER the rescan, and re-reads nPages, so it
     * cannot be hoisted above it. */
    {
        BrUiPage_ *pTmp;
        uint16_t   i = pSelf->nPages;
        if (i < BR_PHASE_PAGES) {
            pSelf->aFlags[i] = 1;
        }
        pTmp = (BrUiPage_ *)BrOperatorNew(BR_UI_PAGE_ALLOC_SIZE);
        pTmp = (pTmp != NULL) ? BrUiPageCtor_10048470(pTmp) : NULL;
        i = pSelf->nPages;
        if (i < BR_PHASE_PAGES) {
            pSelf->aPages[i] = pTmp;
        }
        if (pTmp == NULL) {
            BrErrShow(g_brS71.pErrHost, 4);
        }
        pSelf->nPages++;
        if (pTmp == NULL) {
            return;                   /* DEVIATION, as in Br71PageNew */
        }
        pTmp->pOwner = pSelf;
        pTmp->f10    = 0;
        pTmp->fX     = 195.0f;
        pTmp->fY     = 130.0f;
        pScr = pTmp;
    }

    /* 0x1004F818 */
    BR71_NEW_CTL();
    Br71PlaceRoot(pCtl, pSelf);
    pScr->cCtl++;

    /* 0x1004F880 -- the title */
    BR71_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pSelf, pScr->fX, 10.0f, 0x100009, 2, 5, 1, -1);
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x34), 1, 1, g_brS71.p0AB508);
    pScr->cCtl++;

    /* 0x1004F919 -- the season list.
     *
     * The object at control +0x3838 is the whole embedded BrTextList
     * (br_ui.h ADJ-6), not the one-vtable-slot stand-in this module used to
     * carry: +0x383C is list.f04 and +0x1E1F4 is list.f1A99C[8]
     * (0x3838 + 0x1A99C + 8*4 == 0x1E1F4). */
    BR71_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pSelf, pScr->fX, pScr->fY, 0x3001, 2, 5, 1, -1);
    pList                  = &pCtl->list;
    pCtl->pfn04            = pH->p1003EAE0;
    pCtl->list.f1A99C[8].i = 1;
    pList->pVtbl->f14(pList, 0x40001, g_brS71.p0AB538, 4, 0, -1);
    pCtl->list.f04         = pH->p10042170;

    /* GOTCHA: the loop tests the COMPUTED ADDRESS, not the entry. `lea
     * eax,[eax+ebp+4] / test eax,eax` is only ever false if base + k + 4
     * wraps to zero, so in practice the row is always appended. Reproduced
     * rather than "fixed": the guard is in the original.
     * The base is re-read from 0x10AA2908 on every iteration. */
    for (k = 0; k < BR71_LIST_BYTES; k += BR71_LIST_STRIDE) {
        char *pRow = (char *)g_brS71.pAA2908->fC0 + k + 4;
        if (pRow != NULL) {
            pList->pVtbl->f10(pList, pRow, 0, 1, g_brS71.p0AB4D8, 1);
        }
    }
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x1004FA0C -- fY - (-76) */
    BR71_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pSelf, pScr->fX, pScr->fY - (-76.0f),
                     0x103011, 2, 5, 1, -1);
    pCtl->pfn0C  = pH->p10047360;
    pCtl->pfn08  = pH->p10045090;
    pCtl->pfn04  = pH->p10041890;
    pCtl->w1E20C = 2;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x1E), 1, 0, g_brS71.p0AB448);
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x1004FA7B -- "does AutoSave.brf open?". The handle is only closed
     * again; the answer is a flag. */
    fAutoSave = 1;
    pFile = pEnv->pfnFopen(g_brS71.pszAutoSaveBrf, g_brS71.pszFopenMode);
    if (pFile == NULL) {
        fAutoSave = 0;
    } else {
        pEnv->pfnFclose(pFile);
    }

    /* 0x1004FAF9 -- fY - (-95).
     * `neg / sbb / and 0xFFFFFFF0 / add 0x102011` is a branchless select:
     * 0x102001 when the file exists, 0x102011 when it does not. The SAME
     * value is pushed twice, and the second push is then overwritten by the
     * fstp -- only the flags copy survives. */
    flags = (fAutoSave != 0) ? 0x102001 : 0x102011;
    BR71_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pSelf, pScr->fX, pScr->fY - (-95.0f),
                     flags, 2, 5, 1, -1);
    pCtl->pfn0C = pH->p10047360;
    pCtl->pfn08 = pH->p100450C0;
    if (fAutoSave != 0) {
        pCtl->w1E20C = 3;
        pCtl->pVtbl->f34(pCtl, BrStrGet(0x35), 1, 1, g_brS71.p0AB448);
    } else {
        pCtl->w1E20C = 2;
        pCtl->pVtbl->f34(pCtl, BrStrGet(0x35), 1, 0, g_brS71.p0AB448);
    }
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x1004FBD9 -- fY - (-114) */
    BR71_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pSelf, pScr->fX, pScr->fY - (-114.0f),
                     0x102001, 2, 5, 1, -1);
    pCtl->pfn0C  = pH->p10047360;
    pCtl->pfn08  = pH->p10046E10;
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0xC), 1, 1, g_brS71.p0AB448);
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x1004FC91 -- no text, no w1E20C */
    BR71_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pSelf, 80.0f, 46.0f, 9, 2, 5, 0, 6);
    pScr->cCtl++;

    /* 0x1004FCFF */
    BR71_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pSelf, 330.0f, 153.0f, 0x100009, 2, 5, 1, -1);
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x36), 1, 1, g_brS71.p0AB468);
    pScr->cCtl++;

    /* 0x1004FD96 -- text comes from 0x1039B720 directly, NOT through
     * BrStrGet, and no cSel++ follows. */
    BR71_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pSelf, 330.0f, 97.0f, 0x5001, 2, 5, 1, -1);
    pCtl->pfn04  = pH->p10040A50;
    pCtl->w1E20C = 5;
    pCtl->pVtbl->f34(pCtl, g_brS71.p39B720, 1, 3, g_brS71.p0AB468);
    pScr->cCtl++;

    /* 0x1004FE2E */
    BR71_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pSelf, 440.0f, 181.0f, 0x100009, 2, 5, 1, -1);
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x37), 1, 1, g_brS71.p0AB488);
    pScr->cCtl++;

    /* 0x1004FEC5 */
    BR71_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pSelf, 440.0f, 129.0f, 0x5001, 2, 5, 1, -1);
    pCtl->pfn04  = pH->p10040AC0;
    pCtl->w1E20C = 5;
    pCtl->pVtbl->f34(pCtl, g_brS71.p39B720, 1, 3, g_brS71.p0AB488);
    pScr->cCtl++;

    /* 0x1004FFF4 */
    BR71_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pSelf, 440.0f, 243.0f, 0x100009, 2, 5, 1, -1);
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x38), 1, 1, g_brS71.p0AB478);
    pScr->cCtl++;

    /* 0x1004FFF4 (the last one) -- w1E20C is 0x34 here, not 5. */
    BR71_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pSelf, 440.0f, 224.0f, 0x5001, 2, 5, 1, -1);
    pCtl->pfn04  = pH->p10041300;
    pCtl->w1E20C = 0x34;
    pCtl->pVtbl->f34(pCtl, g_brS71.p39B720, 1, 4, g_brS71.p0AB478);
    pScr->cCtl++;
}

/* ==========================================================================
 * 0x100575F0
 * ========================================================================== */

void BrOptFn100575F0(BrPhase_ *pThis)
{
    const BrS71Hooks *pH   = g_brS71.pHooks;
    const BrS71Env   *pEnv = g_brS71Env;
    BrUiPage_        *pScr;
    BrUiCtl_         *pCtl;
    BrTextBox        *pItem;
    const char       *pszSrc;

    pThis->iPage = 0;
    pEnv->pfn100586A0();          /* the same slot-table reset 0x1003BF60 runs */

    /* Br71PageNew re-does the iPage store; harmless and keeps one helper. */
    pScr = Br71PageNew(pThis, 130.0f);
    if (pScr == NULL) {
        return;
    }

    /* 0x100576E3 */
    BR71_NEW_CTL();
    Br71PlaceRoot(pCtl, pThis);
    pScr->cCtl++;

    /* 0x1005774A -- the title */
    BR71_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pThis, pScr->fX, 10.0f, 0x100009, 2, 5, 1, -1);
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x62), 1, 1, g_brS71.p0AB508);
    pScr->cCtl++;

    /* 0x100577E2 -- a3 is 4 here, and w1E20C is 0x34 */
    BR71_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pThis, pScr->fX, pScr->fY, 0x100009, 2, 5, 1, -1);
    pCtl->w1E20C = 0x34;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x63), 1, 4, g_brS71.p0AB448);
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x10057883 -- fixed coordinates, no text, no w1E20C, no cSel++ */
    BR71_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pThis, 156.0f, 172.0f, 9, 2, 5, 0, 0x39);
    pScr->cCtl++;

    /* 0x100578F4 -- the name field. This is the control that breaks the
     * pattern: three hook slots, a literal text pointer, and then the text
     * box at +0x2B5C is loaded from 0x10A9D018 and told to re-measure.
     *
     * That block is aText[0], the FIRST of three 0x438-byte BrTextBox
     * elements the constructor builds there (br_ui.h ADJ-1), and every
     * +0x2F7x / +0x2F8x offset this builder writes is one of its own fields
     * (ADJ-2): +0x2F78 f41C, +0x2F80 left, +0x2F84 f428, +0x2F88 right,
     * +0x2F8C f430. */
    BR71_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pThis, pScr->fX, 174.0f, 0x200001, 2, 5, 1, -1);
    pCtl->pfn08  = pH->p10042B00;
    pCtl->pfn04  = pH->p1003F210;
    pCtl->pfn10  = pH->p1003F280;
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, g_brS71.p39B720, 1, 1, g_brS71.p0AB448);

    pItem = &pCtl->aText[0];

    /* GOTCHA: the test is `strlen(0x10A9D018) > 1`, not `!= 0`. A one-
     * character session name is treated as absent and replaced by string
     * 0xC1. The copy that follows is a `rep movsd`+`rep movsb` of
     * strlen+1 bytes, i.e. it copies the terminator. */
    if (strlen(g_brS71.pA9D018) > 1u) {
        pszSrc = g_brS71.pA9D018;
    } else {
        pszSrc = BrStrGet(0xC1);
    }
    /* DEVIATION (memory safety): bounded. The original copies strlen+1 bytes
     * into a fixed field with no check at all.
     *
     * The bound is slice3_39.h's BR_TEXTBOX_MAX (0x400), which is what the
     * element constructor's `rep stosd` of 0x100 dwords from element +0x09
     * establishes. This module used to say 0x401, measured from +0x2B65 up to
     * the int16 at +0x2F66; the extra byte at element +0x409 is padding the
     * constructor does not clear and nothing writes. */
    {
        size_t cb = strlen(pszSrc) + 1u;
        if (cb > (size_t)BR_TEXTBOX_MAX) {
            cb = (size_t)BR_TEXTBOX_MAX;
        }
        memcpy(pItem->sz, pszSrc, cb);
        pItem->sz[BR_TEXTBOX_MAX - 1] = '\0';
    }
    pItem->pVtbl->pfn04(pItem);

    pCtl->rcLeft    = 0xC5;
    pItem->left     = 0xC5;
    pCtl->rcRight   = 0x135;
    pItem->right    = 0x135;
    pCtl->rcTop     = 0xAC;
    pItem->f428     = 0xAC;
    pCtl->rcBottom  = 0xBC;
    pItem->f430     = 0xBC;

    /* `mov ax,[+0x2F88] / sub ax,[+0x2F80] / sub eax,0x10 / mov [+0x2F78],ax`
     * -- the subtraction is 16-bit, the -0x10 is 32-bit, and only the low
     * word is stored. 0x135 - 0xC5 - 0x10 == 0x60. */
    pItem->f41C = (int16_t)(uint16_t)((uint16_t)((uint16_t)pItem->right -
                                                 (uint16_t)pItem->left)
                                      - 0x10u);
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x10057A54 -- fY - (-95) */
    BR71_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pThis, pScr->fX, pScr->fY - (-95.0f),
                     0x102001, 2, 5, 1, -1);
    pCtl->pfn0C  = pH->p10047360;
    pCtl->pfn08  = pH->p100443E0;
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x1E), 1, 1, g_brS71.p0AB448);
    g_brS71.pAA29BC = pCtl;
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x10057B16 -- fY - (-114) */
    BR71_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pThis, pScr->fX, pScr->fY - (-114.0f),
                     0x102001, 2, 5, 1, -1);
    pCtl->pfn0C  = pH->p10047360;
    pCtl->pfn08  = pH->p100444C0;
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0xC), 1, 1, g_brS71.p0AB448);
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x10057BCE -- no text, no cSel++ */
    BR71_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pThis, 80.0f, 46.0f, 9, 2, 5, 0, 7);
    pScr->cCtl++;
}
