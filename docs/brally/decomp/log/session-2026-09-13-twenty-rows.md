# Session 2026 09 13 twenty rows

*Recorded 2026-09-13.*

> "20 T1/T2 rows to T4/T3" delivered 2026-09-13 -- 14 byte-exact + 6 @t3; three new source levers (for-loop fills, pointer-local copy pairs, /Gi re-sweep) and the ledger-driver method

**Project lead ask (2026-09-13):** "pick 20 of the T1 or T2 functions and decompile
them to T4 or T3 if you can't hit T4. DO NOT make excuses." Delivered 20 from
the tools/tiers.py --list T1/T2 pool (check `/tmp/t2list.txt` style lists
against the report before probing -- five rows I opened were already @t3).

**Byte-exact (14):** 0x10063CC0 BrReplaySeek, 0x10038A80 BrMenuSetTrackLetter,
0x10015550 BrHudDrawSplitLine, 0x10054070 BrUiTick (cpp), 0x10021240 br_dl_skip,
0x1002F282 BrSessionReinitVideo (cpp /Od), 0x10014E00 BrHudDrawEntrants,
0x1006FCE0 BrCarSlotSetup, 0x100087D0 BrCleanupName, 0x1005C6D0 BrCarRespawn,
and four with NO source change under the `/O2 /Gi` sweep shape: 0x100540D0,
0x100541B0, 0x10054280 (font glyph walks), 0x10054730 BrHudLayoutInit (618 B).
**@t3 certified (6):** 0x10027A70, 0x100011C0 BrSurfBlt24, 0x10017910 BrEarLoad,
0x10038CA0, 0x10038F40, 0x1003AA10 (cpp lane, rows 0+0 register plans).
T1->T2 with dossiers: 0x1001CA30, 0x100271F0, 0x10062B80, 0x10058680,
0x10009010, 0x10010FB0.

**Levers proven (all on the tail of docs/VC5-IDIOMS.md):**
- A `rep stos` fill whose three setup instructions are permuted vs the
  original (`lea edi` first) is NOT a memset in the source: spell it as an
  indexed `for` (constant count) or `for (; i < N; i++) dst[i] = 0` (variable).
  Two rows sat "DO NOT RE-PROBE" on this for days.
- Per-statement load/store copy pairs (`mov [d],edx; mov eax,[s+4]; ...`)
  need both arrays addressed through POINTER LOCALS; member subscripts batch.
- **/Gi resolves the SIB base/index wall.** When a sweep shape is added,
  re-sweep every non-matching C++ row before probing any -- report_cpp.csv
  keeps the old verdict and a stale row looks exactly like a wall. I wrote
  and retired two @t3 tags the same hour because of this.

**Method that paid:** a ledger driver (`cppledger.py` + `cppvariants.py` in
the scratch) that applies (old,new) substitution lists to the tree body,
compiles each through the cpp harness and prints diff-line movement. Two
passes of 10-12 real, orthogonal variants per row = Gate B in ~3 min, and
3 of the 8 "certification-only" rows CRACKED to byte-exact during the pass.

**Why:** the certification pass is not theatre -- writing real variants
against the residue is where the byte-exact cracks came from.
**How to apply:** for any Gate-A-clean row, run the driver before writing
ledger lines; read the "(current numbers)" tuple from `t3.py --qualify`
(bytes, insns, regions, rows) and write those exact numbers or the pass is
not counted. `filing.py` rewrite DROPPED 9 filed rows again (restore them
from `git diff` before committing, see [filing-py-drops-rows](../traps/filing-py-drops-rows.md)). Never
`git stash` a peer-modified file to test a baseline (I did, and popped it).
Related: [cpp-lane-class-cracks-2026-09-12](../cpp-lane/cpp-lane-class-cracks-2026-09-12.md), [do-not-lower-t3-standard](../rules/do-not-lower-t3-standard.md).
