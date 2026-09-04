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
original binary. Snapshot of 2026-09-04: **1,161 functions reproduce the original
bytes exactly (179,727 B).** Every image is then reassembled from those claims and
diffs to **0 bytes**, with 0 overlapping address claims — all four in-scope
binaries pass.

**Every byte-exact function now lives in the module that owns what it does.**
Until 2026-09-03 two thirds of the matched C code sat in `sliceN_MM.c` address
batches — 570 functions at the worst, 516 when the clear-out started. It is 11
now, each one recorded in its file's header with what was tried and why it
cannot move (ten share one state block; one is byte-exact only inside its own
translation unit). Both halves of rule 6 are gated, and the gates are
ratchets — see **Keeping it that way** below.

- **Game DLL (`BRGlide.dll`)** — 1,056 functions byte-exact, 167,278 B, **34.8% of
  its 480,853 B `.text`**. Of those, 882 are C and 174 are C++. Most of what
  remains is structural, not "coloring": 174 functions still carry real,
  non-codegen diffs (wrong or missing code), while only a 42-function tail is
  down to pure register-allocation/scheduling differences with the instructions
  already correct.
- **C++ class (vtables, EH frames)** — a separate lane, because these functions
  are unreachable from C at all; screened for before a function is ranked as a
  C target.
- **Executables** — **all three in-scope EXEs are now game-code complete**:
  BRally.exe (launcher), BossRally.exe (intro shim) and, as of 2026-09-03,
  SetVideo.exe (display wizard) — its last function, a 2,144-byte `WinMain`,
  had been the one hold-out. 105 functions, 12,593 B; every non-CRT map row in
  all three is byte-exact and all three images diff to 0. What is left in them
  is statically-linked MSVC 5.0 CRT (SetVideo 27,888 B, BossRally 20,075 B,
  BRally 495 B), reproduced by linking rather than decompiled — the same call
  as `BRD3D.dll` under rule 0.
- **Documentation** — **every byte-exact function says what it does**: 1,161 of
  1,161 carry a `WHAT IT DOES:` comment above their `@implements` tag, and
  `tools/fileaudit.py` fails the build if one lands without. The looser sets are
  covered too — all 1,395 *tagged* functions (byte-exact or still diffing) are
  described. Untagged port-side code is not: ~415 function definitions in files
  with no `@implements` tag remain undescribed, none of them byte-exact.
- **macOS/Metal port** — the same source boots, renders the front end and retail
  car geometry, runs physics + collision. **The port build is currently broken**:
  `src/core/slice2_12.c`'s narrow `BrFixPackS16Q15Neg` return type is only
  reconciled with the header under `BR_MATCHING_BUILD`, so clang stops there and
  the suite does not run. A dedicated session was spun up for this on 2026-09-03
  and ended without fixing it, so treat it as open. Last green run was 136/136 on
  2026-08-27; that number is historical, not live.

## Progress Report

**The C lane by tier** (`.venv/bin/python3 tools/tiers.py`) — 1,334 functions,
377,641 B of `.text`. That is the 1,505-function hand-C target less the 171 the
C++ lane owns; **this table is C only**, and the C++ lane has its own below.
Nothing is counted in both.

The tiers track how close each function's rebuilt code is to the original, from
not-started to exact. Two boundaries matter: **T1→T2** is a machine-made rough
draft on the side vs. real code built into the project; and inside "it works,"
**T3b** behaves like the original but is built differently (so it compiles to
different instructions), while **T3a** is down to the last cosmetic gap — same
instructions, only the register choices differ.

| Tier | Meaning | Fns | `.text` B |
|---|---|--:|--:|
| T1 | **Not started** — no real code in the project yet (just a machine rough-draft on the side) | 274 | 176,164 |
| T2 | **In progress** — real code is in the project, but the logic still differs from the original (or isn't confirmed right yet) | 176 | 108,963 |
| T3b | **Works, built differently** — behaves like the original, but compiles to different instructions; needs reshaping | 15 proven¹ | (within T2) |
| T3a | **Works, near-identical** — same instructions as the original, only which registers were used differs | 37 | 9,503 |
| **T4** | **Done** — matches the original exactly, byte for byte | **847** | **83,011** |

**Every tier — T1 included — already has at least a rough C draft from the
decompiler.** No one is reading raw assembly from a blank slate; the original
assembly is only the reference each draft is checked against. "Not started"
(T1) means that draft hasn't been turned into real project code yet, not that
no C exists.

Only **T1, T3a, and T4 are counted automatically** by the tier tool. T3a is
strong static evidence (the instructions match), which is not the same as a
runtime equivalence proof.

`tiers.py` prints T4 as a combined 1,018 over the full 1,505 target, with the
C++ lane called out beneath it; the table above subtracts that lane to stay
C-only, so its T4 is 1,018 − 171 = 847 and its target 1,505 − 171 = 1,334.

Earlier snapshots showed a much larger T1 and a much smaller T4, and the
difference was a **counting bug, not progress**: converting a function to the
C++ lane REMOVES its row from the C report, and the tier tool read that absence
as "not started", so T1 climbed every time a C++ match landed. Fixed in
`tiers.py`; T1 fell 459 → 274 the moment it did.

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

**The C++ lane by tier** (vtables + EH frames; `tools/cpp_sweep.py`) — same tiers,
so the two tables read alike. **This one is C++ only**; add it to the C table
above for the DLL total. A C++ function is scored on **four pieces**, not one:
its code, and the three tables the compiler emits alongside it to unwind the
stack when an exception passes through (unwind, scope, funcinfo). Done means all
four.

| Tier | Meaning | Fns | `.text` B |
|---|---|--:|--:|
| T1 | **Not started** — screened as C++-only, no source written yet | 20 | 28,997 |
| T2 | **In progress** — source exists; the three exception tables already match, the code does not | 25 | 23,568 |
| T3a | **Works, near-identical** — code byte-exact, one table still differs | 0 | 0 |
| **T4** | **Done** — all four pieces byte-exact | **174** | **80,300** |

Three differences from the C table, all real:

- **T1 here is a lower bound, not a census.** The C table has a fixed target
  derived from the function map; there is no equivalent count of "every C++
  function in the DLL". T1 is whatever `tools/cpp_screen.py` has recognised in
  the *current* unmatched residue — 20 strong of 172 screened. A further 49
  screened weak (fastcall-representable) are deliberately excluded: those are
  reachable from C and belong to the C lane. T1 falling is therefore progress,
  not scope loss: this lane is being drained faster than the screen finds new
  members.
- **T3a is empty for a reason.** Every unfinished row here is unfinished in its
  *code*; the exception tables come out right first. So nothing currently sits in
  the "instructions right, only registers differ" tier.
- **T1 is the one figure that is not yet disjoint from the C table.** A function
  screened as C++ but not yet written still has its row in the C report, so those
  22 also sit in the C table's T1/T2 until they are converted. T2, T3a and T4
  here are exclusively C++.

`tiers.py`, run against the same tree, sizes the done lane at 171 / 75,339 B
rather than 174 / 80,300 B: it ran moments earlier, and it takes each function's
size from the function map instead of the extracted bytes. Both are right at
their own strictness; don't average them.

Screening for this class *before* ranking a function as a C target is now part of
triage — 41 rows / 37,677 B that C cannot reach had previously been ranked as
prime C targets.

**Executables** (game code; static CRT is linked, not decompiled):

| Binary | Game code | Fenced CRT | |
|---|---|---|---|
| BRally.exe (launcher) | 28 fns · 2,860 B | 495 B | game code complete |
| BossRally.exe (intro) | 35 fns · 2,482 B | 20,075 B | game code complete |
| SetVideo.exe (config) | 42 fns · 7,251 B | 27,888 B | game code complete |

All three have **zero** unmatched non-CRT rows in their function maps, and each
reassembles to 0 differing bytes. SetVideo was the last to close (2026-09-03):
its 42 include 3 one-to-nineteen-byte CRT stubs, so the game span itself —
`0x401000`–`0x402D20` — is 7,228 B of code plus 228 B of inter-function
alignment, i.e. the whole 7,456-byte range.

**Totals** — 1,136 byte-exact functions / 176,104 B (`python3 tools/total.py`):
1,031 in `BRGlide.dll` (163,511 B, 34.0% of its `.text`) and 105 across the three
EXEs (12,593 B).

**The image gate** (`python3 tools/image_build.py`) is the deliverable check, and
the one a per-function diff cannot perform. Each function is diffed alone, so a
function whose bytes are right can still be wrong about *where* it goes: two
functions claiming one address, a claimed range overrunning its neighbour, a
wrong size. The gate refuses collisions rather than resolving them, lays every
claim into one image at the address it claims, uses the original's own bytes for
everything not yet decompiled, and diffs the result against the retail DLL.

    placed into the image            : 1028 functions, 163,511 bytes (34.00% of .text)
        C     854 fns    83,211 B  (1,536 B of that filled from the reference, 1.8%)
        C++   174 fns    80,300 B  (10,636 B of that filled from the reference, 13.2%)
    overlapping address claims       : 0
    ASSEMBLED IMAGE vs ORIGINAL      : 0 differing bytes

Both lanes are placed together, so a C claim and a C++ claim landing on one
address would be caught; none do. Read the reference-filled column as a
discount on the evidence, not on the match: those are relocation slots whose
target has no address in any surveyed map (per-file statics, and the C++ lane's
mangled symbols), so the reference image's own dword is used. Such a slot cannot
fail the diff — the C lane is 1.9% reference-filled, the C++ lane 13.2%, and
resolving the latter through real addresses is open work.

Port test suite does not currently build — see Status Summary.

![decomp progress treemap](docs/progress-map.svg)

Every box is one function, sized by its bytes in `.text` and grouped by module;
filled means byte-exact.

Regenerate: `python3 tools/match_sweep.py [file.c]` (merges into
`build/match/report.csv`); `python3 tools/progressmap.py --svg docs/progress-map.svg`.

## Keeping it that way

Rule 6 says a function is not done until it says what it does and lives in its
module. Both halves are now gated, and every gate below exists because the
thing it catches actually happened.

**A new match must be BORN in its module.** `tools/precommit_rule6.py` (the
pre-commit hook, installed by `tools/install_hooks.py`) refuses a commit that

  * adds an `@implements` with no `WHAT IT DOES:` comment beside it, or
  * creates a new `sliceN_MM.c`, or
  * **adds a new `@implements` VA to an existing `sliceN_MM.c`.**

The third is the one that matters and it was missing until 2026-09-04.
Refusing to let anyone *create* an address batch never stopped anyone dropping
a new match *into* one — which is exactly what the old `autofile.py` did, by
address, unattended. That is where all 570 stranded functions came from. It
compares by VA against the last commit, so re-spelling a function the batch
already holds (the normal way a wall falls) is untouched; only a VA the file
did not have before is rejected.

**The audit is a ratchet, not a wish.** `python3 tools/fileaudit.py` checks all
three lanes and exits 1 when a number goes UP: undescribed functions (baseline
0), address batches (58), stranded matched functions (11). It used to fail
unconditionally while any backlog existed — and a check that always fails is a
check people stop running, which is how the rule went unenforced for months.
Lower a baseline as you drain it; never raise one.

**A green sweep is not proof a move is safe.** `match_sweep.py` only ever
compiles `/DBR_MATCHING_BUILD`, so a `#else` port arm calling something whose
declaration did not travel with it is an implicit declaration (C89: a warning)
and an undefined symbol in the object — a link failure with a clean `n/n
match` either side of it. `python3 tools/portcheck.py --baseline main` compiles
the other configuration and reads the symbol table. On its first tree-wide run
it found five real defects the sweep had passed, including a file that would
not compile at all. Run it after any refile, with `--map <newfile>=<origin>`
for a file you created and `--ignore snprintf`.

**The sweep compiles NOTHING for a file with no `@implements` tag.** So an
emptied batch file can stop compiling and no sweep will ever say so. The
tree-wide `portcheck.py` run is what covers those; that is how the broken one
above was found.

**Moving byte-exact code can change it.** A function's surroundings decide its
codegen, not just its text — see the "surrounding TU decides commutative
operand order" entry in `docs/VC5-IDIOMS.md`. Carry the source file's ENTIRE
preamble verbatim; do not trim an include because it looks unused. Sweep both
files after every move and keep the move only if the moved function still
matches and nothing left behind regressed.


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

### Reference data (you supply; none tracked in git)

These are the exact dumps the match counts were produced against — `setup.sh`
checks the MD5s and warns if a different image is in `reference/`.  Without
them the tree still builds and the suites that need retail data skip with a
reason, but the matching pipeline cannot run and no byte-exact claim can be
verified.

```
reference/msvc/VCPP-5.00.iso                  Visual C++ 5.0 disc image
reference/brally/BossRally.BIN                retail PC disc
    MD5  31c64f9b1e09788c2dfc384b44af8f6c     616,572,096 bytes (MODE1/2352)
reference/brally/BossRally.cue                cue sheet (data + 12 audio tracks)
    MD5  a48a4a5860558177c3041afee57e03c9     622 bytes
reference/tgrally/Top Gear Rally (USA).z64    Top Gear Rally ROM (optional)
    MD5  6f7030284b6bc84a49e07da864526b52     8,388,608 bytes (big-endian, NGRE)
```

Rosetta 2 is required on Apple Silicon (the Wine build is x86_64). `setup.sh`
pulls `BRD3D.dll`, `BRGlide.dll` and the other game binaries out of the BIN
into `orig/` — do not copy them by hand. Assets are extracted from the same
images and never committed or redistributed; without them the tree still
builds and the suites that need retail data skip with a reason.

- **`sh build_match.sh`** — compiles with the original compiler and diffs; each
  function reports MATCH or DIFF with the first divergence. `tools/pe_patch.py`
  patches matches back into the DLL for drop-in testing.
- **`./build.sh`** — builds the macOS port with clang (core + tests + a runnable
  `build/brally`). Modules and tests are auto-discovered. `./tools/regress.sh`
  runs every suite.
