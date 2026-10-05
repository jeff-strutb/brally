# Cross jump wall final return order

*Recorded 2026-09-21.*

> A logged "cross-jumping wall" often breaks by reordering which return is PHYSICALLY LAST in the source; that return becomes VC5's shared tail.

A "cross-jumping wall" (docs/VC5-IDIOMS.md, "our cl merges identical error
tails") is frequently NOT a wall. VC5 picks ONE return as the shared physical
tail block and jumps all matching returns forward to it; the others stay
inline/duplicated. **Which return it shares is decided by which return is
PHYSICALLY LAST in the source.** If the original shares return-1 (its last
block is `mov eax,1; epilogue`) but your source ends with `return 0`, VC5
mirrors every merge/duplicate decision the wrong way - it shares return-0 and
duplicates the return-1s, and the register-blind diff looks like an
irreducible tail-merge wall.

Fix: make the return the original shares be the last statement. E.g. flip
`if (cond) return 1; ...; return 0;` to `if (!cond) { ...; return 0; } return 1;`.

**Proven 2026-09-21 on 0x100553B0 BrSlotScrollStep (1453 B, the largest
untiered function).** The header had it PARKED at +3 B / 8-insn gap, "5 of 8
are the cross-jumping wall, DO NOT RE-PROBE". Flipping the final if-sense so
`return 1` is last took the first byte diff from +0x0 to +0x82 (whole prologue
+ all three early returns now byte-exact) and the register-blind insn gap from
8 to 0. Certified T3 (a8733d97): residue is just fdiv tail-merge + one
fsub/fsubr operand role + i1a9b4=1 register reuse, A5 oracle EQUIVALENT.

**Why:** the header's DEAD list tried goto/result-var/ternary/nesting but never
the final-if-sense flip. Statement order controls block layout, which controls
VC5's cross-jump choice - the same optimizer knob as [file-position-regalloc-lever](file-position-regalloc-lever.md).

**How to apply:** when a T3/T4 diff is dominated by return-epilogue
merge/duplicate mismatches, read the ORIGINAL's tail: find which return is the
last physical block (the shared one others jump to). Reorder your returns so
that same return is last. Re-measure before assuming the wall. Do NOT chase raw
byte count blindly - a per-arm divisor local cut 0x100553B0's raw diff
1072->683 but pushed register-blind rows past Gate A2's limit; the clean
register-blind residue is what T3 needs. See [walls-are-dated-verdicts](../rules/walls-are-dated-verdicts.md),
feedback-outcomes-not-excuses.
