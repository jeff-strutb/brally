# Boss Rally — decompilation project

## Quick start

Requires only clang and the macOS SDK. No dependencies, no package manager, no
game data:

```
git clone git@github.com:jeff-strutb/brally.git
cd brally
./build.sh
./build/brally
```

That boots the ported core and prints what it built:

```
after ctor:
phase  nPages=0 iPage=0 f0C=0 f68=1

running builder 0x1004D640 ...
  page 0  cCtl=7 cSel=3 origin=(195.0,130.0) flags=1

controls built: 7   setText=3 place=7
stubs: none reached -- everything the run touched is ported
```

`./build/brally -w` does the same and opens a Metal window drawing the controls
at the coordinates the builder computed. `./tools/regress.sh` runs every suite.

Verified from a clean clone on macOS 26 / Apple Silicon.

### What that output is, and what it is not

It is the game's own menu code running: the real phase constructor
(`0x10048710`) and a real screen builder (`0x1004D640`), both decompiled. Every
coordinate is computed by ported game logic -- the host invents no geometry.

It is **not the game**. There is no input, no game loop, and no scene
rendering. One menu screen is constructed and its controls are placed. Treat it
as proof the core executes, not as something to play.

Without extracted game data, five test suites skip and the window has no
background texture; everything else runs. See "Asset policy" below.


Target: **`BRD3D.dll`** (`sha256 29af141e…`), the DirectDraw build, for the shared
game core. It contains the entire game and exports one symbol, `RallyMain`.
`BRGlide.dll` is the same game against 3dfx Glide.

### Which binary is the reference (decided, and it is not one answer)

**Shared core -> either. Renderer -> BRGlide.**

The 1,739 functions shared between the two builds are byte-for-byte the same
logic; that is how they were identified. All core decompilation to date is
therefore unaffected by this choice, and BRD3D remains fine for it.

For the **renderer**, BRGlide is the better reference, for two reasons:

1. **Glide was the mature target.** This is a 1997/1999 title; 3dfx Voodoo was
   the dominant 3D gaming platform of that window and Glide the well-trodden
   API. DirectDraw/D3D in the DX5/6 era was the weaker, later path, and the
   DDraw build has the shape of a port rather than the lead platform.
2. **BRGlide is a cleaner binary to analyse.** It imports `MSVCRT.dll`, i.e. it
   links the CRT *dynamically*, whereas BRD3D statically links ~100 KB of MSVC
   CRT that has to be identified and fenced off. Roughly a fifth of BRD3D's
   `.text` is library code we must never port. BRGlide simply does not carry it.

Practical consequence: when implementing the Metal backend against the ~73
divergent functions, read the **Glide** side. The game emits N64 F3DEX display
lists either way -- the producer is shared, only the consumer differs -- and the
Glide consumer is the better model of what the game intends to draw.

`config/functions_glide.csv` already exists; `tools/crossdiff.py` pairs the two
builds function-for-function.

    orig/       pristine binaries + sha256 (the match target)
    tools/      analysis tooling (Python + capstone, in .venv)
    config/     generated maps: functions, names, strings, shared-code classification
    asm/        annotated disassembly, one file per 64KB of .text
    port/       the portable port: include/ src/ tests/

## Status

**~34% of the game is decompiled.** That figure has a stated denominator so it
can be checked, which the three earlier figures in this file's history did not:

| | bytes |
|---|---|
| Shared game code (`BRGlide.dll`) | 424,373 |
| Unknown / untriaged | 39,973 |
| D3D-only | 13,343 |
| **Game code to port** | **477,689** |
| **Ported** | **163,299 — 34%** |
| **Ported and audited-equivalent** | **~26%** (76% of a random sample) |

Excluded, deliberately: the statically linked C runtime (61,513 bytes — the
host supplies it) and the D3D renderer boundary (19,797 bytes — replaced by
Metal, not ported).

### What the executables are

The disc ships four PE images and only one of them is the game. This was not
established until the maps below were built, and until then every coverage
figure in this file used the wrong denominator.

| image | `.text` | functions | what it is |
|---|---|---|---|
| `Boot.exe` | 96,768 | 1,453 | CD autorun shell — runs SETUP/DXSETUP/SetVideo. **Installer** |
| `BossRally.exe` | 23,552 | 215 | plays `brally.avi`, launches `brally.exe`. **Intro player** |
| `BRally.exe` | 3,584 | 39 | reads `BossRally.ini`, `LoadLibrary`s the renderer DLL, calls `RallyMain`. **Launcher** |
| `BRGlide.dll` | 481,280 | ~2,700 | **the game** |

Maps: `config/functions_boot.csv`, `functions_bossrally.csv`,
`functions_brally.csv`, `functions_glide.csv`.

### The entry point is not ported

`RallyMain` is `BRGlide.dll`'s only export and the whole game's entry point —
Glide `0x1001CC00`, 324 bytes. Its five-state machine is now transcribed
(`port/src/br_boot.c`, `0 → 4 → 3 → 1 → 2`). **Every one of its eleven callees
is still absent**, including `0x10019730` (205 B, the main loop), `0x10019670`
(187 B, window creation — which names the wndproc at `0x100194C0`, where all
input arrives), and `0x1001D8A0` (924 B, the argument parse everything
downstream depends on).

An earlier revision of this section said nine of eleven were absent and two
were ported. Both "ported" entries were **prose mentions of the address inside
a comment**, found by a `grep -rl` that cannot tell a mention from a
definition. `tools/isported.py` answers this properly and reports all eleven
absent.

`port/host/brally.c` has been standing in for that startup — choosing a
builder, constructing a phase, inventing the wiring — while the original's
actual initialisation sat unread. Every "nothing builds the root menu" and
"transitions land on an empty phase" note elsewhere in this file traces back to
this single gap. Porting `RallyMain` and its chain is the correct root of the
call graph and the next work.

### How to read the other numbers in this file

They measure different things and only one of them is coverage.

| | |
|---|---|
| Modules | 121, organised by concern under `port/src/` — see `port/src/README.md` |
| ...still named after an address batch | **62** (`sliceN_MM.c`, loose at the top level) |
| Test suites | 105, 0 failures |
| Screen builders running | **16 of 16** (`./build/brally -all`) |
| Unported functions stubbed so the host links | **50** (`port/host/br_stubs.c`) |
| UI hook slots filled | 99 of 108 |
| Functions classed present in both renderer builds | 1,955 (`config/shared.csv`) |

**None of these is a completion measure.** "16 of 16 builders run clean" means
nothing crashed — several of those builders lead to placeholders. "105 suites
pass" means this project's own tests agree with this project's own code; two
tests have been found that could not fail under any implementation, one of
which hid a bug that dropped 96% of track geometry. Treat the 30% above as the
only coverage number here, and treat it as a ceiling on code *transcribed*, not
a floor on code *correct*: 42 call sites reach a placeholder or stub, and 43
hole annotations across 6 modules mark paths that are deliberately inert.

### The equivalence audit, and what the coverage figure is really worth

**It has now been done, on a sample.** 33 functions drawn at random from the
776 claimed-ported, audited against the original's disassembly by three
read-only passes that did not write the code:

| verdict | | |
|---|---:|---|
| EQUIVALENT | 25 / 33 | **76%** |
| ACCEPTABLE-DEVIATION | 5 / 33 | documented at the site, with a reason |
| DIVERGENT | 3 / 33 | 9% |

95% confidence interval on the strict rate: **61% – 90%**.

So of ~34% transcribed, roughly **26% of the game is transcribed *and*
verified**. Treat that as the real number.

All three divergences were genuine and all three are now fixed:

- an x87 **NaN polarity** inversion (`fcomp` sets C0 for less-than *and* for
  unordered, so the original returns on NaN and `alpha < 0.1f` did not);
- a buffer sized **4× too small** because `0x13000` was read as bytes where
  `rep stosd` counts dwords — and the port had added a bound the original does
  not have, turning the sizing error into silent truncation;
- a **lost aliased store**: a word written at an address that lies *inside* a
  block just zeroed, where the port modelled the two as separate objects.

The last one is the instructive case. Fixing it turned the suite red, because
the test asserted all 83 entries were zero — **the test had certified the bug
as correct behaviour**. Any later pass reading it would have "restored" the
divergence to keep the suite green.

The same audit found four tests that cannot fail, including one that applies a
byte-swap **twice** and compares against the input, which a no-op passes. That
is why a coverage percentage alone means very little here, and why
mutation-testing every new assertion is now mandatory (see CONVENTIONS.md).

## What actually works today

**Menus.** All sixteen ported screen builders run, in the game's own artwork,
with captions recovered from `BRString.dll` and laid out by the game's own
place routine. Selection moves, controls activate, screens transition.
`./build/brally -keys 4 "dd"` drives it headlessly; `-shot <n> <file.ppm>`
writes a frame offscreen with no window server, which is how every rendering
claim here is checked.

**Assets.** Extracted at build time from the builder's own disc, never
committed: 172 menu sprites, the string table, tracks, and the CD audio. A
missing disc degrades — the build still succeeds, suites that need data SKIP
with a reason, and the menu draws every sprite at the right size in the right
place as an outlined placeholder.

**Tracks.** `.TRK` files parse: 7,877 vertices and 9,226 faces for `desert`,
with every face index checked against the vertex array and the section extents
checked against the offsets the header stores.

**3D.** Retail car geometry renders **through Metal, textured and lit**: the
display-list interpreter walks the game's own lists, the ported clipper runs,
and the result matches an independent software rasteriser at 0.9999 silhouette
IoU. Textures decode from the game's CI4/CI8/RGBA16 data through the ported N64
texel expander.

**Physics.** A car falls under the game's own 1/30 s integrator, finds real
track triangles through the collision grid, and on flat ground settles to
**0.190132** — which is the analytic force balance, not a number that was
tuned to look right.

### What does NOT work

**There is no game.** The list below is not a set of loose ends; it is most of
the runtime.

- **The entry point is not ported.** `RallyMain` and nine of its eleven callees
  are absent — see above. There is no real startup, no real main loop, and the
  host fabricates both.
- **Cars fall through the world.** The collision *broad phase* is ported and the
  *response* (`0x10067710` + `0x10065C80`) is not, so contacts are detected and
  never resolved. A headless race shows z descending monotonically for the whole
  run, with the response entered as a counted no-op 14,400 times.
- **Nothing renders a race.** `0x1001B27A` — the in-race render, HUD and mirror,
  ~5.5 KB — is unported. `-race` is headless by design and no mode anywhere
  draws a race.
- **No car can drive itself.** Drive torque at `car+0x1CC` is written by the
  control chain at `0x1006F170`, unported. Lap progress in the race harness
  comes from the original's own phantom-entrant path, not from driving.
- **Menu transitions dead-end.** Screens build and navigate, but destinations
  are placeholders: `g_pBrUiBuildCtx86` is assigned only in tests.

The front end *renders* well and that is the most misleading thing in this
repository. Screens draw in the game's own art with the game's own font, which
reads as "nearly done" and is not: the layer underneath it has no boot, no
loop, and no destinations.

### The stub report is the work queue

`build/brally` links against a stub for every unported function. Each stub
records its own hit count and the program prints them at exit, so the question
"which of the remaining functions actually matter?" is answered by running the
program rather than by guessing. The first boot named exactly one blocker
(`BrUiCtlCtor`, `0x100476C0`); porting it took the report to
"stubs: none reached".

Run with `BR_STUB_ABORT=1` to die at the first stub instead of continuing on a
zero return.

Two caveats that matter. Stubs return an integer 0, so a caller expecting a
**float** gets whatever was in `xmm0`, not `0.0` -- such a gap shows up as a hit
rather than being silently trusted. And the 63 provisional data symbols are
zeroed blocks; anything whose behaviour depends on a non-zero initial value will
differ from the original until its owning module is ported.

### Read this before trusting any number above

`config/functions.csv` has **three confirmed failure modes**, all found during analysis:
1. **Invents entries** -- `0x100331FF`, `0x100334D7`, `0x100312BB` are
   mid-instruction, not function starts.
2. **Misses entries** -- e.g. `0x1002C2B0`, `0x100311E4`, `0x10031212`,
   `0x10031347`, `0x10069A70`, `0x10073D80`.
3. **Records wrong sizes** -- `0x10069A60` is 10 bytes, the map says 30, which
   swallows the padding *and* the following function.

Because extents can swallow functions, **the denominator ~1,708 is itself
uncertain**. Treat the map as a good index, never as ground truth.

The codebase is *not* pure C: ~9% of functions take `this` in `ecx` and ~14%
use vtable dispatch. RTTI is absent only because MSVC 5 defaults it off.

See `CONVENTIONS.md` for the coding rules this port follows -- most of them are
non-obvious and each one has cost real time.

## Tooling

- `tools/pe.py` — PE/COFF reader (sections, imports, exports, base relocations).
- `tools/funcmap.py` — function discovery. Seeds from entry point, exports, every
  direct `CALL` target, and every relocated dword pointing into `.text`. Recursive
  descent from each seed; switch tables recovered from `jmp [reg*4+disp]` where the
  table entries are themselves relocated. Address-taken pointers are only accepted as
  function *starts* if they follow inter-function padding or a terminator instruction —
  without that filter, switch-table entries get mistaken for functions.
- `tools/names.py` — recovers names from function-scoped diagnostic strings
  (`CHK_FReadOpen():`, `DDraw_DoInit:`, …). Ceiling is 21 such strings → 15 names.
- `tools/crossdiff.py` — matches functions between the two DLLs by instruction-mnemonic
  fingerprint (raw bytes don't match; addresses and register allocation differ).
- `tools/dumpasm.py` — annotated disassembly with resolved call targets, inlined string
  literals, import names, and `g_<rva>` tags for globals.

Run anything with `.venv/bin/python tools/<x>.py`.

## Known-good names so far

`CHK_FReadOpen`, `CleanupName`, `GetNumForName`, `GetPodLength`, `ReadPod`, `LoadPod`,
`Add`, `AppendTexture`, `TIDFromTextureAppend`, `DDraw_DoInit`,
`MakeEnemyCarColorPanels`, `APPMSG_HOSTSTARTED`, `RallyMain` (export).

## Goal: portable source, not byte-matching

Decided 2026-08-13. The target is **readable, platform-agnostic C that recompiles
into a working game** -- not a byte-identical rebuild of BRD3D.dll.

### Accuracy first; playability is the consequence, not the target

Restated 2026-08-16, after this project spent a long stretch doing the opposite.

The model is MAME, not ZSNES. Correctness of the decompilation is the point;
being able to play the result is what falls out of getting it right. It is not
a milestone to be pursued directly, and it is not evidence of anything on its
own.

What that rules out, concretely, because each of these was done here:

- Standing in for an unported function so that something visible happens. A
  placeholder is debt with a counter on it, never a foundation to build on.
- Reporting "it builds", "N suites pass" or "16 of 16 builders run" as
  progress. Those measure that nothing crashed and that our own tests agree
  with our own code.
- Chasing a visible symptom — a menu that will not navigate, a race that will
  not draw — by wiring around the gap instead of decompiling the function the
  gap is made of. The entry point went unread for weeks while the host
  hand-wrote a substitute for it.

The metric that counts is per-function behavioural equivalence against the
original's disassembly. Everything else is diagnostics.

This removes every toolchain blocker: no MSVC 5.0, no Wine, no Windows VM.
We build with clang natively and verify by **running against real shipped game
data** instead of by diffing bytes.

    port/include/  portable headers
    port/src/      portable C99 implementation
    port/tests/    tests that run against files from the retail disc
    testdata/      real game data used by the tests

    clang -std=c99 -Wall -Wextra -Iport/include \
        port/src/br_pod.c port/tests/test_pod.c -o build/test_pod
    ./build/test_pod testdata/BossRally.pod

`tools/objdiff.py` is retained but is no longer the verification path; it stays
useful for checking that a rewrite did not change behaviour structurally.

### Architecture

The DirectDraw/Glide split in the originals is the seam to build on: the ~1,684
functions shared between BRD3D.dll and BRGlide.dll are the platform-agnostic
game core, and the per-renderer halves are exactly what gets replaced. Phases:

1. Decompile the shared core to portable C (in progress).
2. Replace the platform layer -- DirectDraw/Glide/DirectInput/DirectPlay/MSACM
   -- with a portable backend (SDL is the obvious candidate).
3. Asset loading needs no reversing; the formats are open (BMP, WAV, POD, RCA,
   and raw N64 CI4/LUT4 textures shipped verbatim).

Portability rules adopted now, so they do not have to be retrofitted:
- integers decoded byte-wise, never by struct overlay, so the code is endian-
  and alignment-agnostic (the N64 build of this game is big-endian);
- no Win32 types or calling conventions in portable code.

### Completed modules

| Module | Source | Status |
|---|---|---|
| **br_vec** (18 fns) | core, `0x1003AC30`-`0x1003B130` | decompiled, verified by identity |
| **br_mat** (7 fns) | core, incl. `guFrustumF`/`guPerspective` | decompiled, verified |
| **br_span** (3 fns) | core, `0x1003A660`, `0x1003A910` | decompiled, verified |
| **br_seg** (2 fns) | core, `0x1002B970` | decompiled, verified |
| **br_pod** (6 fns) | core, `0x100085F0`-`0x10008850` | verified vs retail archive |
| **br_pool** (2 fns) | core, `0x10069490` | decompiled, verified |
| **br_slots** (2 fns) | core, `0x100586A0` | decompiled, verified |
| **br_state** (2 fns) | core, `0x1003E080`, `0x10073F40` | decompiled, verified |
| **br_obj** (4 fns) | core, `0x10073B40`-`0x10073F50` | **MISNAMED — see erratum in header; these are bit-stream members** |
| **br_bits** (3 fns) | core, `0x10035FA0`, `0x100383C0`, `0x10074030` | decompiled, verified |
| **br_vecd** (6 fns) | core, `0x100305B0`-`0x10030DE0` | decompiled, verified |
| br_img | format reversed | decodes retail `splash`/`loading.img` |
| br_rca | format reversed + **loader decompiled** | name, gears, and the 0x8000/0x803C8000 segment pair |
| br_n64tex | N64 CI4/LUT4 | decodes retail `cargfx/` |
| br_f3d | N64 display lists (**F3DEX**, corrected) | 547 triangles from retail cars |
| Metal backend | replaces DirectDraw | renders retail artwork |

**Core decompiled: ~966 of ~1,708.**  50 test suites, all green.
(The per-module table below lists only the hand-written `br_*` modules; the 41
`slice*` modules from the batched analysis passes carry the bulk and are listed in
the module list below.)

### Semantics that will bite if forgotten

| Thing | Gotcha |
|---|---|
| POD entry `+0x08` | is **two bytes** (`+0x08`,`+0x09`) from two separate `Add()` args -- `+0x0A/+0x0B` are **never written** (stack garbage). The header's 4th magic byte is never assigned either; do not validate it |
| `BrNetSlot +0x058..0x557` | slice1_02 recorded this as never touched -- it is **eight `BrCarState` records of 0xA0**, indexed in parallel with the sequence and occupied arrays |
| 22-byte record | **little-endian**, unlike every other serialised form in this game, and it **steals two bytes** from the top of earlier dword stores -- write order is load-bearing |
| decoded booleans | are **128.0f**, not 1.0f -- except `f70`/`f74`, which really are 1.0f |
| `guLookAtF` (`0x100309A0`) | **not stock libultra** -- Gram-Schmidts `up` (`y = norm(up - dot(up,z)z); x = y x z`) instead of `x = norm(up x z); y = z x x`. Do not substitute a stock version. No degenerate guard: `eye == at` yields an all-zero 3x3, no error |
| `guRotateF` (`0x10030EE0`) | built from lookAt as `L . Rz . L^T`, and passes `up = (y,z,x)` -- a cyclic shift of the axis, not a world up. Not a typo |
| **two rect encodings** | `0x1001BE30` reads the 12-bit fields as **10.2 fixed point** (masked to 10 bits); `0x1001C7A0` reads the *same layout* as plain signed integers. Feeding one to the other silently scales by **4** |
| rect Y flip | both handlers call as `(x1, cy-y2-1, x2+1, cy-y1)` -- a zero-extent rect still yields a 1x1 box |
| `0x10016B40` dial | the two radii are **different constants** (5.0f/7.0f vs 15.0f/20.0f); the code reaches past a clobbered stack slot for the second. Assuming they match gives a degenerate needle |
| `0x10016B40` | mixes per-view and **record-0** access on one pointer, and indexes the dial table **backwards** (`aDial[15-v]`) |
| `0x10019620` | integrates **upward** -- `fsubr` against `-343.0*dt` adds 343/s. It is a thunder front's distance, ending past 2048.0 |
| command `0xE1` | is **FILL RECTANGLE** in this build with plain 12-bit **integer** corners -- NOT the N64's `0xF6` with 10.2 fixed point |
| `0x10031688` | scales asymmetrically (original bug): the lower-right corner is shifted **twice** and the upper-left not at all |
| `0x100312BB` | is the **middle of a function**; the real entry is `0x100312A7`. Another function-map error, alongside `0x100331FF`/`0x100334D7` |
| `.rca` geometry | N64 struct at file **0x8000**, N64 address **0x803C8000** -- that pair goes to `BrSegSetBases`. No flat vertex array exists; geometry is reached via F3DEX lists after rebasing |
| `.rca` name padding | loader overwrites file bytes `0x7C` and `0x80..0x93` (inside szName's padding) with backend surface handles |
| `0x100370D0` | swaps `+0x90` but rebases `+0x94` -- every other site does both to the same dword. Retail cars leave `+0x90` holding a raw N64 address forever |
| `0x10038B20` tag 5 | has **no count of its own** -- it reuses the vertex count from the last tag-4 record, carried across loop iterations. A tag 5 before any tag 4 does nothing |
| palette size | comes from the **parallel** table at `0x106C7C64` (+0x20, same index/stride), not from the record being copied: `0x1000000` -> 32 bytes (CI4), else 512 (CI8) |
| `BrVec3Length` | was **missing** from br_vec.h despite sitting inside the cluster; sum of squares is rounded to **float32 before the sqrt**, unlike `BrVec3dLen` |
| camera frames | entity holds **five 0x44-byte frames** at +0x0000/+0x273C/+0x2780/+0x27C4/+0x2808: 4x4 row-major (fwd/right/up/pos) + one trailing float |
| `0x100019D0` | compiled-in bug: `[this+0x28F8]` gets two back-to-back unconditional stores in both arms, so the clamps are dead and it is **always 0.05f**. Do not fix |
| keepalive counters | read AFTER the wrap reset, so on the frame the counter hits its limit the packet is **skipped**, not sent |
| `BrVec3Lerp` | computes `(a-b)*t+b` -- **t=0 gives b, t=1 gives a** (inverted) |
| `BrRbSolveAccel` | **copy-paste bug in the original**: X and Y fold in all four children's accel, **Z never sums them** -- it only divides by mass. Pinned by test |
| two quaternion matrix builders | `0x100695D0` divides by the norm (guarded at 0), `0x10074450` does **not** and scales the matrix instead. **Not interchangeable** |
| `BrCarState` quaternion | **scalar-first**: `f00 = w`, `f04/f08/f0C = x/y/z`, `f10..f18` translation. Proven via `0x100695D0`'s only caller |
| lerp convention **bit in practice** | `BrPathWalk` passes `(out, pts[i+1], pts[i], u)` while `BrPathWalkFrom` passes `(out, pts[i], pts[i+1], s)` -- mirrored, and because `BrVec3Lerp` is `(a-b)*t+b` these are genuinely different functions of the parameter. They coincide only at `s==1` |
| NaN in car state | `fcomp 0.0 / test ah,0x40` is **C3 alone**, and unordered sets C3 -- so **NaN takes the zero branch**. Writing these as `== 0.0f` in C is wrong |
| volume sliders | slider A's table is **linear**, slider B's (master volume, `0x100BBAE0`) is a **perceptual curve**. Do not merge them |
| **two lerp conventions** | `0x10010B00` computes `(arg2-arg1)*t+arg1`, so **t=0 gives the FIRST arg** -- the opposite of `BrVec3Lerp`. Same math, opposite argument order. Do NOT harmonise |
| LRU (`0x100169B0`) | stamp compare is **unsigned** (`jb`), so a wrapped stamp sorts lowest and is re-picked immediately; ties go to the **later** index |
| `0x10010BF0` | writes the Z output FIRST (it is the depth guard); on rejection **nothing** is written, so callers can only tell kept-vs-replaced via the flags array. Both guards reject on NaN |
| `0x106C0860` | is a `BrMat4` (confirmed: passed to `BrMat4Scale`), row-vector convention |
| scratch-in-arg-slot | several functions write scratch into their own incoming stack args (`0x10010D10`, `0x10010BF0`, `0x10030810`, `0x1003D180`). Not out-parameters |
| `BrVec3Div` | one reciprocal then 3 muls, not 3 divides; differs in low bits |
| `BrMat4Copy` | **source first**, unlike every dest-first vector routine |
| `BrSpanAdd` | clamps out-of-range input rather than rejecting it |
| `BrSegFixup` | below-base pointers become **0**, not passed through |
| `BrSpanReset` | **CORRECTED**: emptiness markers are **64 and 0**, not INT_MAX/INT_MIN; and `0x1003A4D0` never touches the grid at all (it is cleared by `0x1003A990`). Both errors fixed |
| `BrAtan2` | takes **x first**, range `[0,2pi)`, and is a 16-step bisection with a 0.005 early-out -- accuracy ~0.01 rad. Do **not** substitute `atan2f` |
| `BrMtxInvert` | returns 1 on **both** exits (success and identity fallback) -- useless as a status. Do not fix |
| three normalisers | `0x1003AE50` maps zero to `(0,0,1)`; `0x10074180` has **no guard** (NaN); `BrVec3dNormalise` leaves zero untouched. Same operation, three behaviours |
| `car+0x10AC` | is a **struct-of-arrays** -- cursor advances 4 bytes per wheel while fields sit at +0x00/+0x10/+0x20/+0x30. Reading it as a record is silently wrong |
| wheel index order | `{0x994, 0x57C, 0x370, 0x788}` -- **not** address order |
| `BrPoolAlloc` | never returns NULL; past 256/frame every caller aliases one slot |
| `BrVec3d` | separate DOUBLE-precision vector type -- do not fold into `BrVec3` |
| `BrVec3dDot` | sums `(z*z + y*y) + x*x`; FP addition is not associative |
| `BrVec3dCross` | **output is the THIRD arg**, opposite to `BrVec3Cross` |
| `BrVec3dNormalise` | guards on `len != 0.0` exactly, no epsilon; tiny lengths still blow up |
| `BrPackNormalByte` | `+1.0` maps to 128 and clamps to 127 -- asymmetric by design |
| `BrMat4Frustum` | **7 args, no `scale`** -- stock libultra `guFrustumF` has 8 |
| `BrMat4Perspective` | writes a **hardcoded** perspNorm of 1; stock computes it from n/f |
| `BrMat4Perspective` | **DECLARED WRONG — takes 7 args, not 6**; the omitted trailing `scale` is present at both call sites. See adjudications |
| `BrSlotsReset` | empty marker is **-1**, so id 0 is a VALID occupied slot |
| **`BrSlotTable` layout** | **ERRATUM**: slots (`0x10AA2538`, 8x12B) and `count` (`0x10AA288C`) are **756 bytes apart**, not adjacent -- the struct is a fiction. Do not overlay it on the real globals |
| **`0x10AA288C` dual role** | it is BOTH `BrSlotTable::count` AND the DirectPlay **send gate** -- a non-empty slot table silently suppresses network tags 2/3/4/5. Tags 6/7/8 ignore it |
| `0x1000C4D0` | slice1_03 named this `BrComCallLocked68`; it is **`IDirectPlay4A::Send`** under a critical section, always `DPID_ALLPLAYERS` + `DPSEND_GUARANTEED` |
| option cyclers | `0x10AA33D4` steps **up**, `0x10AA33D0` steps **down** (33D4 wins if both) -- these are `a1`/`a0` in `br_state.h`, which only names them positionally |
| search cyclers | move even when nothing is selectable: the full-circle test compares the **already-stepped** value, so all-rejected lands on the first candidate, not where the user was |
| `0x10042880` | itoa scratch at frame+0x10 and the name at frame+0x14 -- **4 bytes apart**, so a four-digit index corrupts the prefix before it is read. Preserved |
| `0x10040330` | **real bug, preserved**: outer pass i zeroes flag[i] but the inner loop only visits j>i, so flags set on j are wiped when the outer loop reaches j. Conflict 0-vs-1 leaves flag[0]=1, flag[1]=0 |
| `0x1003FA00` | **saves the wrong field** -- saves obj+0x40, restores into obj+0x2F70, destroying that field's prior contents |
| `BrUiObj` | offsets **cannot** be reconciled into one struct: arrays at +0x2B5C and +0x3C98 both stride 0x438, but their difference (0x113C) is not a multiple of 0x438. Modelled as offset constants, not a struct -- do not "fix" this |
| `0x1003E7A0` | loop guard is unsigned `jbe` over a signed count; `W40A <= -32` makes the original spin ~2^32 times. Preserved |
| `0x1003BD50` RNG | borrows the 16807 multiplier but the modulus is **2^27**, not 2^31-1. Do NOT substitute a stock minimal-standard PRNG. Seed 0 is absorbing |
| SP GUID table | GUID order maps to rows **0,1,3,2** -- rows 2 and 3 are swapped vs address order. Regenerating from the GUID array gets it wrong |
| `0x1003CE80` tail | scans 0..31 inclusive; if nothing is available the index is left at its STARTING value with **no failure signal** |
| `0x1003D850` | **dead code** -- matches a GUID, walks a string list, discards everything, returns 1. Do not port |
| lap-time formatter | seconds via `ftol(centis * 0.01f)` and `0.01f` is `0.00999999977648`, so **exact multiples of 100 centis lose a second**: 1.000s prints `0:00.100`, 60.000s prints `0:59.100`. Verified. Real times are never exact multiples of 10ms, which is why it shipped. Invariant that DOES hold: `min*6000+sec*100+hundredths == trunc(t*100)` |
| `0x10041300` | looks the string id up twice and `_strupr`s the second result -- **uppercases the string-table entry itself, permanently** |
| `if (pItem + 0x2B65)` | an **interior pointer, never NULL** -- the guarded call always runs. Not a real null check |
| phase switcher | after the enter hook it **re-reads** the current-phase global for `+0x0C` and `+0x68`; a hook that re-points the phase gets the flags on ITS object. Caching that read changes behaviour |
| phase switcher | **three outcomes, not two** -- "already built" and "just built" both return 1 but run different epilogues; allocation failure returns **0** |
| `0x10AA2888` vs `0x10AA288C` | adjacent but **distinct** globals -- one is the phase flag, the other the slot-table count / DirectPlay send gate |
| `BrIsAnyActive` | 5th flag is **inverted** -- set forces 0, and suppresses only the flags after it |
| `BrSegSetBases` | arg order is (n64Base, hostBase); swapping them inverts every fixup |
| `BrObjClear` | clears +0x00..+0x0C only -- **+0x10 deliberately survives** |
| **br_obj naming** | **ERRATUM**: all four are members of one big-endian BIT STREAM class, not object accessors. `BrObjConsumeFlag` = *align read cursor to byte*. Behaviour correct, names wrong. Merge into slice1_09's stream API |
| bit stream | length arg lands in the **write cursor**, not a size field -- which is why "at end" means read cursor caught up to write cursor |
| bit stream | every BYTE-granular accessor silently aligns first, discarding a partial byte. `ReadBits` does **not**. All multi-byte is big-endian, MSB-first |
| `BrVec3Normalise` (0x10074250) | **NO zero-length guard**, unlike `BrVec3dNormalise` which has one. Do not unify them |
| `0x100747C0` transform | applies the upper 3x3 **TRANSPOSED** (like `MulVec3Transposed`, not `MulVec3`) plus the fourth ROW as translation. `pOut` must not alias `pV` |
| entity array | stride is **0x2B68** (11112 bytes), confirmed twice: magic divide and two caller base addresses differing by exactly that |
| `0x10074F70` ring | no read cursor, no fullness check -- the 257th push overwrites slot 0 |
| `BrMat4Mul` (0x100306C0) | destination is **LAST** -- `(a, b, out)`. Its two paths sum the four products in **different orders**; both preserved. Null `pOut` writes through address 0 |
| `BrVtxCacheResolve` | key is the (pointer, count) PAIR but a miss byte-swaps the source **in place** -- resolving the same memory twice under different counts corrupts it |
| `BrF3DVtxFixup` | a zero `w1` does **not** stay null: the segment prefix merges in before the fixup, so `BrSegFixup`'s null rule is unreachable from here |
| `BrPeerFind` | returns 0, -1 and 1..15 with **three different meanings**; record 0 is reserved and never scanned. Treating 0 as "not found" is wrong |
| `0x10008B80` / `0x100378A0` | **stubs in the shipped DLL** -- a bare `ret` and a flag-set. Called with 1/3/6 args that are all ignored. Their intended semantics are unrecoverable from this binary |
| `BrRdpACMux` vs `CCMux` | `1013` (LOD_FRACTION) maps to **0** in alpha but **13** in colour -- the one asymmetric token |
| `BR_VTX_NORMAL_SCALE` | `0x1008F440` = 1/128 exactly, the inverse of `BrPackNormalByte`'s 128.0 -- they are an encode/decode pair |
| `BrObjInitInline` | +0x10 aims at storage INSIDE the object; never free it, re-point on copy |
| `BrHandleLookup` | valid range is **1..0x12E**; handle 0 is reserved null, not index 0 |
| `BrHandleLookup` sig | **CORRECTED**: original takes ONE arg (the handle); the table is hardcoded. Our table parameter is an undocumented-until-now DEVIATION. Observed handles are string ids -> likely a string table |
| `0x10060030` error box | **silently drops the error code** -- every call site passes the failing HRESULT and it is never loaded. Also caption/text are the inverse of how call sites read |
| `0x1006C740` | 2D test, not 3D: the dominant axis is projected out and never re-examined, so a point far off the plane still reports inside. Axis ties pick axis **2** |
| `0x1006C740` barycentric | `u` weights **C** and `v` weights **B** -- reversed from the obvious reading. Returns **AX only** |
| tint blit | colour key is `R==0 && G==B`; alpha byte copied through untouched; source stride is `(x1-x0)*4`, **not** `x1*4`, and x0/y0 apply to the DESTINATION only |
| `0x10063420` | mixes a clamped and an **unclamped** index into one 24-byte-record table -- values >3 read out of bounds. Looks like a real bug |
| `0x1006ABA0`/`0x1006AE00` | despite matching x24 / /24 factors these touch **different globals** (`0x10B502E8` vs `0x10B502EC`) -- NOT a getter/setter pair |
| race entrants | 16 objects of stride **0x2B68** at `0x10ACDEA8` + parallel 16 of stride `0x15C`; index cached at `+0x140`. For index < 2 only, `+0xE8C` points at a 332-byte sub-block, else NULL -- always check |
| sound return codes | **INVERTED WITHIN ONE MODULE**: `0x10072820` returns 1=success, everything else returns an HRESULT where 0=success. `0x10072A90` consumes both. Getting either backwards silently inverts playback |
| disabled sound | all three gates zero => returns **success**; a NULL voice returns **failure**. Absent hardware is success, bad argument is not |
| `0x100722D0` | on the Unlock-failed path `mov esi,eax` overwrites the saved HRESULT with the retry's result -- a genuinely failed create can report success with a NULL buffer. Reproduced |
| sound Unlock | passed `nDataBytes`, **not** the count Lock reported -- a short lock would overrun the copy |
| sound neutral point | **400** is neutral for both pan and volume; engine does no clamping, relies on DirectSound |
| chain walkers | `0x10072BF0`/`0x10072C20` **skip the head** -- they start at `pHead->pNext`; the head is never stopped or freed |
| `0x1006F720` cache | compares a **zero**-extended u16 against a **sign**-extended i16, so negative-x/y cells never hit and rebuild every frame. Real bug. Also no bound check on a 150-entry cell |
| ground probe `0x1006F0C0` | rejects unless `\|dot(N,D)\| > 0.001`, `t < 2.0`, `t < best`, and **`N.z > 0.2`** (upward faces only); `t` initialised to `100.0f` |
| `BrSwapVec3` | swaps exactly 3 u32s (unrolled); it is not a general byteswap |
| `BrFChkFRead`/`BrChkFRead` | take **`FILE **`, not `FILE *` — a `FILE *` compiles and reads garbage |
| `BrAdler32` | `buf==NULL` returns 1 ignoring the seed; `len==0` returns the seed unchanged. Not interchangeable |
| `CHK_*` family | all call **`exit(1)`** on failure — not recoverable, callers get no error return |
| `BrChkRealloc` | checks size AFTER realloc; `(NULL,0)` allocates then drops the pointer (leak, preserved) |
| `BrChkAlloc(0)` | returns NULL quietly — zero size falls through as the return value, not an OOM abort |
| `BrTicks30FromMs` | **not** `ms*30/1000`; it is `3*(ms/100) + min((ms%100)/33, 3)`, so 99ms and 100ms both give tick 3 (verified). Duplicated inline at `0x10075100` — keep in sync |
| `BrCdVolumeScale` | masks to 8 bits before scaling, so **256 is silence**, not full volume |
| `BrGrid64Sample` | `x` is column, `y` is row (`row<<6 + col`); rejection value 0 is indistinguishable from a real zero cell |
| `BrU16CursorNext` | returns **AX only**; and `pos+1` carries into bit 0 of the count, so `pos==0xFFFF` makes the count fail to decrement → caller loops forever |
| `test r,r / jbe` | `test` clears CF so `jbe` degenerates to `je` — an equality test, not a comparison. Appears in adler32 and likely elsewhere |
| clip vertex list | **CIRCULAR, and every clip call rotates it** — head ends one past where it started; on early exit it can point at a discarded vertex |
| clip loop exits | `cVerts < 2` is tested BEFORE `--i <= 0`, so a fully-rejected triangle exits with `cVerts == 1`, not 0. Callers must test `< 2`, never `== 0` |
| clip lerp args | always passed **outside-first**, which appears as *opposite* operand order on the two splice paths. Do not "harmonise" |
| clip node pool | only addresses inside `0x104C01A8..0x104C0BA8` (64 records x 0x28) are recycled; foreign vertices are silently dropped but still consume a pool node — the free list drains one-way |
| clip NaN | NaN clips as **OUTSIDE** (branch on x87 C0, set for unordered). Port as `>= 0.0f`, never `!(< 0.0f)` — the inverse spelling flips it |
| `BrTextDraw` align | case 0 **falls through** to default, so any align other than 0/1/2 does not write x at all and reuses the previous call's x. Centring uses `sar` (arithmetic), so negatives round the other way |
| time formatting | truncates toward zero in all four divisions; negatives yield negative fields (`-1.5f` -> `0:-1.-50`) and minutes never wrap into hours (3661.25s -> `61:01.25`) |
| POD `0x10008C60` | read helper takes **(FILE*, buf, n)** — file FIRST, unlike stdio |
| POD `BrFatal` | formats the message then **never prints or stores it** — leaked, then `exit(1)`. Every POD diagnostic in the original is silently discarded |
| `0x1000BEA0` dispatch | two tables collapse to one `if`: only message id **0x107** (and id 5) reach real code. Forwarding order is `f0C, f10, f08` — **not** ascending |

### Identified but NOT yet decompiled

| Address | Size | Identification | Evidence |
|---|---|---|---|
| ~~`0x100309A0`~~ | 427 | **DONE** -- `guLookAtF`, but NOT stock libultra | subtracts one 3-float point from another (eye-at), calls `0x10030600` (`BrVec3dNormalise`), then `0x10030640` (`BrVec3dDot`) against a third vector, followed by a Gram-Schmidt `fmul`/`fsubr` pattern. Args look like `(mtx, eye[3], at[3], up[3])` at `[esp+0x50..0x74]`. |

Structural note worth keeping: `guLookAt` takes **float** arguments but spills
them to `qword` and performs the entire construction in **double** precision.
That is why the engine carries a separate `BrVec3d` type -- the precision
matters in the view-matrix path, and folding it into `BrVec3` would change
results.



| Address | Size | Identification | Evidence |
|---|---|---|---|
| `0x100309A0` | 427 | matrix compose | called by `0x10030E20` which then packs results via `BrPackNormalByte` |
| `0x1003D0B0` | 127 | DirectPlay buffer resize | `0x8877001E` = DPERR_BUFFERTOOSMALL, then GlobalAlloc/GlobalLock retry. **Platform layer, not core.** |
| `0x10004A10` | 60 | thread-safe field getter | WaitForSingleObject/ReleaseMutex around an array of stride `0x978`. **Needs a threading abstraction.** |

Deliberately not written yet: these are dense interleaved x87 or platform-coupled,
and a partial trace of them would produce plausible-but-wrong code. `guFrustumF`
in particular should be transcribed line by line rather than assumed to match
stock libultra -- Boss Game Studios may have modified it.

### Integration decisions (integration)

**`BrComVtbl`/`BrComObj` name collision — RESOLVED.** slice1_03 and slice1_06 both
defined these with different layouts; including both failed to compile. They are
genuinely different interfaces (03 = generic holder, 06 = DirectPlay, keyed by
`DPERR_BUFFERTOOSMALL 0x8877001E`), so slice1_06's were renamed to
`BrDPlayVtbl`/`BrDPlayObj`. Renamed, not merged -- merging would have been wrong.

**`br_f3d` microcode — CORRECTED to F3DEX.** The file decoded G_VTX with plain
F3D's layout (`n = (byte3+1)/16`). A later pass established the real format from the
game's OWN G_VTX fixup at `0x1002C150`, which extracts `n` as `(w0 >> 10) & 0x3F`
-- F3DEX packing. Verified: the two readings disagree on **57 of 151** G_VTX
commands in ce.rca, and F3D systematically undercounts (8 where the truth is 24,
16 where it is 32). Triangle decoding was unaffected. `bb.rca` went from 56 to 76
triangles after the fix. **OPEN:** `(v0+n)` reaches 63, so the vertex cache may be
64 entries, not 32 -- unsettled.

**`0x1007DFE0` — three independent readings disagreed. ADJUDICATED as `operator new`.**
- one reading: `operator new`, tail-calls `_nh_malloc(size, 1)`
- one reading: listed it as `malloc`
- one reading: called it `calloc(n, 1)`

The first reading wins on evidence: it read `_nh_malloc`'s body (the `__newmode` global at
`0x118AC344`, the 0->1 size clamp, the `>0xFFFFFFE0` rejection, the `_callnewh`
retry) and distinguished it from `0x1007D350` = `malloc`, which passes `__newmode`
where this passes a literal 1. **`calloc` is wrong and dangerous**: the second
argument is the new-handler flag, NOT an element count, so the result is NOT
zero-initialised. It was later noted this call appears 147 times in one function --
assuming zeroed memory there would be a real bug.

### INTEGRATION STATE — measured by an actual full link (not predicted)

A link of **all 46 modules** into one binary was attempted. Results:

- **0 duplicate definitions.** The predicted `BrG_<ADDR>` global collision did
  **not** materialise -- analysis passes used file-scope statics or address-suffixed
  names. My earlier warning about this was wrong; recorded so nobody spends
  effort on a non-problem.
- **304 undefined symbols** -- the real, measured integration debt. Full list in
  regenerable with `tools/linkqueue.py`. These are cross-slice references to functions
  that are declared (usually as `/* XSLICE */`) but implemented nowhere yet.

This supersedes an earlier crude grep-based audit that reported "115 phantom
declarations". That number counted any address not mentioned in a `.c` and was
an upper bound, not a finding. **Use the link, not the grep.**

`tools/linkqueue.py` regenerates the outstanding list: every name on it is a
function some module already calls but nothing defines yet.

**Naming debt: 53 addresses carry more than one function name** (e.g. `0x10032873`
is both `BrFrameBegin` and `BrFrameBeginRec`; `0x10035BBA` is `BrFatal`,
`BrLogEmit` and `BrLogSet`). This is heuristic -- it matches names to the nearest
address comment -- so treat it as an order-of-magnitude figure, not an exact list.

The round-3 contract added a "grep the address before naming" rule. It did not
work, and the reason is structural: analysis passes grep for the *address*, but the
collision happens when two analysis passes independently coin the same *name*, or coin
different names for one address they each reached from different call sites.
Address-grepping cannot catch either. A shared append-only name registry, or
integration reconciliation, is the only thing that will.

### Superseded note — global symbol collisions at final link

A later pass defines **101 file-scope globals** named `BrG_<ADDR>` (the convention
`slice1_07.h`/`slice1_08.h` set). Several of those addresses have 10-35 users
DLL-wide (`0x106C0680` alone has 35), so other analysis passes covering those ranges will
define the *same* objects.

**Today there is no duplicate**, but that is misleading: the build gives every
module its own test binary, so slice modules are never linked together. The
collision will surface only when everything is linked into one game binary.

Resolution when that happens: promote shared globals into a single owning
translation unit (`port/src/br_globals.c`) with `extern` declarations elsewhere.
Do NOT resolve it by renaming per-module -- that would create N copies of what
is one object in the original, and the aliasing is load-bearing (see the
`0x10AA288C` dual-role entry above).

Also outstanding: `0x10008B80` and `0x10042AF0` are called cdecl with **varying
arity**, which C99 cannot express in one prototype. A later pass declared one name
per observed arity (`BrStub8B80_0/_1i/_1p/_5i`). They all resolve to one original
address; unify at final link.

### Function-map errors found during analysis (cumulative)

`0x100331FF`, `0x100334D7`, `0x100312BB` are **not function entries** -- packets
starting there begin mid-instruction. A later pass also found **4 additional real
functions** hiding inside listed byte ranges (`0x1002C2B0`, `0x100311E4`,
`0x10031212`, `0x10031347`), i.e. the map both invents entries and misses them.
`0x1003289F` was flagged as suspect but is genuine (`BrScissorSet`).

Also confirmed: `0x10069A60` is really **10 bytes** (`mov ecx` + `jmp`, then five
`nop`s). The map records 30, swallowing the padding *and* `0x10069A70`, which is
absent from the map entirely. So the map's *sizes* are wrong too, not just its
entry set -- an over-long extent silently hides a following function.

Net: treat `config/functions.csv` as a good starting index, not ground truth.

### Adjudicated cross-slice conflicts (integration)

**`0x10575510`/`0x10575518` — float, NOT pointer. slice1_05 is wrong.**
`slice1_05.h` models these as `BrCursorPair { void *f10, *f18; }`. A later pass reads
them as floats throughout. Adjudicated in a later pass's favour on direct evidence:
`0x1002B134` is `fcomp dword ptr [0x10575518]`, an unambiguous float compare.
`0x10575510` is only ever written with `mov`, which is ambiguous (a float bit
pattern moves the same way), so it follows its partner. Fix slice1_05 at
integration; do not propagate the pointer typing.

**`BrMat4Perspective` takes SEVEN arguments, not six — my declaration is wrong.**
`br_mat.h` carries a self-flagged caveat asking for a call-site check. A later pass
did it: two call sites (`0x10033E83`, `0x10033F7E`) both clean up with
`add esp,0x1c` = 28 bytes = 7 args, and one uses literals that pin the order:
`(mf, perspNorm, 45.0f, 1.3333334f, 10.0f, 2000.0f, 1.0f)` -- stock
`guPerspectiveF` order **including the trailing `scale`** that br_mat.h omits.
NOTE: I could not reproduce the disassembly myself (that address is one of the
map's mid-function entries), so this rests on a later pass's reading plus the
literal argument values, which are self-consistent. Treat as high-confidence,
not proven-by-me.

### RESOLVED — canonical phase layout is now `port/include/br_phase.h`

`BrPhase_` there is the merged superset (promoted from slice3_32's `BrPhaseFull`,
which had already reconciled the destructor at `0x10048870` and the vtable at
`0x1008F700`). Verified to coexist with all four earlier partial models, so
nothing already compiled breaks. **New code uses br_phase.h; do not add a fifth
model.**

This makes the LP64 hazard concrete rather than theoretical:

    original (32-bit)   sizeof 0xC8 = 200   nPages@16  pCur@100  f68@104
    this host (LP64)    sizeof      = 304   nPages@28  pCur@192  f68@200

So byte offsets do NOT survive on a 64-bit host. `br_phase.h` therefore asserts
only what is portable -- field ORDER -- and supplies `BR_PHASE_ALLOC_SIZE` so
callers stop allocating the `0xC8` literal, which under-allocates by 104 bytes
here. Nothing may overlay this struct on a file image or foreign buffer.

### Original blocker description (kept for context)

Five menu-screen builders (`0x10049F40`, `0x1004D640`, `0x10056FF0`, `0x1004F700`,
`0x10053CF0`) have been declined **independently by five separate attempts**. Their
bodies are not the problem -- `slice3_33.c` has already decompiled the same family
five times. The blocker is a TYPE:

- `slice2_26.h` `BrPhase` and `slice2_25.h` `BrOptObj` model the 0xC8-byte object as
  `{pVtbl, pfn04, pfn08, f0C, f68}`.
- The builders need `+0x10` (uint16 screen count), `+0x14` (screen array) and
  `+0x6C` (parallel int array) -- which is `slice3_33.h`'s `BrUiPhase`, an
  incompatible layout for the same memory.
- `slice3_32.h` adds a third view, `BrPhaseFull`, with the full 0xC8 map recovered
  from the destructor and vtable: `+0x10` page count, `+0x12` index,
  `+0x14` `BrUiPage*[20]`, `+0x64` current page, `+0x68`, `+0x6C` `int32[20]`,
  `+0xBC` selection, `+0xC0`/`+0xC4` two refcounted objects.

**Casting `BrPhase*` -> `BrUiPhase*` links cleanly and is silently wrong at every
field access.** Merging on `slice3_32.h`'s `BrPhaseFull` layout (the best-evidenced
one) unblocks all five at once and retires three competing models.

Also settled by a later pass: the `pfn08` slot's argument is an **entity record**
(`+0x2AE8` is the sub-object), so `slice2_25.h`'s `void(*)(BrOptObj*)` typing is the
wrong shape; `slice2_26.h`'s `void(*)(void *pEntity)` is right.

### Analysis tooling

`tools/modules.py` -- topological work order (callees before callers), 513
shared leaves with no prerequisites, 1255 address-contiguous module clusters.
`tools/globals.py` -- 2569 referenced globals, 213 identified as arrays via
indexed-access scale; this is what unblocks the global-dependent core.

    ./build.sh
    ./build/test_pod testdata/BossRally.pod
    ./build/test_rca
    ./build/test_n64tex
    ./build/test_gfx testdata/splash.img build/out.ppm
    ./build/brview                    # windowed viewer, Esc to quit

### .rca car definitions

`RCar` magic, NUL-terminated name padded to 0x94, then a parameter block.
Established by diffing `ce.rca` ("TYPE-CE") against `bb.rca` ("Beach Ball"):
six descending floats at +0x9C are the forward gear ratios, identical for
gears 1-4 (3.23, 2.10, 1.46, 1.11) and differing only in the top two
(0.85/0.60 vs 0.90/0.65). Everything after that is exposed as raw indexed
words rather than guessed at; naming them needs the physics code decompiled.
Geometry and the embedded RGBA5551 textures are not yet decoded.

This is the handling data -- the most directly moddable thing found so far.

**Geometry is NOT yet decoded, and two hypotheses have been tested and
rejected.** Recorded so they are not retried:

1. *Float XYZ triples.* A scan appeared to find 6738 floats at 0x16C0, but the
   scan wrongly accepted `0.0` as float-like and had locked onto a large zero
   region. Excluding zeros, the entire file contains only 128 bytes of
   plausible float data -- far too little for a car mesh.
2. *N64-style s16 `Vtx` (16-byte: s16 x,y,z, flag, u,v, rgba).* The best
   "match" at 0x85D0 has every field of every vertex holding the same value
   (2313 across x, y, z, flag, u and v). It is uniform filler, and the
   coherence metric was fooled because near-constant data trivially has a
   small step between consecutive entries.

3. *Big-endian floats.* Also near-empty (160 bytes file-wide). Not the layout.

**CONFIRMED: the `.rca` payload is BIG-ENDIAN and byte-swapped at load time.**
From the parser `sub_100370D0`, which opens by assembling bytes in reverse:

    mov al,[esi+0x8001] / mov ah,[edi] / shl eax,8 / or eax,ecx ...   ; u32 swap
    mov dl,[esi+0x8004] / mov cl,[esi+0x8007] / mov [esi+0x8007],dl   ; 4<->7
    mov dl,[esi+0x8005] / mov cl,[esi+0x8006] / mov [esi+0x8006],dl   ; 5<->6

So `.rca` is N64 data shipped verbatim on the PC disc, like `.ci4`/`.lut4`.
Any future scan of this file MUST read big-endian. Note the little-endian
header fields (`RCar`, name, gear ratios) still parse correctly little-endian,
so the file is mixed -- only the payload from +0x8000 is swapped.

*Unconfirmed:* a big-endian s16 `Vtx`-style scan yields a candidate at
`0x00FB30` with car-scale coordinates, but `flag` and `uv` are zero on every
entry and the trailing bytes look like noise. That is also what a 16-byte
strided read over non-vertex data produces, so it is NOT treated as solved.

Lesson: statistical format-guessing works for simple raster formats (`.img`,
`.lut4`) and for parameter blocks validated by cross-file diffing, but not for
structured geometry. Read `sub_100370D0` (1641 bytes) instead.

### Located asset loaders (next decomp targets)

| Address | Size | References |
|---|---|---|
| `0x10037740` | 338 | `.rca`, `RCar`, `cars/` -- builds the path, parsing is downstream |
| `0x10007BD0` | 2541 | `cars/` |
| `0x100798F0` | 408 | `splash.img` |
| `0x1002F690` | 606 | `loading.img` |

The two `.img` loaders should settle the channel-order TODO above: they will
show which flag the original passes to select RGBA5551 vs ARGB1555.

### Pixel formats (three of them, do not conflate)

| Format | Alpha | Endian | Used by |
|---|---|---|---|
| ARGB1555 | bit 15 | little | `loading.img` |
| RGBA5551 | bit 0 | little | `splash.img` |
| RGBA5551 | bit 0 | **big** | `.lut4` palettes (N64 order) |

### .img format note

`.img` is a 12-byte header (`u32 width`, `u32 height`, `u32 format`) followed by
16-bit pixels. Every retail file reports format `0x1555`, but the files do **not
agree on channel order**: `splash.img` is RGBA5551 (alpha in bit 0) and
`loading.img` is ARGB1555 (alpha in bit 15). Each is unreadable under the other
interpretation, so the format word is not the discriminator and the original
must choose at the call site.

The loader detects it from the data, which is unambiguous because an alpha bit
is near-constant while a colour bit is not: 9759 of 9759 nonzero pixels in
splash.img have bit 0 set, and 51200 of 51200 pixels in loading.img have bit 15
set. A real colour LSB/MSB would sit near 50%. **TODO:** replace with the flag
the original passes, once the loader referencing those filenames is decompiled.

Deviations from the original are documented at each site in `port/src/br_pod.c`.
Two original bugs were deliberately *not* reproduced: bounds checks that
reported and then indexed anyway, and a name-length check that ran after the
copy it was meant to guard.

## Phase 1 acceptance: the game running on this Mac, renderer on Metal

### What the analysis says the port actually costs

`tools/apiboundary.py` aligns the two builds' call graphs to find every call site
where BRD3D and BRGlide diverge -- one logical operation, two implementations.
That set *is* the platform API:

| | functions | bytes |
|---|---|---|
| Shared game core (must be decompiled) | ~1,708 | ~340 KB |
| **Platform/renderer boundary (must be reimplemented)** | **73** | **41 KB** |

So the Metal port is the small half. The bulk of the work is the game core,
which is platform-agnostic already and needs no renderer knowledge.

Known false positive in that list: `0x1007C8A0` (`_ftol`, the CRT float-to-int
helper) survives because the two builds statically link different versions of
it. Anything else CRT-shaped near the top deserves the same suspicion.

### Architecture

"Platform agnostic" and "Metal" are not in tension: build a thin renderer
interface and make Metal its first backend. The original proves the seam is
real -- it already shipped two backends behind one core.

    port/src/core/      decompiled game logic, no platform types
    port/src/gfx/       renderer interface
    port/src/gfx/metal/ Metal backend (Objective-C)
    port/src/plat/      window, input, audio, file I/O

### Toolchain on this machine

- Metal device confirmed: **Apple M4 Pro**. `Metal`, `MetalKit`, `QuartzCore`,
  `AppKit`, `GameController`, `AudioToolbox` frameworks all present.
- The offline shader compiler is **not** installed (`xcodebuild
  -downloadComponent MetalToolchain`). Avoidable: compile shaders at runtime
  with `newLibraryWithSource:options:error:`. Not a blocker.
- `cmake` is not installed; currently building via a direct `clang` invocation.

### Route to something runnable

1. Platform skeleton: AppKit window + Metal device + swapchain, clear to colour.
2. Asset pipeline on top of the POD reader: `.img`, `.bmp`, `.ci4`/`.lut4`
   textures decoded and uploaded as Metal textures.
3. **Milestone -- asset viewer.** Draw real game textures and `.rca` car models
   with Metal. This derisks the entire renderer *before* the core decomp lands,
   and is the first point where something visible runs.
4. Implement the 73 boundary functions against the renderer interface.
5. Decompile the ~1,708-function core (the long pole).
6. Input (GameController), audio (AudioToolbox), then the game loop.

## Next steps

1. **Stand up the toolchain** (above) and prove the loop: compile one trivial function,
   diff against the original bytes.
2. **CRT boundary — now partly established.** `0x1007CC40` is `_cexit`, identified
   from its callee (`_doexit(0,0,1)`, atexit walk, `ExitProcess`). So the CRT starts at
   or below `0x1007CC40`, NOT `0x10078000+` as earlier guessed. Confirmed CRT, do not
   port: `0x1007CC40 _cexit`, `0x1007CD10/20 _lockexit/_unlockexit`, `0x1007CE90 fopen`,
   `0x1007CF00 getc`, `0x1007D350 malloc`, `0x1007DE40 operator delete` (511 call
   sites), `0x1007DFE0 operator new` (84 sites), `0x1007E170 _chkstk`,
   `0x1007ECB0 _aulldiv`, `0x1007ED20 _allmul`, `0x1007FD10 _alldiv`,
   `0x10086A10 __initmbctable`. Highest confirmed *game* code in that region:
   `0x10079550`.

   CORRECTION: an earlier note in this file called `0x1007DFE0` "malloc". It is
   `operator new` — both tail-call `_nh_malloc`, but malloc (`0x1007D350`) passes the
   `__newmode` global while operator new passes a literal 1.
3. **Recover globals.** 16,021 relocations in `.text` point into `.data`; clustering
   them by access pattern is how the ~25MB BSS layout gets reconstructed.
4. **Start decompiling** from the leaves — the pod/file I/O group (`LoadPod`, `ReadPod`,
   `GetPodLength`, `CHK_FReadOpen`) is well-named, self-contained, and a good first
   module.

## Caveats on generated data

- **Unaligned starts are NOT errors.** 547 of 2632 entries are not 4-aligned, but a
  linear-disassembly audit found **zero** straddled boundaries: every start sits on a
  real instruction boundary. MSVC does not 4-align every entry.
  Caveat: that audit proves *instruction* alignment, not that each entry is a whole
  function rather than a mid-function basic block. `0x1003289F` was flagged as a
  suspect case and is now **RESOLVED** -- it is a real function, `BrScissorSet`.
  Separately, two packet listings (`0x100331FF`, `0x100334D7`) genuinely begin
  mid-instruction, so those ARE scanner errors; both were correctly skipped.
- Function *boundaries* are inferred, not authoritative. Extents run from one detected
  start to the next; a missed start silently merges two functions. One such bug was
  found and fixed (thiscall `ret imm16` endings); others likely remain.
- The `d3d_only` / `shared` classification in `config/shared.csv` is a heuristic
  fingerprint match, not proof.

---

## Soundtrack

The retail PC game selects a music backend from `PlayMusic=` in `BossRally.ini`:
`0` off, `1` Redbook CD audio via WINMM MCI, `2` the EAR driver (`earpds.dll`).
**`2` is what is compiled into `.data`**, so a default install used EAR, not the
CD. `0x100027C0` is a two-way dispatcher on that value, NOT an enable check --
anything other than 1 takes the second backend and runs normally.

The two backends differ at end of track, and this is load-bearing:
**CD re-plays the same track; EAR advances with wraparound.** The port models
this as `BR_AUDIO_REPEAT_TRACK` / `BR_AUDIO_ADVANCE` rather than two code paths.

Other recovered behaviour: the front end plays track 2 once at init; races pick
randomly via `rand()*(nTracks-5)/32768+3`, which on the retail disc yields only
tracks 3-10; `Next` clamps where `NextWrap` wraps; volume is a 0-255 value from a
ten-entry table at `0x100ADF68` and callers gate on non-zero *before* playing, so
0 means "no music", not "silent music". `BrCdVolumeScale` masks to 8 bits first,
so 256 is silence.

The port plays neither Redbook nor XM. It plays FLAC produced locally:

    python tools/extract_cdaudio.py reference/brally/BossRally.cue        build/audio/cd
    python tools/extract_xm.py "reference/tgrally/Top Gear Rally (USA).z64" build/audio/n64

Both are idempotent; a re-run extracts nothing. The N64 modules are not raw in
the ROM -- they sit in a chunked-zlib container (big-endian `u32` total, `u32`
unpacked size, then length-prefixed zlib streams of <=16000 bytes, **2-byte**
aligned). `tools/xm_render.c` renders them, since ffmpeg here has no libopenmpt.

**Fidelity caveat on the XM path:** the renderer is from-scratch and validated
structurally (every effect used by all six modules is implemented; anything
outside that set makes extraction FAIL rather than write wrong audio) and
arithmetically (for the four modules without tempo changes, rendered length
matches tracker arithmetic exactly). It has NOT been A/B'd against FastTracker II
or a real N64 capture, so fine detail -- vibrato depth scaling, envelope edge
cases -- may differ. Sample playback uses linear interpolation; the N64 mixer
resampled, and point-sampling would bake aliasing permanently into a lossless
file.

## Asset policy: extracted at build time, never distributed

This repository contains **only our own source**. That goes beyond "do not commit
the disc image": no asset derived from the originals is ever a tracked file *or* a
distributed build artifact.

The build **extracts what it needs from originals the builder supplies**:

    reference/brally/BossRally.BIN + .cue   retail PC disc image  (you provide)
    reference/tgrally/*.z64                 Top Gear Rally ROM    (you provide)

A fresh clone plus your own legally-obtained copies is sufficient to produce
everything. Extraction tooling lives in `tools/`, is reusable and idempotent, and
is not a set of one-off scripts whose output someone ships.

This covers the soundtrack in particular. The retail PC game streams Redbook CD
audio and the N64 build plays XM modules; the port uses neither, playing locally
produced lossless files instead. Those are generated on your machine from your own
disc and ROM, and are gitignored.

IP hygiene and preservation both: we distribute our code, the originals stay with
whoever owns a copy.

## Getting the game data (not in this repository)

To build and run the tests you need a retail copy of Boss Rally (PC, 1999) and must
populate:

    orig/       BRD3D.dll, BRGlide.dll   (BRD3D.dll is the core decompilation
                target -- sha256 29af141ebd44bbcc79a9e58ca9cba62936792d6750c2e8b9
                df1a3805ae684b99. BRGlide.dll is the RENDERER reference; see the
                note at the top of this file.)
    testdata/   BossRally.pod, splash.img, loading.img, ce.rca, bb.rca,
                cargfx/skytexdesert.ci4 + .lut4

Without `testdata/` the tree still compiles; the suites that read retail files fail
at runtime, which is the intended signal rather than a silent pass.

NOTE TO FUTURE MAINTAINERS: this section was once added directly to the git
checkout and then silently destroyed by a `cp` of the working copy over it. Edit
the working copy, never the checkout.
