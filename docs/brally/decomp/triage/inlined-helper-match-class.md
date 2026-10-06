# Inlined helper match class

*Recorded 2026-08-22.*

> The reliably matchable divergence class: the port factored a helper the original hand-inlined. Plus the ++c codegen lever and the walls that are NOT worth attacking.

**Measured 2026-08-21, `c78f090`..`361d22a`. Five functions matched this way.**

## The class that pays: "we call, they inline"

The original rarely factors. Where the port introduced a shared helper, the
original hand-inlined the body at every site with that site's constants folded
in. The tell in `report.csv` is **recomp_size much SMALLER than orig_size**  - 
a 16-byte thunk against a 77-byte original. Query it directly:

```python
ratio = recomp_size / orig_size          # < 0.65 and orig < 260
```

These come in FAMILIES (the same template at 3+ addresses), so one crack pays
repeatedly. Landed this way: `BrVec3Dist`, `BrVec3Length` (both: original
tail-calls the fsqrt wrapper `BrSqrtF` at 0x10002250, never an inline fsqrt or
a call to the neighbouring DistSq), and the three frame-bank allocators
`BrPool16Alloc` / `BrPool32Alloc` / `BrSub_10069490`, which are one template
with (usable, per-frame, slot-size) of (20,21,16), (20,21,32), (256,257,64).

**Keep the port's factored version behind `#else`.** Guarding with
`BR_MATCHING_BUILD` is established house style, and it is what lets the
matching body reach absolute globals and game-internal leaves the port cannot
link. See [port-safety-additions-block-matches](port-safety-additions-block-matches.md) for the inverse problem.

## The `= ++c` lever

`BrG_count = ++c;` and `BrG_count = c + 1;` compute the same number, but
`c + 1` lets VC5 form the value early (`lea eax,[ecx+1]` plus a store BEFORE
the address arithmetic), which costs a byte and changes the register chosen for
the index LEA. The pre-increment keeps the count live across the address
computation and emits the original's trailing `inc ecx / mov [count],ecx`.
That single token was 78 bytes vs an exact 77.

## Iterate in a scratch .obj, not by rebuilding the file

Ten candidate spellings in one file, compiled once (~20s), beats ten 12-second
file rebuilds - and it reads the answer straight out of the bytes:

```
cp variants.c build/brally/win32/match/ && sh tools/toolchains/wine.sh tools/toolchains/msvc5/bin/cl.exe \
   /nologo /O2 /c '/Fobuild\match\obj\v.obj' 'build\match\v.c'
```
then `parse_coff_obj` from `tools/brally/match_diff.py` (returns `{name: (bytes,relocset)}`).
cl.exe treats a leading `/` as an option, so the source path must be RELATIVE.
Mask relocated operands before comparing. **Reproduce the real calling
convention in the scratch** - a cdecl stand-in for a `__fastcall` original gave
a false positive that did not survive transfer.

## Walls - do not spend attempts here

- **C++ EH: 42 of 576 diff rows.** `push -1` + scopetable + `fs:[0]`. Detect in
  bulk: `d[:2]==b'\x6a\xff' and b'\x64\xa1\x00\x00\x00\x00' in d[:20]`. Every
  201-byte `BrPhaseActivate_*` / `BrOptOpen*` is this. See [cxx-eh-frame-wall](../cpp-lane/cxx-eh-frame-wall.md).
- **VC5 DOES canonicalise commutative fmul/fadd operands.** The note in
  br_vec.c is correct and I was wrong to doubt it: swapping operands, `*=` vs
  explicit product, and hoisting stores all produced byte-identical output
  across 8 spellings. Register-allocation diffs in Dot/Scale/MulAdd/ScaleBy/
  DivBy are NOT source-controllable.
- **`BrEntitySetIndex` 0x10076AE0** is an encoding wall, now confirmed against
  **17 spellings**: original `sub eax,0x10`, VC5 always `add eax,-0x10`. The
  hoisted-store shape yields `sub` only under cdecl, where an extra `mov ecx`
  separates them; under the real fastcall it never does. Stop.
- **The globals-struct indirection.** slice2_23.c's UI functions take
  `BrUiGlobals *pG` and index through it; the original reads those same globals
  ABSOLUTELY (`a1 <addr>`, `mov ecx,[eax*4+<table>]`). Marking the helper chain
  `static __inline` DOES inline it (32 → 144 bytes), so that part is solvable  - 
  the remaining blocker is the parameter, which is baked into the file's
  signatures. Likely explains most of that file's 28 diffs.

Related: [divergence-class-triage](divergence-class-triage.md), [matching-progress](../log/matching-progress.md), [file-as-you-match](../rules/file-as-you-match.md).
