# ports/ — derived platform layers (NOT byte-matched)

This tree is downstream of the decomp. The decomp (the repo root) is the
master: the portable game logic in `src/core/` plus the byte-matched original
**Win9x** platform layer (`src/backends/{glide,d3d,win32}`, `src/exe/`), all
verified bit-exact against the shipped 1999 binaries.

A **port** takes that master's portable core and supplies a *new* platform
layer for a different target. Port code is written fresh, uses the target's own
system libraries and graphics API, and is **not** byte-matched — there is no
original binary for a port to reproduce. So nothing here carries `@implements`,
appears in the match reports, or is touched by the byte-exact tooling
(`tools/`, `config/` maps and fences, the image assembler). That machinery
belongs to the decomp; a port has no bytes to match, so it has no use for it.

The one axis that *is* shared between the Win9x original and a port is the
platform-abstraction seam: where the original build links Microsoft's CRT
(`/ML`, `/MT`, `/MD` — see `config/binaries.csv`), a macOS build links the
Mac's libc; where the original draws through Glide/Direct3D, a macOS build
draws through Metal. Same slot in the design, different platform.

## Music — an open decision, do not design around it silently

No port plays music yet, deliberately. The intent is a player-facing choice
between the PC soundtrack (the disc's CD audio) and the N64 one (tracker
modules), and the user wants to settle *how* that is built with the findings in
front of them. **Read `MUSIC-DECISION-PENDING.md` and ask before writing audio
code.**

## macos/

The macOS/Metal port. `metal/br_gfx_metal.m` is the graphics backend (the peer
of the win9x `glide`/`d3d` backends, but new code, not decomp). Built by
`build.sh` with clang.

### Where the port's code lives

`src/` and `include/` are exactly what MSVC 5.0 compiles for the byte-matched
build: no `BR_MATCHING_BUILD` or `_MSC_VER` conditionals, no port arms
(the pre-commit hook refuses one). Everything the Mac port does differently is
here:

| path | what |
|---|---|
| `macos/patch/<path>.port` | a **spec** per decomp module or header the port compiles differently: the top-level items it drops (by name -- `fn:BrFontLoad`, `var:DAT_…`, `define:…`) and the port bodies it compiles in their place. `tools/portgen.py` documents the format. |
| `macos/core/<dir>/<file>.c` | port-only modules (audio engine, mixer, image loader, boot stubs, …), under the same responsibility layout as `src/core/` |
| `macos/legacy/sliceN_MM.c` | the BRD3D-era transcription the port still runs on, by address batch |
| `macos/include/` | port-only headers |
| `macos/tools/` | `portgen.py` (applies the specs; `build.sh` runs it first), `portsplit.py` (turns a file's conditionals into a spec), `portpp.py` (proves a change leaves every port TU's tokens unchanged) |

`build.sh` runs `portgen.py`, which writes the port's copy of each spec'd
module to `build/port/src/…` and the port's view of every header to
`build/port/include/` -- the only header directory the port compiles against.
A module with no spec compiles straight from `src/`.

Specs name items, not text, so the decomp can keep re-spelling a body the port
drops without touching the port. When an item a spec names is renamed or
deleted, `portgen.py` stops the build and says which key went stale. When a
new decomp body will not compile for the port, add `@drop <key>` for it (and
an `@after` block if the port needs its own version).
