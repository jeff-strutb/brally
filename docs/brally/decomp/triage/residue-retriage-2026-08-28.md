# Residue retriage

*Recorded 2026-08-28.*

> Only 7% (40/580) of the diff residue is a true coloring wall; 223 are complete transcriptions with a >=70% structural gap. The 'coloring, move on' verdict was applied far beyond the evidence. Rank with tools/brally/fnmatch/triage.py.

**2026-08-28, after [brtex3dexpand-wall-broken](../functions/brtex3dexpand-wall-broken.md).** `tools/brally/fnmatch/triage.py`
measures how much of a function's gap SURVIVES normalising every GP register
to one name, and ranks complete transcriptions ahead of the missing-code
class. Over the 580 `status=diff` rows in `build/brally/win32/match/report.csv`:

| verdict | count | share |
|---|---:|---:|
| **SHAPE** - complete, >=70% structural | **223** | 38% |
| mixed - complete, 15-70% structural | 128 | 22% |
| MISSING CODE - recomp <80% of orig | 189 | 33% |
| **true coloring wall - <15% structural** | **40** | **7%** |

**Only 7% of the residue is actually a coloring wall.** The
[divergence-class-triage](divergence-class-triage.md) verdict ("register allocation = document and move
on") was applied far more broadly than the evidence supports, because it was
applied to RAW diff counts, which are ~70% naming inflation on a rotated
function. Concrete misdiagnoses found: **BrRcaFixup 0x10030770** was retired
as a coloring wall and is **93% structural with 139 EXTRA instructions**;
**BrTex3dRegister 0x10028BB0** is 30% structural (45 shapes wrong out of 532
instructions, raw count 149).

## Best-profile targets

Complete function + near-zero instruction delta + large structural gap = right
amount of code, wrong codegen shape. That is exactly the class the
counter-fold cracked.

- **BrCarStateEncode 0x10006510** - 98% complete, -7 insns, **100% structural** (313)
- **BrGbiCall10021560 0x100215C0** - 100% complete, +1 insn, 91% (213)
- **BrCrPlaneResolve 0x10067470** - 96% complete, -8 insns, 94% (184)
- **BrNetReset 0x10005CD0** - 108% complete, +10 insns, 98% (156)
- **BrRcaFixup 0x10030770** - 134% complete, +139 insns, 93% (593)
- BrCarStateEncodeDelta, BrCarDrawWheels, BrHudDrawDial, BrScenePropsDraw

The 189 MISSING-CODE rows are the separate, already-known
[inlined-helper-match-class](inlined-helper-match-class.md) workstream - do not mix them in.

**CAVEAT:** triage.py reads `build/brally/win32/match/obj_*/`, only as fresh as the last
sweep that touched each file. A row that looks wrong for a function you just
edited is a stale .obj (BrTex3dExpand's own row was stale when this ran).

Method: [register-rotation-is-a-symptom](register-rotation-is-a-symptom.md). Harness: [fnmatch-harness](../toolchain/fnmatch-harness.md).
