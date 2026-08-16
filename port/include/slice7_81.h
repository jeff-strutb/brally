/* slice7_81.h -- the SCREEN-ENTRY and PHASE-CHANGING control hooks.
 *
 * ---------------------------------------------------------------------------
 * WHAT THIS MODULE IS
 *
 * br_uinav.c ports TWO of the ~50 `pfn08` bodies the sixteen screen builders
 * install on their menu rows: 0x10045AF0 (forward) and 0x10046C90 (back). This
 * module is the rest of that family -- the hooks that BUILD a screen, that
 * REPUBLISH one, and that TEAR one down and go somewhere else:
 *
 *   ACTIVATE     lazily construct a 0xC8 phase with 0x10048710, publish it in
 *                a per-screen singleton AND in the current-phase global
 *                0x10AA2904, install an enter hook, and CALL it. The enter hook
 *                is a screen builder.
 *   INSTALLER    run an ACTIVATE, then poke a LEAVE routine into some OTHER
 *                control's +0x08 slot -- i.e. wire up the back row of the
 *                screen that was just built.
 *   LEAVE        drive the owning phase's vtable +0x1C (0x10048AA0, release
 *                every page), notify the current phase through ITS +0x00 with
 *                1, clear some singletons, and repoint 0x10AA2904.
 *
 * ---------------------------------------------------------------------------
 * DUPLICATE OWNERSHIP -- READ THIS BEFORE ADDING OR "FIXING" ANYTHING
 *
 * EVERY address below is ALREADY PORTED, in port/src/slice3_31.c (and
 * 0x100450F0 in port/src/slice2_26.c), over slice2_25.h's `BrGameObj` and
 * slice2_26.h's `BrPhaseCtx`. Those bodies are correct and are NOT being
 * replaced. This file is a SECOND TRANSCRIPTION over br_ui.h's struct model,
 * for exactly the reason br_uinav.h gives for its own seven duplicates:
 *
 *     a byte-offset body cannot be pointed at a struct whose fields have
 *     moved under LP64.
 *
 * and here that is not a stylistic point, it is load-bearing. Measured, not
 * assumed:
 *
 *   Every LEAVE routine starts `mov ecx,[eax+0x2AE8] / call [[ecx]+0x1C]`.
 *   slice2_25.h models +0x2AE8 as `BrGameObj::pSub` and slice3_31.c calls
 *   `pObj->pSub->pVtbl->pfnSlot7(pObj->pSub)`. But br_ui.h puts `BrUiCtl_::
 *   pOwner` at that same original +0x2AE8 (slice6_73.h records the rename
 *   `f2AE8 -> pOwner`), the phase vtable's slot +0x1C is 0x10048AA0
 *   (br_phase.h reads the table out of the image at 0x1008F700), and
 *   0x10048AA0 is "release every page of this phase" -- which is precisely
 *   what a leave routine has to do first.
 *
 *   So the "entity"/"BrGameObj" argument of every one of these hooks IS THE
 *   CONTROL THAT CARRIES THE HOOK, and its "+0x2AE8 sub-object" is that
 *   control's owning PHASE. br_uinav.c's transcription of 0x10046C90 -- the
 *   one member of the family that was already done over the struct model --
 *   writes exactly `pOwner->pVtbl->f1C(pOwner)`, which is an independent
 *   second sighting of the same fact.
 *
 *   On a 32-bit host both readings address the same bytes. On LP64 they do
 *   not: `BrGameObj`'s pSub sits at a host offset the widened `BrUiCtl_` has
 *   moved past. Handing a `BrUiCtl_ *` to slice3_31.c's leave routines would
 *   link cleanly and read a wild pointer.
 *
 * The one place delegation WOULD have been safe is the two ACTIVATE bodies,
 * whose every field is a `BrPhase_ *`. They are transcribed here anyway,
 * because delegating them means instantiating slice3_31.c's `BrPhaseCtx31`,
 * and that struct carries a SECOND storage for 0x10AA292C / 0x10AA2974 /
 * 0x10AA2928 / 0x10AA2918 -- the aliased-storage bug CONVENTIONS.md describes,
 * introduced deliberately. Nothing in the tree instantiates BrPhaseCtx31
 * today, so this module owns those words and says so below.
 *
 * ---------------------------------------------------------------------------
 * STORAGE OWNERSHIP, stated address by address
 *
 * OWNED ELSEWHERE -- reached, never duplicated:
 *   0x10AA2904  current phase        br_uinav.h  BrUiNav::pAA2904
 *   0x10AA2908  root phase           br_uinav.h  BrUiNav::pAA2908
 *   0x10AA2924                       br_uinav.h  BrUiNav::pAA2924
 *   0x100AA010                       br_uinav.h  BrUiNav::n0AA010
 *   0x10ACED34                       br_uinav.h  BrUiNav::nACED34
 *   0x10AA29C0  a CONTROL            br_uinav.h  BrUiNav::pAA29C0
 *   0x10AA29B0  a CONTROL            slice6_71.h g_brS71.pAA29B0
 *   0x10AA29C8  a CONTROL            slice6_73.h g_br73.pAA29C8
 *   0x10AA29F0  a CONTROL            slice6_73.h g_br73.pAA29F0
 *   0x10AA29F4  a CONTROL            slice6_73.h g_br73.pAA29F4
 *   0x10AA29CC                       slice6_73.h g_br73.pAA29CC
 *   0x10AA2518 / 0x10A9D618          slice6_73.h g_br73.szAA2518 / szA9D618
 *   0x1039B720                       slice6_73.h g_aBr39B720
 *   0x100AB3F4                       slice5_61.h g_br0AB3F4
 *
 * OWNED HERE (BrUiHook81Ctx below). Every one is also DECLARED, but never
 * defined, by slice2_26.h's BrPhaseCtx or slice3_31.h's BrPhaseCtx31; if
 * either of those is ever instantiated, exactly one of the two must keep the
 * storage and the other must alias into it:
 *   0x10AA2918  0x10AA2928  0x10AA292C  0x10AA2934  0x10AA2938  0x10AA293C
 *   0x10AA2940  0x10AA2958  0x10AA2974  0x10AA2980  0x10AA2990  0x10AA291C
 *   0x10AA28E4
 *
 * ---------------------------------------------------------------------------
 * CONFLICTS FOUND, reported rather than silently "resolved"
 *
 * 1. 0x10AA29B0 / 0x10AA29C8 / 0x10AA29F0 / 0x10AA29F4 are CONTROLS, not
 *    phases. slice2_26.h types all four `BrPhase *` and its comment on
 *    0x10AA29B0 reads "receives the +0x08 hook"; slice3_31.h repeats that.
 *    But slice6_71.c:289 stores a `BrUiCtl_ *` into 0x10AA29B0, and
 *    slice6_73.c stores `BrUiCtl_ *` into 0x10AA29C8 (:499, :507, :613),
 *    0x10AA29F0 (:491) and 0x10AA29F4 (:725) -- and those are the WRITERS,
 *    so they settle it.
 *
 *    The reading survived because both objects carry a code pointer at
 *    original +0x08 (phase pfnHook, control pfn08) and, on LP64, both land at
 *    the same host offset -- two pointers in. What does NOT survive is the
 *    TYPE: BrPhaseHookFn_ is `void (*)(void *)` and BrUiCtlHookFn_ is
 *    `int32_t (*)(BrUiCtl_ *)`. A leave routine installed through the phase
 *    model and then called by 0x10048180 through the control model is called
 *    through the wrong prototype. This module installs the control-typed
 *    hook, which is what 0x10048180 actually invokes.
 *
 * 2. 0x10AA291C is a `BrPhase_ *`, not an int32. br_uinav.h:237 declares
 *    `int32_t nAA291C` on the strength of 0x10046C90, which only CLEARS it.
 *    0x10046D70 READS it and stores it into 0x10AA2904, and 0x10045900
 *    BUILDS a phase into it (slice3_31.h). The field should be retyped
 *    `BrPhase_ *pAA291C`; until it is, this module owns a pointer-typed copy
 *    and the two agree only while both are NULL -- which is the state the
 *    host harness is in, because nothing has ported 0x10045900's caller.
 *
 * 3. 0x10AA29CC: slice3_31.h declares `int32_t nAA29CC`, slice6_73.h declares
 *    `unsigned char *pAA29CC` and calls it "the twin of g_brPAA29D0". The
 *    writers here (0x10046B10, 0x10046EB0) store a plain 0 dword, which is
 *    consistent with either -- but only the pointer model has storage, so
 *    that is the one used, and the store NULLs the record array the host
 *    wired. Faithful: the original clears the same dword.
 *
 * 4. RETURN TYPES. slice3_31.h declares every LEAVE `void` "because eight of
 *    them are stored into BrPhase.pfn08, whose type is void(void*)". They are
 *    not: they are stored into a CONTROL's +0x08, whose type (br_ui.h ADJ-8)
 *    is `int32_t (*)(BrUiCtl_ *)`, and every one of them ends `xor eax,eax /
 *    ret`. The 0 matters: 0x10048180 returns 0 when a +0x08 hook returns 0,
 *    and 0x10048530 then stops walking the page -- which is how the frame
 *    that tore its own screen down does not go on walking it. br_uinav.h
 *    records the same thing for 0x10046C90. The `int32_t` reading is used
 *    here and the constant is preserved on every path.
 *
 * ---------------------------------------------------------------------------
 * REFERENCE BINARY
 *
 * Read from orig/BRGlide.dll (the reference) and cross-checked against
 * orig/BRD3D.dll. config/shared.csv classes the whole family `shared`; the
 * pairs used are 0x10045AA0<->0x1003EF40, 0x10045C90<->0x1003F130,
 * 0x1003E680<->0x10037C90 and 0x1003E510<->0x10037B20. The two listings for
 * 0x10045AA0 are instruction-for-instruction identical, differing only in the
 * global addresses (0x100AA010<->0x100A9360, 0x10ACED34<->0x10AF2094,
 * 0x10AA29B0<->0x10AC5D08).
 *
 * MEASURED, and it corrects a brief: 0x10045AA0's three callees were reported
 * as "none yet ported". All three are ported --
 *   0x1003E680  slice6_70.c BrExt_1003E680  (slice6_73.c has a copy, renamed
 *               under BR_HOST_LINK, so the host link has exactly one)
 *   0x1003E510  slice5_61.c BrSub1003E510
 *   0x10045C90  slice3_31.c BrExt_10045C90
 * -- and 0x10045AA0 ITSELF is ported, as slice3_31.c's BrPhaseHook_10045AA0.
 * What was missing was never a body; it was a control-typed adapter and the
 * storage for the singletons. That is what this file is.
 * ---------------------------------------------------------------------------
 */
#ifndef SLICE7_81_H
#define SLICE7_81_H

#include <stdint.h>

/* br_uinav.h pulls slice3_32.h in under the rename slice3_32.c itself uses,
 * so it must come AFTER slice6_73.h and never the other way round. Same order
 * port/host/brally.c uses, and for the same reason. */
#include "slice6_73.h"   /* g_br73, BrUi73Hooks, BrOptObjCtor, BrOperatorNew */
#include "slice6_71.h"   /* g_brS71 -- owns 0x10AA29B0                       */
#include "br_uinav.h"    /* BrUiNav, g_pBrUiNav -- owns 0x10AA2904/08/24     */

/* ===========================================================================
 * The singletons this module owns. See STORAGE OWNERSHIP above: everything
 * that another header already defines is reached there and is NOT repeated
 * in here.
 * ========================================================================== */
typedef struct BrUiHook81Ctx {
    BrPhase_ *pAA2918;   /* 0x10AA2918  built by 0x100451E0 / 0x1004BDC0     */
    BrPhase_ *pAA291C;   /* 0x10AA291C  built by 0x10045900  -- CONFLICT 2   */
    BrPhase_ *pAA2928;   /* 0x10AA2928  built by 0x10045BC0 / 0x10050060     */
    BrPhase_ *pAA292C;   /* 0x10AA292C  built by 0x10045C90 / 0x100509F0     */
    BrPhase_ *pAA2934;   /* 0x10AA2934  built by 0x10045EA0 / 0x10052F50     */
    BrPhase_ *pAA2938;   /* 0x10AA2938  built by 0x10045F70 / 0x10053CF0     */
    BrPhase_ *pAA293C;   /* 0x10AA293C  built by 0x100460A0 / 0x10054B50     */
    BrPhase_ *pAA2940;   /* 0x10AA2940 */
    BrPhase_ *pAA2958;   /* 0x10AA2958 */
    BrPhase_ *pAA2974;   /* 0x10AA2974  the SECOND object 0x10045C90 builds  */
    BrPhase_ *pAA2980;   /* 0x10AA2980  built by 0x10045390 / 0x1004D1F0     */
    BrPhase_ *pAA2990;   /* 0x10AA2990  built by 0x10045460 / 0x1004D640     */
    int32_t   nAA28E4;   /* 0x10AA28E4  cleared by the two name-reset leaves */
} BrUiHook81Ctx;

/* The single instance, zero-initialised. A phase singleton starting NULL is
 * the original's state too: every ACTIVATE below is the thing that fills it,
 * and 0 is what .bss holds until one runs. */
extern BrUiHook81Ctx g_brHook81;

/* Put every singleton back to NULL WITHOUT releasing anything -- for tests
 * and for a host that re-boots the menu in one process. It does not free,
 * because the original never does either: the phases are leaked on purpose
 * and the LEAVE routines only drop the reference. */
void BrUiHook81Reset(void);

/* ===========================================================================
 * ACTIVATE
 *
 * Shape, and it is the same in all three (br_uinav.h documents it once for
 * 0x10045AF0; repeated here because the GOTCHAs are per-body):
 *
 *     if (slot != NULL) { current = slot; return 1; }   // NO rebuild
 *     p = operator new(0xC8); p = p ? 0x10048710(p) : NULL;
 *     slot = p; current = p;
 *     if (p == NULL) return 0;                          // NOTE: 0, not 1
 *     p->pfnEnter = <builder>;
 *     slot->pfnEnter(slot);          // the SLOT is re-read for the call
 *     current->f0C = 1;              // and the CURRENT phase is re-read
 *     current->f68 = 1;              // ...and re-read AGAIN
 *     return 1;
 *
 * The three re-reads are the original's and are preserved: an enter hook that
 * repoints 0x10AA2904 lands these two flags on ITS phase, not on the object
 * just constructed.
 *
 * HARDENING (port), the same one br_uinav.c and slice2_26.c document: the
 * original pushes the literal 0xC8. BrPhase_ is bigger than that on LP64 and
 * 0x10048710 writes the whole object, so BR_PHASE_ALLOC_SIZE is used.
 *
 * DEVIATION (memory safety): the three f0C / f68 stores are NULL-guarded. The
 * original would fault if an enter hook had NULLed the current phase.
 *
 * All three ignore their argument in the original -- 0x10045AA0 pushes one at
 * 0x10045C90 and the callee never reads it -- so they take none here.
 * ========================================================================== */

/* 0x10045C90 (293 bytes) TWO objects. First 0x10AA292C with enter hook
 * 0x100509F0 and BOTH flags; then, on the just-built path only, a second
 * 0xC8 object in 0x10AA2974 with enter hook 0x10049F40 and f0C ONLY -- no
 * f68. Either allocation failing returns 0.
 *
 * GOTCHA: only the FIRST object is published as the current phase. The second
 * is built, filled by a real builder and left unreferenced by 0x10AA2904, so
 * "New Season" lands on the 0x100509F0 screen and the 0x10049F40 screen is
 * reached later, through the +0x08 hook 0x10045AA0 pokes into it. */
int32_t BrUiHook81Activate_10045C90(void);

/* 0x10045BC0 (201 bytes) Slot 0x10AA2928, enter hook 0x10050060. */
int32_t BrUiHook81Activate_10045BC0(void);

/* 0x100451E0 (214 bytes) Prologue BrExt_100419D0(0x100AD300). Slot
 * 0x10AA2918, enter hook 0x1004BDC0. */
int32_t BrUiHook81Activate_100451E0(void);

/* ===========================================================================
 * INSTALLERS -- control +0x08 hooks. All return 1, whatever the activate did.
 * ========================================================================== */

/* 0x10045AA0 (72 bytes) "New Season".
 *
 *     n0AA010 = 0; 0x1003E680(); nACED34 = 0;
 *     0x10045C90(pCtl);                       // the argument is ignored
 *     0x10AA29B0->pfn08 = 0x10046D70;         // the back row of the screen
 *     n0AA010 = 0;                            // ...again
 *     0x1003E510();
 *     return 1;
 *
 * GOTCHA: n0AA010 is cleared TWICE with only the activate between, and the
 * activate does not read it.
 * GOTCHA: 0x10AA29B0 is dereferenced with no NULL check even though the
 * activate can fail. DEVIATION: guarded here.
 *
 * PAIRING EVIDENCE: installed by 0x1004F2B0 on the control whose caption is
 * string 0x0B. Read out of slice6_73.c's own transcription of that builder
 * (port/src/slice6_73.c:435-437) --
 *     control 2  (string 0x0A)  pfn08 = 0x10045AF0
 *     control 3  (string 0x0B)  pfn08 = 0x10045AA0   <-- this one
 *     control 4  (string 0x0C)  pfn08 = 0x10046C90
 * -- which is the same three-row table br_uinav.h quotes. */
int32_t BrUiHook81_10045AA0(BrUiCtl_ *pCtl);

/* 0x100458A0 (32 bytes)  0x10045BC0(pCtl); 0x10AA29F4->pfn08 = 0x10046B10.
 *
 * PAIRING EVIDENCE: installed by 0x10054B50 on the FIRST of its three
 * rectangle controls (place flags 0x402001, step id 0x78/0x79) --
 * port/src/slice6_73.c:835-836. slice6_72.c:1004 shows 0x10052030 installing
 * the neighbouring 0x100457E0 on its own first rectangle, the same way. */
int32_t BrUiHook81_100458A0(BrUiCtl_ *pCtl);

/* 0x10045880 (32 bytes)  0x100451E0(pCtl); 0x10AA29C8->pfn08 = 0x10046AD0.
 *
 * PAIRING EVIDENCE: installed by 0x10054B50 on the THIRD rectangle control
 * (place flags 0x402001, step id 0x54/0x55) -- port/src/slice6_73.c:863-864. */
int32_t BrUiHook81_10045880(BrUiCtl_ *pCtl);

/* 0x100450F0 (30 bytes)  Dispatch, and the odd one out:
 *
 *     0x10AA29F4->pfn08(pCtl);    // ANOTHER control's hook, THIS control as
 *     n0AA010 = 0;                // the argument -- br_phase.h's "+0x08 takes
 *     return 0;                   // the CALLER's own argument" asymmetry
 *
 * GOTCHA: it returns 0 where its three neighbours return 1, so it stops the
 * frame (see CONFLICT 4). GOTCHA: 0x10AA29F4 is dereferenced unguarded;
 * DEVIATION: guarded.
 *
 * PAIRING EVIDENCE: installed by 0x10050060 -- port/src/slice6_73.c:713-715,
 * on the control that same builder then records into 0x10AA29F4 at :725.
 * ALREADY PORTED as slice2_26.c's BrPhaseDispatch_100450F0(BrPhaseCtx *,
 * void *); this is the control-typed transcription (CONFLICT 1). */
int32_t BrUiHook81_100450F0(BrUiCtl_ *pCtl);

/* ===========================================================================
 * LEAVE -- control +0x08 hooks. Every one returns 0 (CONFLICT 4).
 *
 * Shared prologue, in this order:
 *     pCtl->pOwner->pVtbl->f1C(pCtl->pOwner);        // 0x10048AA0
 *     if (current != NULL) current->pVtbl->f00(current, 1);
 * The owner is dereferenced unguarded, exactly as the original does; only the
 * current phase is NULL-tested, and that test IS the original's.
 *
 * GOTCHA (load order), preserved: five of the seven read their DESTINATION
 * out of its singleton BEFORE clearing the others; the two name-reset ones
 * (0x10046B10, 0x10046EB0) read it AFTER. No routine clears the word it is
 * about to read, so the two orders agree -- but they are reproduced anyway.
 * ========================================================================== */

/* 0x100463C0 (56 B)  -> 0x10AA2958; clears 0x10AA2940.
 * PAIRING: installed by 0x100558A0 -- port/src/slice6_73.c:313-314. */
int32_t BrUiHook81_100463C0(BrUiCtl_ *pCtl);

/* 0x10046620 (66 B)  -> 0x10AA2980; clears 0x10AA2990 and 0x10AA29F0.
 * PAIRING: installed by 0x1004D640 -- port/src/slice6_73.c:503-504. */
int32_t BrUiHook81_10046620(BrUiCtl_ *pCtl);

/* 0x10046AD0 (56 B)  -> 0x10AA293C; clears 0x10AA2918.
 * PAIRING: not installed by any builder directly -- 0x10045880 pokes it into
 * 0x10AA29C8's +0x08 at run time (see above). */
int32_t BrUiHook81_10046AD0(BrUiCtl_ *pCtl);

/* 0x10046B10 (158 B) -> 0x10AA293C, with the name reset: clears 0x10AA2928,
 * 0x10AA29C0, 0x10AA29CC and 0x10AA28E4, sets 0x100AB3F4 to -1 and copies
 * 0x1039B720 into BOTH 0x10AA2518 and 0x10A9D618.
 * DEVIATION: the original's two copies are unbounded inline `rep movs`; here
 * they are bounded by g_br73.cbScratch and always NUL-terminated, which is
 * slice3_31.c's deviation too.
 * PAIRING: poked into 0x10AA29F4's +0x08 by 0x100458A0. */
int32_t BrUiHook81_10046B10(BrUiCtl_ *pCtl);

/* 0x10046D70 (76 B)  -> 0x10AA291C; clears 0x10AA292C, 0x10AA29B0 and
 * 0x10AA2974 -- i.e. it drops BOTH objects 0x10045C90 built and the control
 * it wired.
 * PAIRING: poked into 0x10AA29B0's +0x08 by 0x10045AA0. */
int32_t BrUiHook81_10046D70(BrUiCtl_ *pCtl);

/* 0x10046EB0 (158 B) The twin of 0x10046B10, byte for byte apart from the
 * destination: -> 0x10AA2934.
 * PAIRING: installed by 0x10050060 -- port/src/slice6_73.c:721-722. */
int32_t BrUiHook81_10046EB0(BrUiCtl_ *pCtl);

/* 0x100470E0 (56 B)  -> 0x10AA2938; clears 0x10AA293C.
 * PAIRING: installed by 0x10054B50 on the row whose caption is string 0x0C
 * ("Back") -- port/src/slice6_73.c:823-824. */
int32_t BrUiHook81_100470E0(BrUiCtl_ *pCtl);

/* ===========================================================================
 * Installation
 *
 * The original stores these as literal addresses inside each builder; the
 * port turns that into slice6_73.h's BrUi73Hooks table, so a host fills the
 * table once and the builders read it. This fills the eleven slots above and
 * TOUCHES NOTHING ELSE -- an unwired slot must stay a visible hole, so the
 * remaining entries are left exactly as the caller had them.
 * ========================================================================== */
void BrUiHook81Install(BrUi73Hooks *pHooks);

#endif /* SLICE7_81_H */
