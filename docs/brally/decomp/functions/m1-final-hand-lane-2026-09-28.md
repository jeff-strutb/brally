# M1 final hand lane

*Recorded 2026-09-28.*

> 2026-09-28: last BRGlide T2 + all six UNCOVERED T3s hand-transcribed to T4 (0 T1/T2, 190 T3 all EQUIVALENT). Levers: symbol-count TU state (3 per decl, period 8), CSE homing, overwrite-to-pin args, compound-assign reload, C++ POD writer.

Session 2026-09-28 (project lead: "hand transcribe the last not-yet-T3/T4, ignore prior walls").
Result: BRGlide T1 0 / T2 0 / T3 190 (all live EQUIVALENT, 0 uncovered) / T4 1309.
Commits 40aab55f (0x10067470), 70a03928 (POD C++), 069fa9d9 (0x10024680),
4e9a79da (0x10005400), e58231da (0x10041180), 783303c7 (0x10021570), 3c92980e (t3_live rows).

Levers that broke dated "walls" (all now in docs/brally/VC5-IDIOMS.md tail):
- **Symbol-count TU state**: commutative x87 roles cycle with period 8 in steps of
  3 per file-scope declaration ahead of the function (extern, earlier fn's params);
  own locals, labels, blank lines, block-scope externs do NOT count. Diagnose with
  1..8 dummy `int f(int)` pads; commit via literal-for-extern (-3) or block-scope
  extern (0). 0x10067470.
- Negated scale homed in a dead arg slot = named `d = (dot) - k` + unnamed CSE
  `(0.0f - d)`; one local per arm or VC5 IL-merges the tails.
- VC5 sinks single-use named values into call args; PIN them by overwriting the
  source variable before the call (0x10021570, 24 variants inert before this).
- `f += now - last;` (compound) stops store-to-load forwarding into later calls.
- `done = 1;` before the clamp store stops arm tail-merging.
- Split pointer steps `p += 4; p += 4;` give add+mov return instead of lea.
- thiscall via +4 subobject with __fastcall C twin mis-scheduled => it's C++.

Image-gate trap met again: g_brCrPlane struct fused two original globals;
out lives at 0x117781A0 as `g_brCrPlaneOut` ([placed-image-verification](../oracle/placed-image-verification.md)).
Related: [x87-wall-mechanism-2026-09-13](../levers/x87-wall-mechanism-2026-09-13.md), [walls-are-dated-verdicts](../rules/walls-are-dated-verdicts.md).
