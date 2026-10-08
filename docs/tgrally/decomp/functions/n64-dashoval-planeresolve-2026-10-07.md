# N64 dashoval planeresolve

*Recorded 2026-10-07.*

> BrPaintDashOval 0x802523CC and BrCrPlaneResolve 0x8025DCB8 attempts by 569e0e on 2026-10-07: both parked; DashOval residue = dash counter n loses $fp to store penalties; PlaneResolve = as1 hoist only

**BrPaintDashOval 0x802523CC (paintshop.c), parked 2026-10-07.** Drafts: build/tgrally/n64/search/802523CC/s569 (run.sh, log.py = instrSP).
- The ROM's walks are the while-form of the tree source (t0): uopt strength-reduces b2b*x / a2b*y into the memory temps at 0x54/0x58 and b2*x into s7 (SR inits are products with x, unlike FrameOval's explicit ex/ey = 0). The squares at 0x5c/0x60 are spill temps (0x5c is reused for the walk-2 y>>1 spill), not an address-taken struct.
- Frame: d5 = an extra int first, then b2, a2 declared before b2b, a2b, which puts b2b at 0x70 and a2b at 0x6c as in the ROM.
- Open: the ROM gives $fp to the dash counter n. Ours: n's p1 save is 4.07; f22 store-penalty units (16 of them, -10 each) cut it, so cy (10), the walk-2 parity and d (6.83) take the callee registers first, and ry takes $fp in walk 1. A diagnostic force of ry uncoloured gives 281 -> 209. Dead: register n, n++; n %= 8, n = 0 moved late, a midpoint user variable m, coordinate variables (t, dy), p = &ry, r[2] (r[1] gets promoted).

**BrCrPlaneResolve 0x8025DCB8 (collresp.c), 4 words, parked again.** The ROM's last sign arm keeps `li v0,1` at the bc1f target. Ours (natural if/else) hoists it like arms 1 and 2; the `!(c<0)` tree form gives bc1t (4). Dead line layouts: face: on its line, store on the else line, everything on one line, chain on the if line, `goto face`. ugen .s is identical for arms 1 and 3, including .loc. The decision must come from .G-only records (binasm `call-live`/unknown); decomp_workbench.binasm has parse_peepdbg for as1 peephole logs.

Related: [n64-aiscancorridor-wip-2026-10-07](n64-aiscancorridor-wip-2026-10-07.md), [n64-axlegrip-wip-2026-10-06](n64-axlegrip-wip-2026-10-06.md).

**fb7238 (2026-10-07):** as1 scheduling traces work with `cc ... -O2 -Wa,-R` (stdout, whole file, sections split on "Initial nodes:"; each node prints inst word, lineno, aftercycles, besttime and picks). In arm 1 of the natural form, as1 already holds the join's `lui at,0x3f00` (lineno of `D_8037EAA8[0] = sgn * 0.5f`) in the pre-branch node set, i.e. it schedules across the block boundary. Arm 3's join falls into `face:` (3 preds) where arms 1/2 end with `b face`. The natural arm-3 form grades 5 here (current `!(c<0)` source = 4). Not finished.

**66d9b9 (2026-10-07):** in the `-Wa,-R` trace the move is decided BEFORE scheduling. In the second pass, the arm-2 pre-branch block (lineno 654-656) already contains `li v0,1` (lineno 659) as a DAG node, and arm 3 does the same in ours. The ugen .s differs only in that arm 3 has no `.noalias $3,$sp` (arms 1/2 emit it after `la $3, D_8037EAA8`) and falls through into face. Untested next: edit the ugen .s (add `.noalias`, or `b $187` before `$187:`) and assemble it with toolchain `cc -c` to find which record stops the move.

**eebbc5 (2026-10-07 late):** with `-Wa,-R`, as1's global pass puts arm 3's else-target `li v0,1` (lineno of `sgn = 1`) into the bc1f block's node set exactly as for arms 1/2 (natural form = 5 words). An unused label in the else arm and a goto-to-join form also grade 5.
- **66d9b9 tool:** build/tgrally/ext/instrAS/as1 is a recompiled as1 with env AS_BO=1. It logs f_do_branch_opt (fills the branch delay slot from a single-instruction target block, after an f_defuse liveness check) and func_41fe24 (HB lines: target-hoist pass with a budget at 0x1003022c, which is 2 here). Run it with -v so the procedure names interleave. Get the .G/.T from `cc -c -K` (keeps the intermediates). In ours, neither pass logs a `li v0,1` (0x24020001) block for PlaneResolve, so the move must happen inside scheduling (f_schedule/f_reschedule), as fb7238's -Wa,-R trace showed. Next: instrument f_reschedule's cross-block node intake.
