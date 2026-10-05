# Feedback t3 certified standard

*Recorded 2026-09-16.*

> RULE: T3 = FUNCTIONALLY EXACT to the original game -- same inputs produce same outputs (behavioral equivalence). NOT a byte-diff verdict. tools/t3.py --qualify formalizes it (Gate 0 completeness, Gate A residue is compiler-choice, Gate B @t4-pass ledger), but the STANDARD ITSELF is same-in/same-out.

** THE STANDARD, STATED BY THE PROJECT (do not re-derive, do not fog it up):
T3 = FUNCTIONALLY EXACT TO THE ORIGINAL GAME. Same inputs produce the same
outputs. That is the whole test -- objective, behavioral, clear.**

- Byte-level residue is IRRELEVANT to T3. Register allocation (idx in eax vs
  ecx), instruction scheduling, which callee-saved reg holds the zero, basic-
  block layout (a block before vs after the `ret`), a "lost-sync" gap in the
  register-blind byte differ -- EVERY one of those is a T4 (byte-exact) concern
  and is BEHAVIOR-NEUTRAL. None of it changes same-in/same-out. Never judge T3
  with cpp_score diffs, divergence.py region counts, or lost-sync. Those measure
  T4 only.
- The ONLY thing that disqualifies T3 is a genuine LOGIC/semantic error: a
  flipped branch, wrong struct offset, dropped side effect, miscomputed stride,
  a guessed/unknown value. That is what "functionally exact" hinges on.
- Therefore there is NO "unfinished T4 that isn't T3." If the residue is purely
  compiler choices (registers/scheduling/layout), the behavior already matches
  -> it IS T3. The only non-T3 state is genuine INCOMPLETENESS (a byte/branch you
  don't understand), which is not "unfinished T4", it is just unfinished/below-T3.
- To TEST T3, prove functional equivalence: audit the transcription against the
  disassembly for semantic divergence, and run the A5 in-image equivalence oracle
  ([equivalence-oracle-in-image-2026-09-10](../oracle/equivalence-oracle-in-image-2026-09-10.md) -- "the ONLY gate that tests what
  T3 CLAIMS"). NOT the byte grind.
-  RECURRING FAILURE (2026-09-15, this had to be corrected by hand): staring
  at register-blind divergence / cpp_score and treating byte residue or a lost-
  sync tail as if it threatened T3, then inventing a fake "complete-but-not-T3"
  limbo to avoid committing. If you catch yourself measuring T3 with a byte
  differ, STOP -- wrong tool, wrong tier.

Gate B (the @t4-pass ledger) governs when you may STOP grinding T4 and certify;
it does not create a "complete-but-not-T3" state. See [do-not-lower-t3-standard](do-not-lower-t3-standard.md).

---

**RULE (2026-09-06):** two grades of done, T3 and T4, nothing between
(T3a/T3b retired). The project lead insisted on two things explicitly:
1. **T3 must be functionally done with no missing understanding or
   functionality** -- it is not "close enough", it is complete; only the
   compiler's own choices differ. Gate 0 checks it (WHAT IT DOES present,
   no TODO/FIXME/XXX/HACK/STUB/???/guess/placeholder/unknown/#if 0 in the
   body).
2. **A gap threshold alone is not enough** -- otherwise we cross it and give
   up. Gate B is a mechanical ledger of sincere T4 attempts (`@t4-pass` lines
   in the file header with end-of-pass numbers): passes under 10 fresh
   compiles do not count; >= 3 counted passes; the last three counted records
   identical to the current measurement (two consecutive zero-movement
   passes); at least one census-driven. Crossing Gate A is a PRECONDITION,
   never the trigger to stop.

**Why:** 31 passes on 0x1000EAF0 bought no bytes while hundreds of T1/T2
rows waited -- but parking must not become an excuse.

**How to apply:** never certify by judgment; run `tools/t3.py --qualify`,
which refuses to emit a tag until gates 0, A and B all pass. End EVERY pass
at a near-exact function by appending its `@t4-pass` line. Once Gate A
passes, cap passes (40 min / 20 probes, grep the dead list first); when the
ledger meets Gate B, certification is mandatory. Never open a T3 function
unless the project lead names it. Remove `@t3` when byte-exact (tool flags STALE).
0x1000EAF0: gates 0+A pass, ledger has ONE zero-movement pass -> needs one
more (project rule 11b).
Related: [byte-exact-non-negotiable](byte-exact-non-negotiable.md), no-token-thrashing,
scenedl-0x1000eaf0-state, feedback-work-by hand-one-function.
