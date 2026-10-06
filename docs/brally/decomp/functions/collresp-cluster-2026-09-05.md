# Collresp cluster

*Recorded 2026-09-05.*

> T1 intake by CLUSTER on br_collresp.c -- 3 byte-exact (0x10066260/0x10066800/0x10066AD0), 2 parked (0x10066610/0x10066950); a committed match regressed when its neighbours were re-spelled

2026-09-05 session on `src/brally/core/driving/br_collresp.c` (the OBB collision TU):
- **Byte-exact:** 0x10066800 BrCollRespSegBox (332 B, 6faab8c), 0x10066260
  BrCollRespBoxClassify (644 B, b34199a), 0x10066AD0 BrCollRespBroadPhase
  (669 B, bb2352c). Image gate 0 diff bytes after b34199a; re-run after bb2352c.
- **Parked with dead-probe lists in the file:** 0x10066610 PointInTri (482/492 B,
  2 regions = the two B-row `fcompp` compares, ~30 spellings dead) and
  0x10066950 BrCrExact (326/322 B, one x87 chain + one spill, ~25 dead).

**Why:** the playbook's cluster rule worked exactly as advertised -- all five
functions already had port bodies and every declaration in one live TU; three
fell in one session. The T1 list showed them as untouched.

**How to apply:**
-  **A committed match can regress with NO edit to its text** when a neighbour
  in the same TU is re-spelled (SegBox went match->diff via the symbol-index
  tie-break; fixed by declaring `int i, j, k` BEFORE its arrays). After any
  function edit, re-sweep the file and read EVERY row -- the `n/m` total hid
  a match->diff + diff->match swap. Full entry in docs/brally/VC5-IDIOMS.md
  ("A COMMITTED match can regress...").
- Levers that paid here (all in VC5-IDIOMS.md now): accumulated local
  `t = a; t += b` beats the paren for a commutative flip; a static helper is
  never inlined -- write it out; `? 0 : -1` for the neg/sbb/neg/dec return;
  `? 0 : 1` ternary vs boolean chooses the 0-arm-first layout; an unnamed
  twice-written half-sum + one named `C`; a pointer stepped in the for
  clause vs recomputed from an index (+0x14 bias, lea per push).
- Related: [resume-state](../log/resume-state.md), [declaration-order-tiebreak](../levers/declaration-order-tiebreak.md), [parked-is-not-walled](../triage/parked-is-not-walled.md).
