# Lapsave source truth levers

*Recorded 2026-09-21.*

> BrLapSaveRestore 0x1005F6C0 went 2076 B/47 rows -> 2103/2104 B in one session by un-Ghidra-ing the source; six reusable levers incl. the fix for an 'x87 scheduling wall'.

**0x1005F6C0 BrLapSaveRestore (2104 B): certified-T3 'three walls' were ALL source-shape, not allocation. 4ffbf4d0, C++ lane, 1 byte residue.** Re-triage other T3 parks whose residue note says "x87 scheduling" or "cmp-fold allocation" ([walls-are-dated-verdicts](../rules/walls-are-dated-verdicts.md)).

Reusable levers, each proven by the byte diff closing:
- ** "x87 scheduling wall" (fxch st(2), fild hoisted before fsub) = a NAMED int temp for the converted operand.** `cc = a - b; d = ... (float)cc * len` is wrong; write `(float)(a - b)` inline. Contradicts "respelling is dead" in [x87-wall-mechanism-2026-09-13](x87-wall-mechanism-2026-09-13.md) for this sub-class -- try it first.
- **Ghidra pointer walks are strength reduction.** `sub r,4` next to `dec n`, `lea [esp+n*4+K]` preheaders, IVs homed in stack slots => source is INDEXED (`saved[--n]`, `rank[i]`, `this->slot[i]`). Rewriting with indices also fixes the IV stack-slot order.
- **Strict load/store interleave on a field copy = inlined helper with pointer params** (`V3Copy(dst, src)`); direct member copies get loads hoisted. Float-typed vs int-typed copies also schedule differently.
- **`xor r,r; mov rl,[m]` x3 then byte stores = int temps (inlined setter args, read right-to-left).** In the C lane this rotated the whole allocation (zero reg lost); in the final C++ shape it did not -- a rotation can be a symptom of OTHER wrong structure, retry late.
- **fastcall-shim dummy `xor edx,edx` for a 2-arg thiscall cannot be removed in C** -> C++ lane with a declared-only class; mangled member callees need rows in `config/globals_hand.csv` or A5 says UNCLASSIFIED; t3b_verify CLI needs `--name Class::Method` (plain name silently grades the STALE C obj in obj_O2).
- Positive `&&` chains with a store per arm + a call per arm (compiler cross-jumps the calls, duplicates the pushes); goto/shared-tail spellings from earlier probers were the regression (git history had a closer version: check `git log -- <file>` for a better old shape).

Method that worked: normalized instruction-STREAM diff (sbs.py -> sed -> diff), not just the multiset; fix earliest structural fork, one lever per probe.

Dead for the last byte (`mov [slot],edx` vs zero-reg at the group-counter init): for forms, store at loop top, chained assign, goto entry, unsigned, volatile, address-taken, single spilled variable.

Also: never `git stash` in this shared tree ([parallel-session-clobber](../traps/parallel-session-clobber.md)). Header edit by a parallel session (slice2_21.h, 67f9d29f) regressed 3 T4 fns in br_vec.c via symbol-index tie-break ([declaration-order-tiebreak](declaration-order-tiebreak.md)) -- image gate FAILED for that reason on 2026-09-21.
