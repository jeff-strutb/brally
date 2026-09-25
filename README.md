# Boss Rally — bit-exact decompilation

Jeffrey Wilbur (StrutB)

## Project Purpose

A bit-exact decompilation of **Boss Rally** (PC, 1999), held to the MAME
standard: the C source is the single source of truth and must compile to
**byte-identical output** under the original compiler (MSVC 5.0), while the same
source also cross-compiles to modern platforms. One tree, two build targets (the
SM64 model). Progress is measured only in per-function byte-identical
equivalence; playability is a consequence, never a reason to reorder matching.
Do keep the port buildable.

## Game background

Boss Game Studios built *Top Gear Rally* for the N64 (1997), then shipped *Boss
Rally* for Windows (1999) on the same engine. The PC build still emits N64 F3DEX
display lists, ships N64-format textures and big-endian geometry off the disc,
and carries the ROM's diagnostic strings — so the N64 ROM is read here as a
second witness to the same logic, never transcribed. The PC game ships two
renderer DLLs over one shared core: **`BRGlide.dll`** (3dfx Glide — the mature
target and the reference for all matching) and `BRD3D.dll` (Direct3D — statically
links Microsoft's CRT, so it's reference-only, out of scope).

## Cheats

*Boss Rally* was long believed to have **no cheat codes at all** — none are printed
in the manual, and none have ever circulated online. The decompilation turned up a
working cheat system hiding in plain sight: the game quietly watches the last 32
keys you type on the front-end menus, and the instant the tail of what you've typed
spells one of six code words, it fires. There's no cheat menu and no prompt — you
just type the word while sitting in the menus (it also works while typing into an
on-screen name box, since that feeds the same buffer). A little chime confirms it.

Each code is a person's first name:

| Code | What it (likely) does |
|---|---|
| `hazel` | Unlocks **all cars** |
| `brielle` | Unlocks the **three bonus cars** (takes the roster from 11 up to 14) |
| `benjamin` | Unlocks **all tracks** |
| `lynette` | Unlocks a **hidden second set of tracks** the menus normally won't show |
| `sophia` | Plays the game's **ending / credits sequence** without having to finish the championship |
| `madeleine` | Cranks a **visual effect** (the "ripple" effect) up to maximum — a novelty toggle, not a gameplay advantage |

The five unlock codes are certain from the code. `madeleine` is the odd one out:
it's clearly a deliberate toggle with its own confirmation sound, and it removes the
cap on how strong the game's ripple effect can get — but exactly how that looks on
screen isn't recoverable from the binary alone, so treat its description as a best
guess pending someone actually typing it in-game.

## Split screen

The N64 game *Top Gear Rally* has a two-player split-screen mode. The PC version
appears to ship with **no way to select it** — yet the decompilation shows the
split-screen renderer is fully present, complete, and wired up. It was built, and
then left one step short of playable.

What's actually in the binary:

- **A complete two-way split renderer.** The frame setup switches on the view
  count with exactly two arms — one full-screen view, or **two stacked
  half-height views** (top and bottom, each clipped to its own half). There is no
  three- or four-way path; it's strictly a two-player top/bottom split.
  (`BrFrameBeginDl`, `src/core/drawing/br_framebegin.c`)
- **A per-view frame loop.** The frame drawer iterates `for (i = 0; i < views; i++)`,
  building each view's own camera and scene, and the camera code halves its height
  "which is what a split screen needs." Every downstream system — HUD, lap-time
  layout, on-screen captions, the "wait for player" prompts — already carries live
  `views == 2` branches. (`BrFrameDraw`, `src/core/drawing/br_framedrive.c`;
  `src/core/scene/br_camera.c`)
- **Generic multi-car control.** Cars are driven by a per-car function pointer;
  AI cars point at the AI controller, and the engine already runs any number of
  independently-controlled cars per frame. The physics don't care whether a given
  car is steered by a human or the AI. (`BrRaceDriverStep`,
  `src/core/racing/br_racestep.c`)

So the screen, the cameras, the HUD, the second car, and the physics are all ready.
The one missing piece is **input for a second local player**:

- Input is a singleton. There is one keyboard buffer and one mouse buffer (the
  `[2]` you see is current/previous-frame double-buffering, not two players), and
  one control layout. (`src/core/controls/br_inputpoll.c`, `br_ctrlquery.c`)
- The routine that applies a player's controls to a car, `BrCtlInputApply`, takes
  a car pointer but reads its input from a **single global** with no device or
  player index — hand it any car and it feeds that car the same one human's input.
  (`src/core/driving/br_ctlinput.c`)

To turn this into a working mode you would need to add a second device binding /
control layout, give the input applier a per-player selector so entrant 0 and
entrant 1 read different devices, assign the second entrant a human controller
instead of the AI one, and expose the mode in a menu. Everything below that — the
hard part, the rendering — is already done.

## Status

Two milestones, both measured in bytes of BRGlide `.text` (480,853 B — the
primary target; in-scope EXE game code is separately complete). The block below
is a snapshot; counts move as sessions land matches. Regenerate the bars and the
treemap in one step with `python3 tools/progressbar.py` — do not hand-edit them.

<!-- PROGRESS:BEGIN — generated by tools/progressbar.py; do not edit by hand -->
_Snapshot 2026-09-25._

```
M1  Contract-valid — compiles & ports (T3 + T4)
    ███████████████████████████████████░░░░░  87.7%   421,570 / 480,853 B   1,446 / 1,500 fns
M2  Byte-exact (T4)
    ████████████████████░░░░░░░░░░░░░░░░░░░░  50.8%   244,158 / 480,853 B   1,263 / 1,500 fns
```

The bars sit close by design: matching is byte-exact-first, so only 183
certified-but-not-yet-exact functions (177,412 B) separate M1 from M2. Byte
percentages trail function percentages (96.4% / 84.2% of functions) because the
functions still open are several times larger than the matched ones.

By binary — the three EXEs are complete at both milestones (their game code is fully
byte-exact; the static CRT filling out each image is reproduced by linking, not
decompiled, and is out of scope). All remaining work is in BRGlide.dll.

| Area | M1 — contract-valid | M2 — byte-exact |
|---|---|---|
| **BossRally.exe** | `████████████████████` 100% — 35/35 fns, 2,482 B | `████████████████████` 100% — 35/35 fns, 2,482 B |
| **BRally.exe** | `████████████████████` 100% — 28/28 fns, 2,860 B | `████████████████████` 100% — 28/28 fns, 2,860 B |
| **SetVideo.exe** | `████████████████████` 100% — 42/42 fns, 7,251 B | `████████████████████` 100% — 42/42 fns, 7,251 B |
| **BRGlide.dll** | `██████████████████░░` 87.7% — 421,570 B, 1,446 fns | `██████████░░░░░░░░░░` 50.8% — 244,158 B, 1,263 fns |
<!-- PROGRESS:END -->

Query the tree. Do not trust a number in this file.

```bash
.venv/bin/python tools/refcheck.py     # corpus must be Glide-keyed
.venv/bin/python tools/total.py        # byte-exact functions and bytes, all lanes
.venv/bin/python tools/tiers.py        # T1/T2/T3/T4 of the C target (bytes, not just fns)
.venv/bin/python tools/image_build.py  # M2 gate: assembled DLL vs original; must be 0 differing bytes
.venv/bin/python tools/image_build_t3.py  # M1 gate: T3+T4 all compile & place; only T3 bodies differ
.venv/bin/python tools/brbox_diff.py --all  # M1 behaviour gate: T3 image vs original, every script, every frame
.venv/bin/python tools/fileaudit.py    # WHAT IT DOES, filing, T3 tags
```

`image_build.py` is the Milestone-2 (byte-exact) gate; `image_build_t3.py` is
the Milestone-1 (contract-valid) one — it compiles every T3-certified function
as well and proves the whole corpus builds and places, emitting `BRGlide.T3.dll`.
Its T3 bodies differ in bytes by design, but it is a **working drop-in**: it runs
the retail game (tested in a Win98 VM under 86Box) and behaves identically to
the original on every scripted session (below).

T1 = draft only; T2 = in the tree, not done; T3 = certified complete, not
byte-exact (`tools/t3.py --qualify`); T4 = bytes diff clean.

### How T3 is verified

"Not byte-exact" must still mean "does exactly what the original does". Two
oracles run the **original game** headless (`tools/brbox.py`: the retail
`BRGlide.dll` under Unicorn, driven by input scripts in `tools/brbox_scripts/` —
boot, menus, quick race to the finish, time attack and saves, championship and
season save/load, options and video modes, cheats and credits, a force-feedback
wheel, CD music, and two-machine DirectPlay races with join and dropout):

- **A5 — per function** (`tools/t3live.py`, ledger `config/t3_live.csv`): at
  real calls in the running game, the original body and the T3 body run from the
  identical captured state; memory, imports, registers, x87 state and live
  return values must agree. Unreached functions are UNCOVERED, never passed.
- **A7 — whole image** (`tools/brbox_diff.py --all`, ledger
  `config/whole_image.csv`): the assembled `BRGlide.T3.dll` and the original run
  every script side by side and must agree on **every frame** — every Glide call
  and its arguments, the whole data area, and every network packet (both
  machines of a network session run the image under test). Currently 26/26
  scripts identical.

A7 exists because A5 alone was not enough: the whole-image run found some 25
real transcription bugs — inverted branches, swapped or wrong arguments,
off-by-one indexing, x87 rounding points, and image-builder relocation errors —
in functions A5 had certified. `t3.py --qualify` requires both, and A7 is never
superseded. Bytes the original itself reads without ever writing (stale stack)
are listed with their evidence in `config/ub_scrub.csv` and neutralised on both
sides. Function counts
overstate progress: remaining functions are several times larger than matched
ones. Quote bytes of `.text` with the denominator (BRGlide `.text` is 480,853 B).

In-scope EXE game code (BRally.exe, BossRally.exe, SetVideo.exe) is complete;
what remains in those images is statically-linked CRT, reproduced by linking.
The macOS/Metal port is a separate build (`./build.sh`); if it does not compile,
see `docs/MEMORY.md`. C++ EH functions are a separate lane (`tools/cpp_sweep.py`).

**macOS full-boot port (interim 32-bit lane, 2026-09-25).**
`ports/macos/wasm/build_wasm.sh` compiles the verified build's sources to
wasm32, translates them to C and links a native arm64 `build/wasm/brally`. The
original image's data sits at its original addresses, rendering goes through
Metal, and the game data is extracted from the retail bin/cue
(`tools/extract_disc.py`). No decomp source is edited for it. Current build:
666 objects, 2,785 functions (1,497 of BRGlide's 2,148 at their original
addresses), 291 host imports, 0 missing game functions. A run boots, draws the
copyright screen, and reaches the first track load. There it stops: a
colour-indexed texture reaches `BrTex3dExpand` with no palette, because two
scan globals get no port address (details in `ports/macos/wasm/FINDINGS.csv`).
The native 64-bit port comes later.

A byte-exact session picks targets with `python3 tools/t4lane.py --claim`
(Pool B). Procedure: `docs/MATCHING.md`.

The image gate (`tools/image_build.py`) is the deliverable check a per-function
diff cannot perform: it lays every claim at the address it claims, refuses
overlaps, fills unmatched ranges from the original, and diffs the image.

![decomp progress treemap](docs/progress-map.svg)

Every box is one function, sized by its bytes in `.text` and grouped by module:
green = byte-exact (T4, M2), blue = contract-valid but not yet byte-exact (T3),
amber = still diffing (T2), gray = not started (T1), purple = linker/CRT
(fenced). Green + blue is the M1 corpus. Regenerate: `python3
tools/progressmap.py --svg docs/progress-map.svg`.

## Keeping it that way

A function is not done until it says what it does and lives in its module.
`tools/precommit_rule6.py` refuses a commit that adds an `@implements` without
a `WHAT IT DOES:` comment, creates a `sliceN_MM.c`, or adds a new VA to an
existing address batch. `tools/fileaudit.py` is a ratchet across all three
lanes (undescribed 0, batches 58, stranded 11) and fails on a bad `@t3` tag.
After a refile: `python3 tools/portcheck.py --baseline main` — the sweep only
compiles `/DBR_MATCHING_BUILD`. Moving byte-exact code can change it; sweep
both files and keep the move only if nothing regressed.

## Architecture

The repo root is the decomp. `ports/` is derived platform code, not byte-matched.

    src/core/                 portable game logic (byte-matched)
    src/backends/{glide,d3d,win32}   original Win9x platform layer
    src/exe/                  the three Win9x executables (game code done)
    include/  tests/
    tools/                    matching pipeline + staged MSVC 5.0
    config/                   function maps, globals, binaries.csv, fenced.csv
    build/match/              extracted reference bytes, per-function report
    ports/macos/              macOS/Metal port — NEW code, no `@implements`
    n64/                      Top Gear Rally (IDO/MIPS); writes only build/n64/

`@implements <addr>` is a hard claim: MSVC 5.0 emits those original bytes.
The engine is dispatch-driven — only ~13% is reachable by following `call`
from the entry point — so functions are taken leaves-first.

Further reading: [ARCHITECTURE.md](ARCHITECTURE.md), [docs/MATCHING.md](docs/MATCHING.md),
[docs/VC5-IDIOMS.md](docs/VC5-IDIOMS.md), `ports/README.md`. Retired starting
docs live in `docs/archive/`.

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
