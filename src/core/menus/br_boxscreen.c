/* br_boxscreen.c -- menus: the twenty-control screen with three boxes.
 *
 * BrExt_10054B50 builds the largest of the menu screens: twenty controls,
 * three of them drawn rectangles sharing one left edge.  The Br73
 * construction kit (static __inline in the matching build) is carried here as
 * a copy of the one slice6_73.c's other builders use.
 *
 * Filed out of the address batch slice6_73.c, whose preamble is carried
 * verbatim below.
 */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
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
/* Under the matching build the whole Br73 construction kit is __inline and
 * spelled to the GLIDE original's exact shape (see the Br72 twin in
 * slice6_72.c): the original inlines every helper, stores unchecked, calls
 * the one-argument fatal 0x100378C0 error routine directly, and falls through
 * after it. */
#ifdef BR_MATCHING_BUILD
#define BR73_HELPER static __inline
#define BR73_NEW_RAW FUN_10074572
extern void FUN_100378c0(int32_t); /* fatal error routine */
extern void *FUN_10074572(uint32_t);   /* the EH-aware operator new the original calls */ /* the fatal error routine, as the original calls it */
/* the same two ctors slice6_72 calls direct (matching-only externs, so the
 * two TUs never co-link these) */
extern BrUiPage_ * BR_THISCALL1 FUN_100418c0(BrUiPage_ *);
extern BrUiCtl_  * BR_THISCALL1 FUN_10040b10(BrUiCtl_ *);
static __inline void Br73Err(int32_t idx)
{
    FUN_100378c0(idx);   /* one arg, as 0x100378C0 takes */
}
#else
#define BR73_HELPER static
#define BR73_NEW_RAW BrOperatorNew
static void Br73Err(int32_t idx)
{
    if (g_br73.pErrHost != NULL) {
        BrErrShow(g_br73.pErrHost, idx);
    }
}
#endif

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
BR73_HELPER BrUiPage_ *Br73PageNew(BrPhase_ *pPhase, int32_t nFlag)
{
    BrUiPage_ *pPage;
    uint16_t   i;

    i = pPhase->nPages;
#ifndef BR_MATCHING_BUILD
    if (i < BR_PHASE_PAGES)
#endif
    {
        pPhase->aFlags[i] = nFlag;
    }

    pPage = (BrUiPage_ *)BR73_NEW_RAW(BR73_ALLOC(BrUiPage_, BR73_PAGE_ORIG_SIZE));
#ifdef BR_MATCHING_BUILD
    pPage = (pPage != NULL) ? FUN_100418c0(pPage) : NULL;  /* direct, as orig */
#else
    pPage = (pPage != NULL) ? BrUiPageCtor_10048470(pPage) : NULL;
#endif

    i = pPhase->nPages;
#ifndef BR_MATCHING_BUILD
    if (i < BR_PHASE_PAGES)
#endif
    {
        pPhase->aPages[i] = pPage;
    }
    if (pPage == NULL) {
        Br73Err(4);
    }
    pPhase->nPages++;

#ifndef BR_MATCHING_BUILD
    /* Matching build falls through like the original: Br73Err(4) is fatal. */
    if (pPage == NULL) {
        return NULL;
    }
#endif

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
BR73_HELPER BrUiCtl_ *Br73CtlNew(BrUiPage_ *pPage)
{
    BrUiCtl_ *pCtl;

    pCtl = (BrUiCtl_ *)BR73_NEW_RAW(BR73_ALLOC(BrUiCtl_, BR73_CTL_ORIG_SIZE));
#ifdef BR_MATCHING_BUILD
    pCtl = (pCtl != NULL) ? FUN_10040b10(pCtl) : NULL;     /* direct, as orig */
#else
    pCtl = (pCtl != NULL) ? BrUiCtlCtor(pCtl) : NULL;
#endif

#ifdef BR_MATCHING_BUILD
    pPage->apCtl[pPage->cCtl] = pCtl;      /* unchecked, as the original */
#else
    if (pPage->cCtl < BR73_PAGE_CTL_MAX) {
        pPage->apCtl[pPage->cCtl] = pCtl;
    }
#endif
    if (pCtl == NULL) {
        Br73Err(4);
    }
    return pCtl;
}

/* Allocate, register and place one control.  a4/a5 are 2 and 5 at every call
 * site in this packet, so they are not parameters. */
BR73_HELPER BrUiCtl_ *Br73Ctl(BrUiPage_ *pPage, BrPhase_ *pPhase,
                         float x, float y, int32_t flags,
                         int32_t a6, int32_t a7)
{
    BrUiCtl_ *pCtl = Br73CtlNew(pPage);

#ifndef BR_MATCHING_BUILD
    if (pCtl != NULL)
#endif
    {
        /* matching: unconditional, as the original -- the error path above
         * never returns in practice */
        pCtl->pVtbl->f38(pCtl, pPhase, x, y, flags, 2, 5, a6, a7);
    }
    return pCtl;
}

/* The label tail: `BrStrGet(id)` then vtable +0x34. */
BR73_HELPER void Br73Text(BrUiCtl_ *pCtl, int id, int32_t a2, int32_t a3,
                     const void *pStyle)
{
    pCtl->pVtbl->f34(pCtl, BrStrGet(id), a2, a3, pStyle);
}

/* Shorthand so the transcriptions stay readable.  Relies on the local names
 * pPage / pPhase / pCtl, which every builder declares. */
#ifdef BR_MATCHING_BUILD
/* fully macro-expanded: VC5's inline budget gave up on four of the twenty
 * __inline sites, and a call to our own static helper cannot place */
#define BR73_CTL(x, y, flags, a6, a7)                                        \
    do {                                                                     \
        pCtl = (BrUiCtl_ *)BR73_NEW_RAW(BR73_ALLOC(BrUiCtl_,                \
                                                    BR73_CTL_ORIG_SIZE));    \
        pCtl = (pCtl != NULL) ? FUN_10040b10(pCtl) : NULL;                   \
        pPage->apCtl[pPage->cCtl] = pCtl;                                    \
        if (pCtl == NULL) { FUN_100378c0(4); }                               \
        pCtl->pVtbl->f38(pCtl, pPhase, (x), (y), (flags), 2, 5,              \
                         (a6), (a7));                                        \
    } while (0)
#else
#define BR73_CTL(x, y, flags, a6, a7)                                        \
    do {                                                                     \
        pCtl = Br73Ctl(pPage, pPhase, (x), (y), (flags), (a6), (a7));        \
        if (pCtl == NULL) { return; }                                        \
    } while (0)
#endif

/* ==========================================================================
 * 0x10054B50 -- 20 controls, three of them rectangles
 * ========================================================================== */

/* WHAT IT DOES: lays out the largest of the menu screens -- twenty controls,
 * three of which are drawn boxes rather than text. All three boxes end up
 * sharing a left edge because only the first works one out and the other two
 * reuse it, which is the original's doing and not a simplification here. */

/* ===== matching-build immediates for BrExt_10054B50 =====================
 * Same story and same verification as slice6_72's block: the original
 * stores hooks and pushes styles as immediates; the env/g_br73 routing is
 * port scaffolding the image never initialises.  Three hooks recur in BOTH
 * builders with identical glide values (p10043FA0, p10041300, p100413B0),
 * which pins the sequence mapping independently. */
#ifdef BR_MATCHING_BUILD
extern void FUN_10039f30(void); extern void FUN_10039f60(void);
extern void FUN_100406e0(void); extern void FUN_10040530(void);
extern void FUN_1003ed30(void); extern void FUN_1003ed10(void);
extern void FUN_1003abd0(void); extern void FUN_1003ad10(void);
extern void FUN_1003ac70(void);
extern void FUN_100407b0(void); extern void FUN_1003d4f0(void);
extern void FUN_1003a860(void); extern void FUN_1003a910(void);
extern unsigned char DAT_100aabe8, DAT_100aabf8, DAT_100aac08, DAT_100aac18,
                     DAT_100aac98, DAT_100aaca8;
extern int32_t DAT_100aabc8, DAT_100aabcc;
#define BR73H(f) ((BrUiCtlHookFn_)&f)
#define pH73_p100409F0  BR73H(FUN_10039f30)
#define pH73_p10040A20  BR73H(FUN_10039f60)
#define pH73_p10047360  BR73H(FUN_100407b0)
#define pH73_p10047290  BR73H(FUN_100406e0)
#define pH73_p100470E0  BR73H(FUN_10040530)
#define pH73_p100458A0  BR73H(FUN_1003ed30)
#define pH73_p10043FA0  BR73H(FUN_1003d4f0)
#define pH73_p10045880  BR73H(FUN_1003ed10)
#define pH73_p10041300  BR73H(FUN_1003a860)
#define pH73_p100413B0  BR73H(FUN_1003a910)
#define pH73_p10041670  BR73H(FUN_1003abd0)
#define pH73_p100417B0  BR73H(FUN_1003ad10)
#define pH73_p10041710  BR73H(FUN_1003ac70)
#define pS73_p0AB448    ((const void *)&DAT_100aabe8)
#define pS73_p0AB458    ((const void *)&DAT_100aabf8)
#define pS73_p0AB468    ((const void *)&DAT_100aac08)
#define pS73_p0AB478    ((const void *)&DAT_100aac18)
#define pS73_p0AB4F8    ((const void *)&DAT_100aac98)
#define pS73_p0AB508    ((const void *)&DAT_100aaca8)
#define g73_n0AB428     DAT_100aabc8
#define g73_n0AB42C     DAT_100aabcc
#else
#define pH73_p100409F0  (pH->p100409F0)
#define pH73_p10040A20  (pH->p10040A20)
#define pH73_p10047360  (pH->p10047360)
#define pH73_p10047290  (pH->p10047290)
#define pH73_p100470E0  (pH->p100470E0)
#define pH73_p100458A0  (pH->p100458A0)
#define pH73_p10043FA0  (pH->p10043FA0)
#define pH73_p10045880  (pH->p10045880)
#define pH73_p10041300  (pH->p10041300)
#define pH73_p100413B0  (pH->p100413B0)
#define pH73_p10041670  (pH->p10041670)
#define pH73_p100417B0  (pH->p100417B0)
#define pH73_p10041710  (pH->p10041710)
#define pS73_p0AB448    (pS->p0AB448)
#define pS73_p0AB458    (pS->p0AB458)
#define pS73_p0AB468    (pS->p0AB468)
#define pS73_p0AB478    (pS->p0AB478)
#define pS73_p0AB4F8    (pS->p0AB4F8)
#define pS73_p0AB508    (pS->p0AB508)
#define g73_n0AB428     (g_br73.n0AB428)
#define g73_n0AB42C     (g_br73.n0AB42C)
#endif

/* WHAT IT DOES: lays out the largest of the menu screens -- twenty controls,
 * three of them drawn boxes that share a left edge because only the first
 * works one out (the original's doing; the fuller description is with the
 * dossier at the head of this section). */
/* @implements 0x10054B50 d3d BrExt_10054B50 */
#ifdef BR_MATCHING_BUILD
int32_t BrExt_10054B50(BrPhase_ *pSelf)
#else
void BrExt_10054B50(BrPhase_ *pSelf)
#endif
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
    pPage->pfn04 = pH73_p100409F0;
    pPage->pfn08 = pH73_p10040A20;

    BR73_CTL(0.0f, 0.0f, 9, 0, 0);
    pPage->cCtl++;

    BR73_CTL(pPage->fX, 10.0f, 0x100009, 1, -1);
    pCtl->w1E20C = 3;
    Br73Text(pCtl, 0x4F, 1, 1, pS73_p0AB508);
    pPage->cCtl++;

    BR73_CTL(pPage->fX, pPage->fY + BR73_ROW_95, 0x102001, 1, -1);
    pCtl->pfn0C = pH73_p10047360;
    pCtl->pfn08 = pH73_p10047290;
    pCtl->w1E20C = 3;
    Br73Text(pCtl, 0x50, 1, 1, pS73_p0AB448);
    pPage->cCtl++; pPage->cSel++;

    BR73_CTL(pPage->fX, pPage->fY + BR73_ROW_114, 0x102001, 1, -1);
    pCtl->pfn0C = pH73_p10047360;
    pCtl->pfn08 = pH73_p100470E0;
    pCtl->w1E20C = 3;
    Br73Text(pCtl, 0x0C, 1, 1, pS73_p0AB448);
    pPage->cCtl++; pPage->cSel++;

    /* both read with `fild`, i.e. they are INTEGER globals */
    fRectX = (float)g73_n0AB428;
    fRowY  = (float)g73_n0AB42C;

    /* rectangle 1 -- the only one that actually calls __ftol on the x */
    BR73_CTL(fRectX, fRowY, 0x402001, 1, 0x78);
    pCtl->pfn0C = pH73_p10047360;
    pCtl->pfn08 = pH73_p100458A0;
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
    pCtl->pfn0C = pH73_p10047360;
    pCtl->pfn08 = pH73_p10043FA0;
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
    pCtl->pfn0C = pH73_p10047360;
    pCtl->pfn08 = pH73_p10045880;
    pCtl->rcTop = BrFtolTrunc(fRowY);
    pCtl->rcLeft = iRectX;
    pCtl->rcRight = iRectR;
    pCtl->rcBottom = pCtl->rcTop + 0x21;
    pCtl->f2968 = 0;
    pCtl->aStepId[1] = 0x55;
    pPage->cCtl++;

    BR73_CTL(106.0f, 85.0f, 0x100001, 1, -1);
    pCtl->w1E20C = 3;
    Br73Text(pCtl, 0x38, 1, 1, pS73_p0AB458);
    pPage->cCtl++;

    BR73_CTL(440.0f, 66.0f, 0x5001, 1, -1);
    pCtl->pfn04 = pH73_p10041300;
    pCtl->w1E20C = 0x34;
    pCtl->pVtbl->f34(pCtl, g_aBr39B720, 1, 4, pS73_p0AB458);
    pPage->cCtl++;

    BR73_CTL(106.0f, 123.0f, 0x100001, 1, -1);
    pCtl->w1E20C = 3;
    Br73Text(pCtl, 0x36, 1, 1, pS73_p0AB458);
    pPage->cCtl++;

    BR73_CTL(440.0f, 104.0f, 0x5001, 1, -1);
    pCtl->pfn04 = pH73_p100413B0;
    pCtl->w1E20C = 0x34;
    pCtl->pVtbl->f34(pCtl, g_aBr39B720, 1, 4, pS73_p0AB458);
    pPage->cCtl++;

    BR73_CTL(0.0f, 70.0f, 0x100009, 1, -1);
    pCtl->w1E20C = 3;
    Br73Text(pCtl, 0x52, 1, 1, pS73_p0AB468);
    pPage->cCtl++;

    BR73_CTL(330.0f, 97.0f, 0x5001, 1, -1);
    pCtl->pfn04 = pH73_p10041670;
    pCtl->w1E20C = 5;
    pCtl->pVtbl->f34(pCtl, g_aBr39B720, 1, 3, pS73_p0AB468);
    pPage->cCtl++;

    BR73_CTL(0.0f, 150.0f, 0x100009, 1, -1);
    pCtl->w1E20C = 3;
    Br73Text(pCtl, 0x53, 1, 1, pS73_p0AB468);
    pPage->cCtl++;

    BR73_CTL(450.0f, 125.0f, 0x100009, 1, -1);
    pCtl->w1E20C = 3;
    Br73Text(pCtl, 0x40, 1, 1, pS73_p0AB4F8);
    pPage->cCtl++;

    BR73_CTL(450.0f, 185.0f, 0x100009, 1, -1);
    pCtl->w1E20C = 3;
    Br73Text(pCtl, 0x46, 1, 1, pS73_p0AB4F8);
    pPage->cCtl++;

    BR73_CTL(450.0f, 141.0f, 0x5001, 1, -1);
    pCtl->pfn04 = pH73_p100417B0;
    pCtl->w1E20C = 5;
    pCtl->pVtbl->f34(pCtl, g_aBr39B720, 1, 3, pS73_p0AB4F8);
    pPage->cCtl++;

    /* string id 0x40 again, this time against a different style block */
    BR73_CTL(pPage->fX, 203.0f, 0x100009, 1, -1);
    pCtl->w1E20C = 3;
    Br73Text(pCtl, 0x40, 1, 1, pS73_p0AB478);
    pPage->cCtl++;

    BR73_CTL(pPage->fX, 265.0f, 0x100009, 1, -1);
    pCtl->w1E20C = 3;
    Br73Text(pCtl, 0x41, 1, 1, pS73_p0AB478);
    pPage->cCtl++;

    BR73_CTL(450.0f, 217.0f, 0x5001, 1, -1);
    pCtl->pfn04 = pH73_p10041710;
    pCtl->w1E20C = 5;
    pCtl->pVtbl->f34(pCtl, g_aBr39B720, 1, 3, pS73_p0AB478);
    pPage->cCtl++;
#ifdef BR_MATCHING_BUILD
    return 1;                          /* the original's `mov eax,1` */
#endif
}
