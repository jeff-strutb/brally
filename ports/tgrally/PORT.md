# Top Gear Rally: the native port

The N64 game (`n64/`, certified complete at M1) as a native 64-bit program,
built by any C compiler, with a thin platform layer under it. The same
principles as the Boss Rally port (`ports/brally/PORTABLE-CORE.md`):

1. **The original game first.** The core is the decomp's own code. What it
   draws, plays and computes is what the cartridge does; the platform layer
   only answers what the game asks of the N64's operating system and
   hardware. Enhancements come later and stay switchable.
2. **Platform agnostic.** Game logic is built once. Each OS supplies the
   window, input, audio out, timing and files behind one small interface
   (the Boss Rally port's `platform/host/host.h`, shared). Rendering goes
   through an interface neutral between graphics APIs.
3. **macOS first** (Metal). Windows and Linux follow on the same core.
4. **The user's own ROM, at build time.** Nothing derived from the cartridge is
   committed. The build takes what the port needs from the builder's
   `Top Gear Rally (USA)` ROM (`tools/assets.py`: the initialised data and
   every asset, ROM 0x70AB0 to the end; the header, boot and compiled code are
   left behind) and assembles it into the executable. A built game needs no
   ROM.

## How the core is made

`src/` and `include/` are a fork of `src/tgrally` and `src/tgrally/include/tgr` at the
commit in `src/FORKED-FROM`, with the port's changes on top (addresses, byte
order, the arena). The decomp keeps moving (M2 respells functions to match
byte for byte), so `tools/sync.py` merges its progress in: each `src/tgrally`
file changed since the fork commit is merged three ways (base: the fork
commit, theirs: `src/tgrally` at HEAD, ours: the port's file). Conflicts are
resolved by hand, keeping the decomp's new code and the port's conversions;
`sync.py --resolved` then runs `tools/errfix.py` and `tools/abicheck.py` over
the merged files and moves the stamp. After a sync, rebuild and run the
suite. `src/tgrally` itself is never edited for the port.

A function M2 restructures (arms swapped, blocks moved) is better rebuilt
than merged hunk by hunk: take the decomp's file and re-apply the port's
conversions, which can be read off as the diff from `src/tgrally` at the fork
commit to the port's file (`tools/segsync.py rebuild` does this per function,
keeping the port's text for every function the decomp left alone; `diff`
shows each rebuilt one against the port's, `check` lists what the port had
that the file lacks). Code that moved merges cleanly in its new place
without its conversions, so after any sync check that no line the port had
converted survives verbatim. Data the decomp now defines in a TU (an
initialised global, a function static) stays a declaration here:
`tools/globals.py` defines it in the arena and lifts its value from the ROM,
and `tools/staticlift.py` turns function statics into their address symbols.
gbi commands written as raw `words.w0`/`w1` stores need `tgr_wr32`.

Of the library code the decomp fenced, the pure parts the game calls come
along (libultra's `gu` matrix and trigonometry routines, zlib's inflater).
libultra's formatted output is in `platform/libc/xprintf.c`, because its
number formatting differs from the host's (`"%.0f"` of 0.7 is `0`); it is
checked against the ROM's own code by `tools/printfcheck.py`. The rest of
libultra (the OS, the I/O managers, the RCP) is what the platform layer
replaces.

## The memory model

The game is ILP32 big-endian; the port is LP64 little-endian.

**One arena at the original addresses.** All of the game's memory is one
8 MB block, `tgr_rdram`, laid out as the N64's RAM. Every `.data` and `.bss`
symbol is an assembler alias into it at its original address
(`build/tgrally/null-null/gen/arena.s`, from `tools/globals.py`), so every 32-bit
address the game holds (in display lists, segment tables, loaded track and
car data, its own structures) is exactly the value the cartridge would hold.
`tgr_addr.h` translates: `TGR_PTR(T, a)` an address to a native pointer,
`tgr_addr32(p)` back, `TGR_FN`/`tgr_fnaddr` for the code addresses the game
stores. Pointer fields in game memory are `TgrAddr` (4 bytes).

**Pointer variables and tables are native.** A standalone pointer variable,
or a table of pointers in `.data`, is a native object outside the arena
(`tgr_natives`); its ROM values become native pointers at start-up, and an
address the game stores for one maps back to the original's address (a page
map in `tgr_addr.h` sends those addresses to the native table).

**Byte order.** Scalars the CPU uses are native: at start-up the ROM's
`.data` is copied into the arena and every symbol's multi-byte units swapped,
by the layouts `tools/globals.py` reads from the source with libclang (each
byte once; objects reached only through a pointer table are lifted with the
table's pointee type). What the RSP reads (`Gfx`, `Vtx`, `Mtx`, `Vp`, lights,
textures, palettes) and what is loaded from the cartridge (track, models,
animations, car models) stays big-endian, typed `be16_t`, `be32_t`, `bef_t`
and read through `BE16`, `BE32`, `BEF`, `BEPTR`, `SET32` and friends
(`tgr_core.h`). The compiler rejects mixing the two.

**Conversions as the N64 did them.** A float converted to an unsigned type
goes through `tgr_f2u`/`tgr_f2ull` (out of range gives all ones, as IDO's code
and libultra's `__d_to_ull` give; the host clamps). Apple arm64 packs stack
arguments by their declared size, so every declaration of a function matches
its definition exactly (`tools/abicheck.py`, `tools/protofix.py`).

## The platform layer

| | |
|---|---|
| `platform/os/` | libultra's API, natively: threads (one runs at a time, scheduled at OS calls, as the VR4300 ran them), message queues and events, the video retrace, PI DMA from the cartridge's data built into the executable, the controllers and Controller Pak, timers, the audio interface, RCP tasks |
| `platform/gfx/` | the RSP and RDP: an F3DEX 1.21 display-list interpreter (transform, lighting, clipping, the texture loads into TMEM) feeding a renderer with the RDP's combiner and blender state |
| `platform/render/` | renderers behind `rdr.h`: Metal (draws at the window's resolution: the frame scaled to the largest area of its shape the window holds, the N64's 4:3 or the whole window when a race fills it, `TGR_SCALE` without a window; 4x multisampling stands in for the RDP's coverage, alpha to coverage for texture edges, the VI's gamma), soft (the N64's own resolution, the reference: 8-sample coverage kept per pixel, 5-bit colour dithered as the RDP dithers, and the VI's anti-aliasing, dither filter, divot filter and gamma on each finished frame), null (headless) |
| `platform/audio/` | the game's software mixer (`mixer.s` in the decomp) in C, and the audio interface's buffers out to the host |
| host | `ports/brally/platform/host/` (window, input, audio out, time, files) |

## Playing

`package_app.sh [--rom FILE]` builds `Top Gear Rally.app` (Metal). The ROM
(`--rom`, else `$TGR_ROM`, else `reference/tgrally/Top Gear Rally (USA).z64`;
any byte order, checked by its SHA-1) is read only while building; the app is
one executable with the game's data inside and runs anywhere without it.
Saves are a Controller Pak in port 1, kept in
`~/Library/Application Support/Top Gear Rally/controller-pak-1.bin`.

| N64 | game controller | keyboard |
|---|---|---|
| stick | left stick (to the N64's octagonal gate: 80 at the cardinal points, 70 on each axis at the diagonals) | arrows |
| A, B | A, B | X or Return, Z |
| C buttons | right stick; X is C-left, Y is C-up | I J K L |
| Z | left trigger | Space |
| L, R | shoulders (right trigger is R too) | Q, W or E |
| START | menu | Escape or P |
| D-pad | d-pad | |
| (Original / Remastered) | View | Tab |

Two profiles, as the Boss Rally port has (`platform/include/tgr_view.h`):
**Remastered** (the default) and **Original**, switched in play with Tab or the
controller's View button (the one button the N64 pad has no use for);
`TGR_PROFILE=original` starts in the other, `TGR_FLAG_<NAME>=0|1` overrides one
flag. Remastered lets the window take any shape: a race then fills it at the
window's own resolution, each view's lens widened to the window's shape (Hor+
when wider than 4:3, Vert+ when taller) and its culling wedge with it
(`drawing/frameloop.c`, only for the race's views, `racing/racetick.c`); the
rear-view mirror keeps its shape at the top centre, and the HUD keeps its
shape at the edges it sat by, each gauge or line of text moved as one
(`gfx/rcp.c`, the Boss Rally port's placement). Menus, the car select and the
paint shop stay 4:3, centred. Original keeps the window and the picture at
the N64's 4:3. View > Enter Full Screen (Ctrl-Cmd-F). In the background the
game is paused: the N64's clock stops. Without a window, `TGR_WINDOW=WxH`
stands in for one (screenshots).

**iPhone.** `ios/build.sh [--rom FILE] [--bin FILE] [--sim] [--install [--device ID]]`
builds the game as `libtgrally.a` (`link.sh` with `HOST=ios`, the same Metal
renderer) and Boss Rally's engine as `libbrally.a` (`ports/brally/link64.sh`
with `HOST=ios`; its data track from the builder's disc image goes in the app
as `Resources/disc`), each linked into one object that shows only its entry
points (the two share some hundred function names), under a Swift host
(`ios/Sources`: the window, touch, Core Audio, the save folders),
generates the Xcode project with xcodegen into `build/tgrally/ios-metal`
and signs it (`--sim`: a Simulator build; `--install` puts it on the first
paired iPhone). Landscape only; the game draws into a 16:9 area centred on the
screen (Remastered: a race fills it, menus keep 4:3 inside it). Touch:

| | |
|---|---|
| in a race | a finger right of the brake zone holds the accelerator; sideways steers (full lock 90 points either way, the neutral point sliding with the finger past it), a wheel drawn under the finger turning with it; lift to coast. A finger on the left 30% of the screen (a pedal is drawn there) brakes and, the car stopped, reverses; it lets the accelerator go while held. In Boss Rally's races it is that game's reverse (the gamepad's button 2), which drives backwards at any speed, slowing the car first; in this game's own it is B until the car is below 3 km/h (`tgr_race_speed`), then A with the stick pulled back (this game reverses only from first gear) |
| menus, pause menu | tap an item: the carousel's arrows step it, the item between them is A, a list row moves the cursor there and chooses it, the button prompts (Select, Go Back, OK, Cancel, Exit) press their button; a tap on a screen with nothing to choose is A. Swipe: the content follows the finger (left brings on the next item, up the row below); a value row (a volume) goes up with the finger |
| anywhere | two-finger tap: START; three-finger tap: the sound's low-pass filter, off / 8 / 7 / 6 / 5 kHz (5 at first, kept between runs; `TGR_AUDIO_LOWPASS=Hz` on the desktop) |

The menus name their tap targets as they draw (port lines in `src/`, `platform/os/touch.c`);
`TGR_TOUCHLOG=1` logs each tap and what it met.

**Boss Rally's races** (`TGR_FLAG_BR_RACES`, Remastered; the iPhone app is
the first host to carry it). The menus are Top Gear Rally's; each race the
player starts is run by Boss Rally's engine, whose tracks are the N64 track
images (`host_race.h`). At `BrRaceTick`'s first call `src/racing/handoff.c`
hands over the race the menus chose (the courses numbered alike, mirrors at
+5 here and +6 there; the car, the weather, the tires, the suspension and
the transmission alike in both; Boss Rally has no handling choice), and
waits; `ports/brally/platform/common/race_handoff.c` starts it from Boss
Rally's menus the way its attract demo does, the countdown at once, and
leaves the replay. The outcome comes back as the race's own end leaves it
(place, total, each lap, the best lap, the lap and course records) and
through `BrRaceResultSave`/`BrRaceResultRestore` to the results screen, so
a championship round's points, place, the season and its unlocks follow as
from the N64's race. Not handed over: two players (Boss Rally races one
locally), the demos, the season-end ceremony. Not carried back: the instant
replay and a Time Attack ghost (each an input recording for its own
engine's physics). Boss Rally boots at the first race and waits at its
menus between races; while it races it has the screen, the sound, the
events and the touch (its pause menu reads the keyboard: a swipe is an
arrow key, a tap Return, two fingers Escape), its "quit game" ends only the
race. The music is this game's: the track's own (`BrMusicLoadTrack`), its
mixer playing on while the game thread waits on a message queue for the
race (the other engine runs on a host thread); the host mixes it with Boss
Rally's effects (`ios/Sources/Host.swift`, those taken from 44.1 kHz to this
game's 48 kHz). Boss Rally's CD music is not in the app.
Checks: `BR_HANDOFF_TEST=mode,track,mirrored,weather,laps,car[;...]` runs
Boss Rally's races alone (headless: `build/brally/null-null/brally64`, the
autopilot driving; in the app it starts in front), `BR_HANDOFF_AUTOPILOT=1`
lets the autopilot drive the races Top Gear Rally hands over, and
`TGR_HANDOFF_FAKE=place,total,best` stands in for the other engine here.

Time is the console's: 60 retraces a second against the wall clock, the
N64's count advancing exactly a retrace's worth (781,250) each, and audio
played as the audio interface reads it from RAM (the game mixes into one ring
only 46 ms ahead of the DMA, so a buffer is not final when it is queued),
60 ms behind, trimmed by up to 0.2% to the output device's clock. `TGR_STATS=1` reports, each second, the
retraces and the N64 time they covered, frames drawn, the worst gap between
retraces, the time spent presenting and the audio buffered.

## Verification

`tools/tgrally/n64box.py` runs the original ROM headless (the CPU under Unicorn,
the OS modelled exactly as the platform layer implements it: one game
thread at a time, one virtual clock, one scripted input timeline). The
port's tools use it through `tools/tgrbox.py`, which corrects its model of
`__d_to_ull` to the ROM's and leaves a non-CI tile's palette field (never read
by the RDP, stack residue in `BrTexLoad`) out of the display-list digest.

| tool | |
|---|---|
| `tools/suite.py` | every `tools/tgrally/n64box_scripts` script through `streamcmp.py`, in parallel, on a snapshot of the binary; the original's streams are cached per script (`build/tgrally/null-null/boxcache`), so a full suite takes seconds |
| `tools/streamcmp.py` | one script: every display list (with the data it names), frame-buffer swap and audio buffer of the port against the original, first difference reported |
| `tools/lockstep.py` | the game's state and display list at the k-th graphics task, symbol by symbol and command by command |
| `tools/fncheck.py` | the game's state at the k-th call of any game function |
| `tools/watch.py` | which of the original's instructions write an address |

`BrPaintDashRect` reads an uninitialised byte (`c`, at sp+0x59 in the ROM)
when a dashed rectangle is under four pixels wide, and gets whatever the last
function to reach that stack address left there. The port keeps that byte
(`tgr_dash_slot`, paintshop.c) and stores it where the ROM does: the paint
shop's setup, `BrPaintFillRect`, the style menus' panels and `BrPakMessage`
(each saves a word whose second byte lands there). Every script is identical
to the original: display lists, swaps and audio.
