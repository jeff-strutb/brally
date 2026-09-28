/* br_vecnorm.c -- in-place vector normalise.
 *
 * Glide 0x1006D410 / 0x1006D4B0 sit together, away from the 0x100343xx
 * vector cluster.  A TU that also contains Cross/Dot/Scale schedules the
 * scale-out as three fld-st copies; this file alone reproduces orig. */
/* WHAT IT DOES: combines two orientation quaternions (w, x, y, z order)
 * into their product, one quaternion for both rotations together. */
/* @implements 0x1006D2E0 glide BrQuatMul */
/* Byte-exact 2026-09-27.  Source facts: the pB components are loaded first,
 * then pA, each in index order; rows 0..2 are computed into named locals and
 * row 3 is computed inline in its own store, with the stores written 3, 2,
 * 1, 0 -- that is the original's store order, and computing the three rows
 * ahead of the stores keeps the accumulator layout.  FILE POSITION: above
 * the file's #includes; below them the header declarations shift VC5's
 * accumulator slots (faddp st(2)/st(7)). */
void BrQuatMul(float *pOut, const float *pA, const float *pB)
{
    float b0 = pB[0];
    float b1 = pB[1];
    float b2 = pB[2];
    float b3 = pB[3];
    float a0 = pA[0];
    float a1 = pA[1];
    float a2 = pA[2];
    float a3 = pA[3];
    float r0, r1, r2;
    r0 = a0 * b0 - a1 * b1 - a2 * b2 - a3 * b3;
    r1 = a1 * b0 + a0 * b1 - a3 * b2 + a2 * b3;
    r2 = a2 * b0 + a3 * b1 + a0 * b2 - a1 * b3;
    pOut[3] = a3 * b0 - a2 * b1 + a1 * b2 + a0 * b3;
    pOut[2] = r2;
    pOut[1] = r1;
    pOut[0] = r0;
}

#include "br_vec.h"

#include <math.h>

#ifdef BR_MATCHING_BUILD
extern float BrSqrtF(float x);   /* 0x10002570 -- fld [esp+4]; fsqrt; ret */
#endif


/* WHAT IT DOES: shrink or stretch a vector IN PLACE so it is exactly one unit
 * long, keeping the direction. That is what turns an arbitrary difference
 * between two points into a pure "which way", which is what the lighting,
 * steering and collision code want. A zero-length vector divides by zero here,
 * exactly as the original does -- callers are expected not to pass one. */
/* @implements 0x1006D4B0 glide BrVec3Normalise */
/* @implements 0x10074250 d3d BrVec3Normalise */
/* @n64 0x802581CC located */
void BrVec3Normalise(BrVec3 *pV)
{
    /* Length's three named locals (y,z integer-homed, x kept on x87) plus
     * DivBy's copy-into-fresh-temp on y and z of the scale-out.  x of the
     * scale-out is `pV->x * k` so k is a memory operand (fst home + fld). */
    float x = pV->x;
    float y = pV->y;
    float z = pV->z;
    float k, k2, k3;
#ifdef BR_MATCHING_BUILD
    k = 1.0f / BrSqrtF(y * y + z * z + x * x);
#else
    k = 1.0f / sqrtf(y * y + z * z + x * x);
#endif
    pV->x = pV->x * k;
    pV->y = (k2 = k) * pV->y;
    pV->z = (k3 = k) * pV->z;
}

/* WHAT IT DOES: the four-component version of BrVec3Normalise -- all four
 * components count towards the length, so this is a plain 4D normalise and
 * NOT a perspective divide. */
/* @implements 0x1006D410 glide BrVec4Normalise */
/* @implements 0x100741B0 d3d BrVec4Normalise */
/* @n64 0x8025813C located */
void BrVec4Normalise(BrVec4 *pV)
{
    /* z (f08) is the x87-resident square (like x in Vec3); y,w,x are
     * integer-homed.  Scale-out: f00 * k homes k; the rest copy-assign. */
    float y = pV->f04;
    float w = pV->f0C;
    float z = pV->f08;
    float x = pV->f00;
    float k, k2, k3, k4;
#ifdef BR_MATCHING_BUILD
    k = 1.0f / BrSqrtF(y * y + z * z + w * w + x * x);
#else
    k = 1.0f / sqrtf(y * y + z * z + w * w + x * x);
#endif
    pV->f00 = pV->f00 * k;
    pV->f04 = (k2 = k) * pV->f04;
    pV->f08 = (k3 = k) * pV->f08;
    pV->f0C = (k4 = k) * pV->f0C;
}
