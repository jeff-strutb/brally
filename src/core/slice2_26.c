/* slice2_26.c -- BRD3D.dll 0x100447D0-0x100456B0, a later pass.
 *
 * The phase (screen/mode) switcher. See slice2_26.h for the mechanism, the
 * calling-convention deviations, and the per-function notes.
 *
 * Structural notes that apply throughout:
 *
 *  - Every global in this range is read through pCtx on every use, never
 *    cached in a local, because the original reloads them across calls and
 *    several of those reloads are observable (a hook that re-points the
 *    current phase, or 0x1003C020 changing nAA287C). Where the original DOES
 *    cache a value across a call, a local is used and the fact is commented.
 *
 *  - The SEH frames the original pushes (push -1 / push handler / fs:[0]) are
 *    pure MSVC exception bookkeeping around the `new` expression. There is no
 *    throw and no catch here, so nothing is emitted for them.
 *
 *  - The `push ecx` / `sub esp,8` prologues only reserve the stack slot the
 *    unwinder needs for the object under construction; they carry no data
 *    beyond the object pointer, which is a local here.
 */

#include <stddef.h>
#include "br_phase.h"   /* BR_PHASE_ALLOC_SIZE */

#ifdef BR_MATCHING_BUILD
/* Header prototype threads pCtx; the original is one-arg cdecl. */
#define BrPhaseHook_100450C0 BrPhaseHook_100450C0_port
#endif
#include "slice2_26.h"
#ifdef BR_MATCHING_BUILD
#undef BrPhaseHook_100450C0
extern int32_t  g_br0AA010;   /* 0x100AA010 */
extern BrPhase *g_brAA29B0;   /* 0x10AA29B0 */
#define BR26_AA29B0  g_brAA29B0
#define BR26_0AA010  g_br0AA010
#else
#define BR26_AA29B0  (pCtx->pAA29B0)
#define BR26_0AA010  (pCtx->n0AA010)
#endif
#include "br_phaseact.h"   /* the one activate body -- see br_phaseact.h */

/* ==========================================================================
 * Byte-offset access into the 0x2B68 entity record (slice1_09.h precedent:
 * too few fields are touched to justify inventing a struct).
 * ========================================================================== */

static BrEntSub *BrEntitySub(void *pEntity)
{
    return *(BrEntSub **)(void *)((unsigned char *)pEntity + BR_ENTITY_OFF_SUB);
}

static void BrEntityClearFlag1C10(void *pEntity)
{
    uint32_t *pFlags =
        (uint32_t *)(void *)((unsigned char *)pEntity + BR_ENTITY_OFF_FLAGS);
    *pFlags &= ~BR_ENTITY_FLAG_1C_10;
}

static void BrEntityClearF2B64(void *pEntity)
{
    *((unsigned char *)pEntity + BR_ENTITY_OFF_F2B64) = 0;
}

/* ==========================================================================
 * The shared body of the thirteen activate routines
 * ========================================================================== */

/* THE SHARED ACTIVATE BODY has moved to br_phaseact.c, which is a leaf both
 * this file and slice3_31.c can link.  slice3_31.c had transcribed the same
 * inlined sequence a second time, and the two disagreed about whether the two
 * flag stores are guarded -- they are not.  See br_phaseact.h. */

/* The shared prologue of the eight leave routines: poke the entity's
 * sub-object, then notify a phase. ppNotify is a pointer to the global rather
 * than its value because the original loads that global AFTER the +0x1C call
 * returns. */
static void BrPhaseLeavePrologue(void *pEntity, BrPhase **ppNotify)
{
    BrEntSub *pSub = BrEntitySub(pEntity);
    BrPhase  *pNotify;

    pSub->pVtbl->f1C(pSub);

    pNotify = *ppNotify;
    if (pNotify != NULL)
        pNotify->pVtbl->f00(pNotify, 1);
}

/* The extra head that 0x10044970 and 0x10044A30 share. */
static void BrPhaseLeaveHead(BrPhaseCtx *pCtx, void *pEntity)
{
    if (pCtx->nA9D000 != 0) {
        BrEntSub *pSub = BrEntitySub(pEntity);
        pSub->pVtbl->f18(pSub, 0);
        BrExt_10038F30(0);
    }
}

/* The tail those two share up to the point where they diverge: returns the
 * mode value that the 2/3 test must use -- which is a RE-READ of nAA287C when
 * 0x1003C020 was called, not the value read before it. */
static int32_t BrPhaseLeaveMode(BrPhaseCtx *pCtx)
{
    int32_t mode = pCtx->nAA287C;

    if (mode == 0 || mode == 1) {
        if (pCtx->nA9D000 == 0) {
            BrExt_1003C020();
            mode = pCtx->nAA287C;
        }
    }
    return mode;
}

/* ==========================================================================
 * 0x100447D0
 * ========================================================================== */

/* WHAT IT DOES: brings the multiplayer game screen up. It clears the player
 * slot table, and if this machine is hosting it first republishes the session
 * so other machines can see it and clears the "closed" flag on the advert. It
 * then opens the options in either the hosting or the joining form, builds the
 * screen if it is not already there, and puts the menus into multiplayer mode.
 * If the screen cannot be built it gives up before any of that last part. */
/* @implements 0x100447D0 d3d BrPhaseActivate_100447D0 */
int BrPhaseActivate_100447D0(BrPhaseCtx *pCtx)
{
    pCtx->nA9CFFC = 1;
    BrSlotsReset(pCtx->pSlots);

    if (pCtx->nAA2884 != 0) {
        BrHostItem *pItem = NULL;

        if (pCtx->p277B40 != NULL)
            BrExt_1003D0B0(pCtx->p277B40, &pItem);

        if (pItem != NULL) {
            pItem->f04 &= ~BR_HOSTITEM_FLAG_20;
            /* the host global is re-read; the item and 0 follow it. */
            pCtx->p277B40->pVtbl->f7C(pCtx->p277B40, pItem, 0);
        }
    }

    BrExt_10043BF0(0);
    BrExt_10043CD0(0);
    BrExt_10043E70(0);

    if (pCtx->nAA2884 != 0) {
        BrExt_100440D0(0);
        BrExt_100443E0(0);
    } else {
        BrExt_10044280(0);
    }

    if (BrPhaseActivateSlot(pCtx, &pCtx->pAA2954, BrExt_10058750) ==
        BR_ACT_FAILED) {
        /* Everything below -- including n0AA010 -- is skipped. */
        return 0;
    }

    pCtx->n0AA010 = 6;

    if (pCtx->nAA2884 != 0) {
        if (pCtx->nAA2888 == 0) {
            BrExt_1003C150();
            pCtx->nAA2888 = 1;
        } else if (pCtx->nAA2884 != 0) {
            /* The original re-tests the value it has just branched on, so
             * this arm is unconditional in practice. Kept as written. */
            BrExt_1003CDA0();
        }
    }

    if (pCtx->pA9D008 != NULL) {
        void *p8 = pCtx->pA9D008->f08;
        if (p8 != NULL)
            BrExt_1003DB00(pCtx->pA9D008, p8);
    }

    return 1;
}

/* ==========================================================================
 * The leave routines
 * ========================================================================== */

/* WHAT IT DOES: leaves a screen and tears the race session down with it --
 * closes the session, clears the entity's "in play" flag, and hands control
 * back to a remembered screen. On the way out it re-checks what mode the game
 * is in, because closing the session can change it, and clears the flag a
 * second time for two of those modes. */
/* @implements 0x10044970 d3d BrPhaseLeave_10044970 */
int BrPhaseLeave_10044970(BrPhaseCtx *pCtx, void *pEntity)
{
    int32_t mode;

    BrPhaseLeaveHead(pCtx, pEntity);
    BrPhaseLeavePrologue(pEntity, &BR_PHASE_CUR);

    pCtx->nAA2950 = 0;
    BR_PHASE_CUR = pCtx->pAA2948;

    if (pCtx->pAA29D8 != NULL)
        BrEntityClearFlag1C10(pCtx->pAA29D8);

    BrExt_1003BF60();

    pCtx->nAA2898 = 1;
    mode = BrPhaseLeaveMode(pCtx);

    if (mode == 2 || mode == 3) {
        /* pAA29D8 is re-read here. */
        if (pCtx->pAA29D8 != NULL)
            BrEntityClearFlag1C10(pCtx->pAA29D8);
    }

    return 0;
}

int BrPhaseLeave_10044A30(BrPhaseCtx *pCtx, void *pEntity)
{
    int32_t mode;

    BrPhaseLeaveHead(pCtx, pEntity);
    BrPhaseLeavePrologue(pEntity, &BR_PHASE_CUR);

    pCtx->nAA2950 = 0;
    BR_PHASE_CUR = pCtx->pAA294C;

    BrExt_1003BF60();

    mode = BrPhaseLeaveMode(pCtx);

    if (mode == 2 || mode == 3) {
        if (pCtx->pAA29D8 != NULL) {
            BrEntityClearFlag1C10(pCtx->pAA29D8);
            /* pAA29D8 is re-read before the byte store. */
            BrEntityClearF2B64(pCtx->pAA29D8);
        }
    }

    return 0;
}

/* WHAT IT DOES: leaves a screen and forgets a good deal more than its
 * neighbours do -- five separate remembered screens, entities and counters are
 * cleared -- then closes the session and returns to a remembered screen. This
 * is the leave that unwinds the most state. */
/* @implements 0x10044AE0 d3d BrPhaseLeave_10044AE0 */
int BrPhaseLeave_10044AE0(BrPhaseCtx *pCtx, void *pEntity)
{
    BrPhaseLeavePrologue(pEntity, &BR_PHASE_CUR);

    pCtx->pAA2948 = NULL;
    pCtx->nAA29B8 = 0;
    pCtx->pAA29D8 = NULL;
    pCtx->nAA29D4 = 0;
    pCtx->nAA2880 = 0;
    BR_PHASE_CUR = pCtx->pAA2940;

    BrExt_1003BF60();
    return 0;
}

/* WHAT IT DOES: leaves a screen, forgetting the two controls it had published
 * for other code to reach, and returns to a remembered screen. Unlike its
 * neighbours it does not close the session. */
/* @implements 0x10044B40 d3d BrPhaseLeave_10044B40 */
int BrPhaseLeave_10044B40(BrPhaseCtx *pCtx, void *pEntity)
{
    BrPhaseLeavePrologue(pEntity, &BR_PHASE_CUR);

    pCtx->nAA298C = 0;
    pCtx->nAA29E8 = 0;
    BR_PHASE_CUR = pCtx->pAA2940;
    return 0;
}

/* WHAT IT DOES: leaves a screen and returns the player all the way to the root
 * menu rather than to whatever opened it, forgetting one remembered screen on
 * the way. */
/* @implements 0x10044C70 d3d BrPhaseLeave_10044C70 */
int BrPhaseLeave_10044C70(BrPhaseCtx *pCtx, void *pEntity)
{
    BrPhaseLeavePrologue(pEntity, &BR_PHASE_CUR);

    pCtx->pAA295C = NULL;
    BR_PHASE_CUR = pCtx->pAA2908;
    return 0;
}

int BrPhaseLeave_10044CB0(BrPhaseCtx *pCtx, void *pEntity)
{
    BrPhaseLeavePrologue(pEntity, &BR_PHASE_CUR);

    pCtx->pAA290C = NULL;
    pCtx->nAA29AC = 0;
    BR_PHASE_CUR = pCtx->pAA295C;
    return 0;
}

/* WHAT IT DOES: leaves a screen, forgets it, and returns to a remembered
 * parent screen. The plainest member of the leave family. */
/* @implements 0x10044DE0 d3d BrPhaseLeave_10044DE0 */
int BrPhaseLeave_10044DE0(BrPhaseCtx *pCtx, void *pEntity)
{
    BrPhaseLeavePrologue(pEntity, &BR_PHASE_CUR);

    pCtx->pAA2964 = NULL;
    BR_PHASE_CUR = pCtx->pAA295C;
    return 0;
}

/* WHAT IT DOES: leaves a screen -- and it is the one member of the family that
 * notifies the screen being closed rather than the one currently showing, which
 * matters when the two differ. It then forgets that screen, returns to a
 * remembered parent, and puts the menus into a particular mode. */
/* @implements 0x10044F00 d3d BrPhaseLeave_10044F00 */
int BrPhaseLeave_10044F00(BrPhaseCtx *pCtx, void *pEntity)
{
    /* Notifies pAA2968 -- the phase being dropped -- not pAA2904. */
    BrPhaseLeavePrologue(pEntity, &pCtx->pAA2968);

    pCtx->pAA2968 = NULL;
    BR_PHASE_CUR = pCtx->pAA295C;
    pCtx->n0AA010 = 2;
    return 0;
}

/* ==========================================================================
 * The remaining activate routines
 * ========================================================================== */

int BrPhaseActivate_10044B90(BrPhaseCtx *pCtx)
{
    BrExt_100419D0(pCtx->p0AD300);

    return (BrPhaseActivateSlot(pCtx, &pCtx->pAA295C, BrExt_10059760) !=
            BR_ACT_FAILED) ? 1 : 0;
}

/* WHAT IT DOES: brings one particular menu screen up, clearing two counters
 * first so it starts from a known state, and builds the screen if it does not
 * already exist. Which screen it is was not established here. */
/* @implements 0x10044D00 d3d BrPhaseActivate_10044D00 */
int BrPhaseActivate_10044D00(BrPhaseCtx *pCtx)
{
    pCtx->nAA28C8 = 0;
    pCtx->nAA28CC = 0;

    return (BrPhaseActivateSlot(pCtx, &pCtx->pAA2964, BrExt_10059BB0) !=
            BR_ACT_FAILED) ? 1 : 0;
}

int BrPhaseActivate_10044E20(BrPhaseCtx *pCtx)
{
    /* GOTCHA: the sources cross over -- 0x10ACEE8C goes to 0x10AA28CC and
     * 0x10ACEE94 goes to 0x10AA28C8. */
    pCtx->nAA28CC = pCtx->nACEE8C;
    pCtx->nAA28C8 = pCtx->nACEE94;

    return (BrPhaseActivateSlot(pCtx, &pCtx->pAA2968, BrExt_1005A6E0) !=
            BR_ACT_FAILED) ? 1 : 0;
}

/* WHAT IT DOES: brings up one particular menu screen, and does the most work
 * of the activate family around it: it resets a shared text buffer, switches
 * the menus into a mode of their own, and runs a preparation step. Three
 * further set-up calls happen only when the screen has to be built -- coming
 * back to an already-built screen skips them. Which screen it is was not
 * established here. */
/* @implements 0x10044F50 d3d BrPhaseActivate_10044F50 */
int BrPhaseActivate_10044F50(BrPhaseCtx *pCtx)
{
    BrActResult r;

    BrExt_100419D0(pCtx->p0AD300);
    pCtx->n0AA010 = 1;
    BrExt_1003E680();

    r = BrPhaseActivateSlot(pCtx, &pCtx->pAA290C, BrPhaseEnterPlaceholder_1004B430);
    if (r == BR_ACT_FAILED)
        return 0;
    if (r == BR_ACT_EXISTING)
        return 1;   /* the three calls below run only on the built path */

    BrExt_10008B80();   /* a bare `ret` in this build */
    BrExt_1003DFC0();
    BrExt_1003E510();
    return 1;
}

/* WHAT IT DOES: brings one particular menu screen up, building it if it does
 * not already exist. Which screen it is was not established here; the screen
 * builder behind it is not transcribed in this tree. */
/* @implements 0x10045110 d3d BrPhaseActivate_10045110 */
int BrPhaseActivate_10045110(BrPhaseCtx *pCtx)
{
    return (BrPhaseActivateSlot(pCtx, &pCtx->pAA2914, BrPhaseEnterPlaceholder_1004A580) !=
            BR_ACT_FAILED) ? 1 : 0;
}

/* WHAT IT DOES: brings up the options menu -- this is what the Options button
 * on the season-progress screen reaches -- after clearing the shared text
 * buffer, and builds it if it is not already there. */
/* @implements 0x100451E0 d3d BrPhaseActivate_100451E0 */
int BrPhaseActivate_100451E0(BrPhaseCtx *pCtx)
{
    BrExt_100419D0(pCtx->p0AD300);

    return (BrPhaseActivateSlot(pCtx, &pCtx->pAA2918, BrPhaseEnterPlaceholder_1004BDC0) !=
            BR_ACT_FAILED) ? 1 : 0;
}

/* WHAT IT DOES: brings one particular menu screen up, building it if needed.
 * Which screen it is was not established here; its builder is not transcribed
 * in this tree. */
/* @implements 0x100452C0 d3d BrPhaseActivate_100452C0 */
int BrPhaseActivate_100452C0(BrPhaseCtx *pCtx)
{
    return (BrPhaseActivateSlot(pCtx, &pCtx->pAA297C, BrPhaseEnterPlaceholder_1004C4A0) !=
            BR_ACT_FAILED) ? 1 : 0;
}

/* WHAT IT DOES: brings one particular menu screen up, building it if needed.
 * Which screen it is was not established here -- the builder behind it is one
 * of the ones still untranscribed. */
/* @implements 0x10045390 d3d BrPhaseActivate_10045390 */
int BrPhaseActivate_10045390(BrPhaseCtx *pCtx)
{
    return (BrPhaseActivateSlot(pCtx, &pCtx->pAA2980, BrExt_1004D1F0) !=
            BR_ACT_FAILED) ? 1 : 0;
}

/* WHAT IT DOES: brings one particular menu screen up and then runs an extra
 * step -- on both the freshly-built and the already-built path, unlike most of
 * this family, which is the only thing distinguishing it. Which screen it is
 * was not established here. */
/* @implements 0x10045460 d3d BrPhaseActivate_10045460 */
int BrPhaseActivate_10045460(BrPhaseCtx *pCtx)
{
    if (BrPhaseActivateSlot(pCtx, &pCtx->pAA2990, BrExt_1004D640) ==
        BR_ACT_FAILED) {
        return 0;
    }

    BrExt_1007AC00();   /* both the built and already-built paths reach this */
    return 1;
}

/* WHAT IT DOES: the twin of the routine above -- a different screen, but the
 * same extra step run whether the screen was just built or was already there.
 * Which screen it is was not established here. */
/* @d3donly 0x10045520 BrPhaseActivate_10045520 -- glide twin 0x1003E9B0 COMDAT-folded onto BrPhaseActivate_10045460 */
int BrPhaseActivate_10045520(BrPhaseCtx *pCtx)
{
    if (BrPhaseActivateSlot(pCtx, &pCtx->pAA2994, BrExt_1004DB00) ==
        BR_ACT_FAILED) {
        return 0;
    }

    BrExt_1007AC00();
    return 1;
}

/* WHAT IT DOES: brings up the car-selection screen, building it if it is not
 * already there. */
/* @implements 0x100455E0 d3d BrPhaseActivate_100455E0 */
int BrPhaseActivate_100455E0(BrPhaseCtx *pCtx)
{
    return (BrPhaseActivateSlot(pCtx, &pCtx->pAA2984, BrExt_1004DFC0) !=
            BR_ACT_FAILED) ? 1 : 0;
}

/* WHAT IT DOES: brings up the game-options screen -- force feedback, skid
 * marks, specular lighting and car shadow -- building it if it is not already
 * there. */
/* @implements 0x100456B0 d3d BrPhaseActivate_100456B0 */
int BrPhaseActivate_100456B0(BrPhaseCtx *pCtx)
{
    return (BrPhaseActivateSlot(pCtx, &pCtx->pAA2988, BrExt_1004E830) !=
            BR_ACT_FAILED) ? 1 : 0;
}

/* ==========================================================================
 * Hook installers and the dispatcher
 * ========================================================================== */

/* WHAT IT DOES: opens a screen and then wires up its Back row so the player
 * can get out again. It suppresses one flag across the opening and restores it
 * afterwards, and it deliberately reads the row to wire AFTER the screen is
 * built, so it catches the row the new screen just published rather than a
 * stale one. */
/* @implements 0x10045050 d3d BrPhaseHook_10045050 */
int BrPhaseHook_10045050(BrPhaseCtx *pCtx, void *pArg)
{
    /* The original pushes pArg at 0x10045110, which ignores it (both are
     * __cdecl, so the extra argument is harmless). Nothing else uses pArg. */
    (void)pArg;

    pCtx->n0AC304 = 0;
    (void)BrPhaseActivate_10045110(pCtx);   /* result discarded */
    pCtx->n0AC304 = 1;

    /* pAA29B4 is read after the activation, so an activation that changes it
     * is what gets hooked. */
    pCtx->pAA29B4->pfnHook = BrExt_10046CD0;
    pCtx->n0AA010 = 0;
    return 1;
}

/* WHAT IT DOES: opens a screen and then wires its Back row to the routine that
 * returns the player to the previous screen. Same pattern as its neighbour
 * above, without the flag juggling. */
/* @implements 0x10045090 d3d BrPhaseHook_10045090 */
int BrPhaseHook_10045090(BrPhaseCtx *pCtx, void *pArg)
{
    BrExt_10045C90(pArg);

    pCtx->pAA29B0->pfnHook = BrExt_10046DC0;
    pCtx->n0AA010 = 0;
    return 1;
}

/* WHAT IT DOES: the same as the routine above -- open a screen, wire its Back
 * row -- with one extra preparation call in front of it. That call is a
 * do-nothing stub in this build, so the two behave identically here; the
 * difference is preserved because the order is what the original recorded. */
/* @implements 0x100450C0 d3d BrPhaseHook_100450C0 */
#ifdef BR_MATCHING_BUILD
int BrPhaseHook_100450C0(void *pArg)
#else
int BrPhaseHook_100450C0(BrPhaseCtx *pCtx, void *pArg)
#endif
{
    BrExt_10041BD0();
    BrExt_10045C90(pArg);

    BR26_AA29B0->pfnHook = BrExt_10046DC0;
    BR26_0AA010 = 0;
    return 1;
}

/* WHAT IT DOES: passes a click straight on to whatever routine a particular
 * remembered control has had plugged into it, then puts the menus back into
 * their default mode. Unlike the three routines above it reports failure rather
 * than success, on every path. */
/* @implements 0x100450F0 d3d BrPhaseDispatch_100450F0 */
int BrPhaseDispatch_100450F0(BrPhaseCtx *pCtx, void *pArg)
{
    pCtx->pAA29F4->pfnHook(pArg);
    pCtx->n0AA010 = 0;
    return 0;   /* GOTCHA: 0, unlike its three neighbours */
}
