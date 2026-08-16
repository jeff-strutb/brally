/* br_carphys.h -- the per-frame car physics step, 0x1005A7A0.
 *
 * REFERENCE IS orig/BRGlide.dll.  The D3D twin is 0x10061720 and the two are
 * `matched by body` in config/shared.csv; every address below was checked with
 * tools/whereis.py before a line was written, and TWO of the answers that tool
 * gave were WRONG in a way worth recording:
 *
 *   0x1006D850  whereis reports "paired 0x10066800 (glide)", i.e. it read the
 *               address as a D3D one and matched a DIFFERENT function of the
 *               same number.  0x1006D850 is a real Glide function (239 B) and
 *               its D3D twin is 0x100745F0 == BrRbIntegrateState, which is
 *               ALREADY PORTED in slice3_44.c.  The whole neighbourhood pairs
 *               on a constant Glide->D3D delta of 0x6DA0
 *               (0x1006D530/0x100742D0, 0x1006D600/0x100743A0,
 *               0x1006D6B0/0x10074450, 0x1006DDD0/0x10074B70,
 *               0x1006D4B0/0x10074250), and 0x1006D850 + 0x6DA0 == 0x100745F0
 *               sits exactly there.  Confirmed by reading the body: it is the
 *               explicit-Euler step (pos += dt*vel, quat += dt*qDot,
 *               renormalise) with the arguments (dst, src, dt).
 *   0x100645A0  whereis reports "paired 0x1006B5F0 (d3d) matched by prefix".
 *               That address IS in the physics neighbourhood (delta 0x7050),
 *               but br_sfx.c also names 0x1006B5F0 -- a DIFFERENT function in
 *               a different build (BrSfxHzFromRatio).  The pairing is the
 *               tool reading a Glide address as a D3D one; 0x100645A0 is
 *               ported here as BrCarPhysDrive and 0x1006B5F0 stays br_sfx's.
 *
 * ======================================================================
 * WHAT 0x1005A7A0 IS
 * ======================================================================
 * __thiscall on the 0x2B68-byte car record, ecx == pCar.  One frame at a
 * FIXED timestep: the immediate 0x3D088889 (1/30 s) is pushed at every
 * integrator call site inside it; there is no accumulator and no frame-time
 * input.  br_phys.h already names that constant BR_PHYS_DT.
 *
 * It is a TWO-PASS force solve with a position integration between the two,
 * and the two passes use TWO DIFFERENT FORCE LISTS hung off the same body:
 *
 *   pass A   body->pForces = car+0xBA0 :  four zero nodes at the wheel
 *            corners, which 0x100684F0 fills with the SUSPENSION SPRING, then
 *            one node carrying GRAVITY.
 *   pass B   body->pForces = car+0xC20 :  four zero nodes at the same corners,
 *            which 0x10068600 fills with the SHOCK ABSORBER, then one node
 *            which 0x10067F30 fills with AERODYNAMIC DRAG.
 *
 * Between them sits the sign-change damper (below), which is what stops pass
 * B's dissipative forces from reversing the velocity they are damping.
 *
 * THE TEN STEPS, in the original's order:
 *
 *    1. 0x1005A7BE  re-point body->pForces at car+0xBA0 and each wheel's at
 *                   car+0xD20 / 0xD60 / 0xD40 / 0xD80 -- NOTE the order, the
 *                   middle two are crossed relative to the child array -- and
 *                   zero the force vector of each wheel's FIRST node.
 *    2. 0x1005A8A0  0x100684F0: the suspension spring, into pass A's nodes.
 *    3. 0x1005A8A5  if (car->fE84 == 0) four calls to 0x100651A0, the per-
 *                   wheel tyre force -- PORTED, BrCarPhysTyre.  The gate byte
 *                   is &fE80 for wheels 0/1 and &fE78 for 2/3, which an
 *                   earlier reading of these four call sites had wrong.
 *    4. 0x1005A943  car->fE84 = 0; zero body->accel and body->angAccel.
 *    5. 0x1005A96E  0x10064210 == BrRbAccumAll: forces -> accelerations.
 *    6. 0x1005A983  0x1006D600 == BrRbIntegrateVelocity on the LIVE state.
 *    7. 0x1005A9AD  0x100645A0(body, dt, &fE7C, &fE74, &fE80, &fE78) --
 *                   PORTED, BrCarPhysDrive.  It is the axle velocity
 *                   constraint, not an engine; see its banner below.
 *    8. 0x1005A9B6  0x1006D530 == BrRbQuatDerivative on the live state, then
 *                   swap to pass B's list, drop the wheels' lists to NULL,
 *                   0x10067F30 (drag) and 0x10068600 (dampers), zero the
 *                   accelerations again and re-run BrRbAccumAll.
 *    9. 0x1005AA34  copy live -> `next`, integrate `next`'s velocity a SECOND
 *                   time with pass B's accelerations, then run the
 *                   SIGN-CHANGE DAMPER between the two.
 *   10. 0x1005AB85  copy live -> `save`, BrRbQuatDerivative(save) TWICE (the
 *                   dead duplicate), 0x10067C30 (the substep position
 *                   integration), copy `next` -> live, BrWheelSuspensionSetZ,
 *                   and rebuild each wheel's matrix with 0x1006D6B0.
 *
 * ======================================================================
 * THE SIGN-CHANGE DAMPER  (0x1005AA52 .. 0x1005AB7F)
 * ======================================================================
 * Undocumented anywhere else in this tree, and it is the reason pass B is
 * stable.  Over THREE components, on TWO fields of the state -- the linear
 * velocity at state+0x0C and the angular velocity at state+0x28 -- the
 * original does:
 *
 *      s_old = sign(live[i])        0.0f / 1.0f / -1.0f, the triple at
 *      s_new = sign(next[i])        0x10077780 / 84 / 88
 *      live[i] = (s_old == s_new) ? next[i] : 0.0f
 *
 * so a force that would REVERSE a component instead ZEROES it.  The compare
 * is `fxch st(1)` + `fcomp st(1)` + `test ah,0x40` + `jne`, i.e. the store of
 * `next` happens on C3 -- EQUAL OR UNORDERED.  Both operands are one of three
 * constants so unordered cannot arise there; the polarity still matters and
 * is written as `==`.
 *
 * The sign classifier itself is three-way and its polarity DOES matter:
 * `fcom` + `test ah,0x40` picks 0.0f for EQUAL-OR-UNORDERED, then
 * `test ah,0x41` + `jne` picks -1.0f for LESS-OR-UNORDERED.  A NaN therefore
 * classifies as 0.0f at the first test and never reaches the second.
 *
 * The loop runs `ecx = state+0x28` with `ecx += 4` and a counter of 3, and
 * touches `[ecx]` and `[ecx-0x1C]` -- so it is state+0x28..0x30 and
 * state+0x0C..0x14, three components each, interleaved.  The comparison
 * operand is `[ecx+0xE0]`, which is exactly the same field in `next`
 * (0x2BC - 0x1DC == 0xE0).
 *
 * WHERE THE RESULT GOES, and why it is not overwritten: the damped LIVE state
 * is copied to `save` at 0x1005AB85, and `save` is what 0x10067C30 integrates
 * from.  The live state itself is then clobbered by `next` at 0x1005ABBC.  So
 * the damper's output survives via `save`, not via the live copy.
 *
 * ======================================================================
 * THE DEAD DUPLICATED CALL  (0x1005AB9B and 0x1005ABAA)
 * ======================================================================
 * BrRbQuatDerivative(save) is called TWICE in a row on the same object with
 * nothing in between.  It is idempotent -- qDot is a pure function of angVel
 * and quat, neither of which it writes -- so the second call cannot change
 * anything.  Both are kept.  This is the kind of thing that gets "tidied"
 * away and then makes a byte-level diff impossible.
 *
 * ======================================================================
 * THE HOLE, NAMED PLAINLY -- AND WHAT CLOSING TWO OF THEM DID NOT FIX
 * ======================================================================
 * ONE callee is still missing.  It is a function pointer here, NULL by
 * default, and every call is COUNTED so a run can report how much of the
 * step it really ran.  A silent no-op would make "the car moved"
 * unfalsifiable.
 *
 *   inside 0x10067C30, five collision callees, 5196 B in all:
 *      0x10066AD0   669 B    0x10066D70  1782 B    0x10067710  1301 B
 *      0x10068F80  1444 B    (plus 0x1006DDD0, which IS ported as
 *                            BrMat4BuildScaledTransposed but whose two
 *                            outputs feed only the four above)
 *   They run FOUR TIMES PER FRAME, once per position substep.
 *
 * ...AND THAT PARAGRAPH IS NOW TWO-FIFTHS OUT OF DATE, plus one factual
 * error that cost this project the same two passes.  See br_collresp.h.
 *
 *   - 0x10066D70 IS PORTED, as BrCollRespTipKick.  The claim below that it is
 *     "a test whose non-zero result only drives a printf" was WRONG: the
 *     return value does only feed a printf (0x10008D60, which is one `c3`
 *     byte in this build), but the function's last act is to write
 *     save.angVel and call BrRbQuatDerivative on it.  It is the ONLY
 *     unported callee of 0x1005A7A0 that wrote a pitch response directly,
 *     and it was dismissed on the strength of its return value.
 *     It is also NOT the divergence's answer, and that is measured, not
 *     assumed: its third gate rejects a corner whose normal-velocity
 *     magnitude EXCEEDS 0.1, so it fires on a car that has come to rest
 *     balanced on one or two wheels and never on one that is still moving.
 *   - 0x10066AD0 IS PORTED, with its six callees, as BrCollRespBroadPhase.
 *     It was pointless before, and that is the useful part of the story:
 *     the box comes from f1DC/f1E0/f1E4/f1E8, which nothing in this port
 *     filled, and with two extents at zero 0x10067C30's reciprocals are
 *     infinite, so the whole OBB chain classified every triangle out.  The
 *     box is now loaded from the .rca by 0x1006FD90 (br_cardata.h) and the
 *     gather runs: g_cBrCollRespGathered counts what it finds.
 *   - 0x10067710 remains, as BR_CP_HOLE_BOX, together with its impulse
 *     solver 0x10065C80.  It is the RESPONSE -- the consumer of the list
 *     the broad phase fills -- and it is the half that writes `next` back,
 *     so nothing the car does can change until it lands.  NOTE that the
 *     hole is now counted ONCE PER SUBSTEP (0x10067D9B) rather than once
 *     per frame, because it no longer stands for the once-per-frame gather.
 *   - 0x10068F80 remains, as BR_CP_HOLE_CARCAR.  It is CAR VERSUS CAR only:
 *     it walks the entrant array at 0x10AF0858 and tests pairs of car+0x1DC
 *     positions against 5.0f (0x10077BDC).  Nothing in it can touch the
 *     ground, so it is not a candidate for the divergence at all.
 *
 * 0x100651A0 and 0x100645A0 were the other two and are now ported.  Neither
 * was what the previous pass hoped, and this is the single most useful thing
 * in this header:
 *
 *   - 0x100651A0 has NO lateral force and NO load transfer in it.  It makes
 *     one force, along the wheel's own rolling direction, whose magnitude is
 *     f1CC/f1C8 -- and f1CC is written by the control pass at 0x1006F2xx,
 *     which is not ported, so in this port it is 0 and the tyre pass adds
 *     EXACTLY ZERO force.  Landing it changed no observable number.
 *   - 0x100645A0 does constrain the body, hard, by overwriting velocity --
 *     but only the world-Z component of angVel (yaw) and the world X and Y
 *     of vel.  It cannot touch PITCH or ROLL.
 *
 * Measured on a 1-in-100 slope (3 cm across the 3 m wheelbase), the whole of
 * the divergence is in angVel.y: 0.017 rad/s at step 9, 2.6 at step 12, 7.2
 * at step 13, 11.96 at step 15, while angVel.x and angVel.z stay under
 * 0.01 rad/s -- the latter because 0x100645A0 is now pinning them.
 *
 * That measurement is reproducible without a track file:
 * test_collresp.c's TestSlopeDivergence builds the same slope out of two
 * collision triangles and prints the triple every other step.  It is a
 * PRINT, not an assertion -- asserting a particular divergence would encode
 * today's behaviour rather than a property of the code.  The one thing it
 * does assert is the SHAPE: |angVel.x| and |angVel.z| stay under 1 while y
 * runs away, so a future pass cannot make the magnitude smaller by moving
 * the divergence into roll or yaw without being caught.
 *
 * WHAT THE SLOPE MEASUREMENT SAYS ABOUT THE SPRING, since the collision hole
 * turned out not to be the answer: at the step the divergence starts, the
 * front wheels' f1D8 has passed 0 and the rear pair's is near -0.15.
 * 0x100684F0 clamps f1D8 above at 0 and kills the force entirely below
 * -0.3999, so the spring is BOUNDED -- about 28800 N at full compression and
 * exactly 0 past full extension -- while the pitch it has to arrest is not.
 * The one damper in the frame, 0x10068600, resists only a wheel moving UP.
 * That asymmetry is in the bytes of both functions and is reproduced; naming
 * it here so the next pass does not spend itself re-deriving it.
 *
 * What IS ported: gravity, the spring, the shock absorber, the aerodynamic
 * drag, the tyre pass, the drivetrain's axle constraint, both force solves,
 * both velocity integrations, the sign-change damper, the four-substep
 * position integration and the ground probe.
 *
 * ======================================================================
 * PORTABILITY DEVIATIONS, all of them representation-only
 * ======================================================================
 *  - The original's car record is one 0x2B68-byte blob and every object below
 *    is a byte offset into it.  Five pointer fields inside BrRbBodyFull widen
 *    under LP64 (slice3_42.h says so), so the blob's tail offsets do not
 *    survive.  BrCarPhys is that blob's physics part as a STRUCT; every
 *    member carries its original offset in a comment.  Nothing is overlaid on
 *    a foreign buffer, so this costs nothing.
 *  - car+0x278 and car+0x2BC are body+0x114 and body+0x158, i.e. they live
 *    inside BrRbBodyFull's `pad114` (0x19C - 0x114 == 0x88 == exactly two
 *    0x44-byte states).  They are separate members here.
 *  - BrRbIntegrateVelocity takes slice3_44.h's BrRbBody, which is a SECOND
 *    model of the same original object and puts `accel` at a different HOST
 *    offset.  A cast would be the "two models of one object" bug CONVENTIONS
 *    records, so the two vectors it reads are copied into a temporary.
 */
#ifndef BR_CARPHYS_H
#define BR_CARPHYS_H

#include <stdint.h>

#include "br_vec.h"
#include "br_mat.h"
#include "br_cardata.h"   /* BrCarData -- the .rca record 0x1006FD90 reads   */
#include "br_phys.h"      /* BrGroundHit, BrWheelSuspensionSetZ, BR_PHYS_DT  */
#include "slice3_42.h"    /* BrRbBodyFull, BrRbForce, BrRbAccumAll, ...      */
#include "slice3_44.h"    /* BrRbState and the four integrator primitives    */

/* ======================================================================
 * Constants, every one read out of orig/BRGlide.dll or orig/BRD3D.dll
 * ====================================================================== */

/* 0x10077780 / 84 / 88 -- the sign triple the damper classifies with.  The
 * same three values appear again at 0x10077A78 / 7C / 80 and the code uses
 * BOTH copies; they are not folded here because the addresses differ. */
#define BR_CP_SIGN_ZERO    0.0f
#define BR_CP_SIGN_POS     1.0f
#define BR_CP_SIGN_NEG   (-1.0f)

/* 0x10077BD0, a DOUBLE (`fcom qword ptr`).  A wheel whose f1D8 is at or below
 * this has no contact: the spring input is forced to 0x10077BD8 and the
 * contact counter is reset. */
#define BR_CP_CONTACT_MIN  (-0.3999)

/* 0x10077BD8.  Both the "no contact" substitute and the suspension's rest
 * offset -- the spring's input is `min(f1D8, 0) - (-0.3)`. */
#define BR_CP_SUSP_REST   (-0.300000012f)

/* 0x100684F0's contact counter saturates here (`cmp eax, 0x64; jge`). */
#define BR_CP_CONTACT_MAX  100

/* 0x100684F0 sets this byte in the body when a wheel's contact counter goes
 * from zero to non-zero -- a touchdown edge. */
#define BR_CP_TOUCHDOWN    0x80u

/* 0x10067F30.  Drag is vel * -110; above 4.0 m/s, off mode 3, and with any
 * wheel on surface 4, a further vel * -220 is added. */
#define BR_CP_DRAG_K      (-110.0f)
#define BR_CP_DRAG_SPEED    4.0f
#define BR_CP_DRAG_K2     (-220.0f)
#define BR_CP_DRAG_SURFACE  4

/* 0x10067C30's substep: 0x10077B94 is 1/120 and 0x10077B98 is the 0.002
 * residue the loop stops at, so BR_PHYS_DT yields exactly four substeps. */
#define BR_CP_SUBSTEP      0.008333333767f
#define BR_CP_SUBSTEP_EPS  0.002f

/* 0x10077AC8.  m[2][2] below this means the car is on its roof. */
#define BR_CP_UPRIGHT_MIN  0.5f

/* ---- 0x100651A0, the tyre pass.  Every one read out of BRGlide.dll. ---- */

/* 0x10077AE0, a DOUBLE (`fcomp qword ptr`).  The contact plane's n.z must be
 * at or above this or the wheel makes no force at all -- about 45.6 degrees
 * of slope.  The compare is float-against-double, so the boundary is the
 * double's. */
#define BR_CP_TYRE_MINNZ    (0.7)

/* 0x10077AD0.  The wheel's own mass enters the load as `bodyMass - wheelMass
 * * -4`, i.e. all four wheels are folded into one chassis mass. */
#define BR_CP_TYRE_WHEEL_K  (-4.0f)

/* 0x10077AE8 and 0x10077AEC.  The vertical load a wheel is assumed to carry:
 * ((mTotal * 2.943) * n.z) * 3.5.  2.943 == 0.3 * 9.81 and 2.943 * 3.5 ==
 * 10.3005, which is the same 10.5-ish per-wheel figure the wheel gravity
 * node carries (-103.005 == 10.5 * 9.81).  Kept as the original's two
 * separate multiplies because the rounding differs from one 10.3005. */
#define BR_CP_TYRE_LOAD_G    2.943000316619873f
#define BR_CP_TYRE_LOAD_K    3.5f

/* 0x10077AF0.  Half the raw drive force is reported back through pA, as
 * `*pA - q * -0.5`. */
#define BR_CP_TYRE_REPORT  (-0.5f)

/* 0x10077AF4.  WHEELSPIN.  When the demanded force exceeds the load the
 * delivered force does not saturate at the load -- it COLLAPSES to a tenth
 * of it.  That discontinuity is the original's and is the whole traction
 * model. */
#define BR_CP_TYRE_SPIN     0.10000000149011612f

/* 0x10077AF8.  How much of the slip (r*w + v.e) is bled out of the wheel's
 * spin each step. */
#define BR_CP_TYRE_RELAX    0.4000000059604645f

/* 0x10077AFC.  The wheel's spin is clamped to +-300 through the same
 * three-way sign classifier the damper uses -- so a NaN spin becomes
 * 0 * 300, not 300. */
#define BR_CP_TYRE_SPIN_MAX 300.0f

/* 0x10077B00.  Radians per second -> degrees per second.  Note it is the
 * FLOAT 57.2957763671875, not the double 57.29577951308232. */
#define BR_CP_RAD_TO_DEG    57.2957763671875f

/* 0x10077B08 / 0x10077B10 / 0x10077B18 / 0x10077B20, all DOUBLES, and
 * 0x10077B28, a FLOAT.  The wheel's rolled-up angle in degrees: rejected
 * outright (reset to 0) if it leaves +-36000 or stops being finite, then
 * folded into (0, 360] from above and [0, ...) from below. */
#define BR_CP_ANGLE_LIMIT   36000.0
#define BR_CP_ANGLE_TURN    360.0
#define BR_CP_ANGLE_TURN_N  (-360.0f)
#define BR_CP_ANGLE_ZERO    (0.0)

/* ---- 0x100645A0, the drivetrain.  All read out of BRGlide.dll. ---- */

/* 0x10077A84 and 0x10077A88 and 0x10077A8C.  The brake term, per axle:
 *      b = sign(f1C4) * |f1D0| * -2 / f1C8 / (mass * 0.25) * dt * dt
 * -- yes, dt TWICE; the two `fmul [esp+0xa4]` pairs at 0x100646FA..0x1006471C
 * are unambiguous -- and then |b| > 1 collapses it to sign(b) * 1.5. */
#define BR_CP_DRV_BRAKE_K   (-2.0f)
#define BR_CP_DRV_MASS_K     0.25f
#define BR_CP_DRV_BRAKE_MAX  1.5f

/* 0x10077A88 again -- the SAME constant, reached from a different place, as
 * the lateral/longitudinal ratio above which pair A is declared to be
 * sliding.  Named twice on purpose: folding them would assert a relationship
 * the original does not have. */
#define BR_CP_DRV_SLIDE_RATIO 0.25f

/* 0x10077A90 (and the identical immediate 0x45FA0000 at 0x10064918) and
 * 0x10077AA8 / 0x10077AC4.  The lateral-force threshold an axle must exceed
 * before its velocity is corrected at all: 8000 normally, 5600 once the
 * axle's own gate byte is set -- i.e. once it corrected LAST frame. */
#define BR_CP_DRV_HOLD_OFF   8000.0f
#define BR_CP_DRV_HOLD_ON    5600.0f

/* 0x10077A98, a DOUBLE.  "Is this axle braking at all". */
#define BR_CP_DRV_BRAKE_EPS  (0.0001)

/* 0x10077AA0 / 0x10077AA4.  A braking axle adds to the force demand, which
 * REDUCES the grip factor below.  Both are negative and the code SUBTRACTS
 * them.  Which one applies depends on whether the OTHER pair is braking. */
#define BR_CP_DRV_BRAKE_ADD1 (-10000.0f)
#define BR_CP_DRV_BRAKE_ADD2 (-100000.0f)

/* 0x10077AAC and 0x10077AB0 (a DOUBLE).  The grip factor is
 *      t3 / clamp(demand, t3, t2)  *  t1  *  20
 * and gets a further 1.5 when the steered pair is pointing straight. */
#define BR_CP_DRV_GRIP_SCALE 20.0f
#define BR_CP_DRV_STRAIGHT   (1.5)

/* 0x10077AB8 and 0x10077ABC.  Below 27 m/s the factor has a floor that rises
 * to 0.1 at a standstill -- so a stationary car still gets corrected. */
#define BR_CP_DRV_SLOW_SPEED 27.0f
#define BR_CP_DRV_SLOW_K     0.003703703870996833f

/* 0x10077AC0.  The dead band of the sign-change test on pair A's X. */
#define BR_CP_DRV_ZERO_EPS   9.999999747378752e-06f

/* 0x10077AC8.  Half -- the two axles' X velocities are averaged -- and the
 * same constant again as the clamp on the roll term below. */
#define BR_CP_DRV_HALF       0.5f

/* 0x10077AD4 / 0x10077AD8.  The chassis's own f1D4 (the visual roll) chases
 * -8 * the retained side force, at 4/15 of a unit per step. */
#define BR_CP_DRV_ROLL_STEP  0.2666666805744171f
#define BR_CP_DRV_ROLL_STEPN (-0.2666666805744171f)

/* 0x100B4C30 / 0x100B4D50 (72 floats, [24*compound + 8*weather + surface]),
 * 0x100B4E70 / 0x100B4ED0 and 0x100B5178 (24 floats, [8*weather + surface]).
 * The last is not selectable -- it is addressed as a fixed .rdata table. */
#define BR_CP_DRV_T23_FLOATS (BR_CP_GRIP_WEATHERS * BR_CP_GRIP_SURFACES)

/* 0x100B4F30 / 0x100B5050: 3 tyre-compound rows of 3 weather rows of 8
 * surfaces.  Indexed [24*compound + 8*weather + surface]. */
#define BR_CP_GRIP_COMPOUNDS 3
#define BR_CP_GRIP_WEATHERS  3
#define BR_CP_GRIP_SURFACES  8
#define BR_CP_GRIP_FLOATS   (BR_CP_GRIP_COMPOUNDS * BR_CP_GRIP_WEATHERS \
                             * BR_CP_GRIP_SURFACES)

/* 0x10067C30's two reset counters: 0x23 for the AI arm, 0x78 for the human
 * arm, and the "has it moved a metre" test is against 1.0f (0x10077A7C). */
#define BR_CP_RESET_AI     0x23
#define BR_CP_RESET_HUMAN  0x78
#define BR_CP_STUCK_DIST   1.0f

/* The masses and the two gravity forces are IMMEDIATES in the car
 * constructor (D3D 0x10062C50), not table data:
 *
 *   body   mode 1, dim (3.5, 2.0, 1.5), mass 1000        0x40600000 &c.
 *   wheels mode 2, dim (0,0,0),         mass 0
 *   chassis gravity node   f = (0, 0, -15450.75)   0xC6716B00, kind 0
 *   wheel   gravity node   f = (0, 0,  -103.005)   0xC2CE028F, kind 0
 *
 * -15450.75 == 1575 * 9.81 and -103.005 == 10.5 * 9.81, so the units are
 * metres, kilograms and seconds -- but the chassis MASS is 1000, not 1575,
 * so the chassis sees 15.45 m/s^2, not 9.81.  That is the original's number
 * and is not "corrected" here. */
#define BR_CP_BODY_MASS      1000.0f
#define BR_CP_BODY_DIM_X        3.5f
#define BR_CP_BODY_DIM_Y        2.0f
#define BR_CP_BODY_DIM_Z        1.5f
#define BR_CP_GRAVITY_BODY  (-15450.75f)
#define BR_CP_GRAVITY_WHEEL   (-103.0050049f)

/* The four wheel corners, the `r` of both force lists' first four nodes:
 * (-1.5,-1), (-1.5,+1), (+1.5,-1), (+1.5,+1) in list order. */
#define BR_CP_CORNER_X   1.5f
#define BR_CP_CORNER_Y   1.0f

/* car+0x31C, the spring rate, from D3D 0x10062D21:
 *      k = (20.0f - suspIndex * -4.0f) * 16000.0f
 * with the three constants read out of BRD3D.dll at 0x1008F89C / 0x1008F8C4 /
 * 0x1008F8C8.  car+0x320, the damper rate, is the immediate -3000. */
#define BR_CP_SPRING_BASE   20.0f
#define BR_CP_SPRING_STEP  (-4.0f)
#define BR_CP_SPRING_SCALE 16000.0f
#define BR_CP_DAMPER_K   (-3000.0f)

/* Wheel mount Z, D3D 0x10062DBD and friends: 0xBDCCCCCD. */
#define BR_CP_WHEEL_Z     (-0.100000001f)

/* ======================================================================
 * The object
 * ====================================================================== */

/* Five nodes per chassis list; the fifth is gravity (list A) or drag (list B).
 * Two nodes per wheel list; the second is the wheel's own weight. */
#define BR_CP_CHASSIS_NODES 5
#define BR_CP_WHEEL_NODES   2

typedef struct BrCarPhys {
    BrRbBodyFull body;              /* car+0x164                            */
    BrRbBodyFull wheel[4];          /* car+0x370, 0x57C, 0x788, 0x994       */

    /* car+0xBA0, 0xBE0, 0xBC0, 0xC00, 0xCA0 -- IN THAT ORDER, which is the
     * order the constructor links them, not the order of the addresses. */
    BrRbForce    aListA[BR_CP_CHASSIS_NODES];
    /* car+0xC20, 0xC60, 0xC40, 0xC80, 0xD00 */
    BrRbForce    aListB[BR_CP_CHASSIS_NODES];
    /* car+0xD20, 0xD60, 0xD40, 0xD80 -- the tyre-force node of wheel i,
     * indexed by the ORDER 0x1005A7A0 assigns them (see step 1). */
    BrRbForce    aWheelF[4];
    /* car+0xCE0 is shared by wheels 0 and 1; car+0xCC0 by wheels 2 and 3.
     * Both carry the same constant, and the sharing is the original's. */
    BrRbForce    aWheelW[2];

    BrRbState    save;              /* car+0x278 == body+0x114              */
    BrRbState    next;              /* car+0x2BC == body+0x158              */

    /* car+0x340..0x34C == body+0x1DC..+0x1E8.  THE CAR'S COLLISION BOX: the
     * three FULL extents and a Z offset.  0x10067C30 reciprocates the first
     * three for the OBB transform (so the box becomes the unit cube
     * 0x10066260 classifies against +-0.5) and 0x10066D70 halves them for its
     * corner.
     *
     * WHERE THEY COME FROM, corrected.  This header used to say the
     * constructor (Glide 0x1005BCC0) writes them at 0x1005BD40 / 42 / 48 / 4E
     * as (0, 0, 2.0f, 0) and that 0x10059A80 then replaces them from a
     * car-data record's +0x10..+0x1C.  BOTH ARE THE SAME MISREADING: those
     * two functions address car+0x1DC, which is the live BrRbState
     * (body+0x78), not body+0x1DC == car+0x340.  The four stores at
     * 0x1005BD40.. are the initial pos/vel/quat this module already writes as
     * BrCarPhysInit's identity state at (0, 0, 2.0).
     *
     * The ONLY writer of car+0x340..+0x34C in either image is 0x1006FD90, at
     * 0x1006FEBF / C5 / D1 / DD, out of the car-data object at car+0x29C4
     * offsets +0xC8..+0xD4.  br_cardata.h has the whole chain from the disc;
     * BrCarPhysApplyCarData below is that copy. */
    float        f1DC, f1E0, f1E4;  /* car+0x340..0x348                      */
    float        f1E8;              /* car+0x34C                             */

    /* car+0x308 == body+0x1A4..+0x1AC, the CHASSIS's own contact-plane
     * normal.  0x10066D70 reads it to choose the sign of its pitch kick.
     * NOTHING in either build writes it for the chassis: the only writer of
     * +0x1A4..+0x1B0 anywhere is 0x10068070, the wheel probe, and all four of
     * its call sites pass a child body.  Modelled as an explicit zero, which
     * is what a zeroed car record gives and which selects the negative arm.
     * It cannot be a byte offset here -- it falls inside BrRbBodyFull's
     * pad1A0, which does not survive LP64. */
    BrVec3       bodyPlaneN;        /* car+0x308                             */
    int32_t      f1F8;              /* car+0x35C, the stuck/roll-over timer  */
    uint8_t      b208;              /* car+0x36C, the touchdown flag         */

    BrVec3       lastPos;           /* car+0x2AB0, the previous frame's m[3] */

    int32_t      fE84;              /* car+0xE84, 1 suppresses the tyre pass
                                     * on the first frame only               */
    float        fE74;              /* car+0xE74, the REAR pair's force sum  */
    uint8_t      bE78;              /* car+0xE78.  NOT a float: 0x1005A7A0
                                     * passes its ADDRESS to both the rear
                                     * tyre calls and to the drivetrain, and
                                     * every access on both sides is a BYTE
                                     * (`mov al,[ecx]` / `mov byte [edi],1`).
                                     * It is the rear pair's grip-table gate. */
    float        fE7C;              /* car+0xE7C, the FRONT pair's force sum */
    uint8_t      bE80;              /* car+0xE80, the FRONT pair's gate --
                                     * same byte treatment as bE78, and
                                     * 0x1005A8DB clears it immediately
                                     * before the two front calls, so the
                                     * front wheels can never see it set     */

    /* car+0x361 == body+0x1FD.  0x100651A0 and 0x100645A0 both index the
     * grip tables with it and both reach it off the BODY pointer, i.e. out
     * of the end of BrRbBodyFull and into the car record.  It cannot be a
     * byte offset here (LP64), so it is a member. */
    uint8_t      b1FD;
    /* car+0x36D == body+0x209.  Written only by the drivetrain. */
    uint8_t      b209;

    int32_t      suspIndex;         /* car+0xE94, the spring-rate index      */
    int32_t      fAi;               /* car+0xF08 == 0x1005E690 in the
                                     * original; non-zero here means "this
                                     * entrant is driven by the AI"          */

    /* The surface byte 0x10068070 records at wheel+0x1A0.  It cannot live in
     * BrRbBodyFull (br_phys.h explains why), so it lives here and is filled
     * by the ground probe at the END of each step -- which means the drag
     * pass reads LAST frame's surface, exactly as the original does. */
    BrGroundHit  aHit[4];
} BrCarPhys;

/* ======================================================================
 * The remaining unported callees, as counted hooks
 * ====================================================================== */

typedef struct BrCarPhysHooks {
    /* 0x10067710 and its impulse solver 0x10065C80 -- the OBB RESPONSE.
     * One hook per substep.  Its broad phase, 0x10066AD0 and the six
     * callees under it, IS PORTED now (br_collresp.h) and fills the
     * candidate list this would consume; what is missing is the half that
     * writes `next` back. */
    void (*pfnCollide)(BrCarPhys *pCar);
    /* 0x10068F80, 1444 B -- CAR VERSUS CAR.  It walks the entrant array at
     * 0x10AF0858 on a 0x80 stride for g_100B2F00 entrants and tests each
     * pair's car+0x1DC positions against 0x10077BDC (5.0f); nothing in it
     * touches the ground.  It needs the entrant array, which is not this
     * module's object, so it stays a hook. */
    void (*pfnCarCar)(BrCarPhys *pCar);
} BrCarPhysHooks;

extern BrCarPhysHooks g_brCarPhysHooks;

/* How many times each hole was entered since the last reset.  Indexed by
 * BR_CP_HOLE_*.  A run that reports zeroes here ran none of the missing
 * physics; a run that reports non-zeroes ran a NO-OP in its place.
 *
 * 0x100651A0, 0x100645A0 and 0x10066D70 used to be entries here (the last as
 * part of BR_CP_HOLE_COLLIDE).  They are PORTED now and their slots are gone
 * rather than left reporting zero, which would read as "never entered". */
enum { BR_CP_HOLE_BOX, BR_CP_HOLE_CARCAR, BR_CP_HOLE_COUNT };
extern uint32_t g_aBrCarPhysHole[BR_CP_HOLE_COUNT];
void  BrCarPhysHoleReset(void);
const char *BrCarPhysHoleName(int i);

/* ======================================================================
 * The pieces
 * ====================================================================== */

/* body+0x78..+0xBC is byte-for-byte a BrRbState under both models -- 68 bytes
 * of float in the same order, no padding on any ABI this port targets.
 * test_carphys asserts it. */
BrRbState *BrCarPhysBodyState(BrRbBodyFull *pBody);

/* ======================================================================
 * 0x1006FD90, 368 B -- THE CAR-DATA APPLY, and the end of the box hole
 * ======================================================================
 * __thiscall on the car.  It resets six matrices, copies the physics block
 * out of the car-data object at car+0x29C4, and clears that pointer
 * (0x1006FEF1).  Its last four stores are the reason this port has a
 * collision box at all:
 *
 *      car+0x340 = data+0xC8      car+0x348 = data+0xD0
 *      car+0x344 = data+0xCC      car+0x34C = data+0xD4
 *
 * WHAT IS AND IS NOT PORTED, named rather than implied.  Only those four
 * land here.  The other eleven destinations (car+0xE28..+0xE64) are gear
 * ratios and three sign-extended bytes; none of them is a member of
 * BrCarPhys, none is read by anything this module ports, and inventing
 * members for them would be modelling a record nothing consumes.  They are
 * decoded and available on BrCarData, so whoever lands the control pass at
 * 0x1006F2xx does not have to re-derive the offsets.  The six matrix resets
 * are 0x1006E5A0 on car+0x40..+0x100 and car+0x273C..+0x2890 -- the camera
 * frames, not this module's object.
 *
 * pData == NULL is a no-op, which is the state a run has when the CARS/
 * directory is absent.  It leaves the box unwritten and
 * BrCollRespBoxDegenerate reports that; a silent zero would make "no
 * collisions" unfalsifiable. */
void BrCarPhysApplyCarData(BrCarPhys *pCar, const BrCarData *pData);

/* car+0x29C4, the car-data object pointer.  0x1006FCB0 writes it before the
 * constructor runs and 0x1006FD90 clears it after consuming it.
 *
 * DEVIATION, and it is the same shape as the one this header already records
 * for g_pBrCarPhysGrip: the original has an entrant-selection path
 * (0x1002EBD1 -> 0x10030DE0) that this port does not run, so the pointer
 * would stay NULL and every car would have no box.  BrCarPhysInit therefore
 * falls back to BrCarDataDefault() -- car 0, "ce", read off the disc's own
 * CARS/ce.rca -- when this is NULL.  That makes the path defined; it does not
 * invent a number, and a run with no CARS/ still reports the box as
 * degenerate rather than pretending. */
extern const BrCarData *g_pBrCarPhysCarData;

/* Build the rigid body the way the car constructor (D3D 0x10062C50 /
 * 0x10063000) does: masses, dimensions, inertia, both force lists, the four
 * wheel bodies and their mount points, and the identity state at
 * (0, 0, 2.0) -- and then apply the car data, as 0x1005E7B0 does by calling
 * 0x1006FD90 on the freshly constructed car.
 *
 * `aMount` is the four (x, y) mount offsets; pass NULL for the symmetric
 * default.
 *
 * THE MOUNTS ARE NOT TAKEN FROM THE CAR DATA, and that is deliberate.  The
 * constructor reads them at object+0x80EC..+0x80FC, which is INSIDE the
 * big-endian N64 half of the .rca -- the half 0x10030770's 1641-byte swap
 * pass rewrites and which this port does not transcribe.  BrCarData decodes
 * them with an explicit byte swap so they are available and checkable, but
 * applying them would (a) assert a swap that has not been transcribed and
 * (b) make the wheel mounts front/rear ASYMMETRIC (ce.rca: front x 1.135,
 * rear x -1.338), which moves the flat-ground settle.  The corner geometry
 * the force lists encode -- (+-1.5, +-1) -- stays the default until the swap
 * pass lands. */
void BrCarPhysInit(BrCarPhys *pCar, const float aMount[4][2]);

/* Place the car: position, and a yaw in radians about Z.  The original does
 * this through the driver-record constructor's grid placement; this is the
 * state write that ends up equivalent. */
void BrCarPhysPlace(BrCarPhys *pCar, const BrVec3 *pPos, float yaw);

/* 0x100684F0, 265 B.  The suspension spring, into the CHASSIS's current force
 * list -- four nodes walked in list order against wheel[0..3].
 *
 *      v = f1D8;  if (!(v > -0.3999)) { v = -0.3; contact = 0; }
 *                 else if (contact < 100) ++contact;
 *      if (v > 0)   v = 0;                       clamp above
 *      v -= -0.3;                                rest offset
 *      if (!(v >= 0)) v = 0;                     clamp below
 *      node->f.z = sign(v) * v * v * k;          node->f.x = node->f.y = 0
 *
 * Every clamp is the original's negated form: `test ah,0x41` keeps the value
 * for LESS-OR-EQUAL-OR-UNORDERED, so a NaN f1D8 lands on the no-contact arm
 * and a NaN v survives the two clamps and produces a NaN force.  Writing them
 * the tidy way inverts all three. */
void BrCarPhysSpring(BrRbBodyFull *pBody, uint8_t *pTouchdown);

/* 0x10068600, 200 B.  The shock absorber, into the chassis's current list:
 *
 *      BrRbVelAtBodyPointXY(&v, body, wheel)      == 0x100642F0
 *      node->f.z = (wheel->f1B4 == 0)   ? 0
 *                : (!(v.z >= 0))        ? 0
 *                :                        body->f1BC * v.z
 *
 * so it only ever resists a wheel moving UP, and only while that wheel has
 * a contact count.  NaN takes the zero arm. */
void BrCarPhysDamper(BrRbBodyFull *pBody);

/* 0x10067F30, 305 B.  Aerodynamic drag into one node:
 *
 *      node->f = body->vel * -110
 *      if (sqrt(|vel|^2) > 4  &&  mode != 3  &&  any wheel surface == 4)
 *          node->f += body->vel * -220
 *
 * The speed test is `test ah,0x41` + `jne` on the sqrt, i.e. it rejects
 * less-equal-or-unordered, so a NaN speed skips the second term.  The four
 * surface bytes are read as SIGNED bytes (`movsx`) and compared against 4. */
void BrCarPhysDrag(const BrRbBodyFull *pBody, const BrGroundHit aHit[4],
                   BrRbForce *pNode, int32_t mode);

/* The sign classifier the damper uses, exposed because its polarity is the
 * single easiest thing in this module to get backwards:
 *      NaN and 0.0 -> 0.0f;  < 0 -> -1.0f;  > 0 -> +1.0f. */
float BrCarPhysSign(float v);

/* 0x1005AA52..0x1005AB7F -- the sign-change damper, over the two velocity
 * fields of the state.  pLive is written; pNext is read. */
void BrCarPhysSignDamp(BrRbState *pLive, const BrRbState *pNext);

/* ======================================================================
 * 0x100651A0, 1355 B -- THE PER-WHEEL TYRE PASS
 * ======================================================================
 * br_carphys.h used to call this "grip, slip and the drive torque" and
 * guessed that lateral force and load transfer lived here.  THE GUESS WAS
 * WRONG and the bytes say so plainly: there is no slip angle in it, no
 * lateral force, and no load transfer.  What it computes is ONE force,
 * along the wheel's own rolling direction:
 *
 *   1. bail out entirely if the wheel has no contact record (f19C == 0) or
 *      the contact plane's n.z is below 0.7.  Not even the wheel's spin is
 *      updated on that path.
 *   2. a  = row 1 of the chassis matrix     -- the car's local +Y in world
 *      c  = a x n                           -- forward, in the ground plane
 *      d  = n x c                           -- lateral, in the ground plane
 *      e  = cos(wheel->f1C0) * c + sin(wheel->f1C0) * d
 *      so e is the wheel's heading, steered by f1C0, laid on the ground.
 *   3. if ALL FOUR wheels have a contact count:
 *          q    = wheel->f1CC / wheel->f1C8         (drive torque / radius)
 *          load = ((mass - wheelMass*-4) * 2.943 * n.z) * 3.5
 *          *pA += -q * -0.5
 *          if (*pB) q *= grip[weather][compound][surface]
 *          if (|q| > |load|)  q = |load/q| * q * 0.1     <-- WHEELSPIN
 *          wheel->pForces->f += M * (-q * e)         (body frame)
 *          w = f1C4 + dt*(f1CC - q*f1C8);  w -= 0.4*(f1C8*w + dot(vel, e))
 *      else the wheel just free-spins: w = f1C4 + dt*f1CC, and NO force at
 *      all is written.  One wheel off the ground disables all four tyres.
 *   4. clamp w to +-300, integrate the wheel's display angle f1D4 in
 *      DEGREES, reject it if it stops being finite or leaves +-36000, and
 *      fold it into [0, 360].
 *
 * CONSEQUENCE WORTH STATING: q is a pure function of f1CC, which nothing in
 * this module writes.  With f1CC == 0 -- which is what BrRbInitInertia
 * leaves and what the port has until 0x100645A0 and the control pass
 * 0x1006F2xx run -- the force is exactly zero and this whole pass is
 * observable only in f1C4 and f1D4, neither of which anything else reads.
 *
 * DEVIATIONS, both forced by earlier ones:
 *  - the original takes (body, wheel, pA, pB, dt) and reads the contact
 *    plane out of wheel+0x1A4..+0x1AC.  br_phys.h moved those six fields to
 *    BrGroundHit, so this takes the car and a wheel INDEX and reads
 *    pCar->aHit[i].  Same object, reachable.
 *  - the compound byte at body+0x1FD is car+0x361, past the end of
 *    BrRbBodyFull; it is pCar->b1FD here.
 *  - pB is `const uint8_t *` because every access to it on both sides is a
 *    byte.  The step passes &bE80 for wheels 0/1 and &bE78 for 2/3. */
void BrCarPhysTyre(BrCarPhys *pCar, int iWheel, float *pA,
                   const uint8_t *pB, float dt);

/* 0x11773690, the live grip table, and the two candidates 0x100B4F30 /
 * 0x100B5050 it is aimed at.  Layout [24*compound + 8*weather + surface].
 *
 * DEVIATION: the original leaves the pointer NULL until 0x10069530 runs, so
 * a tyre pass with its gate byte set before car selection would dereference
 * NULL.  Nothing in this port calls the selector, so the pointer is
 * initialised to the table 0x10069530 picks for car index 0 rather than
 * left NULL.  That makes the path defined; it does not invent a number. */
extern const float g_aBrCarPhysGripA[BR_CP_GRIP_FLOATS];   /* 0x100B4F30 */
extern const float g_aBrCarPhysGripB[BR_CP_GRIP_FLOATS];   /* 0x100B5050 */
extern const float *g_pBrCarPhysGrip;                      /* 0x11773690 */

extern const float g_aBrCarPhysDrvT1A[BR_CP_GRIP_FLOATS];  /* 0x100B4C30 */
extern const float g_aBrCarPhysDrvT1B[BR_CP_GRIP_FLOATS];  /* 0x100B4D50 */
extern const float *g_pBrCarPhysDrvT1;                     /* 0x11778808 */
extern const float g_aBrCarPhysDrvT2A[BR_CP_DRV_T23_FLOATS]; /* 0x100B4E70 */
extern const float g_aBrCarPhysDrvT2B[BR_CP_DRV_T23_FLOATS]; /* 0x100B4ED0 */
extern const float *g_pBrCarPhysDrvT2;                     /* 0x11778820 */
extern const float g_aBrCarPhysDrvT3[BR_CP_DRV_T23_FLOATS];  /* 0x100B5178 */

/* ======================================================================
 * 0x100645A0, 3070 B -- THE DRIVETRAIN, and it is not an engine model
 * ======================================================================
 * It reads the four scalars at car+0xE74..0xE80 and it writes them, which is
 * what br_carphys.h said before anyone read it.  What it ALSO does, and what
 * matters far more, is WRITE THE CHASSIS'S OWN LINEAR AND ANGULAR VELOCITY:
 *
 *   for each axle -- pair A at child[0]/child[1], pair B at child[2]/child[3],
 *   and only when at least one wheel of EACH pair has a contact count:
 *      v      = BrMat4MulVec3(m, BrRbVelAtPoint(body, (child->f78.x, 0, 0)))
 *      lat    = the part of v across the axle -- v.y for pair A, and for
 *               pair B v minus its projection on (cos f1C0, sin f1C0, 0),
 *               so pair B is the STEERED one
 *      demand = |lat| * mass / dt + |*pA|   (+ a brake penalty)
 *      if (demand > hold)  v -= slip(demand) * lat
 *   and then, if either axle ran:
 *      yaw      = (vA.y - vB.y) / (child0->f78.x - child2->f78.x)
 *      velWorld = ((vA.x + vB.x) * 0.5,  vA.y - yaw * child0->f78.x,  as-is)
 *      angVelWorld.z = yaw
 *   written back through m and m-transpose.
 *
 * So the drivetrain is a KINEMATIC CONSTRAINT on the rigid body: it solves
 * the two axle velocities for a single yaw rate and lateral velocity and
 * overwrites the integrator's answer with them.  That is the damping the
 * ported subset was missing, and it is the reason the ported subset diverges
 * rotationally without it -- nothing else in 0x1005A7A0 can resist yaw.
 *
 * It finishes by chasing the chassis's own f1D4 (the visual roll) toward
 * -8 * the side force it retained, at a fixed 4/15 per step.
 *
 * WHAT IT DOES NOT DO: there is no engine, no gearbox and no throttle in it.
 * The drive torque f1CC and the brake torque f1D0 are written by the control
 * pass at 0x1006F2B2..0x1006F55x, which is NOT ported -- so in this port
 * both are 0 and the brake term below is identically zero.  The lateral
 * constraint runs regardless, because it is driven by the axle velocities.
 *
 * DEVIATIONS: the same two as the tyre pass (the surface bytes live in
 * BrGroundHit, the compound byte at body+0x1FD is pCar->b1FD) plus
 * body+0x209, which is pCar->b209. */
void BrCarPhysDrive(BrCarPhys *pCar, float dt);

/* 0x104B15E8, the weather / condition index.  The tables are indexed with
 * `n - 1` clamped into [0, 2] BY A 16-BIT SIGNED COMPARE, so 0 and anything
 * above 3 and anything negative all land on row 0.  .bss, so it starts 0. */
extern int32_t g_brCarPhysWeather;

/* 0x10069530, 106 B.  Point the three car tables at one of two sets, chosen
 * through a 15-entry byte map and a 5-way jump table: car 0..4, 6..10, 12
 * take set A; 5, 11, 13, 14 take set B; anything above 14 takes set B. */
void BrCarPhysSelectCar(int32_t iCar);

/* 0x10067C30, 762 B.  Four substeps of BrRbIntegrateState from `save` into
 * `next`, rebuilding the body matrix each time, then the roll-over /
 * stuck timer and the last-position store.  The five collision callees are
 * the BR_CP_HOLE_COLLIDE hook. */
void BrCarPhysAdvance(BrCarPhys *pCar);

/* 0x1005A7A0 -- one frame.  The whole point of this module. */
void BrCarPhysStep(BrCarPhys *pCar);

#endif /* BR_CARPHYS_H */
