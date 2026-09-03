/* slice8_89.c -- 0x10044D00 and 0x10045050, the two BrUi72Hooks control hooks
 * no other module fills, plus the activate and the leave they reach.
 *
 * See slice8_89.h for what this module is, why eight of the ten addresses it
 * was commissioned for turned out to be already ported, which builder line
 * proves each pairing, the three conflicts it reports, and the exact Glide /
 * D3D address pairs every body was read from.
 *
 * Transcribed from orig/BRGlide.dll (the project reference) and re-read at the
 * D3D addresses to fix the global numbering.
 * ========================================================================== */
#include "slice8_89.h"

#include <stddef.h>
#include <string.h>

BrUiHook89Ctx g_brHook89;

void BrUiHook89Reset(void)
{
    memset(&g_brHook89, 0, sizeof(g_brHook89));
}

/* ==========================================================================
 * Cross-slice declarations.
 *
 * These are copied by hand rather than reached through their headers, which
 * is slice8_85.c's convention for the same situation: the owning headers
 * (slice4_50.h for both words) are not known to combine with slice6_72.h and
 * slice6_73.h in one translation unit, and a wrong guess there costs a build,
 * not a behaviour. The declarations below are VERBATIM from the owner so a
 * future diff of the two is a diff.
 * ========================================================================== */

/* XSLICE port/include/slice4_50.h:191-192, defined port/src/slice4_50.c:50-51 */
extern int32_t g_brAA28CC;   /* 0x10AA28CC */
extern int32_t g_brAA28C8;   /* 0x10AA28C8 */

/* XSLICE 0x10059BB0 -- 0x10044D00's enter hook, a C++ UI screen builder.
 * NOT PORTED; port/host/br_stubs.c satisfies it, which is why activating this
 * row lands on an empty phase today. Declared with the shape the ORIGINAL
 * calls it with (phase +0x04, one argument, the phase itself) rather than the
 * stub's, exactly as slice7_81.c declares BrExt_100509F0 and for the same
 * reason. slice2_26.h declares the same address over its own five-field
 * `BrPhase`, so that header can never be combined with this one. */
extern void BrExt_10059BB0(BrPhase_ *pSelf);

/* XSLICE 0x1004A580 -- 0x10045110's enter hook. slice8_86.c defines this name
 * as `void (void *)`; slice2_26.h declares it as `void (BrPhase *)` and
 * slice3_33.h declares the BODY as the two-argument BrExt_1004A580. That arity
 * conflict is slice7_82.h's, is unresolved, and is NOT resolved here: the
 * ADDRESS is only ever STORED into a phase's +0x04 by this module, never
 * called with slice3_33's signature, so the placeholder is the correct thing
 * to store. Same choice slice7_81.c makes for 0x1004BDC0. */
extern void BrPhaseEnterPlaceholder_1004A580(BrPhase_ *pSelf);

/* ==========================================================================
 * Shared shapes
 *
 * DUPLICATION, stated rather than hidden: Br89NewPhase and Br89Activate are
 * the same three shapes port/src/slice7_81.c has as Br81NewPhase and
 * Br81Activate. Those are file-static and this is a different translation
 * unit, so the bodies are repeated -- the same call slice8_85.c documents for
 * its own copy of the leave prologue. If either copy is ever corrected, BOTH
 * must be. They are NOT delegated to, because delegating would not help: the
 * slots differ and the storage is owned by two different structs on purpose.
 * ========================================================================== */

/* `operator new(0xC8)` -- which does NOT zero -- then 0x10048710. */
static BrPhase_ *Br89NewPhase(void)
{
    BrPhase_ *p = (BrPhase_ *)BrOperatorNew(BR_PHASE_ALLOC_SIZE);
    return (p != NULL) ? BrOptObjCtor(p) : NULL;
}

/* The body every ACTIVATE in the family shares. See slice8_89.h for the shape
 * and for why each of the three re-reads is spelled out.
 *
 * Returns 1 when the phase is available (already built or just built) and 0
 * when the allocation failed. Neither activate in this module has a
 * just-built-only epilogue, so unlike slice7_81.c's Br81Activate this one does
 * not report which of the two paths ran. */
static int32_t Br89Activate(BrPhase_ **ppSlot, BrPhaseEnterFn_ pfnEnter)
{
    BrUiNav  *pNav = g_pBrUiNav;
    BrPhase_ *p;

    if (*ppSlot != NULL) {
        /* Already built: republish and return WITHOUT running the builder. */
        pNav->pAA2904 = *ppSlot;
        return 1;
    }

    p = Br89NewPhase();
    /* Both globals are written even when the allocation failed. */
    *ppSlot       = p;
    pNav->pAA2904 = p;
    if (p == NULL) {
        return 0;
    }

    p->pfnEnter = pfnEnter;
    (*ppSlot)->pfnEnter(*ppSlot);                       /* re-read of the slot */

    /* DEVIATION (memory safety): the original stores through the re-read
     * pointer with no NULL test. */
    if (pNav->pAA2904 != NULL) {
        pNav->pAA2904->f0C = 1;                         /* re-read            */
    }
    if (pNav->pAA2904 != NULL) {
        pNav->pAA2904->f68 = 1;                         /* and re-read again  */
    }
    return 1;
}

/* ==========================================================================
 * ACTIVATE
 * ========================================================================== */

int32_t BrUiHook89Activate_10045110(void)
{
    return Br89Activate(&g_brHook89.pAA2914,
                        BrPhaseEnterPlaceholder_1004A580);
}

/* ==========================================================================
 * The two BrUi72Hooks slots
 * ========================================================================== */

int32_t BrUiHook89_10044D00(BrUiCtl_ *pCtl)
{
    /* The argument is NEVER read. Every `[esp+4]` in the original is the SEH
     * link, sixteen bytes below where the argument sits -- see the GOTCHA in
     * slice8_89.h. The SEH frame itself carries no behaviour the port can
     * observe: it exists so `operator new`'s unwind can find the try level,
     * and the two `[esp+0xC]` stores (0, then -1) are that level. */
    (void)pCtl;

    /* The prologue. The original loads the slot BEFORE these two stores and
     * tests it BETWEEN them, then branches; the stores therefore happen on
     * BOTH paths. Hoisting them ahead of the load is safe and is the only
     * reordering in this file: the two words are distinct globals that cannot
     * alias the slot. */
    g_brAA28C8 = 0;
    g_brAA28CC = 0;

    return Br89Activate(&g_brHook89.pAA2964, BrExt_10059BB0);
}

int32_t BrUiHook89_10045050(BrUiCtl_ *pCtl)
{
    BrUiCtl_ *pTarget;

    /* The original loads the argument into EAX here, before the first store,
     * and pushes it at the activate -- which never reads it. */
    (void)pCtl;

    g_brHook89.n0AC304 = 0;
    (void)BrUiHook89Activate_10045110();     /* the result is DISCARDED */

    /* 0x10AA29B4 is read AFTER the activate and BEFORE n0AC304 goes back to 1;
     * both orders are the original's. */
    pTarget            = g_brHook89.pAA29B4;
    g_brHook89.n0AC304 = 1;

    /* DEVIATION: NULL-guarded. The original faults if the activate failed, or
     * if 0x10046CD0 has already cleared this word. */
    if (pTarget != NULL) {
        pTarget->pfn08 = BrUiHook89_10046CD0;
    }

    g_pBrUiNav->n0AA010 = 0;
    return 1;                                /* unconditional -- see the GOTCHA */
}

/* ==========================================================================
 * LEAVE
 * ========================================================================== */

/* WHAT IT DOES: leaves a screen and goes back: tells the owning screen to
 * release its pages, tells the current screen it is going away, and repoints
 * the game at the screen behind it. */
/* port-only body; Glide match is src/core/cpp/0x10040120.cpp */
int32_t BrUiHook89_10046CD0(BrUiCtl_ *pCtl)
{
    BrUiNav  *pNav   = g_pBrUiNav;
    BrPhase_ *pOwner = pCtl->pOwner;
    BrPhase_ *pCur;
    BrPhase_ *pNext;

    /* The LEAVE prologue the whole family shares:
     *     mov ecx,[eax+0x2AE8] / mov edx,[ecx] / call [edx+0x1C]
     * +0x2AE8 is the control's OWNING PHASE (br_ui.h `pOwner`) and phase
     * vtable slot +0x1C is 0x10048AA0, "release every page".
     *
     * DEVIATION (slice8_85.c's, applied for the same reason): the vtable and
     * the slot are guarded. The original always has both; in this port the
     * phase vtable is wired by the host and a slot it has not filled is NULL.
     * A guarded-out call is a MISSING EFFECT, not an equivalence. */
    if (pOwner != NULL && pOwner->pVtbl != NULL && pOwner->pVtbl->f1C != NULL) {
        pOwner->pVtbl->f1C(pOwner);
    }

    /* The CURRENT phase is read here, AFTER +0x1C has run. Its NULL test is
     * the original's own; the vtable tests are the deviation above. */
    pCur = pNav->pAA2904;
    if (pCur != NULL && pCur->pVtbl != NULL && pCur->pVtbl->f00 != NULL) {
        (void)pCur->pVtbl->f00(pCur, 1);
    }

    pNext              = g_brHook89.pAA2930;   /* read BEFORE the clears */
    g_brHook89.pAA2914 = NULL;
    g_brHook89.pAA29B4 = NULL;
    pNav->pAA2904      = pNext;
    return 0;
}

/* ==========================================================================
 * Installation
 * ========================================================================== */

void BrUiHook89Install72(BrUi72Hooks *pHooks)
{
    if (pHooks == NULL) {
        return;
    }

    /* 0x10052030 -- port/src/slice6_72.c:979 */
    pHooks->p10045050 = BrUiHook89_10045050;

    /* 0x10059760 -- port/src/slice6_72.c:1213 */
    pHooks->p10044D00 = BrUiHook89_10044D00;

    /* NOT TOUCHED, and every one is a visible hole rather than a silent
     * no-op. slice8_84.c's BrUiHook84Install72 owns the rest of this table
     * and slice7_80.c's BrUiOptInstall72 owns four more; this installer must
     * not race either, so it writes exactly the two slots above.
     *   p10046260  the third hole slice8_84.h lists under NOT DONE (D); it
     *              needs slice3_31's context wired, which nothing does.
     *   p10047250  p100474B0  likewise. */
}
