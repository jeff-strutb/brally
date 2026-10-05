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
4. **The user's own ROM.** Nothing derived from the cartridge is committed or
   compiled in. The game's initial data and every asset are read from the
   user's `Top Gear Rally (USA).z64` at run time.

## How the core is made

`src/` and `include/` are a fork of `n64/src` and `n64/include/tgr` at the
commit in `src/FORKED-FROM`, with the port's changes on top (addresses, byte
order, the arena). The decomp keeps moving (M2 respells functions to match
byte for byte), so `tools/sync.py` merges its progress in: each `n64/src`
file changed since the fork commit is merged three ways (base: the fork
commit, theirs: `n64/src` at HEAD, ours: the port's file). Conflicts are
resolved by hand, keeping the decomp's new code and the port's conversions;
`sync.py --resolved` then runs `tools/errfix.py` and `tools/abicheck.py` over
the merged files and moves the stamp. After a sync, rebuild and run the
suite. `n64/src` itself is never edited for the port.

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
(`build/tgrally/gen/arena.s`, from `tools/globals.py`), so every 32-bit
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
| `platform/os/` | libultra's API, natively: threads (one runs at a time, scheduled at OS calls, as the VR4300 ran them), message queues and events, the video retrace, PI DMA from the ROM file, the controllers and Controller Pak, timers, the audio interface, RCP tasks |
| `platform/gfx/` | the RSP and RDP: an F3DEX 1.21 display-list interpreter (transform, lighting, clipping, the texture loads into TMEM) feeding a renderer with the RDP's combiner and blender state |
| `platform/render/` | renderers behind `rdr.h`: Metal (draws at the window's resolution: the N64's frame scaled to the window's 4:3 area, `TGR_SCALE` without a window; 4x multisampling stands in for the RDP's coverage, alpha to coverage for texture edges, the VI's gamma), soft (the N64's own resolution, the reference: 8-sample coverage kept per pixel, 5-bit colour dithered as the RDP dithers, and the VI's anti-aliasing, dither filter, divot filter and gamma on each finished frame), null (headless) |
| `platform/audio/` | the game's software mixer (`mixer.s` in the decomp) in C, and the audio interface's buffers out to the host |
| host | `ports/brally/platform/host/` (window, input, audio out, time, files) |

## Verification

`n64/tools/n64box.py` runs the original ROM headless (the CPU under Unicorn,
the OS modelled exactly as the platform layer implements it: one game
thread at a time, one virtual clock, one scripted input timeline). The
port's tools use it through `tools/tgrbox.py`, which corrects its model of
`__d_to_ull` to the ROM's and leaves a non-CI tile's palette field (never read
by the RDP, stack residue in `BrTexLoad`) out of the display-list digest.

| tool | |
|---|---|
| `tools/suite.py` | every `n64/tools/n64box_scripts` script through `streamcmp.py`, in parallel, on a snapshot of the binary; the original's streams are cached per script (`build/tgrally/boxcache`), so a full suite takes seconds |
| `tools/streamcmp.py` | one script: every display list (with the data it names), frame-buffer swap and audio buffer of the port against the original, first difference reported |
| `tools/lockstep.py` | the game's state and display list at the k-th graphics task, symbol by symbol and command by command |
| `tools/fncheck.py` | the game's state at the k-th call of any game function |
| `tools/watch.py` | which of the original's instructions write an address |

Known difference: `BrPaintDashRect` reads an uninitialised byte (`c`, at
sp+0x59 in the ROM) when a dashed rectangle is under four pixels wide; the
ROM gets whatever earlier functions left at that stack address. The port
models the function's own spill only, so the paint shop scripts that draw
such a rectangle after another function wrote that byte differ in one dash's
colour (`paint_shapes`, `paint_walk`, `paint_walk2`). Every other script is
identical to the original, display lists, swaps and audio.
