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
 *               but br_sfx.c also names 0x1006B5F0 -- a different function in
 *               a different build.  Neither is ported; the function is left
 *               out (see THE HOLES below) rather than guessed at.
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
 *                   wheel tyre force.  NOT PORTED -- see THE HOLES.
 *    4. 0x1005A943  car->fE84 = 0; zero body->accel and body->angAccel.
 *    5. 0x1005A96E  0x10064210 == BrRbAccumAll: forces -> accelerations.
 *    6. 0x1005A983  0x1006D600 == BrRbIntegrateVelocity on the LIVE state.
 *    7. 0x1005A9AD  0x100645A0(body, dt, &fE7C, &fE74, &fE80, &fE78).
 *                   NOT PORTED -- see THE HOLES.
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
 * THE HOLES, NAMED PLAINLY
 * ======================================================================
 * Three callees are too large to transcribe from a partial trace and are
 * NOT ported.  Each is a function pointer here, NULL by default, and each
 * call is COUNTED so a run can report how much of the step it really ran --
 * the same discipline port/host/brally.c uses for its stand-ins.  A silent
 * no-op would make "the car moved" unfalsifiable.
 *
 *   0x100651A0  1355 B  the per-wheel TYRE force: grip, slip and the drive
 *                       torque.  Writes the wheels' own force nodes.  Without
 *                       it the car has no traction and no drive.
 *   0x100645A0  3070 B  the engine / drivetrain solve.  Reads and writes the
 *                       four scalars at car+0xE74..0xE80.
 *   inside 0x10067C30, five collision callees:
 *      0x10066AD0   669 B    0x10066D70  1782 B    0x10067710  1301 B
 *      0x10068F80  1444 B    (plus 0x1006DDD0, which IS ported as
 *                            BrMat4BuildScaledTransposed but whose two
 *                            outputs feed only the four above)
 *
 * What IS ported is everything that makes the car fall, settle on its
 * suspension and stay there: gravity, the spring, the shock absorber, the
 * aerodynamic drag, both force solves, both velocity integrations, the
 * sign-change damper, the four-substep position integration and the ground
 * probe that reads the track's real collision triangles.
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

    float        f1DC, f1E0, f1E4;  /* car+0x340..0x348, reciprocated in
                                     * 0x10067C30 and consumed only by the
                                     * unported collision callees            */
    float        f1E8;              /* car+0x34C, same                       */
    int32_t      f1F8;              /* car+0x35C, the stuck/roll-over timer  */
    uint8_t      b208;              /* car+0x36C, the touchdown flag         */

    BrVec3       lastPos;           /* car+0x2AB0, the previous frame's m[3] */

    int32_t      fE84;              /* car+0xE84, 1 suppresses the tyre pass
                                     * on the first frame only               */
    float        fE74, fE78;        /* car+0xE74/0xE78, the REAR pair        */
    float        fE7C;              /* car+0xE7C, the FRONT pair's first     */
    uint8_t      bE80;              /* car+0xE80, written as a BYTE at
                                     * 0x1005A8DB and as a dword elsewhere   */

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
 * The three unported callees, as counted hooks
 * ====================================================================== */

typedef struct BrCarPhysHooks {
    /* 0x100651A0(body, wheel, pA, pB, dt) */
    void (*pfnTyre)(BrCarPhys *pCar, BrRbBodyFull *pWheel,
                    float *pA, float *pB, float dt);
    /* 0x100645A0(body, dt, &fE7C, &fE74, &fE80, &fE78) */
    void (*pfnDrive)(BrCarPhys *pCar, float dt);
    /* the five collision callees inside 0x10067C30, as one hook per substep */
    void (*pfnCollide)(BrCarPhys *pCar);
} BrCarPhysHooks;

extern BrCarPhysHooks g_brCarPhysHooks;

/* How many times each hole was entered since the last reset.  Indexed by
 * BR_CP_HOLE_*.  A run that reports zeroes here ran none of the missing
 * physics; a run that reports non-zeroes ran a NO-OP in its place. */
enum { BR_CP_HOLE_TYRE, BR_CP_HOLE_DRIVE, BR_CP_HOLE_COLLIDE,
       BR_CP_HOLE_COUNT };
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

/* Build the rigid body the way the car constructor (D3D 0x10062C50 /
 * 0x10063000) does: masses, dimensions, inertia, both force lists, the four
 * wheel bodies and their mount points, and the identity state at
 * (0, 0, 2.0).  `aMount` is the four (x, y) mount offsets; pass NULL for the
 * symmetric default the constructor builds out of the car data object's
 * +0x80EC..+0x80FC. */
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

/* 0x10067C30, 762 B.  Four substeps of BrRbIntegrateState from `save` into
 * `next`, rebuilding the body matrix each time, then the roll-over /
 * stuck timer and the last-position store.  The five collision callees are
 * the BR_CP_HOLE_COLLIDE hook. */
void BrCarPhysAdvance(BrCarPhys *pCar);

/* 0x1005A7A0 -- one frame.  The whole point of this module. */
void BrCarPhysStep(BrCarPhys *pCar);

#endif /* BR_CARPHYS_H */
