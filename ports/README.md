# ports/: derived platform layers (NOT byte-matched)

This tree is downstream of the decompilations. The decomps (`src/brally/`,
`src/tgrally/`) are the masters, verified against the shipped binaries. A
**port** runs that game logic on a new platform with a new platform layer
written fresh against the target's own libraries and graphics API. Port code
is **not** byte-matched: there is no original binary for a port to reproduce,
so nothing here carries `@implements`, appears in the match reports, or is
touched by the byte-exact tooling. The decomp never changes for a port; a port
carries its own differences.

| folder | what |
|---|---|
| `brally/` | Boss Rally, native 64-bit: a retyped fork of the decomp's core (`src/`, `include/`) plus a cross-platform layer (`platform/`: hosts null / macOS / Windows, renderers null / soft / Metal / Vulkan). `PORTABLE-CORE.md` describes it. |
| `brally-wasm/` | Boss Rally, the 32-bit lane: the decomp's MSVC arm compiled to wasm32 and translated to native arm64 C, with the original data at its original addresses, plus a native macOS host (`wasm/host`, `wasm/native`). Ships as the Remastered `Boss Rally.app`. `NATIVE_RENDERER.md` describes its renderer. |
| `tgrally/` | Top Gear Rally, native: a fork of the N64 decomp with an arena memory model and the same hosts. `PORT.md` describes it. |
| `common/` | the Remastered assets (models, textures, music). The binaries stay out of git; their recipes are tracked. |

## Building and checking

```sh
# Boss Rally 64-bit: one host and one renderer per build -> build/brally/HOST-RENDER/
HOST=macos RENDER=metal ports/brally/link64.sh
RENDER=soft ports/brally/link64.sh                 # headless, the reference rasteriser
ports/brally/tools/suite.sh                         # every brbox scenario, headless
ports/brally/package_app.sh                         # build/brally/macos-metal/Boss Rally 64.app
.venv/bin/python ports/brally/tools/sync.py         # what the decomp changed since the fork

# Boss Rally 32-bit lane and the Remastered app -> build/brally/wasm32/
sh ports/brally-wasm/wasm/build_wasm.sh
sh ports/brally-wasm/wasm/package_app.sh [--bin BossRally.BIN] [--rom "Top Gear Rally (USA).z64"]

# Top Gear Rally -> build/tgrally/HOST-RENDER/
HOST=macos RENDER=metal ports/tgrally/link.sh
.venv/bin/python ports/tgrally/tools/suite.py       # every n64box script vs the original's streams
.venv/bin/python ports/tgrally/tools/sync.py        # carry decomp changes into the fork
ports/tgrally/package_app.sh                        # build/tgrally/macos-metal/Top Gear Rally.app
```

`Boss Rally.app` carries everything it reads: the whole data track as its CD
root, the PC soundtrack (CD audio off the BIN/CUE) and the N64 soundtrack
(off the ROM). It needs neither image nor this tree once built; saves go to
`~/Library/Application Support/Boss Rally`. Extraction happens once per set of
sources (cached in `build/brally/wasm32/app/extract`, keyed on their MD5s).

## Music: decided 2026-09-29

The 32-bit lane plays both soundtracks natively (`brally-wasm/wasm/native/music.m`):
the disc's CD audio through AVAudioEngine, and the N64 modules live through
libopenmpt, linked statically. Tab switches between them live
(`brally-wasm/wasm/native/version.m`). The game's own music logic still decides
what plays when; only its Windows backends (MCI and the EAR engine's CD
channel) are replaced. The findings the choice was made on, and the rules that
still hold (the exports stay 1:1 rips), are in `docs/brally/port/music-decision.md`.

## Sound effects

The 32-bit lane plays the game's effects natively too. The game still drives
DirectSound (one static buffer per sample: volume, pan, pitch, play, stop);
`brally-wasm/wasm/host/host_dx.c` mixes those buffers with DirectSound's gain and
pan laws and `brally-wasm/wasm/native/sound.m` sends the mix to AVAudioEngine at
the device rate, through a peak limiter. Muted while the app is inactive, as
DirectSound mutes a window without focus. `BR_SFX=0` silences it;
`BR_SFXWAV=path` records it (headless runs render it offline, in step with
the game's clock).

## History

Until 2026-10-05 a legacy macOS harness (`build.sh`, unit tests under
`tests/`, the address-batch `sliceN_MM.c` files and per-file port specs that
patched the decomp's sources for a clang build) also lived here. The image,
T3, A5 and A7 gates and the ports' own suites superseded it, and it was
retired; it is in git history.
