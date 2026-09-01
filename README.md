# Boss Rally — bit-exact decompilation

Jeffrey Wilbur (StrutB)

## Project Purpose

A bit-exact decompilation of **Boss Rally** (PC, 1999), held to the MAME
standard: the C source is the single source of truth and must compile to
**byte-identical output** under the original compiler (MSVC 5.0), while the same
source also cross-compiles to modern platforms. One tree, two build targets (the
SM64 model). Progress is measured only in per-function byte-identical
equivalence; playability is a consequence, never a reason to reorder work.

## Game background

Boss Game Studios built *Top Gear Rally* for the N64 (1997), then shipped *Boss
Rally* for Windows (1999) on the same engine. The PC build still emits N64 F3DEX
display lists, ships N64-format textures and big-endian geometry off the disc,
and carries the ROM's diagnostic strings — so the N64 ROM is read here as a
second witness to the same logic, never transcribed. The PC game ships two
renderer DLLs over one shared core: **`BRGlide.dll`** (3dfx Glide — the mature
target and the reference for all matching) and `BRD3D.dll` (Direct3D — statically
links Microsoft's CRT, so it's reference-only, out of scope).

## Status Summary

The matching pipeline is live end-to-end: MSVC 5.0 runs under Wine, and each
source file is compiled and diffed function-by-function against bytes from the
original binary. **877 functions reproduce the original bytes exactly (90,919 B).**
The assembled DLL image diffs to **0 bytes** over every matched claim.

- **Game DLL (`BRGlide.dll`)** — 731 functions byte-exact, 47.8% of the hand-C
  target by count. The ceiling is register-allocation ("coloring") walls, where
  the C is structurally correct but the compiler assigns registers differently.
- **C++ exception-handling class** — 42 functions byte-exact on all four pieces
  (incl. unwind tables), 17,164 B.
- **Executables** — BRally.exe and BossRally.exe are **game-code complete**;
  SetVideo.exe has one function left (WinMain).
- **macOS/Metal port** — the same source boots, renders the front end and retail
  car geometry, runs physics + collision; test suite **136/136 green**.

## The compiler: MSVC 5.0, with one confirmed exception

`BRGlide.dll` is overwhelmingly **Microsoft Visual C++ 5.0** — 850+ functions
reproduce its bytes exactly, which is proof, not assumption. All matching work
defaults to it (`tools/msvc5`, run under Wine).

One function contradicts that, and it is worth recording because it pins down a
second compiler in the shipped image. The network car-state encoder
(`0x10006510`) emits a code sequence that MSVC 5.0 **cannot** produce: it pushes
a call's later argument *before* evaluating an earlier one while simultaneously
narrowing a shift to 16-bit width. Those two behaviours are mutually exclusive
under 5.0 — and, as tested exhaustively, under VS97 SP3 and VC6 as well (18
source spellings × 3 front ends). **Visual C++ 4.2** (`cl 10.20.6166`, June
1996) produces all of it in one shot, including the clean `thiscall` and the
x87 flag tests. So at least one object in the DLL — legacy networking/bitstream
code — was compiled with 4.2 and linked into an otherwise-5.0 binary.

This is *not* a project-wide compiler change. A bulk re-check under 4.2
(`tools/vc42_probe.py`, against `tools/msvc42`) produced **zero** new byte-exact
matches and instead *broke* dozens of functions that are exact under 5.0 (audio,
input, the fixed-point quantisers). 4.2 is staged and available for the specific
TUs that need it; 5.0 remains the reference for everything else. How we know
which is which: compile the translation unit under both and diff the
already-known-exact functions — the compiler that keeps them exact is the right
one for that TU.

## Progress Report

**Game DLL by tier** (`python3 tools/tiers.py`) — hand-C target 1,529 functions:

| Tier | State | Fns | `.text` B |
|---|---|--:|--:|
| T1 | still asm (no hand-C yet) | 364 | 197,111 |
| T2 | decomp'd, real diffs remain | 388 | 185,733 |
| T3 | codegen-only diff (same instructions; register/scheduling) | 46 | 7,653 |
| **T4** | **byte-exact** | **731** | **63,306** |

T3 is a static proxy ("done bar codegen"), not a runtime proof.

**Executables** (game code; static CRT is linked, not decompiled):

| Binary | Game code | |
|---|---|---|
| BRally.exe (launcher) | 24 / 24 fns · 2,831 B | 100% |
| BossRally.exe (intro) | 28 / 28 fns · 2,431 B | 100% |
| SetVideo.exe (config) | 38 / 39 fns · 5,084 B | 70% (WinMain left) |

**Totals** — 877 byte-exact functions / 90,919 B (`python3 tools/total.py`);
DLL image assembles to 0 differing bytes (`python3 tools/image_build.py`); port
tests 136/136.

![decomp progress treemap](docs/progress-map.svg)

Regenerate: `python3 tools/match_sweep.py [file.c]` (merges into
`build/match/report.csv`); `python3 tools/progressmap.py --svg docs/progress-map.svg`.

## Architecture

**The repo root is the decomp** — the master, byte-matched against the 1999
binaries. `ports/` is the only thing outside it: derived platform code with no
original bytes to match.

    src/core/                 portable game logic (byte-matched, feeds both targets)
    src/backends/{glide,d3d,win32}   original Win9x platform layer (byte-matched)
    src/exe/                  the three Win9x executables (byte-matched)
    include/  tests/
    tools/                    matching pipeline + staged MSVC toolchain
    config/                   function maps, globals; binaries.csv (per-binary
                              compiler + CRT model); fenced.csv / fenced_exe.csv
                              (linker/CRT — reproduced by linking, not decompiled)
    build/match/              extracted reference bytes, per-function report
    ports/macos/              macOS/Metal port — NEW code, not byte-matched

`@implements <addr>` is a hard claim: that function compiles to byte-identical
output under MSVC 5.0 (proven against the extracted bytes). Passing a port test
is not sufficient. The engine is dispatch-driven — only ~13% is reachable by
following `call` from the entry point — so functions are taken leaves-first from
a topological order, not by walking the call graph.

Further reading: [ARCHITECTURE.md](ARCHITECTURE.md), [CONVENTIONS.md](CONVENTIONS.md),
[DECOMP_NOTES.md](DECOMP_NOTES.md), and `ports/README.md`.

## Building & Setup

- **`./setup.sh`** — stages the matching build entirely inside the repo (nothing
  installed on the host). Downloads a pinned Wine build, copies MSVC 5.0 out of a
  VC++ 5.0 disc image, and extracts the game binaries and original function bytes
  from the retail disc. Needs (you supply; none tracked in git): `reference/msvc/VCPP-5.00.iso`,
  the Boss Rally disc (`reference/brally/BossRally.BIN` + `.cue`), optionally the
  Top Gear Rally ROM, plus Rosetta 2 on Apple Silicon.
- **`sh build_match.sh`** — compiles with the original compiler and diffs; each
  function reports MATCH or DIFF with the first divergence. `tools/pe_patch.py`
  patches matches back into the DLL for drop-in testing.
- **`./build.sh`** — builds the macOS port with clang (core + tests + a runnable
  `build/brally`). Modules and tests are auto-discovered. `./tools/regress.sh`
  runs every suite.
