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

#include "slice2_26.h"

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

typedef enum BrActResult {
    BR_ACT_FAILED   = 0,   /* operator new returned NULL; caller returns 0   */
    BR_ACT_EXISTING = 1,   /* the singleton was already built                */
    BR_ACT_CREATED  = 2    /* built here; the enter hook has run             */
} BrActResult;

static BrActResult BrPhaseActivateSlot(BrPhaseCtx     *pCtx,
                                       BrPhase       **ppSlot,
                                       BrPhaseEnterFn  pfnEnter)
{
    BrPhase *p;

    if (*ppSlot != NULL) {
        pCtx->pAA2904 = *ppSlot;
        return BR_ACT_EXISTING;
    }

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
/* operator new does not zero the block; whatever the constructor at
     * 0x10048710 leaves untouched stays garbage. */
    p = (BrPhase *)BrOperatorNew(BR_PHASE_ALLOC_SIZE);
    p = (p != NULL) ? BrOptObjCtor(p) : NULL;

    /* Both globals are written even when the allocation failed, so a failed
     * activation leaves the current phase NULL. */
    *ppSlot       = p;
    pCtx->pAA2904 = p;

    if (p == NULL)
        return BR_ACT_FAILED;

    p->pfnEnter = pfnEnter;

    /* The original re-reads the slot global here and calls through that,
     * rather than through the register holding the new object. */
    p = *ppSlot;
    p->pfnEnter(p);

    /* ...and re-reads the current-phase global once per store. If the enter
     * hook re-pointed it, these two flags land on the hook's phase. */
    /* DEVIATION: the original dereferences unconditionally and would fault if
     * the hook cleared the current phase. Guarded here for memory safety;
     * behaviour is unchanged wherever the original does not fault. */
    if (pCtx->pAA2904 != NULL)
        pCtx->pAA2904->f0C = 1;
    if (pCtx->pAA2904 != NULL)
        pCtx->pAA2904->f68 = 1;

    return BR_ACT_CREATED;
}

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

int BrPhaseLeave_10044970(BrPhaseCtx *pCtx, void *pEntity)
{
    int32_t mode;

    BrPhaseLeaveHead(pCtx, pEntity);
    BrPhaseLeavePrologue(pEntity, &pCtx->pAA2904);

    pCtx->nAA2950 = 0;
    pCtx->pAA2904 = pCtx->pAA2948;

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
    BrPhaseLeavePrologue(pEntity, &pCtx->pAA2904);

    pCtx->nAA2950 = 0;
    pCtx->pAA2904 = pCtx->pAA294C;

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

int BrPhaseLeave_10044AE0(BrPhaseCtx *pCtx, void *pEntity)
{
    BrPhaseLeavePrologue(pEntity, &pCtx->pAA2904);

    pCtx->pAA2948 = NULL;
    pCtx->nAA29B8 = 0;
    pCtx->pAA29D8 = NULL;
    pCtx->nAA29D4 = 0;
    pCtx->nAA2880 = 0;
    pCtx->pAA2904 = pCtx->pAA2940;

    BrExt_1003BF60();
    return 0;
}

int BrPhaseLeave_10044B40(BrPhaseCtx *pCtx, void *pEntity)
{
    BrPhaseLeavePrologue(pEntity, &pCtx->pAA2904);

    pCtx->nAA298C = 0;
    pCtx->nAA29E8 = 0;
    pCtx->pAA2904 = pCtx->pAA2940;
    return 0;
}

int BrPhaseLeave_10044C70(BrPhaseCtx *pCtx, void *pEntity)
{
    BrPhaseLeavePrologue(pEntity, &pCtx->pAA2904);

    pCtx->pAA295C = NULL;
    pCtx->pAA2904 = pCtx->pAA2908;
    return 0;
}

int BrPhaseLeave_10044CB0(BrPhaseCtx *pCtx, void *pEntity)
{
    BrPhaseLeavePrologue(pEntity, &pCtx->pAA2904);

    pCtx->pAA290C = NULL;
    pCtx->nAA29AC = 0;
    pCtx->pAA2904 = pCtx->pAA295C;
    return 0;
}

int BrPhaseLeave_10044DE0(BrPhaseCtx *pCtx, void *pEntity)
{
    BrPhaseLeavePrologue(pEntity, &pCtx->pAA2904);

    pCtx->pAA2964 = NULL;
    pCtx->pAA2904 = pCtx->pAA295C;
    return 0;
}

int BrPhaseLeave_10044F00(BrPhaseCtx *pCtx, void *pEntity)
{
    /* Notifies pAA2968 -- the phase being dropped -- not pAA2904. */
    BrPhaseLeavePrologue(pEntity, &pCtx->pAA2968);

    pCtx->pAA2968 = NULL;
    pCtx->pAA2904 = pCtx->pAA295C;
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

int BrPhaseActivate_10045110(BrPhaseCtx *pCtx)
{
    return (BrPhaseActivateSlot(pCtx, &pCtx->pAA2914, BrPhaseEnterPlaceholder_1004A580) !=
            BR_ACT_FAILED) ? 1 : 0;
}

int BrPhaseActivate_100451E0(BrPhaseCtx *pCtx)
{
    BrExt_100419D0(pCtx->p0AD300);

    return (BrPhaseActivateSlot(pCtx, &pCtx->pAA2918, BrPhaseEnterPlaceholder_1004BDC0) !=
            BR_ACT_FAILED) ? 1 : 0;
}

int BrPhaseActivate_100452C0(BrPhaseCtx *pCtx)
{
    return (BrPhaseActivateSlot(pCtx, &pCtx->pAA297C, BrPhaseEnterPlaceholder_1004C4A0) !=
            BR_ACT_FAILED) ? 1 : 0;
}

int BrPhaseActivate_10045390(BrPhaseCtx *pCtx)
{
    return (BrPhaseActivateSlot(pCtx, &pCtx->pAA2980, BrExt_1004D1F0) !=
            BR_ACT_FAILED) ? 1 : 0;
}

int BrPhaseActivate_10045460(BrPhaseCtx *pCtx)
{
    if (BrPhaseActivateSlot(pCtx, &pCtx->pAA2990, BrExt_1004D640) ==
        BR_ACT_FAILED) {
        return 0;
    }

    BrExt_1007AC00();   /* both the built and already-built paths reach this */
    return 1;
}

int BrPhaseActivate_10045520(BrPhaseCtx *pCtx)
{
    if (BrPhaseActivateSlot(pCtx, &pCtx->pAA2994, BrExt_1004DB00) ==
        BR_ACT_FAILED) {
        return 0;
    }

    BrExt_1007AC00();
    return 1;
}

int BrPhaseActivate_100455E0(BrPhaseCtx *pCtx)
{
    return (BrPhaseActivateSlot(pCtx, &pCtx->pAA2984, BrExt_1004DFC0) !=
            BR_ACT_FAILED) ? 1 : 0;
}

int BrPhaseActivate_100456B0(BrPhaseCtx *pCtx)
{
    return (BrPhaseActivateSlot(pCtx, &pCtx->pAA2988, BrExt_1004E830) !=
            BR_ACT_FAILED) ? 1 : 0;
}

/* ==========================================================================
 * Hook installers and the dispatcher
 * ========================================================================== */

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

int BrPhaseHook_10045090(BrPhaseCtx *pCtx, void *pArg)
{
    BrExt_10045C90(pArg);

    pCtx->pAA29B0->pfnHook = BrExt_10046DC0;
    pCtx->n0AA010 = 0;
    return 1;
}

int BrPhaseHook_100450C0(BrPhaseCtx *pCtx, void *pArg)
{
    BrExt_10041BD0();
    BrExt_10045C90(pArg);

    pCtx->pAA29B0->pfnHook = BrExt_10046DC0;
    pCtx->n0AA010 = 0;
    return 1;
}

int BrPhaseDispatch_100450F0(BrPhaseCtx *pCtx, void *pArg)
{
    pCtx->pAA29F4->pfnHook(pArg);
    pCtx->n0AA010 = 0;
    return 0;   /* GOTCHA: 0, unlike its three neighbours */
}
