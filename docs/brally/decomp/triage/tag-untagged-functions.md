# Feedback tag untagged functions

*Recorded 2026-08-19.*

> Scan for implemented-but-untagged functions. They are free verified coverage -- just add the @implements line.

**Check for functions that are implemented and tested but missing their
@implements tag.** These are free coverage -- the work is done, only the
bookkeeping label is absent.

**Why:** Session 2026-08-19 found three functions (BrTriContainsPoint,
BrCollGridCellAcquire, BrEntSetPos) that were fully implemented with test
suites but had never been given their @implements line. Each was a one-line
commit to tag. One of them (BrCollGridCellAcquire) was the last blocker for
a frontier function -- tagging it unblocked 0x1006EC30.

**How to apply:** when looking for cheap wins on the frontier, run
`tools/manifest.py --audit` to find addresses that are "ONLY inferred" (body
exists, no manifest line). Each is a potential free tag. Also check callee
lists: if whereis.py says a callee is "not implemented" but grep finds a body
with the d3d twin's address in a comment, it just needs its tag.

Related: [readme-status-was-stale](../traps/readme-status-was-stale.md), verify-blockers-through-twins.
