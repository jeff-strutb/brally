# Refile byte identity

*Recorded 2026-09-25.*

> All 66 T3/T4 functions refiled out of slice*.c byte-identically (2026-09-25); how: per-function snapshot compare, position sweep for TU state, fileaudit now fails T3-in-batch

2026-09-25: every T3 and T4 function left the `sliceN_MM.c` batches (66 at
start: 41 T3 + 25 T4). fileaudit only read status=match rows, so it had
reported 12 stranded while 41 T3 sat unseen; it now fails a certified T3 in a
batch (commit a7a12a15), stranded baseline 0, batch baseline 56.

**Rule (stated forcefully):** a refile must leave every body exactly as
it compiled before. "Only byte-identical with X in front of it" is NOT an
excuse to leave a function in the wrong module -- find the placement.

**Method that worked (fast, parallel by .c file):**
- Snapshot every tagged function's compiled bytes per sweep variant BEFORE
  moving (reloc bytes masked, reloc TARGET names compared, /Od bodies trimmed
  at the next symbol).  `match_diff.parse_coff_obj` returns /Od bodies to
  the END of the shared .text section -- useless for identity checks; a
  function-trimmed parser is needed. The sweep's `recomp_size` for /Od rows
  carries the same artifact (e.g. 4035 vs 1357) -- not a change.
- Verify by compiling to a scratch dir (never `match_sweep` from parallel
  workers: it races on report.csv, and sweeping a slice after its function
  left can DROP the new file's row -- re-sweep the new file with --force).
- TU state from preceding function definitions ([x87-wall-mechanism-2026-09-13](../levers/x87-wall-mechanism-2026-09-13.md))
  is the only obstacle. Fix by POSITION SWEEP: insert the block at every
  top-level boundary of candidate module files, keep the position where every
  function in the file is identical. Found homes for BrSpanAddLine (T2
  neighbour disturbed by a move) and the .rca fixup pair (gamedata/br_carload.c).
- Static helper DEFINITIONS count as predecessors even if unused in the
  matching arm; `$S<n>` suffixes on file-statics change with TU content and
  are name-only (bytes identical).
- A file-static aggregate made extern can reschedule /O2 code (BrScenePropsDraw
  1776->1648 B); kept TU-static in its matching arm.
- Not reached: BrFadeDrawSprite (T4) stays in drawing/br_gbihandlers.c --
  standalone it misses the original by 17 B (eax/edx tie-break); no position in
  br_fade.c/br_fadewipe.c reproduces it.

**Image gates in a clean worktree** ([gi-serial-idb-a7-recipe](../oracle/gi-serial-idb-a7-recipe.md)): symlink
main's `build/match/obj_img_*` (hash-keyed, freshness-proven) or the T4 gate
recompiles all 370 TUs serially (~20 min instead of 45 s); main's obj_cpp may
be empty -- compile every report_cpp row (T3 C++ rows too, not just 4/4).
