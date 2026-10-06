# ports/: derived platform layers (NOT byte-matched)

This tree is downstream of the decomp. The decomp (the repo root) is the
master: the portable game logic in `src/brally/core/` plus the byte-matched original
**Win9x** platform layer (`src/brally/backends/{glide,d3d,win32}`, `src/brally/exe/`), all
verified bit-exact against the shipped 1999 binaries.

A **port** takes that master's portable core and supplies a *new* platform
layer for a different target. Port code is written fresh, uses the target's own
system libraries and graphics API, and is **not** byte-matched: there is no
original binary for a port to reproduce. So nothing here carries `@implements`,
appears in the match reports, or is touched by the byte-exact tooling
(`tools/brally/`, `config/brally/` maps and fences, the image assembler). That machinery
belongs to the decomp; a port has no bytes to match, so it has no use for it.

The one axis that *is* shared between the Win9x original and a port is the
platform-abstraction seam: where the original build links Microsoft's CRT
(`/ML`, `/MT`, `/MD`: see `config/brally/binaries.csv`), a macOS build links the
Mac's libc; where the original draws through Glide/Direct3D, a macOS build
draws through Metal. Same slot in the design, different platform.

## Music: decided 2026-09-29

The 32-bit lane plays both soundtracks natively (`brally-wasm/wasm/native/music.m`):
the disc's CD audio through AVAudioEngine, and the N64 modules live through
libopenmpt, linked statically. Tab switches between them live
(`brally-wasm/wasm/native/version.m`). The game's own music logic still decides what plays when; only its Windows
backends (MCI and the EAR engine's CD channel) are replaced. The findings the
choice was made on, and the rules that still hold (the exports stay 1:1 rips),
are in `docs/brally/port/music-decision.md`. The native 64-bit lane (`build.sh`) still
has no music.

## The portable 64-bit core (planned)

The 32-bit lane is to be replaced by one native 64-bit build of the core
for macOS, Linux and Windows, each OS supplying only its platform backends.
The plan and what stands in the way are in `common/PORTABLE-CORE.md`;
`common/tools/lp64audit.py` measures it.

## Sound effects

The 32-bit lane plays the game's effects natively too. The game still drives
DirectSound (one static buffer per sample: volume, pan, pitch, play, stop);
`brally-wasm/wasm/host/host_dx.c` mixes those buffers with DirectSound's gain and
pan laws and `brally-wasm/wasm/native/sound.m` sends the mix to AVAudioEngine at
the device rate, through a peak limiter. Muted while the app is inactive, as
DirectSound mutes a window without focus. `BR_SFX=0` silences it;
`BR_SFXWAV=path` records it (headless runs render it offline, in step with
the game's clock).

## brally-wasm/

The macOS/Metal port. `metal/br_gfx_metal.m` is the graphics backend (the peer
of the win9x `glide`/`d3d` backends, but new code, not decomp). Built by
`build.sh` with clang.

### Boss Rally.app (the 32-bit lane, self-contained)

```sh
ports/brally-wasm/wasm/package_app.sh [--bin BossRally.BIN] [--rom "Top Gear Rally (USA).z64"]
```

builds the lane (`wasm/build_wasm.sh`) and writes `build/brally/wasm32/app/Boss Rally.app`,
which carries everything it reads: the whole data track as its CD root, the
PC soundtrack (CD audio off the BIN/CUE) and the N64 soundtrack (off the ROM).
The app needs neither image nor this tree once built; saves go to
`~/Library/Application Support/Boss Rally`. Extraction happens once per set of
sources (cached in `build/brally/wasm32/app/extract`, keyed on their MD5s). The N64
soundtrack is bundled as the ROM's modules (about 1 MB), not rendered audio.

### Where the port's code lives

`src/brally/` and `src/brally/include/` are exactly what MSVC 5.0 compiles for the byte-matched
build: no `BR_MATCHING_BUILD` or `_MSC_VER` conditionals, no port arms
(the pre-commit hook refuses one). Everything the Mac port does differently is
here:

| path | what |
|---|---|
| `brally-wasm/patch/<path>.port` | a **spec** per decomp module or header the port compiles differently: the top-level items it drops (by name: `fn:BrFontLoad`, `var:DAT_...`, `define:...`) and the port bodies it compiles in their place. `tools/brally/portgen.py` documents the format. |
| `brally-wasm/core/<dir>/<file>.c` | port-only modules (audio engine, mixer, image loader, boot stubs, ...), under the same responsibility layout as `src/brally/core/` |
| `brally-wasm/legacy/sliceN_MM.c` | the BRD3D-era transcription the port still runs on, by address batch |
| `brally-wasm/include/` | port-only headers |
| `brally-wasm/tools/` | `portgen.py` (applies the specs; `build.sh` runs it first), `portsplit.py` (turns a file's conditionals into a spec), `portpp.py` (proves a change leaves every port TU's tokens unchanged) |

`build.sh` runs `portgen.py`, which writes the port's copy of each spec'd
module to `build/brally/macos-legacy/port/src/...` and the port's view of every header to
`build/brally/macos-legacy/port/include/`: the only header directory the port compiles against.
A module with no spec compiles straight from `src/brally/`.

Specs name items, not text, so the decomp can keep re-spelling a body the port
drops without touching the port. When an item a spec names is renamed or
deleted, `portgen.py` stops the build and says which key went stale. When a
new decomp body will not compile for the port, add `@drop <key>` for it (and
an `@after` block if the port needs its own version).
