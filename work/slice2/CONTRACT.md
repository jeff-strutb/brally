# Slice-1 decompilation contract — READ FULLY BEFORE WRITING CODE

You are decompiling part of Boss Rally (PC, 1999) from `BRD3D.dll` into portable
C99. Other agents are working on disjoint address ranges at the same time. This
contract exists so the pieces link together and stay consistent.

## Non-negotiables

1. **Only write these three files**, where `NN` is your agent number:
   - `port/src/slice1_NN.c`
   - `port/include/slice1_NN.h`
   - `port/tests/test_slice1_NN.c`

   Do **not** edit `build.sh`, any existing header, or any other agent's files.
   Integration is done by the coordinator.

2. **Use the existing types. Do not invent your own.** They already exist and
   are already verified:
   - **COM/vtable types**: if you model a COM interface, name it for the
     interface (`BrDPlayVtbl`, `BrDSoundVtbl`), NEVER a generic `BrComVtbl`.
     A previous round produced two different `BrComVtbl` definitions that
     could not coexist in one translation unit.
   - `slice1_01.h` .. `slice1_10.h` already exist and cover 0x10001000-0x10086A10
     in part. Check whether your function is already done there before writing it.
   - `br_vec.h`  — `BrVec3` (float x,y,z) and 18 operations
   - `br_vecd.h` — `BrVec3d` (double x,y,z) — a SEPARATE type, never fold it
     into `BrVec3`
   - `br_mat.h`  — `BrMat4` (row-major `float m[4][4]`, row-vector convention)
   - `br_seg.h`, `br_pool.h`, `br_bits.h`, `br_obj.h`, `br_slots.h`, `br_span.h`

   Read those headers first. If a function you are given is already implemented
   there, do not duplicate it — note it in your report and move on.

3. **Verify by building.** Your work is not done until:
   `clang -std=c99 -Wall -Wextra -Iport/include -c port/src/slice1_NN.c`
   compiles with **zero warnings**, and your test binary builds and passes.

## How to decompile here

- Your assigned functions and their annotated disassembly are in
  `work/slice1/agentNN.asm`. Addresses are the source of truth.
- Naming: `Br<Module><Verb>`, e.g. `BrTexUpload`. Structs `Br<Thing>`.
  Fields whose meaning you cannot establish get positional names (`f10`,
  `f14`) — **do not invent semantic names you cannot justify**.
- Keep the original's argument order even when it is inconsistent. Several
  already-found examples: `BrMat4Copy` takes source first while every vector
  routine takes destination first; `BrVec3dCross` puts output third while
  `BrVec3Cross` puts it first. Preserve, document, do not harmonise.
- Preserve original behaviour including apparent bugs. Where you deliberately
  deviate (for portability or memory safety), say so in a comment at that line
  beginning `DEVIATION:`.
- x87: operand order is NOT apparent without tracing the stack through every
  `fxch`. Trace it. If you cannot resolve a function confidently, **leave it
  out and report it** — a wrong-but-plausible function is far worse than a
  missing one. This has already burned this project three times.
- Endianness: decode integers byte-wise, never by struct overlay. Some payloads
  in this game are big-endian.

## Tests

Assert **behaviour and invariants**, not volume. A test that encodes your
expectation rather than a property of the code is worse than none — two such
tests have already produced false failures here. Prefer:
- mathematical identities (a cross product is perpendicular to both inputs)
- round-trips (swap twice restores the original)
- boundary conditions actually present in the original (clamps, guards)
- aliasing behaviour where the original permits it

## Cross-slice dependencies

If your function calls something outside your packet, do NOT reimplement it.
Declare it `extern` using the exact form `/* XSLICE 0xADDR */` on the line above
the declaration, so the coordinator can wire it mechanically:

    /* XSLICE 0x10073C90 */
    extern uint32_t BrSomeThing(void *p, int n);

Provide a working stand-in in your TEST file only, clearly marked.

## Known-correct facts (do not re-derive, do not contradict)

- Microcode is **F3DEX**: G_VTX has `n` in bits[15:10], `v0+n` in bits[7:1].
  Plain F3D's `16n-1` low-byte layout is WRONG for this game.
- `0x1007DFE0` is `operator new` (`_nh_malloc(size,1)`), NOT calloc -- memory is
  **not** zeroed. `0x1007D350` is `malloc`. `0x1007DE40` is `operator delete`.
- `0x1007C8A0` is `__ftol` (truncates toward zero, low dword before any clamp).
- `0x1007DB00` is `floor` (sets x87 RC toward -inf), not trunc.
- `0x10008B80` and `0x100378A0` are **stubs** in this build (a bare `ret`, and a
  flag-set) despite being called with real arguments.
- Anything at or above `0x1007CC40` is statically linked MSVC CRT. Do not port it.
- Entity/car records are stride `0x2B68`; the parallel array is stride `0x15C`.

## Report back (this is as important as the code)

End with a short report:
- functions completed, by address
- functions you skipped and exactly why
- every `DEVIATION:` you made
- **gotchas** — inverted conventions, reserved sentinel values, asymmetric
  clamps, arguments in a surprising order. These are the highest-value thing
  you produce; they are what gets lost otherwise.
