/* slice6_77.h -- two callees the ported screen builders reach through their
 * module-context function tables, decompiled from BRD3D.dll.
 *
 * WHY THESE TWO, AND WHY TOGETHER
 * -------------------------------
 * They have nothing in common as game logic. They were found together because
 * they are the only two NULL slots that stop a ported screen builder from
 * running to completion in the host harness:
 *
 *   0x100586A0  BrSub100586A0   BrS71Env::pfn100586A0, called by
 *                               BrOptFn100575F0 (slice6_71.c) as its FIRST
 *                               statement -- the builder died before it had
 *                               allocated anything.
 *   0x100795D0  BrFfbReprobe    Br72Env::pfn100795D0, called by
 *                               BrExt_1004E830 (slice6_72.c) after the title
 *                               control, i.e. two controls in.
 *
 * Both slots were NULL because their targets were unported. A NULL jump
 * destroys the frame pointer chain, so both showed up in lldb as a bare
 * `frame #0: 0x0` with no backtrace; both were pinned by breaking on the
 * source line and reading the slot rather than by unwinding.
 *
 * ==========================================================================
 * 0x100586A0 -- the slot-table reset
 * ==========================================================================
 * Thirty-five bytes:
 *
 *     mov  [0x10AA288C], 0
 *     eax = 0x10AA253C
 *   loop:
 *     mov  [eax-4], -1        ; id
 *     mov  [eax],    0
 *     mov  [eax+4],  0
 *     eax += 12
 *     cmp  eax, 0x10AA259C ; jl loop
 *
 * so eight 12-byte records from 0x10AA2538, and the free marker is -1, NOT 0
 * (zero is a valid slot id).
 *
 * ALREADY PORTED, UNDER ANOTHER NAME. br_slots.c has carried this body since
 * the first pass as `BrSlotsReset(BrSlotTable *)`. The reason the two were
 * never joined up is recorded in port/host/br_wire75.c's "audited and
 * deliberately not wired" list: the original takes NO argument and operates on
 * two globals that a `BrSlotTable` instance cannot represent, because the
 * counter it clears (0x10AA288C) is 0x2F4 bytes past the end of the eight-slot
 * array (0x10AA2538..0x10AA2598) and is separately the DirectPlay send gate.
 *
 * This module closes that gap the way br_wire75.c said it had to be closed --
 * with an adjudication rather than a cast. The storage is slice2_25's, which
 * already owns both addresses SEPARATELY and correctly (`g_aBrAA2538` and
 * `g_brAA288C`); the loop is br_slots.c's, factored out as BrSlotsResetArray
 * so that one original address keeps exactly one body.
 *
 * CONSEQUENCE WORTH KNOWING: because 0x10AA288C doubles as the DirectPlay send
 * gate, every call to this function OPENS that gate. See the warning at the
 * foot of br_slots.h.
 *
 * ==========================================================================
 * 0x100795D0 -- the force-feedback re-probe
 * ==========================================================================
 * 145 bytes, and it is pure global bookkeeping around two calls that are
 * already ported:
 *
 *     save   0x10B4E1D0 (mode) and 0x10B4E1E0 (exclusive flag)
 *     force  mode = 2, 0x10B4E1D4 = &record[2], exclusive = 1
 *     call   0x100791D0   (BrFfbInit,     slice3_45.c)
 *     call   0x10079550   (BrFfbShutdown, slice1_10.c)
 *     restore mode
 *     0x10B4E1D4 = &record[mode in 1..3 ? mode : 0]
 *     restore exclusive
 *
 * So it tears the wheel down and brings it back up under a known mode purely
 * to refresh 0x118ABDBC ("a force-feedback device is present and exclusive"),
 * which BrExt_1004E830 reads on the very next line to decide whether its
 * force-feedback row is selectable. That is why a screen BUILDER, of all
 * things, calls into DirectInput.
 *
 * THE FOUR RECORDS. 0x10B4DF30, 0x10B4DFD8, 0x10B4E080 and 0x10B4E128 are
 * 0xA8 apart, i.e. records 0..3 of slice2_25's `g_aBrB4DF30`. slice5_63.h
 * already documented this exact selection for 0x1005FBC0, and slice2_25.c
 * already implements it for 0x10043400 -- 0x100795D0 is the third site.
 *
 * GOTCHA: the selection is a `dec/je` chain tested three times, so mode 0 AND
 * every mode above 3 both land on record 0. Not a clamp -- a fall-through.
 *
 * GOTCHA: the mode is restored BEFORE the chain runs (the chain destroys the
 * register it was held in), so the store to 0x10B4E1D0 is not conditional on
 * which record is chosen.
 *
 * GOTCHA (fidelity, no observable effect): on the mode == 1 path the original
 * stores the exclusive flag BEFORE the record pointer; the other three paths
 * store it after. Different globals, so the order cannot be observed. This
 * port uses one order for all four.
 *
 * PLATFORM BOUNDARY. This function is portable; its callee BrFfbInit is not
 * self-contained -- it dereferences the IDirectInput root at 0x118ABD70
 * without a NULL test, exactly as the original does. That pointer is a COM
 * object and does not belong in portable code, so the HOST supplies it (see
 * port/host/br_wire77.c). Nothing about DirectInput is added here.
 */
#ifndef SLICE6_77_H
#define SLICE6_77_H

/* 0x100586A0. Argumentless, as the original is: it resets slice2_25's
 * g_aBrAA2538 in place and clears g_brAA288C.
 *
 * Also declared by slice2_25.h and slice6_70.h, which have called it since
 * before it had a body. Both declarations agree with this one. */
void BrSub100586A0(void);

/* 0x100795D0. Argumentless and void; the original loads nothing into eax on
 * any of its four exits and no caller examines a result. */
void BrFfbReprobe(void);

#endif /* SLICE6_77_H */
