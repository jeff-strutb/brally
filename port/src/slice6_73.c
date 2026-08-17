/* slice6_73.c -- BRD3D.dll, packet 73 (slice 6).  See slice6_73.h for the
 * scope, the conflicts and the reasons the other ten addresses are absent.
 *
 * The six screen builders are transcriptions: there is no algorithm in them,
 * and the value is entirely in the coordinates, string ids, flag words, hook
 * addresses and ordering, plus the handful of places the pattern breaks.
 *
 * Float literals are the exact values of the 32-bit patterns the original
 * pushes (195.0f == 0x43430000, 10.0f == 0x41200000, ...).  The row offsets
 * +19/+38/+57/+76/+95/+114/+133/+33 come from the NEGATIVE .rdata constants
 * at 0x1008F680..0x1008F69C, every one of which is used as an `fsub` and
 * therefore ADDS its magnitude; they were read out of orig/BRD3D.dll with
 * tools/pe.py, not assumed.
 *
 * The C++ exception frames the originals set up (`push -1 / push <funclet> /
 * fs:[0]`, plus the state variable each keeps at [esp+0x18] or [esp+0x24])
 * have no observable effect on any path that returns, and are not reproduced.
 */

#include <string.h>
#include <stdio.h>
#include <stddef.h>

#include "slice6_73.h"

/* --- DUPLICATE OWNERSHIP (host link only) -------------------------------
 * slice6_73 and slice6_70 each independently ported 0x1003E680. Both bodies
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
#define BrSub1003E680 BrSub1003E680_dup_73
#define BrExt_1003E680 BrExt_1003E680_dup_73
#endif



/* The module's globals.  Zero-initialised; a caller wires the pointers. */
BrUi73Ctx g_br73;

/* The collision-grid cache slots slice2_11.h does not model. */
uint32_t g_aBrCollGridStamp[BR73_COLL_CELLS];   /* 0x117554C8 */
int16_t  g_aBrCollGridKey[BR73_COLL_CELLS];     /* 0x117554D8 */
uint32_t g_brCollGridClock;                     /* 0x117554E0 */

/* The two table bases the original passes 0x10002DE0 / 0x10002EF0 as globals
 * and slice1_01.c takes as parameters. */
const uint16_t *g_pBrGrid64;                    /* 0x106C7C6C */
const uint16_t *g_pBrTriTable;                  /* 0x106C7C68 */

/* The eight row offsets, as magnitudes.  See the banner. */
#define BR73_ROW_19    19.0f    /* 0x1008F680 */
#define BR73_ROW_38    38.0f    /* 0x1008F684 */
#define BR73_ROW_76    76.0f    /* 0x1008F68C */
#define BR73_ROW_95    95.0f    /* 0x1008F690 */
#define BR73_ROW_114  114.0f    /* 0x1008F694 */
#define BR73_ROW_33    33.0f    /* 0x1008F69C */

/* 0x1008F6A4.  The float nearest -1/11; the original multiplies by it and
 * then SUBTRACTS the product from the low endpoint, so the net effect is a
 * forward interpolation by n/11. */
#define BR73_NEG_RECIP_11  (-0.09090909361839294f)

/* ==========================================================================
 * 0. Small shared helpers
 * ========================================================================== */

/* DEVIATION (memory safety): 0x1003E260 is reached through slice1_06.h's
 * injected host.  An unwired host means "no reporter", where the original
 * always has one; nothing else changes. */
static void Br73Err(int32_t idx)
{
    if (g_br73.pErrHost != NULL) {
        BrErrShow(g_br73.pErrHost, idx);
    }
}

/* The page prologue, identical in all six builders except for the flag value
 * (1 everywhere but 0x10050060's SECOND page) and the two extra hook stores
 * 0x10054B50 makes.
 *
 * GOTCHA reproduced: aFlags[] is written with the count as it stands on
 * entry, the allocation happens next, and apPages[] is then written with the
 * count RE-READ from the object.  Nothing in between can change it, but the
 * original does re-read, and a constructor that published itself could make
 * the two differ.
 *
 * DEVIATION (memory safety, twice): the two array writes are bounded.  The
 * original has the same implicit bounds -- aPages ends where aFlags begins
 * and aFlags ends at the object's 0xC8 bytes -- but does not check them.
 *
 * DEVIATION (memory safety): on allocation failure the original reports error
 * index 4 and then dereferences NULL.  Index 4 is FATAL in g_aBrErrTable, so
 * BrErrShow does not return there in practice; the port returns NULL. */
static BrUiPage_ *Br73PageNew(BrPhase_ *pPhase, int32_t nFlag)
{
    BrUiPage_ *pPage;
    uint16_t   i;

    i = pPhase->nPages;
    if (i < BR_PHASE_PAGES) {
        pPhase->aFlags[i] = nFlag;
    }

    pPage = (BrUiPage_ *)BrOperatorNew(BR73_ALLOC(BrUiPage_, BR73_PAGE_ORIG_SIZE));
    pPage = (pPage != NULL) ? BrUiPageCtor_10048470(pPage) : NULL;

    i = pPhase->nPages;
    if (i < BR_PHASE_PAGES) {
        pPhase->aPages[i] = pPage;
    }
    if (pPage == NULL) {
        Br73Err(4);
    }
    pPhase->nPages++;

    if (pPage == NULL) {
        return NULL;
    }

    pPage->pOwner = pPhase;
    pPage->f10    = 0;
    pPage->fX     = 195.0f;     /* 0x43430000 -- the same in all six */
    pPage->fY     = 130.0f;     /* 0x43020000 -- likewise            */
    return pPage;
}

/* The control prologue, ~80 occurrences.
 *
 * GOTCHA reproduced: the slot is written BEFORE the NULL test, and cCtl is
 * NOT advanced here -- every block bumps it at its end, so a failed
 * allocation leaves a NULL in the array and still moves the cursor on. */
static BrUiCtl_ *Br73CtlNew(BrUiPage_ *pPage)
{
    BrUiCtl_ *pCtl;

    pCtl = (BrUiCtl_ *)BrOperatorNew(BR73_ALLOC(BrUiCtl_, BR73_CTL_ORIG_SIZE));
    pCtl = (pCtl != NULL) ? BrUiCtlCtor(pCtl) : NULL;

    if (pPage->cCtl < BR73_PAGE_CTL_MAX) {
        pPage->apCtl[pPage->cCtl] = pCtl;
    }
    if (pCtl == NULL) {
        Br73Err(4);
    }
    return pCtl;
}

/* Allocate, register and place one control.  a4/a5 are 2 and 5 at every call
 * site in this packet, so they are not parameters. */
static BrUiCtl_ *Br73Ctl(BrUiPage_ *pPage, BrPhase_ *pPhase,
                         float x, float y, int32_t flags,
                         int32_t a6, int32_t a7)
{
    BrUiCtl_ *pCtl = Br73CtlNew(pPage);

    if (pCtl != NULL) {
        pCtl->pVtbl->f38(pCtl, pPhase, x, y, flags, 2, 5, a6, a7);
    }
    return pCtl;
}

/* The label tail: `BrStrGet(id)` then vtable +0x34. */
static void Br73Text(BrUiCtl_ *pCtl, int id, int32_t a2, int32_t a3,
                     const void *pStyle)
{
    pCtl->pVtbl->f34(pCtl, BrStrGet(id), a2, a3, pStyle);
}

/* Shorthand so the transcriptions stay readable.  Relies on the local names
 * pPage / pPhase / pCtl, which every builder declares. */
#define BR73_CTL(x, y, flags, a6, a7)                                        \
    do {                                                                     \
        pCtl = Br73Ctl(pPage, pPhase, (x), (y), (flags), (a6), (a7));        \
        if (pCtl == NULL) { return; }                                        \
    } while (0)

/* ==========================================================================
 * 0x10048710 -- the phase constructor
 * ========================================================================== */

/* WHAT IT DOES: creates the object that holds one whole menu -- its pages,
 * which page is showing, and two lists of a hundred default names each,
 * pre-filled as "Driver 1", "Driver 2" and so on from a pattern in the
 * game's text table. It leaves the "what to do when this menu opens" slot
 * untouched, so it holds rubbish until whoever created the menu fills it in. */
/* @implements 0x10048710 d3d BrOptObjCtor */
BrPhase_ *BrOptObjCtor(BrPhase_ *pThis)
{
    BrNameList *pList;
    const char *pszFmt;
    int         i;
    int         k;

    /* Note the order, and note what is NOT here: +0x04 is never written. */
    pThis->pfnHook = NULL;              /* +0x08 */
    pThis->f0C     = 0;
    pThis->nPages  = 0;
    pThis->iPage   = 0;
    pThis->pCur    = NULL;              /* +0x64 */
    pThis->f68     = 1;
    pThis->fBC     = 0;
    /* DEVIATION: the original stores the literal 0x1008F700.  Its nine slots
     * live in other packets, so the pointer comes from the context instead of
     * being hardcoded.  The store happens at the same point in the sequence. */
    pThis->pVtbl   = g_br73.pPhaseVtbl;

    for (k = 0; k < 2; ++k) {
        pList = (BrNameList *)BrOperatorNew(BR73_ALLOC(BrNameList, 0x6594u));
        pList = (pList != NULL)
                    ? BrNameListInit(pList, g_br73.pNameListVtbl, g_aBr39B720)
                    : NULL;
        if (k == 0) {
            pThis->fC0 = pList;
        } else {
            pThis->fC4 = pList;
        }
        if (pList == NULL) {
            Br73Err(6);
        }
    }

    /* One fill loop drives BOTH lists; the original interleaves them inside a
     * single `esi += 0x104` loop and calls BrStrGet twice per iteration --
     * once for each sprintf -- with the SAME id 0xBE.  100 slots, because
     * 100 * 0x104 == 0x6590 and the loop runs while the byte offset is less
     * than that.
     *
     * DEVIATION (memory safety): a NULL list is skipped rather than
     * dereferenced, and snprintf bounds the slot.  A NULL format is skipped
     * too -- BrStrGet returns NULL for a bad id and the original would hand
     * that straight to sprintf. */
    for (i = 0; i < BR_NAMELIST_COUNT; ++i) {
        pList  = (BrNameList *)pThis->fC0;
        pszFmt = BrStrGet(0xBE);
        if (pList != NULL && pszFmt != NULL) {
            snprintf(pList->asz[i], BR_NAMELIST_STRIDE, pszFmt, i);
        }
        pList  = (BrNameList *)pThis->fC4;
        pszFmt = BrStrGet(0xBE);
        if (pList != NULL && pszFmt != NULL) {
            snprintf(pList->asz[i], BR_NAMELIST_STRIDE, pszFmt, i);
        }
    }

    /* `mov ecx,0x14 / rep stosd` at +0x6C -- twenty dwords, which is what
     * fixes BR_PHASE_PAGES at 20. */
    for (i = 0; i < BR_PHASE_PAGES; ++i) {
        pThis->aFlags[i] = 0;
    }

    return pThis;
}

/* ==========================================================================
 * 0x100558A0 -- 17 controls
 * ========================================================================== */

/* WHAT IT DOES: lays out one of the game's menu screens -- seventeen labels,
 * buttons and pickers, each at a fixed place with a fixed caption and a hook
 * saying what happens when the player chooses it -- and switches the game
 * into the mode that screen belongs to first. There is no logic here beyond
 * the layout; the whole content of the function is the coordinates and the
 * captions. Which screen it is was not established. */
/* @implements 0x100558A0 d3d BrOptFn100558A0 */
void BrOptFn100558A0(BrPhase_ *pSelf)
{
    const BrUi73Hooks  *pH = g_br73.pHooks;
    const BrUi73Styles *pS = &g_br73.aStyles;
    BrPhase_  *pPhase = pSelf;
    BrUiPage_ *pPage;
    BrUiCtl_  *pCtl;

    pPhase->iPage = 0;
    g_br73.n0AA010 = 6;

    pPage = Br73PageNew(pPhase, 1);
    if (pPage == NULL) {
        return;
    }

    /* the unnamed root control -- the owner is the PHASE, not the page */
    BR73_CTL(0.0f, 0.0f, 9, 0, 0);
    pPage->cCtl++;

    /* the title */
    BR73_CTL(pPage->fX, 10.0f, 0x100009, 1, -1);
    pCtl->w1E20C = 3;
    Br73Text(pCtl, 0x54, 1, 1, pS->p0AB508);
    pPage->cCtl++;

    BR73_CTL(pPage->fX, pPage->fY, 0x102001, 1, -1);
    pCtl->pfn0C = pH->p10044030;
    pCtl->pfn08 = pH->p10044010;
    pCtl->w1E20C = 3;
    Br73Text(pCtl, 0x55, 1, 1, pS->p0AB448);
    pPage->cCtl++; pPage->cSel++;

    BR73_CTL(pPage->fX, pPage->fY + BR73_ROW_19, 0x102001, 1, -1);
    pCtl->pfn0C = pH->p10044070;
    pCtl->pfn08 = pH->p10044050;
    pCtl->w1E20C = 3;
    Br73Text(pCtl, 0x56, 1, 1, pS->p0AB448);
    pPage->cCtl++; pPage->cSel++;

    BR73_CTL(pPage->fX, pPage->fY + BR73_ROW_38, 0x102001, 1, -1);
    pCtl->pfn0C = pH->p100440B0;
    pCtl->pfn08 = pH->p10044090;
    pCtl->w1E20C = 3;
    Br73Text(pCtl, 0x57, 1, 1, pS->p0AB448);
    pPage->cCtl++; pPage->cSel++;

    /* the row cursor jumps straight from +38 to +114: the +57, +76 and +95
     * slots are all skipped here */
    BR73_CTL(pPage->fX, pPage->fY + BR73_ROW_114, 0x102001, 1, -1);
    pCtl->pfn0C = pH->p10047360;
    pCtl->pfn08 = pH->p100463C0;
    pCtl->w1E20C = 3;
    Br73Text(pCtl, 0x0C, 1, 1, pS->p0AB448);
    pPage->cCtl++; pPage->cSel++;

    BR73_CTL(95.0f, 317.0f, 9, 0, 0x67);
    pPage->cCtl++;

    BR73_CTL(130.0f, 336.0f, 0x100009, 1, -1);
    pCtl->w1E20C = 0x34;
    Br73Text(pCtl, 0x59, 1, 4, pS->p0AB448);
    pPage->cCtl++;

    BR73_CTL(130.0f, 374.0f, 0x100009, 1, -1);
    pCtl->w1E20C = 3;
    Br73Text(pCtl, 0x5A, 1, 1, pS->p0AB448);
    pPage->cCtl++;

    BR73_CTL(155.0f, 393.0f, 9, 0, 0x39);
    pPage->cCtl++;

    /* the first of the two spinners: text comes from the edit buffer, not the
     * string table, and the rect mirrors at +0x2F80.. get the SAME values in
     * the SAME order as +0x50.. */
    BR73_CTL(155.0f, 393.0f, 0x200001, 1, -1);
    pCtl->pfn08 = pH->p10042AC0;
    pCtl->pfn04 = pH->p1003F050;
    pCtl->pfn10 = pH->p10042AF0;
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, g_aBr39B720, 1, 1, pS->p0AB448);
    if (pCtl->aText[0].pVtbl != NULL) {
        pCtl->aText[0].pVtbl->pfn04(&pCtl->aText[0]);
    }
    pCtl->rcLeft   = 0x9B;  pCtl->aText[0].left  = 0x9B;
    pCtl->rcRight  = 0x15B; pCtl->aText[0].right = 0x15B;
    pCtl->rcTop    = 0x189; pCtl->aText[0].f428  = 0x189;
    pCtl->rcBottom = 0x199; pCtl->aText[0].f430  = 0x199;
    /* the width is computed in 16 bits from the mirrors, then 0x10 is taken
     * off in 32 bits and the result truncated back to 16 */
    pCtl->aText[0].f41C = (int16_t)((int32_t)(uint16_t)((uint16_t)pCtl->aText[0].right
                                       - (uint16_t)pCtl->aText[0].left) - 0x10);
    pPage->cCtl++; pPage->cSel++;

    BR73_CTL(130.0f, 412.0f, 0x100009, 1, -1);
    pCtl->w1E20C = 3;
    Br73Text(pCtl, 0x5B, 1, 1, pS->p0AB448);
    pPage->cCtl++;

    BR73_CTL(155.0f, 431.0f, 9, 0, 0x39);
    pPage->cCtl++;

    BR73_CTL(155.0f, 431.0f, 0x200001, 1, -1);
    pCtl->pfn08 = pH->p10042AC0;
    pCtl->pfn04 = pH->p1003F0B0;
    pCtl->pfn10 = pH->p10042AF0;
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, g_aBr39B720, 1, 1, pS->p0AB448);
    if (pCtl->aText[0].pVtbl != NULL) {
        pCtl->aText[0].pVtbl->pfn04(&pCtl->aText[0]);
    }
    pCtl->rcLeft   = 0x9B;  pCtl->aText[0].left  = 0x9B;
    pCtl->rcRight  = 0x15B; pCtl->aText[0].right = 0x15B;
    pCtl->rcTop    = 0x1AF; pCtl->aText[0].f428  = 0x1AF;
    pCtl->rcBottom = 0x1BF; pCtl->aText[0].f430  = 0x1BF;
    pCtl->aText[0].f41C = (int16_t)((int32_t)(uint16_t)((uint16_t)pCtl->aText[0].right
                                       - (uint16_t)pCtl->aText[0].left) - 0x10);
    pPage->cCtl++; pPage->cSel++;

    BR73_CTL(80.0f, 46.0f, 9, 0, 7);
    pPage->cCtl++;

    /* GOTCHA: f2AB6 gets cCtl + 1, read BEFORE the increment, so it names the
     * slot this control will NOT occupy. */
    BR73_CTL(339.0f, 61.0f, 1, 1, 0x45);
    pCtl->pfn04 = pH->p10040930;
    pCtl->cChild++;
    pCtl->aChild[0] = (uint16_t)(pPage->cCtl + 1);
    pPage->cCtl++;

    BR73_CTL(pPage->fX, 160.0f, 0x101001, 1, -1);
    pCtl->pfn04 = pH->p1003FC40;
    pCtl->w1E20C = 3;
    /* text is 0x100AD300 itself, not a string-table id */
    pCtl->pVtbl->f34(pCtl, pS->p0AD300, 1, 1, pS->p0AB4A8);
    pPage->cCtl++;
}

/* ==========================================================================
 * 0x1004F2B0 -- 6 controls
 * ========================================================================== */

/* WHAT IT DOES: lays out another menu screen the same way -- a title, three
 * choices the player can move between, and two decorations. Which screen it
 * is was not established. */
/* @implements 0x1004F2B0 d3d BrExt_1004F2B0 */
void BrExt_1004F2B0(BrPhase_ *pSelf)
{
    const BrUi73Hooks  *pH = g_br73.pHooks;
    const BrUi73Styles *pS = &g_br73.aStyles;
    BrPhase_  *pPhase = pSelf;
    BrUiPage_ *pPage;
    BrUiCtl_  *pCtl;

    pPhase->iPage = 0;
    pPage = Br73PageNew(pPhase, 1);
    if (pPage == NULL) {
        return;
    }

    BR73_CTL(0.0f, 0.0f, 9, 0, 0);
    pPage->cCtl++;

    BR73_CTL(pPage->fX, 10.0f, 0x100009, 1, -1);
    pCtl->w1E20C = 3;
    Br73Text(pCtl, 9, 1, 1, pS->p0AB508);
    pPage->cCtl++;

    BR73_CTL(pPage->fX, pPage->fY, 0x102001, 1, -1);
    pCtl->pfn0C = pH->p10047360;
    pCtl->pfn08 = pH->p10045AF0;
    pCtl->w1E20C = 3;
    Br73Text(pCtl, 0x0A, 1, 1, pS->p0AB448);
    pPage->cCtl++; pPage->cSel++;

    BR73_CTL(pPage->fX, pPage->fY + BR73_ROW_19, 0x102001, 1, -1);
    pCtl->pfn0C = pH->p10047360;
    pCtl->pfn08 = pH->p10045AA0;
    pCtl->w1E20C = 3;
    Br73Text(pCtl, 0x0B, 1, 1, pS->p0AB448);
    pPage->cCtl++; pPage->cSel++;

    BR73_CTL(pPage->fX, pPage->fY + BR73_ROW_114, 0x102001, 1, -1);
    pCtl->pfn0C = pH->p10047360;
    pCtl->pfn08 = pH->p10046C90;
    pCtl->w1E20C = 3;
    Br73Text(pCtl, 0x0C, 1, 1, pS->p0AB448);
    pPage->cCtl++; pPage->cSel++;

    BR73_CTL(80.0f, 46.0f, 9, 0, 6);
    pPage->cCtl++;
}

/* ==========================================================================
 * 0x1004D640 -- 7 controls
 * ========================================================================== */

/* WHAT IT DOES: lays out another menu screen, seven controls' worth, and
 * hangs on to three of them so that other code can reach back and change them
 * while the screen is up. Which screen it is was not established. */
/* @implements 0x1004D640 d3d BrExt_1004D640 */
void BrExt_1004D640(BrPhase_ *pSelf)
{
    const BrUi73Hooks  *pH = g_br73.pHooks;
    const BrUi73Styles *pS = &g_br73.aStyles;
    BrPhase_  *pPhase = pSelf;
    BrUiPage_ *pPage;
    BrUiCtl_  *pCtl;

    pPhase->iPage = 0;
    pPage = Br73PageNew(pPhase, 1);
    if (pPage == NULL) {
        return;
    }

    BR73_CTL(0.0f, 0.0f, 9, 0, 0);
    pPage->cCtl++;

    BR73_CTL(pPage->fX, 10.0f, 0x100009, 1, -1);
    pCtl->w1E20C = 3;
    Br73Text(pCtl, 0x2B, 1, 1, pS->p0AB508);
    pPage->cCtl++;

    BR73_CTL(80.0f, 46.0f, 9, 0, 9);
    pPage->cCtl++;

    BR73_CTL(96.0f, 61.0f, 9, 0, 0x8E);
    pPage->cCtl++;

    /* the list.  Note a7 is 0 here, not -1. */
    BR73_CTL(pPage->fX, pPage->fY, 0x3001, 1, 0);
    pCtl->pfn04  = pH->p1003ED10;
    pCtl->list.f1A99C[8].i = 1;
    if (pCtl->list.pVtbl != NULL) {
        pCtl->list.pVtbl->f14(&pCtl->list, 0x40001, pS->p0AB528, 7, 0, 0);
    }
    g_br73.pAA29F0 = pCtl;
    pPage->cCtl++; pPage->cSel++;

    BR73_CTL(pPage->fX, pPage->fY + BR73_ROW_95, 0x102001, 1, -1);
    pCtl->pfn0C = pH->p10047360;
    pCtl->pfn08 = pH->p1003ECB0;
    pCtl->w1E20C = 3;
    Br73Text(pCtl, 0x2C, 1, 1, pS->p0AB448);
    g_br73.pAA29C8 = pCtl;
    pPage->cCtl++; pPage->cSel++;

    BR73_CTL(pPage->fX, pPage->fY + BR73_ROW_114, 0x102001, 1, -1);
    pCtl->pfn0C = pH->p10047360;
    pCtl->pfn08 = pH->p10046620;
    pCtl->w1E20C = 3;
    Br73Text(pCtl, 0x0C, 1, 1, pS->p0AB448);
    g_br73.pAA29C8 = pCtl;      /* overwritten, one control after the last */
    pPage->cCtl++; pPage->cSel++;
}

/* ==========================================================================
 * 0x1004DFC0 -- the car screen, 12 controls
 * ========================================================================== */

/* WHAT IT DOES: lays out the car-selection screen -- twelve controls,
 * including the list of car names and a bar whose fill is worked out from
 * which of the twelve cars the cursor is on, so the display slides smoothly
 * as the player scrolls. */
/* @implements 0x1004DFC0 d3d BrExt_1004DFC0 */
void BrExt_1004DFC0(BrPhase_ *pSelf)
{
    const BrUi73Hooks  *pH = g_br73.pHooks;
    const BrUi73Styles *pS = &g_br73.aStyles;
    BrPhase_  *pPhase = pSelf;
    BrUiPage_ *pPage;
    BrUiCtl_  *pCtl;
    int32_t    n;
    int        i;

    pPhase->iPage = 0;
    pPage = Br73PageNew(pPhase, 1);
    if (pPage == NULL) {
        return;
    }

    BR73_CTL(0.0f, 0.0f, 9, 0, 0);
    pPage->cCtl++;

    BR73_CTL(pPage->fX, 10.0f, 0x100009, 1, -1);
    pCtl->w1E20C = 3;
    Br73Text(pCtl, 0x21, 1, 1, pS->p0AB508);
    pPage->cCtl++;

    BR73_CTL(80.0f, 46.0f, 9, 0, 9);
    pPage->cCtl++;

    /* the car list */
    BR73_CTL(pPage->fX, pPage->fY, 0x3001, 1, 0);
    pCtl->pfn04  = pH->p1003EE20;
    pCtl->list.f1A99C[8].i = 1;

    /* first clamp of 0x10AA2A34: `jl -> 0`, `> 0xB -> 0xB`, else keep */
    n = g_br73.nAA2A34;
    if (n < 0) {
        n = 0;
    } else if (n > 0x0B) {
        n = 0x0B;
    }
    if (pCtl->list.pVtbl != NULL) {
        pCtl->list.pVtbl->f14(&pCtl->list, 0x40001, pS->p0AB548, 4, n, -1);
    }
    pCtl->list.f04 = pH->p1004E810;
    /* the original caches the +0x10 slot in a local BEFORE the loop and calls
     * through the cached copy twelve times */
    if (pCtl->list.pVtbl != NULL) {
        for (i = 0; i < 12; ++i) {
            pCtl->list.pVtbl->f10(&pCtl->list, g_br73.apCarName[i],
                                   0, 1, pS->p0AB4D8, 1);
        }
    }

    /* second read of 0x10AA2A34, with a DIFFERENT three-way split.
     * DEVIATION (x87): the original keeps the whole chain in registers.  This
     * note used to call them "80-bit"; they are 53-bit (CRT control word
     * 0x027F -- CONVENTIONS.md), so the chain is exactly a C `double` and the
     * float here rounds at every step.  Left as float pending a re-trace of
     * this chain's spill points; see slice3_44.c for the shape of the fix.
     *
     * Offsets: control +0x1E1E8 / +0x1E200 / +0x1E204 are list.f1A99C[5] /
     * [11] / [12] (br_ui.h ADJ-6).  The original copies the first two arms as
     * RAW DWORDS (`mov ecx,[ebx+0x1E200] / mov [ebx+0x1E1E8],ecx`) and only
     * the third goes through x87, which is why those slots are BrTextWord. */
    n = g_br73.nAA2A34;
    if (n < 0) {
        pCtl->list.f1A99C[5].f = pCtl->list.f1A99C[11].f;
    } else if (n > 0x0B) {
        pCtl->list.f1A99C[5].f = pCtl->list.f1A99C[12].f;
    } else {
        float t = pCtl->list.f1A99C[12].f - pCtl->list.f1A99C[11].f;
        t = t * (float)n;
        t = t * BR73_NEG_RECIP_11;
        pCtl->list.f1A99C[5].f = pCtl->list.f1A99C[11].f - t;    /* fsubr: m32 - st(0) */
    }
    /* control +0x1E1C8 and +0x1E1D0.  br_ui.h's ADJ-6 named these two as the
     * one pair the canonical control could NOT express: they land at list
     * +0x1A990 / +0x1A998, inside the span slice3_39.h left unmodelled. They
     * are modelled there now -- in the object that owns them. */
    pCtl->list.f1A990 = BrFtolTrunc(pCtl->list.f1A99C[5].f);
    pCtl->list.f1A998 = pCtl->list.f1A990 + 0x10;
    pPage->cCtl++; pPage->cSel++;

    BR73_CTL(pPage->fX, pPage->fY + BR73_ROW_76, 0x102001, 1, -1);
    pCtl->pfn0C = pH->p10047360;
    pCtl->pfn08 = pH->p10042CF0;
    pCtl->w1E20C = 3;
    Br73Text(pCtl, 0x2E, 1, 1, pS->p0AB448);
    pPage->cCtl++; pPage->cSel++;

    BR73_CTL(pPage->fX, pPage->fY + BR73_ROW_95, 0x102001, 1, -1);
    pCtl->pfn0C = pH->p10047360;
    pCtl->pfn08 = pH->p10042D60;
    pCtl->w1E20C = 3;
    Br73Text(pCtl, 0x2F, 1, 1, pS->p0AB448);
    pPage->cCtl++; pPage->cSel++;

    BR73_CTL(pPage->fX, pPage->fY + BR73_ROW_114, 0x102001, 1, -1);
    pCtl->pfn0C = pH->p10047360;
    pCtl->pfn08 = pH->p100466C0;
    pCtl->w1E20C = 3;
    Br73Text(pCtl, 0x0C, 1, 1, pS->p0AB448);
    g_br73.pAA29C8 = pCtl;
    pPage->cCtl++; pPage->cSel++;

    BR73_CTL(76.0f, 211.0f, 1, 1, 0x68);
    pCtl->pfn04 = pH->p1003E950;
    pPage->cCtl++;

    BR73_CTL(75.0f, 267.0f, 9, 1, 0x6A);
    pPage->cCtl++;

    BR73_CTL(79.0f, 256.0f, 1, 1, 0x6B);
    pCtl->pfn04 = pH->p1003EA40;
    pPage->cCtl++;

    /* the last two are labels with a +0x04 hook and NO cSel bump */
    BR73_CTL(339.0f, 90.0f, 0x102001, 1, -1);
    pCtl->pfn04 = pH->p1003E980;
    pCtl->w1E20C = 3;
    Br73Text(pCtl, 0x2E, 1, 1, pS->p0AB468);
    pPage->cCtl++;

    BR73_CTL(339.0f, 128.0f, 0x102001, 1, -1);
    pCtl->pfn04 = pH->p1003E9E0;
    pCtl->w1E20C = 3;
    Br73Text(pCtl, 0x2F, 1, 1, pS->p0AB468);
    pPage->cCtl++;
}

/* ==========================================================================
 * 0x10050060 -- the season screen.  TWO pages.
 * ========================================================================== */

/* WHAT IT DOES: lays out the season and track screen. Alone among these
 * builders it makes TWO pages rather than one, and asks the saved-game list
 * for every file matching the season save pattern so the player can pick one.
 * The second page is marked differently from the first, and the page cursor
 * is not rewound before it is built. */
/* @implements 0x10050060 d3d BrExt_10050060 */
void BrExt_10050060(BrPhase_ *pSelf)
{
    const BrUi73Hooks  *pH = g_br73.pHooks;
    const BrUi73Styles *pS = &g_br73.aStyles;
    BrPhase_   *pPhase;
    BrUiPage_  *pPage;
    BrUiCtl_   *pCtl;
    BrNameList *pList;
    int         k;

    /* The prologue runs BEFORE the argument is even loaded: it flips
     * 0x10AA2848 on, refreshes the name list of the 0x10AA2908 phase through
     * that list's own vtable +0x04, and flips it off again. */
    g_br73.nAA2848 = 1;
    if (g_br73.pAA2908 != NULL) {
        pList = (BrNameList *)g_br73.pAA2908->fC0;
        if (pList != NULL && pList->pVtbl != NULL) {
            const BrNameListVtbl_ *pv = (const BrNameListVtbl_ *)pList->pVtbl;
            pv->f04(pList, pS->p0AD348);    /* "RallySeason*.BRF" */
        }
    }
    pPhase = pSelf;
    g_br73.nAA2848 = 0;
    g_br0AB3F4 = -1;

    pPhase->iPage = 0;
    pPage = Br73PageNew(pPhase, 1);
    if (pPage == NULL) {
        return;
    }

    BR73_CTL(0.0f, 0.0f, 9, 0, 0);
    pPage->cCtl++;

    BR73_CTL(pPage->fX, 10.0f, 0x100009, 1, -1);
    pCtl->w1E20C = 3;
    Br73Text(pCtl, 0x39, 1, 1, pS->p0AB508);
    pPage->cCtl++;

    /* the season list: 100 rows straight out of the phase's name list */
    BR73_CTL(pPage->fX, pPage->fY, 0x3001, 1, -1);
    pCtl->pfn04  = pH->p1003EB10;
    pCtl->list.f1A99C[8].i = 1;
    if (pCtl->list.pVtbl != NULL) {
        pCtl->list.pVtbl->f14(&pCtl->list, 0x40001, pS->p0AB4D8, 5, 0, -1);
    }
    pCtl->list.f04 = pH->p10042020;
    pCtl->list.f14 = pH->p10041DF0;
    /* the loop walks a BYTE offset 0..0x6590 in steps of 0x104 -- 100 rows --
     * and re-reads 0x10AA2908 every iteration.  The NULL test is on the
     * COMPUTED slot address, which can only be NULL if the list itself is,
     * so it never fires in the original. */
    for (k = 0; k < BR_NAMELIST_COUNT; ++k) {
        const char *psz = NULL;
        if (g_br73.pAA2908 != NULL) {
            pList = (BrNameList *)g_br73.pAA2908->fC0;
            if (pList != NULL) {
                psz = pList->asz[k];
            }
        }
        if (psz != NULL && pCtl->list.pVtbl != NULL) {
            pCtl->list.pVtbl->f10(&pCtl->list, psz, 0, 1, pS->p0AB4D8, 0);
        }
    }
    pPage->cCtl++; pPage->cSel++;

    /* the one control in the packet with f1E20C == 2 and a3 == 0 */
    BR73_CTL(pPage->fX, pPage->fY + BR73_ROW_95, 0x103011, 1, -1);
    pCtl->pfn0C = pH->p10047360;
    pCtl->pfn08 = pH->p100450F0;
    pCtl->pfn04 = pH->p100418D0;
    pCtl->w1E20C = 2;
    Br73Text(pCtl, 0x1E, 1, 0, pS->p0AB448);
    pPage->cCtl++; pPage->cSel++;

    BR73_CTL(pPage->fX, pPage->fY + BR73_ROW_114, 0x102001, 1, -1);
    pCtl->pfn0C = pH->p10047360;
    pCtl->pfn08 = pH->p10046EB0;
    pCtl->w1E20C = 3;
    Br73Text(pCtl, 0x0C, 1, 1, pS->p0AB448);
    g_br73.pAA29F4 = pCtl;
    pPage->cCtl++; pPage->cSel++;

    BR73_CTL(80.0f, 46.0f, 9, 0, 6);
    pPage->cCtl++;

    BR73_CTL(330.0f, 153.0f, 0x100009, 1, -1);
    pCtl->w1E20C = 3;
    Br73Text(pCtl, 0x36, 1, 1, pS->p0AB468);
    pPage->cCtl++;

    BR73_CTL(330.0f, 97.0f, 0x5001, 1, -1);
    pCtl->pfn04 = pH->p10040A50;
    pCtl->w1E20C = 5;
    pCtl->pVtbl->f34(pCtl, g_aBr39B720, 1, 3, pS->p0AB468);
    pPage->cCtl++;

    BR73_CTL(440.0f, 181.0f, 0x100009, 1, -1);
    pCtl->w1E20C = 3;
    Br73Text(pCtl, 0x37, 1, 1, pS->p0AB488);
    pPage->cCtl++;

    BR73_CTL(440.0f, 129.0f, 0x5001, 1, -1);
    pCtl->pfn04 = pH->p10040AC0;
    pCtl->w1E20C = 5;
    pCtl->pVtbl->f34(pCtl, g_aBr39B720, 1, 3, pS->p0AB488);
    pPage->cCtl++;

    BR73_CTL(440.0f, 243.0f, 0x100009, 1, -1);
    pCtl->w1E20C = 3;
    Br73Text(pCtl, 0x38, 1, 1, pS->p0AB478);
    pPage->cCtl++;

    BR73_CTL(440.0f, 224.0f, 0x5001, 1, -1);
    pCtl->pfn04 = pH->p10041300;
    pCtl->w1E20C = 0x34;
    pCtl->pVtbl->f34(pCtl, g_aBr39B720, 1, 4, pS->p0AB478);
    pPage->cCtl++;

    /* --- the SECOND page.  Its flag is 0, not 1, and iPage is not re-zeroed.
     * The page prologue is otherwise identical, so it goes through the same
     * helper with a different flag. */
    pPage = Br73PageNew(pPhase, 0);
    if (pPage == NULL) {
        return;
    }

    BR73_CTL(0.0f, 232.0f, 0x100009, 1, -1);
    pCtl->pfn0C = pH->p10047360;
    pCtl->pfn04 = pH->p10047210;
    pCtl->pfn14 = pH->p1003E7A0;
    pCtl->w1E20C = 3;
    Br73Text(pCtl, 0x3A, 1, 1, pS->p0AB438);
    g_br73.pAA29C0 = pCtl;
    pPage->cCtl++;      /* no cSel bump */
}

/* ==========================================================================
 * 0x10054B50 -- 20 controls, three of them rectangles
 * ========================================================================== */

/* WHAT IT DOES: lays out the largest of the menu screens -- twenty controls,
 * three of which are drawn boxes rather than text. All three boxes end up
 * sharing a left edge because only the first works one out and the other two
 * reuse it, which is the original's doing and not a simplification here. */
/* @implements 0x10054B50 d3d BrExt_10054B50 */
void BrExt_10054B50(BrPhase_ *pSelf)
{
    const BrUi73Hooks  *pH = g_br73.pHooks;
    const BrUi73Styles *pS = &g_br73.aStyles;
    BrPhase_  *pPhase = pSelf;
    BrUiPage_ *pPage;
    BrUiCtl_  *pCtl;
    float      fRectX;     /* [esp+0x10] -- (float)0x100AB428 */
    float      fRowY;      /* [esp+0x2C] -- (float)0x100AB42C, advanced by 33 */
    int32_t    iRectX = 0; /* ebx  -- __ftol(fRectX), computed ONCE           */
    int32_t    iRectR = 0; /* [esp+0x14] -- iRectX + 0x7F, likewise           */

    pPhase->iPage = 0;
    pPage = Br73PageNew(pPhase, 1);
    if (pPage == NULL) {
        return;
    }
    /* the only builder that gives the page its own hooks */
    pPage->pfn04 = pH->p100409F0;
    pPage->pfn08 = pH->p10040A20;

    BR73_CTL(0.0f, 0.0f, 9, 0, 0);
    pPage->cCtl++;

    BR73_CTL(pPage->fX, 10.0f, 0x100009, 1, -1);
    pCtl->w1E20C = 3;
    Br73Text(pCtl, 0x4F, 1, 1, pS->p0AB508);
    pPage->cCtl++;

    BR73_CTL(pPage->fX, pPage->fY + BR73_ROW_95, 0x102001, 1, -1);
    pCtl->pfn0C = pH->p10047360;
    pCtl->pfn08 = pH->p10047290;
    pCtl->w1E20C = 3;
    Br73Text(pCtl, 0x50, 1, 1, pS->p0AB448);
    pPage->cCtl++; pPage->cSel++;

    BR73_CTL(pPage->fX, pPage->fY + BR73_ROW_114, 0x102001, 1, -1);
    pCtl->pfn0C = pH->p10047360;
    pCtl->pfn08 = pH->p100470E0;
    pCtl->w1E20C = 3;
    Br73Text(pCtl, 0x0C, 1, 1, pS->p0AB448);
    pPage->cCtl++; pPage->cSel++;

    /* both read with `fild`, i.e. they are INTEGER globals */
    fRectX = (float)g_br73.n0AB428;
    fRowY  = (float)g_br73.n0AB42C;

    /* rectangle 1 -- the only one that actually calls __ftol on the x */
    BR73_CTL(fRectX, fRowY, 0x402001, 1, 0x78);
    pCtl->pfn0C = pH->p10047360;
    pCtl->pfn08 = pH->p100458A0;
    pCtl->rcTop = BrFtolTrunc(fRowY);         /* written FIRST, before f50 */
    iRectX    = BrFtolTrunc(fRectX);
    iRectR    = iRectX + 0x7F;
    pCtl->rcLeft = iRectX;
    pCtl->rcRight = iRectR;
    pCtl->rcBottom = pCtl->rcTop + 0x21;
    pCtl->f2968 = 0;
    fRowY += BR73_ROW_33;                   /* the cursor advances in FLOAT */
    pCtl->aStepId[1] = 0x79;
    pPage->cCtl++;

    /* rectangle 2 -- reuses iRectX and iRectR; only the y is recomputed */
    BR73_CTL(fRectX, fRowY, 0x402001, 1, 0x52);
    pCtl->pfn0C = pH->p10047360;
    pCtl->pfn08 = pH->p10043FA0;
    pCtl->rcTop = BrFtolTrunc(fRowY);
    pCtl->rcLeft = iRectX;
    pCtl->rcRight = iRectR;
    pCtl->rcBottom = pCtl->rcTop + 0x21;
    pCtl->f2968 = 0;
    fRowY += BR73_ROW_33;
    pCtl->aStepId[1] = 0x53;
    pPage->cCtl++;

    /* rectangle 3 -- same reuse, and the row cursor is NOT advanced */
    BR73_CTL(fRectX, fRowY, 0x402001, 1, 0x54);
    pCtl->pfn0C = pH->p10047360;
    pCtl->pfn08 = pH->p10045880;
    pCtl->rcTop = BrFtolTrunc(fRowY);
    pCtl->rcLeft = iRectX;
    pCtl->rcRight = iRectR;
    pCtl->rcBottom = pCtl->rcTop + 0x21;
    pCtl->f2968 = 0;
    pCtl->aStepId[1] = 0x55;
    pPage->cCtl++;

    BR73_CTL(106.0f, 85.0f, 0x100001, 1, -1);
    pCtl->w1E20C = 3;
    Br73Text(pCtl, 0x38, 1, 1, pS->p0AB458);
    pPage->cCtl++;

    BR73_CTL(440.0f, 66.0f, 0x5001, 1, -1);
    pCtl->pfn04 = pH->p10041300;
    pCtl->w1E20C = 0x34;
    pCtl->pVtbl->f34(pCtl, g_aBr39B720, 1, 4, pS->p0AB458);
    pPage->cCtl++;

    BR73_CTL(106.0f, 123.0f, 0x100001, 1, -1);
    pCtl->w1E20C = 3;
    Br73Text(pCtl, 0x36, 1, 1, pS->p0AB458);
    pPage->cCtl++;

    BR73_CTL(440.0f, 104.0f, 0x5001, 1, -1);
    pCtl->pfn04 = pH->p100413B0;
    pCtl->w1E20C = 0x34;
    pCtl->pVtbl->f34(pCtl, g_aBr39B720, 1, 4, pS->p0AB458);
    pPage->cCtl++;

    BR73_CTL(0.0f, 70.0f, 0x100009, 1, -1);
    pCtl->w1E20C = 3;
    Br73Text(pCtl, 0x52, 1, 1, pS->p0AB468);
    pPage->cCtl++;

    BR73_CTL(330.0f, 97.0f, 0x5001, 1, -1);
    pCtl->pfn04 = pH->p10041670;
    pCtl->w1E20C = 5;
    pCtl->pVtbl->f34(pCtl, g_aBr39B720, 1, 3, pS->p0AB468);
    pPage->cCtl++;

    BR73_CTL(0.0f, 150.0f, 0x100009, 1, -1);
    pCtl->w1E20C = 3;
    Br73Text(pCtl, 0x53, 1, 1, pS->p0AB468);
    pPage->cCtl++;

    BR73_CTL(450.0f, 125.0f, 0x100009, 1, -1);
    pCtl->w1E20C = 3;
    Br73Text(pCtl, 0x40, 1, 1, pS->p0AB4F8);
    pPage->cCtl++;

    BR73_CTL(450.0f, 185.0f, 0x100009, 1, -1);
    pCtl->w1E20C = 3;
    Br73Text(pCtl, 0x46, 1, 1, pS->p0AB4F8);
    pPage->cCtl++;

    BR73_CTL(450.0f, 141.0f, 0x5001, 1, -1);
    pCtl->pfn04 = pH->p100417B0;
    pCtl->w1E20C = 5;
    pCtl->pVtbl->f34(pCtl, g_aBr39B720, 1, 3, pS->p0AB4F8);
    pPage->cCtl++;

    /* string id 0x40 again, this time against a different style block */
    BR73_CTL(pPage->fX, 203.0f, 0x100009, 1, -1);
    pCtl->w1E20C = 3;
    Br73Text(pCtl, 0x40, 1, 1, pS->p0AB478);
    pPage->cCtl++;

    BR73_CTL(pPage->fX, 265.0f, 0x100009, 1, -1);
    pCtl->w1E20C = 3;
    Br73Text(pCtl, 0x41, 1, 1, pS->p0AB478);
    pPage->cCtl++;

    BR73_CTL(450.0f, 217.0f, 0x5001, 1, -1);
    pCtl->pfn04 = pH->p10041710;
    pCtl->w1E20C = 5;
    pCtl->pVtbl->f34(pCtl, g_aBr39B720, 1, 3, pS->p0AB478);
    pPage->cCtl++;
}

/* ==========================================================================
 * 0x10041A00 / 0x100424D0 -- the name commit / restore pair
 * ========================================================================== */

/* Both reach a record through the SAME index math the original uses:
 * n*3 -> *5 -> *9 -> *8, i.e. a 0x438 stride, and both reach the flag at
 * +0x44C from the same base, which is 0x14 bytes past the end of record n.
 * Reproduced, not corrected -- see slice5_61.h, which found the same thing on
 * the sibling array. */
static unsigned char *Br73Rec(unsigned char *pBase, int32_t n)
{
    /* g_br0AB3F4 is signed and IS set to -1 by the name-reset paths; the
     * original does not guard against it.  Same arithmetic slice5_61.c uses
     * on the sibling array. */
    return pBase + (ptrdiff_t)n * (ptrdiff_t)BR61_REC29D0_STRIDE;
}

/* WHAT IT DOES: starts editing a saved driver's name: it flips that record
 * between "being edited" and "not", and on the way in it puts the existing
 * name aside and blanks the record so the player types into an empty box.
 * The put-aside copy is what the cancel half below puts back. */
/* @implements 0x10041A00 d3d BrExt_10041A00 */
int32_t BrExt_10041A00(void *pArg)
{
    unsigned char *pRec;
    int32_t        fWasZero;
    int32_t        fFlag;

    /* pArg->pSub (+0x2AE8) then that object's +0x70 = 0.  Routed through the
     * hook because slice2_25.h, which models the object, cannot be included
     * here; the effect is preserved, not dropped. */
    if (g_br73.pfnClearSub70 != NULL) {
        g_br73.pfnClearSub70(pArg);
    }

    /* DEVIATION, AND IT IS OURS. The original does not test this pointer:
     * 0x10041A22 loads it and 0x10041A27 indexes straight off it, so a NULL
     * faults. This guard was added without a note, which is what made the
     * round-3 equivalence audit call it DIVERGENT rather than an acceptable
     * deviation -- an undocumented guard is indistinguishable from a
     * misreading, and this project has produced both.
     *
     * It is NOT dead code: slice7_81.c and slice8_84.c each set this pointer
     * to NULL while faithfully reproducing the original's zero-store, so the
     * path is reachable. On it the original faults; this returns 1 AND SKIPS
     * PUBLISHING g_brAA28D8, leaving that latch stale -- a second-order
     * effect beyond suppressing the crash, and the reason the deviation is
     * worth more than a comment.
     *
     * Kept rather than removed, because the pointer genuinely has no owner in
     * this tree yet and the original's fault would be a harness crash rather
     * than reproduced behaviour. When an owner lands, this goes. The sibling
     * transcription of the same routine, BrExt_10042410 in slice5_61.c, has
     * no such guard and should stay that way. */
    if (g_br73.pAA29CC == NULL) {
        return 1;
    }

    pRec = Br73Rec(g_br73.pAA29CC, g_br0AB3F4);

    memcpy(&fFlag, pRec + BR61_REC29D0_OFF_FLAG, sizeof(fFlag));
    fWasZero = (fFlag == 0);
    memcpy(pRec + BR61_REC29D0_OFF_FLAG, &fWasZero, sizeof(fWasZero));

    /* re-read from memory, which is why 0x10AA28D8 is always 0 or 1 */
    memcpy(&fFlag, pRec + BR61_REC29D0_OFF_FLAG, sizeof(fFlag));
    g_brAA28D8 = fFlag;

    if (fFlag != 0) {
        char *pszName = (char *)pRec + BR61_REC29D0_OFF_NAME;
        strcpy(g_aBrA9D078, pszName);       /* DEVIATION: rep movsd/movsb */
        strcpy(pszName, g_aBr39B720);
    }
    return 1;
}

/* WHAT IT DOES: cancels a name edit, putting the name that was set aside back
 * on the record and clearing the edit box. It looks at the flag the commit
 * half above set but writes into a DIFFERENT array of records than that half
 * read from -- an asymmetry that is the original's, not a slip here. */
/* @implements 0x100424D0 d3d BrExt_100424D0 */
int32_t BrExt_100424D0(void *pArg)
{
    unsigned char *pRec;
    char          *pszName;

    if (g_br73.pfnClearSub70 != NULL) {     /* see BrExt_10041A00 */
        g_br73.pfnClearSub70(pArg);
    }

    g_br73.nAA28EC = 0;

    if (g_brAA28D8 == 0) {
        return 1;
    }
    /* the original tests the ADDRESS 0x10A9D078 against zero here; it is a
     * literal, so the branch is dead.  Kept as an always-true condition. */

    if (g_brPAA29D0 == NULL) {
        return 1;
    }
    pRec    = Br73Rec(g_brPAA29D0, g_br0AB3F4);
    pszName = (char *)pRec + BR61_REC29D0_OFF_NAME;

    strcpy(pszName, g_aBrA9D078);
    strcpy(g_aBrA9D078, g_aBr39B720);
    return 1;
}

/* ==========================================================================
 * 0x1003E680 -- the "new session" global reset
 * ========================================================================== */

/* WHAT IT DOES: wipes the slate for a new game -- every choice the player
 * could have made on the setup screens goes back to its default, the "1 of 1"
 * counters are rebuilt, and three large blocks of session state are cleared.
 * It prints the same number into both counter strings, because the value that
 * ought to have made the second one different was zeroed moments before. */
/* @implements 0x1003E680 d3d BrSub1003E680 */
void BrSub1003E680(void)
{
    int i;

    g_br73.nA9D068 = 0;
    g_br73.n0AC648 = 2;
    g_br73.nAA2A00 = 0;
    g_br73.nAA2A04 = 0;
    g_br73.nAA2A08 = 0;
    g_br73.n0AC64C = 1;
    g_br73.n0AC650 = 1;
    g_br73.n0AC654 = 1;
    g_br73.n0AC658 = 3;
    g_br73.nAA2A10 = 0;
    g_br73.nAA2A14 = 0;
    g_br73.nAA28A0 = 0;
    g_br73.nAA28A4 = 0;
    g_br73.nAA28AC = 0;
    g_br73.nAA28B0 = 0;
    g_br73.nAA28B4 = 0;
    g_br73.bAA28B8 = 0;             /* a BYTE store in the original */
    g_br73.nAA28BC = 0;
    g_br73.nAA28C0 = 0;
    g_br73.nAA26E8 = 0;
    g_br73.nA9D06C = 0;
    g_br73.nAA28C4 = 0;
    g_br73.nAA28C8 = 0;
    g_br73.nAA28D0 = 0;
    g_br73.nAA289C = 0;

    /* Two sprintf("%d") calls.  The first takes the literal 1; the second
     * takes 0x10AA28A4 + 1, and 0x10AA28A4 was zeroed above, so it is also 1.
     * DEVIATION: snprintf against the buffer capacity the caller declares. */
    if (g_br73.szAA2518 != NULL && g_br73.cbScratch != 0) {
        snprintf(g_br73.szAA2518, g_br73.cbScratch, "%d", 1);
    }
    if (g_br73.szA9D618 != NULL && g_br73.cbScratch != 0) {
        snprintf(g_br73.szA9D618, g_br73.cbScratch, "%d",
                 (int)(g_br73.nAA28A4 + 1));
    }

    if (g_br73.pPairBuf != NULL) {
        (void)BrPairBufReset(g_br73.pPairBuf);      /* 0x1003E1D0 */
    }

    /* 0x10AA289C is cleared a SECOND time here, with nothing in between that
     * could have changed it. */
    g_br73.nAA289C = 0;

    if (g_br73.aAA26F0 != NULL) {
        for (i = 0; i < 0x53; ++i) { g_br73.aAA26F0[i] = 0; }
    }
    if (g_br73.aA9DBD8 != NULL) {
        for (i = 0; i < 0x53; ++i) { g_br73.aA9DBD8[i] = 0; }
    }
    if (g_br73.a220B20 != NULL) {
        for (i = 0; i < 0x46; ++i) { g_br73.a220B20[i] = 0; }
    }

    /* 0x10AA27E0 LIES INSIDE THE BLOCK JUST ZEROED, and this port modelled the
     * two as disjoint objects so the store was lost. Found by the equivalence
     * audit; verified here against the listing:
     *
     *   1003E758  mov ecx, 0x53              83 dwords
     *   1003E75F  mov edi, 0x10AA26F0
     *   1003E76A  rep stosd                  zeroes 0x10AA26F0 .. 0x10AA283C
     *   1003E784  mov word ptr [0x10AA27E0], 0x102
     *
     *   0x10AA27E0 - 0x10AA26F0 = 0xF0 = dword index 60, and 60 < 83.
     *
     * So the original leaves 0x00000102 at aAA26F0[60] and this port left 0.
     * The word is the LOW half of that dword (little-endian), so the upper 16
     * bits stay as the fill left them -- zero.
     *
     * What makes the miss notable rather than merely unlucky: the very next
     * instruction does the identical thing to 0x10220B20 and THAT one is
     * preserved below, comment and all. The idiom was understood; one of the
     * two instances was modelled as a separate field and the aliasing dropped
     * out with it.
     *
     * wAA27E0 is kept as the named view because 405 lines of this header
     * describe it that way and other code reads it; both are written, which is
     * what the single store in the original actually means for two views of
     * one word. */
    g_br73.wAA27E0 = 0x0102;
    if (g_br73.aAA26F0 != NULL) {
        g_br73.aAA26F0[BR73_AA27E0_INDEX] =
            (g_br73.aAA26F0[BR73_AA27E0_INDEX] & ~0xFFFF) | 0x0102;
    }
    if (g_br73.a220B20 != NULL) {
        g_br73.a220B20[0] = -1;     /* re-dirties the block just cleared */
    }

    BrSub1003E510();                /* 0x1003E510 */
}

/* slice2_26.h wants the same body under a second name.  Both are declared
 * `void (void)`, so this is a naming duplicate, not a mispairing. */
void BrExt_1003E680(void)
{
    BrSub1003E680();
}

/* ==========================================================================
 * 0x1003D030 -- the 16-byte join blob
 * ========================================================================== */

/* WHAT IT DOES: fetches the small identifying blob for the network game the
 * player has highlighted, which is what the join attempt hands to DirectPlay
 * to say which session it wants. It reports success even when there was
 * nothing to fetch, so the caller cannot tell the difference. */
/* @implements 0x1003D030 d3d BrSub1003D030 */
int32_t BrSub1003D030(void *pBlob)
{
    const void *pSrc;

    if (g_br73.apJoinBlob == NULL) {
        return 0;
    }
    pSrc = g_br73.apJoinBlob[g_br73.nAA2880];
    if (pSrc == NULL) {
        return 0;
    }
    /* four dword copies in the original */
    memcpy(pBlob, pSrc, 16);
    return 0;
}

/* ==========================================================================
 * 0x10071550
 * ========================================================================== */

/* WHAT IT DOES: runs two other routines in order and reports success
 * unconditionally. What the two do was not established, so the purpose is
 * unclear; the caller ignores the answer in any case. */
/* @implements 0x10071550 d3d BrSub10071550 */
void BrSub10071550(void)
{
    if (g_br73.pfn10071560 != NULL) { g_br73.pfn10071560(); }
    if (g_br73.pfn10071630 != NULL) { g_br73.pfn10071630(); }
    /* the original returns 1; slice4_50.h declares it void */
}

/* ==========================================================================
 * 0x10031140 -- ADAPTER, not a second body.  See CONFLICT 4 in the header.
 * ========================================================================== */

void BrSub_10031140(BrMat4 *pM, int32_t a, int32_t b, float c)
{
    float tx, ty;

    /* The original copies all three coordinates as raw dwords; slice2_15.h
     * types the first two int32_t because the camera stores them that way.
     * memcpy rather than a union so no aliasing rule is bent. */
    memcpy(&tx, &a, sizeof(tx));
    memcpy(&ty, &b, sizeof(ty));

    BrMat4Translate(pM, tx, ty, c);
}

/* ==========================================================================
 * 0x1006F720 -- load the collision-grid cell containing (x, y)
 * ========================================================================== */

short BrCollGridCellAcquire(float x, float y)
{
    int32_t      ix, iy;
    int16_t      key;
    int          i;
    int          iVictim = 0;
    uint32_t     best    = 0x40000000u;
    BrCollPlane *pCell;
    uint32_t     packed;
    uint16_t     n = 0;
    uint16_t     tri;

    ++g_brCollGridClock;

    /* `__ftol / cdq / and edx,0x1F / add / sar 5` -- a divide by 32 that
     * truncates toward zero, then the y half is scaled by 64 to make a
     * 64-column key. */
    ix = BrFtolTrunc(x);
    ix = (ix + ((ix < 0) ? 31 : 0)) >> 5;
    iy = BrFtolTrunc(y);
    iy = ((iy + ((iy < 0) ? 31 : 0)) >> 5) << 6;
    key = (int16_t)(ix + iy);

    /* GOTCHA, and it is a real defect: the stored key is compared
     * ZERO-extended (`mov bp,[ecx]` into a cleared ebp) against the requested
     * key SIGN-extended (`movsx edx,di`).  Any cell whose key is negative --
     * every cell left of or above the origin -- therefore never matches, and
     * is reloaded on every single query.  Reproduced. */
    for (i = 0; i < BR73_COLL_CELLS; ++i) {
        uint32_t stored = (uint32_t)(uint16_t)g_aBrCollGridKey[i];

        if (stored == (uint32_t)(int32_t)key) {
            g_aBrCollGridStamp[i] = g_brCollGridClock;
            return (short)i;
        }
        if (g_aBrCollGridStamp[i] < best) {
            iVictim = i;
            best    = g_aBrCollGridStamp[i];
        }
    }

    g_aBrCollGridKey[iVictim]   = key;
    g_aBrCollGridStamp[iVictim] = g_brCollGridClock;

    if (g_pBrCollGrid == NULL) {
        return (short)iVictim;
    }
    /* 75 * best << 6 == 4800 * best == BR_COLL_CELL_PLANES records. */
    pCell = g_pBrCollGrid + (size_t)iVictim * BR_COLL_CELL_PLANES;

    /* 0x10002DE0 returns the cell's CSR row: the first triangle index in the
     * low 16 bits and the count in the high 16.  The original keeps the whole
     * dword as a 4-byte cursor and hands its address to 0x10002EF0, which is
     * exactly slice1_01.h's BrU16Cursor {pos, remaining}. */
    packed = BrGrid64Sample(g_pBrGrid64, x, y);
    if (packed != 0) {
        BrU16Cursor cur;

        cur.pos       = (uint16_t)(packed & 0xFFFFu);
        cur.remaining = (uint16_t)(packed >> 16);

        while ((tri = BrU16CursorNext(g_pBrTriTable, &cur)) != 0) {
            BrCollPlane *p = &pCell[n];
            uint32_t     u = (uint32_t)tri;
            BrVec3       a, b, nrm;

            /* three u16 vertex indices on an 8-byte stride */
            p->pV0 = &g_pBrCollVerts[g_pBrCollTriIdx[4u * u + 0u]];
            p->pV1 = &g_pBrCollVerts[g_pBrCollTriIdx[4u * u + 1u]];
            p->pV2 = &g_pBrCollVerts[g_pBrCollTriIdx[4u * u + 2u]];
            p->tri = tri;
            p->flags = (uint8_t)(g_pBrCollTriFlags[u] & 7u);

            a.x = p->pV1->x - p->pV0->x;
            a.y = p->pV1->y - p->pV0->y;
            a.z = p->pV1->z - p->pV0->z;
            b.x = p->pV2->x - p->pV0->x;
            b.y = p->pV2->y - p->pV0->y;
            b.z = p->pV2->z - p->pV0->z;

            /* the plain cross product (V1-V0) x (V2-V0); traced through every
             * fxch, no operand order is guessed */
            nrm.x = a.y * b.z - a.z * b.y;
            nrm.y = a.z * b.x - a.x * b.z;
            nrm.z = a.x * b.y - a.y * b.x;

            /* 0x10074250 == slice1_09.h's BrVec3Normalise, which the original
             * calls on the record itself.  DEVIATION (LP64): BrCollPlane's
             * first three floats are not a BrVec3 in the port, so the vector
             * round-trips through a local.  Note BrVec3Normalise has NO
             * zero-length guard, by design -- a degenerate triangle stores
             * NaNs here exactly as it does in the original. */
            BrVec3Normalise(&nrm);
            p->nx = nrm.x; p->ny = nrm.y; p->nz = nrm.z;

            /* d = -((nx*V0.x + V0.y*ny) + V0.z*nz).  The association is the
             * original's: the x and y terms are summed first. */
            p->d = -((p->nx * p->pV0->x + p->pV0->y * p->ny)
                     + p->pV0->z * p->nz);

            ++n;
            /* DEVIATION (memory safety): the original has no bound here and
             * will run past the cell's 150 records if the row is longer.
             * The port stops at the cell size. */
            if (n >= BR_COLL_CELL_PLANES) {
                break;
            }
        }
    }

    if (g_pBrCollGridCount != NULL) {
        /* DEVIATION: slice2_11.h types this `const uint16_t *` because its
         * own use only reads.  The original WRITES it here.  See CONFLICT 5
         * in slice6_73.h; the const should be dropped at integration. */
        uint16_t *pCount = (uint16_t *)(size_t)g_pBrCollGridCount;
        pCount[iVictim] = n;
    }
    return (short)iVictim;
}
