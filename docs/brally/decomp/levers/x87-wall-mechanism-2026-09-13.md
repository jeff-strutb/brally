# X87 wall mechanism

*Recorded 2026-09-13.*

> x87 operand-role wall CRACKED to mechanism - TU-state scheduling from preceding function definitions, proven byte-exact on the 116 B ext lab shape; multi-component state; practice rules

**2026-09-13 micro-TU sweep (follow-on to [ext-corpus-2026-09-13](../corpus/ext-corpus-2026-09-13.md)): the
wall that blocks the five largest ([five-largest-2026-09-13b](../log/five-largest-2026-09-13b.md)) and the
matrix rows has a MEASURED mechanism.** Lab preserved at
`build/brally/analysis/corpus/lab/lab.py` (git-ignored): one micro .c vs original bytes,
~3 s/probe. Full write-up: docs/brally/VC5-IDIOMS.md tail (commit 7cd4020).

1. **Straight-line FP operand roles (fld side vs memory side) are decided
   by TU compilation state from PRECEDING FUNCTION DEFINITIONS, not the
   function's own source.** Proof: the ext 116 B ApplyP shape goes
   BYTE-EXACT at 8 or 12 dummy `int f(int)` predecessors; pad content
   there irrelevant (fat/float identical); prototypes and globals advance
   nothing. Expression respelling: fully canonicalised (factor swaps,
   re-associations byte-identical).
2. **The state is multi-component, not one counter.** On 0x1006D530:
   straight-line pads of ANY size/kind sample only two output states
   (212/216 B); loop pads sample by parity; ≥3 branches in a pad shift
   saturating; 40+ states never reach the real TU's 206 B. Synthetic pads
   cannot enumerate real-predecessor states.
3. **Rolled-loop anchors are a DIFFERENT decision** - 0x1006DD20 BrMat3Mul
   is ordinal-insensitive (19 diff at every count). This lever never
   touches strength-reduction anchor residue.

**Practice (the paying moves):**
- Stuck straight-line x87 row → run the ~15-compile micro-at-states
  diagnostic FIRST. Any state zeroing it = source CORRECT, row is
  TU-composition/position, STOP respelling. No state zeroing it (in pads)
  = inconclusive on source, but roles are still state-scheduled.
- In-tree, the only reachable states are reorder/refile of REAL functions
  (pads are not committable). The five largest sit in single-function
  files (br_chasestep.c, br_carcol.c, br_obb.c...) - their TU state is
  near-empty; the ORIGINAL's TU had real predecessors.  NEXT LEVER: try
  co-filing each stuck x87 row with its original-TU neighbours (the
  original link order/map says who they were) instead of one-function
  files - that is the state the original compiler actually had.
- This measurement is Gate-A ammunition: st(i)-permutation residue is
  allocation/scheduling BY MEASUREMENT.

Explains [collresp-cluster-2026-09-05](../functions/collresp-cluster-2026-09-05.md) (committed match went diff when
neighbours re-spelled) and 0x1006D530's "position is load-bearing" note.

**Co-filing test #1 (2026-09-13 evening): NULL on the obb/carcol pair.**
Merged BrObbOverlap (0x10068900) ahead of BrCarCarCollide (0x10068F80) in
one TU per VA adjacency; swept; every report row value IDENTICAL to the
two-file state; reverted per rule 6. Two findings: (1) VA adjacency did
not imply same original TU here - the sweep's best variant differs
(carcol O2, obb O2p), and per-TU flags must be uniform, so they were
separate TUs; pick co-filing candidates by matching best-variant AND
adjacency. (2)  report.csv `detail` is RAW POSITIONAL byte-diff (407 on
carcol's 7-region row is normal); never read it as region count - a
co-filing effect smaller than a length wobble needs msetdiff, not the row.
 Sweep objs cache per variant dir (build/brally/win32/match/obj_*/<file>.obj) - rm
them when a file's TU membership changes, or the row is stale.
