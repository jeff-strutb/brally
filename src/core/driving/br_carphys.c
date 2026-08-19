/* br_carphys.c -- 0x1005A7A0 and the four force generators it drives.
 *
 * Transcribed from orig/BRGlide.dll:
 *
 *   0x1005A7A0  1206 B   BrCarPhysStep      the frame
 *   0x100684F0   265 B   BrCarPhysSpring    the suspension spring
 *   0x10068600   200 B   BrCarPhysDamper    the shock absorber
 *   0x10067F30   305 B   BrCarPhysDrag      aerodynamic drag
 *   0x10067C30   762 B   BrCarPhysAdvance   the four-substep position step
 *
 * 0x10067C30's own collision callees live in br_collresp.c; read that file's
 * header before changing BrCarPhysAdvance, because the substep loop's shape
 * (in particular the conditional BrRbQuatDerivative + BrRbBuildMatrix pair at
 * 0x10067DA7) is load-bearing and was missing here for three passes.
 *
 * plus the car constructor's rigid-body half, D3D 0x10062C50 / 0x10063000
 * (the Glide twins are in the same packet and were read for the offsets; the
 * IMMEDIATES quoted below were read out of BRD3D.dll because that is the
 * build whose disassembly of the constructor was already in work/).
 *
 * See br_carphys.h for the contracts, the ten steps, the sign-change damper,
 * the dead duplicate call and the three holes.
 *
 * ON THE COMPARISONS.  Same rule as br_phys.c and for the same reason: every
 * clamp below is the exact negation the x87 flag test implies.
 *     test ah,0x41 (C0|C3) taken -> "less, equal or unordered"
 *     test ah,1    (C0)    taken -> "less or unordered"
 *     test ah,0x40 (C3)    taken -> "equal or unordered"
 * An integrator is nothing but clamps, and one of them backwards inverts the
 * whole thing silently.
 */
#include <math.h>
#include <string.h>

#include "br_carphys.h"
#include "br_collresp.h"
#include "br_collrespsolve.h"

/* ==================================================================== */
/* The holes                                                             */
/* ==================================================================== */

BrCarPhysHooks g_brCarPhysHooks;
uint32_t       g_aBrCarPhysHole[BR_CP_HOLE_COUNT];

static const char *const g_aBrCarPhysHoleName[BR_CP_HOLE_COUNT] = {
    "0x10067710 OBB response (+0x10065C80)",
    "0x10068F80 car vs car  (1444 B)"
};

void BrCarPhysHoleReset(void)
{
    int i;
    for (i = 0; i < BR_CP_HOLE_COUNT; ++i) {
        g_aBrCarPhysHole[i] = 0u;
    }
}

const char *BrCarPhysHoleName(int i)
{
    if (i < 0 || i >= BR_CP_HOLE_COUNT) {
        return "(none)";
    }
    return g_aBrCarPhysHoleName[i];
}

/* ==================================================================== */
/* Two models of one object, bridged rather than cast                    */
/*                                                                       */
/* slice3_44.h's BrRbBody and slice3_42.h's BrRbBodyFull describe the SAME */
/* original 0x1DC-byte object.  BrRbBodyFull has five pointers in it, so   */
/* under LP64 `accel` sits at a different HOST offset in the two, and a    */
/* cast between them is exactly the "two models of one object" bug         */
/* CONVENTIONS.md records.  These two adapters copy the fields the callee  */
/* reads or writes instead.  They are the price of not merging the two     */
/* headers, which is not this module's to do.                              */
/* ==================================================================== */

BrRbState *BrCarPhysBodyState(BrRbBodyFull *pBody)
{
    /* body+0x78..+0xBC: pos, vel, quat, angVel, qDot -- 68 bytes of float in
     * the same order under both models.  test_carphys asserts the sizes. */
    return (BrRbState *)(void *)&pBody->f78;
}

/* 0x1006D600 == D3D 0x100743A0 == BrRbIntegrateVelocity, which reads only
 * pBody->accel and pBody->angAccel. */
/* WHAT IT DOES: adds one time step's worth of acceleration to a car's speed
 * and spin. This is a thin adapter: the actual work lives with the rigid-
 * body physics, and the copying here exists only because two modules model
 * the car's body differently. */
/* @implements 0x1006D600 glide BrCpIntegrateVelocity */
static void BrCpIntegrateVelocity(BrRbState *pS, const BrRbBodyFull *pB,
                                  float dt)
{
    BrRbBody tmp;

    memset(&tmp, 0, sizeof tmp);
    tmp.accel[0]    = pB->accel.x;
    tmp.accel[1]    = pB->accel.y;
    tmp.accel[2]    = pB->accel.z;
    tmp.angAccel[0] = pB->angAccel.x;
    tmp.angAccel[1] = pB->angAccel.y;
    tmp.angAccel[2] = pB->angAccel.z;

    BrRbIntegrateVelocity(pS, &tmp, dt);
}

/* 0x10074870 == BrRbInitInertia, which reads mode/dim/mass and writes the two
 * 3x3 tensors plus a dozen scalars. */
static void BrCpInitInertia(BrRbBodyFull *pB)
{
    BrRbBody tmp;

    memset(&tmp, 0, sizeof tmp);
    tmp.mode   = pB->mode;
    tmp.dim[0] = pB->dim[0];
    tmp.dim[1] = pB->dim[1];
    tmp.dim[2] = pB->dim[2];
    tmp.mass   = pB->mass;

    BrRbInitInertia(&tmp);

    pB->f00        = tmp.f00;
    pB->f14        = tmp.f14;
    pB->inertia    = tmp.inertia;
    pB->invInertia = tmp.invInertia;
    pB->f19C       = tmp.f19C;
    pB->f1B4       = tmp.f1B4;
    pB->f1C0       = tmp.f1C0;
    pB->f1C4       = tmp.f1C4;
    pB->f1C8       = tmp.f1C8;
    pB->f1CC       = tmp.f1CC;
    pB->f1D0       = tmp.f1D0;
    pB->f1D4       = tmp.f1D4;
    pB->f1D8       = tmp.f1D8;
}

/* ==================================================================== */
/* 0x100684F0 -- the suspension spring                                   */
/* ==================================================================== */

float BrCarPhysSign(float v)
{
    /* `fcom` + `test ah,0x40` + `je` -- the ZERO arm is taken for EQUAL OR
     * UNORDERED, so NaN classifies as 0.0f and never reaches the second
     * test.  `!(v != 0)` would be wrong; `v == 0` is false for NaN, so the
     * unordered case has to be spelled out. */
    if (v == BR_CP_SIGN_ZERO || !(v == v)) {
        return BR_CP_SIGN_ZERO;
    }
    /* `fcom` + `test ah,0x41` + `jne` -- the +1 arm needs BOTH C0 and C3
     * clear, i.e. strictly greater.  Everything else is -1. */
    if (v > BR_CP_SIGN_ZERO) {
        return BR_CP_SIGN_POS;
    }
    return BR_CP_SIGN_NEG;
}

void BrCarPhysSpring(BrRbBodyFull *pBody, uint8_t *pTouchdown)
{
    BrRbForce *pNode = pBody->pForces;
    int        i;

    for (i = 0; i < 4; ++i) {
        BrRbBodyFull *pWheel;
        float         v;
        int32_t       contact, contactWas;

        if (pNode == NULL) {
            return;      /* the original walks blind; a truncated list here
                          * would be a construction bug, not a frame bug */
        }

        /* The x and y components are zeroed before anything else, every
         * frame, for all four nodes. */
        pNode->f.y = 0.0f;
        pNode->f.x = 0.0f;

        /* The four-arm jump table at 0x10068509 is `switch (i - 0)` over the
         * child array, in order. */
        pWheel = pBody->child[i];

        /* f1B4 is an int32 in the original (`mov eax, [ecx+0x1b4]`) and is
         * ALSO read as a u16 in the same breath (`mov di, word ptr ...`) so
         * the touchdown edge below can be tested on the low half only.
         * slice3_42.h types it float; the port keeps the original's integer
         * reading and converts, because every use of it is a counter. */
        contact    = (int32_t)pWheel->f1B4;
        contactWas = (int32_t)(uint16_t)(uint32_t)contact;

        v = pWheel->f1D8;

        if (contact < BR_CP_CONTACT_MAX) {
            pWheel->f1B4 = (float)(contact + 1);
        }

        /* `fcom qword [0x10077BD0]` + `test ah,0x41` + `je` skips the block,
         * so the block runs for LESS-EQUAL-OR-UNORDERED.  The constant is a
         * DOUBLE and the operand a float, so the compare happens in double;
         * writing it against a double literal reproduces the boundary. */
        if (!((double)v > BR_CP_CONTACT_MIN)) {
            v            = BR_CP_SUSP_REST;
            pWheel->f1B4 = 0.0f;
        }

        /* `fcom` + `test ah,0x41` + `jne` skips the clamp, so the clamp runs
         * only for strictly greater.  NaN keeps v. */
        if (v > 0.0f) {
            v = 0.0f;
        }

        v = v - BR_CP_SUSP_REST;

        /* `fcom` + `test ah,1` + `je` skips, so the clamp runs for LESS OR
         * UNORDERED.  A NaN lands on 0.0f here -- which is the one place a
         * NaN gets absorbed rather than propagated. */
        if (!(v >= 0.0f)) {
            v = 0.0f;
        }

        /* sign(v) * v * v * k, with the multiply order traced: `fld st(1)`
         * then `fmulp st(2)` forms v*v, then `fmul st(1)` folds in the sign,
         * then `fmul [body+0x1B8]`. */
        pNode->f.z = BrCarPhysSign(v) * v * v * pBody->f1B8;

        /* The touchdown edge: the counter is non-zero NOW and its low 16 bits
         * were zero BEFORE.  0x100685D2 re-reads f1B4 rather than using the
         * value it incremented, so a wheel whose contact was just reset to 0
         * above does NOT trip this. */
        if ((int32_t)pWheel->f1B4 != 0 && contactWas == 0) {
            if (pTouchdown != NULL) {
                *pTouchdown = (uint8_t)BR_CP_TOUCHDOWN;
            }
        }

        pNode = pNode->pNext;
    }
}

/* ==================================================================== */
/* 0x10068600 -- the shock absorber                                      */
/* ==================================================================== */

void BrCarPhysDamper(BrRbBodyFull *pBody)
{
    BrRbForce *pNode = pBody->pForces;
    int        i;

    for (i = 0; i < 4; ++i) {
        BrRbBodyFull *pWheel;
        BrVec3        v;
        float         f;

        if (pNode == NULL) {
            return;
        }

        pNode->f.y = 0.0f;
        pNode->f.x = 0.0f;

        pWheel = pBody->child[i];

        /* 0x100642F0 == D3D 0x1006B340 == BrRbVelAtBodyPointXY, already
         * ported in slice3_42.c.  Only the Z component is read back; the
         * original still computes all three. */
        v.x = 0.0f; v.y = 0.0f; v.z = 0.0f;
        BrRbVelAtBodyPointXY(&v, pBody, pWheel);

        if ((int32_t)pWheel->f1B4 == 0) {
            f = 0.0f;
        } else if (!(v.z >= 0.0f)) {
            /* `fcomp` + `test ah,1` + `je` jumps to the multiply, so the
             * ZERO arm is LESS OR UNORDERED. */
            f = 0.0f;
        } else {
            f = pBody->f1BC * v.z;
        }

        pNode->f.z = f;
        pNode = pNode->pNext;
    }
}

/* ==================================================================== */
/* 0x10067F30 -- aerodynamic drag                                        */
/* ==================================================================== */

void BrCarPhysDrag(const BrRbBodyFull *pBody, const BrGroundHit aHit[4],
                   BrRbForce *pNode, int32_t mode)
{
    float speed, sq;
    int   i, onSurface = 0;

    pNode->f.x = pBody->vel.x * BR_CP_DRAG_K;
    pNode->f.y = pBody->vel.y * BR_CP_DRAG_K;
    pNode->f.z = pBody->vel.z * BR_CP_DRAG_K;

    /* The sum order is the original's: (vx*vx + vy*vy) + vz*vz, formed by
     * `faddp st(2)` then `faddp st(1)`. */
    sq = (pBody->vel.x * pBody->vel.x + pBody->vel.y * pBody->vel.y)
         + pBody->vel.z * pBody->vel.z;
    speed = (float)sqrt((double)sq);   /* 0x10002570 is `fld; fsqrt; ret` */

    /* `fcomp` + `test ah,0x41` + `jne` LEAVES on less-equal-or-unordered, so
     * the second term needs a strictly greater speed. */
    if (!(speed > BR_CP_DRAG_SPEED)) {
        return;
    }
    if (mode == 3) {
        return;
    }

    /* The four surface bytes are read with `movsx` -- SIGNED -- and compared
     * against 4 in the order wheel0, wheel1, wheel2, wheel3. */
    for (i = 0; i < 4; ++i) {
        if ((int)(signed char)aHit[i].surface == BR_CP_DRAG_SURFACE) {
            onSurface = 1;
            break;
        }
    }
    if (!onSurface) {
        return;
    }

    /* `fst [esp+0x18]` of the Z term is dead -- nothing reads that slot --
     * and is not reproduced because it has no observable effect. */
    pNode->f.y = pBody->vel.y * BR_CP_DRAG_K2 + pNode->f.y;
    pNode->f.z = pBody->vel.z * BR_CP_DRAG_K2 + pNode->f.z;
    pNode->f.x = pBody->vel.x * BR_CP_DRAG_K2 + pNode->f.x;
}

/* ==================================================================== */
/* 0x100651A0 -- the per-wheel tyre pass                                 */
/*                                                                       */
/* See br_carphys.h for what this is and, more importantly, for what it   */
/* is NOT: there is no slip angle, no lateral force and no load transfer  */
/* anywhere in these 1355 bytes.                                         */
/* ==================================================================== */

/* Every `fabs` in 0x100651A0 is spelled as `fcom 0` + `test ah,1` + a
 * conditional `fchs`, i.e. the sign is flipped for LESS OR UNORDERED.  So a
 * NaN comes back NEGATED, not absolute.  fabsf() clears the sign bit and is
 * therefore the wrong function here. */
static float BrCpAbsX87(float v)
{
    if (!(v >= 0.0f)) {
        return -v;
    }
    return v;
}

/* 0x100B4F30 and 0x100B5050, 288 bytes each, read out of BRGlide.dll.
 * [24*compound + 8*weather + surface]. */
const float g_aBrCarPhysGripA[BR_CP_GRIP_FLOATS] = {
    /* compound 0 */
    0.899999976f, 0.899999976f, 0.899999976f, 0.899999976f,
    0.899999976f, 0.899999976f, 0.899999976f, 0.899999976f,
    0.899999976f, 0.899999976f, 0.899999976f, 0.899999976f,
    0.899999976f, 0.899999976f, 0.899999976f, 0.899999976f,
    0.899999976f, 0.899999976f, 0.899999976f, 0.899999976f,
    0.899999976f, 0.899999976f, 0.899999976f, 0.899999976f,
    /* compound 1 */
    0.899999976f, 0.899999976f, 0.899999976f, 0.899999976f,
    0.899999976f, 0.899999976f, 0.899999976f, 0.899999976f,
    0.899999976f, 0.899999976f, 0.899999976f, 0.899999976f,
    0.899999976f, 0.899999976f, 0.899999976f, 0.899999976f,
    0.899999976f, 0.899999976f, 0.899999976f, 0.899999976f,
    0.899999976f, 0.899999976f, 0.899999976f, 0.899999976f,
    /* compound 2 */
    0.899999976f, 0.899999976f, 0.899999976f, 0.899999976f,
    0.899999976f, 0.899999976f, 0.899999976f, 0.899999976f,
    0.899999976f, 0.899999976f, 0.899999976f, 0.899999976f,
    0.899999976f, 0.899999976f, 0.899999976f, 0.899999976f,
    0.899999976f, 0.899999976f, 0.899999976f, 0.899999976f,
    0.899999976f, 0.899999976f, 0.899999976f, 0.899999976f
};

const float g_aBrCarPhysGripB[BR_CP_GRIP_FLOATS] = {
    /* compound 0 */
    0.649999976f, 0.649999976f, 0.649999976f, 0.850000024f,
    0.649999976f, 0.649999976f, 0.649999976f, 0.649999976f,
    0.649999976f, 0.649999976f, 0.649999976f, 0.850000024f,
    0.649999976f, 0.649999976f, 0.649999976f, 0.649999976f,
    0.550000012f, 0.550000012f, 0.550000012f, 0.550000012f,
    0.550000012f, 0.550000012f, 0.550000012f, 0.550000012f,
    /* compound 1 */
    0.600000024f, 0.600000024f, 0.600000024f, 0.899999976f,
    0.600000024f, 0.600000024f, 0.600000024f, 0.600000024f,
    0.600000024f, 0.600000024f, 0.600000024f, 0.899999976f,
    0.600000024f, 0.600000024f, 0.600000024f, 0.600000024f,
    0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
    /* compound 2 */
    0.550000012f, 0.550000012f, 0.550000012f, 0.949999988f,
    0.550000012f, 0.550000012f, 0.550000012f, 0.550000012f,
    0.550000012f, 0.550000012f, 0.550000012f, 0.949999988f,
    0.550000012f, 0.550000012f, 0.550000012f, 0.550000012f,
    0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f
};

/* See the DEVIATION in br_carphys.h: the original's 0x11773690 is .bss and
 * starts NULL. */
const float *g_pBrCarPhysGrip = g_aBrCarPhysGripA;

int32_t g_brCarPhysWeather;          /* 0x104B15E8, .bss, starts 0 */

void BrCarPhysSelectCar(int32_t iCar)
{
    /* 0x10069530: `cmp eax,0xE; ja setB`, then `cl = [eax+0x100695B0]` and a
     * five-way jump through 0x1006959C whose five targets are
     * A, A, A, B, B.  The 15 map bytes are 0 0 0 0 0 4 1 1 1 1 1 4 2 4 3. */
    static const unsigned char kMap[15] = {
        0, 0, 0, 0, 0, 4, 1, 1, 1, 1, 1, 4, 2, 4, 3
    };
    int arm;

    if ((uint32_t)iCar > 14u) {
        arm = 3;                     /* the `ja` target is set B */
    } else {
        arm = kMap[iCar];
    }
    g_pBrCarPhysGrip = (arm >= 3) ? g_aBrCarPhysGripB : g_aBrCarPhysGripA;
}

/* 0x1006543F..0x10065486 and the identical pair inside 0x100645A0.  The
 * weather index is `n - 1` clamped into [0, 2] by SIXTEEN-BIT signed
 * compares, so 0x10001 answers row 0 and not row 0x10000. */
static int BrCpWeatherRow(void)
{
    int16_t w = (int16_t)(g_brCarPhysWeather - 1);

    if (w > 2 || w < 0) {
        return 0;
    }
    return (int)w;
}

void BrCarPhysTyre(BrCarPhys *pCar, int iWheel, float *pA,
                   const uint8_t *pB, float dt)
{
    BrRbBodyFull      *pBody  = &pCar->body;
    BrRbBodyFull      *pWheel = pBody->child[iWheel];
    const BrGroundHit *pHit   = &pCar->aHit[iWheel];
    BrRbForce         *pNode;
    BrVec3             axis, a, c, d, e, v, fw, fb;
    float              nx, ny, nz;
    float              cs, sn, q, load, dot, w;
    int                i;

    /* 0x100651BA: `mov eax,[esi+0x19c]; test eax,eax; je ret`.  br_phys.h
     * carries the original's NULL/non-NULL as 0.0f/1.0f. */
    if (pWheel->f19C == 0.0f) {
        return;
    }

    nx = pHit->nx;
    ny = pHit->ny;
    nz = pHit->nz;

    /* 0x100651D0: `fcomp qword [0x10077AE0]` + `test ah,1` + `jne ret`, so
     * the function LEAVES on less-or-unordered.  Written negated: a NaN
     * normal leaves.  The constant is a double and the operand a float, so
     * the compare happens in double. */
    if (!((double)nz >= BR_CP_TYRE_MINNZ)) {
        return;
    }

    /* 0x100651AA/B2/C0 build (0, 1, 0) on the stack and 0x1006D9D0 ==
     * BrMat4MulVec3Transposed rotates it by the CHASSIS matrix, so `a` is
     * row 1 of m -- the car's own +Y axis in world space. */
    axis.x = 0.0f;
    axis.y = 1.0f;
    axis.z = 0.0f;
    BrMat4MulVec3Transposed(&a, &pBody->m, &axis);

    /* 0x10065201..0x1006525B: c = a x n, component order traced through the
     * fxch chain rather than assumed. */
    c.x = a.y * nz - a.z * ny;
    c.y = a.z * nx - a.x * nz;
    c.z = a.x * ny - a.y * nx;

    /* 0x1006526B..0x100652C1: d = n x c, i.e. the SAME cross with the
     * operands the other way round.  For a unit n that is `a` projected into
     * the ground plane. */
    d.x = c.z * ny - c.y * nz;
    d.y = c.x * nz - c.z * nx;
    d.z = c.y * nx - c.x * ny;

    /* 0x100652C5 / 0x100652D8: 0x100023E0 is `fld [esp+4]; fcos; ret` and
     * 0x10002560 is the same with `fsin`.  Both take wheel->f1C0. */
    cs = (float)cos((double)pWheel->f1C0);
    sn = (float)sin((double)pWheel->f1C0);

    /* 0x100652DD..0x1006535B: e = cos*c + sin*d, each component formed as
     * `sin*d + cos*c`. */
    e.x = sn * d.x + cs * c.x;
    e.y = sn * d.y + cs * c.y;
    e.z = sn * d.z + cs * c.z;

    /* 0x1006534B..0x1006538E: all four children must have a contact count.
     * The original tests them in the order 0, 2, 1, 3. */
    for (i = 0; i < 4; ++i) {
        static const int kOrder[4] = { 0, 2, 1, 3 };
        if ((int32_t)pBody->child[kOrder[i]]->f1B4 == 0) {
            /* 0x10065588: the free-spin arm.  NO force is written -- one
             * wheel off the ground silently disables all four tyres. */
            w = pWheel->f1CC * dt + pWheel->f1C4;
            goto spin;
        }
    }

    /* 0x1006539B: 0x100643E0 == BrRbVelAtBodyPoint, the velocity of the
     * wheel's own mount point in the BODY frame. */
    v.x = 0.0f; v.y = 0.0f; v.z = 0.0f;
    BrRbVelAtBodyPoint(&v, pBody, pWheel);

    /* 0x100653B2: torque / radius == force. */
    q = pWheel->f1CC / pWheel->f1C8;

    /* 0x100653CF..0x10065429: the vertical load this wheel is assumed to
     * carry.  Note the wheel's OWN mass enters as `- mass * -4`. */
    load = ((pBody->mass - pWheel->mass * BR_CP_TYRE_WHEEL_K)
            * BR_CP_TYRE_LOAD_G) * nz * BR_CP_TYRE_LOAD_K;

    /* 0x100653E7..0x10065431: dot(v, e), associated (x + y) + z. */
    dot = (v.x * e.x + v.y * e.y) + v.z * e.z;

    /* 0x1006541B/0x10065433: `fsubr [eax]`, i.e. *pA - (q * -0.5). */
    *pA = *pA - q * BR_CP_TYRE_REPORT;

    /* 0x1006543B: ONE BYTE of pB.  The step clears bE80 immediately before
     * the two front calls, so in practice only the rear pair can ever get
     * here -- and only once the drivetrain has set bE78. */
    if (*pB != 0u) {
        int idx = BrCpWeatherRow() * BR_CP_GRIP_SURFACES
                  + 24 * (int)pCar->b1FD
                  + (int)pHit->surface;   /* `movzx`, not `movsx` */
        q = g_pBrCarPhysGrip[idx] * q;
    }

    /* 0x1006548A..0x100654E4.  THE TRACTION MODEL, and it is not a
     * saturation: once the demand passes the load the delivered force drops
     * to a TENTH of the load.  The compare is on the x87 magnitudes, and
     * the clamp arm needs a STRICT ordered greater-than (`test ah,0x41` +
     * `jne` skips it for less, equal or unordered). */
    if (BrCpAbsX87(q) > BrCpAbsX87(load)) {
        q = BrCpAbsX87(load / q) * q * BR_CP_TYRE_SPIN;
    }

    /* 0x100654E6..0x10065525: the force is -q along e, rotated into the body
     * frame by 0x1006D980 == BrMat4MulVec3. */
    fw.x = e.x * -q;
    fw.y = e.y * -q;
    fw.z = e.z * -q;
    BrMat4MulVec3(&fb, &pBody->m, &fw);

    /* 0x1006552A..0x10065557: three separate `mov eax,[esi+0x18]` reloads in
     * the original, one per component.  The node is the wheel's own list
     * head, which 0x1005A7A0 zeroed at the top of the frame. */
    pNode = pWheel->pForces;
    pNode->f.x = fb.x + pNode->f.x;
    pNode->f.y = fb.y + pNode->f.y;
    pNode->f.z = fb.z + pNode->f.z;

    /* 0x1006555A..0x10065584.  The wheel's spin: driven by f1CC, braked by
     * the reaction q*r, then relaxed toward rolling by 0.4 of the slip.  The
     * `f1CC` read here is the SAVED copy from before the grip table touched
     * q, which is the stack slot at [esp+0x58]. */
    w = (pWheel->f1CC - q * pWheel->f1C8) * dt + pWheel->f1C4;
    w = w - (pWheel->f1C8 * w + dot) * BR_CP_TYRE_RELAX;

spin:
    /* 0x10065598: `fcom` then `fst`, so f1C4 takes the unclamped value and
     * the clamp below overwrites it. */
    pWheel->f1C4 = w;

    /* 0x1006559E..0x10065614: |w| > 300 replaces it with sign(w) * 300 --
     * through the same three-way classifier, so a NaN lands on 0 * 300. */
    if (BrCpAbsX87(w) > BR_CP_TYRE_SPIN_MAX) {
        pWheel->f1C4 = BrCarPhysSign(w) * BR_CP_TYRE_SPIN_MAX;
    }

    /* 0x10065616..0x10065643: the display angle, in DEGREES, and `_finite`
     * on it as a double. */
    pWheel->f1D4 = pWheel->f1D4
                   - (pWheel->f1C4 * BR_CP_RAD_TO_DEG) * dt;

    if (!isfinite((double)pWheel->f1D4)) {
        pWheel->f1D4 = 0.0f;         /* 0x100656DA */
        return;
    }
    /* 0x10065649 / 0x1006565C: both bounds are doubles, and both reject to
     * the same store of 0. */
    if (!((double)pWheel->f1D4 >= -BR_CP_ANGLE_LIMIT)) {
        pWheel->f1D4 = 0.0f;
        return;
    }
    if (!((double)pWheel->f1D4 < BR_CP_ANGLE_LIMIT)) {
        pWheel->f1D4 = 0.0f;
        return;
    }

    /* 0x1006566F: fold down.  The original RELOADS and RESTORES f1D4 every
     * iteration here... */
    while ((double)pWheel->f1D4 > BR_CP_ANGLE_TURN) {
        pWheel->f1D4 = (float)((double)pWheel->f1D4 - BR_CP_ANGLE_TURN);
    }

    /* 0x100656A1: ...and fold up, where it does NOT -- the loop target is
     * the FSUB, so the accumulation stays in the x87 register and is stored
     * once at the end.  Kept as a double accumulator for that reason.  The
     * operand here is the FLOAT -360 at 0x10077B28, not the double. */
    if (!((double)pWheel->f1D4 >= BR_CP_ANGLE_ZERO)) {
        double t = (double)pWheel->f1D4;
        do {
            t = t - (double)BR_CP_ANGLE_TURN_N;
        } while (!(t >= BR_CP_ANGLE_ZERO));
        pWheel->f1D4 = (float)t;
    }
}

/* ==================================================================== */
/* 0x100645A0 -- the drivetrain, i.e. the axle velocity constraint       */
/*                                                                       */
/* See br_carphys.h.  The short version: this is what stops the car       */
/* spinning, and it does it by OVERWRITING body->vel and body->angVel,    */
/* not by adding a force.                                                */
/* ==================================================================== */

const float g_aBrCarPhysDrvT1A[BR_CP_GRIP_FLOATS] = {
    /* compound 0 */
    0.0820000023f, 0.0820000023f, 0.0820000023f, 0.0799999982f, 0.0820000023f, 0.0820000023f, 0.0820000023f, 0.0820000023f,
    0.0820000023f, 0.0820000023f, 0.0719999969f, 0.0719999969f, 0.0820000023f, 0.0820000023f, 0.0820000023f, 0.0820000023f,
    0.0649999976f, 0.0649999976f, 0.0649999976f, 0.0649999976f, 0.0649999976f, 0.0649999976f, 0.0649999976f, 0.0649999976f,
    /* compound 1 */
    0.0799999982f, 0.0799999982f, 0.0799999982f, 0.0850000009f, 0.0799999982f, 0.0799999982f, 0.0799999982f, 0.0799999982f,
    0.0799999982f, 0.0799999982f, 0.0700000003f, 0.075000003f, 0.0799999982f, 0.0799999982f, 0.0799999982f, 0.0799999982f,
    0.0599999987f, 0.0599999987f, 0.0599999987f, 0.0599999987f, 0.0599999987f, 0.0599999987f, 0.0599999987f, 0.0599999987f,
    /* compound 2 */
    0.0780000016f, 0.0780000016f, 0.0780000016f, 0.0900000036f, 0.0780000016f, 0.0780000016f, 0.0780000016f, 0.0780000016f,
    0.0780000016f, 0.0780000016f, 0.0680000037f, 0.075000003f, 0.0780000016f, 0.0780000016f, 0.0780000016f, 0.0780000016f,
    0.0599999987f, 0.0599999987f, 0.0599999987f, 0.0599999987f, 0.0599999987f, 0.0599999987f, 0.0599999987f, 0.0599999987f
};
const float g_aBrCarPhysDrvT1B[BR_CP_GRIP_FLOATS] = {
    /* compound 0 */
    0.075000003f, 0.075000003f, 0.075000003f, 0.0799999982f, 0.075000003f, 0.075000003f, 0.075000003f, 0.075000003f,
    0.0719999969f, 0.0719999969f, 0.0719999969f, 0.0719999969f, 0.0719999969f, 0.0719999969f, 0.0719999969f, 0.0719999969f,
    0.0649999976f, 0.0649999976f, 0.0649999976f, 0.0649999976f, 0.0649999976f, 0.0649999976f, 0.0649999976f, 0.0649999976f,
    /* compound 1 */
    0.0700000003f, 0.0700000003f, 0.0700000003f, 0.0850000009f, 0.0700000003f, 0.0700000003f, 0.0700000003f, 0.0700000003f,
    0.0700000003f, 0.0700000003f, 0.0700000003f, 0.075000003f, 0.0700000003f, 0.0700000003f, 0.0700000003f, 0.0700000003f,
    0.0599999987f, 0.0599999987f, 0.0599999987f, 0.0599999987f, 0.0599999987f, 0.0599999987f, 0.0599999987f, 0.0599999987f,
    /* compound 2 */
    0.0649999976f, 0.0649999976f, 0.0649999976f, 0.0900000036f, 0.0649999976f, 0.0649999976f, 0.0649999976f, 0.0649999976f,
    0.0680000037f, 0.0680000037f, 0.0680000037f, 0.075000003f, 0.0680000037f, 0.0680000037f, 0.0680000037f, 0.0680000037f,
    0.0599999987f, 0.0599999987f, 0.0599999987f, 0.0599999987f, 0.0599999987f, 0.0599999987f, 0.0599999987f, 0.0599999987f
};
const float g_aBrCarPhysDrvT2A[BR_CP_DRV_T23_FLOATS] = {
    90000.0f, 90000.0f, 90000.0f, 80000.0f, 90000.0f, 90000.0f, 90000.0f, 90000.0f,
    90000.0f, 90000.0f, 90000.0f, 90000.0f, 90000.0f, 90000.0f, 90000.0f, 90000.0f,
    120000.0f, 120000.0f, 120000.0f, 120000.0f, 120000.0f, 120000.0f, 120000.0f, 120000.0f
};
const float g_aBrCarPhysDrvT2B[BR_CP_DRV_T23_FLOATS] = {
    90000.0f, 90000.0f, 90000.0f, 80000.0f, 90000.0f, 90000.0f, 90000.0f, 90000.0f,
    90000.0f, 90000.0f, 90000.0f, 90000.0f, 90000.0f, 90000.0f, 90000.0f, 90000.0f,
    120000.0f, 120000.0f, 120000.0f, 120000.0f, 120000.0f, 120000.0f, 120000.0f, 120000.0f
};
const float g_aBrCarPhysDrvT3[BR_CP_DRV_T23_FLOATS] = {
    3000.0f, 3000.0f, 3000.0f, 3000.0f, 3000.0f, 3000.0f, 3000.0f, 3000.0f,
    3000.0f, 3000.0f, 3000.0f, 3000.0f, 3000.0f, 3000.0f, 3000.0f, 3000.0f,
    3000.0f, 3000.0f, 3000.0f, 3000.0f, 3000.0f, 3000.0f, 3000.0f, 3000.0f
};

/* 0x11778808 / 0x11778820.  Same DEVIATION as g_pBrCarPhysGrip: .bss in the
 * original, aimed here at the set 0x10069530 gives car index 0. */
const float *g_pBrCarPhysDrvT1 = g_aBrCarPhysDrvT1A;
const float *g_pBrCarPhysDrvT2 = g_aBrCarPhysDrvT2A;

/* 0x100649D4..0x10064A4B and the identical block at 0x10064EC6..0x10064F21,
 * then 0x10064A4F..0x10064B38 / 0x10064F25..0x10064FE8.
 *
 * The surface index is the MEAN of the pair's two surface bytes, formed as
 * `(s0 + s1 + 1) >> 1` on zero-extended bytes -- so it rounds up, and a pair
 * straddling surfaces 3 and 4 answers 4.
 *
 * Returns the fraction of the axle's lateral velocity to remove. */
static float BrCpDrvSlip(BrCarPhys *pCar, int iA, int iB, float demand)
{
    BrRbBodyFull *pBody = &pCar->body;
    int   w   = BrCpWeatherRow() * BR_CP_GRIP_SURFACES;
    int   s   = ((int)pCar->aHit[iA].surface
                 + (int)pCar->aHit[iB].surface + 1) >> 1;
    float t1  = g_pBrCarPhysDrvT1[w + 24 * (int)pCar->b1FD + s];
    float t2  = g_pBrCarPhysDrvT2[w + s];
    float t3  = g_aBrCarPhysDrvT3[w + s];
    float p   = demand;
    float steer, sq, speed;

    /* `fcomp` + `test ah,0x41` + `jne` SKIPS the replacement for less, equal
     * or unordered, so only a strict ordered greater caps it. */
    if (p > t2) {
        p = t2;
    }
    /* `fcom` + `test ah,1` + `je` skips, so the floor applies for less or
     * unordered. */
    if (!(p >= t3)) {
        p = t3;
    }
    p = t3 / p;                                   /* fdivr */

    p = p * t1 * BR_CP_DRV_GRIP_SCALE;

    /* `fcomp 0` + `test ah,0x40` + `je` skips, so the bonus applies for
     * EQUAL OR UNORDERED -- the same three-way idiom BrCarPhysSign uses, and
     * a NaN steer angle takes it. */
    steer = pBody->child[2]->f1C0;
    if (steer == 0.0f || !(steer == steer)) {
        p = (float)((double)p * BR_CP_DRV_STRAIGHT);
    }

    /* |p| > 1 gives exactly 1.0f -- NOT sign(p), which is what the store of
     * the immediate 0x3F800000 at 0x10064A96 says. */
    if (BrCpAbsX87(p) > 1.0f) {
        p = 1.0f;
    }

    /* 0x10064AC2..0x10064B38: the speed of the WHOLE BODY, summed
     * (vx*vx + vy*vy) + vz*vz in the original's association. */
    sq = (pBody->vel.x * pBody->vel.x + pBody->vel.y * pBody->vel.y)
         + pBody->vel.z * pBody->vel.z;
    speed = (float)sqrt((double)sq);

    /* `fcom` + `test ah,1` + `je` skips the floor, so it applies for less or
     * unordered. */
    if (!(speed >= BR_CP_DRV_SLOW_SPEED)) {
        float cand = (BR_CP_DRV_SLOW_SPEED - speed) * BR_CP_DRV_SLOW_K;
        if (!(p >= cand)) {
            p = cand;
        }
    }
    return p;
}

/* 0x10064603..0x100647EE, once per axle.  The axle's brake contribution, in
 * the same units as the axle velocities below. */
static float BrCpDrvBrake(BrRbBodyFull *pBody, BrRbBodyFull *pWheel, float dt)
{
    float b;

    /* sign(f1C4) is the same three-way classifier again; |f1D0| is the x87
     * abs, so a NaN brake torque comes back negated. */
    b = BrCpAbsX87(pWheel->f1D0) * BrCarPhysSign(pWheel->f1C4)
        * BR_CP_DRV_BRAKE_K;
    b = b / pWheel->f1C8;
    b = b / (pBody->mass * BR_CP_DRV_MASS_K);
    /* Both `fmul [esp+0xa4]` pairs are on the SAME value: dt appears twice. */
    b = b * dt * dt;

    if (BrCpAbsX87(b) > 1.0f) {
        b = BrCarPhysSign(b) * BR_CP_DRV_BRAKE_MAX;
    }
    return b;
}

void BrCarPhysDrive(BrCarPhys *pCar, float dt)
{
    BrRbBodyFull *pBody = &pCar->body;
    BrVec3        p, tmp, vA, vB, world;
    float         brakeA, brakeB;
    float         sideForce = 0.0f;     /* [esp+0x58], zeroed at 0x100645B6 */
    int           ran       = 0;        /* [esp+0x5c], likewise            */
    float         roll, target;
    int           i;

    /* 0x100645C2..0x100645FD: a wheel with no contact RECORD has its contact
     * COUNT cleared, which is how a wheel that has left the ground stops
     * counting as loaded for the tests below. */
    for (i = 0; i < 4; ++i) {
        if (pBody->child[i]->f19C == 0.0f) {
            pBody->child[i]->f1B4 = 0.0f;
        }
    }

    /* 0x10064603: the brake terms, pair B first (child[2]) then pair A
     * (child[0]) -- that is the original's order and the two divisions are
     * interleaved, which is why they are read out here rather than folded. */
    brakeB = BrCpDrvBrake(pBody, pBody->child[2], dt);
    brakeA = BrCpDrvBrake(pBody, pBody->child[0], dt);

    /* 0x100647F2..0x10064833: the velocity at pair A's axle centre, in world
     * space.  The point is (child0->f78.x, 0, 0) -- Y and Z are the two
     * dwords zeroed at 0x100647F2/0x100647FA. */
    p.x = pBody->child[0]->f78.x;
    p.y = 0.0f;
    p.z = 0.0f;
    BrRbVelAtPoint(&tmp, pBody, &p);
    BrMat4MulVec3(&vA, &pBody->m, &tmp);

    /* 0x1006488C.  Cleared every call, set only by the slide test below. */
    pCar->b209 = 0u;

    /* 0x10064893..0x100648C5: at least one wheel of EACH pair must be in
     * contact.  Otherwise pair A's gate is cleared and the whole axle
     * correction is skipped. */
    if (((int32_t)pBody->child[0]->f1B4 != 0
         || (int32_t)pBody->child[1]->f1B4 != 0)
        && ((int32_t)pBody->child[2]->f1B4 != 0
            || (int32_t)pBody->child[3]->f1B4 != 0)) {
        float demand, hold, add;
        int   braking;

        /* 0x100648CA..0x10064928.  Pair A is the UNSTEERED one, so its
         * lateral velocity is just the world Y. */
        demand = BrCpAbsX87(vA.y) * pBody->mass / dt
                 + BrCpAbsX87(pCar->fE7C);

        /* 0x1006492A..0x100649B4.  Whether pair B is braking chooses WHICH
         * penalty a braking pair A attracts; the flag itself is always pair
         * A's.  Both constants are negative and are SUBTRACTED. */
        add = ((double)BrCpAbsX87(brakeB) > BR_CP_DRV_BRAKE_EPS)
              ? BR_CP_DRV_BRAKE_ADD1 : BR_CP_DRV_BRAKE_ADD2;
        braking = ((double)BrCpAbsX87(brakeA) > BR_CP_DRV_BRAKE_EPS);
        demand  = demand - (float)braking * add;

        /* 0x100649B6: the hold-off is lower once the gate is already set. */
        hold = (pCar->bE80 != 0u) ? BR_CP_DRV_HOLD_ON : BR_CP_DRV_HOLD_OFF;

        ran        = 1;                 /* 0x10064920 */
        pCar->bE80 = 0u;                /* 0x100649E1 */

        /* 0x10064A9E: `test ah,1` + `je` takes the correction when C0 is
         * CLEAR, i.e. demand >= hold.  Note the rear block below uses a
         * strict greater instead; the asymmetry is the original's. */
        if (demand >= hold) {
            float f = BrCpDrvSlip(pCar, 0, 1, demand);
            float lat = vA.y;

            vA.y       = lat - f * lat;   /* 0x10064B46 */
            pCar->bE80 = 1u;              /* 0x10064B4A */
            sideForce  = f * vA.y;        /* 0x10064B53, on the NEW vA.y */
        } else {
            vA.y      = 0.0f;             /* 0x10064AB9 */
            sideForce = 0.0f;
        }

        /* 0x10064B59..0x10064BCB: the "sliding" flag.  Above 1 m/s of X the
         * test is on the RATIO, below it on the lateral speed alone. */
        if (BrCpAbsX87(vA.x) > 1.0f) {
            if (BrCpAbsX87(vA.y / vA.x) > BR_CP_DRV_SLIDE_RATIO) {
                pCar->b209 = 0x80u;
            }
        } else if (BrCpAbsX87(vA.y) > 1.0f) {
            pCar->b209 = 0x80u;
        } else {
            pCar->b209 = 0u;
        }

        /* 0x10064BD2..0x10064C87: the brake bites on X, and a brake that
         * would REVERSE X zeroes it instead -- the same sign-change rule as
         * 0x1005AA52, with a dead band of 1e-5. */
        {
            float was = vA.x;
            vA.x = vA.x - brakeA;
            if (BrCpAbsX87(vA.x) > BR_CP_DRV_ZERO_EPS) {
                /* `test ah,0x40` + `jne` SKIPS the zeroing when the two
                 * signs compare EQUAL OR UNORDERED. */
                if (!(BrCarPhysSign(vA.x) == BrCarPhysSign(was))) {
                    vA.x = 0.0f;
                }
            }
        }
    } else {
        /* 0x100648C2.  vA keeps exactly what the transform gave it -- the
         * original jumps straight to 0x10064C8F with the slot untouched. */
        pCar->bE80 = 0u;
    }

    /* 0x10064C8F: pair B's axle velocity, computed unconditionally -- even
     * on the path that just bailed out of pair A. */
    p.x = pBody->child[2]->f78.x;
    p.y = 0.0f;
    p.z = 0.0f;
    BrRbVelAtPoint(&tmp, pBody, &p);
    BrMat4MulVec3(&vB, &pBody->m, &tmp);

    /* 0x10064CC2..0x10064CFB: the same both-pairs test again, in the other
     * order.  Failing it skips pair B's correction but NOT the write-back,
     * which is gated on `ran` alone. */
    if (((int32_t)pBody->child[2]->f1B4 != 0
         || (int32_t)pBody->child[3]->f1B4 != 0)
        && ((int32_t)pBody->child[0]->f1B4 != 0
            || (int32_t)pBody->child[1]->f1B4 != 0)) {
        float cs, sn, along, demand, hold;
        BrVec3 lat, was;
        int    braking;

        ran = 1;                         /* 0x10064D0D */

        /* 0x10064D01..0x10064D5D: pair B IS the steered pair, so its lateral
         * part is what is left after projecting onto (cos, sin, 0). */
        cs = (float)cos((double)pBody->child[2]->f1C0);
        sn = (float)sin((double)pBody->child[2]->f1C0);
        along = vB.x * cs + vB.y * sn;

        was = vB;
        lat.x = vB.x - along * cs;
        lat.y = vB.y - along * sn;
        lat.z = vB.z - 0.0f;             /* the Z term is `- 0`, stored as 0 */

        /* 0x10064DE3..0x10064E27: |lat| * mass / dt + |*pA2|. */
        {
            float sq = (lat.x * lat.x + lat.y * lat.y) + lat.z * lat.z;
            demand = (float)sqrt((double)sq) * pBody->mass / dt
                     + BrCpAbsX87(pCar->fE74);
        }

        /* 0x10064E39..0x10064E71.  Only ONE penalty constant here, and the
         * flag is pair B's own brake. */
        braking = ((double)BrCpAbsX87(brakeB) > BR_CP_DRV_BRAKE_EPS);
        demand  = demand - (float)braking * BR_CP_DRV_BRAKE_ADD1;

        hold = (pCar->bE78 != 0u) ? BR_CP_DRV_HOLD_ON : BR_CP_DRV_HOLD_OFF;
        pCar->bE78 = 0u;                 /* 0x10064E89 */

        /* 0x10064E8E: `test ah,0x41` + `jne` SKIPS for less, equal or
         * unordered -- a STRICT greater, unlike pair A's `>=`. */
        if (demand > hold) {
            float f = BrCpDrvSlip(pCar, 2, 3, demand);

            vB.x = was.x - lat.x * f;
            vB.y = was.y - lat.y * f;
            vB.z = was.z - lat.z * f;
            pCar->bE78 = 1u;             /* 0x10065020 */
        }
    }

    /* 0x10065030..0x100650D0.  The whole point of the function. */
    if (ran) {
        float x0   = pBody->child[0]->f78.x;
        float x2   = pBody->child[2]->f78.x;
        float velX = (vA.x + vB.x) * BR_CP_DRV_HALF;
        float yaw  = (vA.y - vB.y) / (x0 - x2);
        float velY = vA.y - yaw * x0;

        /* angVel: to world, replace Z with the solved yaw rate, back. */
        BrMat4MulVec3(&world, &pBody->m, &pBody->angVel);
        world.z = yaw;
        BrMat4MulVec3Transposed(&pBody->angVel, &pBody->m, &world);

        /* vel: to world, replace X and Y, back.  Z survives, which is what
         * leaves gravity and the suspension in charge of the vertical. */
        BrMat4MulVec3(&world, &pBody->m, &pBody->vel);
        world.x = velX;
        world.y = velY;
        BrMat4MulVec3Transposed(&pBody->vel, &pBody->m, &world);
    }

    /* 0x100650D3..0x1006519D: the chassis's own f1D4 -- the visual roll --
     * chases -8 * the retained side force at a fixed step.  The side force
     * is first clamped to +-0.5 through the three-way sign classifier, then
     * doubled, then multiplied by -4. */
    roll = sideForce;
    if (BrCpAbsX87(roll) > BR_CP_DRV_HALF) {
        roll = BrCarPhysSign(roll) * BR_CP_DRV_HALF;
    }
    /* `fadd st,st` then `fmul [0x10077AD0]`.  That is the SAME -4.0f the tyre
     * pass folds the wheel mass with -- one constant, two unrelated uses --
     * so it is spelled with the same name rather than duplicated. */
    target = (roll + roll) * BR_CP_TYRE_WHEEL_K;   /* i.e. -8 * sideForce */

    /* `fsub st(1)` forms f1D4 - target; within one step, snap. */
    if (!((double)BrCpAbsX87(pBody->f1D4 - target) >= BR_CP_DRV_ROLL_STEP)) {
        pBody->f1D4 = target;
    } else if (!(target > pBody->f1D4)) {
        /* `test ah,0x41` + `jne` takes this arm for less, equal or
         * unordered. */
        pBody->f1D4 = pBody->f1D4 - BR_CP_DRV_ROLL_STEP;
    } else {
        pBody->f1D4 = pBody->f1D4 - BR_CP_DRV_ROLL_STEPN;
    }
}

/* ==================================================================== */
/* 0x1005AA52 .. 0x1005AB7F -- the sign-change damper                    */
/* ==================================================================== */

void BrCarPhysSignDamp(BrRbState *pLive, const BrRbState *pNext)
{
    float       *pOldA = &pLive->angVel.x;      /* state + 0x28 */
    const float *pNewA = &pNext->angVel.x;
    float       *pOldV = &pLive->vel.x;         /* state + 0x0C */
    const float *pNewV = &pNext->vel.x;
    int          i;

    /* The original interleaves the two fields in ONE loop of three, walking
     * `ecx` from state+0x28 with `[ecx-0x1C]` as the second field.  Kept
     * interleaved, because a future byte-diff wants the same order. */
    for (i = 0; i < 3; ++i) {
        /* `fxch st(1)` + `fcomp st(1)` + `test ah,0x40` + `jne` stores the
         * NEW value on C3 -- equal or unordered -- and zero otherwise.  Both
         * operands are one of three constants, so unordered cannot arise. */
        if (BrCarPhysSign(pOldA[i]) == BrCarPhysSign(pNewA[i])) {
            pOldA[i] = pNewA[i];
        } else {
            pOldA[i] = 0.0f;
        }

        if (BrCarPhysSign(pOldV[i]) == BrCarPhysSign(pNewV[i])) {
            pOldV[i] = pNewV[i];
        } else {
            pOldV[i] = 0.0f;
        }
    }
}

/* ==================================================================== */
/* 0x10067C30 -- the four-substep position integration                   */
/* ==================================================================== */

void BrCarPhysAdvance(BrCarPhys *pCar)
{
    BrRbBodyFull   *pBody = &pCar->body;
    float           t     = BR_PHYS_DT;
    float           m22;
    BrCollRespFrame frame;
    BrMat4         *pMatBox;
    float           ext[4];
    BrCrEffect      eff;

    /* 0x10067C4E..0x10067C88: the candidate list's head and its bump cursor
     * are cleared once per frame, before the gather. */
    BrCollRespListReset();

    /* 0x10067CAB..0x10067CD6: the BROAD PHASE, once per frame.  Three 0.1f
     * immediates go into the scale slots, 0x1006DDD0 builds the box matrix
     * out of them and the body matrix, and 0x10066AD0(body, matBox) gathers
     * the nearby triangles into the list at 0x11778198.  PORTED -- see
     * br_collresp.h.  With the box coming out of the .rca the scale is
     * finite and this really runs; before the car-data loader landed every
     * transformed vertex was a NaN and the gather rejected everything. */
    memset(&frame, 0, sizeof frame);
    BrCollRespBuildBoxMatrix(&frame, &pBody->m, BR_CR_BROAD_SCALE,
                             BR_CR_BROAD_SCALE, BR_CR_BROAD_SCALE);
    (void)BrCollRespBroadPhase(pBody, BrCollRespFrameMat(&frame));

    if (BrCollRespBoxDegenerate(pCar->f1DC, pCar->f1E0, pCar->f1E4)) {
        ++g_cBrCollRespDegenerate;
    }

    for (;;) {
        /* 0x10067D31: 0x10066D70.  PORTED -- BrCollRespTipKick.  Its return
         * value only feeds the 0x10008D60 stub, which really is one `c3`
         * byte in this build, but the function itself writes save.angVel.
         * br_collresp.h explains what this header used to get wrong. */
        (void)BrCollRespTipKick(pBody, pCar->aHit, &pCar->bodyPlaneN,
                                &pCar->save, pCar->f1DC, pCar->f1E0,
                                pCar->f1E4, pCar->f1E8);

        /* 0x10067D4A: 0x10068F80, car versus car.  No arguments; it walks the
         * entrant array itself. */
        ++g_aBrCarPhysHole[BR_CP_HOLE_CARCAR];
        if (g_brCarPhysHooks.pfnCarCar != NULL) {
            g_brCarPhysHooks.pfnCarCar(pCar);
        }

        /* 0x1006D850 == D3D 0x100745F0 == BrRbIntegrateState(dst, src, dt). */
        BrRbIntegrateState(&pCar->next, &pCar->save, BR_CP_SUBSTEP);

        /* 0x1006D6B0 == BrRbBuildMatrix(&body->m, state). */
        BrRbBuildMatrix(&pBody->m, &pCar->next);

        /* 0x10067D74..0x10067D9B: the box matrix is rebuilt from the freshly
         * integrated body matrix, this time with (1/f1DC, 1/f1E0, 1/f1E4) as
         * the scale -- so the box becomes the unit cube 0x10066260 tests
         * against -- and handed to 0x10067710 with the body.
         *
         * 0x10067DA3: and if 0x10067710 says it rewrote `next`, the state's
         * quaternion derivative and the body matrix are rebuilt so the
         * rewrite survives the rest of the frame.  THAT conditional pair is
         * where a collision's rotational response reaches the integrator, and
         * it is the reason this call could never be a no-op hook: the hook had
         * no way to report that it changed anything.  0x10067710 has now
         * LANDED (BrCrRespWalk) and both are reproduced just below. */
        BrCollRespBuildBoxMatrix(&frame, &pBody->m,
                                 BR_CR_ONE / pCar->f1DC,
                                 BR_CR_ONE / pCar->f1E0,
                                 BR_CR_ONE / pCar->f1E4);

        /* 0x10067D84..0x10067D97: matBox.m[3][2] -= f1E8, applied to the box
         * matrix just built and on the SAME matrix the walker is handed.  It
         * shifts every triangle's box-space Z down by f1E8, lifting the
         * classified unit box clear of a surface the chassis rests ON: on flat
         * ground at the suspension rest the box no longer straddles the ground,
         * so 0x10067710 finds no contact and the spring balance stands (z ~=
         * 0.19, not the box-floor height ~0.40).
         *
         * br_collresp.h once filed this subtraction as a DEAD ACCUMULATOR
         * ("[R-0x08] never read").  IT IS LIVE.  [R-0x08] is matBox.m[3][2]
         * (byte 0x38 of the box matrix at [esp+0x1c]), it is the box's Z
         * translation, and the walker reads it: 0x1006DA20 BrMat4TransformPoint
         * adds [matrix+0x38] into out.z.  Dropping it here is what floats the
         * car -- verified against the BRGlide bytes. */
        pMatBox = BrCollRespFrameMat(&frame);
        pMatBox->m[3][2] -= pCar->f1E8;

        /* 0x10067D9B: 0x10067710(body, matBox) -- THE RESPONSE, consumer of the
         * list the broad phase filled.  The port expands the two original
         * arguments (body, matBox) into the chassis sub-objects the walker
         * reads, per br_carphys.h's LP64 rule: mass/invInertia/m off the body,
         * ext = f1DC..f1E8, next/save.pos/save.quat the sibling states.  The
         * impact record body+0x1EC..0x200 is modelled as a zeroed effect --
         * threshold reads 0 so the damping/peak path stays off (as for a zeroed
         * car record), and the colour/intensity the solver writes have no
         * consumer yet.  DEVIATION stated rather than hidden. */
        ++g_aBrCarPhysHole[BR_CP_HOLE_BOX];
        ext[0] = pCar->f1DC; ext[1] = pCar->f1E0;
        ext[2] = pCar->f1E4; ext[3] = pCar->f1E8;
        memset(&eff, 0, sizeof eff);
        {
            int nResp = BrCrRespWalk(pBody->mass, &pBody->invInertia, &pBody->m,
                                     ext, &pCar->next, &pCar->save.pos,
                                     (const BrVec3 *)&pCar->save.quat, &eff,
                                     pMatBox);
            g_cBrCollRespResponded += (uint32_t)nResp;
            if (nResp != 0) {
                /* 0x10067DA7: the walker rewrote `next`; the qDot + matrix
                 * rebuild are what make the rewrite survive the rest of the
                 * frame. */
                BrRbQuatDerivative(&pCar->next);
                BrRbBuildMatrix(&pBody->m, &pCar->next);
            }
        }

        /* 0x10067DBA: t -= 1/120, stored back, then compared.
         * `fcomp` + `test ah,0x41` + `je <loop>` continues while BOTH C0 and
         * C3 are clear, i.e. while t is strictly greater than 0.002.  A NaN
         * ENDS the loop, which is what `>` gives. */
        t = t - BR_CP_SUBSTEP;

        /* 0x10067DC4: the result becomes the next substep's source. */
        pCar->save = pCar->next;

        if (!(t > BR_CP_SUBSTEP_EPS)) {
            break;
        }
    }

    /* 0x10067DE2 -- one more matrix build outside the loop. */
    BrRbBuildMatrix(&pBody->m, &pCar->next);

    /* 0x10067E0A: m[2][2] against 0.5f.  The `fnstsw` is deferred past an
     * integer `cmp` so the SAME x87 result serves both arms below. */
    m22 = pBody->m.m[2][2];

    /* 0x10067E1C: and the result is copied to `save` a second time. */
    pCar->save = pCar->next;

    if (pCar->fAi != 0) {
        /* the AI arm: `cmp [car+0xF08], 0x1005E690` matched */
        if (!(m22 >= BR_CP_UPRIGHT_MIN)) {       /* C0: less or unordered */
            if (pCar->f1F8 >= 0) {
                --pCar->f1F8;
            } else {
                /* 0x10067E4D stores -1 and 0x10067E57 immediately re-reads,
                 * decrements and stores again, so the value lands on -2. */
                pCar->f1F8 = -1;
                pCar->f1F8 = pCar->f1F8 - 1;
            }
        } else {
            pCar->f1F8 = BR_CP_RESET_AI;
        }
    } else {
        if (!(m22 >= BR_CP_UPRIGHT_MIN)) {
            float dx, dy, dz, d2;

            if (pCar->f1F8 < 0) {
                pCar->f1F8 = -1;
            }
            dx = pBody->m.m[3][0] - pCar->lastPos.x;
            dy = pBody->m.m[3][1] - pCar->lastPos.y;
            dz = pBody->m.m[3][2] - pCar->lastPos.z;
            /* (dx*dx + dy*dy) + dz*dz, in the original's association. */
            d2 = (dx * dx + dy * dy) + dz * dz;
            /* `fcomp 1.0f` + `test ah,1` + `je <end>`: the decrement runs for
             * LESS OR UNORDERED, i.e. the car has not moved a metre. */
            if (!(d2 >= BR_CP_STUCK_DIST)) {
                --pCar->f1F8;
            }
        } else {
            pCar->f1F8 = BR_CP_RESET_HUMAN;
        }
    }

    /* 0x10067EFE: the previous-frame position, three raw dword copies. */
    pCar->lastPos.x = pBody->m.m[3][0];
    pCar->lastPos.y = pBody->m.m[3][1];
    pCar->lastPos.z = pBody->m.m[3][2];
}

/* ==================================================================== */
/* 0x1005A7A0 -- one frame                                               */
/* ==================================================================== */

void BrCarPhysStep(BrCarPhys *pCar)
{
    BrRbBodyFull *pBody  = &pCar->body;
    BrRbState    *pState = BrCarPhysBodyState(pBody);
    int           i;

    /* --- step 1 ---------------------------------------------------------
     * 0x1005A7BE.  The wheel lists are assigned in the order 0xD20, 0xD60,
     * 0xD40, 0xD80 against child[0..3] -- the middle two are CROSSED relative
     * to the address order, and the constructor links each of those nodes to
     * a shared weight node, so the crossing is real and not a misreading. */
    pBody->pForces           = &pCar->aListA[0];
    pBody->child[0]->pForces = &pCar->aWheelF[0];
    pBody->child[1]->pForces = &pCar->aWheelF[1];
    pBody->child[2]->pForces = &pCar->aWheelF[2];
    pBody->child[3]->pForces = &pCar->aWheelF[3];

    /* Each wheel's FIRST node has its force zeroed -- three separate stores
     * with the child pointer re-loaded each time in the original. */
    for (i = 0; i < 4; ++i) {
        BrRbForce *pN = pBody->child[i]->pForces;
        pN->f.x = 0.0f;
        pN->f.y = 0.0f;
        pN->f.z = 0.0f;
    }

    /* --- step 2: 0x100684F0 --------------------------------------------- */
    BrCarPhysSpring(pBody, &pCar->b208);

    /* --- step 3: 0x100651A0 x4, gated on car+0xE84 -----------------------
     * The four call sites at 0x1005A8DE / 0x1005A901 / 0x1005A91E /
     * 0x1005A93B were re-read from the pushes rather than taken on trust,
     * and the previous pass's note had ONE of them wrong: the gate byte is
     * &bE80 for wheels 0 and 1 and &bE78 for wheels 2 and 3, not &bE78 for
     * all four.  Only three of the four scalars are pre-cleared -- bE78 is
     * not, so the rear pair sees LAST frame's drivetrain flag. */
    if (pCar->fE84 == 0) {
        pCar->fE7C = 0.0f;
        pCar->fE74 = 0.0f;
        pCar->bE80 = 0u;

        BrCarPhysTyre(pCar, 0, &pCar->fE7C, &pCar->bE80, BR_PHYS_DT);
        BrCarPhysTyre(pCar, 1, &pCar->fE7C, &pCar->bE80, BR_PHYS_DT);
        BrCarPhysTyre(pCar, 2, &pCar->fE74, &pCar->bE78, BR_PHYS_DT);
        BrCarPhysTyre(pCar, 3, &pCar->fE74, &pCar->bE78, BR_PHYS_DT);
    }

    /* --- step 4: 0x1005A943 --------------------------------------------- */
    pCar->fE84       = 0;
    pBody->accel.x   = 0.0f;
    pBody->accel.y   = 0.0f;
    pBody->accel.z   = 0.0f;
    pBody->angAccel.x = 0.0f;
    pBody->angAccel.y = 0.0f;
    pBody->angAccel.z = 0.0f;

    /* --- step 5: 0x10064210 == BrRbAccumAll ----------------------------- */
    BrRbAccumAll(pBody);

    /* --- step 6: 0x1006D600 == BrRbIntegrateVelocity -------------------- */
    BrCpIntegrateVelocity(pState, pBody, BR_PHYS_DT);

    /* --- step 7: 0x100645A0(body, dt, &fE7C, &fE74, &fE80, &fE78) ------- */
    BrCarPhysDrive(pCar, BR_PHYS_DT);

    /* --- step 8: 0x1005A9B6 --------------------------------------------- */
    BrRbQuatDerivative(pState);

    /* The SECOND list, and the wheels drop theirs entirely.  Note the
     * original writes body->f18 to car+0xC20 BEFORE clearing the children,
     * and clears them in the order child0, child2, child1, child3. */
    pBody->pForces           = &pCar->aListB[0];
    pBody->child[0]->pForces = NULL;
    pBody->child[2]->pForces = NULL;
    pBody->child[1]->pForces = NULL;
    pBody->child[3]->pForces = NULL;

    /* 0x10067F30 fills the FIFTH node of list B, car+0xD00. */
    BrCarPhysDrag(pBody, pCar->aHit, &pCar->aListB[4], /*mode*/ 0);

    /* 0x10068600 fills the first four. */
    BrCarPhysDamper(pBody);

    pBody->accel.x   = 0.0f;
    pBody->accel.y   = 0.0f;
    pBody->accel.z   = 0.0f;
    pBody->angAccel.x = 0.0f;
    pBody->angAccel.y = 0.0f;
    pBody->angAccel.z = 0.0f;

    BrRbAccumAll(pBody);

    /* --- step 9: 0x1005AA34, the second velocity integration and the
     * SIGN-CHANGE DAMPER ------------------------------------------------- */
    pCar->next = *pState;
    BrCpIntegrateVelocity(&pCar->next, pBody, BR_PHYS_DT);

    BrCarPhysSignDamp(pState, &pCar->next);

    /* --- step 10: 0x1005AB85 -------------------------------------------- */
    pCar->save = *pState;
    BrRbQuatDerivative(&pCar->save);
    /* DEAD DUPLICATE, 0x1005ABAA.  BrRbQuatDerivative is a pure function of
     * angVel and quat and writes only qDot, so the second call cannot change
     * anything.  Preserved -- see the header. */
    BrRbQuatDerivative(&pCar->save);

    /* 0x10067C30(car, body) */
    BrCarPhysAdvance(pCar);

    /* 0x1005ABBC: the integrated state becomes the live one. */
    *pState = pCar->next;

    /* 0x10068450 == BrWheelSuspensionSetZ, ported in br_phys.c.  This is what
     * fills f1D8 for NEXT frame's spring, and the surface bytes for next
     * frame's drag. */
    BrWheelSuspensionSetZHit(pBody, pCar->aHit);

    /* 0x1005ABD5: rebuild each wheel's own matrix from its own state. */
    for (i = 0; i < 4; ++i) {
        BrRbBodyFull *pWheel = pBody->child[i];
        BrRbBuildMatrix(&pWheel->m, BrCarPhysBodyState(pWheel));
    }
}

/* ==================================================================== */
/* Construction -- D3D 0x10062C50 / 0x10063000                           */
/* ==================================================================== */

static void BrCpNode(BrRbForce *pN, BrRbForce *pNext, int32_t kind,
                     float fx, float fy, float fz,
                     float rx, float ry, float rz)
{
    /* 0x100746E0 writes seven dwords and never touches slot 0; the links are
     * a separate pass at 0x10063153 / 0x100632BC. */
    pN->pNext = pNext;
    pN->kind  = kind;
    pN->f.x   = fx; pN->f.y = fy; pN->f.z = fz;
    pN->r.x   = rx; pN->r.y = ry; pN->r.z = rz;
}

void BrCarPhysInit(BrCarPhys *pCar, const float aMount[4][2])
{
    /* The constructor's own default mounts, from car+0x29C4's +0x80EC..+0x80FC
     * used as (rear x, rear y) for wheels 2/3 and (front x, front y) for
     * wheels 0/1, with wheels 1 and 3 mirrored in y.  Nothing in this tree
     * reads a .rca's suspension block yet, so the fallback is the corner
     * geometry the force lists already encode. */
    static const float kMount[4][2] = {
        {  BR_CP_CORNER_X, -BR_CP_CORNER_Y },
        {  BR_CP_CORNER_X,  BR_CP_CORNER_Y },
        { -BR_CP_CORNER_X, -BR_CP_CORNER_Y },
        { -BR_CP_CORNER_X,  BR_CP_CORNER_Y }
    };
    const float (*pM)[2] = (aMount != NULL) ? aMount : kMount;
    BrRbState   *pState;
    int          i;

    memset(pCar, 0, sizeof *pCar);

    /* --- the chassis ---------------------------------------------------- */
    pCar->body.mode   = 1;
    pCar->body.dim[0] = BR_CP_BODY_DIM_X;
    pCar->body.dim[1] = BR_CP_BODY_DIM_Y;
    pCar->body.dim[2] = BR_CP_BODY_DIM_Z;
    pCar->body.mass   = BR_CP_BODY_MASS;
    BrCpInitInertia(&pCar->body);

    /* car+0x340..0x34C, the collision box.  The constructor 0x1005BCC0 does
     * NOT write it -- no store anywhere in it carries a displacement of
     * 0x340 -- so it starts as the memset above leaves it, all zero, and
     * 0x1006FD90 is what fills it.  See br_cardata.h for why this header and
     * br_collresp.h used to say otherwise.
     *
     * The zeros are left implicit rather than written out, because writing
     * them would once again claim they are a constructor's answer. */

    /* car+0x31C / +0x320 */
    pCar->body.f1B8 = (BR_CP_SPRING_BASE
                       - (float)pCar->suspIndex * BR_CP_SPRING_STEP)
                      * BR_CP_SPRING_SCALE;
    pCar->body.f1BC = BR_CP_DAMPER_K;

    pState = BrCarPhysBodyState(&pCar->body);
    pState->pos.x = 0.0f;
    pState->pos.y = 0.0f;
    pState->pos.z = 2.0f;         /* 0x40000000 at 0x10062CD8 */
    pState->quat.f00 = 1.0f;      /* scalar first */
    BrRbBuildMatrix(&pCar->body.m, pState);

    /* --- the four wheels ------------------------------------------------ */
    for (i = 0; i < 4; ++i) {
        BrRbBodyFull *pW = &pCar->wheel[i];
        BrRbState    *pS;

        pW->mode = 2;             /* "no torque" */
        pW->mass = 0.0f;
        BrCpInitInertia(pW);

        pS = BrCarPhysBodyState(pW);
        pS->pos.x = pM[i][0];
        pS->pos.y = pM[i][1];
        pS->pos.z = BR_CP_WHEEL_Z;
        pS->quat.f00 = 1.0f;
        BrRbBuildMatrix(&pW->m, pS);

        /* f1B4 is the contact counter and starts clear; the constructor
         * leaves it so via BrRbInitInertia. */
        pCar->body.child[i] = pW;
    }

    /* --- force list A: four corners (kind 1) then gravity (kind 0) ------ */
    BrCpNode(&pCar->aListA[0], &pCar->aListA[1], 1, 0.0f, 0.0f, 0.0f,
             -BR_CP_CORNER_X, -BR_CP_CORNER_Y, 0.0f);
    BrCpNode(&pCar->aListA[1], &pCar->aListA[2], 1, 0.0f, 0.0f, 0.0f,
             -BR_CP_CORNER_X,  BR_CP_CORNER_Y, 0.0f);
    BrCpNode(&pCar->aListA[2], &pCar->aListA[3], 1, 0.0f, 0.0f, 0.0f,
              BR_CP_CORNER_X, -BR_CP_CORNER_Y, 0.0f);
    BrCpNode(&pCar->aListA[3], &pCar->aListA[4], 1, 0.0f, 0.0f, 0.0f,
              BR_CP_CORNER_X,  BR_CP_CORNER_Y, 0.0f);
    BrCpNode(&pCar->aListA[4], NULL, 0, 0.0f, 0.0f, BR_CP_GRAVITY_BODY,
             0.0f, 0.0f, 0.0f);

    /* --- force list B: the same corners then the drag node -------------- */
    BrCpNode(&pCar->aListB[0], &pCar->aListB[1], 1, 0.0f, 0.0f, 0.0f,
             -BR_CP_CORNER_X, -BR_CP_CORNER_Y, 0.0f);
    BrCpNode(&pCar->aListB[1], &pCar->aListB[2], 1, 0.0f, 0.0f, 0.0f,
             -BR_CP_CORNER_X,  BR_CP_CORNER_Y, 0.0f);
    BrCpNode(&pCar->aListB[2], &pCar->aListB[3], 1, 0.0f, 0.0f, 0.0f,
              BR_CP_CORNER_X, -BR_CP_CORNER_Y, 0.0f);
    BrCpNode(&pCar->aListB[3], &pCar->aListB[4], 1, 0.0f, 0.0f, 0.0f,
              BR_CP_CORNER_X,  BR_CP_CORNER_Y, 0.0f);
    BrCpNode(&pCar->aListB[4], NULL, 0, 0.0f, 0.0f, 0.0f,
             0.0f, 0.0f, 0.0f);

    /* --- the wheels' own lists: a tyre node then a shared weight node --- */
    BrCpNode(&pCar->aWheelW[0], NULL, 0, 0.0f, 0.0f, BR_CP_GRAVITY_WHEEL,
             0.0f, 0.0f, 0.0f);
    BrCpNode(&pCar->aWheelW[1], NULL, 0, 0.0f, 0.0f, BR_CP_GRAVITY_WHEEL,
             0.0f, 0.0f, 0.0f);
    BrCpNode(&pCar->aWheelF[0], &pCar->aWheelW[0], 1, 0.0f, 0.0f, 0.0f,
             0.0f, 0.0f, 0.0f);
    BrCpNode(&pCar->aWheelF[1], &pCar->aWheelW[0], 1, 0.0f, 0.0f, 0.0f,
             0.0f, 0.0f, 0.0f);
    BrCpNode(&pCar->aWheelF[2], &pCar->aWheelW[1], 1, 0.0f, 0.0f, 0.0f,
             0.0f, 0.0f, 0.0f);
    BrCpNode(&pCar->aWheelF[3], &pCar->aWheelW[1], 1, 0.0f, 0.0f, 0.0f,
             0.0f, 0.0f, 0.0f);

    for (i = 0; i < 4; ++i) {
        pCar->wheel[i].pForces = &pCar->aWheelF[i];
    }
    pCar->body.pForces = &pCar->aListA[0];

    /* 0x10062C5B: the tyre pass is suppressed on the very first frame. */
    pCar->fE84 = 1;

    pCar->save = *pState;
    pCar->next = *pState;

    /* 0x1005E7B7: the entrant's start-of-race pass calls 0x1006FD90 on the
     * freshly constructed car.  car+0x29C4 was set by 0x1006FCB0 just before
     * the constructor ran; here it falls back to the default record when
     * nothing set it.  See br_carphys.h's DEVIATION. */
    if (g_pBrCarPhysCarData == NULL) {
        g_pBrCarPhysCarData = BrCarDataDefault();
    }
    BrCarPhysApplyCarData(pCar, g_pBrCarPhysCarData);
}

/* ==================================================================== */
/* 0x1006FD90 -- the car-data apply                                      */
/* ==================================================================== */

const BrCarData *g_pBrCarPhysCarData;   /* car+0x29C4 */

void BrCarPhysApplyCarData(BrCarPhys *pCar, const BrCarData *pData)
{
    if (pData == NULL) {
        /* 0x1006FD90 dereferences car+0x29C4 unconditionally; the original
         * cannot reach here with it NULL because 0x1006FCB0 always sets it.
         * This port can, and a crash would be a worse answer than a measured
         * hole -- BrCollRespBoxDegenerate is what reports it. */
        return;
    }

    /* 0x1006FEB9..0x1006FEE3: four dwords, +0xC8..+0xD4 -> car+0x340..0x34C
     * == body+0x1DC..+0x1E8.  Plain 32-bit moves, no conversion. */
    pCar->f1DC = pData->boxX;
    pCar->f1E0 = pData->boxY;
    pCar->f1E4 = pData->boxZ;
    pCar->f1E8 = pData->boxOffZ;

    /* 0x1006FEF1: `mov [ebx+0x29c4], ebp` with ebp == 0 -- the record is
     * consumed once.  Modelled on the caller's pointer rather than on a
     * member, because BrCarPhys has no +0x29C4. */
    g_pBrCarPhysCarData = NULL;
}

void BrCarPhysPlace(BrCarPhys *pCar, const BrVec3 *pPos, float yaw)
{
    BrRbState *pState = BrCarPhysBodyState(&pCar->body);
    float      h      = yaw * 0.5f;
    int        i;

    pState->pos      = *pPos;
    pState->vel.x    = 0.0f;
    pState->vel.y    = 0.0f;
    pState->vel.z    = 0.0f;
    pState->angVel.x = 0.0f;
    pState->angVel.y = 0.0f;
    pState->angVel.z = 0.0f;
    /* scalar first, rotation about Z */
    pState->quat.f00 = (float)cos((double)h);
    pState->quat.f04 = 0.0f;
    pState->quat.f08 = 0.0f;
    pState->quat.f0C = (float)sin((double)h);
    pState->qDot.f00 = 0.0f;
    pState->qDot.f04 = 0.0f;
    pState->qDot.f08 = 0.0f;
    pState->qDot.f0C = 0.0f;

    BrRbBuildMatrix(&pCar->body.m, pState);
    pCar->save    = *pState;
    pCar->next    = *pState;
    pCar->lastPos = *pPos;

    for (i = 0; i < 4; ++i) {
        pCar->wheel[i].f1B4 = 0.0f;
    }

    /* PRIME f1D8 FROM THE ACTUAL GEOMETRY, and this is not optional.
     *
     * BrRbInitInertia leaves f1D8 == 0, and 0 does NOT mean "no ground" -- it
     * means "the wheel is exactly ON the ground", i.e. FULL suspension
     * compression.  A car dropped in with f1D8 == 0 therefore takes
     * 4 * 0.3^2 * k == 115200 N of spring on its first frame against 15450 N
     * of weight, and is fired upwards at 3.3 m/s before it has fallen at all.
     * That was observed, not guessed: the first frame's chassis acceleration
     * was +99.75 m/s^2.
     *
     * 0x1005A7A0 ends with BrWheelSuspensionSetZ for exactly this reason --
     * f1D8 is always LAST frame's probe.  A car that has never stepped has no
     * last frame, so placement runs the probe once.  It is the same ported
     * function the step's tail calls, not a second copy. */
    BrWheelSuspensionSetZHit(&pCar->body, pCar->aHit);
}
