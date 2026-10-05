# T3 cpp rescreen

*Recorded 2026-09-15.*

> All 126 T3 re-screened for C++ mis-classification - none mis-laned; cpp_screen flag != mislane

2026-09-15: re-screened all 126 @t3 functions with `tools/cpp_screen.py`.
Verdict: NONE are C-lane functions that should have been C++-lane. Do not
repeat this screen expecting a lode.

- 13 strong flags. 5 are genuine virtual-dispatch giants (`eax-vcall
  vtbl-cache`, 3.4-4.0KB: 0x100498A0 0x1004AEE0 0x1004BE00 0x1004CBA0
  0x1004DA00) - already have C++ twins in src/core/cpp/, correctly certified
  C++-lane T3 (CALL side = [cxx-thiscall-wall](../cpp-lane/cxx-thiscall-wall.md)). 0x1006D0B0 also twinned.
- The other 7 strong (`this-ecx ret-n`, no twin) are ALL already declared
  thiscall in the C lane (`__fastcall(T*, int _edx, ...)` / BR_THISCALL1).
  Their T3 residue is real register colouring, not a missing convention.

**Why:** `cpp_screen` fires on ANY callee-cleaned thiscall shape, including
functions already correctly spelled as thiscall callees ([thiscall-via-fastcall](../cpp-lane/thiscall-via-fastcall.md)).
A `this-ecx`/`ret-n` flag is NOT evidence of C-lane misclassification. The
tool screens unmatched residue for routing; on already-matched T3 rows it
just re-detects the convention that was already handled.

**How to apply:** the lever that could still move these colouring walls is
TU co-filing (constant-pool / x87 TU-state, [x87-wall-mechanism-2026-09-13](../levers/x87-wall-mechanism-2026-09-13.md),
[tu-constant-pool-oracle-2026-09-15](../corpus/tu-constant-pool-oracle-2026-09-15.md)), NOT re-routing to the C++ lane. The
only true C++-lane shape is `eax-vcall`/`vtbl-cache` virtual dispatch, and
those are already twinned. Tiny `rows 0+0` measures tempt a "residue is the
calling convention" hypothesis - it isn't; the idiom is already applied.
