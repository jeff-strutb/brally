# Toolchain self contained

*Recorded 2026-08-20.*

> The matching-build toolchain must live inside the repo - Wine downloaded to tools/brally/, MSVC extracted from the ISO. Never install to the host.

**HARD REQUIREMENT (project lead, 2026-08-19):** the matching-build toolchain is
staged INSIDE the repo. Never `brew install` Wine, never install to
/Applications, never hand-copy compiler files.

`setup.sh` now does both halves automatically:

- **Wine** - downloads a pinned, sha256-checksummed portable macOS build
  (11.0_1, Gcenx/macOS_Wine_builds) into `tools/toolchains/wine/`. x86_64, runs under
  Rosetta 2 on Apple Silicon. Pinned deliberately: bumping Wine means
  re-verifying every matched function.
- **MSVC 5.0** - mounts `reference/msvc/VCPP-5.00.iso` with `hdiutil` and
  copies `DEVSTUDIO/VC/{BIN,INCLUDE,LIB}` plus `SHAREDIDE/BIN/MSPDB50.DLL`
  into `tools/toolchains/msvc5/`. Compiler ends up at `tools/toolchains/msvc5/bin/cl.exe`.

`tools/toolchains/wine.sh` is the wrapper everything calls. It picks the repo-local Wine
(falling back to PATH only if absent) and sets `WINEPREFIX` to
`build/toolchains/wineprefix` so `~/.wine` is never touched. It also sets `WINEDEBUG=-all`,
which silences the MoltenVK banner Wine otherwise prints on every compile.

**`tools/toolchains/msvc5-compat/` is tracked in git and must stay that way.** It holds
hand-written `stdint.h` / `stdbool.h` shims (VC5 predates C99). A previous
session put them inside `tools/toolchains/msvc5/include/`, where re-extracting the ISO
silently destroys them - the symptom is a `stdint.h` fatal error. Both
build paths add it to the include path AHEAD of the VC5 headers.

**Two gotchas that cost real time:**
1. `cl.exe` parses `/Fo` as a Windows path. Given forward slashes it reports
   "cannot open compiler generated file" and writes nothing. Convert to
   backslashes.
2. A failed compile used to leave the previous `.obj` in place, and the diff
   reported STALE results as fresh - a broken build that looks like a passing
   one. Both build paths now delete the object before compiling. If match
   counts ever jump inexplicably, suspect this first.

Related: [matching-progress](../log/matching-progress.md), [msvc-version](msvc-version.md), [matching-decomp-pivot](../rules/matching-decomp-pivot.md)
