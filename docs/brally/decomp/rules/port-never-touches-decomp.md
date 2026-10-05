# Feedback port never touches decomp

*Recorded 2026-09-25.*

> RULE - port work (ports/macos, wasm lane) must NEVER edit decomp source (src/, include/, tests of it), even byte-neutral edits; all fixes go port-side (flags, port include dir, pre-includes, generated overlays in build/).

The decomp'd source stays pure. Port work must not modify src/core, include/, or their tests - not even edits a sweep proves byte-identical.

**Why:** 2026-09-25 the project lead asked "you're not even remotely modifying any of the decomp'd source, right? That needs to stay pure" after I had made four byte-neutral source edits (slice3_44.h prototype, slice3_44.c rename, br_netopen.c include move, test_slice3_44.c) to get the Mac build compiling. I reverted them.

**How to apply:** solve port compile problems with: compiler flags; ports/macos/include (searched last) for missing headers; per-file `-include` port headers (e.g. br_mat3_port.h renames a clashing prototype); generated patched copies under build/ only for third-party headers (MSVC SDK). If a fix seems to need a source edit, disclose it and ask. Related: [macos-port-32bit-wasm-lane](../../port/macos-port-32bit-wasm-lane.md), [port-build-drift](../../port/port-build-drift.md).
