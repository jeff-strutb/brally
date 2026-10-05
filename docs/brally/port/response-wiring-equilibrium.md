# Response wiring equilibrium

*Recorded 2026-08-17.*

> RESOLVED - flat-settle equilibrium is 0.190132 (suspension only); the OBB does NOT fire at rest because the f1E8 box offset lifts the classify box clear of the ground. The in-tree "dead accumulator" header claim was WRONG.

# Flat-settle equilibrium - RESOLVED 2026-08-17 (0.19 is correct)

The open 0.19-vs-0.40 question is **settled: 0.190132 (suspension only) is the
faithful number.** The OBB collision response does NOT fire on flat ground at
the suspension rest. Ground-truth proof, static + oracle:

**The f1E8 chassis-box offset is LIVE, applied to the box matrix, not dead.**
The caller `BrCarPhysAdvance` (0x10067C30) at 0x10067D84..0x10067D97 does
`matBox.m[3][2] -= f1E8` (byte 0x38 of the box matrix at `[esp+0x1c]`) in the
substep loop, RIGHT BEFORE calling the walker 0x10067710, and passes that same
matBox. Gapless machine-code proof:
- 0x1006DDD0 (BrMat4BuildScaledTransposed) writes bytes 0x30/0x34/**0x38** as
  the box's live Z-translation (via the 0x1006D9D0 MulVec3Transposed result),
  m[3][3]=1.0 - standard row-major BrMat4, translation in row 3.
- 0x1006DA20 (BrMat4TransformPoint) tail adds `m[3][2]` (byte 0x38) into
  `out.z`. So decrementing byte 0x38 shifts every transformed triangle's
  box-space Z by -f1E8.
- Oracle (scratch box_offset_test.py, real bytes): rest state pos
  (10,10,0.190132) level, scale (1/3.5,1/2.0,1/0.8). Ground world-z=0 maps to
  box-z **-0.2377 with NO offset** (inside [-0.5,0.5] -> FIRES -> floats to
  ~0.40) but **-0.9377 WITH offset** (outside -> box clears ground -> does NOT
  fire -> stays 0.190132). Box world-z range at rest: no-offset [-0.21,0.59]
  (contains ground), with-offset [0.35,1.15] (above ground).

**CORRECTION OWED to br_collresp.h (~lines 48-53):** the "A DEAD ACCUMULATOR,
kept out" section is WRONG. `[R-0x08]` IS matBox.m[3][2] (S+0x54 where matBox is
at S+0x1c, offset 0x38), it IS read - by the walker's BrMat4TransformPoint - and
it is the box's Z lift. The prior session mis-identified the slot as outside the
matrix. My earlier wiring experiment floated to 0.40 precisely because it
omitted this offset.

**NOTE:** f1E8 is used TWICE and both are real: (1) the box-matrix Z offset
here (caller applies it), and (2) inside the walker as ext[3] in the box-corner
target `g_brCrPlane.normal.z = ext[2]*sign.z + ext[3]` - the port's walker
already does (2). The wiring adds (1).

**LANDED (2026-08-17, uncommitted):** br_carphys.c `BrCarPhysAdvance` now, after
`BrCollRespBuildBoxMatrix`, does `pMatBox->m[3][2] -= pCar->f1E8;` then calls
`BrCrRespWalk(mass, &invInertia, &m, ext{f1DC..f1E8}, &next, &save.pos,
(BrVec3*)&save.quat, &eff{0}, pMatBox)` where the pfnCollide hole was, with the
`if (r) { BrRbQuatDerivative(&next); BrRbBuildMatrix(&m, &next); }` conditional
pair. pfnCollide hook removed; deps add `br_collrespsolve`. **135/135 green.**
The header's dead-accumulator paragraph in br_collresp.h is corrected in-tree.

**DYNAMIC behaviour observed (C, real box 3.5/2.0/0.8/off 0.7):** measured
carefully - from ANY drop that settles to 0.190132 (≤~1.8 m) the box NEVER fires
(the offset holds it clear at the spring rest and a gentle fall never breaches
it); only a hard drop ≥2 m breaches the box, and that FIRES and BOUNCES (does
NOT settle in 401 frames - e.g. 2 m → 516 contacts, ends z≈1.40). So no single
drop both fires and settles. (An earlier note here wrongly said "≤1 m fires ~11×
and settles" - the 11 was the whole suite's fire count, not the 1 m settle's;
corrected in commit 15430a7.) The flat-settle tests drop from 1 m and assert the
response stayed QUIET (==0) - that quiet IS the box-lift check, since a box that
fired on flat ground is the 0.40 bug. A separate TestResponseFires drops from
3 m and asserts the walker DID fire (proves wiring live from the other side).
**CAVEAT:** the ≥2 m bounce is assumed-faithful (every component is a verified
transcription; walker == real bytes over 14000 cases), not oracle-checked
end-to-end; the definitive check is the real 401-frame 0x10067C30 loop in
x87emu, deferred as low-ROI (hard free-drops don't occur in gameplay - the game
places cars ON the surface; z=2.0 is only the constructor default, overridden by
BrCarPhysPlace at race start). See [collision-response-decomp-state](collision-response-decomp-state.md).
