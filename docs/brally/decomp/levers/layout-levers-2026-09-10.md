# Layout levers

*Recorded 2026-09-10.*

> 2026-09-10 T3 lane: 3 T3 certifications + 1 byte-exact (a 4th was parked rather than certify on a widened classifier), all from two new lever families -- BLOCK LAYOUT (a cold arm goes out of line only when it skips the join's own assignment; two arms share one store via goto; write the success arm first) and LOAD-ONCE (read a global into a local and write the local back; a zero carried in from an earlier block does not fold).

**Certified 2026-09-10 on the UNCHANGED gate** (ledgers via `crank.py <VA>
--budget 30` run twice): 0x100183B0 BrFadeDrawBars 803 B; 0x10028200
FUN_10028200 441 B; 0x1000DF00 BrPolyClipPlane 347 B.  **Byte-exact:**
0x1003C600 BrOptCycleBD3E0 208 B.

 0x10067710 BrCrRespWalk 1,301 B was certified and then PARKED AND
REVERTED, together with the t3.py class it rested on: every other gate
passes (A1 gap 1, A2 3+4 vs 9.4, A4 clean, Gate B's two zero-movement
passes) and the 4 remaining rows are ONE x87 operand-hand fork -- orig
`fld st; fmul [nx]`, ours `fld [nx]; fmul st(2)`, the stack-dup case of
the commutative fold t3.py already applies to two MEMORY operands.
Thirteen source spellings are dead (dossier).  Certifying it needs the
project lead's decision on that fold, not another grind.  [do-not-lower-t3-standard](../rules/do-not-lower-t3-standard.md)

** THE TWO LEVER FAMILIES.  Both are in docs/VC5-IDIOMS.md's tail.**

1. **BLOCK LAYOUT.**  A cold arm sits out of line past the epilogue ONLY
   when it sets the join's value itself and `goto`s PAST the join's own
   assignment of it.  Textual placement reaches none of it -- trailing
   label, mid-function label, negated-then, ||-chain all compile to the
   same inline bytes.  Two arms writing one global share ONE store when
   the first `goto`s into the second's.  Write the SUCCESS arm first when
   the original falls through into it (`if (ok) store; else goto fail;`).
2. **LOAD-ONCE.**  `n = g; if (n) { g = n - 1; }` gives `cmp reg,reg`
   against a kept zero; `if (g) g -= 1;` gives `cmp [g],reg` plus a
   reload.  A guard test reads off the RECORD POINTER, not a second
   base+index expression (the second one CSEs base+index into its own
   register).  A zero that must not fold is a local carried in from an
   EARLIER block.  Splitting the do-nothing exit into its own `else if`
   arm stops a shared load being hoisted above the first `je`.

** TOOLING BUILT THIS SESSION (rebuild it, it is what made the day):**
`scratch/gscore.py <VA> [tag] [-v]` monkeypatches `t3._find_obj` to read
`build/match/obj_fn_<tag>/fn_<VA>_<tag>.obj` and prints t3.py's REAL gate
line (rows, limit, unpaired, gap, lost, regions, PASS/FAIL list); the
harness applies each probe to a pristine `fn.py --make` copy and scores it
with gscore.   The variant object is `obj_fn_<tag>/fn_<VA>_<tag>.obj`, NOT
`obj_<tag>/<file>.obj` -- getting that wrong silently scores the TREE and
every probe reads identical.

** A2 IS THE BINDING GATE ON BIG ROWS**, not A3: rows must be <= 2.5% of the
original's instruction count, so classification alone never certifies a
large function.  Ranking script: measure every diff row, sort by
`rows - max(4, 0.025*oi)`.

Walls proven and not to be respelled: the folded byte OR (0x100311C0,
fourteen spellings), the base+index CSE in 0x100302A0's leaf cursor, the
thiscall struct-wrapper's constant argument (0x10037FA0 -- C++ lane), the
0x3F/0x1F literal pooling (0x10034010, 0x1003C080).

Related: [t3-lane-largest-2026-09-09](../log/t3-lane-largest-2026-09-09.md), [t3-certified-standard](../rules/t3-certified-standard.md),
[resume-state](../log/resume-state.md), [vc5-idiom-dictionary](../corpus/vc5-idiom-dictionary.md).
