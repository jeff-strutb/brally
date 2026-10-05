# Largest to t3 survey

*Recorded 2026-09-12.*

> SUPERSEDED - the 'measured NO, every large row walled' verdict is dead: 0x10028BB0 BrTex3dRegister and 0x100250D0 BrTex3dExpand are both CERTIFIED T3 under A5. Keep the BrTex3dRegister levers (own-statement nested call, |= over global re-read) as T4 idioms.

The project lead asked for 5 of the largest uncertified functions taken to T3 in one day.
Re-qualified the 14 largest uncertified non-EH diff rows (fresh, not the
2-day-old survey): the answer is NO, and it is measured, not assumed.

**Where the five nearest large candidates actually stand (2026-09-12):**
- 0x1005D770 BrCtlAiBody: A3=10 unpaired, everything else PASSES. The rows are
  the dossier's R2/R3 (pinned-ebx arm, un-cross-jumped rule-7 tail) - 26 dead
  probes on file; my two candidate angles were already on the dead list.
- 0x100250D0 BrTex3dExpand: A2 62 vs 60.2. Both remaining families diagnosed
  allocation in the file header (12 sessions). No live lever on record.
- 0x100645A0 BrCarPhysDriveMatch: A2 55/21.6, Gate B PASSES.  NEW DEAD
  (dossier updated, committed 69fc40a): the original selects `hold` at the
  read (`cmp byte [param_5]; je; fld K; jmp; fld slot`) and carries it in st
  to a `fcomp st(1)`; spelling that as a ternary into a reused local AND into
  a fresh single-use local both HOME the slot (+3 insns). Same axis as the
  declined stack-dup fold. Park.
- 0x10067710 BrCrRespWalk: **CERTIFIED @t3 2026-09-12 (commit 30d9dac).**
  The project lead REVERSED the 2026-09-10 decline  ("functionally equivalent
  is functionally equivalent"): the stack-dup commutative fold (`fld st; op
  [M]` == `fld [M]; op st(k)`, fmul/fadd, crossed quad, identical memory
  operand) is now IN t3.py, right after the memory-memory fold. Re-qualified
  every certified VA: zero demotions; it promotes ONLY 0x10067710. The other
  class-#2 rows (0x100311C0, 0x1005D770, 0x100645A0) have different shapes
  and did NOT move. Both ceiling-ternary polarities on 0x1005D770 are now
  dead (constant-first: A3 10 -> 14, dossier updated).
- 0x10028BB0 BrTex3dRegister: worked this session, rows 35 -> 33 (commit
  2315e33), remaining 16 unpaired are ONE wall (below).

**Two levers that PAID on 0x10028BB0** (RAW 58+63 -> 46+49, bytes -12 -> -4):
1. A nested call as its OWN STATEMENT: `t = f(0,0,0); g(t, a, b)` makes VC5
   call the inner FIRST then load the outer args (orig shape); the nested
   spelling pushes the outer args before the inner call. Both crc sites.
2. `r.f260 |= 0x80` instead of `r.f260 = GLOBAL | 0x80` when f260 was just
   assigned from GLOBAL: single-use lets the register die, so the earlier
   `f260 & 2` test compiles as orig's `test byte ptr [slot], 2` instead of a
   load-then-test. Ripples (~8 raw rows from one edit).

** THE id-SPILL WALL (0x10028BB0's remaining 16 unpaired, one decision, two
symptom families):** ours spills `id` to a stack slot where the orig keeps it
in esi. Symptom A: both FUN_100306d0 call sites reload identically, so VC5
cross-jumps the tail (EXTRA jmp; MISSING call/push/add esp/pop x4/ret - the
orig DUPLICATES the epilogue in the f278 arm). Symptom B: the freed
callee-saved hosts a pinned 0 (EXTRA cmp R,R x5 + reg stores; MISSING
test R,R x4 + `mov [slot],0` x2) - the [callee-saved-zero-web-class](../levers/callee-saved-zero-web-class.md) axis.
DEAD 2026-09-12, do not re-run: explicit `return id` inside the arm;
inverted-guard with two returns (both byte-identical - VC5 still merges);
`id` declared first and last (inert). The lever, if one exists, is whatever
un-spills id - not the zero, not the control shape.

 Tool fact: `claim_lane.py claim N` is refused, but `claim_lane.py claim
--va <VA>...` works for locking a hand-picked set (t4lane calls it that way).

Related: [gate-a-distance-survey-2026-09-10](gate-a-distance-survey-2026-09-10.md), [t3-frontier-map-2026-09-10b](t3-frontier-map-2026-09-10b.md),
[do-not-lower-t3-standard](../rules/do-not-lower-t3-standard.md), [callee-saved-zero-web-class](../levers/callee-saved-zero-web-class.md).

**2026-09-12 afternoon - two attribution questions MEASURED:**
-  VC4.2 bitstream experiment (0x10006BA0, twin committed): VC4.2-optimal
  spellings took it 274 -> 298 of 302 insns, 913/925 B, 6 regions, residue
  scheduling-only (prologue push order, three 1-2-insn transpositions, the
  sites-1-5 bool diamond). NOT byte-exact = attribution NOT yet proven to the
  61/61 standard; but under VC5 the shape is measured impossible.  EVENING:
  bound reached at 298/302, 913/925 B - the ENTIRE residue is two cl 10.20
  peepholes the original's compiler lacked (bool diamond at 5 tail writes:
  EIGHT spellings + /Op /Ox /O1 /O2y all dead, full list in the file; and the
  imm8-foldable step=0x80 guard, whose 0x1000 siblings match). The named-bool
  assignment is what puts `push 1` after the bool - keep it. NEXT = try cl
  10.00/10.10 (VC4.0/4.1): a toolchain acquisition, PROJECT'S call (rule 5).
  The vc42_probe positional score is meaningless here - read divergence.py
  insns.
-  "Are the stuck large C functions really C++?" - MEASURED NO for the front
  end: br_ctlai.c compiled whole under VC5's C1XX (extern "C" wrapper,
  build/match/t3d/ctlai_cpp.cpp) gives IDENTICAL bytes/insns (3863/1084) and
  48 regions vs C's 47. The allocation walls are not front-end artifacts.
  (EH fns and the bitstream family remain genuinely C++ and already routed.)
