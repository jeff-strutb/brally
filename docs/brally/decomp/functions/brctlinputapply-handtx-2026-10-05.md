# Brctlinputapply handtx

*Recorded 2026-10-05.*

> 0x1005AFF0 BrCtlInputApply re-transcribed (C++ lane, 46d1eac6), 3 insns from T4. Levers: volatile keeps a dead store; frame slots = refcount sort; per-case block-local signs; header prefix fixes operand order; VC5 tail cross-jump + FUN_0043d16c re-duplication at <=20 B is the open residue.

Banked 2026-10-05 as T3 (commit 46d1eac6, src/core/driving/BrCtlInputApply_1005AFF0.cpp,
`BrCar::CtlInputApply`, C twin in br_ctlinput.c retired to a prototype). Live oracle
EQUIVALENT on 20_quickrace_drive; A7 whole-image run still owed after the commit.

**Levers that worked (all measured):**
- Dead store the original keeps (speed clamp, `fstp` then overwritten) = **volatile** local:
  one volatile read, clamp in x87 registers, one volatile store. DCE (C2 FUN_00404ba4) drops
  any non-volatile dead store; arrays/structs/pointers/inline helpers/pragmas did not help.
- Frame layout: C2 packs scalars first (sorted by refcount; join newest non-interfering slot
  >= half size), aggregates last. Escaped aggregates interfere to function end. Read the
  original's slots to decide which values are ONE variable (x/scale/z, v/mag/prev merged);
  a 4-way-used sign must be per-case block locals or its refcount sorts it too early.
- Pad call with no edx = C++ thiscall member; whole function then in the C++ lane.
- `#include <windows.h>` + CRT headers first: commutative operand order (fadd/fmul) follows
  front-end symbol ids (glide.h not needed here). See [symbol-id-wrap-lever-2026-10-05](../levers/symbol-id-wrap-lever-2026-10-05.md).
- `fld src; fst var; fcomp` throttle shape = separate statement inside the key `if`
  (assignment inside `&&` gives an int copy).
- Constant-pool literals per [photo-family-literal-lever-2026-10-05](../levers/photo-family-literal-lever-2026-10-05.md).

**Open residue (stopped at the project lead's call):** cases 1 and 2 end in the same statement; the
original shares it (case 1 jumps into case 2). VC5 cross-jumps it, then FUN_0043d16c
duplicates any jumped-to block whose size estimate is <= 20 B (threshold flag DAT_0048d020,
bit 23 of per-function opt flags; /G3-/G5 and pragma letters do not move it); ours estimates
exactly 20 (stack local operand counts 3). Global-store toys keep the share, pointer stores do
not. Dead ends: goto, block scope, double/cast curve, symbol-id wrap before d, SP3 C2.
Trace kit: c2cap/hs (hooks for DCE 0x404c0e/0x404c25, dup 0x43d3ab, jump 0x43d23f).
