/* br_seasonscreen.c -- menus: the season-progress screen builder.
 *
 * BrExt_10052030 builds the between-rounds championship screen (heading,
 * Reset Round, Continue/Back, standings readouts, three picture buttons).
 * The two construction helpers are static __inline in the matching build and
 * are carried here as a copy of the ones slice6_72.c's other builders use.
 *
 * Filed out of the address batch slice6_72.c, whose preamble is carried
 * verbatim below.
 */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <string.h>

#include "slice6_72.h"

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
/* Under the matching build both helpers are __inline: the GLIDE original
 * (0x1004AEE0 and family) INLINES them at every construction site -- the
 * placed body may not call a helper the original never linked.  VC5 /O2
 * honours __inline in C (/Ob1), so one keyword reproduces the original's
 * shape while the port keeps the out-of-line helpers. */
#ifdef BR_MATCHING_BUILD
#define BR72_HELPER static __inline
#define BR72_NEW_RAW FUN_10074572
extern void FUN_100378c0(int32_t); /* fatal error routine */
extern void *FUN_10074572(uint32_t);   /* the EH-aware operator new the original calls */ /* the fatal error routine, as the original calls it */
extern BrUiPage_ * BR_THISCALL1 FUN_100418c0(BrUiPage_ *);  /* page ctor */
extern BrUiCtl_  * BR_THISCALL1 FUN_10040b10(BrUiCtl_ *);   /* ctl ctor  */
#else
#define BR72_HELPER static
#define BR72_NEW_RAW BrOperatorNew
#endif
BR72_HELPER BrUiPage_ *Br72ScreenNew(BrPhase_ *pPhase, float fX, float fY)
{
    Br72Env   *pE = g_pBr72Env;
    BrUiPage_ *pScr;
    uint16_t   i;

    i = pPhase->nPages;
    pPhase->iPage = 0;
#ifndef BR_MATCHING_BUILD
    if (i < BR_PHASE_PAGES)
#endif
    {
        pPhase->aFlags[i] = 1;
    }

    pScr = (BrUiPage_ *)BR72_NEW_RAW(BR72_ALLOC(BrUiPage_,
                                                 BR72_PAGE_ORIG_SIZE));
#ifdef BR_MATCHING_BUILD
    /* the original constructs DIRECT (`mov ecx,eax; call 0x100418c0`);
     * the env's ctor pointer is port scaffolding nothing in the image
     * initialises */
    pScr = (pScr != NULL) ? FUN_100418c0(pScr) : NULL;
#else
    pScr = (pScr != NULL) ? pE->pfnPageCtor(pScr) : NULL;
#endif

    /* The original re-reads the counter here rather than reusing `i`. */
    i = pPhase->nPages;
#ifndef BR_MATCHING_BUILD
    if (i < BR_PHASE_PAGES)
#endif
    {
        pPhase->aPages[i] = pScr;
    }
    if (pScr == NULL) {
#ifdef BR_MATCHING_BUILD
        FUN_100378c0(4);   /* one arg, as 0x100378C0 takes */
#else
        BrErrShow(pE->pErrHost, 4);
#endif
    }
    pPhase->nPages++;

#ifndef BR_MATCHING_BUILD
    /* The matching build falls through like the original: BrErrShow(4) is
     * fatal in practice, and the original dereferences pScr regardless. */
    if (pScr == NULL) {
        return NULL;                    /* DEVIATION: see above */
    }
#endif

    pScr->pOwner = pPhase;
    pScr->f10    = 0;
    pScr->fX     = fX;
    pScr->fY     = fY;
    return pScr;
}

BR72_HELPER BrUiCtl_ *Br72CtlNew(BrUiPage_ *pScr)
{
    Br72Env  *pE = g_pBr72Env;
    BrUiCtl_ *pCtl;

    pCtl = (BrUiCtl_ *)BR72_NEW_RAW(BR72_ALLOC(BrUiCtl_,
                                                BR72_CTL_ORIG_SIZE));
#ifdef BR_MATCHING_BUILD
    pCtl = (pCtl != NULL) ? FUN_10040b10(pCtl) : NULL;   /* direct, as orig */
#else
    pCtl = (pCtl != NULL) ? pE->pfnCtlCtor(pCtl) : NULL;
#endif

#ifdef BR_MATCHING_BUILD
    /* The original's exact shape: the store is unchecked (apCtl ends at the
     * first float and the original relies on it), and after the error report
     * control FALLS THROUGH into the caller's next dereference -- BrErrShow
     * on index 4 is fatal and does not return in practice.  The original's
     * 0x100378C0 takes only the index (`push 4; call; add esp,4`), so the
     * matching call drops the host argument to keep the same push count. */
    pScr->apCtl[pScr->cCtl] = pCtl;
    if (pCtl == NULL) {
        FUN_100378c0(4);
    }
#else
    /* Stored BEFORE the null test, exactly as the original does. */
    if (pScr->cCtl < BR72_PAGE_CTL_MAX) {
        pScr->apCtl[pScr->cCtl] = pCtl;
    }
    if (pCtl == NULL) {
        BrErrShow(pE->pErrHost, 4);
    }
#endif
    return pCtl;
}

/* Shorthand so the transcriptions stay readable.  Relies on the local names
 * pScr / pCtl, which every builder below declares.  The matching build drops
 * the per-site null return: the original has no test after the inlined
 * constructor -- BrErrShow(4) already ended the world. */
#ifdef BR_MATCHING_BUILD
#define BR_NEW_CTL()                                    \
    do {                                                \
        pCtl = Br72CtlNew(pScr);                        \
    } while (0)
#else
#define BR_NEW_CTL()                                    \
    do {                                                \
        pCtl = Br72CtlNew(pScr);                        \
        if (pCtl == NULL) { return; }                   \
    } while (0)
#endif

/* ==========================================================================
 * 0x10052030 -- BrExt_10052030.  Twenty-three controls.
 * ========================================================================== */
/* WHAT IT DOES: builds the season-progress screen the player sees between
 * championship rounds. It carries the heading, a Reset Round row, Continue and
 * Back, the standings readouts, and down the right-hand side three picture
 * buttons -- save, main menu and options -- each of which has a second
 * "pressed" picture it swaps to. */

/* ===== matching-build immediates for BrExt_10052030 =====================
 * The GLIDE original stores hook FUNCTIONS and pushes style ADDRESSES as
 * immediates; the port routes them through g_pBr72Env, a struct nothing in
 * the original image ever initialises.  Under the matching build every env
 * read below becomes the original's own immediate.  Verified two ways: the
 * style set maps one-to-one by USE COUNT at a constant -0x860 region drift,
 * and two hooks land exactly on functions this tree already places by VA
 * (p10040730 -> 0x10039C70 BrMenuCap0730, p100415A0 -> 0x1003AB00
 * BrMenuText15A0).  The trailing hex in each FUN_/DAT_ name IS the address
 * the resolver uses. */
#ifdef BR_MATCHING_BUILD
extern void FUN_100407b0(void); extern void FUN_10040790(void);
extern void FUN_1003e5a0(void); extern void FUN_100404b0(void);
extern void FUN_1003ec70(void); extern void FUN_1003d4f0(void);
extern void FUN_1003ec50(void); extern void FUN_10039d20(void);
extern void FUN_100393c0(void); extern void FUN_10039c70(void);
extern void FUN_10038f40(void); extern void FUN_1003ab00(void);
extern void FUN_1003aa10(void); extern void FUN_1003a860(void);
extern void FUN_1003a910(void); extern void FUN_1003a070(void);
extern unsigned char DAT_100aabe8, DAT_100aabf8, DAT_100aac18, DAT_100aac48;
extern unsigned char DAT_100aac58, DAT_100aac98, DAT_100aaca8, DAT_100acad8;
extern unsigned char DAT_10396f08;
extern int32_t DAT_100aabc8, DAT_100aabcc;
#define BR72H(f) ((BrUiCtlHookFn_)&f)
#define pH_p10047360  BR72H(FUN_100407b0)
#define pH_p10047340  BR72H(FUN_10040790)
#define pH_p10045050  BR72H(FUN_1003e5a0)
#define pH_p10047060  BR72H(FUN_100404b0)
#define pH_p100457E0  BR72H(FUN_1003ec70)
#define pH_p10043FA0  BR72H(FUN_1003d4f0)
#define pH_p100457C0  BR72H(FUN_1003ec50)
#define pH_p100407E0  BR72H(FUN_10039d20)
#define pH_p1003FE80  BR72H(FUN_100393c0)
#define pH_p10040730  BR72H(FUN_10039c70)
#define pH_p1003FA00  BR72H(FUN_10038f40)
#define pH_p100415A0  BR72H(FUN_1003ab00)
#define pH_p100414B0  BR72H(FUN_1003aa10)
#define pH_p10041300  BR72H(FUN_1003a860)
#define pH_p100413B0  BR72H(FUN_1003a910)
#define pH_p10040B30  BR72H(FUN_1003a070)
#define pE_p0AB448    ((const void *)&DAT_100aabe8)
#define pE_p0AB458    ((const void *)&DAT_100aabf8)
#define pE_p0AB478    ((const void *)&DAT_100aac18)
#define pE_p0AB4A8    ((const void *)&DAT_100aac48)
#define pE_p0AB4B8    ((const void *)&DAT_100aac58)
#define pE_p0AB4F8    ((const void *)&DAT_100aac98)
#define pE_p0AB508    ((const void *)&DAT_100aaca8)
#define pE_p0AD300    ((const void *)&DAT_100acad8)
#define pE_p39B720    ((const void *)&DAT_10396f08)
#define pE_nAB428     DAT_100aabc8
#define pE_nAB42C     DAT_100aabcc
#else
#define pH_p10047360  (pH->p10047360)
#define pH_p10047340  (pH->p10047340)
#define pH_p10045050  (pH->p10045050)
#define pH_p10047060  (pH->p10047060)
#define pH_p100457E0  (pH->p100457E0)
#define pH_p10043FA0  (pH->p10043FA0)
#define pH_p100457C0  (pH->p100457C0)
#define pH_p100407E0  (pH->p100407E0)
#define pH_p1003FE80  (pH->p1003FE80)
#define pH_p10040730  (pH->p10040730)
#define pH_p1003FA00  (pH->p1003FA00)
#define pH_p100415A0  (pH->p100415A0)
#define pH_p100414B0  (pH->p100414B0)
#define pH_p10041300  (pH->p10041300)
#define pH_p100413B0  (pH->p100413B0)
#define pH_p10040B30  (pH->p10040B30)
#define pE_p0AB448    (pE->p0AB448)
#define pE_p0AB458    (pE->p0AB458)
#define pE_p0AB478    (pE->p0AB478)
#define pE_p0AB4A8    (pE->p0AB4A8)
#define pE_p0AB4B8    (pE->p0AB4B8)
#define pE_p0AB4F8    (pE->p0AB4F8)
#define pE_p0AB508    (pE->p0AB508)
#define pE_p0AD300    (pE->p0AD300)
#define pE_p39B720    (pE->p39B720)
#define pE_nAB428     (pE->nAB428)
#define pE_nAB42C     (pE->nAB42C)
#endif

/* WHAT IT DOES: builds the season-progress screen the player sees between
 * championship rounds (the full description is with the dossier at the head
 * of this section: heading, Reset Round, Continue/Back, standings readouts,
 * and the three right-hand picture buttons with their pressed states). */
/* @implements 0x10052030 d3d BrExt_10052030 */
#ifdef BR_MATCHING_BUILD
int32_t BrExt_10052030(BrPhase_ *pPhase)
#else
void BrExt_10052030(BrPhase_ *pPhase)
#endif
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
#ifndef BR_MATCHING_BUILD
    /* The original never tests: allocation failure already hit the fatal
     * error path inside the inlined ScreenNew. */
    if (pScr == NULL) {
        return;
    }
#endif

    /* 0x100520B6 -- the root */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 0.0f, 0.0f, 9, 2, 5, 0, 0);
    pScr->cCtl++;

    /* 0x10052137 -- the title */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, 10.0f, 0x100009, 2, 5, 1, -1);
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x44), 1, 1, pE_p0AB508);
    pScr->cCtl++;

    /* 0x100521D0 -- fY - (-19) */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY - (-19.0f),
                     0x102001, 2, 5, 1, -1);
    pCtl->pfn0C  = pH_p10047360;
    pCtl->pfn08  = pH_p10047340;
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x45), 1, 1, pE_p0AB448);
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x10052289 -- fY - (-95).
     * GOTCHA: -38, -57 and -76 are all skipped.
     * GOTCHA: f1E20C is 2 and f34's third argument is 0 -- the only such
     * pair in this function. */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY - (-95.0f),
                     0x102001, 2, 5, 1, -1);
    pCtl->pfn0C  = pH_p10047360;
    pCtl->pfn08  = pH_p10045050;
    pCtl->w1E20C = 2;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x1E), 1, 0, pE_p0AB448);
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x10052342 -- fY - (-114) */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY - (-114.0f),
                     0x102001, 2, 5, 1, -1);
    pCtl->pfn0C  = pH_p10047360;
    pCtl->pfn08  = pH_p10047060;
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x0C), 1, 1, pE_p0AB448);
    pScr->cCtl++;
    pScr->cSel++;

    /* 0x100523FB.  Both globals are read with `fild`, i.e. as INTEGERS. */
    fA = (float)pE_nAB428;
    fB = (float)pE_nAB42C;

    /* 0x10052401 -- rect 1.  GOTCHA: none of the three rects bumps cSel. */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, fA, fB, 0x402001, 2, 5, 1, 0x78);
    pCtl->pfn0C = pH_p10047360;
    pCtl->pfn08 = pH_p100457E0;
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
    pCtl->pfn0C = pH_p10047360;
    pCtl->pfn08 = pH_p10043FA0;
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
    pCtl->pfn0C = pH_p10047360;
    pCtl->pfn08 = pH_p100457C0;
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
    pCtl->pfn04 = pH_p100407E0;
    pCtl->cChild++;
    pCtl->aChild[0] = (uint16_t)(pScr->cCtl + 1);
    pScr->cCtl++;

    /* 0x100526E3 -- (fX, 275.0f).  Text by ADDRESS. */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, 275.0f, 0x101001, 2, 5, 1, -1);
    pCtl->pfn04  = pH_p1003FE80;
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, pE_p0AD300, 1, 1, pE_p0AB4B8);
    pScr->cCtl++;

    /* 0x10052779 -- (325.0f, 72.0f) */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 325.0f, 72.0f, 1, 2, 5, 1, 0x11);
    pCtl->pfn04 = pH_p10040730;
    pCtl->cChild++;
    pCtl->aChild[0] = (uint16_t)(pScr->cCtl + 1);
    pScr->cCtl++;

    /* 0x10052805 -- (fX, 152.0f) */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, 152.0f, 0x101001, 2, 5, 1, -1);
    pCtl->pfn04  = pH_p1003FA00;
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, pE_p0AD300, 1, 1, pE_p0AB4A8);
    pScr->cCtl++;

    /* 0x1005289B -- (450.0f, 125.0f) */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 450.0f, 125.0f, 0x100009, 2, 5, 1, -1);
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x40), 1, 1, pE_p0AB4F8);
    pScr->cCtl++;

    /* 0x10052932 -- (450.0f, 185.0f) */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 450.0f, 185.0f, 0x100009, 2, 5, 1, -1);
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x46), 1, 1, pE_p0AB4F8);
    pScr->cCtl++;

    /* 0x100529C9 -- (450.0f, 141.0f).  Text is the writable buffer
     * 0x1039B720, a3 = 3, f1E20C = 5. */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 450.0f, 141.0f, 0x5001, 2, 5, 1, -1);
    pCtl->pfn04  = pH_p100415A0;
    pCtl->w1E20C = 5;
    pCtl->pVtbl->f34(pCtl, pE_p39B720, 1, 3, pE_p0AB4F8);
    pScr->cCtl++;

    /* 0x10052A61 -- (fX, 203.0f) */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, 203.0f, 0x100009, 2, 5, 1, -1);
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x40), 1, 1, pE_p0AB478);
    pScr->cCtl++;

    /* 0x10052AFA -- (fX, 265.0f) */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, 265.0f, 0x100009, 2, 5, 1, -1);
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x41), 1, 1, pE_p0AB478);
    pScr->cCtl++;

    /* 0x10052B93 -- (450.0f, 217.0f) */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 450.0f, 217.0f, 0x5001, 2, 5, 1, -1);
    pCtl->pfn04  = pH_p100414B0;
    pCtl->w1E20C = 5;
    pCtl->pVtbl->f34(pCtl, pE_p39B720, 1, 3, pE_p0AB478);
    pScr->cCtl++;

    /* 0x10052C2B -- (106.0f, 85.0f) */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 106.0f, 85.0f, 0x100001, 2, 5, 1, -1);
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x38), 1, 1, pE_p0AB458);
    pScr->cCtl++;

    /* 0x10052CC2 -- (440.0f, 66.0f).  a3 = 4 and f1E20C = 0x34. */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 440.0f, 66.0f, 0x5001, 2, 5, 1, -1);
    pCtl->pfn04  = pH_p10041300;
    pCtl->w1E20C = 0x34;
    pCtl->pVtbl->f34(pCtl, pE_p39B720, 1, 4, pE_p0AB458);
    pScr->cCtl++;

    /* 0x10052D5A -- (106.0f, 123.0f) */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 106.0f, 123.0f, 0x100001, 2, 5, 1, -1);
    pCtl->w1E20C = 3;
    pCtl->pVtbl->f34(pCtl, BrStrGet(0x36), 1, 1, pE_p0AB458);
    pScr->cCtl++;

    /* 0x10052DF1 -- (440.0f, 104.0f) */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, 440.0f, 104.0f, 0x5001, 2, 5, 1, -1);
    pCtl->pfn04  = pH_p100413B0;
    pCtl->w1E20C = 0x34;
    pCtl->pVtbl->f34(pCtl, pE_p39B720, 1, 4, pE_p0AB458);
    pScr->cCtl++;

    /* 0x10052E89 -- the last control.
     * GOTCHA: the subtrahend is 0x1008F6A0 == +19.0f, NOT one of the negative
     * row constants, so this is a genuine SUBTRACTION -- the only row step in
     * the family that moves a control UP. */
    BR_NEW_CTL();
    pCtl->pVtbl->f38(pCtl, pPhase, pScr->fX, pScr->fY - 19.0f,
                     0x5001, 2, 5, 1, -1);
    pCtl->pfn04  = pH_p10040B30;
    pCtl->w1E20C = 0x34;
    pCtl->pVtbl->f34(pCtl, pE_p39B720, 1, 4, pE_p0AB448);
    pScr->cCtl++;
#ifdef BR_MATCHING_BUILD
    return 1;                          /* the original's `mov eax,1` */
#endif
}
