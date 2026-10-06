# The portable 64-bit core

Status: done as planned (2026-10-03). The core builds and plays on macOS
(Metal or Vulkan) and builds for Windows (Vulkan), and matches the macOS
32-bit lane (`ports/brally-wasm/wasm/`) tick for tick under `BR_VCLOCK`. That lane
stays the home of the Remastered lighting, car, skies and music until they
move over. The README's "Native port" section describes the result; this
file is the design it was built to.

## Goal

One native build of `src/brally/core` that any 64-bit C compiler can build, with a
thin platform layer under it:

| OS | renderer | status |
|---|---|---|
| macOS | Metal (or Vulkan on MoltenVK) | plays |
| Windows | Vulkan | builds |
| Linux | Vulkan | not started (a window host is what is missing) |

Game logic is built once, for every OS. Each OS supplies only the platform
backends (window, input, audio out, timing, files, CD audio, renderer).

## What stands in the way

The matching source is ILP32: int, long and pointers are all 4 bytes, and
it depends on that in five ways. `ports/brally/tools/lp64audit.py`
measures each one from the compiler's own view of the tree. Run it for the
current counts (never copy a count from here):

```sh
.venv/bin/python ports/brally/tools/lp64audit.py   # needs build/brally/wasm32 from build_wasm.sh
cat build/brally/analysis/lp64audit/summary.txt
```

1. **The original image's data.** The 32-bit lane loads BRGlide.dll's own
   `.data` and `.rdata` at their original addresses. The source defines only
   a small part of the initialized data the game reads. Everything else is
   image-only, including the pointer slots inside it (the DLL's base
   relocations): function-pointer tables, string tables, pointers to other
   tables. A native build has no image to map. `image.csv` lists every
   address the code reads and whether source defines it.
2. **Loaded N64-format data.** Models (`.rca`), tracks and display lists
   come off the disc in N64 format. The loader rewrites their 4-byte
   segmented addresses in place into host pointers (`br_seg.c`,
   `br_dlrebase.c`). A 64-bit pointer does not fit in those slots, and the
   display-list interpreter reads them as 8-byte commands.
3. **Game structs with pointer fields.** The compiler lays these out again
   at 64 bits by itself. What breaks is code that assumes the 32-bit layout:
   the tree's own offset and size static asserts, byte-offset pointer
   arithmetic, and pointers stored in `int` or `DWORD` (`layout.csv`,
   `asserts.csv`, `casts.csv`). A small set also changes where `long` is 64
   bits (LP64 only), which fixed-width types fix.
4. **C++ class views.** The C++ lane declares a class per function as a view
   over one shared object. At 64 bits every view of one object must agree on
   each field's offset.
5. **Inline asm** in five drawing files.

Floating point is not on this list. The 32-bit lane already computes with
IEEE float and double and no x87 extended precision, and a native build does
the same, provided every build uses `-ffp-contract=off`. Without it, arm64
clang fuses multiply-add, which rounds differently.

## How the core is made

The core is a one-time copy of the decomp: `ports/brally/src` and
`ports/brally/include`, taken from the commit in `src/brally/FORKED-FROM`, and edited
directly. Every function is certified to behave as the original does, so
later matching work in `src/brally/` (byte shape, not behaviour) never has to flow
into it. `src/brally/` and `src/brally/include/` stay exactly what MSVC 5.0 compiles.

`tools/brally/sync.py` catches the core up: it lists every decomp commit since
`FORKED-FROM` with the files it touched, each function whose signature
changed, and each file the decomp added, removed or renamed. A change of
behaviour, signature, a global's identity or a file name is carried by hand
into the core's retyped code and checked in lockstep; a respelling is not.
`sync.py --stamp` then moves `FORKED-FROM`. A blind three-way merge does not
work here: a respelled decomp body merges into the retyped one and the result
reads neither's locals (`sync.py --merge FILE` does one file on request).

The core never includes the MSVC 5.0 SDK headers. `platform/include/win32.h`
declares the part of Win32, winmm and DirectSound the game uses, with
fixed-width integer types, the SDK's struct layouts and COM vtable orders.
Each OS's platform layer implements it.

`build64.sh` compiles every core file natively (`-ffp-contract=off`).

## Work, in order

1. **Compile.** Every core file builds as native 64-bit code. Where a
   declaration disagrees with a definition, the definition is the truth
   (it is what matched the original). Handles and pointers kept in `int`
   become their real types.
2. **Retype raw offsets.** Decompiler-style bodies read memory through
   hard-coded 32-bit offsets and strides (`*(T *)(p + K)`,
   `(&DAT_x)[i * K]`, `param_N + K`). Each one becomes a named field of the
   record it addresses, so the compiler lays it out at 64 bits.
3. **One definition per global.** The 32-bit lane lets many names reach one
   original address. The core gets one definition per address, its initial
   value lifted once from BRGlide.dll (pointer slots from the DLL's base
   relocations become references to symbols), and every alias names it.
4. **Disc data.** N64-format models, tracks and display lists keep their
   4-byte address slots, holding 32-bit offsets into one asset arena, read
   through one accessor.
5. **Link and platform layer.** Move `ports/brally-wasm/wasm/host` and `native`
   from guest memory to real pointers behind `win32.h` and the Glide API.
   Metal first. The renderer interface stays neutral between graphics APIs,
   and Remastered shaders have one source translated per API.
6. **Lockstep.** Run the core and the 32-bit lane under `BR_VCLOCK` from the
   same inputs and compare game state every tick. The 32-bit lane retires
   when every script and attract sequence matches.
