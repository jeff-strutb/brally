# Com vtable levers

*Recorded 2026-09-10.*

> Two reusable levers that took BrFfbSetup (a parked +45-byte 'structural wall') to size-exact 80/80: COM vtable function pointers must be BR_STDCALL (callee-cleaned, kills a per-call `add esp,K`), and the vtable-cast helpers (BrDiRoot/BrDiDev/BrDiEff) must be __inline or VC5 emits an out-of-line call per use. Plus the correction: 'these are hard/park' was wrong.

**2026-09-10: BrFfbSetup 0x10079390 was PARKED as a +45-byte "structural,
COM-heavy, hard" wall. It was not a wall. Four ordinary source facts took it
to SIZE-EXACT (80/80 insns, 440/440 B, REGNORM 6+6), committed c19f37c.** The
project lead was right to push back on "the pool is dry / the rest are harder" -- that
reflex is the thing to distrust. ([byte-exact-non-negotiable](../rules/byte-exact-non-negotiable.md))

**Two of the four levers are REUSABLE across the whole COM/DirectInput surface
(slice3_45.c still has 3 more BrFfb diffs; both levers moved them too):**

1. **COM vtable function pointers must be `BR_STDCALL`.** A vtable declared
   `long (*pfn)(BrDiObj*, ...)` is __cdecl -> the CALLER cleans -> ours emits
   `add esp,K` after every interface call; the original has none because COM
   methods are __stdcall (callee-cleaned). Fix: `long (BR_STDCALL *pfn)(...)`
   on every typed slot of every interface vtable. In src/brally/include/slice3_45.h this
   was the single biggest lever (removed 2 `add esp,0x14`, cascaded the pushes
   into place). BR_STDCALL is `__stdcall` in the matching build and empty in
   the port (br_match.h), so it is safe both ways.

2. **A vtable-cast helper must be `static __inline`.** `BrDiRoot`/`BrDiDev`/
   `BrDiEff` are one-line `(cast)p->pVtbl` returns. Plain `static` -> VC5 5.0
   emits an OUT-OF-LINE call per use (`call <helper>; ... add esp,4`), where
   the original inlines the cast to `mov ecx,[eax]; call [ecx+0x48]`. Mark
   them `static __inline`. slice2_12.c already carried the note "must be
   __inline here or VC5 emits a call instead" -- same class.

The other two levers were function-local: the DirectInput axis buffer is a
STACK local in the matching build (a global frees a register and adds `push
ebp` -- gate the storage with `#ifdef BR_MATCHING_BUILD` since it dangles after
return, [port-safety-additions-block-matches](../triage/port-safety-additions-block-matches.md)); and `g_brFfb.pDevice` is
re-read at each call site, not hoisted into a local (hoisting pins it in a
callee-saved reg).

**The residue (6 REGNORM) is NOT codegen -- it is 6 UNMAPPED d3d globals.**
fn.py cannot substitute an unmapped global's address, so it reads the reloc
slot as an immediate ("mov [A],I" vs the original's "mov [A],A"). coff_relocs.py
proves every slot is a real relocation. This is a BOOTSTRAP: reloc_learn only
learns from a masked-MATCH function, but the function can't reach masked-match
until the globals are mapped. Break it by matching a smaller sibling that
shares the globals, or hand-map from the original disasm (addresses are in the
BrFfbSetup header comment). d3d globals are learned per-function, so a lone
diff function using private globals stays 6+6 until something seeds them.

Method that worked, in order: fn.py register-blind (not raw diff count) ->
sbs.py to read the frame/reg divergence -> fix the biggest structural lever
first (frame, then convention, then inlining) -> coff_relocs.py to prove the
residue is relocations, not immediates. ~2 dozen measure/edit cycles, size gap
+45B -> +0B.

Related: [pool-refresh-method-2026-09-10](../triage/pool-refresh-method-2026-09-10.md) (its "the rest are harder" framing
is what this corrects), [thiscall-via-fastcall](thiscall-via-fastcall.md), [port-safety-additions-block-matches](../triage/port-safety-additions-block-matches.md).
