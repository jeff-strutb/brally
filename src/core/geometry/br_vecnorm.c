/* br_vecnorm.c -- in-place vector normalise.
 *
 * Glide 0x1006D410 / 0x1006D4B0 sit together, away from the 0x100343xx
 * vector cluster.  A TU that also contains Cross/Dot/Scale schedules the
 * scale-out as three fld-st copies; this file alone reproduces orig. */
#include "br_vec.h"

#include <math.h>

#ifdef BR_MATCHING_BUILD
extern float BrSqrtF(float x);   /* 0x10002570 -- fld [esp+4]; fsqrt; ret */
#endif

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
