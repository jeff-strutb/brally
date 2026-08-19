/* br_uiroot.c -- RESPONSIBILITY: menus/ -- the front end: pages, controls,
 * navigation.  This module is Glide 0x100425E0 (D3D 0x100491B0), the ROOT
 * PHASE'S pfnEnter: it builds the main menu page and its sixteen controls.
 * See br_uiroot.h for the full derivation, the control table and the install
 * audit.
 *
 * REFERENCE BINARY: orig/BRGlide.dll.  config/shared.csv classes 0x100425E0
 * `shared` against D3D 0x100491B0 with the same 2,659-byte extent, so the two
 * builds cannot disagree about this body.
 *
 * FLOATS.  Every float instruction here is `fld m32 / fsub m32 / fstp m32` --
 * the non-reversed form, st(0) = st(0) - m32 -- and there is not one `fxch`
 * in the function, so no operand order is ambiguous.  The literals below are
 * the exact values of the 32-bit patterns the original pushes:
 *
 *     0x43430000 == 195.0    0x42FA0000 == 125.0    0x41200000 ==  10.0
 *     0x42A00000 ==  80.0    0x42380000 ==  46.0    0x41E80000 ==  29.0
 *
 * and the six `fsub` operands at 0x10077648..0x1007765C are -19, -38, -57,
 * -76, -95 and -114, read out of BRGlide.dll with tools/pe.py.  They are
 * NEGATIVE, so `fsub` adds a row offset.  See br_uiroot.h.
 *
 * There is no comparison anywhere in this function, so CONVENTIONS.md's
 * unordered-compare rule has nothing to bite on here.
 */

#include <stddef.h>

#include "br_uiroot.h"

/* XSLICE 0x100418C0 == D3D 0x10048470 -- __thiscall page constructor; returns
 * `this`.  Declared rather than included, exactly as slice6_71.h does it: the
 * two headers that own the name (slice3_32.h and slice6_73.h) type it over
 * incompatible page models, and this module uses br_ui.h's. */
extern BrUiPage_ *BrUiPageCtor_10048470(BrUiPage_ *pThis);

/* XSLICE 0x10040B10 == D3D 0x100476C0 -- __thiscall control constructor.
 * tools/isported.py calls 0x10040B10 "NOT PORTED, and not mentioned.  Clean
 * target."  It is neither: tools/whereis.py pairs it with D3D 0x100476C0,
 * which port/src/menus/br_uictl.c implements as BrUiCtlCtor.  Same trap as
 * the hook table in the header. */
extern BrUiCtl_ *BrUiCtlCtor(BrUiCtl_ *pThis);

/* XSLICE 0x1006D280 == D3D 0x10074030 -- the string table.  Already ported in
 * slice4_52.c; ids outside 1..0x12E return NULL and this builder does not
 * check, exactly as every other builder in the corpus does not check. */
extern const char *BrStrGet(int id);

BrUiRootCtx g_brUiRoot;

/* 0x10AC4C58 */
int32_t g_brUiRootStatusIdx;

void BrUiRootResetForTest(void)
{
    g_brUiRootStatusIdx = 0;
}

int BrUiRootCtxComplete(const BrUiRootCtx *pCtx)
{
    return pCtx != NULL
        && pCtx->pHooks       != NULL
        && pCtx->pErrHost     != NULL
        && pCtx->pStyleTitle  != NULL
        && pCtx->pStyleRow    != NULL
        && pCtx->pStyleStatus != NULL
        && pCtx->pszStatus    != NULL;
}

/* ==========================================================================
 * 0x100425F9 .. 0x10042685 -- the page
 *
 * The original's ORDER, which is the only thing in here that is not obvious:
 * the page-count word is read BEFORE the allocation (for aFlags) and RE-READ
 * after it (for aPages); the aPages store happens BEFORE the null test; the
 * error report happens BEFORE the count is bumped; and the count is bumped
 * even when the allocation failed.
 *
 * DEVIATION (memory safety, two places): the two array writes are bounded.
 * The original has the same implicit bounds -- br_phase.h's aPages ends
 * exactly where aFlags begins -- and does not check them.
 *
 * DEVIATION (memory safety): on failure the original reports error index 4
 * and then dereferences NULL at `mov [esi+0x340], edi`.  Index 4 is FATAL in
 * slice1_06.c's g_aBrErrTable, so BrErrShow does not return there in the
 * shipped build; the port returns instead of faulting.  Same choice, same
 * reason, as slice6_71.c's Br71PageNew.
 * ========================================================================== */
static BrUiPage_ *BrUiRootPageNew(BrPhase_ *pPhase)
{
    BrUiPage_ *pScr;
    uint16_t   i;

    i = pPhase->nPages;                       /* 0x10042601 */
    pPhase->iPage = 0;                        /* 0x1004260A */
    if (i < BR_PHASE_PAGES) {
        pPhase->aFlags[i] = 1;                /* 0x10042613, ebp == 1 */
    }

    pScr = (BrUiPage_ *)BrOperatorNew(BR_UI_PAGE_ALLOC_SIZE);   /* push 0x348 */
    pScr = (pScr != NULL) ? BrUiPageCtor_10048470(pScr) : NULL;

    i = pPhase->nPages;                       /* 0x1004263C -- a RE-READ */
    if (i < BR_PHASE_PAGES) {
        pPhase->aPages[i] = pScr;             /* 0x1004264F, before the test */
    }
    if (pScr == NULL) {
        BrErrShow(g_brUiRoot.pErrHost, BR_UIROOT_ERR_ALLOC);   /* 0x10042657 */
    }
    pPhase->nPages++;                         /* 0x1004265F, on both paths */

    if (pScr == NULL) {
        return NULL;                          /* DEVIATION: see above */
    }

    pScr->pOwner = pPhase;                    /* 0x10042668 */
    pScr->f10    = 0;                         /* 0x1004266E */
    pScr->fX     = BR_UIROOT_PAGE_X;          /* 0x10042671  0x43430000 */
    pScr->fY     = BR_UIROOT_PAGE_Y;          /* 0x1004267B  0x42FA0000 */
    return pScr;
}

/* The control prologue that fifteen of the sixteen allocations share: the
 * store into apCtl[cCtl] happens BEFORE the null test, and cCtl is NOT bumped
 * here (each caller does it at the point the original does).  Same two
 * DEVIATIONs as the page. */
static BrUiCtl_ *BrUiRootCtlNew(BrUiPage_ *pScr)
{
    BrUiCtl_ *pCtl;

    pCtl = (BrUiCtl_ *)BrOperatorNew(BR_UI_CTL_ALLOC_SIZE);   /* push 0x1E214 */
    pCtl = (pCtl != NULL) ? BrUiCtlCtor(pCtl) : NULL;

    if (pScr->cCtl < BR_UI_PAGE_CTL_MAX) {
        pScr->apCtl[pScr->cCtl] = pCtl;
    }
    if (pCtl == NULL) {
        BrErrShow(g_brUiRoot.pErrHost, BR_UIROOT_ERR_ALLOC);
    }
    return pCtl;
}

#define BR_UIROOT_NEW_CTL()                             \
    do {                                                \
        pCtl = BrUiRootCtlNew(pScr);                    \
        if (pCtl == NULL) { return; }                   \
    } while (0)

/* ==========================================================================
 * One menu row.  Seven copies in the original, differing only in y, caption
 * id and the two hooks -- and in whether the aChild link is written at all.
 *
 * The order inside is the original's and is checkable: place, then the two
 * hooks (+0x0C BEFORE +0x08 at every site), then w1E20C, then the caption,
 * then cChild++ BEFORE aChild[0]=, then cCtl++, then cSel++.
 * ========================================================================== */
static void BrUiRootRow(BrUiCtl_ *pCtl, BrUiPage_ *pScr, BrPhase_ *pPhase,
                        float y, BrUiCtlHookFn_ pfn0C, BrUiCtlHookFn_ pfn08,
                        int idStr, int fChild)
{
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, y, 0x102001, 2, 5, 1, -1);
    pCtl->pfn0C  = pfn0C;
    pCtl->pfn08  = pfn08;
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(idStr), 1, 1, g_brUiRoot.pStyleRow);

    if (fChild) {
        /* `mov dx,[esi+0x14] / inc dx / inc [ebp+0x2AB4] / mov [ebp+0x2AB6],dx`
         * -- cCtl + 1 is computed from the count BEFORE it is bumped. */
        int16_t iNext = (int16_t)(pScr->cCtl + 1);
        pCtl->cChild++;
        pCtl->aChild[0] = iNext;
    }
    pScr->cCtl++;
    pScr->cSel++;
}

/* One highlight: 0x809, always at (80, 46), a6 = 0, a7 = the sprite. */
static void BrUiRootHilite(BrUiCtl_ *pCtl, BrUiPage_ *pScr, BrPhase_ *pPhase,
                           int32_t iSprite)
{
    pCtl->pVtbl->f38(pCtl, pPhase, 80.0f, 46.0f, 0x809, 2, 5, 0, iSprite);
    pScr->cCtl++;
}

/* ==========================================================================
 * 0x100425E0
 * ========================================================================== */
/* @implements 0x100425E0 glide BrUiRootEnter_100425E0 */
void BrUiRootEnter_100425E0(BrPhase_ *pPhase)
{
    const BrUiRootHooks *pH;
    BrUiPage_           *pScr;
    BrUiCtl_            *pCtl;

    /* PORT-ONLY refusal, with no counterpart in the original.  br_boot.h's
     * BrRallyMainOps sets the precedent: a caller that supplies nothing must
     * not receive a plausible main menu. */
    if (!BrUiRootCtxComplete(&g_brUiRoot)) {
        return;
    }
    pH = g_brUiRoot.pHooks;

    pScr = BrUiRootPageNew(pPhase);
    if (pScr == NULL) {
        return;
    }

    /* --- apCtl[0], 0x10042685: the backdrop.  a7 = 0. ------------------- */
    BR_UIROOT_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 0.0f, 0.0f, 9, 2, 5, 0, 0);
    pScr->cCtl++;                                        /* 0x100426E0 */

    /* --- apCtl[199], 0x100426E4: THE CURSOR ----------------------------
     * Stored at page +0x334, which is the LAST apCtl slot, and cCtl is NOT
     * bumped -- so the page frame never reaches it.  Glide 0x10041D10 drives
     * it by hand from the mouse position every frame.  See br_uiroot.h. */
    pCtl = (BrUiCtl_ *)BrOperatorNew(BR_UI_CTL_ALLOC_SIZE);
    pCtl = (pCtl != NULL) ? BrUiCtlCtor(pCtl) : NULL;
    pScr->apCtl[BR_UIROOT_CURSOR_SLOT] = pCtl;           /* 0x1004271F */
    if (pCtl == NULL) {
        BrErrShow(g_brUiRoot.pErrHost, BR_UIROOT_ERR_ALLOC);
        return;                                          /* DEVIATION */
    }
    pCtl->pVtbl->f38(pCtl, pPhase, 0.0f, 0.0f, 9, 2, 5, 0, 1);
    /* no cCtl++ -- the original has none here */

    /* --- apCtl[1], 0x10042745: the title, "Main Menu" ------------------- */
    BR_UIROOT_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, 10.0f, 0x100009, 2, 5, 1, -1);
    pCtl->w1E20C = 3;                                    /* 0x100427C3 */
    pCtl->pVtbl->f34(pCtl, BrStrGet(1), 1, 1, g_brUiRoot.pStyleTitle);
    pScr->cCtl++;                                        /* 0x100427DA */

    /* --- apCtl[2], 0x100427DE: row 0, "Championship", y == fY ----------- */
    BR_UIROOT_NEW_CTL();
    BrUiRootRow(pCtl, pScr, pPhase, pScr->fY,
                pH->p100407B0, pH->p1003ED90, 2, 1);

    /* --- apCtl[3], 0x100428A7: row 0's highlight, sprite 6 -------------- */
    BR_UIROOT_NEW_CTL();
    BrUiRootHilite(pCtl, pScr, pPhase, 6);

    /* --- apCtl[4], 0x1004291B: row 1, "Multiplayer" --------------------- */
    BR_UIROOT_NEW_CTL();
    BrUiRootRow(pCtl, pScr, pPhase, pScr->fY - (-19.0f),
                pH->p10040AF0, pH->p1003D140, 3, 1);

    /* --- apCtl[5], 0x100429E4: row 1's highlight, sprite 7 -------------- */
    BR_UIROOT_NEW_CTL();
    BrUiRootHilite(pCtl, pScr, pPhase, 7);

    /* --- apCtl[6], 0x10042A58: row 2, "Time Attack" --------------------- */
    BR_UIROOT_NEW_CTL();
    BrUiRootRow(pCtl, pScr, pPhase, pScr->fY - (-38.0f),
                pH->p10040AF0, pH->p1003E0E0, 4, 1);

    /* --- apCtl[7], 0x10042B21: row 2's highlight, sprite 8 -------------- */
    BR_UIROOT_NEW_CTL();
    BrUiRootHilite(pCtl, pScr, pPhase, 8);

    /* --- apCtl[8], 0x10042B95: row 3, "Quick Race" ---------------------- */
    BR_UIROOT_NEW_CTL();
    BrUiRootRow(pCtl, pScr, pPhase, pScr->fY - (-57.0f),
                pH->p100407B0, pH->p1003E4A0, 5, 1);

    /* --- apCtl[9], 0x10042C5E: row 3's highlight.  SPRITE 0xA, NOT 9.
     * The two out-of-order indices are the original's; see br_uiroot.h. --- */
    BR_UIROOT_NEW_CTL();
    BrUiRootHilite(pCtl, pScr, pPhase, 0xA);

    /* --- apCtl[10], 0x10042CD2: row 4, "Options" ------------------------ */
    BR_UIROOT_NEW_CTL();
    BrUiRootRow(pCtl, pScr, pPhase, pScr->fY - (-76.0f),
                pH->p10040AF0, pH->p1003E730, 6, 1);

    /* --- apCtl[11], 0x10042D9B: row 4's highlight, SPRITE 9 ------------- */
    BR_UIROOT_NEW_CTL();
    BrUiRootHilite(pCtl, pScr, pPhase, 9);

    /* --- apCtl[12], 0x10042E0F: row 5, "Credits" ------------------------
     * It writes the aChild link like every row above it, and the control
     * created next is the "Quit" ROW rather than a highlight.  Transcribed,
     * not corrected -- see OBSERVATION 2 in br_uiroot.h. */
    BR_UIROOT_NEW_CTL();
    BrUiRootRow(pCtl, pScr, pPhase, pScr->fY - (-95.0f),
                pH->p10040A20, pH->p1003AED0, 7, 1);

    /* --- apCtl[13], 0x10042ED8: row 6, "Quit".  NO aChild link. --------- */
    BR_UIROOT_NEW_CTL();
    BrUiRootRow(pCtl, pScr, pPhase, pScr->fY - (-114.0f),
                pH->p10040AF0, pH->p1003F610, 8, 0);

    /* --- apCtl[14], 0x10042F91: the status line ------------------------- */
    BR_UIROOT_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, 29.0f, 0x100009, 2, 5, 1, -1);
    pCtl->w1E20C = 3;                                    /* 0x1004300F */
    /* A literal .data pointer, not BrStrGet. */
    pCtl->pVtbl->f34(pCtl, g_brUiRoot.pszStatus, 1, 1, g_brUiRoot.pStyleStatus);

    /* 0x10043027 -- published BEFORE cCtl is bumped, so it names THIS
     * control.  Glide 0x1003AF30 reads it back. */
    g_brUiRootStatusIdx = (int32_t)pScr->cCtl;
    pScr->cCtl++;                                        /* 0x1004302D */

    /* The sole exit sets eax = 1 and every caller discards it; the port's
     * return type is void.  See THE RETURN VALUE in br_uiroot.h. */
}
