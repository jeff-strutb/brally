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

/* ==================================================================== */
/* The holes                                                             */
/* ==================================================================== */

BrCarPhysHooks g_brCarPhysHooks;
uint32_t       g_aBrCarPhysHole[BR_CP_HOLE_COUNT];

static const char *const g_aBrCarPhysHoleName[BR_CP_HOLE_COUNT] = {
    "0x100651A0 tyre force  (1355 B)",
    "0x100645A0 drivetrain  (3070 B)",
    "0x10067C30 collision   (5 callees, 5196 B)"
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
    BrRbBodyFull *pBody = &pCar->body;
    float         t     = BR_PHYS_DT;
    float         m22;

    /* 0x10067CAB..0x10067CC3 fill three stack floats with 0.1f and hand them,
     * with the body matrix, to 0x1006DDD0 (== BrMat4BuildScaledTransposed,
     * slice3_44.c) and then to 0x10066AD0.  Both outputs are consumed only by
     * the collision callees, which are the BR_CP_HOLE_COLLIDE hook, so the
     * pre-pass is not run: computing it and throwing it away would be a
     * lookalike, not a port. */

    for (;;) {
        /* 0x10066D70 (a test whose non-zero result only drives a printf),
         * 0x10068F80, 0x10067710 (the collision response) -- all inside the
         * hole. */
        ++g_aBrCarPhysHole[BR_CP_HOLE_COLLIDE];
        if (g_brCarPhysHooks.pfnCollide != NULL) {
            g_brCarPhysHooks.pfnCollide(pCar);
        }

        /* 0x1006D850 == D3D 0x100745F0 == BrRbIntegrateState(dst, src, dt). */
        BrRbIntegrateState(&pCar->next, &pCar->save, BR_CP_SUBSTEP);

        /* 0x1006D6B0 == BrRbBuildMatrix(&body->m, state). */
        BrRbBuildMatrix(&pBody->m, &pCar->next);

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

    /* --- step 3: 0x100651A0 x4, gated on car+0xE84 ----------------------- */
    if (pCar->fE84 == 0) {
        /* The scalars are paired FRONT (fE7C, bE80) and REAR (fE74, fE78),
         * and only three of the four are pre-cleared -- fE78 is not.  That
         * asymmetry is the original's. */
        pCar->fE7C = 0.0f;
        pCar->fE74 = 0.0f;
        pCar->bE80 = 0u;

        ++g_aBrCarPhysHole[BR_CP_HOLE_TYRE];
        if (g_brCarPhysHooks.pfnTyre != NULL) {
            g_brCarPhysHooks.pfnTyre(pCar, pBody->child[0],
                                     &pCar->fE7C, &pCar->fE78, BR_PHYS_DT);
            g_brCarPhysHooks.pfnTyre(pCar, pBody->child[1],
                                     &pCar->fE7C, &pCar->fE78, BR_PHYS_DT);
            g_brCarPhysHooks.pfnTyre(pCar, pBody->child[2],
                                     &pCar->fE74, &pCar->fE78, BR_PHYS_DT);
            g_brCarPhysHooks.pfnTyre(pCar, pBody->child[3],
                                     &pCar->fE74, &pCar->fE78, BR_PHYS_DT);
        }
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

    /* --- step 7: 0x100645A0 --------------------------------------------- */
    ++g_aBrCarPhysHole[BR_CP_HOLE_DRIVE];
    if (g_brCarPhysHooks.pfnDrive != NULL) {
        g_brCarPhysHooks.pfnDrive(pCar, BR_PHYS_DT);
    }

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
