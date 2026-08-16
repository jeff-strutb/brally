/* br_collresp.h -- the collision half of 0x10067C30, the position pass.
 *
 * REFERENCE IS orig/BRGlide.dll.  Every address below was checked with
 * tools/whereis.py first; none of the four callees is ported anywhere else in
 * this tree under either build's number.
 *
 * ======================================================================
 * WHAT 0x10067C30 REALLY CALLS, read off the bytes
 * ======================================================================
 * br_carphys.c's BrCarPhysAdvance modelled the substep loop as
 *
 *     loop { hook; BrRbIntegrateState; BrRbBuildMatrix; t -= 1/120; ... }
 *
 * and that is structurally short of the original in three places.  Walking
 * 0x10067C30 with ESP tracked (R == esp on entry, so arg1 == pCar is at R+4
 * and arg2 == pBody at R+8 -- and the function OVERWRITES its own arg2 slot
 * with the loop's remaining time at 0x10067CFE):
 *
 *   0x10067C9A  a 0x4C-byte frame is carved into a 12-byte SCALE vector at
 *               R-0x4C and a 0x40-byte MATRIX at R-0x40.  They OVERLAP: the
 *               matrix runs R-0x40..R-0x01 and the scale's notional
 *               BrMat4 tail (+0x30..+0x38, which 0x1006DDD0 reads as the
 *               translation) lands on R-0x1C..R-0x14, i.e. on the matrix's
 *               own m[2][1..3].  That is not a misreading of the frame; the
 *               two `lea`s are 0xC apart and 0x1006DDD0 writes 0x40 bytes.
 *               See BrCollRespFrame.
 *   0x10067CC3  BrMat4BuildScaledTransposed(&body->m, matBox, scale) with
 *               scale == (0.1, 0.1, 0.1) -- three 0x3DCCCCCD immediates --
 *               then 0x10066AD0(body, matBox): the BROAD PHASE, once per
 *               frame, against a box ten times the car's own.
 *   0x10067CEF  the scale slots are then OVERWRITTEN with
 *               (1/f1DC, 1/f1E0, 1/f1E4) -- `fld [ebx+0x1dc]` +
 *               `fdivr [0x10077A7C]` (1.0f) -- for the in-loop rebuild.
 *   0x10067D30  the loop, and its body is
 *                   0x10066D70(body)              <- NOT a printf test
 *                   0x10068F80()                  <- car vs car
 *                   BrRbIntegrateState(next, save, 1/120)
 *                   BrRbBuildMatrix(&body->m, next)
 *                   BrMat4BuildScaledTransposed(&body->m, matBox, scale)
 *                   r = 0x10067710(body, matBox)
 *                   if (r) { BrRbQuatDerivative(next);
 *                            BrRbBuildMatrix(&body->m, next); }
 *                   t -= 1/120;  save = next (17 dwords);  while (t > 0.002)
 *
 * THE CONDITIONAL PAIR AT 0x10067DA7 IS THE WHOLE POINT.  0x10067710 returns
 * non-zero only when it has rewritten `next`, and the qDot rebuild + matrix
 * rebuild are what make that rewrite stick for the rest of the frame.
 *
 * A DEAD ACCUMULATOR, kept out.  0x10067D84..0x10067D97 does
 * `[R-0x08] = [R-0x08] - f1E8` once per substep.  R-0x08 is never written
 * anywhere else in the function and never read anywhere else, so it starts as
 * whatever the caller left on the stack and nothing consumes it.  It is not
 * reproduced, and unlike the duplicated BrRbQuatDerivative in 0x1005A7A0 it
 * cannot be: there is no object for it to live in.
 *
 * ======================================================================
 * 0x10066D70, 1782 B -- AND WHY br_carphys.h WAS WRONG ABOUT IT
 * ======================================================================
 * br_carphys.c called this "a test whose non-zero result only drives a
 * printf".  The return value does only drive a printf -- and the printf is
 * 0x10008D60, which is ONE BYTE, `c3`, a bare `ret`, so it is a stub in this
 * build.  But the function is not a test.  It ENDS by writing the chassis's
 * angular velocity:
 *
 *      0x1006740E   save.angVel.x = save.angVel.x - (out.x * -2.0f)
 *      0x10067425   save.angVel.y = save.angVel.y - (out.y * -2.0f)
 *      0x1006743B   save.angVel.z = save.angVel.z - (out.z * -2.0f)
 *      0x10067447   BrRbQuatDerivative(&save)
 *
 * with `out` the body vector (0, +-0.1, 0) rotated into the world by the
 * chassis matrix -- i.e. +-0.2 rad/s about the car's own LATERAL axis, which
 * is PITCH.  Two passes went looking for a pitch response and this is the
 * only unported function in 0x1005A7A0 that writes one directly.
 *
 * It is NOT a damper.  The gate is
 *
 *      1 <= (wheels with a contact count) <= 2      0x100672C6 / 0x100672CF
 *      min corner-to-plane distance <= 0.6          0x100672F0, a DOUBLE
 *      |dot(plane.n, velocity of that corner)| <= 0.1   0x1006736F
 *
 * so it fires when the car has come to REST balanced on one or two wheels,
 * and it topples it.  A corner moving fast into the ground -- which is what a
 * diverging pitch looks like -- fails the third test and gets nothing.  That
 * matters for the divergence hunt and is why it is spelled out here: this
 * function cannot stabilise a car that is already moving.
 *
 * THE SIGN, and the one field the port cannot get from the original.  The
 * +-0.1 is chosen at 0x100673BD by
 *
 *      s = dot( (body+0x1A4, +0x1A8, +0x1AC), body->m row 0 )
 *      vec.y = (s > 0) ? +0.1f : -0.1f
 *
 * body+0x1A4..+0x1AC is the CHASSIS's own contact-plane normal.  Grepping
 * every store in both builds, the only writer of +0x1A4..+0x1B0 is
 * 0x10068070, the WHEEL ground probe, and its four call sites (0x10068450)
 * pass the four child bodies -- never the chassis.  So nothing in the whole
 * physics path writes the chassis's copy; it holds whatever the car record
 * held.  BrCarPhys models it as an explicit zero vector, which selects the
 * -0.1f arm, because `fcom` + `test ah,0x41` takes that arm for EQUAL as well
 * as for less.  DEVIATION: the original would read an uninitialised field if
 * the record were not zeroed.  Stated rather than hidden.
 *
 * ======================================================================
 * WHAT IS STILL MISSING, and the measurement that pins it
 * ======================================================================
 * 0x10066AD0 (the broad phase) and 0x10067710 (the response) are an
 * OBB-versus-triangle system.  The box is the car's, and its three half
 * extents come from body+0x1DC / +0x1E0 / +0x1E4 with body+0x1E8 as a Z
 * offset -- BrCarPhys's f1DC / f1E0 / f1E4 / f1E8.
 *
 * The car constructor (Glide 0x1005BCC0, the twin of the D3D 0x10062C50 that
 * br_carphys.h already quotes -- 0x1005BCCB writes car+0xE84 = 1, exactly the
 * D3D 0x10062C5B br_carphys.h names) sets them at 0x1005BD40 / 0x1005BD42 /
 * 0x1005BD48 / 0x1005BD4E to
 *
 *      f1DC = 0.0f    f1E0 = 0.0f    f1E4 = 2.0f    f1E8 = 0.0f
 *
 * and the real values arrive later, from 0x10059A80, which copies a car-data
 * record's +0x10 / +0x14 / +0x18 / +0x1C into them.  That record is filled by
 * 0x10063B80 out of 0x10B73668 + 24 * (...) -- an address inside .data's
 * VIRTUAL range but far past its 0x41E00 bytes of raw image, i.e. it is
 * filled from the CARS/ files at run time.  This port does not load them.
 *
 * ONE THING THE NEXT PASS MUST NOT INHERIT.  0x10065C80, the impulse solver
 * at the end of that chain, calls 0x1006DD80.  That is 0x10074B20 in the D3D
 * build, which slice3_44.h declares as `BrVec3SubRepeated` and documents as a
 * PRESERVED BUG -- "the outer loop resets the cursor, so the same three
 * subtractions run three times".  It does not.  `mov eax, edi` sits at
 * 0x10074B38, ONE INSTRUCTION BEFORE the outer loop's jump target 0x10074B3A,
 * so the cursor is never reset and the two loops walk NINE elements: it is a
 * packed 3x3 matrix subtract.  The Glide bytes at 0x1006DD80 are identical.
 * 0x10065C80's use settles it -- it passes a 3x3 identity whose diagonal has
 * been overwritten with 1/mass, and a 3x3 built by three chained BrMat3Mul
 * calls; three subtractions there would be meaningless.  slice3_44.c is not
 * this module's to edit, so the correction is filed rather than made, and
 * whoever ports 0x10065C80 must not reach for BrVec3SubRepeated as it stands.
 *
 * With f1DC == f1E0 == 0 the reciprocals at 0x10067CEF are infinite, so the
 * box matrix is degenerate and every transformed triangle vertex classifies
 * as "outside" -- 0x10066260's `fcom 0.5` + `test ah,0x41` sends a NaN down
 * the same arm as a value below -0.5, so all three vertices agree and the
 * broad phase rejects.  The OBB system is therefore INERT, not explosive,
 * until somebody loads the car data.  That is the state this module ships in
 * and it is measured, not assumed: BrCollRespBoxDegenerate() reports it.
 */
#ifndef BR_COLLRESP_H
#define BR_COLLRESP_H

#include <stdint.h>

#include "br_vec.h"
#include "br_mat.h"
#include "slice3_42.h"   /* BrRbBodyFull, BrRbVelAtPoint                */
#include "slice3_44.h"   /* BrRbState, BrRbQuatDerivative, BrMat4Build* */
#include "br_phys.h"     /* BrGroundHit -- the six fields the LP64 DEVIATION
                          * moved out of BrRbBodyFull; see br_phys.h        */

/* 0x10077B78, a DOUBLE (`fcomp qword ptr`).  The nearest box corner must be
 * within this of its wheel's contact plane. */
#define BR_CR_TIP_NEAR      (0.6)

/* 0x10077AF4.  ...and must be nearly at rest against it.  Same address the
 * tyre pass reads as BR_CP_TYRE_SPIN; one constant, two unrelated uses, so it
 * is named again rather than shared. */
#define BR_CR_TIP_STILL     0.10000000149011612f

/* 0x10077B70.  The initial "no corner yet" distance. */
#define BR_CR_TIP_FAR       100.0f

/* 0x100673B3 / 0x100673CA, the two immediates 0x3DCCCCCD and 0xBDCCCCCD --
 * the same 0.1 the broad phase scales with, reached from a different place. */
#define BR_CR_TIP_KICK      0.100000001490116119f

/* 0x10077A84.  The kick is `angVel - out * -2`, i.e. +2 * out.  Same address
 * br_carphys.h names BR_CP_DRV_BRAKE_K. */
#define BR_CR_TIP_GAIN     (-2.0f)

/* 0x10077AC8.  Half -- the box half-extents. */
#define BR_CR_HALF          0.5f

/* 0x10067CAB / B3 / BB.  The broad phase's scale, three immediates. */
#define BR_CR_BROAD_SCALE   0.100000001490116119f

/* 0x10077A7C.  The 1.0f the in-loop scale is `fdivr`'d out of. */
#define BR_CR_ONE           1.0f

/* ======================================================================
 * The 0x4C-byte frame 0x10067C30 hands to 0x1006DDD0
 * ======================================================================
 * `scale` is 0x10067C30's R-0x4C..R-0x41 and `mat` is its R-0x40..R-0x01, so
 * `mat` starts twelve bytes above `scale` and 0x1006DDD0's read of
 * `pS->m[3][0..2]` (its own +0x30..+0x38) lands on mat->m[2][1..3].  Modelled
 * as one array so the overlap is the same one the original has rather than
 * something a compiler is free to lay out differently. */
typedef union BrCollRespFrame {
    float  a[0x4C / 4];
    /* a[0..2]  == the scale vector, 0x1006DDD0's pS->m[0][0..2]
     * a[3..18] == the matrix, i.e. (BrMat4 *)&a[3] */
} BrCollRespFrame;

BrMat4 *BrCollRespFrameMat(BrCollRespFrame *pF);

/* 0x1006DDD0 == BrMat4BuildScaledTransposed, but called the way 0x10067C30
 * calls it: pOut and pS are the same frame, overlapped.  Kept as one entry
 * point so no caller can accidentally un-overlap them. */
void BrCollRespBuildBoxMatrix(BrCollRespFrame *pF, const BrMat4 *pBody,
                              float sx, float sy, float sz);

/* ======================================================================
 * 0x10066D70, 1782 B
 * ======================================================================
 * See the banner.  Returns non-zero when it applied the kick.
 *
 * DEVIATIONS, both forced by br_phys.h's BrGroundHit:
 *  - the original reads each wheel's contact plane out of wheel+0x1A4..+0x1B0;
 *    those six fields live in BrGroundHit here, so this takes the four-entry
 *    array alongside the body.
 *  - the chassis's own +0x1A4..+0x1AC is `pBodyPlaneN`; see the banner for why
 *    it is always zero.
 *
 * pSave is body+0x114 and pNext is body+0x158 in the original; the port keeps
 * them as BrCarPhys members, so they are passed. */
int BrCollRespTipKick(BrRbBodyFull *pBody, const BrGroundHit aHit[4],
                      const BrVec3 *pBodyPlaneN, BrRbState *pSave,
                      float f1DC, float f1E0, float f1E4, float f1E8);

/* Non-zero when the car's collision box is the constructor's degenerate one,
 * i.e. when 0x10067C30's reciprocals are not finite and the OBB chain cannot
 * run.  A measurement, so that "no collisions were reported" can be told
 * apart from "the collision code never ran". */
int BrCollRespBoxDegenerate(float f1DC, float f1E0, float f1E4);

/* How many substeps found a degenerate box, and how many kicks were applied.
 * Counted for the same reason br_carphys.h counts its holes: a silent no-op
 * makes "the car moved" unfalsifiable. */
extern uint32_t g_cBrCollRespTipKick;
extern uint32_t g_cBrCollRespDegenerate;
void BrCollRespCountersReset(void);

#endif /* BR_COLLRESP_H */
