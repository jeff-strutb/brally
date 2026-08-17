/* br_phasecur.h -- THE CURRENT PHASE, 0x10AA2904 (Glide 0x10AC5C5C).
 *
 * WHAT IT IS: the one place the front end records which screen the player is
 * looking at.  Every menu row that goes somewhere writes it, and the thing
 * that draws the screen reads it, so if a row writes one copy and the drawing
 * reads another the game looks like it is ignoring the player.
 *
 * ==========================================================================
 * WHY THIS FILE EXISTS
 * ==========================================================================
 *
 * The original has ONE dword.  The disassembly is unambiguous about that --
 * every accessor is a plain absolute store or load, with no object in sight:
 *
 *     10046F50  a1 74 29 aa 10     mov eax, [0x10aa2974]
 *     10046F55  a3 04 29 aa 10     mov [0x10aa2904], eax     <- 0x10046F50
 *     100444CF  8b 0d 04 29 aa 10  mov ecx, [0x10aa2904]     <- 0x100444C0
 *     10044500  89 0d 04 29 aa 10  mov [0x10aa2904], ecx
 *
 * This tree had NINE declarations claiming to be that dword, of which six had
 * real storage.  They are listed in tools/aliasmap.py --addr 0x10AA2904.  Four
 * of the six were `BrPhase_ *` under three names and one anonymous type alias
 * apiece, and they drifted apart after the first write:
 *
 *     BrUiNav::pAA2904      port/host/brally.c's g_nav      -- what the frame
 *                                                              loop READS
 *     BrPhaseCtx::pAA2904   port/host/br_wire78.c's         -- what slice2_26,
 *                           g_phaseBase                        slice3_31 and
 *                                                              br_phaseact
 *                                                              WROTE
 *     g_brPAA2904           port/src/slice2_25.c            -- what slice2_25,
 *                                                              slice4_50 and
 *                                                              slice5_63 WROTE
 *     BrS71Globals::pAA2904 port/src/slice6_71.c's g_brS71  -- what 0x10038F30
 *                                                              READ
 *
 * Measured, before this file existed: driving the main menu with
 * `./build/brally -keys root ".dx"` ran a row's action, the action published a
 * phase, and `phase=` in the frame report did not move -- because the action
 * published into the second or third of those and the report reads the first.
 * "The menu does not navigate" was investigated as a bug in the menu for weeks;
 * the menu was fine.
 *
 * ==========================================================================
 * HOW IT IS COLLAPSED, AND WHY NOT BY A CAST
 * ==========================================================================
 *
 * All four are `BrPhase_ *` already -- `slice2_25.h` has
 * `typedef BrPhase_ BrOptObj` and `slice2_26.h` has `typedef BrPhase_ BrPhase`
 * -- so no retyping is needed and no cast is involved.  This is NOT the
 * BrPhase_/BrUiPhase situation, where two models disagree about where the
 * object starts and casting one onto the other lands `cScreen` in the low half
 * of a vtable pointer.
 *
 * The slot is reached through a POINTER the host binds, rather than being a
 * plain global, for one reason: `BrUiNav::pAA2904` must stay an ordinary
 * member.  port/src/menus/br_uinav.c reads and writes it at thirteen sites and
 * a transcription pass owns that directory.  So the host's `g_nav.pAA2904` IS
 * the storage, and every other module reaches it through `BR_PHASE_CUR`.
 * `slice4_52.h`'s `BrShutObj **ppAA2904` is the same pattern, already in the
 * tree, for the same address.
 *
 * Unbound -- which is how every unit test runs -- `BR_PHASE_CUR` names a
 * fallback of its own, so a suite that never calls BrPhaseCurBind still has
 * exactly one slot.  What it must never do is name NO slot: a NULL here would
 * turn a plain assignment into a fault.
 */
#ifndef BR_PHASECUR_H
#define BR_PHASECUR_H

#include "br_phase.h"    /* BrPhase_ */

/* The one slot, indirected.  Never NULL: it starts out aimed at this module's
 * own storage and BrPhaseCurBind re-aims it. */
extern BrPhase_ **g_ppBrPhaseCur;

/* 0x10AA2904 as an lvalue.  Read it, assign it, and take its address -- the
 * activate helpers pass `&BR_PHASE_CUR` where the original passes the
 * address of the global. */
#define BR_PHASE_CUR   (*g_ppBrPhaseCur)

/* Aim the slot at the host's storage.  Call once, before anything navigates.
 * A NULL argument restores this module's own fallback rather than unbinding,
 * because there is no safe "no slot" state. */
void BrPhaseCurBind(BrPhase_ **ppSlot);

/* Which slot is in use, for the harness reports.  Comparing this against
 * `&g_nav.pAA2904` is how a run proves the collapse actually happened rather
 * than being asserted in a comment. */
BrPhase_ **BrPhaseCurSlot(void);

#endif /* BR_PHASECUR_H */
