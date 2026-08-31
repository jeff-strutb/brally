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

## macos/

The macOS/Metal port. `metal/br_gfx_metal.m` is the graphics backend (the peer
of the win9x `glide`/`d3d` backends, but new code, not decomp). Built by
`build.sh` with clang.
