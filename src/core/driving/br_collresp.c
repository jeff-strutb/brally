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
uint32_t g_cBrCollRespBroad;
uint32_t g_cBrCollRespGathered;
uint32_t g_cBrCollRespOverflow;
uint32_t g_cBrCollRespResponded;

/* @n64 0x8021C46C located */
void BrCollRespCountersReset(void)
{
    g_cBrCollRespTipKick    = 0u;
    g_cBrCollRespDegenerate = 0u;
    g_cBrCollRespBroad      = 0u;
    g_cBrCollRespGathered   = 0u;
    g_cBrCollRespOverflow   = 0u;
    g_cBrCollRespResponded  = 0u;
}

/* ==================================================================== */
/* The cube's plane constants, all read out of BRGlide.dll               */
/*                                                                       */
/* 0x10077B48 / 0x10077B50 are +-0.5 and are DOUBLES (`fcom qword ptr`), */
/* as are 0x10077B58 / 0x10077B60 (+-1) and 0x10077AB0 / 0x10077B68      */
/* (+-1.5).  The compares are float-against-double, so they happen in    */
/* double and the boundary is the double's.  0x10077A78 is a FLOAT zero  */
/* and 0x10077B20 a DOUBLE zero; they are named apart because the        */
/* addresses are.                                                        */
/* ==================================================================== */
#define BR_CR_FACE_HI    ( 0.5)
#define BR_CR_FACE_LO    (-0.5)
#define BR_CR_EDGE_HI    ( 1.0)
#define BR_CR_EDGE_LO    (-1.0)
#define BR_CR_CORNER_HI  ( 1.5)
#define BR_CR_CORNER_LO  (-1.5)
#define BR_CR_ZERO_F     0.0f
#define BR_CR_ZERO_D     (0.0)

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
/* The frame, and the correction to 0x1006DDD0                           */
/* ==================================================================== */

/* @n64 0x80229530 located */
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
    BrVec3  t, r;
    int     i, j;

    pF->a[0] = sx;
    pF->a[1] = sy;
    pF->a[2] = sz;

    /* THE ROTATION.  0x1006DDE B..0x1006DE26, exactly as slice3_44.c has it:
     *      pOut->m[i][j] = pA->m[j][i] * scale[j],   pOut->m[i][3] = 0
     * with the outer loop walking pOut's rows and the inner one walking the
     * scale.  Reproduced here rather than called, for the reason below. */
    for (i = 0; i < 3; ++i) {
        for (j = 0; j < 3; ++j) {
            pOut->m[i][j] = pBody->m[j][i] * pF->a[j];
        }
        pOut->m[i][3] = 0.0f;
    }
    pOut->m[3][3] = 1.0f;               /* 0x1006DE30 */

    /* THE TRANSLATION, AND THE CORRECTION.  0x1006DE37 reads `[ecx+0x30]`
     * with ecx set at 0x1006DE2C from `[esp+0x24]`.  At THAT point esp is
     * R-0x20, so `[esp+0x24]` is R+4 -- ARG1, the body matrix.  Earlier, at
     * 0x1006DDD9, the SAME displacement `[esp+0x24]` was read with esp at
     * R-0x18, where it is R+0xC -- arg3, the scale.  Two reads, one
     * displacement, two different arguments, because two pushes sit between
     * them.  slice3_44.c takes the translation from pS (arg3), and
     * br_collresp.h used to explain the resulting garbage as a deliberate
     * frame OVERLAP in 0x10067C30.
     *
     * It is not an overlap and it is not deliberate.  Taken from pS the box
     * matrix has no translation in it at all -- the transform is about the
     * WORLD ORIGIN rather than about the car -- so a car anywhere but at
     * (0, 0, 0) would classify every triangle on the track as hundreds of
     * box-widths away and the OBB system could never fire.  That prediction
     * requires the shipped game to have no car-versus-track collision, which
     * it plainly has; a reading that needs the game to be broken is held to
     * a higher standard than one that does not, and this one had less.
     *
     * Taken from pA (arg1) it is the ordinary world-to-box transform:
     *      pOut->m[3][*] = (-pA->m[3][*]) * transpose(pOut)
     *
     * slice3_44.c is not this module's to edit, so the correction is FILED
     * (see br_collresp.h) and this wrapper does the build itself.  Calling
     * slice3_44's version and patching the row afterwards would leave a
     * function in the tree that is wrong for its next caller and right only
     * by accident here. */
    t.x = -pBody->m[3][0];
    t.y = -pBody->m[3][1];
    t.z = -pBody->m[3][2];
    BrMat4MulVec3Transposed(&r, pOut, &t);      /* 0x1006D9D0 */
    pOut->m[3][0] = r.x;
    pOut->m[3][1] = r.y;
    pOut->m[3][2] = r.z;
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

/* ==================================================================== */
/* 0x10066260 -- the 26-plane outcode classify                           */
/*                                                                       */
/* Three stages, each an AND of per-vertex outcodes: if every vertex is   */
/* outside the SAME plane the triangle cannot meet the cube.             */
/*                                                                       */
/* Stage 1 doubles as the accept: a vertex whose outcode is zero is       */
/* inside all six slabs, so 0x100662EE returns 1 immediately.            */
/*                                                                       */
/* THE NaN ARMS.  `fcom hi` + `test ah,0x41` + `jne` sends less, equal    */
/* AND UNORDERED to the second test, where `fcomp lo` + `test ah,1` +     */
/* `je` keeps only "not less-or-unordered".  So a NaN sets the LOW bit    */
/* of the pair -- it classifies as below the plane, not above it and not  */
/* inside.  Every stage repeats that shape and it is written out each     */
/* time rather than factored, because factoring it is how it gets         */
/* inverted.                                                             */
/* ==================================================================== */

/* One plane pair: bit 0 for "above hi", bit 1 for "below lo". */
static unsigned BrCrSide(float v, double hi, double lo)
{
    if (!((double)v <= hi)) {          /* `test ah,0x41` NOT taken */
        return 1u;
    }
    if (!((double)v >= lo)) {          /* `test ah,1` taken        */
        return 2u;
    }
    return 0u;
}

/* 0x100664F0 -- stage 3, the eight corner planes.  Its argument is a
 * vertex and the mask still live; it returns the subset of that mask this
 * vertex is also outside.  The four sums are formed in the original's
 * order and association: (z + y) + x, (y + x) - z, (x - y) + z, (x - y) - z.
 * Written that way because the rounding differs from the tidy form. */
/* WHAT IT DOES: the third and last stage of the cheap collision test: it
 * checks a point against the eight corner planes that cut the corners off
 * the bounding box. It is given the set of planes the point might still be
 * outside and reports which of them it actually is outside. */
/* @implements 0x100664F0 glide BrCrCorner */
static unsigned BrCrCorner(const float aV[3], unsigned mask)
{
    unsigned out = 0u;
    float    t;

    if ((mask & 0x03u) != 0u) {
        t = (aV[2] + aV[1]) + aV[0];
        if ((mask & 0x01u) != 0u && !((double)t <= BR_CR_CORNER_HI)) {
            out |= 0x01u;
        } else if ((mask & 0x02u) != 0u && !((double)t >= BR_CR_CORNER_LO)) {
            out |= 0x02u;
        }
    }
    if ((mask & 0x0Cu) != 0u) {
        t = (aV[1] + aV[0]) - aV[2];
        if ((mask & 0x04u) != 0u && !((double)t <= BR_CR_CORNER_HI)) {
            out |= 0x04u;
        } else if ((mask & 0x08u) != 0u && !((double)t >= BR_CR_CORNER_LO)) {
            out |= 0x08u;
        }
    }
    if ((mask & 0x30u) != 0u) {
        t = (aV[0] - aV[1]) + aV[2];
        if ((mask & 0x10u) != 0u && !((double)t <= BR_CR_CORNER_HI)) {
            out |= 0x10u;
        } else if ((mask & 0x20u) != 0u && !((double)t >= BR_CR_CORNER_LO)) {
            out |= 0x20u;
        }
    }
    if ((mask & 0xC0u) != 0u) {
        t = (aV[0] - aV[1]) - aV[2];
        if ((mask & 0x40u) != 0u && !((double)t <= BR_CR_CORNER_HI)) {
            out |= 0x40u;
        } else if ((mask & 0x80u) != 0u && !((double)t >= BR_CR_CORNER_LO)) {
            out |= 0x80u;
        }
    }
    return out;
}

int BrCollRespBoxClassify(const float aV[9])
{
    unsigned mask;
    int      i;

    /* ---- stage 1, the six faces.  0x10066272..0x100662FD ------------- */
    mask = 0xFFFFFFFFu;
    for (i = 0; i < 3; ++i) {
        const float *p = aV + i * 3;
        unsigned     c = BrCrSide(p[0], BR_CR_FACE_HI, BR_CR_FACE_LO)
                       | (BrCrSide(p[1], BR_CR_FACE_HI, BR_CR_FACE_LO) << 2)
                       | (BrCrSide(p[2], BR_CR_FACE_HI, BR_CR_FACE_LO) << 4);

        /* 0x100662EE: a vertex inside every slab settles it. */
        if (c == 0u) {
            return 1;
        }
        mask &= c;
    }
    /* 0x10066303: any surviving bit is a separating face plane. */
    if (mask != 0u) {
        return 0;
    }

    /* ---- stage 2, the twelve edge planes.  0x1006630E..0x100664AF ---- */
    mask = 0xFFFFFFFFu;
    for (i = 0; i < 3; ++i) {
        const float *p = aV + i * 3;
        unsigned     c = 0u;
        float        t;

        /* The six sums, in the original's order: x+y, x-y, x+z, x-z,
         * y+z, y-z, each against +-1. */
        if ((mask & 0x003u) != 0u) {
            t = p[0] + p[1];
            if ((mask & 0x001u) != 0u && !((double)t <= BR_CR_EDGE_HI)) {
                c |= 0x001u;
            } else if ((mask & 0x002u) != 0u
                       && !((double)t >= BR_CR_EDGE_LO)) {
                c |= 0x002u;
            }
        }
        if ((mask & 0x00Cu) != 0u) {
            t = p[0] - p[1];
            if ((mask & 0x004u) != 0u && !((double)t <= BR_CR_EDGE_HI)) {
                c |= 0x004u;
            } else if ((mask & 0x008u) != 0u
                       && !((double)t >= BR_CR_EDGE_LO)) {
                c |= 0x008u;
            }
        }
        if ((mask & 0x030u) != 0u) {
            t = p[0] + p[2];
            if ((mask & 0x010u) != 0u && !((double)t <= BR_CR_EDGE_HI)) {
                c |= 0x010u;
            } else if ((mask & 0x020u) != 0u
                       && !((double)t >= BR_CR_EDGE_LO)) {
                c |= 0x020u;
            }
        }
        if ((mask & 0x0C0u) != 0u) {
            t = p[0] - p[2];
            if ((mask & 0x040u) != 0u && !((double)t <= BR_CR_EDGE_HI)) {
                c |= 0x040u;
            } else if ((mask & 0x080u) != 0u
                       && !((double)t >= BR_CR_EDGE_LO)) {
                c |= 0x080u;
            }
        }
        if ((mask & 0x300u) != 0u) {
            t = p[1] + p[2];
            if ((mask & 0x100u) != 0u && !((double)t <= BR_CR_EDGE_HI)) {
                c |= 0x100u;
            } else if ((mask & 0x200u) != 0u
                       && !((double)t >= BR_CR_EDGE_LO)) {
                c |= 0x200u;
            }
        }
        if ((mask & 0xC00u) != 0u) {
            t = p[1] - p[2];
            if ((mask & 0x400u) != 0u && !((double)t <= BR_CR_EDGE_HI)) {
                c |= 0x400u;
            } else if ((mask & 0x800u) != 0u
                       && !((double)t >= BR_CR_EDGE_LO)) {
                c |= 0x800u;
            }
        }

        mask = c;
        /* 0x10066496: the moment the intersection empties, stage 2 is
         * done and stage 3 starts -- MID-LOOP, so the remaining vertices
         * are not classified at all. */
        if (mask == 0u) {
            break;
        }
    }
    if (mask != 0u) {
        return 0;
    }

    /* ---- stage 3, the eight corner planes.  0x100664B0..0x100664D9 --- */
    mask = 0xFFu;                      /* `or eax,0xffffffff`, read as dl */
    for (i = 0; i < 3; ++i) {
        mask = BrCrCorner(aV + i * 3, mask);
        if (mask == 0u) {
            break;
        }
    }
    /* 0x100664CE: `neg/sbb/neg/dec` -- 0 when the mask survived, -1 when
     * it emptied. */
    return (mask != 0u) ? 0 : -1;
}

/* ==================================================================== */
/* 0x10066800 -- segment versus the unit cube                            */
/*                                                                       */
/* The slab rejection uses the DIRECTION'S SIGN rather than a min/max, so */
/* it is written the original's way: sgn[i] is +1 unless d[i] is negative */
/* or NaN, and the two rejects are `A[i]*sgn[i] > 0.5` and                */
/* `B[i]*sgn[i] < -0.5`.  Then the three cross-product tests, with the    */
/* cube's half-extent as the FLOAT 0.5 at 0x10077AC8 -- not the double    */
/* the slab test uses.                                                   */
/* ==================================================================== */
int BrCollRespSegBox(const BrVec3 *pA, const BrVec3 *pB)
{
    const float *a = &pA->x;
    const float *b = &pB->x;
    float        d[3];
    int          sgn[3];
    int          i;
    static const int aJ[3] = { 1, 2, 0 };
    static const int aK[3] = { 2, 0, 1 };

    d[0] = b[0] - a[0];
    d[1] = b[1] - a[1];
    d[2] = b[2] - a[2];

    /* 0x10066836: `fcomp 0.0f` + `test ah,1` + `je` -- +1 unless C0, so a
     * NaN direction takes -1. */
    for (i = 0; i < 3; ++i) {
        sgn[i] = !(d[i] >= BR_CR_ZERO_F) ? -1 : 1;
    }

    /* 0x10066871: the slabs.  Both compares are against DOUBLES. */
    for (i = 0; i < 3; ++i) {
        float s = (float)sgn[i];

        if (!((double)(a[i] * s) <= BR_CR_FACE_HI)) {
            return 0;
        }
        if (!((double)(b[i] * s) >= BR_CR_FACE_LO)) {
            return 0;
        }
    }

    /* 0x100668AF: the three axis pairs (1,2), (2,0), (0,1). */
    for (i = 0; i < 3; ++i) {
        int   j = aJ[i];
        int   k = aK[i];
        float S = (float)sgn[k] * d[j] + (float)sgn[j] * d[k];
        float C = a[j] * d[k] - a[k] * d[j];
        float h = S * BR_CR_HALF;

        /* 0x10066919: `fcompp` of h*h against C*C, then `test ah,1` --
         * REJECT on less-or-unordered, i.e. keep only h*h >= C*C. */
        if (h * h < C * C || !(h * h == h * h) || !(C * C == C * C)) {
            return 0;
        }
    }
    return 1;
}

/* ==================================================================== */
/* 0x10066610 -- point in triangle, by 2D crossing count                 */
/*                                                                       */
/* The projection drops the axis of the largest |n|, and WHICH of the     */
/* other two becomes u depends on the SIGN of n at that axis, so the      */
/* winding survives the projection.  The accumulator is a signed crossing */
/* count, not a boolean, and the original returns it as-is.               */
/* ==================================================================== */
int BrCollRespPointInTri(const float aV[9], const BrVec3 *pN,
                         const BrVec3 *pP)
{
    const float *n = &pN->x;
    const float *p = &pP->x;
    float        aAbs[3];
    int          dom, u, v, i, acc = 0;

    /* 0x10066628: `fcomp 0.0f` + `test ah,1` + `fchs` -- a NaN is negated
     * rather than left alone, which is the x87 absolute value this tree
     * documents in three other places. */
    for (i = 0; i < 3; ++i) {
        aAbs[i] = !(n[i] >= BR_CR_ZERO_F) ? -n[i] : n[i];
    }

    /* 0x10066644: the dominant axis, by the original's two-compare tree.
     * Each `test ah,0x41` + `jne` takes the SECOND arm for less, equal or
     * unordered, so ties go to the later axis. */
    if (aAbs[0] > aAbs[2]) {
        dom = (aAbs[0] > aAbs[1]) ? 0 : 1;
    } else {
        dom = (aAbs[1] > aAbs[2]) ? 1 : 2;
    }

    /* 0x10066688: the sign of n at the dominant axis picks the handedness
     * of the projection. */
    if (!(n[dom] >= BR_CR_ZERO_F)) {
        u = (dom + 2) % 3;
        v = (dom + 1) % 3;
    } else {
        u = (dom + 1) % 3;
        v = (dom + 2) % 3;
    }

    /* 0x100666DB: the three edges (v0,v1), (v1,v2), (v2,v0). */
    for (i = 0; i < 3; ++i) {
        const float *A = aV + i * 3;
        const float *B = aV + ((i + 1) % 3) * 3;
        int          su, sv;

        /* Each of these four is `fcompp` + `test ah,1`, i.e. 1 on
         * strictly-less-or-unordered.  Written out four times because the
         * third one has its operands in the other order (`fxch st(1)` at
         * 0x10066754) and folding them would lose that. */
        su = (p[u] < B[u] ? 1 : 0) - (p[u] < A[u] ? 1 : 0);
        if (su == 0) {
            continue;                  /* 0x10066728 */
        }

        sv = (p[v] < B[v] ? 1 : 0) - (p[v] < A[v] ? 1 : 0);
        if (sv == 0) {
            /* 0x100667C3: the edge does not straddle in v, so the crossing
             * is decided by which side of P it lies -- `test ah,0x41` +
             * `je <skip>` counts it for less-or-equal-or-unordered. */
            if (!(A[v] > p[v])) {
                acc += su;
            }
            continue;
        }

        {
            float eu = B[u] - A[u];
            float ev = B[v] - A[v];
            float pu = p[u] - A[u];
            float pv = p[v] - A[v];
            float s  = (float)su;

            /* 0x100667A8: `fcompp` of (eu*pv)*s against (pu*ev)*s, then
             * `test ah,1` + `jne <skip>` -- so the crossing counts on
             * greater-or-equal, and a NaN skips.  The multiply order is
             * the original's: the two products are formed first and only
             * then scaled by s. */
            if (!((eu * pv) * s < (pu * ev) * s)) {
                acc += su;
            }
        }
    }
    return acc;
}

/* ==================================================================== */
/* 0x10066950 -- the exact test, for the classify's -1                   */
/* ==================================================================== */
static int BrCrExact(const float aV[9], const BrVec3 *pN)
{
    const float *n = &pN->x;
    float        s[3];
    BrVec3       P;
    float        num, den, t;
    int          i;

    /* 0x1006695E: the three edges, each against the cube. */
    for (i = 0; i < 3; ++i) {
        if (BrCollRespSegBox((const BrVec3 *)(const void *)(aV + i * 3),
                             (const BrVec3 *)(const void *)
                             (aV + ((i + 1) % 3) * 3)) != 0) {
            return 1;
        }
    }

    /* 0x1006699E: sign(n) per component, as +-1 INTS that are then
     * `fild`ed back -- so the value really is +-1.0f and not the raw
     * component.  `test ah,1` + `je` gives +1 for not-less-not-unordered. */
    for (i = 0; i < 3; ++i) {
        s[i] = !(n[i] >= BR_CR_ZERO_F) ? -1.0f : 1.0f;
    }

    /* 0x100669C0..0x10066A0E: t = dot(n, v0) / dot(n, s), with the
     * numerator associated ((v0.y*n.y + v0.z*n.z) + v0.x*n.x) and the
     * denominator ((n.x*s.x + n.y*s.y) + n.z*s.z).  Division by a zero
     * denominator yields an infinity, which the window test below then
     * rejects -- the original has no guard and needs none. */
    num = (aV[1] * n[1] + aV[2] * n[2]) + aV[0] * n[0];
    den = (n[0] * s[0] + n[1] * s[1]) + n[2] * s[2];
    t   = num / den;

    /* 0x10066A1A..0x10066A35: (t + 0.5) * (t - 0.5) against a DOUBLE zero,
     * `test ah,0x41` + `jne <continue>` -- so it continues on
     * less-or-equal-or-unordered, i.e. |t| <= 0.5 keeps going and a NaN
     * keeps going too. */
    if (!((double)((t - (float)BR_CR_FACE_LO) * (t - (float)BR_CR_FACE_HI))
          <= BR_CR_ZERO_D)) {
        return 0;
    }

    P.x = t * s[0];
    P.y = t * s[1];
    P.z = t * s[2];
    return BrCollRespPointInTri(aV, pN, &P);
}

/* 0x10066AA0 -- classify, and resolve the inconclusive answer. */
/* WHAT IT DOES: decides whether a triangle touches the car's collision box.
 * It tries the cheap box test first, and only when that cannot say either
 * way does it fall through to the exact -- and much more expensive --
 * intersection test. */
/* @implements 0x10066AA0 glide BrCrTest */
int BrCrTest(const float aV[9], const BrVec3 *pN)
{
    int r = BrCollRespBoxClassify(aV);

    if (r != -1) {
        return r;
    }
    return BrCrExact(aV, pN);
}

/* ==================================================================== */
/* 0x10066230 / 0x10067C4E -- the candidate list                         */
/* ==================================================================== */

BrCollRespNode *g_pBrCollRespList;              /* 0x11778198 */

static BrCollRespNode s_aNode[BR_CR_LIST_MAX];  /* 0x117781B0 */
static int            s_iNode;                  /* 0x11778844 */

void BrCollRespListReset(void)
{
    g_pBrCollRespList = NULL;
    s_iNode           = 0;
}

#ifdef BR_MATCHING_BUILD
/* The original's allocator is a POINTER bump cursor (0x11778844), not the
 * port's index -- and it has no bound, which the header explains is safe. */
extern BrCollRespNode *g_pBrCrCursor;           /* 0x11778844 */

/* WHAT IT DOES: remember one more surface the car is currently touching, by
 * pushing it onto the front of this frame's contact list. The collision
 * response then walks that list to work out which way to push the car.
 *
 * The original allocates by bumping a cursor with NO bound check. That is
 * safe rather than lucky: a cell holds at most 150 contacts and the pool is
 * 200 nodes. The port's copy below enforces the bound anyway and counts
 * refusals, because a port that silently wrote past the array would be a
 * worse bug than the one it models. */
/* @implements 0x10066230 glide BrCrListPush */
/* @n64 0x8025C24C located */
void BrCrListPush(const BrCollPlane *pPlane)
{
    BrCollRespNode *pN = g_pBrCrCursor++;

    pN->pPlane = pPlane;
    pN->pNext  = g_pBrCollRespList;
    g_pBrCollRespList = pN;
}
#else
static void BrCrListPush(const BrCollPlane *pPlane)
{
    BrCollRespNode *pN;

    /* The original bumps without a bound.  It cannot overflow -- a cell
     * holds at most BR_COLL_CELL_PLANES == 150 records and the pool is 200
     * nodes -- but a port that silently wrote past the array would be a
     * worse bug than the one it is modelling, so the bound is enforced and
     * the refusal is counted. */
    if (s_iNode >= BR_CR_LIST_MAX) {
        ++g_cBrCollRespOverflow;
        return;
    }
    pN = &s_aNode[s_iNode++];
    pN->pPlane = pPlane;
    pN->pNext  = g_pBrCollRespList;
    g_pBrCollRespList = pN;
}
#endif

/* ==================================================================== */
/* 0x10066AD0 -- the gather                                              */
/*                                                                       */
/* THE WALK DIRECTION IS A GLOBAL.  0x10066B09 tests 0x11778848: non-zero */
/* walks the cell BACKWARDS from its last record, zero walks it forwards. */
/* The two arms are otherwise byte-identical (0x10066B33 and 0x10066C5D   */
/* are the same 214 bytes), and the backward arm reaches its start by     */
/* biasing the base by one record -- `lea esi,[edx+0x11773678]` with the  */
/* real base at 0x11773698.  Nothing in this port writes 0x11778848, so   */
/* it is modelled as the forward arm with the flag exposed rather than    */
/* two copies of the same loop.                                          */
/* ==================================================================== */

int g_brCollRespWalkBack;               /* 0x11778848 */

int BrCollRespBroadPhase(const BrRbBodyFull *pBody, const BrMat4 *pMatBox)
{
    short  cell;
    int    count, i, n = 0;

    ++g_cBrCollRespBroad;

    if (g_pBrCollGrid == NULL || g_pBrCollGridCount == NULL) {
        return 0;
    }

    /* 0x10066AD7: the cell key is the body matrix's translation row, i.e.
     * the car's world (x, y).  Same key the ground probe uses. */
    cell = BrCollGridCellAcquire(pBody->m.m[3][0], pBody->m.m[3][1]);
    if (cell < 0) {
        return 0;
    }
    /* 0x10066AFE: the count is a u16. */
    count = (int)g_pBrCollGridCount[cell];
    if (count > BR_COLL_CELL_PLANES) {
        count = BR_COLL_CELL_PLANES;
    }

    for (i = 0; i < count; ++i) {
        int                j = g_brCollRespWalkBack ? (count - 1 - i) : i;
        const BrCollPlane *pP =
            &g_pBrCollGrid[(int)cell * BR_COLL_CELL_PLANES + j];
        float  aV[9];
        BrVec3 nrm, e1, e2;

        /* 0x10066B33..0x10066B61: three BrMat4TransformPoint calls
         * (0x1006DA20), the box matrix applied to the record's three
         * vertex pointers. */
        BrMat4TransformPoint((BrVec3 *)(void *)&aV[0], pMatBox, pP->pV0);
        BrMat4TransformPoint((BrVec3 *)(void *)&aV[3], pMatBox, pP->pV1);
        BrMat4TransformPoint((BrVec3 *)(void *)&aV[6], pMatBox, pP->pV2);

        /* 0x10066B66..0x10066C0B: e1 = v1 - v0, e2 = v2 - v0, and the
         * normal is e2 x e1 -- NOT e1 x e2.  The order is read off the
         * three `fsubp`s at 0x10066BEA / 0x10066BF4 / 0x10066C07:
         *      n.x = e2.z*e1.y - e2.y*e1.z
         *      n.y = e2.x*e1.z - e2.z*e1.x
         *      n.z = e2.y*e1.x - e2.x*e1.y
         * It is only used for its DIRECTION, but the sign decides which
         * cube diagonal BrCrExact intersects, so it matters. */
        e1.x = aV[3] - aV[0]; e1.y = aV[4] - aV[1]; e1.z = aV[5] - aV[2];
        e2.x = aV[6] - aV[0]; e2.y = aV[7] - aV[1]; e2.z = aV[8] - aV[2];
        nrm.x = e2.z * e1.y - e2.y * e1.z;
        nrm.y = e2.x * e1.z - e2.z * e1.x;
        nrm.z = e2.y * e1.x - e2.x * e1.y;

        /* 0x10066C19/0x10066C21: and if it passes, the RECORD -- not the
         * transformed copy -- goes on the list. */
        if (BrCrTest(aV, &nrm) != 0) {
            BrCrListPush(pP);
            ++n;
        }
    }

    g_cBrCollRespGathered += (uint32_t)n;
    return n;
}
