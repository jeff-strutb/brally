# Tiny stub census

*Recorded 2026-08-31.*

> The <=16B function bucket is 3 classes, not one; 33 real ones landed in tiny_stubs.c

The ≤16B function bucket of BRGlide (747 functions on the glide map,
`config/brally/functions_glide.csv` - NOT `functions.csv`, which is D3D-keyed, 2818
rows) is THREE distinct classes. Do not treat it as one "knock out the stubs"
job - that was the initial wrong assumption.

- **~474 C++ EH catch-funclets** - shape `mov eax,[ebp+disp]; push eax; call
  rel; pop ecx; ret` with NO prologue (reads `[ebp+X]` because the EH runtime
  enters them). Packed contiguously in **0x10073000 - 0x10078000** (also
  `lea ecx,[ebp-N]; jmp dtor` unwind funclets, and a few `retf`/`iret` bytes
  that are data misread as code). These belong to the C++ EH workstream
  ([cxx-eh-frame-wall](../cpp-lane/cxx-eh-frame-wall.md)) - compiled out of the parent .cpp's try/catch, NOT
  standalone C. Region-filter them out when hunting real C.
- **~44 import thunks / ILT stubs** - `jmp [imp]` (ff25) and 5-byte `jmp rel`.
  Linker-synthesised; `image_build.py` already lays them in at 0 differing
  bytes. Reproduced at the link stage, never as .c. Fence, don't write.
- **35 genuinely-trivial C** - the only hand-writable ones. **ALL 35 landed
  byte-exact in `src/brally/core/tiny_stubs.c`** (commits 7014d36 + 49358ba,
  2026-08-31). No ≤16B hand-C outstanding.

**The fence (commit 34645d8):** `config/brally/fenced.csv` enumerates the 611
not-hand-C map entries by instruction signature - 41 import thunks + 3 jump
stubs (reproduced at link), 481 EH funclets + 80 EH dispatch + 6 EH data
(reproduced by parent TU try/catch). Conservative: real integer-math helpers
in the EH region (0x10074580+, `f7f1` div etc.) stay OUTSTANDING, not fenced.
`tools/brally/coverage.py` subtracts it: **hand-C target = 1,529 fns (glide map 2,140
− 611 fenced), 741 byte-exact (48.5%), 788 remaining** - the honest denominator,
not the 1,664-off-the-D3D-map figure. USE tools/brally/coverage.py for the real
remaining-work number.

The 33 proved a full idiom set (all /O2, verified `tools/brally/match_diff.py`
against `build/brally/win32/match/orig/0x*.bin` which ARE glide-keyed for these VAs):
byte/dword setter (`g=imm`), setter-returns-1 (`g=imm;return 1` vs
`return g=imm`), copy-then-clear (`g2=g1;return 0`), arg-store
(`return g=x`), float field getter (`p->f` → `fld [eax+0xc]`), pointer clears
(plain and `BR_THISCALL1`), thiscall tail-forward (`method(&g)` →
`mov ecx,imm;jmp`), cdecl registrar-forward (`reg((void*)fn)` →
`push imm;call;add esp,4`), C++ virtual dispatch (`o->vp->m[8];return 1` →
`mov eax,[ecx];call [eax+0x20]`) - ALL matched in plain C. Single-arg
`__fastcall` = `BR_THISCALL1` ([thiscall-via-fastcall](../cpp-lane/thiscall-via-fastcall.md)).

**2 parked one-offs**: 0x1002A932 (orig keeps an EBP frame + no tail-call =
built /Od, not /O2) and 0x1001DC80 (`fsqrt` scheduled BEFORE `add esp,4`;
`(float)sqrt(callee(x))` cleans the stack first).

Count after: DLL-C 706→739, total byte-exact 851→884 ([matching-progress](../log/matching-progress.md)).
tiny_stubs.c carries BrSub<VA> names and local extern decls (relocs are masked
at .obj level, so names/addresses are decoration) - re-home each into its real
module when its subsystem is identified.
