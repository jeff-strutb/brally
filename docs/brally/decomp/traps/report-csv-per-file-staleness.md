# Report csv per file staleness

*Recorded 2026-08-21.*

> report.csv can be stale PER FILE even when its overall match total looks current - count zero-diff rows proves nothing.

**2026-08-20.** `build/brally/win32/match/report.csv` was read as "current" because it held
120 rows with `diffs == 0`, matching the recorded 121/810 total. That inference
was WRONG. The total can look right while individual files are badly out of
date.

Concrete case: report.csv listed **22 diff rows for `src/brally/core/slice1_02.c`**,
implying roughly one match in that file. A fresh single-file sweep of the very
same content reported **11 / 24 matched**. The file was byte-identical to HEAD
at the time, so nothing had changed - the report was simply stale for that file.

**Why it matters:** the report is what you pick candidates from. Stale rows send
workers at functions that already match, and make a real gain indistinguishable
from a stale row correcting itself. In this incident it made it impossible to
tell whether a killed worker's claimed `1 -> 10` was real work or the report
catching up. That ambiguity is expensive.

**How to apply:**
- A full sweep (`python3 tools/brally/match_sweep.py` with NO arguments) is the only
  thing that rewrites report.csv. Single-file sweeps deliberately do not.
- Therefore report.csv drifts continuously as single-file work lands. Re-sweep
  fully to re-baseline before planning a round, and copy the old one aside
  (`report.prev-session.csv`) so gains can be diffed honestly afterwards.
- Never quote a per-file diff count from report.csv without a fresh single-file
  sweep behind it.
- Budget ~20 minutes for a full sweep, and run it ALONE - it owns
  `build/brally/win32/match/obj_O2` and `obj_Od`, so concurrent single-file sweeps race it.

Related: [match-tooling-gotchas](match-tooling-gotchas.md), [matching-progress](../log/matching-progress.md),
the commit-every-match rule.
