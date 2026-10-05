# Five largest

*Recorded 2026-09-13.*

> PARTLY SUPERSEDED - several of these are now CERTIFIED T3 via A5 (0x1000E320 BrSceneVisPrepare 2026-09-20, 0x10001CF0 BrCamChaseStep). The 'ZERO T4/T3, all fail Gate A' headline is dead under the A5 regime. Keep the x87 operand-role class note + the two tool fixes.

**Picked the five largest non-EH, non-C++-lane T1 rows** from `tiers.py
--list` after screening (0x1002F790 is `6a ff` = C++ EH, skipped). Result:
five committed T2 rows, no byte-exact, no certifiable T3 -- every one fails
Gate A on A2/A3 (rows) because the residue repeats per statement.

| VA | file | state |
|---|---|---|
| 0x1000E320 BrSceneVisPrepare | scene/br_scenevis.c | 1992/1992 B, 543/543 insns, 63 B in 2 regions |
| 0x10032E40 BrCarTrailStep | drawing/br_cartrail.c | 1875/1881 B, 438/454 insns, frame exact |
| 0x10001CF0 BrCamChaseStep | drawing/br_chasestep.c | 1578/1567 B, 408/403 insns, 7 regions |
| 0x10068F80 BrCarCarCollide | driving/br_carcol.c | 1444/1444 B, 396/396 insns, 7 regions |
| 0x10068900 BrObbOverlap | driving/br_obb.c | 1671/1653 B, 641/642 insns, 27 regions |

** THE ONE CLASS behind all five (name it before spending a probe on it):
a stack-resident x87 value vs a memory operand.** The original puts the
LOCAL / stack temp on the `fld` side and the field or parameter element as
the memory operand (`fld [esp+S]; fmul [edi+4]`, `fld imp.x; fadd [ebp+0xc]`,
`fxch; fmul K` on the live value); our build does the reverse (`fld [edi+4];
fmul [esp+S]`, `fld field; fadd local`, `fld K; fmulp st(2)`). Same
compiler, same flags: /G3-/G6, /Op (fixes the orders but adds reloads),
/Oa, /Ob1, VC4.2 all measured. Source operand order, pointer copies of the
parameters, array-typed locals, named temps, double-typed temps, compound
assignment: all inert. The idiom dictionary's operand-KIND ladder
(VC5-IDIOMS ~line 709, 1587) is the right frame but its stated ranks do not
predict these sites -- a local slot ranks ABOVE `p[k]` in the originals and
BELOW it in ours. Whatever decides it is not spelled by any construct tried.
Do not re-run these probes; find the ladder rule with a micro-TU sweep.

**Levers that DID pay (per function, all in the file headers):**
- 0x1000E320: `thr` as a TERNARY fixes the whole frame's slot order (nested
  if / if-else forms put the loop counter in the top slot); car walk as an
  explicit pointer in the for-increment (IV update order); `g_idx[k]` read
  at both uses instead of a local (`lea edx,[ecx*8]` x7 multiply); the
  airplane record's position via a BrVec3 pointer (`lea` with +0x30 folded);
  do-while over a `short *` to the distance word (+2 induction base; index
  or struct pointer base at +1); span store before the pending-byte clear.
  msetdiff reloc fix removed six phantom rows (see below).
- 0x10032E40: (float)dx CAST expressions, not fx locals (the `fst` right
  after `fild` is the CSE temp; gives local-first x87 add order); named
  nx/ny/nz for the origin update; switch laid out 4,3,default,0 with case 3
  falling into default; shorts widened at the fild; idx = kk + j.
- 0x10001CF0: the four camera vectors are 16-byte (struct bug found via
  offsets); thiscall helpers take struct-wrapped stack args (no `xor edx`);
  `k` ternary; two call sites for the lift (tail-merged); frame copy inside
  the argument expression `(car->frame2 = car->frame, &...)`; the first
  height store through a pointer keeps the dead 0.02 store.
- 0x10068F80: the "push" vector at +0x2C8 IS the saved velocity (the 0x44 B
  save block covers it); `sd` must be memory-resident (volatile reproduces
  the store/reload -- probably an inlined vector helper); `tone` local.
- 0x10068900: `ok = 1; ok &= test;` gives the `and eax,1` / `and eax,ecx`.

**Tools:** msetdiff.norm's tail-reloc test now requires all four trailing
bytes reloc'd (disp32+imm8 forms mis-rewrote the immediate; validator
121/121 ok); fn.py honours `FN_OPTS='/O2 /Op'` etc. for one-off option
probes. `t3.py` reads a prose "certified @t3 in" as a malformed tag --
reworded in slice2_12.c. A VC5 compile HUNG on one tail reordering of
0x10001CF0 (td variant) -- kill wine, do not wait.

Related: [five-largest-2026-09-13](five-largest-2026-09-13.md), [five-largest-2026-09-12b](five-largest-2026-09-12b.md),
[largest-to-t3-survey-2026-09-12](largest-to-t3-survey-2026-09-12.md), [do-not-lower-t3-standard](../rules/do-not-lower-t3-standard.md).
