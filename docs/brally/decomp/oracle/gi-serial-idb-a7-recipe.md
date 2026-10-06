# Gi serial idb a7 recipe

*Recorded 2026-09-25.*

> Clean-worktree A7 needs the /Gi C++ rows compiled SERIALLY through one vc50.idb; a fresh/private idb makes BrHudLayoutInit 0x10054730 "regress" 30 B

2026-09-24: `/Gi` codegen depends on the accumulated `vc50.idb`. In a clean
worktree, compiling the 9 `O2 Gi` report_cpp rows with a private `/Fd` (or a
fresh idb) makes 0x10054730 BrHudLayoutInit come out 30/618 B different, and
image_build_t3 fails with "T4 spans that REGRESSED: 1". Compiling those 9
rows serially, in sorted (file, va) order, through ONE worktree-root vc50.idb
reproduces main's matching obj byte-for-byte. All other C++ rows are safe to
compile in parallel with a private `/Fd` (241 objs in ~60 s).

Recipe that got A7 26/26 at f8177e97: worktree at HEAD, then symlinks per
[worktree-bootstrap](../toolchain/worktree-bootstrap.md) and copies of the report*.csv files, then
cpp_score.compile_cpp for every row (parallel, private /Fd), then the /Gi rows
serially, then `BR_JOBS=14 image_build_t3.py --jobs 14`, then copy the dll to
build/brally/win32/brbox/image, then `brbox_diff.py --all` (~15-18 min). Check the frame
counts, not only IDENTICAL: 0-frame "IDENTICAL" rows were bogus.

**Why:** false T4 regressions and stale C++ objs blocked the image for other
sessions. **How to apply:** one session owns the global A7 run at a time,
coordinated via SendMessage ([parallel-session-clobber](../traps/parallel-session-clobber.md)).
Related: [inline-int-return-temp-idiom](../levers/inline-int-return-temp-idiom.md), [whole-image-oracle-2026-09-24](whole-image-oracle-2026-09-24.md).

2026-09-27 additions (session, A7 for 0x10066610):
- The worktree's ABSOLUTE path must have the SAME LENGTH as the main checkout
  (/Users/jeffreywilbur/projects/strutb/brally = 43 chars), else /Gi
  BrAiScanCorridor 0x1005D060 comes out 4 bytes off. Used
  /private/tmp/brally_a7_worktree_f917cc_0000 (43) via `git worktree move`.
- Link build/brally/win32/brbox/cd (the game data) into the worktree, or every script
  "runs" 0 frames with exit(1) and all 26 read DIFFERENT (brbox_diff prints
  NO RUN). tools/toolchains/msvc5 has a tracked crt/ subdir: link its other children
  (bin, bin-sp3, include, lib) one by one.
- C++ objs: compile every report_cpp row into the worktree's own
  build/brally/win32/match/obj_cpp (image_build_t3 reads them, never recompiles), non-Gi
  in parallel, the O2-Gi rows serially in (file, va) order with a fresh
  worktree-root vc50.idb; verify all O2-Gi rows reproduce before building.
