# Progressmap t3 undercount fixed

*Recorded 2026-09-21.*

> The treemap under-counted T3 (grey boxes lied); generator fixed 2026-09-20 to color T3 from source tags and self-refresh.

Until 2026-09-20 the decomp progress treemap painted ~15 already-certified T3
functions grey/"todo" - including the 11 KB race-step giant. Two causes, both
fixed (commit 9669b96c):

1. The join gated codegen (T3) status on the function ALSO having a sweep-report
   row (`elif m and va in t3`). Functions filed into their own module
   (`src/core/cpp/<VA>.cpp`) carry an `@t3` tag but no report row, so they fell
   through to grey. Fixed: T3 is colored from the tag set regardless of the
   report.
2. `build/match/tier3.csv` is an untracked build artifact nobody commits, so a
   render against a stale copy under-counts. Fixed: the generator now shells out
   to `tiers.py` at load to regenerate it from source `@t3` tags first.

**So a grey box in the map is now trustworthy as genuinely-undone** - before,
"largest grey box" was mostly a stale-CSV artifact (the top ~7 were all already
certified). Ground-truth check for any candidate: `grep -rlE "@t3 +0x<VA>" src/`.
The `codegen` count should equal the distinct source `@t3` tag count.

Same day, `0x1000E320` BrSceneVisPrepare (1992 B) was certified T3 - its
2026-09-15 "NOT certifiable, A2 raw-distance wall" verdict was a dated false
wall, superseded once the A5 oracle returned EQUIVALENT (reinforces
[walls-are-dated-verdicts](../rules/walls-are-dated-verdicts.md), [upgrade-byteshape-t3-to-a5-proven](../oracle/upgrade-byteshape-t3-to-a5-proven.md)). Note:
`crank.py`'s `corpus_census` returns census=yes whenever the divergence query
runs, even on a corpus MISS - the A5 oracle is the real behavioural proof, not
the census flag. A parallel session was on the same VA concurrently
([parallel-session-clobber](parallel-session-clobber.md)); completed the cert with a pathspec commit.
