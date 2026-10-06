# Macro per site spellability

*Recorded 2026-09-04.*

> A hand-inlining macro whose parameter is used at several sites forces one spelling on all of them; that constraint reads exactly like a scheduler choice and produced a wrong "not source-reachable" verdict.

Proven 2026-09-03 on the seven clip planes (`src/brally/core/drawing/br_dlclip.c`),
worth +1 byte-exact (0x1001F7B0) and 4 -> 2 diff bytes on 0x1001F2B0.

`BR_CLIP_PLANE(NAME, DIST)` used `DIST` at TWO sites (`dCur` and `dPrev`).
The original's two sites disagree with each other about x87 operand order.
One macro parameter yields one spelling, so no argument satisfied both. The
previous session drew the natural conclusion and wrote it into the file
header as settled:

> "Our two sites disagree from a SINGLE macro expansion, therefore the choice
> is made per site by the scheduler, therefore it is not source-reachable."

**Invalid.** The premise is a property of OUR macro, not of the compiler.
Splitting to `BR_CLIP_PLANE(NAME, DIST_CUR, DIST_PREV)` is byte-identical for
every instantiation passing the same argument twice, so the split costs one
sweep to prove and cannot regress siblings. NEAR went byte-exact on the first
sweep after it.

**Why:** the trap generalises past macros. Any time a residue argument runs
"our single X cannot produce the original's two different Ys, so Y is not
source-selectable", check whether the singleness of X is the source's
constraint or an artefact of how WE factored the code. It will look sound
every time.

**How to apply:** before calling anything inside a hand-inlined body
unreachable, count each macro parameter's use sites; if any residue lands
inside a multi-use expansion, split the parameter first and re-measure. Only
then is a per-site "the scheduler chose it" verdict worth recording.

Corollary: the paren lever (`((a) + b) + c` picks the `fld` operand) has a
SITE-DEPENDENT cost - on this body it flips the pair at either site but at
the `dPrev` site also sinks that site's `fadd` past four instructions. A
lever with a site-dependent cost is only usable once the sites are separable.

Full write-up and dead-probe list: `docs/brally/VC5-IDIOMS.md`, "A hand-inlining
macro must be spellable PER USE SITE"; residue map in the br_dlclip.c header.
Screened 2026-09-03: `BR_CLIP_PLANE` is the ONLY function-generating macro in
the tree, so there are no siblings to sweep - do not re-run that screen.

See also [sibling-byte-diff-screen](../triage/sibling-byte-diff-screen.md), [parked-is-not-walled](../triage/parked-is-not-walled.md),
[vc5-idiom-dictionary](../corpus/vc5-idiom-dictionary.md).
