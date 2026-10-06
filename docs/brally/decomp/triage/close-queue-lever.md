# Close queue lever

*Recorded 2026-08-30.*

> The efficient count-mover is closing the refine batch's CLOSE(n) output, not grinding named near-misses.

The cheapest byte-exact wins come from the `--refine` batch's own
`CLOSE(1..5)` lines (grep `CLOSE\([0-9]\)` build/refine_*.log), NOT from the
named near-misses (`BrScenePropsDraw`, `BrRcaFixup`, `BrCarStateEncodeDelta`).

**Why:** the named near-misses are parked precisely because they are the hard
residue - register-coloring walls and the C++ argument-scheduling idiom
([cxx-eh-frame-wall](../cpp-lane/cxx-eh-frame-wall.md), the VC5-IDIOMS arg-scheduling entries). Grinding them
is 20-worker work for <15 bytes.

**The lever (2026-08-30, landed 2):** a CLOSE(n) function is n bytes from
byte-exact and the batch already recorded the transform. Land it by:
1. Disassemble the orig bin (`build/brally/win32/match/orig/0xVA.bin`) - never trust the
   refine's type hints (they called an int `double`, a byte-load a CONCAT).
2. Find whether it slots into an EXISTING tree module that already declares
   its globals/imports - that is what makes it cheap. FUN_10002830 slotted
   next to its already-matched callee; FUN_10006460 slotted into the
   slice1_02.c netcode mutex cluster next to BrNetReset. A CLOSE that needs a
   FRESH subsystem (e.g. 0x100703D0's DirectInput globals + vtable typedefs)
   is NOT cheap - skip it.
3. fn.py / divergence.py to confirm 0 regions, single-file `match_sweep.py`
   to bank + register, commit immediately.

**Two proven byte closes this session:**
- FUN_10002830 (`7aad871`): callee's 2nd param is `unsigned char` not
  `unsigned int` - that is what makes the caller emit `mov al,[g]` (a0) not
  `mov eax,[g]` (a1). One load-opcode byte.
- FUN_10006460 (`325cf86`): mutex handle read via `WaitForSingleObject`, the
  compared global is signed (`test/jl`), helper is FUN_1006ba60.

**Confirmed NOT cheap (arg-scheduling wall):** BrTex3dDownloadAt 0x100283C0
is 4 bytes off - structurally identical, but MSVC hoists ONE grTex arg-load
above the two field stores. Same argument-scheduling idiom class blocking
BrCarStateEncode. Do not re-attempt without a fresh idiom. See
[register-rotation-is-a-symptom](register-rotation-is-a-symptom.md).
