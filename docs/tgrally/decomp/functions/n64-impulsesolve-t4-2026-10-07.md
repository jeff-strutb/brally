# N64 impulsesolve t4

*Recorded 2026-10-07.*

> BrCrImpulseSolve 0x8025BBB8 T4 (f21b6f76, session bfa2d4): invisible FP ring temp = dead product feeding a dropped empty NaN test; uopt colour order f0,f2,f12..f18 by priority; replay harness recipe

BrCrImpulseSolve T4 by hand, f21b6f76, image gate 728/0. Closes [n64-impulsesolve-wip-2026-10-07](n64-impulsesolve-wip-2026-10-07.md) / [n64-impulsesolve-wip-2026-10-06](n64-impulsesolve-wip-2026-10-06.md).

**The lever (general, reuse it):** a ROM that needs ONE extra FP temp get+free with no visible instruction = a dead value computed into a uopt register and consumed only by an empty `if`:
`x = tt[2] * spare[0]; if (x != x);` at the flag-arm start. uopt keeps it (float compares may trap, so the empty if survives uopt and keeps x live); ugen drops the adjacent empty if but evaluates the assignment; the memory operand's load takes the ring head; as1 deletes the dead load and mul. Conditions that had to hold, each measured:
- binary op with one non-candidate memory operand (unary ops pass the dest hint to the child = no ring get; a shared cached node = hold instead);
- the operand must not alias promoted arrays: a load through a pointer (b->mass) makes uopt flush/split promoted array webs (tt) around it; an uninitialised local array element (spare[0]) is safe;
- must NOT read a value whose extra use raises its uopt save (tn[0] read -> coloured f18, kills the ring); empty ifs on a cached node do NOT hold it (ugen drops them before building trees);
- x must be a fresh scalar: uopt webs follow the variable (reusing dd/r merged ranges and moved r f0->f2); its home slot came out of an unused array (spare[3] -> spare[2] + x) to keep the frame;
- read the tt element that must win priority (tt[2]); reading tt[0] swapped f12/f14.
- uopt FP colours: 24=f0, 25=f2, 26=f12, 27=f14, 28=f16, 29=f18; p1 colours by save descending, lowest free colour. f2 was free at the flag-arm start (zero web ends, K not yet loaded) so x took f2.

**Tools built this session (scratch, rebuildable):** ucode replay = run cfe/uopt by hand (cc -v shows the commands; `-XSx.Su` symtab), patch x.O records (workbench parse_ucode raw words, register = word 3 of Rmt lod/str), stock ugen + as1, grade the .o with n64build.grade. Injection (instrE5 DKWB_INJECT_FP) showed pass 2 needs the event between diff2 and the join, pass 1 anywhere from line ~361 to the next call. ugen -S text reassembled through as0 is NOT faithful (different scheduling).
Related: [n64-ugen-invisible-pop-lever](../levers/n64-ugen-invisible-pop-lever.md), [feedback-hand-transcription-only](../rules/hand-transcription-only.md).

**Respelt 2026-10-08 (521f56, c929ae46, the project lead asked for a believable form):** the probe is now `x = tt[2] * rest; if (x != x);` (a debug NaN check of a real input), spare[2] kept unused for the frame. Measured operands: rest, D_802AB7FC, D_802AB804 all EXACT; D_802AB800 308 (CSEs with the scale load that follows), b->mass 239 (pointer load flushes tt), D_8037EAA8[2] 222.
