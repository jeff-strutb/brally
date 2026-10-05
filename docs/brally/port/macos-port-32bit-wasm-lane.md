# Macos port 32bit wasm lane

*Recorded 2026-09-25.*

> macOS full-boot port runs the Windows-build source via wasm32 objects -> w2c.py -> native arm64 C, original data at original VAs; project lead OK'd 2026-09-25 as interim, proper native 64-bit port later.

Decision 2026-09-25 (project lead): the real macOS boot (RallyMain, disc files from testdata/disc) is built the 32-bit way FOR NOW; a proper native 64-bit port comes later.

Pipeline (ports/macos/wasm/): clang --target=wasm32 -DBR_MATCHING_BUILD on src/core (MSVC5 headers + gen_inc.py overlay) -> w2c.py translates each wasm OBJECT to C and is itself the linker (data symbols -> original VAs via symmap.py, function addresses = original VAs, indirect calls dispatch on them) -> native clang. Host layer implements the ~190 BRGlide imports (+COM, Glide->Metal); tools/brbox_imports.py is the behavioural spec.

**Why:** the matching source is ILP32 (4-byte pointers in fields, offset-indexed structs, 32-bit addresses in original .data, name aliasing by address); arm64 macOS forbids low-4GB mappings. The project lead was surprised by "WebAssembly" -- say up front it is only an intermediate compile format, output is native C, no browser/runtime.

**How to apply:** don't re-litigate the choice; keep the 64-bit path in mind as the follow-up. Related: [matching-decomp-pivot](../decomp/rules/matching-decomp-pivot.md), [port-build-drift](port-build-drift.md).
