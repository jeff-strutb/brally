/* slice3_31.c -- another module's packet of BRD3D.dll.
 *
 * See slice3_31.h for what this module is and how it relates to slice2_25 /
 * slice2_26. Everything below is a straight transcription of the annotated
 * disassembly in work/slice3/agent31.asm.
 *
 * DEVIATIONS, all of them, gathered here and repeated at the line they apply
 * to:
 *
 *  1. STATE.  The original reaches every global by absolute address. This
 *     module holds two context pointers installed by BrPhase31SetCtx() and
 *     keeps the original argument lists intact. Portability only.
 *
 *  2. NAME COPIES.  The original's `rep movs` copies from 0x1039B720 are
 *     unbounded. Here they are truncated to BR_NAME31_LEN and always
 *     NUL-terminated. The only buffer size the code attests to is the 32
 *     bytes 0x10047340 zeroes at 0x10A9D618, so 32 is what is used.
 *
 *  3. THE 0x10044CB0 HOOK.  0x10046260 stores slice2_26's leave routine
 *     0x10044CB0 into a BrPhase's +0x08 slot. That slot's type takes one
 *     void*, but slice2_26 declares 0x10044CB0 with an added leading
 *     BrPhaseCtx*. A static thunk bridges the two; it has no address in the
 *     original and is deliberately not exported.
 *
 *  4. RETURN VALUES -- WAS A DEVIATION, AND WAS A BUG.  This entry used to
 *     read "all the LEAVE routines are declared void so they can be stored in
 *     BrPhase.pfnHook ... that no caller in the packet reads". The second
 *     half was false. The +0x08 slot's result IS read, by 0x10048180:
 *
 *         10048280  ff5608   call dword ptr [esi + 8]
 *         10048286  85c0     test eax, eax
 *         10048288  7508     jne  0x10048292
 *
 *     -- a zero return makes 0x10048180 return 0 and skip the rest of the
 *     row's frame. All forty routines here return 0, so the early exit is
 *     what every one of them causes, and `void` could not express it. They
 *     now return int32_t and BrPhaseHookFn_ carries it.
 *
 *     Still true, and still the DEVIATION: 0x10047360's several exits return
 *     1 and no caller in THIS packet reads it.
 *
 *  5. OUT-OF-MODEL FIELDS.  BrSub10047360 reaches +0x1E20C and +0x3850 of
 *     the game object, past the end of slice2_25.h's BrGameObj. They are
 *     read and written byte-wise through memcpy so no alignment or aliasing
 *     assumption is made; the caller must supply BR_GAMEOBJ31_MIN_SIZE bytes.
 */
/* NOTE: these five were renamed BrExt_* -> BrPhaseEnterPlaceholder_* during
 * integration because slice3_33 implements the same addresses with a different
 * signature (it adds a ctx parameter as a documented DEVIATION). The one-arg
 * hook form used here matches the ORIGINAL calling convention. */
#include "slice3_31.h"
#include "br_phaseact.h"   /* the one activate body -- see br_phaseact.h */
#include "br_phase.h"   /* BR_PHASE_ALLOC_SIZE */

#include <string.h>

/* ==========================================================================
 * 0x10008850 -- LoadPod(int, void *)
 * ========================================================================== */

void *BrPodLoadInto(BrPod *pPod, int iEntry, void *pvBuffer)
{
    /* The original reports "LoadPod: %i >= m_cNumPods" and then calls ReadPod
     * anyway; br_pod.c's BrPodRead applies the same bounds test and refuses,
     * so the read does not happen. The buffer comes back either way. */
    (void)BrPodRead(pPod, iEntry, pvBuffer);
    return pvBuffer;
}

/* ==========================================================================
 * Module state (DEVIATION 1)
 * ========================================================================== */

static BrPhaseCtx   *g_pBase;
static BrPhaseCtx31 *g_pExt;

void BrPhase31SetCtx(BrPhaseCtx *pBase, BrPhaseCtx31 *pExt)
{
    g_pBase = pBase;
    g_pExt  = pExt;
}

/* ==========================================================================
 * Shared shapes
 * ========================================================================== */

    /* HARDENING (port): allocate what THIS build's object needs.
     *
     * The original allocates 0xC8 and that is right for a 32-bit build.
     * It is not right here: br_phase.h's BrPhase_ is ~300 bytes on LP64
     * because every pointer widened, and 0x10048710 writes the whole
     * object. Today this module uses the 5-field partial view and the
     * real constructor is NOT wired to these sites, so nothing
     * overflows -- but it has 37 call sites waiting, and the moment it
     * is wired a 0xC8 block becomes a ~100-byte heap overflow per phase
     * activation. BR_PHASE_ALLOC_SIZE is max(sizeof, 0xC8), so this is
     * never smaller than the original either.
     */
/* operator new(0xC8) -- NOT zeroed -- followed by the constructor, exactly as
 * every activate routine in this range and in slice2_26 spells it out. */
static BrPhase *Br31NewPhase(void)
{
    BrPhase *p = (BrPhase *)BrOperatorNew(BR_PHASE_ALLOC_SIZE);
    return (p != NULL) ? BrOptObjCtor(p) : NULL;
}

/* The body every activate routine shares once its prologue has run.
 *
 * THIS IS NO LONGER A SECOND TRANSCRIPTION.  slice2_26.c models the same
 * inlined sequence for its own range as BrPhaseActivateSlot, and for a long
 * time this file carried an independent copy of the same instructions. The
 * two DISAGREED: slice2_26's wrapped the two flag stores in a NULL test that
 * the original does not have, and this file's did not. Both are now the one
 * body, which stores unconditionally as 0x1004559C / 0x100455AB do.
 *
 * What is left here is the calling shape this file's routines want: a 1/0
 * "is the phase available" plus a separate "was it built here", against
 * slice2_26's three-valued result.
 *
 * ppSlot is the per-phase singleton. Returns 1 when the phase is available
 * (already built or just built), 0 when the allocation failed. *pfBuilt is
 * set only on the just-built path, which is the path the per-phase epilogue
 * is allowed to run on. */
static int Br31Activate(BrPhase **ppSlot, BrPhaseEnterFn pfnEnter, int *pfBuilt)
{
    BrActResult r = BrPhaseActivateSlot(g_pBase, ppSlot, pfnEnter);

    *pfBuilt = (r == BR_ACT_CREATED) ? 1 : 0;
    return (r == BR_ACT_FAILED) ? 0 : 1;   /* note: 0, not 1, on failure */
}

/* The second, flag-light object that 0x10045C90 and 0x10045F70 build after
 * their first one: f0C is set, f68 is NOT. Returns 0 on allocation failure. */
static int Br31ActivateSecond(BrPhase **ppSlot, BrPhaseEnterFn pfnEnter)
{
    BrPhase *p = Br31NewPhase();

    *ppSlot = p;
    if (p == NULL)
        return 0;

    p->pfnEnter = pfnEnter;
    (*ppSlot)->pfnEnter(*ppSlot);      /* re-read */
    (*ppSlot)->f0C = 1;             /* re-read */
    return 1;
}

/* The prologue every LEAVE routine shares: drive the game object's +0x2AE8
 * sub-object through vtable slot +0x1C, then notify the current phase through
 * ITS slot +0x00 with the argument 1. The NULL test is on the phase only --
 * the sub-object is dereferenced unguarded, as in the original. */
static void Br31LeavePrologue(void *pEntity)
{
    BrGameObj *pObj = (BrGameObj *)pEntity;
    BrPhase   *pCur;

    pObj->pSub->pVtbl->pfnSlot7(pObj->pSub);

    pCur = BR_PHASE_CUR;
    if (pCur != NULL)
        pCur->pVtbl->f00(pCur, 1);
}

/* DEVIATION 2: the original's copies are unbounded inline `rep movs` -- a
 * `repne scasb` measures the source and the length is then trusted. Here the
 * scan itself is bounded to BR_NAME31_LEN, so an unterminated source cannot
 * run away, and the destination is always NUL-terminated. */
static void Br31CopyName(char *pszDst, const char *pszSrc)
{
    const char *pEnd = (const char *)memchr(pszSrc, '\0', (size_t)BR_NAME31_LEN);
    size_t      n    = (pEnd != NULL) ? (size_t)(pEnd - pszSrc)
                                      : (size_t)BR_NAME31_LEN - 1u;

    memcpy(pszDst, pszSrc, n);
    pszDst[n] = '\0';
}

/* The block the eight "reset the name" LEAVE routines run between the
 * prologue and the repoint. 0x10046E10 uses a different set of globals and
 * so does not call this. */
static void Br31ResetName(void)
{
    g_pExt->pAA2928 = NULL;
    g_pExt->nAA29C0 = 0;
    g_pExt->nAA29CC = 0;
    g_pExt->nAA28E4 = 0;
    g_pExt->n0AB3F4 = -1;
    Br31CopyName(g_pExt->szAA2518, g_pExt->sz39B720);
    Br31CopyName(g_pExt->szA9D618, g_pExt->sz39B720);
}

/* Tear a phase down through vtable slot +0x1C and forget it. */
static void Br31DestroyPhase(BrPhase **ppSlot)
{
    BrPhase *p = *ppSlot;

    if (p != NULL) {
        /* Slot +0x1C == 0x10048AA0. This used to go through a BrPhaseVtblExt
         * cast because slice2_26.h's BrPhaseVtbl had only slot +0x00; the
         * merged BrPhaseVtbl_ has all nine, so the cast is gone. */
        p->pVtbl->f1C(p);
        *ppSlot = NULL;
    }
}

/* Notify a phase through slot +0x00 with 1 and forget it. */
static void Br31NotifyAndClear(BrPhase **ppSlot)
{
    BrPhase *p = *ppSlot;

    if (p != NULL)
        p->pVtbl->f00(p, 1);
    *ppSlot = NULL;
}

/* DEVIATION 3: slice2_26's 0x10044CB0 wants a context as its first argument;
 * BrPhase.pfnHook can only pass one void*. */
/* WHAT IT DOES: an adapter, nothing more. Menu rows can only call a routine
 * that takes one argument, so this supplies the shared context that the real
 * leave routine needs alongside it. */
/* @implements 0x10044CB0 d3d Br31Thunk_10044CB0 */
static int32_t Br31Thunk_10044CB0(void *pEntity)
{
    (void)BrPhaseLeave_10044CB0(g_pBase, pEntity);
    return 0;
}

/* ==========================================================================
 * 0x10045780-0x100458E0 -- the twelve HOOK installers
 * ========================================================================== */

#define BR31_HOOK_C8(fn, leave)                                   \
    int fn(void *pArg)                                            \
    {                                                             \
        (void)BrPhaseActivate_100451E0(g_pBase);   /* ignores pArg */ \
        (void)pArg;                                               \
        g_pExt->pAA29C8->pfnHook = (leave);                         \
        return 1;                                                 \
    }

#define BR31_HOOK_F4(fn, leave)                                   \
    int fn(void *pArg)                                            \
    {                                                             \
        (void)BrPhaseActivate_10045BC0();          /* ignores pArg */ \
        (void)pArg;                                               \
        g_pBase->pAA29F4->pfnHook = (leave);                        \
        return 1;                                                 \
    }

BR31_HOOK_C8(BrPhaseHook_10045780, BrPhaseLeave_10046750)
BR31_HOOK_F4(BrPhaseHook_100457A0, BrPhaseLeaveNamed_10046790)
BR31_HOOK_C8(BrPhaseHook_100457C0, BrPhaseLeave_10046830)
BR31_HOOK_F4(BrPhaseHook_100457E0, BrPhaseLeaveNamed_10046870)
BR31_HOOK_C8(BrPhaseHook_10045800, BrPhaseLeave_10046910)
BR31_HOOK_F4(BrPhaseHook_10045820, BrPhaseLeaveNamed_10046950)
BR31_HOOK_C8(BrPhaseHook_10045840, BrPhaseLeave_100469F0)
BR31_HOOK_F4(BrPhaseHook_10045860, BrPhaseLeaveNamed_10046A30)
BR31_HOOK_C8(BrPhaseHook_10045880, BrPhaseLeave_10046AD0)
BR31_HOOK_F4(BrPhaseHook_100458A0, BrPhaseLeaveNamed_10046B10)
BR31_HOOK_C8(BrPhaseHook_100458C0, BrPhaseLeave_10046BB0)
BR31_HOOK_F4(BrPhaseHook_100458E0, BrPhaseLeaveNamed_10046BF0)

/* ==========================================================================
 * ACTIVATE routines
 * ========================================================================== */

/* 0x10045900 */
/* WHAT IT DOES: brings up a screen that needs the game CD in the drive. It
 * checks for the disc first, and if it is not there it puts up the "please
 * insert Boss Rally CD" message and refuses to open the screen at all. */
/* @implements 0x10045900 d3d BrPhaseActivate_10045900 */
int BrPhaseActivate_10045900(void)
{
    int fBuilt;

    if (BrExt_10045A00() == 0) {
        BrExt_100419D0(BrExt_10074030(0xD));
        return 0;
    }
    BrExt_100419D0(g_pBase->p0AD300);

    return Br31Activate(&g_pExt->pAA291C, BrExt_1004F2B0, &fBuilt);
}

/* 0x10045AA0 -- an installer, not an activate, but it lives here in the
 * original's layout. */
/* WHAT IT DOES: opens a screen, clears the shared text buffer and the season
 * block behind it, wires the new screen's Back row so the player can get out,
 * and puts the menus back in their default mode. */
/* @implements 0x10045AA0 d3d BrPhaseHook_10045AA0 */
int BrPhaseHook_10045AA0(void *pArg)
{
    g_pBase->n0AA010 = 0;
    BrExt_1003E680();
    g_pExt->nACED34 = 0;
    BrExt_10045C90(pArg);
    g_pBase->pAA29B0->pfnHook = BrPhaseLeave_10046D70;
    g_pBase->n0AA010 = 0;
    BrExt_1003E510();
    return 1;
}

/* 0x10045AF0 */
/* WHAT IT DOES: brings up the "load championship season" screen, building it
 * the first time and just making it current thereafter. */
/* @implements 0x10045AF0 d3d BrPhaseActivate_10045AF0 */
int BrPhaseActivate_10045AF0(void)
{
    int fBuilt;
    return Br31Activate(&g_pExt->pAA2924, BrExt_1004F700, &fBuilt);
}

/* 0x10045BC0 */
/* WHAT IT DOES: brings up the screen the season-save button leads to,
 * building it the first time. */
/* @implements 0x10045BC0 d3d BrPhaseActivate_10045BC0 */
int BrPhaseActivate_10045BC0(void)
{
    int fBuilt;
    return Br31Activate(&g_pExt->pAA2928, BrExt_10050060, &fBuilt);
}

/* 0x10045C90 -- pre-declared by slice2_26.h. Two objects; the second is built
 * only on the just-built path of the first. The original returns 1, or 0 if
 * either allocation failed. */
/* WHAT IT DOES: opens a menu screen and, the first time round only, also
 * builds the "are you sure you want to exit the season?" confirmation behind it
 * so it is ready when the player asks to leave. If the screen was already open
 * the confirmation is not rebuilt. It ignores the argument it is handed. */
/* @implements 0x10045C90 d3d BrExt_10045C90 */
void BrExt_10045C90(void *p)
{
    int fBuilt;

    (void)p;    /* the original reads no argument */

    if (!Br31Activate(&g_pExt->pAA292C, BrExt_100509F0, &fBuilt))
        return;
    if (!fBuilt)
        return;

    (void)Br31ActivateSecond(&g_pExt->pAA2974, BrExt_10049F40);
}

/* 0x10045DC0 */
/* WHAT IT DOES: brings up the season-progress screen, copying the current
 * round number into the slot that screen reads from first. */
/* @implements 0x10045DC0 d3d BrPhaseActivate_10045DC0 */
int BrPhaseActivate_10045DC0(void)
{
    int fBuilt;

    g_pExt->nAA28AC = g_pExt->nAA28A4;

    return Br31Activate(&g_pExt->pAA2930, BrExt_10052030, &fBuilt);
}

/* 0x10045EA0 */
/* WHAT IT DOES: brings one particular menu screen up, building it if it is not
 * already there. Which screen it is was not established here. */
/* @implements 0x10045EA0 d3d BrPhaseActivate_10045EA0 */
int BrPhaseActivate_10045EA0(void)
{
    int fBuilt;
    return Br31Activate(&g_pExt->pAA2934, BrExt_10052F50, &fBuilt);
}

/* 0x10045F70 -- the twin of 0x10045C90. */
/* WHAT IT DOES: opens a menu screen and, only when that screen has to be built
 * for the first time, also builds a second screen behind it -- the same
 * two-at-once pattern used for a screen and its confirmation. Which pair these
 * are was not established here. */
/* @implements 0x10045F70 d3d BrPhaseActivate_10045F70 */
int BrPhaseActivate_10045F70(void)
{
    int fBuilt;

    if (!Br31Activate(&g_pExt->pAA2938, BrExt_10053CF0, &fBuilt))
        return 0;
    if (!fBuilt)
        return 1;

    return Br31ActivateSecond(&g_pExt->pAA2978, BrExt_1004A260);
}

/* 0x100460A0 */
/* WHAT IT DOES: brings one particular menu screen up, building it if needed.
 * Which screen it is was not established here. */
/* @implements 0x100460A0 d3d BrPhaseActivate_100460A0 */
int BrPhaseActivate_100460A0(void)
{
    int fBuilt;
    return Br31Activate(&g_pExt->pAA293C, BrExt_10054B50, &fBuilt);
}

/* 0x10046170 */
/* WHAT IT DOES: brings up a menu screen and switches the background music over
 * to a different track as it does so -- and it does that switch every time,
 * including when the screen was already open. */
/* @implements 0x10046170 d3d BrPhaseActivate_10046170 */
int BrPhaseActivate_10046170(void)
{
    int fBuilt;

    BrExt_100419D0(g_pBase->p0AD300);
    BrExt_10072AF0(3, 0x00200020u);
    g_pExt->nAA2854 = 3;            /* both paths, including already-built */

    return Br31Activate(&g_pExt->pAA2910, BrExt_10049C20, &fBuilt);
}

/* 0x10046260 */
/* WHAT IT DOES: opens the screen that the game leaves the menus through,
 * resetting a good deal of shared state on the way: the menu mode, the season
 * block, a marker byte and two guard flags. The three final set-up steps and
 * the wiring of its exit row happen only when the screen has to be built. */
/* @implements 0x10046260 d3d BrPhaseActivate_10046260 */
int BrPhaseActivate_10046260(void)
{
    int fBuilt;

    g_pBase->n0AA010 = 2;
    BrExt_1003E680();
    g_pExt->nACED34  = 0;
    g_pExt->nAD0984  = 1;
    g_pBase->n0AA010 = 2;           /* stored twice by the original */
    g_pBase->n0AC304 = 1;
    g_pExt->b680738  = 0xFF;

    /* The original repeats the n0AC304 store on the not-yet-built path only.
     * The value is the same, so this is observably a no-op -- it is kept so
     * the transcription stays literal. */
    if (g_pBase->pAA290C == NULL)
        g_pBase->n0AC304 = 1;

    if (!Br31Activate(&g_pBase->pAA290C, BrPhaseEnterPlaceholder_1004B430, &fBuilt))
        return 0;
    if (!fBuilt)
        return 1;

    BrExt_10008B80();               /* a bare `ret` in this build */
    BrExt_1003DFC0();
    BrExt_1003E510();
    g_pExt->pAA29AC->pfnHook = Br31Thunk_10044CB0;   /* 0x10044CB0 */
    return 1;
}

/* 0x10046380 */
/* WHAT IT DOES: opens a screen with one guard flag suppressed across the
 * opening, wires the resulting screen's Back row, and leaves the menus in a
 * particular mode. */
/* @implements 0x10046380 d3d BrPhaseHook_10046380 */
int BrPhaseHook_10046380(void *pArg)
{
    g_pBase->n0AC304 = 0;
    (void)BrPhaseActivate_10045110(g_pBase);    /* ignores pArg */
    (void)pArg;
    g_pBase->n0AC304 = 1;
    g_pBase->pAA29B4->pfnHook = BrPhaseLeave_10046D20;
    g_pBase->n0AA010 = 2;
    return 1;
}

/* ==========================================================================
 * LEAVE routines
 * ==========================================================================
 *
 * The next-phase pointer is read out of its global BEFORE the clears and
 * written into pAA2904 after them, which is the original's order.
 */

/* RETURN VALUE: every routine this macro generates ends in `xor eax, eax`,
 * so it returns 0, and that 0 is READ -- 0x10048180 does `test eax,eax / jne`
 * on the +0x08 slot's result and returns 0 itself when it is zero. The macro
 * used to generate `void` and the value was lost for the whole family.
 * Verified one address at a time; the instruction is listed in slice3_31.h. */
#define BR31_LEAVE(fn, next, clears)                    \
    int32_t fn(void *pEntity)                           \
    {                                                   \
        BrPhase *pNext;                                 \
        Br31LeavePrologue(pEntity);                     \
        pNext = (next);                                 \
        clears                                          \
        BR_PHASE_CUR = pNext;                       \
        return 0;                                       \
    }

BR31_LEAVE(BrPhaseLeave_100463C0, g_pExt->pAA2958,
           g_pBase->pAA2940 = NULL;)

/* 0x10046400 -- pre-declared by slice2_25.h. */
/* WHAT IT DOES: leaves a screen, forgetting it and clearing three counters
 * that went with it, and hands the player back to a remembered screen. */
/* @implements 0x10046400 d3d BrSub10046400 */
int32_t BrSub10046400(BrGameObj *p)
{
    BrPhase *pNext;

    Br31LeavePrologue(p);
    pNext = g_pExt->pAA2950;
    g_pBase->pAA2954 = NULL;
    g_pExt->nAA29E4  = 0;
    g_pExt->nAA29E0  = 0;
    g_pExt->nAA285C  = 0;
    BR_PHASE_CUR = pNext;
    return 0;
}

BR31_LEAVE(BrPhaseLeave_10046450, g_pBase->pAA2908,
           g_pBase->pAA290C = NULL; g_pExt->pAA29AC = NULL;)
BR31_LEAVE(BrPhaseLeave_100464A0, g_pBase->pAA2908,
           g_pExt->pAA2910 = NULL;)
BR31_LEAVE(BrPhaseLeave_100464E0, g_pBase->pAA290C,
           g_pBase->pAA2914 = NULL;)
BR31_LEAVE(BrPhaseLeave_10046520, g_pBase->pAA2908,
           g_pBase->pAA2918 = NULL;)
BR31_LEAVE(BrPhaseLeave_100465A0, g_pBase->pAA2918,
           g_pBase->pAA297C = NULL;)
BR31_LEAVE(BrPhaseLeave_100465E0, g_pBase->pAA2918,
           g_pBase->pAA2980 = NULL;)
BR31_LEAVE(BrPhaseLeave_10046620, g_pBase->pAA2980,
           g_pBase->pAA2990 = NULL; g_pExt->nAA29F0 = 0;)
BR31_LEAVE(BrPhaseLeave_10046670, g_pBase->pAA2980,
           g_pBase->pAA2994 = NULL; g_pExt->nAA29EC = 0;)
BR31_LEAVE(BrPhaseLeave_10046710, g_pBase->pAA2918,
           g_pBase->pAA2988 = NULL;)
BR31_LEAVE(BrPhaseLeave_10046750, g_pExt->pAA292C,
           g_pBase->pAA2918 = NULL;)
BR31_LEAVE(BrPhaseLeave_10046830, g_pExt->pAA2930,
           g_pBase->pAA2918 = NULL;)
BR31_LEAVE(BrPhaseLeave_10046910, g_pExt->pAA2934,
           g_pBase->pAA2918 = NULL;)
BR31_LEAVE(BrPhaseLeave_100469F0, g_pExt->pAA2938,
           g_pBase->pAA2918 = NULL;)
BR31_LEAVE(BrPhaseLeave_10046AD0, g_pExt->pAA293C,
           g_pBase->pAA2918 = NULL;)
BR31_LEAVE(BrPhaseLeave_10046BB0, g_pBase->pAA2914,
           g_pBase->pAA2918 = NULL;)
BR31_LEAVE(BrPhaseLeave_10046C90, g_pBase->pAA2908,
           g_pExt->pAA291C = NULL;)
BR31_LEAVE(BrExt_10046CD0,        g_pExt->pAA2930,
           g_pBase->pAA2914 = NULL; g_pBase->pAA29B4 = NULL;)
BR31_LEAVE(BrPhaseLeave_10046D20, g_pBase->pAA295C,
           g_pBase->pAA2914 = NULL; g_pBase->pAA29B4 = NULL;)
BR31_LEAVE(BrPhaseLeave_10046D70, g_pExt->pAA291C,
           g_pExt->pAA292C = NULL; g_pBase->pAA29B0 = NULL;
           g_pExt->pAA2974 = NULL;)
BR31_LEAVE(BrExt_10046DC0,        g_pExt->pAA2924,
           g_pExt->pAA292C = NULL; g_pBase->pAA29B0 = NULL;
           g_pExt->pAA2974 = NULL;)
BR31_LEAVE(BrPhaseLeave_10047060, g_pExt->pAA292C,
           g_pExt->pAA2930 = NULL;)
BR31_LEAVE(BrPhaseLeave_100470A0, g_pExt->pAA2934,
           g_pExt->pAA2938 = NULL;)
BR31_LEAVE(BrPhaseLeave_100470E0, g_pExt->pAA2938,
           g_pExt->pAA293C = NULL;)

/* The two LEAVE routines with a tail the macro cannot carry. Both put the
 * tail call AFTER the repoint of pAA2904. */

/* 0x10046560 */
/* WHAT IT DOES: leaves a screen and, on the way out, switches the wheel's
 * force feedback off -- so this is the exit from a screen that had it
 * running. */
/* @implements 0x10046560 d3d BrPhaseLeave_10046560 */
int32_t BrPhaseLeave_10046560(void *pEntity)
{
    BrPhase *pNext;

    Br31LeavePrologue(pEntity);
    pNext = g_pBase->pAA297C;
    g_pExt->pAA2998 = NULL;
    BR_PHASE_CUR = pNext;

    BrExt_10079550();
    return 0;
}

/* 0x100466C0 */
/* WHAT IT DOES: leaves a screen and writes the settings out to disk as it
 * goes, so changes made on it survive. */
/* @implements 0x100466C0 d3d BrPhaseLeave_100466C0 */
int32_t BrPhaseLeave_100466C0(void *pEntity)
{
    BrPhase *pNext;

    Br31LeavePrologue(pEntity);
    pNext = g_pBase->pAA2918;
    g_pBase->pAA2984 = NULL;
    BR_PHASE_CUR = pNext;

    BrExt_1003E310();
    /* __thiscall: `this` is 0x10B4DF30, its one argument 0x10B4FBE8. */
    BrExt_1006A4A0(g_pExt->pB4DF30, g_pExt->pB4FBE8);
    return 0;
}

/* --- the eight LEAVE routines that also reset the player name ------------ */

/* RETURN VALUE: 0, same as BR31_LEAVE, but reached differently and so worth
 * stating separately. These eight have no `xor eax, eax` in the return slot;
 * the zero is left over from the `repne scasb` scan character that the second
 * inlined strcpy sets up (0x10046818, 0x100468F8, 0x100469D8, 0x10046AB8,
 * 0x10046B98, 0x10046C78, 0x10046E8C, 0x10046F38 -- one per routine), and
 * nothing between there and the `ret` touches eax. So the value is 0 by
 * accident of the compiler's scheduling rather than by an explicit store, and
 * it is 0 all the same. */
#define BR31_LEAVE_NAMED(fn, next)                      \
    int32_t fn(void *pEntity)                           \
    {                                                   \
        BrPhase *pNext;                                 \
        Br31LeavePrologue(pEntity);                     \
        Br31ResetName();                                \
        pNext = (next);                                 \
        BR_PHASE_CUR = pNext;                       \
        return 0;                                       \
    }

BR31_LEAVE_NAMED(BrPhaseLeaveNamed_10046790, g_pExt->pAA292C)
BR31_LEAVE_NAMED(BrPhaseLeaveNamed_10046870, g_pExt->pAA2930)
BR31_LEAVE_NAMED(BrPhaseLeaveNamed_10046950, g_pExt->pAA2934)
BR31_LEAVE_NAMED(BrPhaseLeaveNamed_10046A30, g_pExt->pAA2938)
BR31_LEAVE_NAMED(BrPhaseLeaveNamed_10046B10, g_pExt->pAA293C)
BR31_LEAVE_NAMED(BrPhaseLeaveNamed_10046BF0, g_pBase->pAA2914)
BR31_LEAVE_NAMED(BrPhaseLeaveNamed_10046EB0, g_pExt->pAA2934)

/* 0x10046E10 -- clears a different set of globals from the other seven. */
/* WHAT IT DOES: leaves a screen and resets the name being edited back to the
 * shared working copy -- but it clears a different, smaller set of state than
 * the seven other name-resetting exits do, so it is not simply another one of
 * them. */
/* @implements 0x10046E10 d3d BrPhaseLeaveNamed_10046E10 */
int32_t BrPhaseLeaveNamed_10046E10(void *pEntity)
{
    BrPhase *pNext;

    Br31LeavePrologue(pEntity);

    g_pExt->pAA2924 = NULL;
    g_pExt->nAA28E0 = 0;
    g_pExt->n0AB3F4 = -1;
    Br31CopyName(g_pExt->szAA2518, g_pExt->sz39B720);
    Br31CopyName(g_pExt->szA9D618, g_pExt->sz39B720);

    pNext = g_pExt->pAA291C;
    BR_PHASE_CUR = pNext;
    return 0;
}

/* --- the three one-statement gotos ---------------------------------------- */

/* WHAT IT DOES: switches straight to one particular screen without closing
 * anything down first -- a jump, not a leave. */
/* @implements 0x10046F50 d3d BrPhaseGoto_10046F50 */
int32_t BrPhaseGoto_10046F50(void) { BR_PHASE_CUR = g_pExt->pAA2974; return 0; }
/* WHAT IT DOES: switches straight to a different particular screen, again with
 * no teardown. */
/* @implements 0x10046FC0 d3d BrPhaseGoto_10046FC0 */
int32_t BrPhaseGoto_10046FC0(void) { BR_PHASE_CUR = g_pExt->pAA292C; return 0; }
/* WHAT IT DOES: the third of the plain jumps -- makes one particular screen
 * current and nothing else. */
/* @implements 0x10047050 d3d BrPhaseGoto_10047050 */
int32_t BrPhaseGoto_10047050(void) { BR_PHASE_CUR = g_pExt->pAA293C; return 0; }

/* --- LEAVE routines with a different shape -------------------------------- */

/* 0x10046F60 */
/* WHAT IT DOES: leaves a screen all the way back to the root menu, releasing
 * the confirmation screen that was sitting behind it. It briefly leaves no
 * screen current at all, which the release runs inside -- so anything that
 * release does sees no current screen. */
/* @implements 0x10046F60 d3d BrPhaseLeave_10046F60 */
int32_t BrPhaseLeave_10046F60(void *pEntity)
{
    BrPhase *pSaved;

    Br31LeavePrologue(pEntity);

    pSaved = g_pExt->pAA292C;
    BR_PHASE_CUR = NULL;        /* visible only to the notify below */
    g_pExt->pAA2974  = NULL;
    if (pSaved != NULL) {
        pSaved->pVtbl->f00(pSaved, 1);
        g_pExt->pAA292C = NULL;
    }
    BR_PHASE_CUR = g_pBase->pAA2908;
    return 0;
}

/* 0x10046FD0 */
/* WHAT IT DOES: leaves a screen and throws away three other screens with it,
 * then returns the player to the root menu. This is the exit that unwinds a
 * whole branch of the menus rather than one step. */
/* @implements 0x10046FD0 d3d BrPhaseLeave_10046FD0 */
int32_t BrPhaseLeave_10046FD0(void *pEntity)
{
    Br31DestroyPhase(&g_pExt->pAA2934);
    Br31DestroyPhase(&g_pExt->pAA2938);
    Br31DestroyPhase(&g_pExt->pAA293C);

    Br31LeavePrologue(pEntity);

    g_pExt->pAA2974  = NULL;
    BR_PHASE_CUR = g_pBase->pAA2908;
    return 0;
}

/* 0x10047120 */
/* WHAT IT DOES: leaves a screen and, if nothing has been entered yet, wipes
 * the three blocks of entered data behind it clean. It does NOT change which
 * screen is current -- whatever opens next has to say so itself. */
/* @implements 0x10047120 d3d BrPhaseLeave_10047120 */
int32_t BrPhaseLeave_10047120(void *pEntity)
{
    BrGameObj *pObj = (BrGameObj *)pEntity;

    BrExt_10045C90(pEntity);

    if (g_pExt->nAA26F0 > 0 && g_pExt->bAA26F4 == 0 && g_pExt->bAA26F5 == 0) {
        memset(g_pExt->aAA26F6, 0, sizeof(g_pExt->aAA26F6));   /*  6 dwords */
        memset(g_pExt->aAA270E, 0, sizeof(g_pExt->aAA270E));   /* 12 dwords */
        memset(g_pExt->aAA2740, 0, sizeof(g_pExt->aAA2740));   /* 24 dwords */
    }
    g_pExt->nAA28C4 = 0;

    pObj->pSub->pVtbl->pfnSlot7(pObj->pSub);
    Br31NotifyAndClear(&g_pExt->pAA296C);   /* pAA2904 is NOT touched */
    return 0;
}

/* 0x100471B0 */
/* WHAT IT DOES: leaves a screen, opening the screen behind it and letting go
 * of one other, without changing which screen is current. */
/* @implements 0x100471B0 d3d BrPhaseLeave_100471B0 */
int32_t BrPhaseLeave_100471B0(void *pEntity)
{
    BrGameObj *pObj = (BrGameObj *)pEntity;

    BrExt_10045C90(pEntity);
    pObj->pSub->pVtbl->pfnSlot7(pObj->pSub);
    Br31NotifyAndClear(&g_pExt->pAA2970);
    return 0;
}

/* 0x10047290 */
/* WHAT IT DOES: leaves a screen and unwinds two others with it, then takes one
 * of three different routes out depending on which of two markers is set -- and
 * clears that marker as it goes, so the special route is taken once only. */
/* @implements 0x10047290 d3d BrPhaseLeave_10047290 */
int32_t BrPhaseLeave_10047290(void *pEntity)
{
    BrGameObj *pObj = (BrGameObj *)pEntity;

    BrExt_1005FBC0(1);
    Br31DestroyPhase(&g_pExt->pAA2934);
    Br31DestroyPhase(&g_pExt->pAA2938);

    if (g_pExt->nAA28B0 != 0) {
        BrExt_10043260(pEntity);
        g_pExt->nAA28B0 = 0;
    } else if (g_pExt->nAA28B4 != 0) {
        BrExt_10043330(pEntity);
        g_pExt->nAA28B4 = 0;
    } else {
        BrExt_10045C90(pEntity);
    }

    pObj->pSub->pVtbl->pfnSlot7(pObj->pSub);
    Br31NotifyAndClear(&g_pExt->pAA293C);
    return 0;
}

/* ==========================================================================
 * Small helpers
 * ========================================================================== */

/* 0x100471F0 */
/* WHAT IT DOES: the per-frame check that gets the player out of a screen when
 * whatever it was waiting on has finished or been cancelled: if so it leaves
 * the screen and tells the caller to stop, otherwise it lets the frame carry
 * on. */
/* @implements 0x100471F0 d3d BrPhaseGuard_100471F0 */
int BrPhaseGuard_100471F0(void *pEntity)
{
    if (BrExt_1003E0E0() != 0) {
        BrPhaseLeave_10047120(pEntity);
        return -1;
    }
    return 1;
}

/* 0x10047210 */
/* WHAT IT DOES: handles the player acting on an editable row, taking one of
 * two different routes depending on whether an edit is already in progress. If
 * neither applies it does nothing and lets the frame continue -- and only that
 * do-nothing path leaves the pending keystroke unconsumed. */
/* @implements 0x10047210 d3d BrPhaseEdit_10047210 */
int BrPhaseEdit_10047210(void *pArg)
{
    if (g_pExt->nAA2AD4 != 0) {
        (void)BrExt_10041A00(pArg);
    } else if (BrExt_1003E0E0() != 0) {
        (void)BrExt_10041AC0(pArg);
    } else {
        return 1;                   /* nAA33E4 is NOT cleared on this path */
    }
    g_pExt->nAA33E4 = 0;
    return -1;
}

/* 0x10047250 */
/* WHAT IT DOES: the same two-way edit handling as its neighbour above, but for
 * a different kind of row, with its own pair of handlers. */
/* @implements 0x10047250 d3d BrPhaseEdit_10047250 */
int BrPhaseEdit_10047250(void *pArg)
{
    if (g_pExt->nAA2AD4 != 0) {
        (void)BrExt_10042410(pArg);
    } else if (BrExt_1003E0E0() != 0) {
        (void)BrExt_100424D0(pArg);
    } else {
        return 1;
    }
    g_pExt->nAA33E4 = 0;
    return -1;
}

/* 0x10047340 */
/* WHAT IT DOES: wipes the name the player has been typing back to empty and
 * clears the two markers that went with it, so the field starts over. */
/* @implements 0x10047340 d3d BrPhaseNameClear_10047340 */
int BrPhaseNameClear_10047340(void)
{
    memset(g_pExt->szA9D618, 0, sizeof(g_pExt->szA9D618));   /* 8 dwords */
    g_pExt->nAA28A4 = 0;
    g_pExt->bAA26F5 = 0;
    return 1;
}

/* --- byte-wise access to the game object beyond slice2_25.h's model ------- */

static uint32_t Br31Ld32(const void *pObj, unsigned off)
{
    uint32_t v;
    memcpy(&v, (const unsigned char *)pObj + off, sizeof(v));
    return v;
}

static void Br31St32(void *pObj, unsigned off, uint32_t v)
{
    memcpy((unsigned char *)pObj + off, &v, sizeof(v));
}

static uint16_t Br31Ld16(const void *pObj, unsigned off)
{
    uint16_t v;
    memcpy(&v, (const unsigned char *)pObj + off, sizeof(v));
    return v;
}

static void Br31St16(void *pObj, unsigned off, uint16_t v)
{
    memcpy((unsigned char *)pObj + off, &v, sizeof(v));
}

static void Br31St8(void *pObj, unsigned off, uint8_t v)
{
    ((unsigned char *)pObj)[off] = v;
}

/* The index table at 0x10047470, read out of the DLL. It maps
 * (counter - 2) in [0,0x32] to one of the five arms of the jump table at
 * 0x1004745C. Arm 4 is the same code as the range check's default. */
static const unsigned char g_aJump10047470[0x33] = {
    0, 1, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3
};

/* 0x10047360 -- pre-declared by slice2_25.h. Original returns int; see
 * DEVIATION 4 and the table in slice3_31.h. */
/* WHAT IT DOES: makes the highlighted menu row pulse. Every sixty milliseconds
 * it steps that row's lettering on through grey, white and mid-grey and then
 * back to the start, so the selection breathes rather than sitting still; if
 * the mouse is over the row it jumps straight to yellow instead and the cycle
 * is skipped. Rows that are disabled or hidden are left entirely alone. */
/* @implements 0x10047360 d3d BrSub10047360 */
void BrSub10047360(BrGameObj *p)
{
    uint32_t uFlags;
    int32_t  iCase;
    uint16_t uCount;

    uFlags = Br31Ld32(p, BR_GAMEOBJ_OFF_FLAGS);
    if ((uFlags & BR_GAMEOBJ_FLAG_10) != 0)
        return;                                     /* original: 0 */
    if ((uFlags & BR_GAMEOBJ_FLAG_1000000) != 0)
        return;                                     /* original: 0 */
    if ((Br31Ld32(p, BR_GAMEOBJ_OFF_F3850) & BR_GAMEOBJ_FLAG_1000000) != 0)
        return;                                     /* original: 0 */

    if (g_pExt->nAA284C != 0) {
        /* No NULL guard in the original, and none added. */
        const BrObjAA2E80 *q = g_pExt->pAA2E80;
        if (q->f2C != 0 || q->f30 != 0 || q->f34 != 0 || q->f38 != 0) {
            Br31St8(p, BR_GAMEOBJ_OFF_STATE, 4);
            return;                                 /* original: 1 -- and
                                                     * uFlags is NOT stored */
        }
    }

    if ((uFlags & BR_GAMEOBJ_FLAG_100) == 0)
        return;                                     /* original: 1 */

    uCount = (uint16_t)(Br31Ld16(p, BR_GAMEOBJ_OFF_COUNT) + 1);
    Br31St16(p, BR_GAMEOBJ_OFF_COUNT, uCount);

    /* movsx to 32 bits, then -2, then an UNSIGNED compare against 0x32:
     * counts of 0 and 1 wrap negative and land in the default. */
    iCase = (int32_t)(int16_t)uCount - 2;

    uFlags &= ~BR_GAMEOBJ_FLAG_100;

    if ((uint32_t)iCase <= 0x32u) {
        switch (g_aJump10047470[(uint32_t)iCase]) {
        case 0: Br31St8(p, BR_GAMEOBJ_OFF_STATE, 0); break;
        case 1: Br31St8(p, BR_GAMEOBJ_OFF_STATE, 1); break;
        case 2: Br31St8(p, BR_GAMEOBJ_OFF_STATE, 2); break;
        case 3: Br31St8(p, BR_GAMEOBJ_OFF_STATE, 4); break;
        default: Br31St16(p, BR_GAMEOBJ_OFF_COUNT, 2); break;
        }
    } else {
        Br31St16(p, BR_GAMEOBJ_OFF_COUNT, 2);
    }

    Br31St32(p, BR_GAMEOBJ_OFF_FLAGS, uFlags);      /* original: returns 1 */
}

/* 0x100474B0 */
/* WHAT IT DOES: the once-a-frame step for a menu row -- it just runs the
 * highlight pulse and reports success. */
/* @implements 0x100474B0 d3d BrPhaseTick_100474B0 */
int BrPhaseTick_100474B0(BrGameObj *pObj)
{
    BrSub10047360(pObj);
    return 1;
}

/* 0x100475F0 */
/* WHAT IT DOES: the once-a-frame step for a row the player can type into: it
 * first takes any waiting keystroke into the typed-text buffer, then runs the
 * same highlight pulse as its plainer neighbour above. */
/* @implements 0x100475F0 d3d BrPhaseTick_100475F0 */
int BrPhaseTick_100475F0(BrGameObj *pObj)
{
    BrPhaseKeyPush_10047610();
    BrSub10047360(pObj);
    return 1;
}

/* 0x10047610 */
/* WHAT IT DOES: takes the key the player last pressed and adds it to the
 * running buffer of typed characters -- lower-casing capital letters on the
 * way, so typed names come out in lower case. The buffer holds thirty-two
 * characters and wraps round rather than stopping. */
/* @implements 0x10047610 d3d BrPhaseKeyPush_10047610 */
void BrPhaseKeyPush_10047610(void)
{
    unsigned char bKey;
    signed char   cKey;
    int32_t       i;

    if (g_pExt->nAA33E4 == 0)
        return;

    bKey = (unsigned char)(uint32_t)g_pExt->nAA33E4;

    /* SIGNED byte comparisons: 0x80..0xFF are negative and fall through
     * untouched, so only 'A'..'Z' are lowercased. */
    cKey = (signed char)bKey;
    if (cKey >= 0x41 && cKey <= 0x5A)
        cKey = (signed char)(cKey + 0x20);

    i = g_pExt->nAA2A48;
    g_pExt->aA9E150[i] = (int32_t)cKey;     /* movsx -- sign-extended */

    i++;
    g_pExt->nAA2A48 = i;
    if (i >= 32)                            /* wraps AFTER the store */
        g_pExt->nAA2A48 = 0;

    BrExt_10047660();
    g_pExt->nAA33E4 = 0;
}

/* --- the six mode callbacks ----------------------------------------------- */

/* WHAT IT DOES: one of six near-identical routines behind menu rows: it raises
 * one particular setting's flag and plays the menu's confirmation sound. Which
 * setting each of the six turns on was not established here. */
/* @implements 0x100474D0 d3d BrPhaseMode_100474D0 */
void BrPhaseMode_100474D0(void)
{
    g_pExt->nAA28F0 = 1;
    BrExt_10072AF0(2, 0x00200020u);
    g_pExt->nAA2854 = 2;
}

/* WHAT IT DOES: the same as the routine above for a different setting -- raise
 * its flag, play the confirmation sound. */
/* @implements 0x10047500 d3d BrPhaseMode_10047500 */
void BrPhaseMode_10047500(void)
{
    g_pExt->nAA28F8 = 1;
    BrExt_10072AF0(2, 0x00200020u);
    g_pExt->nAA2854 = 2;
}

/* WHAT IT DOES: the third of the six -- a third setting's flag, the same
 * confirmation sound. */
/* @implements 0x10047530 d3d BrPhaseMode_10047530 */
void BrPhaseMode_10047530(void)
{
    g_pExt->nAA28FC = 1;
    BrExt_10072AF0(2, 0x00200020u);
    g_pExt->nAA2854 = 2;
}

/* WHAT IT DOES: the odd one of the six: instead of raising a flag it sets a
 * value to its maximum, and it plays a different sound from its five
 * neighbours. */
/* @implements 0x10047560 d3d BrPhaseMode_10047560 */
void BrPhaseMode_10047560(void)
{
    g_pExt->n0AC6A4 = 0x7FFF;
    BrExt_10072AF0(3, 0x00200020u);
    g_pExt->nAA2854 = 3;
}

/* WHAT IT DOES: the fifth of the six -- another setting's flag and the same
 * confirmation sound. */
/* @implements 0x10047590 d3d BrPhaseMode_10047590 */
void BrPhaseMode_10047590(void)
{
    g_pExt->nAA2A40 = 1;
    BrExt_10072AF0(2, 0x00200020u);
    g_pExt->nAA2854 = 2;
}

/* WHAT IT DOES: the last of the six -- one more setting's flag and the same
 * confirmation sound. */
/* @implements 0x100475C0 d3d BrPhaseMode_100475C0 */
void BrPhaseMode_100475C0(void)
{
    g_pExt->nAA28F4 = 1;
    BrExt_10072AF0(2, 0x00200020u);
    g_pExt->nAA2854 = 2;
}
