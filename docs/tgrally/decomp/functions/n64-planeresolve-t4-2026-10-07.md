# N64 planeresolve t4

*Recorded 2026-10-07.*

> BrCrPlaneResolve 0x8025DCB8 T4 05bf7aec (2d7add): as1 xbb cross-block hoist mechanics; uopt .alias pseudo = extra as1 block; if/else-if/else + goto three

BrCrPlaneResolve (collresp.c) T4 05bf7aec, gate 732/0, 2026-10-07 by 2d7add. The 4-word residue (arm 3's `li v0,1` hoisted above bc1f in ours, left at the target in the ROM) that ~8 sessions parked as an "as1 global-scheduling" wall.

**as1 mechanism (read from build/tgrally/ext/instr2D/as1.c, a traced copy of the recomp as1; env AS_XB=1 prints XN block list, XS/XW/XI/XC xbb trials, XG; AS_NOXBB=1 disables xbb as a diagnostic):**
- The hoist is f_do_xbb_opt -> func_42aa0c: moves one instruction from a dominated block s3 up into s4. Gates: s4 must have idle cycles (+76 = stall count from the pass-1 schedule), s4 dominates s3, s3 sorted AFTER s4 by "level" (+72, computed in func_429534 from preds with lower id; ~3 per control-dependence step), level diff < 6, <=10 candidates per s4 (100 in loops), then accept only if s4+s3 cycle total strictly drops.
- Block ids = reverse postorder of a DFS that takes the fallthrough first.
- Any alias/noalias pseudo (binasm 0x30/0x31) between a `b` and the next label makes as1 form an extra block; that dropped arm 3's else-block below its branch block in level order, so the hoist was never tried. A `.loc` record there does NOT. Consecutive labels merge (no block).
- `.alias $r,$sp` / `.noalias` come from uopt (ugen just emits the ucode, f_eval L42d6b8). uopt closes a register's noalias range at a block where the web is not already open; a block that starts by reloading the same address (`la $3`) keeps the range open, so no `.alias`.

**Source fix:** the face pick as an if / else-if / else chain, arm 1's failing inner test `else { goto three; }`, arm 3 the final else opened by `three:`; no `face:` label. Same CFG, but uopt emits `.alias $3,$sp` before arm 3.

**How to apply:** for any as1-only residue (cross-block hoist present/absent), trace xbb with instr2D first; test hypotheses by injecting records into the ugen .G (build/tgrally/n64/search/8025DCB8/s2d7/ginj.py) and assembling with tools/toolchains/ido53/as1 (diagnostic only), then find the C that makes uopt/ugen emit them. Related: [n64-dashoval-planeresolve-2026-10-07](n64-dashoval-planeresolve-2026-10-07.md), [feedback-hand-transcription-n64-m2](../rules/hand-transcription-n64-m2.md).
