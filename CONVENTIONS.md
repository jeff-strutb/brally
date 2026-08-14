# Conventions

Rules this port follows. They exist because breaking each one has already cost
real time on this project, and most of them are not obvious.

## Files and types

- Modules are `port/src/<name>.c` + `port/include/<name>.h` + `port/tests/test_<name>.c`.
- **Reuse the shared types; never redefine them.** `br_vec.h` (`BrVec3`),
  `br_vecd.h` (`BrVec3d` — a *separate* double-precision type, never fold it into
  `BrVec3`), `br_mat.h` (`BrMat4`, row-major, row-vector convention),
  `br_phase.h` (`BrPhase_`, the canonical 0xC8 object), plus `br_seg.h`,
  `br_pool.h`, `br_bits.h`, `br_slots.h`, `br_span.h`, `br_crt.h`.
- COM/vtable types are named for the interface (`BrDPlayVtbl`, `BrDSoundVtbl`),
  never a generic `BrComVtbl`. Two incompatible `BrComVtbl` definitions once made
  a pair of headers unable to share a translation unit.
- Before naming anything, grep `port/include/` for the address. One address
  accumulated four names; the fix is always to reuse, never to coin a fifth.

## Transcription

- **Preserve original behaviour, including bugs.** Where you deliberately depart,
  comment the line beginning `DEVIATION:`. Several genuine defects are reproduced
  on purpose — an acceleration solver that never sums Z, a conflict pass that
  wipes its own results, a lap timer that loses a second on exact values.
- Keep the original's argument order even when it is inconsistent. `BrMat4Copy`
  takes source first while every vector routine takes destination first;
  `BrVec3dCross` puts the output third while `BrVec3Cross` puts it first.
  Preserve and document — do not harmonise.
- x87 operand order is **not** apparent without tracing the stack through every
  `fxch`. Trace it. If you cannot resolve a function confidently, leave it out and
  say so. A wrong-but-plausible function links cleanly and fails only at runtime,
  which is the worst outcome available here.
- Comparison polarity: `fcom`/`fcomp` + `test ah,<mask>` sets C0/C3 for
  **unordered** as well, so NaN takes the *true* side. Write these as negated
  comparisons (`!(a >= b)`), never the tidy positive form.
- Read float constants out of the binary rather than assuming them. This is cheap
  and has repeatedly been load-bearing — a dial rendered as a degenerate sliver
  until its two radii turned out to be different constants.

## Portability

- Decode integers **byte-wise**, never by struct overlay. Some payloads are
  big-endian.
- **Byte offsets are 32-bit-only.** On LP64 every pointer widens and struct tails
  shift. Never overlay a struct on a file image or foreign buffer, and never
  allocate an original size literal — `0xC8` under-allocates the phase object by
  104 bytes on a 64-bit host. Use `sizeof`.
- No Win32 types or calling conventions in portable code.
- C99 throughout, `-Wall -Wextra`, zero warnings. Compile-time assertions use the
  negative-array-size trick (`port/tests/test_layout.c`), since C99 has no
  `_Static_assert` and the tree stays on one standard.

## Tests

Assert **behaviour and invariants**, not volume. A test that encodes an
expectation rather than a property of the code is worse than none: three separate
assertions have failed here for that reason — "majority of pixels opaque" against
a transparency-keyed image, a triangle-count threshold against a low-poly model,
a "scissor rects are always in bounds" claim against an asymmetric clamp. In each
case the code was right and the test was wrong.

Prefer mathematical identities (a cross product is perpendicular to both inputs),
round-trips (swap twice restores the original), boundary conditions actually
present in the original, and aliasing behaviour where the original permits it.

## Facts not to re-derive

- Microcode is **F3DEX**: `G_VTX` has `n` in bits[15:10], `v0+n` in bits[7:1].
  Plain F3D's `16n-1` low-byte layout is wrong for this game.
- `0x1007DFE0` is `operator new` (`_nh_malloc(size,1)`) and does **not** zero.
  `0x1007D350` is `malloc`; `0x1007DE40` is `operator delete`.
- `0x1007C8A0` is `__ftol`: truncates toward zero, returns the **low dword** of a
  64-bit `fistp`, so out-of-range yields **0**, not `0x80000000`.
- `0x1007DB00` is `floor` (sets x87 RC toward -inf), not trunc.
- `0x10008B80` and `0x100378A0` are **stubs** in this build despite being called
  with real arguments.
- Anything at or above `0x1007CC40` is statically linked MSVC CRT. Do not port it.
- Entity/car records are stride `0x2B68`; the parallel array is `0x15C`.
  `car+0x1030` is speed in mph; `car+0x10AC` is a struct-of-arrays.
- `.rca`: N64 struct at file `0x8000`, N64 address `0x803C8000`.
- `guLookAtF` (`0x100309A0`) and `guPerspectiveF` (`0x10030930`, **7 args**) are
  **not** stock libultra. `BrAtan2` takes **x first** and is a bisection accurate
  to ~0.01 rad — do not substitute `atan2f`.
- Command byte `0xE1` is FILL RECTANGLE with **integer** corners here.
- `config/functions.csv` is a good index, **not ground truth**: it invents entries
  (`0x100331FF`, `0x100334D7`, `0x100312BB` are mid-instruction), misses real ones,
  and 37 of 2,581 extents end mid-flow. If a listing starts mid-instruction, skip
  it and say so.
