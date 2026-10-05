# Gate a distance survey

*Recorded 2026-09-10.*

> SUPERSEDED - this byte-Gate-A distance survey is obsolete under the A5 regime; rows it listed as far from certification (0x100250D0, 0x1000CBA0 BrObjDlBuild, 0x1005D770 BrCtlAiBody) are all now CERTIFIED T3. Keep only the survey-script pattern (iterate report.csv, record failing gate).

Ran `t3.py --qualify` over EVERY uncertified non-EH row (161 of them) and
recorded which gate fails. Reusable script pattern: iterate report.csv rows
with `status != match`, drop VAs in `t3.py --vas`, drop originals whose first
two bytes are `6aff` (C++ EH), then parse the `A1..A5 PASS/FAIL` lines. ~25 min.

**THE HEADLINE, and it retires the "just grind the biggest" plan:** the
largest uncertified functions are NOT near Gate A. Distance in A2 rows
(missing+extra) against the limit (2.5% of the original's instruction count):

| VA | name | bytes | A2 rows / limit |
|---|---|---:|---|
| 0x100250D0 | BrTex3dExpand | 8,480 | 70 / 60  (A1 PASSES) |
| 0x1000CBA0 | BrObjDlBuild | 4,180 | 256 / 26 (+208 B jump-table diff, gate A6) |
| 0x1005D770 | BrCtlAiBody | 3,858 | 35 / 27 |
| 0x100131E0 | BrSnapInterpDraw | 3,217 | 457 / 21 |
| 0x1005AFF0 | BrCtlInputApply | 3,210 | 328 / 20 |
| 0x100645A0 | BrCarPhysDriveMatch | 3,070 | 55 / 21 |
| 0x10061470 | BrSndCarStep | 2,757 | 101 / 68 |
| 0x1005F6C0 | BrLapSaveRestore | 2,104 | 46 / 14 |

 **BrTex3dExpand is the closest of the giants, not the furthest** -- A1
passes outright and it is TEN rows over A2. Twelve prior sessions settled its
residue as pure allocation. Its A4 runs at key 6, which its own dossier proves
mis-syncs this function (`--key 10`, never 6). Rule 11 needs the project lead to name
the VA; they did on 2026-09-10.

**The only cheap pools left, and they are small.** Two classes, found by the
same survey:

- **Gate-B-owing (7 rows)**: pass gates 0 and A completely, owe only the
  counted `@t4-pass` ledger. 0x100415D0 (738 B, C++ lane, symbol not in the
  obj -- skip), 0x10029B50 BrTexInit 285, 0x1005F580 BrRankAssign 259,
  0x10018EF0 BrVtxExpand 203, 0x100306D0 BrPendListAdd 51, 0x100271F0
  br_tex3d_texel 44 (symbol not in obj), 0x10037F70 BrUiHook85 43.
- **A4-only (6 rows)**: pass 0, A1, A2, A3, A5 and fail ONLY A4 lost-sync.
  0x10060F40 BrSndNearestCommit 683, 0x10058540 BrSprFontRectInit 309,
  0x10058900 BrExt_1005FBC0 289, 0x10029EC0 BrMat4Frustum 288, 0x10013FD0
  BrGfxDrawTexRect 217, 0x10039D20 BrMenuCap07E0 129. On 0x10060F40 A2 is
  0+0 and A3 is 0 unpaired -- the multiset is PROVABLY identical and A4 fails
  only because divergence.py is not register-blind and cannot resync across a
  whole-function register rotation. Whether A4 should be waived when A3
  leaves zero unpaired rows is a GATE question for the project lead, not something to
  change unilaterally.

 **`t3.py --qualify --all` lists ALREADY-CERTIFIED rows as READY.** All 53
"certifiable now" rows on 2026-09-10 were already tagged. Always filter the
READY list against `t3.py --vas` before believing a harvest exists -- the
"400-800 B harvest is empty" note in [resume-state](resume-state.md) is still correct.

Related: [t3-certified-standard](../rules/t3-certified-standard.md), [t3-lane-largest-2026-09-09](t3-lane-largest-2026-09-09.md),
[brtex3dexpand-wall-broken](../functions/brtex3dexpand-wall-broken.md), [counting-reconciliation](../traps/counting-reconciliation.md).
