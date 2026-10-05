# Counting reconciliation

*Recorded 2026-08-28.*

> The combined match count spans THREE report CSVs (DLL-C, C++, EXE) reconciled by tools/total.py. As of 2026-08-27: 706 functions match (570 DLL-C + 36 C++ + 100 EXE).

## The count is now THREE binaries × three scorers - total.py reconciles them

Matching is no longer one sweep. There are three independent report CSVs, each
from its own scorer, plus a reconciler:

| CSV | scorer / sweep | scope | status col |
|---|---|---|---|
| `build/match/report.csv` | `match_sweep.py` (cl C, /O2+/Od) | BRGlide.dll C | `$4` |
| `build/match/report_cpp.csv` | `cpp_sweep.py` → `cpp_score.py` (cl /GX) | BRGlide.dll C++ EH | `$4` |
| `build/match/report_exe.csv` | `exe_sweep.py` | BRally/SetVideo/BossRally | **`$5`** (leading `exe` col) |

**`python3 tools/total.py`** re-scores and reconciles all three, and writes the
manifests `build/match/cpp_matches.csv` and `build/match/exe_matches.csv` that
`progressmap.py` reads. It is the authoritative combined number.

**As of 2026-08-27 (later): 741 functions match** - 599 DLL-C + 39 C++ + 103
EXE; **0 compile errors**. (Watch the EXE CSV's extra leading column  - 
`awk -F, '$4=="match"'` under-counts it; use `$5`.)

## report.csv is a GITIGNORED LOCAL artifact - source files are the truth

`build/match/report.csv` (and the cpp/exe reports) are NOT the durable record  - 
they are regenerated locally by the sweeps. The COMMITTED `.c`/`.cpp` files are
the source of truth. To audit for lost/unbanked matches: cross-reference each
`match` row's `file` against `git ls-files` - any match whose source is
untracked is at-risk. On 2026-08-27 all 741 matches had committed source (0
unbanked), which is why a mid-wave stop lost nothing (matches were
committed on the spot - see the commit-every-match rule).

**Activity ≠ matches.** A wide `--refine` batch reporting "refining 183
functions" processed 183 CANDIDATES; it banked 0. Scrolling Run/Search/Autofile
output looks the same for failed search as for success - count committed
matches, not visible churn.

## progressmap ↔ total.py counters must agree (fixed 2026-08-27)

`progressmap.py` undercounted by 23 (reported 713 vs total.py's 736): it keyed
ONE report row per VA (**last-wins**), so stale `diff` rows from old slice
copies - functions filed into their own module but whose old slice row was never
re-swept - shadowed their real `match` rows. Fixed (`a2040a1`): prefer a `match`
row over a non-match one for the same VA (**any-match-wins**), independent of
row order. Both counters now agree. Note: their BYTE sums differ by ~100 B by
design - progressmap sums the function-universe size (to draw every cell),
total.py sums the extracted orig_size; function COUNTS are identical. ~26 stale
duplicate `diff` rows still linger in report.csv (the counter now ignores them);
proper purge is re-sweeping those slices when the pipeline is quiet.

## progressmap.py recolor (2026-08-27)

`progressmap.py` now distinguishes **fenced** CRT (linked, never a decomp
target - purple `#6e5494`) from real **todo** (gray). `CRT_START` per EXE
(brally 0x401BC0, setvideo 0x402D20, bossrally 0x401BC0) marks everything above
it as fenced. C++ matches render green grouped as `src/core/cpp/(C++ EH)`; each
EXE is its own region `EXE: <name>.exe`. Result on last run: 100 match /
477 fenced / 19 todo across the EXE map. Fenced ≠ unfinished - it is CRT we
deliberately do not decompile (see [glide-is-the-reference](../rules/glide-is-the-reference.md), rule: standard
CRT is linked for the matching build and provided by libc for the port).

See [image-build-gate](../oracle/image-build-gate.md) (deliverable check), [matching-progress](../log/matching-progress.md),
[exe-decomp-state](../functions/exe-decomp-state.md), [cxx-eh-frame-wall](../cpp-lane/cxx-eh-frame-wall.md).
