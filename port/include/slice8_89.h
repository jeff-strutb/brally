/* slice8_89.h -- the two control hooks slice6_72.c installs and nothing else
 * fills: 0x10044D00 and 0x10045050, plus the two callees they reach.
 *
 * ---------------------------------------------------------------------------
 * WHY THIS MODULE EXISTS -- AND WHAT THE BRIEF THAT ASKED FOR IT GOT WRONG
 *
 * This file was commissioned as "ten unported hook functions":
 *
 *     0x10044D00 0x10045050 0x100450F0 0x10045880 0x100458A0 0x10045AA0
 *     0x100463C0 0x10046620 0x10046EB0 0x100470E0
 *
 * EIGHT OF THE TEN WERE ALREADY PORTED, over the same br_ui.h control model a
 * new module would have used, and are already stored into their BrUi73Hooks
 * slots by port/src/slice7_81.c's BrUiHook81Install:
 *
 *     0x100450F0  0x10045880  0x100458A0  0x10045AA0
 *     0x100463C0  0x10046620  0x10046EB0  0x100470E0
 *
 * They are covered by port/tests/test_slice7_81.c and are NOT duplicated here.
 * Re-porting them would have created exactly the aliased-storage defect
 * CONVENTIONS.md forbids, because slice7_81.h owns 0x10AA2918 / 0x10AA291C /
 * 0x10AA2928 / 0x10AA292C / 0x10AA2934 / 0x10AA2938 / 0x10AA293C / 0x10AA2940
 * / 0x10AA2958 / 0x10AA2974 / 0x10AA2980 / 0x10AA2990 / 0x10AA28E4 and a
 * second copy of any leave routine would have cleared the WRONG WORD.
 *
 * The genuine gap was the other two, and it is not in the BrUi73Hooks table at
 * all -- it is in BrUi72Hooks (port/include/slice6_72.h), which slice6_72.c's
 * builders read:
 *
 *     slice6_72.c:1213   pCtl->pfn08 = pH->p10044D00;   (builder 0x10059760)
 *     slice6_72.c: 979   pCtl->pfn08 = pH->p10045050;   (builder 0x10052030)
 *
 * port/src/slice8_84.c's BrUiHook84Install72 names both as deliberate holes
 * ("0x10045050 (:979) is NOT installed", "0x10044D00 (:1213) [is] NOT
 * installed"; see NOT DONE (D) in slice8_84.h). This module closes them, and
 * BrUiHook89Install72 touches ONLY those two slots so it composes with
 * BrUiHook84Install72 and BrUiOptInstall72 in any order.
 *
 * ---------------------------------------------------------------------------
 * ARE THESE SCREEN TRANSITIONS?
 *
 * Yes -- both of them, and the two callees are the matching teardown.
 *
 *   0x10044D00  ACTIVATE. Builds (or republishes) the 0x10AA2964 phase with
 *               enter hook 0x10059BB0 -- a C++ UI screen builder -- and makes
 *               it CURRENT (0x10AA2904). This is a forward transition: it is
 *               the row captioned string 0x6C on the 0x10059760 screen.
 *   0x10045050  INSTALLER. Runs the 0x10045110 activate (slot 0x10AA2914,
 *               enter hook 0x1004A580), then pokes the LEAVE routine
 *               0x10046CD0 into 0x10AA29B4's +0x08 -- i.e. it opens a screen
 *               AND wires that screen's way back out. Forward transition plus
 *               its return path. It is the row captioned string 0x1E on the
 *               0x10052030 screen.
 *   0x10046CD0  LEAVE, reached only through the poke above. Releases the
 *               owning phase's pages, notifies the current phase, drops
 *               0x10AA2914 and 0x10AA29B4, and repoints 0x10AA2904 at
 *               0x10AA2930. This is the "Back" of the pair, the same shape as
 *               slice8_85.c's BrUiHook85_100466C0.
 *
 * The 0x10046xxx addresses in the brief's list, said to be the transition
 * group next to the phase pop, are the ones that were already done. The two
 * that were actually missing are the 0x10044xxx / 0x10045xxx pair above.
 *
 * ---------------------------------------------------------------------------
 * STORAGE OWNERSHIP, stated address by address
 *
 * REACHED, never duplicated:
 *   0x10AA2904  current phase   br_uinav.h   BrUiNav::pAA2904
 *   0x100AA010                  br_uinav.h   BrUiNav::n0AA010
 *   0x10AA28C8                  slice4_50.c  g_brAA28C8
 *   0x10AA28CC                  slice4_50.c  g_brAA28CC
 *
 * OWNED HERE (BrUiHook89Ctx below). Each is ALSO declared, but never defined,
 * by slice2_26.h's BrPhaseCtx and/or slice3_33.h's BrUiBuildCtx; neither of
 * those structs is instantiated anywhere in the tree today (checked by
 * grepping port/src and port/host for the addresses -- no definition exists),
 * so this module is the first and only storage. If either is ever
 * instantiated, exactly one of the two must keep the storage and the other
 * must alias into it:
 *   0x10AA2914  0x10AA2930  0x10AA2964  0x10AA29B4  0x100AC304
 *
 * ---------------------------------------------------------------------------
 * CONFLICTS FOUND, reported rather than silently "resolved"
 *
 * 1. 0x10AA29B4 is a CONTROL, not a phase. slice2_26.h types it `BrPhase *`
 *    and comments "receives the +0x08 hook"; slice3_33.h types the same
 *    address `BrUiCtl *` and lists it among the words its five screen
 *    BUILDERS write. The builders are the writers, so they settle it, and
 *    this is the identical conflict slice7_81.h reports as its CONFLICT 1 for
 *    0x10AA29B0 / 0x10AA29C8 / 0x10AA29F0 / 0x10AA29F4 -- 0x10AA29B4 is
 *    literally the next dword after 0x10AA29B0. What does not survive the
 *    phase reading is the TYPE: the slot is invoked by 0x10048180 as
 *    `int32_t (*)(BrUiCtl_ *)`, not as `void (*)(void *)`.
 *
 * 2. RETURN TYPES, the same point slice7_81.h makes as its CONFLICT 4.
 *    slice2_26.h declares 0x10045050 `int BrPhaseHook_10045050(BrPhaseCtx *,
 *    void *)` and slice3_31.h declares the leave family `void`. Read at the
 *    machine level: 0x10045050 ends `mov eax,1 / ... / ret` and 0x10046CD0
 *    ends `xor eax,eax / ret`, and 0x10048180 propagates that value -- 0
 *    stops the page walk, which is how the frame that tore its own screen
 *    down stops walking it. Both constants are preserved on every path.
 *
 * 3. 0x10AA2930 -- 0x10046CD0's DESTINATION -- has no writer in this tree.
 *    slice3_31.h records 0x10047060 as another routine that clears it and
 *    0x10046830 as one that reads it, and neither is ported. Until something
 *    builds it, 0x10046CD0 correctly repoints 0x10AA2904 at NULL. That is the
 *    ORIGINAL's behaviour for an unbuilt slot, not a port defect, but it is a
 *    visible hole and is stated here rather than hidden.
 *
 * ---------------------------------------------------------------------------
 * REFERENCE BINARY
 *
 * Every body was read from orig/BRGlide.dll -- the project reference -- at the
 * counterparts config/shared.csv gives, and each was re-read at its D3D
 * address to fix the global numbering, because slice6_72.c's transcription is
 * keyed on D3D addresses. The two listings agree instruction for instruction;
 * only the global addresses differ.
 *
 *   D3D         Glide       match method (tools/whereis.py)
 *   0x10044D00  0x1003E250  body
 *   0x10045050  0x1003E5A0  body
 *   0x10045110  0x1003E660  body+callsite
 *   0x10046CD0  0x10040120  body+ptrsite
 *   0x10048710  0x10041B60  callsite   (the phase constructor, BrOptObjCtor)
 *   0x1004A580  0x100439B0  body       (0x10045110's enter hook)
 *   0x10059BB0  0x10052A60  body       (0x10044D00's enter hook)
 *
 * Global numbering, read off the two listings side by side:
 *   0x10AA2904 <-> 0x10AC5C5C     0x10AA2914 <-> 0x10AC5C6C
 *   0x10AA2930 <-> 0x10AC5C88     0x10AA2964 <-> 0x10AC5CBC
 *   0x10AA29B4 <-> 0x10AC5D0C     0x100AA010 <-> 0x100A9360
 *   0x100AC304 <-> 0x100ABAA4     0x10AA28C8 <-> 0x10AC5C20
 *   0x10AA28CC <-> 0x10AC5C24
 * The last two are the only pair the listings cannot separate on their own --
 * 0x10044D00 stores 0 into BOTH, in address order in both builds -- so the
 * assignment above is the ascending-order one and NOTHING in this module
 * depends on it being right. It is recorded so a later reader knows it is
 * unconfirmed rather than measured.
 * ---------------------------------------------------------------------------
 */
#ifndef SLICE8_89_H
#define SLICE8_89_H

#include <stdint.h>

/* Include order matters and is slice8_84.h's: slice6_73.h first (it owns
 * BrOptObjCtor over the canonical BrPhase_), then slice6_72.h for
 * BrUi72Hooks, and br_uinav.h LAST -- it pulls slice3_32.h in under the
 * rename slice3_32.c itself uses, so it can never come first. */
#include "slice6_73.h"   /* BrOptObjCtor (0x10048710), BrOperatorNew        */
#include "slice6_72.h"   /* BrUi72Hooks, BrUiCtl_, BrPhase_, BrPhaseVtbl_   */
#include "br_uinav.h"    /* BrUiNav, g_pBrUiNav -- 0x10AA2904 / 0x100AA010  */

/* ===========================================================================
 * The singletons this module owns. See STORAGE OWNERSHIP above.
 * ========================================================================== */
typedef struct BrUiHook89Ctx {
    BrPhase_ *pAA2914;   /* 0x10AA2914  built by 0x10045110 / 0x1004A580     */
    BrPhase_ *pAA2930;   /* 0x10AA2930  0x10046CD0's destination -- CONFLICT 3 */
    BrPhase_ *pAA2964;   /* 0x10AA2964  built by 0x10044D00 / 0x10059BB0     */
    BrUiCtl_ *pAA29B4;   /* 0x10AA29B4  a CONTROL       -- CONFLICT 1        */
    int32_t   n0AC304;   /* 0x100AC304  cleared then set around 0x10045050   */
} BrUiHook89Ctx;

/* The single instance, zero-initialised -- which is the original's .bss state
 * too: an unbuilt phase slot is NULL until an ACTIVATE fills it. */
extern BrUiHook89Ctx g_brHook89;

/* Put every singleton back WITHOUT releasing anything, for tests and for a
 * host that re-boots the menu in one process. It does not free, because the
 * original never does: the phases are leaked and the LEAVE only drops the
 * reference. Same contract as BrUiHook81Reset / BrUiHook85Reset. */
void BrUiHook89Reset(void);

/* ===========================================================================
 * ACTIVATE
 *
 * 0x10045110 (201 bytes). Slot 0x10AA2914, enter hook 0x1004A580, no
 * prologue. The shape is the family's, documented once in slice7_81.h:
 *
 *     if (slot != NULL) { current = slot; return 1; }   // NO rebuild
 *     p = operator new(0xC8); p = p ? 0x10048710(p) : NULL;
 *     slot = p; current = p;                            // BOTH, even on NULL
 *     if (p == NULL) return 0;                          // NOTE: 0, not 1
 *     p->pfnEnter = 0x1004A580;
 *     slot->pfnEnter(slot);          // the SLOT is re-read for the call
 *     current->f0C = 1;              // and the CURRENT phase is re-read
 *     current->f68 = 1;              // ...and re-read AGAIN
 *     return 1;
 *
 * The three re-reads are the original's: an enter hook that repoints
 * 0x10AA2904 lands f0C and f68 on ITS phase, not on the object just built.
 *
 * HARDENING (port): the original pushes the literal 0xC8. BrPhase_ is larger
 * than that on LP64 and 0x10048710 writes the whole object, so
 * BR_PHASE_ALLOC_SIZE is used -- br_uinav.c, slice2_26.c and slice7_81.c all
 * apply the same hardening at their own call sites.
 *
 * DEVIATION (memory safety): the f0C / f68 stores are NULL-guarded. The
 * original faults if an enter hook NULLed the current phase.
 *
 * The original ignores its argument -- 0x10045050 pushes one and the callee
 * never reads it -- so this takes none, exactly as slice7_81.h's three
 * activates do. */
int32_t BrUiHook89Activate_10045110(void);

/* ===========================================================================
 * The two BrUi72Hooks slots
 * ========================================================================== */

/* 0x10044D00 (221 bytes) An ACTIVATE used DIRECTLY as a control +0x08 hook --
 * it is the only member of the family that is, which is why it has no
 * separate installer. Prologue 0x10AA28C8 = 0x10AA28CC = 0, then the shape
 * above over slot 0x10AA2964 with enter hook 0x10059BB0.
 *
 * Returns 1 (already built, or just built) or 0 (allocation failed), and the
 * 0 is load-bearing: 0x10048180 propagates it and 0x10048530 stops walking
 * the page.
 *
 * GOTCHA, and it is the trap CONVENTIONS.md warns about: the body reads
 * `[esp+4]` four times and NOT ONE of them is the argument. The function
 * opens an SEH frame -- `push -1 / push handler / push fs:[0] / mov fs:[0],esp
 * / push ecx` -- so by the time those reads happen ESP is sixteen bytes lower
 * and `[esp+4]` is the saved fs:[0] link. The argument sits at `[esp+0x14]`
 * and is never touched. `[esp+0xC]` is the SEH try level (0 / -1), not data.
 *
 * PAIRING EVIDENCE: installed by 0x10059760 on the row captioned string 0x6C,
 * read out of slice6_72.c's own transcription of that builder
 * (port/src/slice6_72.c:1213). */
int32_t BrUiHook89_10044D00(BrUiCtl_ *pCtl);

/* 0x10045050 (57 bytes) INSTALLER:
 *
 *     n0AC304 = 0;
 *     0x10045110(pCtl);                    // the argument is ignored
 *     pTarget = 0x10AA29B4;                // read AFTER the activate
 *     n0AC304 = 1;
 *     pTarget->pfn08 = 0x10046CD0;         // wire the new screen's way back
 *     n0AA010 = 0;
 *     return 1;                            // whatever the activate did
 *
 * GOTCHA: the return is an unconditional 1 -- the activate's result is
 * discarded, so an allocation failure is invisible to the caller.
 * GOTCHA: 0x10AA29B4 is dereferenced with no NULL test even though the
 * activate can fail and 0x10046CD0 itself clears the word. DEVIATION: guarded
 * here, exactly as slice7_81.c guards the same pattern.
 *
 * NEAR TWIN, reported so the two are never merged: slice3_31.h's 0x10046380
 * has the same five steps but installs 0x10046D20 rather than 0x10046CD0 and
 * finishes with n0AA010 = 2 rather than 0.
 *
 * PAIRING EVIDENCE: installed by 0x10052030 on the row captioned string 0x1E
 * -- port/src/slice6_72.c:979. */
int32_t BrUiHook89_10045050(BrUiCtl_ *pCtl);

/* 0x10046CD0 (66 bytes) LEAVE:
 *
 *     pCtl->pOwner->pVtbl->f1C(pCtl->pOwner);        // 0x10048AA0
 *     if (current != NULL) current->pVtbl->f00(current, 1);
 *     pNext = 0x10AA2930;                            // read BEFORE the clears
 *     0x10AA2914 = NULL; 0x10AA29B4 = NULL;
 *     current = pNext;
 *     return 0;
 *
 * The load-before-clear order is the original's and is preserved, though it
 * clears nothing it is about to read. slice3_31.h's summary of this address
 * ("-> pAA2930; clears pAA2914 and pAA29B4") is an independent second sighting
 * of all three words.
 *
 * PAIRING: not installed by any builder -- 0x10045050 pokes it into
 * 0x10AA29B4's +0x08 at run time, which is the original's own mechanism and
 * the same one slice7_81.c's 0x10046AD0 / 0x10046B10 / 0x10046D70 use. */
int32_t BrUiHook89_10046CD0(BrUiCtl_ *pCtl);

/* ===========================================================================
 * Installation
 *
 * Fills p10044D00 and p10045050 and TOUCHES NOTHING ELSE, so it composes with
 * BrUiHook84Install72 and BrUiOptInstall72 in any order. A NULL argument is a
 * no-op, as in both of those. 0x10046CD0 has no slot in BrUi72Hooks and needs
 * none: no builder installs it.
 * ========================================================================== */
void BrUiHook89Install72(BrUi72Hooks *pHooks);

#endif /* SLICE8_89_H */
