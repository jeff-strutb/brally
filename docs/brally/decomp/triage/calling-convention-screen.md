# Calling convention screen

*Recorded 2026-09-10.*

> Screen every diff row for a ret-K mismatch - a wrong calling convention is a whole-function defect that can go straight to byte-exact

A diff row whose ORIGINAL ends `ret K` where ours ends `ret` (or a different K)
has the wrong calling convention **declared**, not a codegen fork. That is a
correctness defect in the decomp, and fixing it can close the function outright:
2026-09-10, `0x10055020 BrTextListSetBlob` went from 4+6 residue rows straight
to **BYTE-EXACT** the moment it was declared thiscall.

The screen (run it after any batch of new matches):

```python
# for each report.csv diff row, compare the `ret*` rows of the two
# msetdiff.load bags; print the rows where the dicts differ
```

24 of 236 diff rows differ. Read them in two groups:

- **Different K, or K on one side only** - a real convention defect. Found:
  `0x10055020` (fixed, byte-exact), `0x100723D0 BrFfbEnumDevice` (orig `ret 8`,
  the DirectInput EnumDevices callback - `slice3_45.h` had *documented* the
  `__stdcall` for months without anyone applying it to the prototype, the
  definition or the `BrDiEnumDevicesCb` typedef; fixed, residue 38+13 -> 35+10),
  `0x10062B80 BrCtrlCfgAssign` (orig `ret 0x10` = thiscall + 4 stack args,
  **still open** - but its dossier also records a deliberate collapsed-loop
  DEVIATION, so it cannot be byte-exact as written), `0x10063060
  BrCtrlCfgReadFile` (orig `ret 4`) -- **NOT a convention fix; do not spend a
  session on the declaration.** The thiscall form was built and measured: it is
  correct and moves almost nothing (register-blind 118+184 -> 117+183). The
  function opens `mov eax, dword ptr fs:[0]` and installs an SEH frame (three
  fs-segment references, verified 2026-09-10) -- 1,104 B of `__try` scaffolding
  we do not emit at all, which is where the 66-instruction shortfall lives. It
  belongs to the C++/EH workstream, not the T3 lane.
- **Same K, different COUNT**, or a ret row unpaired on one side - cross-jumping,
  not a convention bug. Leave it (0x1006CED0, 0x1002F282, 0x100194C0).

**THE SCREEN IS EXHAUSTED.** Run tree-wide twice on 2026-09-10, by two sessions
independently: exactly TWO rows in the whole tree have a genuinely differing K,
and one of them turned out to be EH. Its whole yield was one byte-exact win and
one open row. Re-run after a batch of new matches; do not expect a second
harvest from the current tree.

The one live row is `0x10062B80 BrCtrlCfgAssign` (orig `ret 0x10` = thiscall,
four stack arguments), and the convention is only **the first of three**
independent edits: the residue also wants the profile dispatch open-coded as
per-arm pointer adds (the original's `dec eax; je` chain with `add R,0xa8` /
`0x150` / `0x1f8`, against our shared index helper's `shl R,3` / `add R,R`),
and its dossier records a deliberate collapsed-loop DEVIATION -- the original
wraps the body in a 28-iteration loop whose extra passes cannot change anything
-- which blocks byte-exactness on its own.

**Multi-argument thiscall recipe** (`src/brally/include/br_match.h` explains why): wrap
EVERY argument after `this` in its own 4-byte struct and declare the function
`BR_THISCALL1` (`__fastcall`). `__fastcall` *skips* a struct when handing out
ecx/edx rather than stopping at it, so wrapping only the second argument lets
the third take edx and the epilogue cleans the wrong amount. `BR_THISCALL` on
its own is a no-op marker - never redefine it.

**Attribution, when several sessions share the tree:** every session commits as
the same git user, so `git log --author` and blame CANNOT tell them apart. The
only reliable record of whose work a row is is `build/brally/win32/match/lane_claims.csv` --
check which token held the VA, never who authored the commit. I inferred
ownership of a row from its commits on 2026-09-10 and was wrong.

Related: [thiscall-via-fastcall](../cpp-lane/thiscall-via-fastcall.md) (the one-argument case), [resume-state](../log/resume-state.md),
[do-not-lower-t3-standard](../rules/do-not-lower-t3-standard.md), [parallel-session-clobber](../traps/parallel-session-clobber.md).
