# Five largest

*Recorded 2026-09-12.*

> 2026-09-12 evening: project lead demanded 5 of the largest to T4/T3 with no excuses. Result: 2 byte-exact (0x10046E70 2114 B, 0x10044860 2439 B) + the 34-diff photo-tail trio certified @t3 (0x1004AEE0 3862 B, 0x1004BE00 3475 B, 0x1004DA00 3394 B). Two tool changes: /Gi as a 4th C++ sweep shape, t3.py measures C++ rows.

**The lane that paid: C++ page builders in src/brally/core/cpp/ with 34 diffs and
3/4 pieces** -- the largest rows with real headroom were NOT the walled C
rows the earlier survey re-qualified; they were C++ EH rows invisible to
t3.py (which read only report.csv).

- 0x10046E70 BrExt_1004DFC0 **byte-exact**: the "-1 constant-register fork"
  dossier was WRONG (dropping the -1 to 0 as a diagnostic changed nothing).
  Real levers: selector vtable as a named local `vt` + pointer-to-member
  configure call + add-item slot read `add = *(AddPmf *)(vt + 0x10)` placed
  BEFORE the pfn04 store (so the vptr temp does not overlap the walker and
  shares edi); explicit `((hi - lo) * n) * k` grouping (flat product
  reassociates with two fxch).
- 0x10044860 BrPhaseEnterPlaceholder_1004B430 **byte-exact**: `fy = 0.0f`
  as the FIRST statement (before the pinned-zero store) fixes the slot
  layout (1952 -> 36); the four `fld local; fadd member` sites are the
  compiler OPTION `/Gi` -- 40 micro-TU shapes prove VC5 /O2 never loads the
  local first. /Gi is PER-TU (0x10004AD0 is exact only without it).
  cpp_score.DEFAULT_OPTS carries `/O2 /Gi /GX /MD`; report_cpp opt `O2 Gi`.
- Trio 0x1004AEE0/BE00/DA00: photo-1 tail = Pentium-pairing schedule of an
  identical multiset. 726 probes on AEE0 (perm sweeps 240+96+246, helper/
  CSE/volatile shapes, 34 options, corpus MISS). Model that fits all
  variants: copy `mov ebx,eax` at xi's FIRST USE; fld hoisted; fsub issues
  right after fld unless an IR-earlier ready instruction takes the slot;
  lea waits one cycle after the copy (AGI); stores by readiness. Needs the
  yi-reload + xi-copy IR-before the fsub WITHOUT a store -- no C++ spelling
  found. All three certified @t3 (AEE0: 3 ledger passes/726 probes; BE00 and DA00: 240-order sweep + 27-option/corpus pass each). Validator 108 ok after.

**Batch 2 (2026-09-13, "5 more of the largest"):** 0x10006510
BrCarStateEncode (1018 B) @t3 -- Gate A already passed, the ledger was the
documented three-front-end map + an option pass. 0x10069A80 BrGhostLoad
(828 B) @t3 -- two real levers: fail-path float globals zeroed as `= 0.0f`
(int zero joined a zero web with the return byte load), and `fclose(fp);
goto fail;` INLINE at all nine sites (VC5 cross-jumps them and hoists the
next fread's push). 0x10023D70 (1377 B) T2 progress: per-use `pReq[0x28c/4]`
re-reads, `<= 0`, `<< 1` (38+35 -> 32+29); residue = zero/local register
swap (dead class) + one duplicated exit + pSlot zero stores. NO LEVER (dead
lists in the dossiers): 0x100553B0 exit duplication survives goto/result
variable/all-returns-rewritten; 0x1005BCC0 the 1.0f int web absorbs every
float-typed store; 0x100302A0 halfword compose order (6 spellings);
0x100311C0 flag-OR RMW (4 spellings). Two tiny C++ rows pass Gate A
untouched: 0x1006D0B0 (164 B), 0x1006FCE0 (104 B).

**Tooling:** t3.py `report_rows` now overrides with report_cpp.csv rows
(cpp=True), `_find_obj` follows `obj_cpp/<base>_sweep_<VA8>_<i>.obj`, the
oracle gets the mangled `--name` (so it never tests the stale C twin), and
`canon` pairs the EH frame's `fs:[A]` with `fs:[M+0]`. Validator: 105 ok.

**Method that worked:** a 0.6 s C++ probe (`cprobe.py`: compile_cpp +
match_sweep.score + side-by-side window) and generated permutation sweeps
in the background; micro-TUs (10 functions in one compile) to learn a
canonicalisation rule in one shot; diagnostics that CHANGE semantics
(drop an argument) to kill a theory before spelling anything.

Related: [largest-to-t3-survey-2026-09-12](largest-to-t3-survey-2026-09-12.md), [com-vtable-levers-2026-09-10](../cpp-lane/com-vtable-levers-2026-09-10.md),
[do-not-lower-t3-standard](../rules/do-not-lower-t3-standard.md), [corpus-query-tool](../corpus/corpus-query-tool.md).
