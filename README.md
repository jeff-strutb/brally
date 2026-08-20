# Boss Rally - a bit-exact decompilation from Jeffrey Wilbur (StrutB)

## Purpose

A complete, bit-exact decompilation of Boss Rally (PC, 1999) — and of Top Gear
Rally where the two share code — held to the MAME standard: the C source is the
single source of truth, and it must compile to **byte-identical output** under
the original compiler (MSVC 5.0) while also cross-compiling cleanly to modern
platforms. One tree, two build targets, the SM64 model. The result is meant to
be both a verified record of the original binary and a portable, platform-
agnostic source you can build and run today. Playability is a consequence of
getting the transcription right, never a reason to reorder the work; progress is
measured only in per-function byte-identical equivalence.

## The two games

Boss Game Studios built *Top Gear Rally* for the Nintendo 64 in 1997 (published
by Kemco, under the licensed *Top Gear* name), then shipped *Boss Rally* for
Windows in 1999 under their own name. They are the same engine. The PC build
still emits **N64 F3DEX display lists**, ships N64-format textures (`.ci4` /
`.lut4`) and big-endian car geometry verbatim off the disc, and carries the same
function-scoped diagnostic strings as the ROM. The N64 ROM is therefore read
here as a second witness to the same logic rather than decompiled in parallel —
nothing from it is transcribed into the tree. The PC game ships two renderer
DLLs over one shared core: `BRGlide.dll` (3dfx Glide, the mature target and the
reference for renderer code) and `BRD3D.dll` (Direct3D, which statically links
~100 KB of CRT that has to be identified and fenced off).

## Status

The matching pipeline is live end to end: MSVC 5.0 runs under Wine, and each
source file is compiled and diffed function-by-function against bytes extracted
from the original DLL. Roughly half of the game's `.text` has been transcribed
into C, but only a small fraction of that currently reproduces the original
bytes exactly — the matched set is dominated by small leaf functions, and the
large ones are where the remaining work is. Separately, the macOS/Metal port of
that same source boots, renders the front end from the game's own artwork,
parses tracks, draws retail car geometry through Metal, and runs the physics
integrator with collision response wired in; 136 test suites pass. The per-frame
race render is the last major gate.

| Aspect | Measure | |
|---|---|---|
| Transcribed into C | 216,870 / 460,165 bytes of `.text` · 810 / 2,140 functions | `█████████░░░░░░░░░░░` 47% |
| **Byte-exact under MSVC 5.0** | 120 of 810 transcribed functions · 3,791 bytes | `███░░░░░░░░░░░░░░░░░` 15% |
| Race-render frontier | 42 / 50 direct callees of `0x10011FA0` drained | `█████████████████░░░` 84% |
| Port milestones | 3 of 7 done (boot, front end, in-screen navigation); 3 partial | `████████░░░░░░░░░░░░` 43% |
| Port test suites | 136 / 136 green | `████████████████████` 100% |

The second row is the one that counts; everything else is diagnostics.
Regenerate all of it with `sh build_match.sh` → `build/match/report.csv`.

## Building

**`./build.sh`** builds the port target: the whole decomp compiled with clang,
every test binary, and `build/brally` — the full core linked into one runnable
macOS binary, with unported functions satisfied by counted stubs that report at
exit which ones a run actually reached. Modules and tests are *discovered*, not
listed; dropping a `.c` into `src/core/` and its test into `tests/` is enough.
Needs only clang and the macOS SDK. `./tools/regress.sh` runs every suite.

**`./setup.sh`** stages the matching build, entirely inside the repo — nothing
is installed onto the host, and deleting `tools/wine/` and `tools/msvc5/` puts
the machine back exactly as it was. It downloads a pinned, checksummed portable
Wine build, copies the MSVC 5.0 compiler, headers and libraries out of a Visual
C++ 5.0 disc image, and extracts the original function bytes from the game DLL.

Setup needs the following, all supplied by you and none of them tracked in git:

    reference/msvc/VCPP-5.00.iso            Visual C++ 5.0 disc image
    orig/BRD3D.dll, orig/BRGlide.dll        the retail game binaries
    reference/brally/BossRally.BIN + .cue   retail PC disc  (assets, optional)
    reference/tgrally/*.z64                 Top Gear Rally ROM (optional)

plus Rosetta 2 on Apple Silicon, since the Wine build is x86_64. Assets are
extracted at build time and never committed or redistributed; without them the
tree still builds and the suites that need retail data skip with a reason.

Then **`sh build_match.sh`** compiles with the original compiler and diffs. Each
function reports MATCH or DIFF with a hex dump of the first divergence. Matched
functions can be patched back into the original DLL with `tools/pe_patch.py` to
produce a hybrid binary for drop-in testing.

## Architecture

The original already had the seam this project needs: two renderer DLLs over one
platform-agnostic core. The tree keeps that shape.

    src/core/         the decomp — matching C, shared by BOTH build targets
      startup/  driving/  drawing/  racing/  menus/  geometry/
      scene/    gamedata/ settings/ controls/ audio/
      *.c             unfiled address-batch modules (sliceN_MM.c), being retired
    src/backends/
      d3d/  glide/  win32/    original platform calls (matching build)
      metal/  macos/          the macOS port
    include/  tests/  tools/  config/  asm/  orig/

`src/core/` is the deliverable. It holds no platform code and no host
assumptions, so the same file feeds MSVC 5.0 for verification and clang for the
port. Anything that differs between the two original builds — roughly 73
functions — is renderer boundary and lives in `src/backends/`, where Metal is
simply a third backend rather than a fork.

`@implements <addr>` on a function is a hard claim: that function compiles to
byte-identical output under MSVC 5.0, proven by `tools/match_diff.py` against
the extracted original bytes. Passing a port test is not sufficient.

Two structural facts shape the work order. The engine is **dispatch-driven** —
only ~13% of the image is reachable by following `call` from the entry point,
the rest through stored function pointers — so functions are taken leaves-first
from a topological work order rather than by walking the call graph. And
`config/functions.csv` is a good index but not ground truth: it invents entries,
misses others, and records some extents long enough to swallow the following
function.

Further reading: [ARCHITECTURE.md](ARCHITECTURE.md) for the survey the work
order comes from, [CONVENTIONS.md](CONVENTIONS.md) for the coding rules (each of
which has cost real time), and [DECOMP_NOTES.md](DECOMP_NOTES.md) for the
accumulated detail; original bugs that must be preserved, format reversing,
adjudicated conflicts, and the gotchas that bite if forgotten.
