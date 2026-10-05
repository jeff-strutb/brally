# Register rotation is a symptom

*Recorded 2026-08-29.*

> TRIAGE RULE: a whole-function register rotation is a SYMPTOM of a structural source defect in the body, not a terminal coloring wall. Rank residue by the REGISTER-BLIND multiset gap, never by raw diff count.

**Established 2026-08-28 on 0x100250D0 BrTex3dExpand.**

A whole-function register rotation - every `cmp edi,ebx` where orig has
`cmp edi,ebp`, every load landing in a different callee-saved register - reads
like an allocator coin-flip you cannot reach from C. It usually is not. It is
usually the DOWNSTREAM EFFECT of one structural source defect in the body that
changes register pressure.

**Why:** 0x100250D0 was written off as a "coloring wall, mechanically
impossible, do not grind" TWICE, on the strength of a raw diff count. Both
verdicts were wrong. The rotation dissolved once the actual source defect was
fixed (Ghidra's counter-fold - see [brtex3dexpand-wall-broken](../functions/brtex3dexpand-wall-broken.md)), taking the
function from +1152 to +512 bytes. The register rotation had been the
headline symptom the whole time.

**How to apply:**

1. **Never rank residue by raw diff count.** Use the register-blind multiset
   gap - `tools/fnmatch/`, `regnorm` mode (see [fnmatch-harness](../toolchain/fnmatch-harness.md)). On the
   pre-fix file, raw read 1097 extra / 863 missing; register-normalised it
   read 432 / 198. Roughly 650 of that "difference" was ONE rotation. Chasing
   the raw number sends you after register allocation; chasing the regnorm
   number sends you after the source defect that CAUSED it.
2. **When a rotation is live, look for a codegen-shape defect in the body,
   not a spelling permutation in the prologue.** Ask what would change
   register PRESSURE: batched vs stepped stores, a widened temp that should be
   a byte local, a value held live across a region orig recomputes.
3. This does NOT retire [divergence-class-triage](divergence-class-triage.md) - register allocation is
   still the last thing to attack. It changes what counts as evidence that you
   are looking at one: a rotation alone is not evidence, a rotation that
   survives a register-blind gap near zero is.

Supersedes the "scattered/coloring class - document and move on" reflex for
any function whose register-blind gap is still large.
