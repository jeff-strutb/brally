/* br_collresp.c -- the collision half of 0x10067C30.
 *
 * Transcribed from orig/BRGlide.dll:
 *
 *   0x10066D70  1782 B   BrCollRespTipKick   the 1-or-2-wheel pitch kick
 *   0x1006DDD0   156 B   the overlapped call BrCollRespBuildBoxMatrix wraps
 *
 * See br_collresp.h for the contracts, for what 0x10067C30 really calls, and
 * for why the OBB half of the system is inert without the car data.
 *
 * ON THE COMPARISONS.  Same rule as br_phys.c and br_carphys.c: every test
 * below is the exact negation the x87 flag test implies.
 *     test ah,0x41 (C0|C3) taken -> "less, equal or unordered"
 *     test ah,1    (C0)    taken -> "less or unordered"
 *     test ah,0x40 (C3)    taken -> "equal or unordered"
 */
#include <math.h>
#include <string.h>

#include "br_collresp.h"
#include "br_phys.h"      /* BrGroundHit                                  */
#include "slice1_08.h"    /* BrPlaneEval == 0x10065950 (D3D 0x1006C9A0)   */

uint32_t g_cBrCollRespTipKick;
uint32_t g_cBrCollRespDegenerate;

void BrCollRespCountersReset(void)
{
    g_cBrCollRespTipKick    = 0u;
    g_cBrCollRespDegenerate = 0u;
}

/* ==================================================================== */
/* The three-way sign classifier, again                                  */
/*                                                                       */
/* 0x10066DB9..0x10066DEE and the three identical blocks after it.  This  */
/* is the same idiom br_carphys.c exposes as BrCarPhysSign, at a          */
/* different address; it is written out here rather than reached for      */
/* across a module boundary, because the two are only equal by            */
/* coincidence of the compiler emitting the same sequence twice.          */
/*    `fcom 0` + `test ah,0x40` + `je` -> 0.0f for EQUAL OR UNORDERED     */
/*    `fcom 0` + `test ah,0x41` + `jne` -> -1.0f for LESS OR UNORDERED    */
/* A NaN therefore lands on 0.0f at the first test and never reaches the  */
/* second.                                                               */
/* ==================================================================== */
static float BrCrSign(float v)
{
    if (v == 0.0f || !(v == v)) {
        return 0.0f;
    }
    if (v > 0.0f) {
        return 1.0f;
    }
    return -1.0f;
}

/* ==================================================================== */
/* The overlapped frame                                                  */
/* ==================================================================== */

BrMat4 *BrCollRespFrameMat(BrCollRespFrame *pF)
{
    /* 0x10067C9A/0x10067C9E: `lea ecx,[esp+0x10]` and `lea edx,[esp+0x1c]`
     * are 0xC apart, so the matrix begins three floats into the frame. */
    return (BrMat4 *)(void *)&pF->a[3];
}

void BrCollRespBuildBoxMatrix(BrCollRespFrame *pF, const BrMat4 *pBody,
                              float sx, float sy, float sz)
{
    BrMat4 *pOut = BrCollRespFrameMat(pF);

    pF->a[0] = sx;
    pF->a[1] = sy;
    pF->a[2] = sz;

    /* 0x1006DDD0's arguments are (pA, pOut, pS) -- DESTINATION SECOND, which
     * slice3_44.h already flags -- and 0x10067C30 passes pOut and pS as two
     * views of one overlapping frame.  The overlap is real and is why this
     * wrapper exists: 0x1006DDD0 writes pOut->m[0..2][*] and pOut->m[3][3]
     * FIRST and only then reads pS->m[3][0..2] for the translation, and with
     * this frame those three reads land on pOut->m[2][1], m[2][2] and
     * m[2][3] -- the values it has just written.  Calling it through
     * slice3_44's declaration with two disjoint objects would quietly change
     * the answer. */
    BrMat4BuildScaledTransposed(pBody, pOut, (const BrMat4 *)(const void *)pF);
}

int BrCollRespBoxDegenerate(float f1DC, float f1E0, float f1E4)
{
    /* 0x10067CEF/0x10067D10/0x10067D20: `fld [ebx+0x1dc]` then
     * `fdivr [0x10077A7C]`, i.e. 1.0f / f1DC.  A zero extent gives an
     * infinite scale and BrMat4BuildScaledTransposed then produces a matrix
     * of infinities and NaNs. */
    float rx = BR_CR_ONE / f1DC;
    float ry = BR_CR_ONE / f1E0;
    float rz = BR_CR_ONE / f1E4;

    return !(isfinite((double)rx) && isfinite((double)ry)
             && isfinite((double)rz));
}

/* ==================================================================== */
/* 0x10066D70 -- the 1-or-2-wheel pitch kick                             */
/* ==================================================================== */

int BrCollRespTipKick(BrRbBodyFull *pBody, const BrGroundHit aHit[4],
                      const BrVec3 *pBodyPlaneN, BrRbState *pSave,
                      float f1DC, float f1E0, float f1E4, float f1E8)
{
    BrVec3 p, world, v, vec, out;
    BrVec3 n;
    float  best  = BR_CR_TIP_FAR;      /* 0x10066DA0, the immediate 0x42C80000 */
    int    count = 0;                  /* 0x10066D98                          */
    int    iLast = -1;
    float  t, vn, s;
    int    i;

    /* 0x10066D8D: the matrix is rebuilt from `save` before anything else, so
     * every corner below is placed with the state the substep starts from. */
    BrRbBuildMatrix(&pBody->m, pSave);

    for (i = 0; i < 4; ++i) {
        BrRbBodyFull *pWheel = pBody->child[i];

        /* 0x10066DA8 / 0x10066EF3 / 0x10067039 / (the fourth) -- f1B4 is the
         * contact counter and is read as an INT.  A wheel with no count is
         * skipped entirely; it does not even move `best`. */
        if ((int32_t)pWheel->f1B4 == 0) {
            continue;
        }
        ++count;                       /* 0x10066DBF / 0x10066F07 / ...      */
        iLast = i;

        /* 0x10066DF4..0x10066FBF: the box corner on this wheel's side.  The
         * multiply order is the original's -- (extent * 0.5) and only then
         * by the sign -- and the Z term is `f1E8 - f1E4 * 0.5`, formed by
         * `fld f1E8`, `fld f1E4`, `fmul 0.5`, `fsubp st(1)`. */
        p.x = (f1DC * BR_CR_HALF) * BrCrSign(pWheel->f78.x);
        p.y = (f1E0 * BR_CR_HALF) * BrCrSign(pWheel->f78.y);
        p.z = f1E8 - f1E4 * BR_CR_HALF;

        /* 0x1006DA20 == BrMat4TransformPoint (slice1_09.h, D3D 0x100747C0):
         * body -> world, translation included. */
        BrMat4TransformPoint(&world, &pBody->m, &p);

        n.x = aHit[i].nx;
        n.y = aHit[i].ny;
        n.z = aHit[i].nz;

        /* 0x10066E90 / 0x10066EB2 / 0x10066ECB: BrPlaneEval is called TWICE
         * on the reject arm -- once to test the sign and once to produce the
         * value, which is then negated.  Written as an x87 absolute value:
         * `fcomp 0` + `test ah,1` + `je` takes the negating arm for LESS OR
         * UNORDERED, so a NaN comes back NEGATED, not absolute. */
        t = BrPlaneEval(&n, aHit[i].d, &world);
        if (!(t >= 0.0f)) {
            t = -BrPlaneEval(&n, aHit[i].d, &world);
        } else {
            t = BrPlaneEval(&n, aHit[i].d, &world);
        }

        /* 0x10066ED9: `fld best` + `fcomp st(1)` + `test ah,1` + `jne` SKIPS
         * the store when best < t, so the store runs for greater-equal or
         * unordered.  A NaN best is therefore replaced. */
        if (!(best < t)) {
            best = t;
        }
    }

    /* 0x100672C6 / 0x100672CF: strictly one or two wheels.  Signed integer
     * compares, so this really is a range test and not a mask. */
    if (count > 2 || count < 1) {
        return 0;
    }

    /* 0x100672F0: `fcomp qword ptr [0x10077B78]`, a DOUBLE, then
     * `test ah,0x41` + `je <ret 0>` -- the function LEAVES when both flags
     * are clear, i.e. when best is strictly greater.  Written negated so a
     * NaN best stays. */
    if ((double)best > BR_CR_TIP_NEAR) {
        return 0;
    }

    /* 0x1006730F == 0x100644C0 == BrRbVelAtPoint (slice3_42.h, D3D
     * 0x1006B510): the velocity of `p` in the BODY frame.  Note that `p` is
     * the LAST contacting wheel's corner -- the loop above leaves the slot
     * holding whichever wheel wrote it last, and so does the original. */
    v.x = 0.0f; v.y = 0.0f; v.z = 0.0f;
    BrRbVelAtPoint(&v, pBody, &p);

    /* 0x10067314..0x10067344, associated (nx*vx + ny*vy) + nz*vz, and the
     * plane is again the LAST contacting wheel's. */
    n.x = aHit[iLast].nx;
    n.y = aHit[iLast].ny;
    n.z = aHit[iLast].nz;
    vn  = (n.x * v.x + n.y * v.y) + n.z * v.z;

    /* 0x10067359..0x1006736F: the same x87 absolute value, then
     * `fcomp 0.1f` + `test ah,0x41` + `je <ret 0>` -- so the function LEAVES
     * when |vn| is strictly greater than 0.1.  The corner has to be nearly
     * at rest against the ground; a corner driving into it fails here, which
     * is the whole reason this cannot damp a diverging pitch. */
    if (!(vn >= 0.0f)) {
        vn = -vn;
    }
    if (vn > BR_CR_TIP_STILL) {
        return 0;
    }

    /* 0x10067380..0x100673BD: s = dot(chassis contact normal, m row 0), the
     * component of the plane normal along the car's own longitudinal axis,
     * associated (ny*m01 + nz*m02) + nx*m00. */
    s = (pBodyPlaneN->y * pBody->m.m[0][1] + pBodyPlaneN->z * pBody->m.m[0][2])
        + pBodyPlaneN->x * pBody->m.m[0][0];

    /* 0x100673C5: `test ah,0x41` + `je` SKIPS the -0.1 store, so +0.1 needs a
     * strict ordered greater and EQUAL takes the negative arm.  With the
     * chassis plane unwritten -- see the header -- s is 0 and this is always
     * the negative arm. */
    vec.x = 0.0f;
    vec.y = (s > 0.0f) ? BR_CR_TIP_KICK : -BR_CR_TIP_KICK;
    vec.z = 0.0f;

    /* 0x100673F8 == 0x1006D9D0 == BrMat4MulVec3Transposed: rotation only,
     * body -> world. */
    BrMat4MulVec3Transposed(&out, &pBody->m, &vec);

    /* 0x100673FD..0x10067441.  Each component is `fmul -2.0` then
     * `fsubr angVel`, i.e. angVel - (out * -2) == angVel + 2*out, stored x,
     * y, z in that order. */
    pSave->angVel.x = pSave->angVel.x - out.x * BR_CR_TIP_GAIN;
    pSave->angVel.y = pSave->angVel.y - out.y * BR_CR_TIP_GAIN;
    pSave->angVel.z = pSave->angVel.z - out.z * BR_CR_TIP_GAIN;

    /* 0x10067447 == 0x1006D530 == BrRbQuatDerivative, on `save`. */
    BrRbQuatDerivative(pSave);

    ++g_cBrCollRespTipKick;
    return 1;
}
