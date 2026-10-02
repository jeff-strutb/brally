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

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "br_coretypes.h"   /* br_globals: its objects */
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
#define BR72_HELPER static __inline
#define BR72_NEW_RAW FUN_10074572
/* FUN_100378c0: prototype in br_funcs.h */
extern void *FUN_10074572(uint32_t);   /* the EH-aware operator new the original calls */ /* the fatal error routine, as the original calls it */
extern BrUiPage_ * BR_THISCALL1 FUN_100418c0(BrUiPage_ *);  /* page ctor */
extern BrUiCtl_  * BR_THISCALL1 FUN_10040b10(BrUiCtl_ *);   /* ctl ctor  */
BR72_HELPER BrUiPage_ *Br72ScreenNew(BrPhase_ *pPhase, float fX, float fY)
{
    Br72Env   *pE = g_pBr72Env;
    BrUiPage_ *pScr;
    uint16_t   i;

    i = pPhase->nPages;
    pPhase->iPage = 0;
    {
        pPhase->aFlags[i] = 1;
    }

    pScr = (BrUiPage_ *)BR72_NEW_RAW(BR72_ALLOC(BrUiPage_,
                                                 BR72_PAGE_ORIG_SIZE));
    /* the original constructs DIRECT (`mov ecx,eax; call 0x100418c0`);
     * the env's ctor pointer is port scaffolding nothing in the image
     * initialises */
    pScr = (pScr != NULL) ? FUN_100418c0(pScr) : NULL;

    /* The original re-reads the counter here rather than reusing `i`. */
    i = pPhase->nPages;
    {
        pPhase->aPages[i] = pScr;
    }
    if (pScr == NULL) {
        FUN_100378c0(4);   /* one arg, as 0x100378C0 takes */
    }
    pPhase->nPages++;


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
    pCtl = (pCtl != NULL) ? FUN_10040b10(pCtl) : NULL;   /* direct, as orig */

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
    return pCtl;
}

/* Shorthand so the transcriptions stay readable.  Relies on the local names
 * pScr / pCtl, which every builder below declares.  The matching build drops
 * the per-site null return: the original has no test after the inlined
 * constructor -- BrErrShow(4) already ended the world. */
#define BR_NEW_CTL()                                    \
    do {                                                \
        pCtl = Br72CtlNew(pScr);                        \
    } while (0)

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
extern void FUN_100407b0(void); extern void FUN_10040790(void);
extern void FUN_1003e5a0(void); extern void FUN_100404b0(void);
extern void FUN_1003ec70(void); extern void FUN_1003d4f0(void);
extern void FUN_1003ec50(void); extern void FUN_10039d20(void);
/* FUN_100393c0: prototype in br_funcs.h */
/* FUN_10038f40: prototype in br_funcs.h */
/* FUN_1003aa10: prototype in br_funcs.h */
/* FUN_1003a910: prototype in br_funcs.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
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
#define pE_p0AB448    ((const void *)&(*(unsigned char *)&g_hot0))
#define pE_p0AB458    ((const void *)&(*(unsigned char *)&DAT_100aabf8))
#define pE_p0AB478    ((const void *)&(*(unsigned char *)&DAT_100aac18))
#define pE_p0AB4A8    ((const void *)&(*(unsigned char *)&DAT_100aac48))
#define pE_p0AB4B8    ((const void *)&(*(unsigned char *)&DAT_100aac58))
#define pE_p0AB4F8    ((const void *)&(*(unsigned char *)&DAT_100aac98))
#define pE_p0AB508    ((const void *)&(*(unsigned char *)&DAT_100aaca8))
#define pE_p0AD300    ((const void *)&(g_strA[0]))
#define pE_p39B720    ((const void *)&(g_aBr39B720[0]))
#define pE_nAB428     (*(int32_t *)&g_hot2)
#define pE_nAB42C     (*(int32_t *)((char *)&g_hot2 + 0x4))

/* WHAT IT DOES: builds the season-progress screen the player sees between
 * championship rounds (the full description is with the dossier at the head
 * of this section: heading, Reset Round, Continue/Back, standings readouts,
 * and the three right-hand picture buttons with their pressed states). */
/* @implements 0x10052030 d3d BrExt_10052030 */
/* BrExt_10052030: the placed body is BrExt_10052030_1004AEE0.cpp */
