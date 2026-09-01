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
original binary. **885 functions reproduce the original bytes exactly (94,008 B).**
The assembled DLL image diffs to **0 bytes** over every matched claim.

- **Game DLL (`BRGlide.dll`)** — 739 functions byte-exact, 48.3% of the hand-C
  target by count. Most of what remains is structural, not "coloring": 385
  functions still carry real, non-codegen diffs (wrong or missing code), while
  only a 47-function tail is down to pure register-allocation/scheduling
  differences with the instructions already correct.
- **C++ exception-handling class** — 42 functions byte-exact on all four pieces
  (incl. unwind tables), 17,164 B.
- **Executables** — BRally.exe and BossRally.exe are **game-code complete**;
  SetVideo.exe has one function left (WinMain).
- **macOS/Metal port** — the same source boots, renders the front end and retail
  car geometry, runs physics + collision; test suite **136/136 green**.

## Progress Report

**Game DLL by tier** (`python3 tools/tiers.py`) — hand-C target 1,529 functions:

The tiers track how close each function's rebuilt code is to the original, from
not-started to exact. Two boundaries matter: **T1→T2** is a machine-made rough
draft on the side vs. real code built into the project; and inside "it works,"
**T3b** behaves like the original but is built differently (so it compiles to
different instructions), while **T3a** is down to the last cosmetic gap — same
instructions, only the register choices differ.

| Tier | Meaning | Fns | `.text` B |
|---|---|--:|--:|
| T1 | **Not started** — no real code in the project yet (just a machine rough-draft on the side) | 361 | 195,388 |
| T2 | **In progress** — real code is in the project, but the logic still differs from the original (or isn't confirmed right yet) | 382 | 184,227 |
| T3b | **Works, built differently** — behaves like the original, but compiles to different instructions; needs reshaping | 13 proven¹ | (within T2) |
| T3a | **Works, near-identical** — same instructions as the original, only which registers were used differs | 47 | 7,793 |
| **T4** | **Done** — matches the original exactly, byte for byte | **739** | **66,395** |

**Every tier — T1 included — already has at least a rough C draft from the
decompiler.** No one is reading raw assembly from a blank slate; the original
assembly is only the reference each draft is checked against. "Not started"
(T1) means that draft hasn't been turned into real project code yet, not that
no C exists.

Only **T1, T3a, and T4 are counted automatically** by the tier tool. T3a is
strong static evidence (the instructions match), which is not the same as a
runtime equivalence proof.

¹ **T3b is measured by a differential oracle** (`tools/t3b_verify.py`): it runs
both the original bytes and the recompiled bytes through the same interpreter on
identical random inputs and compares the return value and memory side effects.
Same output across many inputs ⇒ behaviorally equivalent (T3b). It is
conservative — it only judges functions it can fully contain (plain-cdecl
scalar/pointer args, no globals, no external calls), and reports everything else
as unclassified rather than guess. First sweep of the 382 T2 functions: **13
proven T3b, 3 found to genuinely differ from the original (real T2), 411 out of
reach.** So T3b's true size is ≥13 and grows as the oracle is extended to
register calling-conventions and global-reading functions. The 13 still count
inside T2's 382 until the tier tool consumes the oracle's manifest.

**Executables** (game code; static CRT is linked, not decompiled):

| Binary | Game code | |
|---|---|---|
| BRally.exe (launcher) | 28 fns · 2,860 B | game code complete |
| BossRally.exe (intro) | 35 fns · 2,482 B | game code complete |
| SetVideo.exe (config) | 41 fns · 5,107 B | WinMain left |

**Totals** — 885 byte-exact functions / 94,008 B (`python3 tools/total.py`);
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
