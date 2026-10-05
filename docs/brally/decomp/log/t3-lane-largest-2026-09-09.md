# T3 lane largest

*Recorded 2026-09-09.*

> 2026-09-09 T3 lane on the LARGEST unfinished functions: +1 byte-exact (BrRaceGateStep 2,538 B) and +3 T3-certified (BrInputPoll 4,145 B, BrGlTrackHdrRead 1,549 B, BrScenePropsDraw 1,768 B); two gate-tool artefacts fixed; the per-function probe harness pattern that made 100+ compiles cheap.

**Method that paid (reuse it):** rank report.csv diff rows by orig_size, run
`tools/t3.py --qualify <VA>` on each large one FIRST -- it is read-only and
says exactly which gate fails. Two of the "semantic" A3 failures were TOOL
artefacts, not code: (1) msetdiff normalised a reloc'd imm32 as `0` when the
displacement had already become `A` (phantom 1+1 on 0x1005FF00; fixed
9fd065e), (2) t3.py's canon promised a lea/add class and did not implement
`lea R,[R+d]` ~ `add R,d` (0x1001D1B0; fixed). Both changes were checked
against every certified tag (none moved) before committing. Screen: if
divergence.py shows N regions and msetdiff shows rows NOT inside them,
suspect the normaliser before the source.

**Gate B is the cost, not Gate A.** Two counted passes (>= 10 REAL compiles
each, census yes) at the CURRENT numbers. Landing a lever moves the numbers,
so two more passes follow every landing. A compile that fails or a probe
whose substitution misses is not a probe -- count only valid compiles.
0x1001D1B0 needed three passes because pass 1 had 9 valid.

**Harness:** scratch `harness.py <VA> <probes.py>` -- P = {name: [(old,
new)] or callable(src)->src} applied to the pristine `fn.py --make w1` copy,
each scored by `fn.py --var`. ~10 s per compile, 8-12 probes per call.
 Callables that `str.replace` a declaration line hit EVERY function in the
TU that shares the line (seven port bodies in slice2_20.c got a `#define h`);
restrict to the function span and check `git diff -U0` before the sweep.

**Levers found (all in docs/VC5-IDIOMS.md tail):** two-operand add
destination = the LATER-DECLARED of two NAMED operands (0x1005FF00,
byte-exact in 11 compiles after 5 sessions of single-operand spellings);
named pointers/pointer locals are scheduler symbols, expressions of the
parameter or counter are not (0x10031B80, 13 -> 4 regions, three levers).

**Parallel session the same day** certified 17 small T3s and held
br_framedrive.c (BrFrameDraw) uncommitted -- claim_lane.py did not know;
`git status --short <file>` before opening any large row. BrFrameDraw's
Gate A is 2 rows short (a global reload across the mirror `if` + a loop-
rotation `jmp`), the other session's note in the file says the yMir/hMir
order is the lever. Next largest after it: BrCtlAiBody 0x1005D770 (25
regions, 12+13 rows), BrObjDlBuild 0x1000CBA0 (17 regions).

Related: [t3-certified-standard](../rules/t3-certified-standard.md), [declaration-order-tiebreak](../levers/declaration-order-tiebreak.md),
[corpus-query-tool](../corpus/corpus-query-tool.md), [filing-and-description-gate](../toolchain/filing-and-description-gate.md).
