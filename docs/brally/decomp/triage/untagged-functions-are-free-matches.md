# Untagged functions are free matches

*Recorded 2026-08-22.*

> ~259 implemented functions across 78 files carry an address in their comment but no @implements tag, so the scorer cannot see them - some already match.

**Measured 2026-08-21.** slice2_24.c had 33 implemented functions and only 7
carried an `@implements` tag. Tagging the other 26 found **4 that were already
byte-identical** - BrMenuFlags1890/18D0/18F0 and BrMenuLeaveTo2 - with no code
change at all. They had been correct and invisible.

**A heuristic scan says this is tree-wide.** The original scan (six-line
lookback) reported ~259 across 78 files: slice2_14.c (14), br_racebegin.c (14),
br_ai.c (12), br_carphys.c (12), slice2_18.c (11), slice5_63.c (10),
slice1_08.c (10).

** CORRECTION (same day, after checking): the "107 untagged across 26 files"
re-scan was mostly FALSE POSITIVES, and the headline lead in it was wrong.**
That scan counted bare `/* 0xADDR */` marker lines. In this tree such a line is
usually an INTERIOR BASIC-BLOCK annotation inside a function body, not a
function start. slice3_33.c "had 42" - it has **8 function definitions**, 3 of
them file-static, and its five screen builders carry documented port-safety
bounds checks the original lacks, so they cannot match at all. Zero free
matches there.

**Do not scan for address comments. Scan for `@implements` against actual
function definitions**, and confirm the address sits on the function's own
banner. Non-round addresses (0x1004A771) and several markers resolving to the
same enclosing function are the tell that you are looking at block annotations.

The original loose scan's ~259 figure is equally unverified. The real lead here
is slice2_24-style: a file whose FUNCTIONS are implemented and untagged. That
one was real and paid 4 matches; nothing has re-established a comparable pocket
since. Treat any scan output as a list to check one file at a time, never as a
count.

**Do NOT bulk-tag from the heuristic.** A tag is a CLAIM that the function
implements that address, and the scan has false positives - comments that
mention a neighbouring address, static helpers, functions whose address
comment refers to a caller. In slice2_24 the addresses were verified against
each function's own banner before tagging. Do it file by file, read the
comment, then sweep that one file (~12s).

**The denominator moves and that is correct.** slice2_24's tagging took the
tree from 810 to 836 measurable. That is new coverage being counted honestly,
not a regression - `git log -S"@implements 0x..."` confirmed those tags had
never existed in the file's history. Expect the same on other files: the
percentage may dip while the absolute count rises.

Related: [tag-untagged-functions](tag-untagged-functions.md), [matching-progress](../log/matching-progress.md),
[sweep-is-incremental-now](../toolchain/sweep-is-incremental-now.md).
