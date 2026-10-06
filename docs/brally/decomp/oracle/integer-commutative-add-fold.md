# Integer commutative add fold

*Recorded 2026-09-21.*

> PROVEN exact-identity fold for t3.py A3 (integer commutative add a+b==b+a), zero demotions across 125 tags, but promotes nothing and does not touch A2 -- built and REVERTED 2026-09-15; land only in a deliberate migration

**The finding (2026-09-15, five-largest session, [resume-state](../log/resume-state.md)):** t3.py's
A3 classifier has x87 commutative folds (fadd/fmul, and the stack-dup fork) but
NO integer commutative-add fold. `mov D, [a]; add D, [b]` computes a+b and
`mov D, [b]; add D, [a]` computes b+a -- **bit-identical result, identical
flags** (integer add carry/overflow are symmetric in the operands), same two
memory reads, NO rounding. It is a *cleaner* exact identity than the x87 folds
already accepted (those carry a "one rounding each" guard; integer add needs
none). Squarely on the ALLOWED side of [do-not-lower-t3-standard](../rules/do-not-lower-t3-standard.md)
(an exact identity with symmetric evidence, not an approximation).

**The guarded fold (mirrors the x87 quad-cancel fork, inserted after it in
classify):** for each side pair (um,ue) and (ue,um), find `mov D,[a]` and
`add D,[b]` on one side with a!=b and the crossed `mov D,[b]`,`add D,[a]` on
the other; cancel the quad. Same destination register token on load and add;
both operands memory. A reloc'd/global operand normalises to `[A]` (canon's
mem drops its displacement), so the **a!=b guard already refuses the
indistinguishable case** -- the very reason the global-CSE class was rightly
declined.

**Measured:** takes 0x1000E320 BrSceneVisPrepare A3 from 16 unpaired -> 0
(region 2, the view-rect edge sums x+w and y+h). `tools/brally/t3.py` (no args)
revalidates **all 125 certified tags UNCHANGED -- zero demotions, zero
staleness**. `--qualify --all` READY count 83->84, and all 84 are already
@t3-tagged, so it **promotes nothing <=400 B**. It does not touch A2.

**Why it was NOT landed:** (1) the session plan said "never edit t3.py"; (2) it
certifies nothing today (0x1000E320 still fails A2 raw distance 20>13.6, and A2
relief was declined); (3) a classifier change that changes no outcome is
scope-creep. REVERTED (`git checkout tools/brally/t3.py`). The finding stands:
0x1000E320 region 2 IS a proven exact identity.

**If ever landed:** do it in a deliberate migration (like the deferred
"bare-decimal addend masking v2" stash), re-run `tools/brally/t3.py` for zero
demotions, and note it certifies nothing on its own -- 0x1000E320 additionally
needs region-1 un-spill and A2 relief. Related: [five-largest-2026-09-13b](../log/five-largest-2026-09-13b.md),
[x87-wall-mechanism-2026-09-13](../levers/x87-wall-mechanism-2026-09-13.md), [do-not-lower-t3-standard](../rules/do-not-lower-t3-standard.md).

**2026-09-20 re-triage (fresh session, claimed+released the VA):** the region-2
sums are STILL not source-controllable -- flipping the source operand order
`.w + .x` -> `.x + .w` and `.y + .h` -> `.h + .y` (variant v5) changed NOTHING
(VC5 canonicalises commutative int adds regardless of source order; the
first-loaded operand is register-context-driven, not textual). Confirmed the
memory: T4 is a genuine compiler wall. The ONLY certification path is A5
EQUIVALENT, and `tools/brally/oracle_profiles.py` has NO profile for 0x1000E320 -> A5
returns UNCLASSIFIED, so it cannot certify today. Building one is a big job
(~15 callees: qsort, span queue, car ranking, light alloc, the FUN_1000c9e0
clamp helper) -- seed only the driver clamp loop (g_brRaceNDriver=0x100b2f00
gates it) with NON-degenerate pDrv/pView fields and stub the rest, or a zero
world gives a false EQUIVALENT. Left as a dedicated-oracle-session target, not
a hand-transcription one. Gate 0 also needs WHAT IT DOES moved within 40 lines
of the @implements tag (currently ~54 lines above).
