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
original binary. Snapshot of 2026-09-03: **1,075 functions reproduce the original
bytes exactly (128,357 B).** The whole DLL image is then reassembled from those
claims and diffs to **0 bytes**, with 0 overlapping address claims.

- **Game DLL (`BRGlide.dll`)** — 822 C functions byte-exact (77,726 B), 54.1% of
  the hand-C target by count. Most of what remains is structural, not "coloring":
  212 functions still carry real, non-codegen diffs (wrong or missing code),
  while only a 40-function tail is down to pure register-allocation/scheduling
  differences with the instructions already correct.
- **C++ class (vtables, EH frames)** — 149 functions byte-exact on all four
  pieces (incl. unwind tables), 40,182 B. This class is unreachable from C at
  all, and is now screened for before a function is ranked as a C target.
- **Executables** — BRally.exe and BossRally.exe are **game-code complete**;
  SetVideo.exe has one function left (WinMain). 104 functions, 10,449 B.
- **macOS/Metal port** — the same source boots, renders the front end and retail
  car geometry, runs physics + collision. **The port build is currently broken**:
  `src/core/slice2_12.c`'s narrow `BrFixPackS16Q15Neg` return type is only
  reconciled with the header under `BR_MATCHING_BUILD`, so clang stops there and
  the suite does not run. Last green run was 136/136 on 2026-08-27; that number
  is historical, not live.

## Progress Report

**Game DLL by tier** (`.venv/bin/python3 tools/tiers.py`) — hand-C target 1,519
functions (453,140 B of `.text`):

The tiers track how close each function's rebuilt code is to the original, from
not-started to exact. Two boundaries matter: **T1→T2** is a machine-made rough
draft on the side vs. real code built into the project; and inside "it works,"
**T3b** behaves like the original but is built differently (so it compiles to
different instructions), while **T3a** is down to the last cosmetic gap — same
instructions, only the register choices differ.

| Tier | Meaning | Fns | `.text` B |
|---|---|--:|--:|
| T1 | **Not started** — no real code in the project yet (just a machine rough-draft on the side) | 445 | 229,355 |
| T2 | **In progress** — real code is in the project, but the logic still differs from the original (or isn't confirmed right yet) | 212 | 137,664 |
| T3b | **Works, built differently** — behaves like the original, but compiles to different instructions; needs reshaping | 15 proven¹ | (within T2) |
| T3a | **Works, near-identical** — same instructions as the original, only which registers were used differs | 40 | 8,395 |
| **T4** | **Done** — matches the original exactly, byte for byte | **822** | **77,726** |

**Every tier — T1 included — already has at least a rough C draft from the
decompiler.** No one is reading raw assembly from a blank slate; the original
assembly is only the reference each draft is checked against. "Not started"
(T1) means that draft hasn't been turned into real project code yet, not that
no C exists.

Only **T1, T3a, and T4 are counted automatically** by the tier tool. T3a is
strong static evidence (the instructions match), which is not the same as a
runtime equivalence proof.

T1 rose and T2 fell sharply against the previous snapshot. That is not a
regression: functions solved twice — once as C, then properly as C++ — have had
their redundant C twin retired (`tools/cpp_twin_retire.py`), which moves them
out of the C project and back to draft-only without changing what is byte-exact.

The T3b footnote below still counts inside T2, and the numbers in this section
move daily; regenerate rather than trust them.

¹ **T3b is measured by a differential oracle** (`tools/t3b_verify.py`): it runs
both the original bytes and the recompiled bytes through the same interpreter on
identical random inputs and compares the return value and memory side effects.
Same output across many inputs ⇒ behaviorally equivalent (T3b). It is
conservative — it only judges functions it can fully contain (plain-cdecl
scalar/pointer args, no globals, no external calls), and reports everything else
as unclassified rather than guess. Last full sweep of the T2 pile (2026-08-28):
**15 proven T3b, 0 behavioral differences open, 358 out of reach** (the first
sweep's 3 real-differ finds have since been fixed). That sweep predates the
current T2 set, so 15 is a floor, not a live count; T3b grows as the oracle is
extended to register calling-conventions and global-reading functions. The 15
still count inside T2 until the tier tool consumes the oracle's manifest.

**C++ classes** (vtables + EH frames; 4-piece matching via `tools/cpp_sweep.py`):

| Pieces | Meaning | Fns | `.text` B |
|---|---|--:|--:|
| **4/4** | All four pieces byte-exact (code + unwind + scope + funcinfo) | **149** | **40,182** |
| 3/4 | One piece still differs | 17 | — |

88 further candidates from `tools/cpp_screen.py` of 193 screened: 38 strong
(C++ TU lane) and 50 weak (fastcall-representable). Screening for this class
*before* ranking a function as a C target is now part of triage — 41 rows /
37,677 B that C cannot reach had previously been ranked as prime C targets.

**Executables** (game code; static CRT is linked, not decompiled):

| Binary | Game code | |
|---|---|---|
| BRally.exe (launcher) | 28 fns · 2,860 B | game code complete |
| BossRally.exe (intro) | 35 fns · 2,482 B | game code complete |
| SetVideo.exe (config) | 41 fns · 5,107 B | WinMain left |

**Totals** — 1,075 byte-exact functions / 128,357 B (`python3 tools/total.py`):
971 in `BRGlide.dll` (117,908 B) and 104 across the three EXEs (10,449 B).

**The image gate** (`python3 tools/image_build.py`) is the deliverable check, and
the one a per-function diff cannot perform. Each function is diffed alone, so a
function whose bytes are right can still be wrong about *where* it goes: two
functions claiming one address, a claimed range overrunning its neighbour, a
wrong size. The gate refuses collisions rather than resolving them, lays every
claim into one image at the address it claims, uses the original's own bytes for
everything not yet decompiled, and diffs the result against the retail DLL.

    placed into the image            : 971 functions, 118,052 bytes (24.55% of .text)
        C     822 fns    77,870 B  (888 B of that filled from the reference, 1.1%)
        C++   149 fns    40,182 B  (7,356 B of that filled from the reference, 18.3%)
    overlapping address claims       : 0
    ASSEMBLED IMAGE vs ORIGINAL      : 0 differing bytes

Both lanes are placed together, so a C claim and a C++ claim landing on one
address would be caught; none do. Read the reference-filled column as a
discount on the evidence, not on the match: those are relocation slots whose
target has no address in any surveyed map (per-file statics, and the C++ lane's
mangled symbols), so the reference image's own dword is used. Such a slot cannot
fail the diff — the C lane is 1.1% reference-filled, the C++ lane 18.3%, and
resolving the latter through real addresses is open work.

Port test suite does not currently build — see Status Summary.

![decomp progress treemap](docs/progress-map.svg)

Every box is one function, sized by its bytes in `.text` and grouped by module;
filled means byte-exact.

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
