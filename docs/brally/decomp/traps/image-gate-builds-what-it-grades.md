# Image gate builds what it grades

*Recorded 2026-09-04.*

> image_build.py graded STALE objects with no freshness test until 2026-09-03 (7234422) - it could print '0 differing bytes' on bytes the tree would not produce. Fixed; it now compiles its own objects and has THREE outcomes (FAILED / INCONCLUSIVE / raced exit 2).

## The gate could lie, and did (fixed 2026-09-03, commit 7234422)

`tools/image_build.py`'s **DLL lane read whatever `.obj` was sitting in
`build/match/obj_<opt>/` with no freshness test at all.** A *missing* object
was caught (its rows surfaced as unplaced); a **stale** one was not. So an
object left over from an older version of a file placed bytes the current tree
would not produce, while the run printed *"0 differing bytes / every claim
holds"*. The EXE lane had always compiled its own objects; the DLL lane
free-rode on the sweep.

**It was live.** 140 of 192 DLL objects predated the last edit to their own
source or to a header - and for `br_collresp.c` the sweep's object differed
from a fresh build of the same file in **63 bytes of real code**, not just the
COFF timestamp. The gate had been grading that object.

> **RETRACTED, and the retraction is the lesson.** I reported
> `0x100664F0 BrCrCorner` from that run as a committed regression (2 differing
> bytes at `+0x12`/`+0x15`, swapped stack-slot displacements) and checked
> `git status` to "prove" it was committed rather than working-tree churn. It
> re-scores **0 diffs** now from the same path. The reading was taken while
> `refile_group.py` was rewriting that file, and **a clean `git status` does
> NOT mean the tree is still** - that job commits as it goes, so every
> intermediate state looks committed. Measure nothing about the DLL lane while
> a refiling or sweep job is running; the gate's own exit-2 race check is the
> guard, and I bypassed it by scoring one function by hand.

 **THE GENERAL HAZARD, and it is rule 10's real cost:** a shared header edit
silently un-matches functions in files nobody re-sweeps, and BOTH the report
and (until now) the gate keep saying they match. After touching anything in
`include/`, the objects of every dependent TU are stale - the gate now rebuilds
them, but `report.csv` rows are NOT re-scored by it. **A green gate does not
re-validate a stale report row's diff count; it only proves what it placed.**

## Three outcomes now, not two

    exit 0  OK             every claim holds.
    exit 1  FAILED         a claim is WRONG - differing bytes, an overlap, two
                           names at one address, a claim outside .text, or a
                           symbol absent from a built object (stale report row,
                           rename, bad VA). A decomp defect. Chase it.
    exit 1  INCONCLUSIVE   a claimed function would not COMPILE, so it was
                           never graded. Everything that built holds. Fix the
                           source or wait for whoever is mid-edit. NOT a claim
                           defect.
    exit 2  raced          sources changed WHILE the run was grading them.
                           Describes no single state of the tree in either
                           direction - record nothing from it.

Before this, every non-pass printed *"the tree's claims do not hold at image
level"*, which sent a session hunting a decomp defect that did not exist while
`refile_group.py` rewrote `src/` underneath the run - three different failure
sets in twenty minutes, each reading as a hard FAIL. See warning 1 in
[image-build-gate](../oracle/image-build-gate.md), which this supersedes on the "re-run before believing a
failure" point: the tool now tells you WHY.

## Cost and cache

First run after a header edit rebuilds every dependent TU (~140 here, a few
minutes); mtime-cached after, into `build/match/obj_img_dll_<tag>/` so it never
races the sweep's own `obj_<tag>/`. `--recompile` forces both lanes.
`tools/audit.py` **check E** covers freshness, race detection, the opt-tag
mapping and the verdict taxonomy - check C only ever proved the image DIFF was
a real comparison, never that the gate compared the CURRENT tree.

See [image-build-gate](../oracle/image-build-gate.md), [toolchain-self-contained](../toolchain/toolchain-self-contained.md).
