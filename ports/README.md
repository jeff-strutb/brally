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
| `tgrally/` | Top Gear Rally, native: a fork of the N64 decomp with an arena memory model and the same hosts. `PORT.md` describes it. |
| `remastered/` | Top Gear Rally Remastered: Top Gear Rally's menus with Boss Rally's engine running the races, the two native ports in one executable over one host (macOS, Windows, the iPhone; Linux headless). `README.md` describes it. |
| `common/` | the Remastered assets (models, textures, music). The binaries stay out of git; their recipes are tracked. |

## Building and checking

```sh
# Boss Rally 64-bit: one host and one renderer per build -> build/brally/HOST-RENDER/
HOST=macos RENDER=metal ports/brally/link64.sh
RENDER=soft ports/brally/link64.sh                 # headless, the reference rasteriser
ports/brally/tools/suite.sh                         # every brbox scenario, headless
ports/brally/package_app.sh                         # build/brally/macos-metal/Boss Rally 64.app
.venv/bin/python ports/brally/tools/sync.py         # what the decomp changed since the fork

# Top Gear Rally -> build/tgrally/HOST-RENDER/
HOST=macos RENDER=metal ports/tgrally/link.sh
.venv/bin/python ports/tgrally/tools/suite.py       # every n64box script vs the original's streams
.venv/bin/python ports/tgrally/tools/sync.py        # carry decomp changes into the fork
ports/tgrally/package_app.sh                        # build/tgrally/macos-metal/Top Gear Rally.app

# Top Gear Rally Remastered (both games) -> build/remastered/HOST-RENDER/
HOST=macos RENDER=metal ports/remastered/link.sh
ports/remastered/package_app.sh                     # build/remastered/macos-metal/Top Gear Rally Remastered.app
ports/remastered/ios/build.sh [--sim]               # build/remastered/ios-metal: the iPhone app
```

`Boss Rally 64.app` carries everything it reads: the whole data track as its
CD root and the PC soundtrack (CD audio off the BIN/CUE). It needs neither image
nor this tree once built. Extraction happens once per disc (cached in
`build/brally/macos-metal/extract`, keyed on its MD5s).

## The 32-bit lane: removed 2026-10-12

`brally-wasm/` (the decomp's MSVC arm compiled to wasm32 and translated to C,
with a native macOS host) was deprecated and then removed: the 64-bit port is
the game. What it still supplied moved: the verified build's placement table
(`ports/brally/tools/placement.py`, made by `link64.sh` into
`build/brally/placement/`), and the Remastered asset pipeline's Blender tools
and texture catalogue (`ports/common/models/tools`, `ports/common/models/texmat.csv`).
Its music and sound decisions are in `docs/brally/port/music-decision.md`.

## History

Until 2026-10-05 a legacy macOS harness (`build.sh`, unit tests under
`tests/`, the address-batch `sliceN_MM.c` files and per-file port specs that
patched the decomp's sources for a clang build) also lived here. The image,
T3, A5 and A7 gates and the ports' own suites superseded it, and it was
retired; it is in git history.
