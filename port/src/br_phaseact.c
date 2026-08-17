/* br_phaseact.c -- one body for the activate sequence twenty-one phase
 * routines inline.  See br_phaseact.h for why it is here and for the
 * instruction-by-instruction evidence.
 */
#include "br_phaseact.h"
#include "br_phase.h"    /* BR_PHASE_ALLOC_SIZE */

/* WHAT IT DOES: brings one of the game's screens into being and makes it the
 * screen the game is on. Each screen is built once and kept, so a second
 * request just switches to the one already there. Building it runs the
 * screen's own set-up routine and then marks it live -- and it marks whatever
 * screen is current when that set-up finishes, which is not necessarily the
 * one just built. */
BrActResult BrPhaseActivateSlot(BrPhaseCtx     *pCtx,
                                BrPhase       **ppSlot,
                                BrPhaseEnterFn  pfnEnter)
{
    BrPhase *p;

    if (*ppSlot != NULL) {
        BR_PHASE_CUR = *ppSlot;
        return BR_ACT_EXISTING;
    }

    /* HARDENING (port): allocate what THIS build's object needs.
     *
     * The original allocates 0xC8 and that is right for a 32-bit build. It is
     * not right here: br_phase.h's BrPhase_ is ~300 bytes on LP64 because
     * every pointer widened, and 0x10048710 writes the whole object. Today
     * these modules use the 5-field partial view and the real constructor is
     * NOT wired to these sites, so nothing overflows -- but the moment it is
     * wired a 0xC8 block becomes a ~100-byte heap overflow per activation.
     * BR_PHASE_ALLOC_SIZE is max(sizeof, 0xC8), so this is never smaller than
     * the original either.
     *
     * operator new does not zero the block; whatever the constructor leaves
     * untouched stays garbage. */
    p = (BrPhase *)BrOperatorNew(BR_PHASE_ALLOC_SIZE);
    p = (p != NULL) ? BrOptObjCtor(p) : NULL;

    /* Both globals are written even when the allocation failed, so a failed
     * activation leaves the current phase NULL. */
    *ppSlot       = p;
    BR_PHASE_CUR = p;

    if (p == NULL)
        return BR_ACT_FAILED;

    p->pfnEnter = pfnEnter;

    /* The original re-reads the slot global here and calls through that,
     * rather than through the register holding the new object. */
    p = *ppSlot;
    p->pfnEnter(p);

    /* ...and re-reads the current-phase global once per store. If the enter
     * hook re-pointed it, these two flags land on the hook's phase.
     *
     * NO GUARD, DELIBERATELY. slice2_26.c's copy of this sequence used to
     * wrap both stores in `if (BR_PHASE_CUR != NULL)` and call it a
     * DEVIATION for memory safety; slice3_31.c's copy of the same bytes did
     * not. The original does not: 0x1004559C loads the global and stores to
     * +0x0C, 0x100455AB loads it again and stores to +0x68, with no test
     * anywhere between. The guarded copy turned "an enter hook cleared the
     * current phase" from a fault into a silent no-op -- inventing a
     * behaviour the game never had, in the copy that looked safer. */
    BR_PHASE_CUR->f0C = 1;
    BR_PHASE_CUR->f68 = 1;

    return BR_ACT_CREATED;
}
