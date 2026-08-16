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

/* Non-zero when the car's collision box is not usable, i.e. when
 * 0x10067C30's reciprocals are not finite and the OBB chain cannot run.  A
 * measurement, so that "no collisions were reported" can be told apart from
 * "the collision code never ran".
 *
 * The box comes from the .rca; br_cardata.h has the chain, and it also has
 * the adjudication of the (0, 0, 2, 0) claim this header used to make. */
int BrCollRespBoxDegenerate(float f1DC, float f1E0, float f1E4);

/* ======================================================================
 * THE BROAD PHASE, 0x10066AD0 and its six callees -- NOW PORTED
 * ======================================================================
 * Everything below was a hole until the car data landed, because with two
 * extents at zero the box matrix is all infinities and every triangle
 * classifies out.  With a real box it is live, and it is the half of the OBB
 * system that can be demonstrated on its own: it produces a COUNT.
 *
 *   0x10066AD0   669 B   gather                  BrCollRespBroadPhase
 *   0x10066AA0    35 B   the two-stage test      (static)
 *   0x10066230    38 B   push onto the list      (static)
 *   0x10066260   644 B   the 26-plane classify   BrCollRespBoxClassify
 *   0x100664F0   278 B   its corner-plane arm    (static)
 *   0x10066950   322 B   the exact test          (static)
 *   0x10066800   332 B   segment versus cube     BrCollRespSegBox
 *   0x10066610   492 B   point in triangle       BrCollRespPointInTri
 *
 * WHAT IT IS.  The box matrix 0x10067C30 builds maps the world onto the
 * car's box scaled to the UNIT CUBE [-0.5, 0.5]^3 -- the halves at
 * 0x10077B48 / 0x10077B50, both DOUBLES.  Each candidate triangle's three
 * vertices are transformed into that space and tested against the cube:
 *
 *   0x10066260 is a 26-plane outcode reject: the 6 faces (+-0.5 per axis),
 *   the 12 edge planes (x+-y, x+-z, y+-z against +-1, 0x10077B58 /
 *   0x10077B60) and the 8 corner planes (+-x+-y+-z against +-1.5,
 *   0x10077AB0 / 0x10077B68).  It returns 1 (a vertex is inside all six
 *   slabs -- definite hit), 0 (all three vertices share an outside side of
 *   one plane -- definite miss) or -1 (inconclusive).
 *
 *   0x10066950 resolves the -1: three segment-versus-cube tests on the
 *   triangle's edges (0x10066800), and if all three miss, the triangle's
 *   own plane is intersected with the cube diagonal that its normal points
 *   along and the intersection point is tested against the triangle by a
 *   2D crossing count (0x10066610).  That last case is the one a cheaper
 *   test gets wrong: a big triangle that slices the box without any vertex
 *   or edge inside it.
 *
 * ON THE NaN ARMS, because they are load-bearing here.  Every one of these
 * compares is a negated x87 test and NaN takes the side the tidy form does
 * not.  In 0x10066260 a NaN coordinate classifies as BELOW -0.5 (the
 * `fcom 0.5` sets C0|C2|C3, so `test ah,0x41` sends it to the second test,
 * where `test ah,1` is also set) -- so all three vertices agree and the
 * triangle is REJECTED.  That is exactly why an infinite box matrix makes
 * this system inert rather than explosive, and it is why the counter below
 * is the thing to look at rather than "did anything crash".
 *
 * THE LIST.  0x10066230 pushes onto a singly linked list whose head is
 * 0x11778198 and whose nodes come from a bump allocator at 0x11778844,
 * reset to 0x117781B0 by 0x10067C54.  The pool runs to 0x117787F0, the next
 * named global, so it is 0x640 bytes == 200 eight-byte nodes; a cell holds
 * at most BR_COLL_CELL_PLANES (150) records, so one frame cannot overflow
 * it and the original's missing bounds check costs nothing.  The size is
 * enforced here anyway, and an overflow is COUNTED rather than silently
 * dropped.
 *
 * WHAT IS STILL MISSING is 0x10067710 (1301 B) and its impulse solver
 * 0x10065C80, i.e. the RESPONSE.  It is the consumer of this list.
 * br_carphys.h's BR_CP_HOLE_BOX now names that alone. */

typedef struct BrCollRespNode {
    const BrCollPlane     *pPlane;   /* node+0, the record 0x10066230 stores */
    struct BrCollRespNode *pNext;    /* node+4                               */
} BrCollRespNode;

/* (0x117787F0 - 0x117781B0) / 8 */
#define BR_CR_LIST_MAX  200

/* 0x11778198 -- the head 0x10067710 would walk. */
extern BrCollRespNode *g_pBrCollRespList;

/* 0x10067C4E..0x10067C8E: the head and the bump cursor, cleared once per
 * frame before the broad phase runs. */
void BrCollRespListReset(void);

/* 0x10066AD0.  Gathers the cell under (pBody->m.m[3][0], m[3][1]) into the
 * list and returns how many records it added.  pMatBox is the box matrix
 * BrCollRespBuildBoxMatrix produced. */
int BrCollRespBroadPhase(const BrRbBodyFull *pBody, const BrMat4 *pMatBox);

/* 0x10066260, exposed because its three-valued answer is the single easiest
 * thing here to collapse into a bool.  aV is three transformed vertices,
 * nine floats.  Returns 1 / 0 / -1 as the banner describes. */
int BrCollRespBoxClassify(const float aV[9]);

/* 0x10066800 -- does the segment pA..pB meet the unit cube? */
int BrCollRespSegBox(const BrVec3 *pA, const BrVec3 *pB);

/* 0x10066610 -- the 2D crossing count of pP against the triangle aV,
 * projected away from pN's dominant axis.  Non-zero means inside. */
int BrCollRespPointInTri(const float aV[9], const BrVec3 *pN,
                         const BrVec3 *pP);

/* How many substeps found a degenerate box, how many kicks were applied,
 * how many times the broad phase ran, and how many triangle records it has
 * gathered in total.  Counted for the same reason br_carphys.h counts its
 * holes: a silent no-op makes "the car moved" unfalsifiable -- and the last
 * two are how a run shows the collision system RUNNING rather than merely
 * linking. */
extern uint32_t g_cBrCollRespTipKick;
extern uint32_t g_cBrCollRespDegenerate;
extern uint32_t g_cBrCollRespBroad;
extern uint32_t g_cBrCollRespGathered;
extern uint32_t g_cBrCollRespOverflow;
void BrCollRespCountersReset(void);

#endif /* BR_COLLRESP_H */

/* ==========================================================================
 * LEAD FROM THE N64 BUILD -- naming and structure only, NOT ground truth.
 *
 * A sibling analysis of Top Gear Rally (N64, 1997) reports a ~17 KB, 30-function
 * collision / resting-contact module at vram 0x8025C000-0x80260000 with its
 * DEBUG STRINGS INTACT:
 *
 *     "Triangle Edge to CubeFace"          0x8025DFCC
 *     "Cube Edge to Triangle Face"         0x8025DFCC
 *     "Resistive collision %10.3f"         0x8025DFCC
 *     "Stand Dist " / "Stand Vel " / "Stand Point "   0x8025D368
 *     "Standing on it's F'in Nose damnit"  0x8025E55C
 *
 * WHY THIS IS WORTH HAVING. Two of those bear directly on this file:
 *
 *   - The edge/face pair independently corroborates the carving arrived at
 *     here from the PC bytes alone: a BOX tested against track triangles,
 *     resolved by explicit edge-versus-face cases rather than one generic
 *     contact routine. Two analyses of two builds reaching the same shape is
 *     worth more than either alone.
 *
 *   - "Standing on it's F'in Nose damnit" is a guard against EXACTLY the
 *     failure this port exhibits: the car pitching up onto its nose. If the PC
 *     block carries a corresponding clamp and it is among the unported bytes,
 *     that explains the divergence as a MISSING GUARD rather than a wrong
 *     force -- a different fix, and a much cheaper one.
 *
 * WEIGH IT CORRECTLY. This is the 1997 N64 title, not the 1999 PC game. Same
 * studio and demonstrably the same lineage (shared display-list format, shared
 * "Track header size mismatch" string), but the physics may have been revised
 * between them. Nobody has decompiled the N64 module or diffed the two.
 *
 * So: use it to NAME things and to check whether a carving looks wrong. Do not
 * use it to decide what the PC code does -- that must still come out of
 * BRGlide.dll. A structure hint from a sibling title is evidence about where to
 * look, not about what is there.
 * ========================================================================== */
