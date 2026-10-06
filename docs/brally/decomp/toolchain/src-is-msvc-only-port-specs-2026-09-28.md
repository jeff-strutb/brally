# Src is msvc only port specs

*Recorded 2026-09-28.*

> Since 2026-09-28 src/brally/ and src/brally/include/ carry no BR_MATCHING_BUILD/_MSC_VER conditionals; Mac-port differences are name-keyed specs in ports/brally-wasm/patch/, applied by portgen.py

Project lead directive 2026-09-28: no port-specific code in src/brally/ - "muddying the purity of the codebase and architecture".

State after commits 38fee43c, 1a0bcee4, 2d28c40b, e1cf1cfd:
- src/brally/ + src/brally/include/ = exactly what MSVC 5.0 compiles (unifdef'd to the matching view). The pre-commit hook (tools/brally/precommit_rule6.py) refuses an added `#if*` on BR_MATCHING_BUILD/_MSC_VER there.
- Port differences: `ports/brally-wasm/patch/<path>.port` specs, keyed by item name (`@drop fn:X`, `@after key` ... `@end`). `ports/brally-wasm/tools/portgen.py` (run first by build.sh) writes build/brally/macos-legacy/port/src/... and build/brally/macos-legacy/port/include/ (the port's only header dir). A stale key stops the build by name.
- The 56 slices are in ports/brally-wasm/legacy/; 34 port-only modules in ports/brally-wasm/core/; 27 port-only headers in ports/brally-wasm/include/.
- Gates for any refactor: `tools/brally/ppgate.py --check` (MSVC cl /EP tokens, all src) and `ports/brally-wasm/tools/portpp.py --check` (clang -E tokens, all 602 port TUs). Both snapshots live in build/.

**Why:** a matching session edits a body without ever touching port code; the port reads the decomp without guards.
**How to apply:** when a new decomp body breaks the port build, add `@drop` (and an `@after` port body if needed) to its spec. Never add a guard to src. Generators that emit `#ifdef BR_MATCHING_BUILD` wrappers (gen_structural2, gen_phaseleave, gen_uitext, permute WRAP) now get refused by the hook. Related: [port-never-touches-decomp](../rules/port-never-touches-decomp.md), [port-build-drift](../../port/port-build-drift.md).
