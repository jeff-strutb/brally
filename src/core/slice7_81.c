/* slice7_81.c -- the screen-entry and phase-changing control hooks, over
 * br_ui.h's struct model.
 *
 * See slice7_81.h for what this module is, which addresses it duplicates, why
 * a second transcription exists at all, and the four conflicts it reports.
 * The short version: every body here also exists in port/src/slice3_31.c (and
 * one in port/src/slice2_26.c) over slice2_25.h's byte-offset `BrGameObj`, and
 * the argument these hooks receive is a `BrUiCtl_`, whose fields have moved
 * under LP64.
 *
 * Transcribed from orig/BRGlide.dll and cross-checked against orig/BRD3D.dll.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "slice7_81.h"

#include "slice5_61.h"   /* BrSub1003E510 (0x1003E510), g_br0AB3F4 (0x100AB3F4) */
#include "slice5_62.h"   /* BrExt_100419D0 (0x100419D0)                         */
#include "slice6_70.h"   /* BrExt_1003E680 (0x1003E680)                         */

#include <stddef.h>
#include <string.h>

BrUiHook81Ctx g_brHook81;

/* XSLICE 0x100509F0 -- the "new season" screen builder. NOT PORTED; the host
 * harness satisfies it from port/host/br_stubs.c, which is why activating
 * "New Season" lands on an empty phase today. Declared with the shape the
 * original calls it with (phase +0x04, one argument, the phase itself) rather
 * than the stub's, exactly as br_stubs.c's banner intends. */
extern void BrExt_100509F0(BrPhase_ *pSelf);

/* XSLICE 0x1004BDC0 -- likewise not ported. slice2_26.h coined the name. */
extern void BrPhaseEnterPlaceholder_1004BDC0(BrPhase_ *pSelf);

/* 0x10049F40 and 0x10050060 ARE ported (slice6_71.c / slice6_73.c) and are
 * declared by the two headers slice7_81.h already includes. */

/* ==========================================================================
 * Shared shapes
 * ========================================================================== */

/* `operator new(0xC8)` -- which does NOT zero -- then 0x10048710.
 *
 * HARDENING (port): BR_PHASE_ALLOC_SIZE is max(sizeof(BrPhase_), 0xC8), so
 * this is never smaller than the original's literal and never under-allocates
 * the widened object. Same hardening slice2_26.c, slice3_31.c and br_uinav.c
 * all apply at their own call sites. */
static BrPhase_ *Br81NewPhase(void)
{
    BrPhase_ *p = (BrPhase_ *)BrOperatorNew(BR_PHASE_ALLOC_SIZE);
    return (p != NULL) ? BrOptObjCtor(p) : NULL;
}

/* The body all three ACTIVATE routines share once their prologue has run.
 *
 * ppSlot is the per-screen singleton; the CURRENT phase is br_uinav.h's, so
 * there is one storage for 0x10AA2904 and not two.
 *
 * Returns 1 when the phase is available (already built or just built) and 0
 * when the allocation failed. *pfBuilt is set only on the just-built path --
 * the only path a caller's epilogue may run on.
 *
 * GOTCHA, and it is the whole reason the three loads are spelled out: the
 * SLOT is re-read for the enter call, and the CURRENT phase is re-read once
 * per flag store. An enter hook that repoints 0x10AA2904 therefore stamps f0C
 * and f68 on whatever it moved to. */
static int32_t Br81Activate(BrPhase_ **ppSlot, BrPhaseEnterFn_ pfnEnter,
                            int *pfBuilt)
{
    BrUiNav  *pNav = g_pBrUiNav;
    BrPhase_ *p;

    *pfBuilt = 0;

    if (*ppSlot != NULL) {
        /* Already built: republish and return WITHOUT running the builder. */
        pNav->pAA2904 = *ppSlot;
        return 1;
    }

    p = Br81NewPhase();
    /* Both globals are written even when the allocation failed. */
    *ppSlot       = p;
    pNav->pAA2904 = p;
    if (p == NULL)
        return 0;                     /* NOTE: 0 here, where 0x10045AF0's
                                       * sibling shape returns 1 */

    p->pfnEnter = pfnEnter;
    (*ppSlot)->pfnEnter(*ppSlot);                       /* re-read of the slot */

    /* DEVIATION (memory safety): the original stores through the re-read
     * pointer with no NULL test. */
    if (pNav->pAA2904 != NULL)
        pNav->pAA2904->f0C = 1;                         /* re-read            */
    if (pNav->pAA2904 != NULL)
        pNav->pAA2904->f68 = 1;                         /* and re-read again  */

    *pfBuilt = 1;
    return 1;
}

/* The second, flag-light object 0x10045C90 builds after its first: f0C is set,
 * f68 is NOT, and the CURRENT phase is not touched at all. Returns 0 on an
 * allocation failure. */
static int32_t Br81ActivateSecond(BrPhase_ **ppSlot, BrPhaseEnterFn_ pfnEnter)
{
    BrPhase_ *p = Br81NewPhase();

    *ppSlot = p;
    if (p == NULL)
        return 0;

    p->pfnEnter = pfnEnter;
    (*ppSlot)->pfnEnter(*ppSlot);      /* re-read */
    (*ppSlot)->f0C = 1;                /* re-read */
    return 1;
}

/* The prologue every LEAVE routine shares.
 *
 *     mov ecx,[eax+0x2AE8] / mov edx,[ecx] / call [edx+0x1C]
 *
 * +0x2AE8 is the control's OWNING PHASE (br_ui.h `pOwner`) and phase vtable
 * slot +0x1C is 0x10048AA0, "release every page". The owner is dereferenced
 * with no guard, as in the original; only the current phase is NULL-tested,
 * and that test is the original's own. */
static void Br81LeavePrologue(BrUiCtl_ *pCtl)
{
    BrPhase_ *pOwner = pCtl->pOwner;
    BrPhase_ *pCur;

    pOwner->pVtbl->f1C(pOwner);

    /* The CURRENT phase is read here, AFTER +0x1C has run. */
    pCur = g_pBrUiNav->pAA2904;
    if (pCur != NULL)
        (void)pCur->pVtbl->f00(pCur, 1);
}

/* The name reset 0x10046B10 and 0x10046EB0 share.
 *
 * DEVIATION: the original's copies are unbounded inline `rep movs` out of
 * 0x1039B720. Here they are bounded by g_br73.cbScratch and NUL-terminated.
 * The source lives in the zero-initialised tail of .data, so it is the empty
 * string until something else writes it. */
static void Br81NameReset(void)
{
    size_t cb = g_br73.cbScratch;

    g_brHook81.pAA2928  = NULL;          /* 0x10AA2928 */
    g_pBrUiNav->pAA29C0 = NULL;          /* 0x10AA29C0 -- a CONTROL           */
    g_br73.pAA29CC      = NULL;          /* 0x10AA29CC -- see CONFLICT 3      */
    g_brHook81.nAA28E4  = 0;             /* 0x10AA28E4 */
    g_br0AB3F4          = -1;            /* 0x100AB3F4 */

    if (g_br73.szAA2518 != NULL && cb > 0) {
        strncpy(g_br73.szAA2518, g_aBr39B720, cb - 1);
        g_br73.szAA2518[cb - 1] = '\0';
    }
    if (g_br73.szA9D618 != NULL && cb > 0) {
        strncpy(g_br73.szA9D618, g_aBr39B720, cb - 1);
        g_br73.szA9D618[cb - 1] = '\0';
    }
}

/* @n64 0x8021E1C0 located */
void BrUiHook81Reset(void)
{
    memset(&g_brHook81, 0, sizeof(g_brHook81));
}

/* ==========================================================================
 * ACTIVATE
 * ========================================================================== */

int32_t BrUiHook81Activate_10045C90(void)
{
    int fBuilt = 0;

    if (Br81Activate(&g_brHook81.pAA292C, BrExt_100509F0, &fBuilt) == 0)
        return 0;
    if (!fBuilt)
        return 1;      /* the already-built path stops here */

    /* The second object. 0x10AA2904 is NOT repointed at it. */
    if (Br81ActivateSecond(&g_brHook81.pAA2974, BrExt_10049F40) == 0)
        return 0;
    return 1;
}

/* @n64 0x8022AF34 located */
int32_t BrUiHook81Activate_10045BC0(void)
{
    int fBuilt = 0;
    return Br81Activate(&g_brHook81.pAA2928, BrExt_10050060, &fBuilt);
}

int32_t BrUiHook81Activate_100451E0(void)
{
    int fBuilt = 0;

    /* Prologue: `push 0x100AD300 / call 0x100419D0`. The pushed value is the
     * ADDRESS of the one-space string, which slice6_73.h models as
     * aStyles.p0AD300. */
    BrExt_100419D0((void *)(size_t)g_br73.aStyles.p0AD300);

    return Br81Activate(&g_brHook81.pAA2918,
                        BrPhaseEnterPlaceholder_1004BDC0, &fBuilt);
}

/* ==========================================================================
 * INSTALLERS
 * ========================================================================== */

int32_t BrUiHook81_10045AA0(BrUiCtl_ *pCtl)
{
    BrUiNav  *pNav = g_pBrUiNav;
    BrUiCtl_ *pBack;

    (void)pCtl;   /* pushed at 0x10045C90 and never read there */

    pNav->n0AA010 = 0;
    BrExt_1003E680();
    pNav->nACED34 = 0;

    (void)BrUiHook81Activate_10045C90();

    /* 0x10AA29B0 is a CONTROL (CONFLICT 1) and +0x08 is its pfn08 slot, so
     * this wires the back row of the screen 0x10049F40 has just built.
     * DEVIATION: NULL-guarded; the original faults if the activate failed. */
    pBack = g_brS71.pAA29B0;
    if (pBack != NULL)
        pBack->pfn08 = BrUiHook81_10046D70;

    pNav->n0AA010 = 0;         /* cleared a second time, with only the
                                * activate in between, which never reads it */
    BrSub1003E510();
    return 1;
}

int32_t BrUiHook81_100450F0(BrUiCtl_ *pCtl)
{
    BrUiCtl_ *pOther = g_br73.pAA29F4;

    /* ANOTHER control's +0x08, called with THIS control as the argument. */
    if (pOther != NULL && pOther->pfn08 != NULL)   /* DEVIATION: guarded */
        (void)pOther->pfn08(pCtl);

    g_pBrUiNav->n0AA010 = 0;
    return 0;                          /* 0, unlike its three neighbours */
}

/* ==========================================================================
 * LEAVE
 * ========================================================================== */

/* WHAT IT DOES: leaves the current screen: tells the owning screen to
 * release all its pages, tells the current screen it is going away, forgets
 * one screen singleton, and makes a previously remembered screen current
 * again. That last one is read before the forgetting, which matters if the
 * two happen to be the same. */
/* port-only body; Glide match is src/core/cpp/0x1003F860.cpp */
int32_t BrUiHook81_100463C0(BrUiCtl_ *pCtl)
{
    BrPhase_ *pNext;

    Br81LeavePrologue(pCtl);
    pNext = g_brHook81.pAA2958;        /* read BEFORE the clear */
    g_brHook81.pAA2940 = NULL;
    g_pBrUiNav->pAA2904 = pNext;
    return 0;
}

int32_t BrUiHook81_10046620(BrUiCtl_ *pCtl)
{
    BrPhase_ *pNext;

    Br81LeavePrologue(pCtl);
    pNext = g_brHook81.pAA2980;        /* read BEFORE the clears */
    g_brHook81.pAA2990 = NULL;
    g_br73.pAA29F0     = NULL;         /* 0x10AA29F0 -- a CONTROL */
    g_pBrUiNav->pAA2904 = pNext;
    return 0;
}

/* WHAT IT DOES: the back handler for one particular screen: releases that
 * screen's pages, forgets it, and returns to whichever screen was remembered
 * as the one behind it. */
/* port-only body; Glide match is src/core/cpp/0x1003FF20.cpp */
int32_t BrUiHook81_10046AD0(BrUiCtl_ *pCtl)
{
    BrPhase_ *pNext;

    Br81LeavePrologue(pCtl);
    pNext = g_brHook81.pAA293C;        /* read BEFORE the clear */
    g_brHook81.pAA2918 = NULL;
    g_pBrUiNav->pAA2904 = pNext;
    return 0;
}

/* WHAT IT DOES: the back handler for a screen that had the player typing a
 * name into it. As well as the usual release-and-return, it clears the name-
 * entry state and copies the shared edit buffer into two places. Unlike the
 * plain leave routines, this one reads the screen it is returning to after
 * doing all that rather than before. */
/* port-only body; Glide match is src/core/cpp/0x1003FF60.cpp */
int32_t BrUiHook81_10046B10(BrUiCtl_ *pCtl)
{
    Br81LeavePrologue(pCtl);
    Br81NameReset();
    /* GOTCHA: this one reads its destination AFTER the clears and the copies
     * (0x10046B9F), where the plain leaves read theirs first. */
    g_pBrUiNav->pAA2904 = g_brHook81.pAA293C;
    return 0;
}

/* WHAT IT DOES: the back handler for the screen that was built as a pair: it
 * releases the pages, forgets both halves of the pair and the control that
 * pointed at them, and returns to the screen behind. */
/* port-only body; Glide match is src/core/cpp/0x100401C0.cpp */
int32_t BrUiHook81_10046D70(BrUiCtl_ *pCtl)
{
    BrPhase_ *pNext;

    Br81LeavePrologue(pCtl);
    pNext = g_brHook81.pAA291C;        /* read BEFORE the clears */
    g_brHook81.pAA292C  = NULL;
    g_brS71.pAA29B0     = NULL;        /* 0x10AA29B0 -- a CONTROL */
    g_brHook81.pAA2974  = NULL;
    g_pBrUiNav->pAA2904 = pNext;
    return 0;
}

/* WHAT IT DOES: the twin of the other name-entry back handler: same release,
 * same name-entry reset, but it returns to a different remembered screen. */
/* port-only body; Glide match is src/core/cpp/0x10040300.cpp */
int32_t BrUiHook81_10046EB0(BrUiCtl_ *pCtl)
{
    Br81LeavePrologue(pCtl);
    Br81NameReset();
    g_pBrUiNav->pAA2904 = g_brHook81.pAA2934;   /* read AFTER, as above */
    return 0;
}

/* WHAT IT DOES: a plain back handler: release the pages, forget one screen
 * singleton, and return to the screen remembered behind it. */
/* port-only body; Glide match is src/core/cpp/0x10040530.cpp */
int32_t BrUiHook81_100470E0(BrUiCtl_ *pCtl)
{
    BrPhase_ *pNext;

    Br81LeavePrologue(pCtl);
    pNext = g_brHook81.pAA2938;        /* read BEFORE the clear */
    g_brHook81.pAA293C = NULL;
    g_pBrUiNav->pAA2904 = pNext;
    return 0;
}

/* ==========================================================================
 * Installation
 * ========================================================================== */

/* @n64 0x802173C8 located */
void BrUiHook81Install(BrUi73Hooks *pHooks)
{
    if (pHooks == NULL)
        return;

    pHooks->p10045AA0 = BrUiHook81_10045AA0;
    pHooks->p10045880 = BrUiHook81_10045880;
    pHooks->p100458A0 = BrUiHook81_100458A0;
    pHooks->p100450F0 = BrUiHook81_100450F0;

    pHooks->p100463C0 = BrUiHook81_100463C0;
    pHooks->p10046620 = BrUiHook81_10046620;
    pHooks->p10046EB0 = BrUiHook81_10046EB0;
    pHooks->p100470E0 = BrUiHook81_100470E0;
    /* 0x10046AD0, 0x10046B10 and 0x10046D70 have no slot in BrUi73Hooks and
     * need none: no builder installs them. They are poked into a control's
     * +0x08 at run time by the three installers above, which is the original's
     * own mechanism. */
}

/* ==========================================================================
 * 0x100770C0
 * ========================================================================== */

int g_18ABD38[14];   /* 0x118ABD38 */
int g_18ABAD4;       /* 0x118ABAD4 */
int g_18ABD80;       /* 0x118ABD80 */

/* WHAT IT DOES: zeroes 14 dwords at 0x118ABD38, writes 0 to 0x118ABAD4, and
 * writes 1 to 0x118ABD80. The three are separate globals, not one struct.
 *
 * GOTCHA: the two scalar stores are written first so MSVC 5 selects `C7 05`
 * immediates; it then schedules the `rep stosd` ahead of them. Writing the
 * zero-fill first CSE's the 0 into `mov [g_18ABAD4], eax`. */
/* @implements 0x100770C0 d3d BrSub100770C0 */
void BrSub100770C0(void)
{
    int i;

    g_18ABAD4 = 0;
    g_18ABD80 = 1;
    for (i = 0; i < 14; ++i)
        g_18ABD38[i] = 0;
}
