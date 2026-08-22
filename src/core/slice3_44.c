/* slice3_44.c -- decompiled from BRD3D.dll, packet 0x10071B80-0x10075F10.
 *
 * See slice3_44.h for the API, the layout notes and the gotchas, and the
 * pass report for the list of addresses that were deliberately skipped.
 *
 * Float constants used here, read out of orig/BRD3D.dll .rdata with
 * tools/pe.py rather than assumed:
 *
 *   0x1008FC48 = 0x3F800000 = 1.0f
 *   0x1008FC4C = 0x3F000000 = 0.5f
 *   0x1008FC54 = 0x3DAAAAAB = 1/12 (the correctly rounded float)
 *   in-line immediates: 0x3F800000 = 1.0f, 0x3F000000 = 0.5f,
 *                       0x3E322D0E = 0.174f
 */
#include "slice3_44.h"

/* ---- constants ---------------------------------------------------- */

/* 0x1008FC54.  1.0f/12.0f rounds to the same 0x3DAAAAAB, but the value is
 * spelled out so a future reader does not have to re-derive it. */
#define BR_K_ONE_TWELFTH  0.0833333358168602f
/* the 0x1C0 immediate 0x3E322D0E */
#define BR_K_1C0          0.174f

/* ---- cross-slice --------------------------------------------------- */

/* XSLICE 0x10008B80 */
/* A bare `ret` in this build (see CONTRACT).  Name and prototype copied from
 * slice2_18.h so integration can wire it mechanically. */
extern void BrStub8B80_1p(const void *p0);

/* XSLICE 0x10075330 */
/* Name copied from slice2_16.h. */
extern void BrGbiCall10075330(void *pv);

/* BrVec4Normalise (0x100741B0) and BrMat4MulVec3Transposed (0x10074770) come
 * in through slice1_09.h / br_mat.h. */

/* ================================================================== */
/* Packed 3x3                                                          */
/* ================================================================== */

/* 0x10074830 */
/* WHAT IT DOES: rotates a 3D vector by a 3x3 matrix. Used all through the
 * car physics, where the same quantity has to be moved between the world's
 * frame of reference and the car's own. */
/* @implements 0x10074830 d3d BrMat3MulVec3 */
void BrMat3MulVec3(BrVec3 *pOut, const BrMat3 *pM, const BrVec3 *pV)
{
    const float *m = pM->m;
    const float v[3] = { pV->x, pV->y, pV->z };
    float       o[3];
    int         i, k;

    for (i = 0; i < 3; ++i) {
        /* the original zeroes the destination slot and then reloads it once
         * per term, so every partial sum is rounded to float */
        o[i] = 0.0f;
        for (k = 0; k < 3; ++k)
            o[i] = m[3 * i + k] * v[k] + o[i];
    }
    pOut->x = o[0];
    pOut->y = o[1];
    pOut->z = o[2];
}

/* 0x10074AC0 */
/* WHAT IT DOES: combines two 3x3 rotations into one, so that applying the
 * result does the same as applying both in turn. */
/* @implements 0x10074AC0 d3d BrMat3Mul */
void BrMat3Mul(BrMat3 *pOut, const BrMat3 *pA, const BrMat3 *pB)
{
    const float *a = pA->m;
    const float *b = pB->m;
    int          i, j;

    for (i = 0; i < 3; ++i) {
        for (j = 0; j < 3; ++j) {
            /* x87 order: (a1*b1j + a0*b0j) + a2*b2j */
            pOut->m[3 * i + j] = (a[3 * i + 1] * b[3 + j]
                                  + a[3 * i + 0] * b[j])
                                 + a[3 * i + 2] * b[6 + j];
        }
    }
}

/* 0x100749D0 */
/* WHAT IT DOES: builds the little 3x3 matrix that stands in for a cross
 * product: multiplying a vector by it gives the same answer as crossing it
 * with the vector this was built from. The physics uses it to turn "a force
 * applied at this offset" into a twist. */
/* @implements 0x100749D0 d3d BrMat3Skew */
void BrMat3Skew(BrMat3 *pOut, const BrVec3 *pV)
{
    pOut->m[0] =  0.0f;
    pOut->m[1] = -pV->z;
    pOut->m[2] =  pV->y;
    pOut->m[3] =  pV->z;
    pOut->m[4] =  0.0f;
    pOut->m[5] = -pV->x;
    pOut->m[6] = -pV->y;
    pOut->m[7] =  pV->x;
    pOut->m[8] =  0.0f;
}

/* WHAT IT DOES: solves the 3x3 system pM * x = pV for x by Cramer's rule and
 * writes x to pOut.  No singularity guard -- a singular matrix yields +-inf or
 * NaN, exactly as the original.  Confirmed equivalent to the original bytes by
 * an x87 emulation of 0x1006DE70 over random and structured inputs. */
/* @implements 0x1006DE70 glide BrMat3Solve */
void BrMat3Solve(BrVec3 *pOut, const BrMat3 *pM, const BrVec3 *pV)
{
    const float *m  = pM->m;
    const float  v0 = pV->x, v1 = pV->y, v2 = pV->z;

    /* --- shared sub-expressions, in the order the original spills them --- */
    const float d0    = m[7] * m[5] - m[4] * m[8];   /* cofactor of m[0] */
    const float m1m8  = m[1] * m[8];
    const float m2m7  = m[2] * m[7];
    const float m1m5  = m[1] * m[5];
    const float m2m4  = m[2] * m[4];

    /* det is the NEGATED determinant -- the original expands with the signs
     * flipped and compensates by negating only pOut->y below. */
    const float det = (((m1m8 * m[3] + m[0] * d0)
                        - m2m7 * m[3])
                       - m1m5 * m[6])
                      + m2m4 * m[6];
    const float inv = 1.0f / det;

    const float n0 = (((v0 * d0 + m1m8 * v1)
                       - m2m7 * v1)
                      - m1m5 * v2)
                     + m2m4 * v2;

    const float g0   = m[6] * m[5] - m[3] * m[8];
    const float m0m8 = m[0] * m[8];
    const float m6m2 = m[6] * m[2];
    const float m0m5 = m[0] * m[5];
    const float m3m2 = m[3] * m[2];
    const float n1 = (((g0 * v0 + m0m8 * v1)
                       - m6m2 * v1)
                      - m0m5 * v2)
                     + m3m2 * v2;

    const float k0   = m[6] * m[4] - m[3] * m[7];
    const float m0m7 = m[0] * m[7];
    const float m6m1 = m[6] * m[1];
    const float m0m4 = m[0] * m[4];
    const float m3m1 = m[3] * m[1];
    const float n2 = (((k0 * v0 + m0m7 * v1)
                       - m6m1 * v1)
                      - m0m4 * v2)
                     + m3m1 * v2;

    pOut->x =  n0 * inv;
    pOut->y = -(n1 * inv);   /* the sign flip that pairs with the negated det */
    pOut->z =  n2 * inv;
}

/* 0x10074A90 */
/* WHAT IT DOES: pulls the rotation part out of a full 4x4 transform,
 * dropping the translation and leaving a compact 3x3. */
/* @implements 0x10074A90 d3d BrMat4ToMat3 */
void BrMat4ToMat3(BrMat3 *pOut, const BrMat4 *pSrc)
{
    int i, j;

    for (i = 0; i < 3; ++i)
        for (j = 0; j < 3; ++j)
            pOut->m[3 * i + j] = pSrc->m[i][j];
}

/* 0x10074A50 */
/* WHAT IT DOES: the same extraction but flipped along the diagonal, which
 * for a pure rotation is the same as reversing it -- so this gives the
 * matrix that undoes the transform's rotation. */
/* @implements 0x10074A50 d3d BrMat4ToMat3Transposed */
void BrMat4ToMat3Transposed(BrMat3 *pOut, const BrMat4 *pSrc)
{
    int i, j;

    for (i = 0; i < 3; ++i)
        for (j = 0; j < 3; ++j)
            pOut->m[i + 3 * j] = pSrc->m[i][j];
}

/* 0x10074A10 */
/* WHAT IT DOES: extracts the rotation part of a 4x4 transform twice at once,
 * once flipped and once straight, because the physics needs both to move
 * quantities into the body's frame and back out again. */
/* @implements 0x10074A10 d3d BrMat4ToMat3Both */
void BrMat4ToMat3Both(BrMat3 *pTransposed, BrMat3 *pStraight,
                      const BrMat4 *pSrc)
{
    int i, j;

    for (i = 0; i < 3; ++i) {
        for (j = 0; j < 3; ++j) {
            /* store order is transposed-first, straight-second */
            pTransposed->m[i + 3 * j] = pSrc->m[i][j];
            pStraight->m[3 * i + j]   = pSrc->m[i][j];
        }
    }
}

/* 0x10074B20 */
/* WHAT IT DOES: subtracts one 3x3 matrix from another, all nine elements. An
 * earlier reading of this had it covering only three, described as a
 * faithfully preserved bug of the original; that was a misreading, and the
 * note in the code explains how it was caught. */
/* @implements 0x10074B20 d3d BrMat3Sub */
void BrMat3Sub(float *pOut, const float *pA, const float *pB)
{
    int i, j;

    /* NINE elements: a 3x3 matrix subtract, nested 3x3 so the cursor
     * walks continuously (the outer body reloads the pA/pOut offsets
     * but does not reset the pB walker).
     *
     * This was previously modelled as three subtractions performed three
     * times, and recorded as a PRESERVED BUG of the original -- "only the
     * inner loop advances any pointer, and the outer one resets it".
     *
     * The outer loop does NOT reset it. The reset `mov eax, edi` sits at
     * 0x1006DD98 and the outer loop's target is 0x1006DD9A -- one instruction
     * LATER. So the cursor advances continuously across all three passes and
     * the function covers nine floats, exactly once each.
     *
     * 0x10065C80 settles it independently: it passes a 3x3 identity scaled by
     * 1/mass, which is meaningless to a routine that only touches three
     * elements.
     *
     * Worth naming the shape of the old error, because it is the kind that
     * survives review: a misreading dressed as a faithfully preserved bug.
     * It looks like diligence, it explains an oddity, and nobody re-derives
     * it. A claim that the original is wrong should be held to a HIGHER
     * standard than a claim that it is right, not a lower one. */
    for (i = 0; i < 3; ++i)
        for (j = 0; j < 3; ++j)
            pOut[3 * i + j] = pA[3 * i + j] - pB[3 * i + j];
}

/* 0x10075340 */
/* WHAT IT DOES: resets the last column of a 4x4 transform to the plain "no
 * perspective" values, undoing anything that had been left there. */
/* @implements 0x10075340 d3d BrMat4SetLastColumn */
void BR_THISCALL1 BrMat4SetLastColumn(BrMat4 *pM)
{
    pM->m[3][3] = 1.0f;
    pM->m[2][3] = 0.0f;
    pM->m[1][3] = 0.0f;
    pM->m[0][3] = 0.0f;
}

/* 0x10074B70 */
void BrMat4BuildScaledTransposed(const BrMat4 *pA, BrMat4 *pOut,
                                 const BrMat4 *pS)
{
    BrVec3 t, r;
    int    i, j;

    for (i = 0; i < 3; ++i) {
        for (j = 0; j < 3; ++j)
            pOut->m[i][j] = pA->m[j][i] * pS->m[0][j];
        pOut->m[i][3] = 0.0f;
    }
    pOut->m[3][3] = 1.0f;

    /* ARG1, NOT ARG3 -- and the two are one byte apart in the listing.
     *
     * Glide 0x1006DDD0 loads the translation at 0x1006DE2C with
     * `mov ecx,[esp+0x24]`.  An earlier load at 0x1006DDD9 uses the SAME
     * displacement, `mov ebp,[esp+0x24]`, and that one IS pS.  They differ
     * because `push esi` / `push edi` sit between them:
     *
     *     entry            esp = R      (arg1 R+4, arg2 R+8, arg3 R+0xC)
     *     sub esp,0x10     esp = R-0x10
     *     push ebx         esp = R-0x14   [esp+0x18] = R+4  = pA
     *     push ebp         esp = R-0x18   [esp+0x24] = R+0xC = pS   <-- arg3
     *     push esi/edi     esp = R-0x20   [esp+0x24] = R+4  = pA    <-- arg1
     *
     * This file read pS here, and br_collresp.h then explained the resulting
     * garbage as a deliberate "frame overlap" -- a misreading dressed as a
     * preserved bug, which CONVENTIONS.md calls the durable kind, and the
     * test below asserted it.  The falsifier is semantic, not just textual:
     * pS is the scale VECTOR, whose m[3][*] is off the end of the three
     * floats that exist, so under the old reading the box matrix had no
     * translation at all and the OBB sat at the world origin.  A car at
     * (927, 361) could not have collided with anything on any track. */
    t.x = -pA->m[3][0];
    t.y = -pA->m[3][1];
    t.z = -pA->m[3][2];

    /* DEVIATION: the original passes &pOut->m[3][0] straight to 0x10074770 as
     * the output vector.  Routing it through a local avoids casting a float[4]
     * row to BrVec3*; 0x10074770 reads only rows 0..2, so this is
     * observationally identical. */
    BrMat4MulVec3Transposed(&r, pOut, &t);
    pOut->m[3][0] = r.x;
    pOut->m[3][1] = r.y;
    pOut->m[3][2] = r.z;
}

/* ================================================================== */
/* Rigid-body integrator                                               */
/* ================================================================== */

/* 0x100742D0 */
/* WHAT IT DOES: works out how fast a body's orientation is changing, given
 * how fast it is spinning. The result is what the integrator adds to the
 * orientation each step to make the body actually turn. */
/* @implements 0x100742D0 d3d BrRbQuatDerivative */
void BrRbQuatDerivative(BrRbState *pS)
{
    /* SPILL MAP, traced instruction by instruction.  The scale is 0x1008FC4C
     * == 3F000000 == 0.5f.  The three halved components go to stack slots
     * [esp], [esp+4], [esp+8] -- but NOT the same way, and that is the whole
     * subtlety of this function:
     *
     *   100742EE  fstp dword [esp]     hx: stored AND POPPED, so the only
     *                                  copy left anywhere is the rounded one.
     *   10074306  fst  dword [esp+4]   hy: stored and KEPT.  st0 still holds
     *   10074314  fst  dword [esp+8]   hz: the unrounded 53-bit product.
     *
     * `fst` is not `fstp`.  Each of hy and hz therefore has TWO live values
     * after its store -- the rounded one in memory and the unrounded one in
     * the register -- and the original uses the register copy exactly ONCE
     * before it is consumed, then reloads the rounded slot for everything
     * else:
     *
     *   10074323  fmul [eax+0x20]   the kept hy, times quat.y  -> qDot.f00
     *   1007433E  fmul [eax+0x1c]   the kept hz, times quat.x  -> qDot.f08
     *
     * Every other use comes from `fld [esp+N]` and is rounded.  So of the
     * twelve products below, ten take a float-rounded half-rate and two do
     * not.  Writing all three as `float` (which this did) rounds two products
     * the original leaves alone; writing all three as `double` would round
     * ten it does not.  Neither is right, and the asymmetry is not visible
     * from the C -- only from the fst/fstp distinction in the bytes.
     *
     * The four results are `fstp dword` at 1007438A..10074397, so each
     * expression rounds to float exactly once, at the store. */
    const double hxU = (double)pS->angVel.x * 0.5;
    const float  hx  = (float)hxU;              /* 100742EE fstp -- popped */

    const double hyU = (double)pS->angVel.y * 0.5;
    const float  hy  = (float)hyU;              /* 10074306 fst  -- kept   */

    const double hzU = (double)pS->angVel.z * 0.5;
    const float  hz  = (float)hzU;              /* 10074314 fst  -- kept   */

    const double w = (double)pS->quat.f00;
    const double x = (double)pS->quat.f04;
    const double y = (double)pS->quat.f08;
    const double z = (double)pS->quat.f0C;

    /* qDot = 0.5 * (0, wx, wy, wz) (x) q, scalar first.  hyU and hzU appear
     * once each, in the two positions named above; every other term uses the
     * rounded hx/hy/hz. */
    pS->qDot.f00 = (float)(((-((double)hx * x)) - hyU * y) - (double)hz * z);
    pS->qDot.f04 = (float)(((double)hy * z + (double)hx * w) - (double)hz * y);
    pS->qDot.f08 = (float)((hzU * x + (double)hy * w) - (double)hx * z);
    pS->qDot.f0C = (float)(((double)hx * y + (double)hz * w) - (double)hy * x);
}

/* 0x100743A0 */
/* WHAT IT DOES: adds one time step's worth of acceleration to a body's speed
 * and spin -- the first half of the physics step, before anything has
 * actually moved. */
/* @d3donly 0x100743A0 BrRbIntegrateVelocity -- glide twin 0x1006D600 claimed by br_carphys.c:BrCpIntegrateVelocity */
void BrRbIntegrateVelocity(BrRbState *pS, const BrRbBody *pBody, float dt)
{
    /* SPILL MAP.  Six products, and exactly ONE of them is rounded:
     *
     *   100743EB  fstp dword [esp+8]   accel[2]*dt -- stored AND POPPED, then
     *   100743F5  fld  dword [esp+8]   reloaded.  This one really is float.
     *
     *   100743FB  fst  dword [esp]     angAccel[0]*dt \  stored and KEPT, and
     *   10074405  fst  dword [esp+4]   angAccel[1]*dt  > the three slots are
     *   1007440B  fst  dword [esp+8]   angAccel[2]*dt /  NEVER RELOADED.
     *
     * Nothing between 1007440F and the epilogue reads [esp], [esp+4] or
     * [esp+8].  Those three stores are dead -- register-allocator spill slots
     * the compiler wrote and did not need -- so the adds at 1007441B,
     * 10074420 and 10074425 all consume the unrounded register copies.  A
     * spill that is never reloaded rounds nothing, and treating `fst` as
     * evidence of rounding without checking for the matching `fld` would have
     * put a float here on the strength of an instruction with no effect.
     *
     * accel[0]*dt and accel[1]*dt are never stored at all.
     *
     * All six results are `fstp dword` at 1007442F..10074442, so each sum
     * rounds to float once, at the store. */
    const double d = (double)dt;

    /* 0x0C <- 0xFC   product never spilled */
    pS->vel.x    = (float)((double)pBody->accel[0] * d + (double)pS->vel.x);
    /* 0x10 <- 0x100  product never spilled */
    pS->vel.y    = (float)((double)pBody->accel[1] * d + (double)pS->vel.y);
    /* 0x14 <- 0x104  product SPILLED and reloaded: rounded before the add */
    pS->vel.z    = (float)((double)(float)((double)pBody->accel[2] * d)
                           + (double)pS->vel.z);
    /* 0x28 <- 0x108  spill slot written but never read */
    pS->angVel.x = (float)((double)pBody->angAccel[0] * d
                           + (double)pS->angVel.x);
    /* 0x2C <- 0x10C  likewise */
    pS->angVel.y = (float)((double)pBody->angAccel[1] * d
                           + (double)pS->angVel.y);
    /* 0x30 <- 0x110  likewise */
    pS->angVel.z = (float)((double)pBody->angAccel[2] * d
                           + (double)pS->angVel.z);
}

/* 0x100745F0 */
/* WHAT IT DOES: advances a body one time step: moves it by its speed, turns
 * it by the rate its orientation is changing, and renormalises the
 * orientation afterwards so accumulated rounding does not slowly distort the
 * body. Speed and spin are carried across unchanged, since the previous step
 * already updated them. */
/* @implements 0x100745F0 d3d BrRbIntegrateState */
void BrRbIntegrateState(BrRbState *pDst, const BrRbState *pSrc, float dt)
{
    /* SPILL MAP.  Seven products; three are spilled and reloaded, four are
     * not, and they alternate in a way no rule of thumb would predict:
     *
     *   10074623  fstp [esp+0x10]  dt*vel.z    reloaded 10074637  ROUNDED
     *   10074670  fstp [esp+0x0C]  dt*qDot[1]  reloaded 10074681  ROUNDED
     *   10074689  fstp [esp+0x14]  dt*qDot[3]  reloaded 100746A0  ROUNDED
     *
     * dt*vel.x, dt*vel.y, dt*qDot[0] and dt*qDot[2] are never stored: they go
     * straight into their `fadd` from the register (10074611, 1007461F,
     * 10074666, 10074695).  So pos.x and pos.y round once and pos.z rounds
     * twice; quat.f00 and quat.f08 round once and quat.f04 and quat.f0C round
     * twice.  There is no pattern to carry over -- each one was read off its
     * own instruction.
     *
     * (The reload at 100746A0 reads [esp+0x18], not [esp+0x14], because the
     * `push eax` at 10074697 moved esp by four between the store and the
     * load.  Same slot, different displacement -- the trap CONVENTIONS.md
     * records under the wheel-probe entry.)
     *
     * Every destination is `fstp dword`, so each expression rounds to float
     * at its store.  vel, angVel and qDot are copied as raw dwords by
     * integer moves and carry no arithmetic at all. */
    const double d = (double)dt;

    /* 10074611: fadd from the register */
    pDst->pos.x = (float)((double)pSrc->pos.x + d * (double)pSrc->vel.x);
    /* 1007461F: fadd st(1), also from the register */
    pDst->pos.y = (float)((double)pSrc->pos.y + d * (double)pSrc->vel.y);
    /* 10074623/10074637: spilled, reloaded, so the product is float first */
    pDst->pos.z = (float)((double)pSrc->pos.z
                          + (double)(float)(d * (double)pSrc->vel.z));

    pDst->vel = pSrc->vel;

    /* 10074666: from the register */
    pDst->quat.f00 = (float)((double)pSrc->quat.f00
                             + d * (double)pSrc->qDot.f00);
    /* 10074670/10074681: spilled and reloaded */
    pDst->quat.f04 = (float)((double)pSrc->quat.f04
                             + (double)(float)(d * (double)pSrc->qDot.f04));
    /* 10074695: from the register */
    pDst->quat.f08 = (float)((double)pSrc->quat.f08
                             + d * (double)pSrc->qDot.f08);
    /* 10074689/100746A0: spilled and reloaded */
    pDst->quat.f0C = (float)((double)pSrc->quat.f0C
                             + (double)(float)(d * (double)pSrc->qDot.f0C));

    BrVec4Normalise(&pDst->quat);

    pDst->angVel = pSrc->angVel;
    pDst->qDot   = pSrc->qDot;
}

/* 0x10074450 */
/* WHAT IT DOES: builds the transform matrix that places a body in the world,
 * from its orientation and position -- the matrix the renderer needs to draw
 * the car where the physics says it is. */
/* @implements 0x10074450 d3d BrRbBuildMatrix */
void BrRbBuildMatrix(BrMat4 *pM, const BrRbState *pS)
{
    const double a = (double)pS->quat.f00;   /* w */
    const double b = (double)pS->quat.f04;   /* x */
    const double c = (double)pS->quat.f08;   /* y */
    const double d = (double)pS->quat.f0C;   /* z */

    /* SPILL MAP.  The old comment here said "every one of these is spilled to
     * a 4-byte slot by the original", and that is true of the five squares
     * but NOT of the six doubled cross terms, three of which are stored with
     * `fst` and keep their unrounded register copy for one further use.
     *
     * Spilled with `fstp` -- popped, so every later use is the rounded slot:
     *   1007449C [esp+4]     aa        100744AA [esp]       bb
     *   100744CE [esp+0x24]  cc        100744EB [esp+0x18]  ab
     *   10074565 [esp]       dd
     *   10074503 [esp+4]     t2cb      1007452D [esp+8]     t2ca
     *   10074537 [esp+0x10]  t2dc
     *
     * Stored with `fst` -- kept, one unrounded use each:
     *   10074521 [esp]       t2da  -> used unrounded at 10074547 (m[0][1])
     *   10074527 [esp+0x0C]  t2db  -> used unrounded at 1007454D (m[0][2])
     *   10074541 [esp+0x14]  t2ba  -> used unrounded at 10074559 (m[1][2])
     *
     * So each doubled term appears in exactly two matrix entries, once
     * unrounded and once rounded, and which of the two is which is fixed by
     * the instruction order.  The doubling itself (`fadd st(0),st(0)`) is
     * applied to the unrounded product, so the value that reaches the `fst`
     * is 2*(the 53-bit product) and the float slot is its single rounding. */
    const double aaU = a * a;
    const float  aa  = (float)aaU;   /* 1007449C fstp */
    const double bbU = b * b;
    const float  bb  = (float)bbU;   /* 100744AA fstp */
    const double ccU = c * c;
    const float  cc  = (float)ccU;   /* 100744CE fstp */
    const double ddU = d * d;
    const float  dd  = (float)ddU;   /* 10074565 fstp */
    /* 100744C3 fsub: BOTH operands come from the rounded slots */
    const float  ab  = (float)((double)aa - (double)bb);   /* 100744EB fstp */

    /* the doubled cross terms, unrounded, and their single float rounding */
    const double t2daU = (d * a) + (d * a);
    const float  t2da  = (float)t2daU;   /* 10074521 fst -- KEPT   */
    const double t2cbU = (c * b) + (c * b);
    const float  t2cb  = (float)t2cbU;   /* 10074503 fstp -- popped */
    const double t2dbU = (d * b) + (d * b);
    const float  t2db  = (float)t2dbU;   /* 10074527 fst -- KEPT   */
    const double t2caU = (c * a) + (c * a);
    const float  t2ca  = (float)t2caU;   /* 1007452D fstp -- popped */
    const double t2dcU = (d * c) + (d * c);
    const float  t2dc  = (float)t2dcU;   /* 10074537 fstp -- popped */
    const double t2baU = (b * a) + (b * a);
    const float  t2ba  = (float)t2baU;   /* 10074541 fst -- KEPT   */

    /* 100744E1 / 1007456F: (bb + aa) from the two rounded slots, then - cc */
    pM->m[0][0] = (float)((((double)bb + (double)aa) - (double)cc)
                          - (double)dd);
    pM->m[0][1] = (float)(t2daU + (double)t2cb);   /* 10074547 */
    pM->m[0][2] = (float)(t2dbU - (double)t2ca);   /* 1007454D */
    pM->m[0][3] = 0.0f;

    pM->m[1][0] = (float)((double)t2cb - (double)t2da);   /* 10074553 */
    pM->m[1][1] = (float)(((double)cc + (double)ab) - (double)dd);
    pM->m[1][2] = (float)(t2baU + (double)t2dc);   /* 10074559 */
    pM->m[1][3] = 0.0f;

    pM->m[2][0] = (float)((double)t2ca + (double)t2db);   /* 1007455F */
    pM->m[2][1] = (float)((double)t2dc - (double)t2ba);   /* 1007457B */
    /* 100745B2..100745BA, after the stores: (ab - cc) + dd, all rounded */
    pM->m[2][2] = (float)(((double)ab - (double)cc) + (double)dd);
    pM->m[2][3] = 0.0f;

    pM->m[3][0] = pS->pos.x;
    pM->m[3][1] = pS->pos.y;
    pM->m[3][2] = pS->pos.z;
    pM->m[3][3] = 1.0f;

    BrStub8B80_1p(pM);
}

/* 0x10074870 */
/* WHAT IT DOES: sets a body up for physics: clears its accumulated forces,
 * plants a few fixed constants, and computes how hard it is to spin about
 * each axis. For the two box-shaped modes that comes from the standard
 * solid-box formula using the body's size and mass; other modes are left
 * with the placeholder value of one, which is worth knowing because it means
 * an unrecognised mode gets unit resistance rather than an error. */
/* @implements 0x10074870 d3d BrRbInitInertia */
void BrRbInitInertia(BrRbBody *pB)
{
    int i, j;

    pB->f00 = 0.0f;
    pB->f04 = 0.0f;
    pB->f08 = 0.0f;
    pB->f0C = 0.0f;
    pB->f10 = 0.0f;
    pB->f14 = 0.0f;

    pB->f1B4 = 0.0f;
    pB->f19C = 0.0f;
    pB->f1C4 = 0.0f;
    pB->f1C0 = BR_K_1C0;
    pB->f1CC = 0.0f;
    pB->f1D0 = 0.0f;
    pB->f1C8 = 0.5f;

    for (i = 0; i < 3; ++i)
        for (j = 0; j < 3; ++j)
            pB->inertia.m[3 * i + j] = (i == j) ? 1.0f : 0.0f;

    if (pB->mode >= 0 && pB->mode <= 1) {
        /* SPILL MAP.  Three squares, and only TWO of them are rounded:
         *
         *   1007492D  fstp [esp+0x18]  z*z  reloaded 10074938 / 10074940
         *   10074946  fstp [esp+0x18]  y*y  reloaded 1007494C / 10074952
         *
         * x*x is never stored -- it is produced at 10074927, duplicated with
         * `fld st(2)` at 1007493E and consumed by two adds from the register.
         * The two stores reuse the SAME slot, which is why z*z has to be
         * reloaded (10074938) before y*y overwrites it.
         *
         * So the diagonal is not symmetric in its rounding: m[0] takes two
         * rounded squares, and m[4] and m[8] each take one rounded and one
         * unrounded. The multiply by mass (0x2C) and by 1/12 (0x1008FC54 ==
         * 3DAAAAAB) stay in registers, so each entry rounds to float exactly
         * once, at its `fstp` (1007497F / 10074982 / 10074985). */
        const double x  = (double)pB->dim[0];
        const double y  = (double)pB->dim[1];
        const double z  = (double)pB->dim[2];
        const double xx = x * x;                   /* never stored */
        const float  yy = (float)(y * y);          /* 10074946 fstp */
        const float  zz = (float)(z * z);          /* 1007492D fstp */
        const double m  = (double)pB->mass;
        const double k  = (double)BR_K_ONE_TWELFTH;

        pB->inertia.m[0] = (float)((((double)zz + (double)yy) * m) * k);
        pB->inertia.m[4] = (float)(((xx + (double)zz) * m) * k);
        pB->inertia.m[8] = (float)(((xx + (double)yy) * m) * k);

        BrGbiCall10075330(&pB->inertia);
    }

    if (pB->mode != 2) {
        /* only the diagonal -- see the header's gotcha.
         *
         * `fld [0x1008FC48] (== 1.0f); fdiv dword ptr; fstp dword ptr` at
         * 10074996..100749B7.  The DIVIDE happens at the register's 53-bit
         * precision and only the result is narrowed, so this is written as a
         * double divide narrowed once -- which is what the instructions do.
         *
         * It is written that way for faithfulness, NOT because it changes an
         * answer: for this particular divide the double rounding is benign.
         * Checked exhaustively rather than argued -- over all 8,388,608 float
         * significands in [1,2), `(float)(1.0/(double)x)` and `1.0f/x` agree
         * on every one, and scaling x by a power of two scales both results
         * exactly, so that settles the whole normal range.  Reverting this
         * line to `1.0f / m[i]` is therefore an equivalent mutation and no
         * test can catch it; that is a property of the arithmetic, not a gap
         * in the suite. */
        pB->invInertia.m[0] = (float)(1.0 / (double)pB->inertia.m[0]);
        pB->invInertia.m[4] = (float)(1.0 / (double)pB->inertia.m[4]);
        pB->invInertia.m[8] = (float)(1.0 / (double)pB->inertia.m[8]);
    }

    pB->f1D4 = 0.0f;
    pB->f1D8 = 0.0f;
}

/* ================================================================== */
/* Misc                                                                */
/* ================================================================== */

/* 0x100746E0 */
/* WHAT IT DOES: copies seven values into a block, with a deliberate shuffle:
 * the last argument lands in the first written slot and the rest shift down
 * behind it, and the block's very first slot is left untouched. What the
 * block is for is not established. */
/* @implements 0x100746E0 d3d BrX100746E0 */
void BrX100746E0(unsigned int *pDst,
                 unsigned int a2, unsigned int a3, unsigned int a4,
                 unsigned int a5, unsigned int a6, unsigned int a7,
                 unsigned int a8)
{
    pDst[1] = a8;   /* the LAST argument, into the FIRST written slot */
    pDst[2] = a2;
    pDst[3] = a3;
    pDst[4] = a4;
    pDst[5] = a5;
    pDst[6] = a6;
    pDst[7] = a7;
    /* pDst[0] is deliberately never written */
}

/* 0x11829850 */
unsigned int g_BrX1829850[8];

/* 0x10074E00 */
/* WHAT IT DOES: clears an eight-word block of state back to zero. What that
 * block holds is not established. */
/* @implements 0x10074E00 d3d BrSub10074E00 */
void BrSub10074E00(void)
{
    int i;

    for (i = 0; i < 8; ++i)
        g_BrX1829850[i] = 0u;
}

/* 0x10074E20 */
/* WHAT IT DOES: copies that same eight-word block out to the caller -- a
 * snapshot of it, whatever it holds. */
/* @implements 0x10074E20 d3d BrSub10074E20 */
void BrSub10074E20(unsigned int *pDst)
{
    int i;

    for (i = 0; i < 8; ++i)
        pDst[i] = g_BrX1829850[i];
}
