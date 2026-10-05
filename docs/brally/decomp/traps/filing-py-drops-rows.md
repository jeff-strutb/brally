# Filing py drops rows

*Recorded 2026-09-05.*

> tools/filing.py rewrites filing.csv from report.csv and silently DROPS another session's in-flight rows; check for LOST rows before committing.

`tools/filing.py` with no arguments rewrites `config/filing.csv` from
`build/match/report.csv` and drops every row whose VA is not in report.csv.
On 2026-09-05 that twice tried to delete the other session's two parked
collresp rows (0x10066610 / 0x10066950).

**Why:** the rewrite treats report.csv as the whole truth, and a parallel
session's transcriptions reach report.csv only after ITS sweep.

**How to apply:** after running filing.py, diff against HEAD and list VAs
present only on `-` lines (the awk one-liner is in `docs/T1-INTAKE-LANE.md`);
re-add LOST rows by hand from `git show HEAD:config/filing.csv` with
`newline=''` (CRLF file), then commit filing.csv on its own pathspec.

Related: [parallel-session-clobber](parallel-session-clobber.md), [filing-and-description-gate](../toolchain/filing-and-description-gate.md).
