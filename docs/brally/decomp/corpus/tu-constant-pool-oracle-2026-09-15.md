# Tu constant pool oracle

*Recorded 2026-09-15.*

> 2026-09-15 PLAN SESSION (no matching done): the original TU partition is recoverable from .rdata FP-constant addresses (VC5 does not fold float literals across TUs: 28 copies of 1.0f); 76.8 KB of open bytes sit in multi-member TU groups spread across 2-10 of our files; the wins plan (Phases 0-5, TU lane) lives in the session scratch file wins-plan-2026-09-15.md and was handed to the project lead

**Finding (measured 2026-09-15, never used before in docs or tools):**
MSVC 5.0 emits floating-point literals per translation unit; `.rdata` of
`reference/brally/orig/BRGlide.dll` holds 28 aligned copies of `1.0f`, 18 of `0.5f`. Two
functions loading the SAME `.rdata` constant address are in the same
original TU. Union-find over x87 constant refs: 258 fns -> 84 groups, 29
multi-member, 25 of 29 VA-compact (<20 KB span). 76,771 B of open T1+T2
bytes sit in multi-member groups, every one spread across 2-10 of our
source files; 23,555 B are open x87 singletons.

Key groups: physics TU 0x100645A0.. (21 fns, 9 files, 13.8 KB open,
contains obb+carcol -- the 09-13 null co-filing test merged 2 of 21, which
is why it was null); race/ctl 0x1005C6D0.. (14/10 files/13.5 KB); dl TU
0x1001EE70.. (24/9/8.3 KB); chase 0x10001510.. (5 fns, 3 T4, contains
0x10001CF0); scene 0x1000C9E0.. (7 fns, contains 0x1000E320 and
BrSceneDlBuild); 0x10005810 TU (2 fns, 1 T4 + the only clean Pool B row).

**Why it matters:** [x87-wall-mechanism-2026-09-13](../levers/x87-wall-mechanism-2026-09-13.md) proved x87 operand
roles are TU state from preceding definitions; the missing piece was WHO the
predecessors were. This oracle answers it. Remaining 204,610 B (T1+T2+T3,
excl. cpp lane) by class: x87 81 KB (40%), byte/16-bit lane 69 KB (34%),
int 45 KB (22%), EH 10 KB.

**Caveats:** all-T4 groups are ALSO spread across files and still match, so
co-filing is the lever only for the role-sensitive x87 class; the per-row
`opt` column mixes flags inside one group (it is the closest variant, not
truth); oracle silent for integer TUs; precision against real TU truth (ext
corpora have source) NOT yet measured -- that is Phase 1 gate 2.

**Plan handed to the project lead (scratch `wins-plan-2026-09-15.md`):** Phase 0
session contract (empty picker => lever session, not re-triage; report only
T4 B / T3 B / pool refill); Phase 1 `tools/brally/tumap.py` + `config/brally/tu_map.csv`
with three validation gates; Phase 2 TU lane (co-file whole TU in VA order,
cheapest TUs first, kill after 5 TUs with zero msetdiff movement); Phase 3
`gen_widen.py` from the proven CRT widen idioms for the byte-lane class;
Phase 4 crank only; Phase 5 cpp residue. Backlog also: fn.py auto FN_OPTS
from the row's opt, t4lane parked -> dated hold, tiers.py T1 definition.

**PHASE 1 BUILT + VALIDATED 2026-09-15 (commit c9e19813):** `tools/brally/tumap.py`
emits `config/brally/tu_map.csv` (tu_id, va, order, flag, tier, size, file,
evidence). 258 fns -> 84 raw FP-const components -> 58 coalesced TUs (VA-span
interleave merge; 24 multi-member, 75,790 B open inside them). Modes:
`--groups` (ranked table), `--lane` (Phase-2 picker: open TUs cheapest first
with move lists, excludes the C++ COMDAT region), `--validate --corpus ext/ext2`.
- **Gate 1 PASS** (coalesced TU spans disjoint; 26 raw components coalesced).
- **Gate 2 PASS** on `--corpus ext`: 100% pair precision (26/26), 9/9 pure
  multi-member groups -- the oracle NEVER merges two source files. ext2
  inconclusive (only 4 FP-const fns, no multi-member group). **Lever proven.**
- **Gate 3 NOT run**: the only all-T4 multi-member control (tu_032, /Od) is
  position-tolerant (br_framebegin.c is already non-VA-ordered yet matches), so
  a co-file control there proves nothing about the role-sensitive x87 /O2
  class. Fold gate 3 into Phase 2's first real /O2 TU (0x100343C0 or
  0x10005810) instead of an artificial control.
- **Oracle CORRECTED a hand note:** tu_032 (0x1002AF17/0x1002B3F0/0x1002BF50)
  share the PHYSICAL const 0x100774B4 (hand-confirmed as DAT_100774b4 'the
  fixed-point scale' in br_framebegin.c) -> they are ONE TU; br_pointdepth.c's
  header "a translation unit of its own" is wrong.
- **Over-merge caveat:** span-coalesce fuses the C++ template/COMDAT region
  (group 0x100393C0, 39 files) -- these genuinely interleave many TUs; --lane
  filters them (cpp members or >12 files). The x87 .c TUs are clean.

**NEXT (Phase 2):** `tumap.py --lane` cheapest first. TU 0x100343C0 is the
prime test: one open T2 (0x10034B70, 696 B) among 11 T4 pool-mates. Co-file
in VA order, rm stale objs, one-file sweep, keep only if nothing regressed.

**How to apply:** do not open a T2 x87 row alone again; run `tumap.py --lane`
and check its TU group first. Related: [walls-are-dated-verdicts](../rules/walls-are-dated-verdicts.md),
[five-largest-2026-09-13b](../log/five-largest-2026-09-13b.md), [pool-refresh-method-2026-09-10](../triage/pool-refresh-method-2026-09-10.md),
[x87-wall-mechanism-2026-09-13](../levers/x87-wall-mechanism-2026-09-13.md).
