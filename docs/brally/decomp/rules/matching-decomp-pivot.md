# Matching decomp pivot

*Recorded 2026-08-19.*

> Project pivoted from port-only to matching decomp + port (SM64 model). Matching build produces bit-identical Win32 DLL, port build targets macOS/Metal. Same source, two build targets.

**Decision (2026-08-19):** The project is restructured from a port-only architecture to a matching decomp with port as a build target (SM64 model).

**Why:** The project lead's goal from day one was MAME-standard, bit-exact decompilation. The port architecture (br_gfx.h, BrRallyMainOps abstractions, Metal-only) diverged from that goal and prevents: (1) producing a hybrid binary that patches recompiled functions into the original DLL, (2) verifying the decomp is truly bit-accurate, (3) drop-in replacement testing on Windows.

**How to apply:**
- Directory structure: `src/brally/core/` (matching C), `src/brally/backends/` (d3d/, glide/, metal/, win32/, macos/)
- `port/` directory is gone. This is a decomp, not a port.
- Core game logic compiles identically under MSVC 5.0 and clang. No `#ifdef`, no platform code.
- The ~21 rendering files and ~3 platform files need original D3D/Glide/Win32 calls in core, with Metal/macOS alternatives in backends.
- All new decomp work is written in matching form from the start.
- `@implements` means the function diffs clean against original bytes, not just passes x87emu.

Related: [msvc-version](../toolchain/msvc-version.md), [goal-coverage-not-playability](goal-coverage-not-playability.md)
