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

## Status (2026-08-27)

The matching pipeline is live end to end: MSVC 5.0 runs under Wine, and each
source file is compiled and diffed function-by-function against bytes extracted
from the original binary. Half of the game DLL's `.text` is transcribed into C,
and **598 functions now reproduce the original bytes exactly** — driven by a
growing dictionary of proven compiler idioms (`docs/VC5-IDIOMS.md`), a
Ghidra-assisted batch pipeline whose `--refine` hill-climb encodes those idioms
as automatic source transforms, and an auto-filer (`tools/autofile.py`) that
files each machine-found match into the tree, verifies it with a single-file
sweep, and commits it. The newest transform recovers each call's **calling
convention directly from the original bytes** (stdcall/thiscall/COM-vtable vs
Ghidra's cdecl default) and is folded into the free loop, so the ~220-function
call-convention class is cracked automatically as it combines with every other
generator. Register-allocation "coloring" walls — where the source is
structurally correct but the compiler assigns registers differently — are the
documented ceiling and the largest remaining functions. A randomized
source-permuter (thousands of compile-and-score iterations per function) was
built and run against them and confirmed they are **not reachable by source
permutation** (the three largest, 1.4–1.8 KB each, moved <3% and never flipped
their register coloring); they are accepted as link-time / hand-asm cases, not
search targets.

**Two large classes were reopened as workstreams.** (1) The ~97 KB of C++
exception-handling functions (20% of the DLL `.text`) that push a
`__CxxFrameHandler` frame were previously fenced as "unreachable from C." A
`.cpp`/`cl /GX` harness (`tools/cpp_score.py`) now reproduces one byte-exact
and — critically — verifies all four pieces including the unwind tables in
`.xdata`/`.rdata` that the normal `.text` comparison cannot see. The harness now has **36 C++ functions matched byte-exact on all four
pieces (15,832 B, including one 8,349-byte function — the largest single match
in the project)**, now filed into `src/core/cpp/` and counted (via
`tools/cpp_sweep.py`) in the 701-function grand total.
The clean wins are small-to-mid C++ functions and function families sharing
one `.cpp` pattern; large C++ bodies land their exception *frame* but their
*body* hits the same register-coloring ceiling as plain C, so those are
classified as walls. (2) Plain SEH (`__try`/`__except`) is matchable in VC5 C
and reproduces its frame byte-exact (five EH helpers matched).

**The EXE workstream opened 2026-08-27.** The three in-scope executables
(~64 KB, ~12% of the target) were untouched; now **BRally.exe is the first
fully game-code-complete binary — 24/24 user functions byte-exact (2,831 B,
79% of its `.text`)**, with the remaining fifth correctly identified as
statically-linked CRT/linker code to fence and link, not decompile. SetVideo.exe
and BossRally.exe are in progress. A key finding: CRT linkage varies per binary
(BRally is `/MD` dynamic, SetVideo is `/ML` static), which dictates the call
form — and shared file/INI helpers reuse byte-identically across the family.

Separately, the macOS/Metal port of that same source boots, renders the front
end from the game's own artwork, parses tracks, draws retail car geometry
through Metal, and runs the physics integrator with collision response wired in;
the port *source* compiles clean and its **test suite is now 136/136 green**.
That suite had drifted out of sync as functions were rematched to their real
(glide, byte-exact) form — the reconciliation is done: stale pre-match test
models were rewritten to the matched behaviour, cross-binary flat-globals-vs-
struct splits (BrFadeTick, BrInputIsDown) were driven correctly, and a couple of
cases that cannot hold on a 64-bit / non-x87 host are quarantined with reasons.
The per-frame race render is the last major rendering gate.

| Aspect | Measure | |
|---|---|---|
| **TOTAL byte-exact (all binaries, C + C++)** | **736 functions · 75,489 bytes** — verify with `python3 tools/total.py` | |
| Assembled image (DLL) | every matched claim laid into the real DLL: **0 differing bytes** — `python3 tools/image_build.py` | `████████████████████` |
| Transcribed into C (DLL) | 246,530 / 480,853 bytes of `.text` · 1,140 / 2,818 mapped functions | `██████████░░░░░░░░░░` 51% |
| **Byte-exact under MSVC 5.0 (DLL)** | 598 of 1,178 transcribed functions · 48,969 bytes | `██████████░░░░░░░░░░` 51% |
| Byte-exact, of all DLL `.text` | 48,969 / 480,853 bytes | `██░░░░░░░░░░░░░░░░░░` 10% |
| **BRally.exe (launcher)** | **24 / 24 user functions · 2,831 B — game code COMPLETE** | `████████████████████` 100% |
| SetVideo.exe (user region) | 37 matched · 4,675 B of 10,448 B (rest is static CRT) | `█████████░░░░░░░░░░░` 45% |
| BossRally.exe (user region) | 35 matched · 2,482 B of 3,008 B (rest is static CRT) | `████████████████░░░░` 81% |
| C++ EH class (tree-resident) | 38 functions · 16,503 B byte-exact on all four pieces (incl. the 8,349 B 0x10056260 — the largest match in the project), filed in `src/core/cpp/` | `████████░░░░░░░░░░░░` 41% of C++ code |
| Port milestones | 3 of 7 done (boot, front end, in-screen navigation); 3 partial | `████████░░░░░░░░░░░░` 43% |
| Port source build | compiles clean (all `src/core` + `src/exe` + `src/core/cpp`) | `████████████████████` |
| Port test suite | **136 / 136 green** | `████████████████████` 100% |

The second and third rows are the ones that count; everything else is
diagnostics. They tell the same story from opposite ends: nearly half of the
transcribed functions are exact, but the exact set is small-function-heavy, so
by raw bytes the campaign is just getting started.

One denominator note: many of the 2,818 mapped entries are 16 bytes or smaller
-- import thunks, jump stubs and exception-handling scaffolding the original
toolchain generated, not code anyone wrote. Those are reproduced by the link
stage of the rebuild, not by matched C, so the realistic hand-matching target
is smaller than the full map. The formal fence list for that class is still
being drawn up; until it lands, the conservative 2,818-function denominator
stands in the table above.

Both denominators move as work lands, so the percentages are not monotonic. A
function only enters the measured set once it carries an `@implements` claim,
and a sweep of one long-untagged module can add more to the denominator than to
the numerator — coverage counted honestly, not a regression. Read the absolute
count alongside the percentage.

Regenerate with `python3 tools/match_sweep.py` → `build/match/report.csv`. Every
run merges its results back, so the normal case is a single file
(`python3 tools/match_sweep.py src/core/<file>.c`, a few seconds, and it prints
the tree total too); the whole-tree sweep is bookkeeping, not part of the work
loop.

`python3 tools/progressmap.py --svg docs/progress-map.svg` renders the same
data as a treemap — every known Glide function as a tile sized by its original
bytes, grouped by module: green byte-exact, amber tagged with diffs remaining,
gray untranscribed. The interactive version is `build/match/map.html`; hover a
tile for name, VA, size and diff count.

![decomp progress treemap](docs/progress-map.svg)

## Building

**`./build.sh`** builds the port target: the whole decomp compiled with clang,
every test binary, and `build/brally` — the full core linked into one runnable
macOS binary, with unported functions satisfied by counted stubs that report at
exit which ones a run actually reached. Modules and tests are *discovered*, not
listed; dropping a `.c` into `src/core/` and its test into `tests/` is enough.
Needs only clang and the macOS SDK. `./tools/regress.sh` runs every suite.

**`./setup.sh`** stages the matching build, entirely inside the repo — nothing
is installed onto the host, and deleting `tools/wine/`, `tools/msvc5/` and
`orig/` puts the machine back exactly as it was. It downloads a pinned,
checksummed portable Wine build, copies the MSVC 5.0 compiler, headers and
libraries out of a Visual C++ 5.0 disc image, extracts the game binaries from
the Boss Rally disc (and the N64 soundtrack from the Top Gear Rally ROM, if
present), and extracts the original function bytes from the game DLL.

Setup needs the following, all supplied by you and none of them tracked in git.
These are the exact dumps the match counts were produced against — `setup.sh`
checks the MD5s and warns if a different image is in `reference/`:

    reference/msvc/VCPP-5.00.iso                  Visual C++ 5.0 disc image
    reference/brally/BossRally.BIN                retail PC disc
        MD5  31c64f9b1e09788c2dfc384b44af8f6c     616,572,096 bytes (MODE1/2352)
    reference/brally/BossRally.cue                cue sheet (data + 12 audio tracks)
        MD5  a48a4a5860558177c3041afee57e03c9     622 bytes
    reference/tgrally/Top Gear Rally (USA).z64    Top Gear Rally ROM (optional)
        MD5  6f7030284b6bc84a49e07da864526b52     8,388,608 bytes (big-endian, NGRE)

plus Rosetta 2 on Apple Silicon, since the Wine build is x86_64. `setup.sh`
pulls `BRD3D.dll`, `BRGlide.dll` and the other game binaries out of the BIN
into `orig/` — do not copy them by hand. Assets are extracted from the same
images and never committed or redistributed; without them the tree still
builds and the suites that need retail data skip with a reason.

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
