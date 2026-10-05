# Ctlai a5 blocker

*Recorded 2026-09-16.*

> 2026-09-16: BrCtlAiBody 0x1005D770 CERTIFIED T3 (committed bd63991f) after building the object-GRAPH seeding capability -- a large `this` buffer whose pointer fields point at seeded scratch sub-objects. This is the lever for the whole AI/scene object-graph class the earlier BrObjDlBuild/ctlai attempts wall on.

** BrCtlAiBody 0x1005D770 (3858 B, __thiscall(BrAiCar*)) is T3 (committed
bd63991f).** A5 oracle EQUIVALENT across 160 valid-state seeds; NINE negative
controls across the steering pipeline (budget const, aim lerp, lateral->steer,
correction-law entry, bias globals, both heading thresholds, corridor-width
limit) all DIFF -> real teeth. Earlier this session I wrongly called it a T4-only
wall; the "no excuses" push was right -- the object graph IS seedable.

** THE REUSABLE CAPABILITY (the lever for the AI/scene object-graph class):
seed a large `this` as a valid OBJECT GRAPH, not a flat buffer.** Committed in
t3b_verify.py + oracle_profiles.py:
- `Profile(buf_sizes={argidx: N})` enlarges a pointer-arg buffer past the 0x100
  default (car is 0x2b68) so every field is a real slot; `_setup_img` uses a
  running allocator so enlarged buffers never overlap.
- the `buf` hook seeds the object's POINTER FIELDS to point at scratch
  sub-objects, which the `bss` hook then fills. BrCtlAiBody: pProfile (f68 bit0
  CLEAR=controller on), pCtl, and a path node whose knot arcs DECREASE and whose
  left/right form a real corridor (left=centre+offset, right=centre-offset), plus
  per-seed varied UNIT frame rows + pos/vel/aim.
- `exact_regions` on the integer AI globals (bias/scan state) + the car's
  counter/flag/gate slots -- they alias tiny denormal floats, so the default
  compare masks a wrong integer decision as FP rounding ([objdl-a5-limit-2026-09-16](objdl-a5-limit-2026-09-16.md)).

** SEEDING GOTCHAS burned in (each was a runaway/false-pass):**
- A null spline pointer -> the segment-search loop (budget `t -= arc[i]-arc[i+1]`,
  exit when <0) never terminates. Seed a real node.
- `pSib` MUST be NULL: the corridor scan's `while(pChild) pChild=pChild->pSib`
  spins on a self-ring. `pNext`->self is fine (budget bounds the main walk).
- Value-range coverage: FIXED frame axes gave heading==dot(fwd,lateral) magnitude
  ~28, so the +-0.05 threshold bands were never hit -> bugs missed. UNIT frame
  rows put heading in [-1,1] -> thresholds testable. Realistic corridor width
  (offset ~6) put halfWidth in the (5,8) `limit` band. Coverage teeth need the
  seeded values in the function's REAL operating ranges.

** EMULATOR (x87emu.py, committed bd63991f):** fsin/fcos/fsincos/fptan/fpatan;
ftol of NaN/inf/out-of-range -> x86 integer-indefinite 0x80000000 (not raise).
Needed to run FP math on the seeded world.

**$T resolver (50a62fb5) CONFIRMED WORKING on the codec methods** (peer's report
of a $T345-350 gap was a misdiagnosis): on 0x1000C4E0 Rip0C4E0::Apply all 10 $T
.rdata float relocs resolve; the real blocker is `_g_brABE44` (.bss global, no
source decl / no learned addr / not clean g_<HEX>) -- the peer's rename lane, not
the $T path. A .bss global with no address is resolvable only by rename or a
`extern T g_x; /* 0x<VA> */` annotation (honoured by _declared_data_va, subject to
the stale-annotation guard [objdl-a5-limit-2026-09-16](objdl-a5-limit-2026-09-16.md)).

See [brtex3dexpand-t3-2026-09-16](../functions/brtex3dexpand-t3-2026-09-16.md), [oracle-runs-orchestrators](oracle-runs-orchestrators.md),
[t3-certified-standard](../rules/t3-certified-standard.md), [parallel-session-clobber](../traps/parallel-session-clobber.md).
