# Generator compounding reality

*Recorded 2026-08-28.*

> ROI CORRECTION: --refine generators compound ONLY when a residue class is one genuinely-repeated missing-code shape. The long/short/frame/callconv classes were phantom-split / coloring-polluted / mixed-cause and swept ~0. Verify a class is homogeneous BEFORE minting.

## Generators compound only for homogeneous missing-code classes (2026-08-27)

The cadence (batch → residue classes → mint a generator → re-batch) is real, but
a residue "class" is NOT automatically a sweepable class. This session minted
generators for the `short`, `long`, and `frame` residue classes and re-ran the
wide `--refine` batch. **Result: ~0 new matches from the batches.** The wins all
came from hand-solving the ONE representative; the "class" did not sweep.

**Why:** these residue buckets were not homogeneous.
- **`long`** was a *map-split artifact* - Ghidra cut one C function's if/else
  join into a phantom second "function" (e.g. 0x10035533 is the tail of
  0x10035400, not standalone). The row count was inflated; there was nothing to
  generalize. 51 fns → 1 real match.
- **`short`** / **`frame`** were mixed-cause (prologue-0, type-refine,
  coloring, map-split) and coloring-polluted. `frame@0xe/68` was a *classifier
  artifact* (bytes 0..0xd already matched; 0xe was a near-`je` displacement).
  35/52 fns → the representative only.
- **`callconv`** (~223-fn call-shape class, `gen_callconv` seed folded
  `edb4172`) is LATENT: 0-fire across two waves. Big pool, no yield yet.

**Contrast - what DID compound:** `stringops` and `charret` earlier. Those were
one genuinely-repeated missing-code shape across distinct functions, so one
transform swept ~20+.

**How to apply - BEFORE minting a generator for a residue class:**
1. Audit that the class is one repeated *missing-code* shape across DISTINCT
   functions - not map-split phantoms (check each residue VA against the
   boundaries of functions already in report.csv), not register-coloring noise,
   not mixed causes.
2. Hand-solve ONE representative first. Only fold a transform if the fix is a
   repeatable codegen shape. If it's coloring or a phantom, STOP and document  - 
   do not re-batch.
3. Rank residue classes by *homogeneity*, not by row count or byte total. A
   6 KB "class" that is 90% phantoms is worth ~1 hand-solve, not a sweep.

**ROI portfolio that actually held:** hand-solved one-offs (SetVideo EXE, C++
near-solves, individual DLL fns) paid; generator re-batches did not. Diversified
hand-solves in disjoint subsystems out-earned the generator bets this session.

Related: [divergence-class-triage](divergence-class-triage.md), [inlined-helper-match-class](inlined-helper-match-class.md),
[ghidra-pipeline](../toolchain/ghidra-pipeline.md), brtex3dexpand-coloring-wall.
