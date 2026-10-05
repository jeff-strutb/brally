# Unswept tu bookkeeping class

*Recorded 2026-09-03.*

> A written-but-never-swept TU keeps the port body's diff row in report.csv, so a finished byte-exact match reads as fresh work; claimcheck.py is the only screen that finds it.

A matching TU that exists on disk but has never been through
`tools/match_sweep.py` is **invisible to every count and every ranking**.
`build/match/report.csv` still carries the PORT body's `diff` row for that VA,
so `tools/fnmatch/triage.py` ranks the function as fresh structural work and
`tools/total.py` does not count it.

**Why:** the sweep is what discovers `@implements` tags and writes rows; a tag
written in a previous session and never compiled leaves report.csv keyed to
whichever file claimed the VA earlier - usually the port body under its D3D
tag. The VA *is* present in report.csv, just pointing at the wrong file, so a
"tags with no report row" screen finds nothing and `tools/stale_claims.py`
reports "every diff row owns its VA".

**How to apply - run these at session start, before triage:**

```bash
python3 tools/claimcheck.py     # "TWO NAMES CLAIMING ONE ADDRESS" is the tell
git status --short src/         # uncommitted matches (rule 7 violations)
ls src/core/generated/*.c | wc -l
grep -c 'src/core/generated/' build/match/report.csv   # must be equal
```

When claimcheck flags a pair, sweep the generated/cpp TU first, then untag the
port body to `/* port-only body; Glide match is <path> */` and re-sweep that
file so the phantom row disappears. Do both in one commit.

Found 2026-09-03: **two free byte-exact matches, 534 bytes** - 0x10033BB0
BrPfxTick (219 B, the TU was UNTRACKED, a killed worker's uncommitted match)
and 0x10033880 BrPfxUpdateB0 (315 B, tracked, never swept). Both scored MATCH
on the first sweep with no editing at all.

Related: the commit-every-match rule (commit every match immediately - this
is what happens when that is skipped), [port-safety-additions-block-matches](../triage/port-safety-additions-block-matches.md),
[counting-reconciliation](counting-reconciliation.md), [resume-state](../log/resume-state.md).
