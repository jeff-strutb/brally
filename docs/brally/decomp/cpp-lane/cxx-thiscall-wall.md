# Cxx thiscall wall

*Recorded 2026-09-21.*

> RESOLVED via the C++ lane - 0x1003FA00 BrPhaseLeave is implemented in src/brally/core/cpp/0x1003FA00.cpp. EAX-vtable-pattern (C++ thiscall) functions aren't a wall; they belong in the C++ lane, not matched in C. Keep the EAX-vs-EDX thiscall analysis.

`BrPhaseLeave_10046560` (Glide VA `0x1003FA00`) has 13 remaining diffs after
inlining `Br31LeavePrologue` and using direct globals. The stuck diffs are in the
f00 vtable call.

**Two f00 call patterns exist in the corpus:**

- **EDX pattern** (`8b 11 6a 01 ff 12`): 3-arg `__fastcall(pThis, pVtbl, 1)` puts
  pVtbl in EDX. Used by `BrPhaseLeave_100471B0` - already matched via
  `Br31PhaseVtblMatch`.

- **EAX pattern** (`8b 01 6a 01 ff 10`): `MOV EAX,[ECX]` then `CALL [EAX]` with
  ECX=pCur (no EDX arg). This is C++ thiscall (`pCur->f00(1)`) - the compiler
  loads the vtable into EAX as an indirect-call temporary, not as a function arg.
  `BrPhaseLeave_10046560` reference uses this. **Cannot be expressed as a C
  function pointer call without inline asm**: any fastcall puts pVtbl in EDX
  (generating `ff 12`); any stdcall with 1 arg makes the compiler load pCur into
  EAX via `a1` (5-byte, shorter), not into ECX (6-byte), so pCur is in EAX
  throughout and the vtable load becomes `8b 00` (MOV EAX,[EAX]).

**How to apply:** If a function's reference has `8b 0d [pCur]; 85 c9; 74 NN;
8b 01; 6a 01; ff 10` and all other work is done, document it as a C++ thiscall
wall and move on. Do NOT re-try 1-arg stdcall, 2-arg fastcall, or cdecl variants
 -  all have been ruled out analytically.
