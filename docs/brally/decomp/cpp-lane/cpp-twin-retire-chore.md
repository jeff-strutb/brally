# Cpp twin retire chore

*Recorded 2026-09-03.*

> Filing a C++ TU does not untag the C body at the same address - the two reports never see each other, so solved functions stay in the DLL-C denominator and in triage's open list. Run tools/cpp_twin_retire.py after any C++ filing batch.

**Closed 2026-09-03: 98 functions were solved twice and counted as open.**
`report_cpp.csv` said `match` while `report.csv` went on saying `diff` for the
same VA, because the C and C++ sweeps write separate files and neither reads
the other. DLL-C tagged went 1,180 → 1,082 with MATCH unchanged at 815;
`tools/fnmatch/triage.py` went 362 → 264 measured functions. Image gate stayed
at 0 differing bytes.

**The chore is now step 5 of "Filing a new match"** in
`docs/cpp-integration-notes.md`, with a pointer from `docs/the notes index`. The
auditor is `tools/cpp_twin_retire.py` - read-only without `--apply`. Run it
after any batch of C++ filing; it currently reports clean.

Two things that make this class hard to see by hand, both worth remembering:

- **The C tag is often the d3d address, not the glide one.**
  `match_diff.parse_implements` translates d3d→glide through
  `config/shared.csv`, so grepping a `.c` file for the glide VA misses most of
  the backlog. Join the two reports on `va` instead.
- **The join is a report artefact until the file agrees.** Before untagging,
  check the `.cpp` is named for the VA, its own `@implements` resolves to that
  VA, and a fresh `cpp_sweep` of it still comes back `match`. Its
  `@implements` name is only a logical label - the real entry point is in
  `@cpp_symbol` (mangled), so a symbol-presence check on the label alone
  wrongly rejects every dtor/ctor TU.

Convention for the retired body (worked examples: `BrFadeRelease` /
`BrFadeLatch` in `src/core/slice2_16.c`, `BrTimeUpdate` in
`src/core/slice1_09.c`): the port-friendly C body STAYS, only its
`@implements` goes, replaced by
`/* port-only body; Glide match is src/core/cpp/0x<VA>.cpp */`. Re-sweep and
confirm MATCH did not move - a retired row was a `diff`, so tagged drops and
matched must not.

**Still open, needs a tooling change not a source edit:** `report.csv` carries
one `.cpp` path as a C row (`src/core/cpp/0x100415D0.cpp`,
`status=not_in_obj`). `match_sweep.sources` walks `*.c` only so it is never
re-swept, and `merge_report` drops a row only when its file is *gone* - so it
sits in the denominator forever.

Related: [counting-reconciliation](../traps/counting-reconciliation.md), [cpp-vcall-family-lode](cpp-vcall-family-lode.md),
[image-build-gate](../oracle/image-build-gate.md), [worktree-bootstrap](../toolchain/worktree-bootstrap.md)
