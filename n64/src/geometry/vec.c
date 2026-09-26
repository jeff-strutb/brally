/* vec.c -- 3-vector math, Top Gear Rally (N64) 0x8022439C-0x80224B7C.
 *
 * The PC decomp has the same routines (src/core/geometry/br_vec.c); these are
 * transcribed from the ROM, not copied.  IDO keeps commutative operands in
 * source order, so every `a * b` below is the order the original wrote.
 */
#include "tgr/vec.h"

/* WHAT IT DOES: cross product -- out = a x b.  Two components are copied to
 * the stack first, so out may alias a or b. */
/* @implements 0x8022439C tgr BrVec3Cross */
void BrVec3Cross(BrVec3 *pOut, BrVec3 *pA, BrVec3 *pB)
{
    float z = pA->x * pB->y - pB->x * pA->y;
    float az = pA->z;
    float bz = pB->z;
    pOut->z = z;
    pOut->y = az * pB->x - bz * pA->x;
    pOut->x = pA->y * bz - pB->y * az;
}

/* WHAT IT DOES: dot product of two vectors. */
/* @implements 0x80224404 tgr BrVec3Dot */
float BrVec3Dot(BrVec3 *pA, BrVec3 *pB)
{
    return pB->z * pA->z + (pA->x * pB->x + pA->y * pB->y);
}

/* WHAT IT DOES: out = -v. */
/* @implements 0x80224434 tgr BrVec3Negate */
void BrVec3Negate(BrVec3 *pOut, BrVec3 *pV)
{
    pOut->x = -pV->x;
    pOut->y = -pV->y;
    pOut->z = -pV->z;
}

/* WHAT IT DOES: negate a vector in place. */
/* @implements 0x8022445C tgr BrVec3NegateSelf */
void BrVec3NegateSelf(BrVec3 *pV)
{
    pV->x = -pV->x;
    pV->y = -pV->y;
    pV->z = -pV->z;
}

/* WHAT IT DOES: component-wise product of the x and y components only
 * (a 2-D multiply on the ground plane); z is left alone. */
/* @implements 0x80224484 tgr BrVec2Mul */
void BrVec2Mul(BrVec3 *pOut, BrVec3 *pA, BrVec3 *pB)
{
    pOut->x = pB->x * pA->x;
    pOut->y = pB->y * pA->y;
}

/* WHAT IT DOES: scale the x and y components by s; z is left alone. */
/* @implements 0x802244A8 tgr BrVec2Scale */
void BrVec2Scale(BrVec3 *pOut, BrVec3 *pV, float s)
{
    pOut->x = pV->x * s;
    pOut->y = pV->y * s;
}

/* WHAT IT DOES: component-wise product of two vectors. */
/* @implements 0x802244C8 tgr BrVec3Mul */
void BrVec3Mul(BrVec3 *pOut, BrVec3 *pA, BrVec3 *pB)
{
    pOut->x = pB->x * pA->x;
    pOut->y = pB->y * pA->y;
    pOut->z = pB->z * pA->z;
}

/* WHAT IT DOES: out = v * s. */
/* @implements 0x802244FC tgr BrVec3Scale */
void BrVec3Scale(BrVec3 *pOut, BrVec3 *pV, float s)
{
    pOut->x = pV->x * s;
    pOut->y = pV->y * s;
    pOut->z = pV->z * s;
}

/* WHAT IT DOES: scale a vector in place by s. */
/* @implements 0x80224528 tgr BrVec3ScaleBy */
void BrVec3ScaleBy(BrVec3 *pV, float s)
{
    pV->x *= s;
    pV->y *= s;
    pV->z *= s;
}

/* WHAT IT DOES: out = v / s, done as one reciprocal and three multiplies. */
/* @implements 0x8022455C tgr BrVec3Div */
void BrVec3Div(BrVec3 *pOut, BrVec3 *pV, float s)
{
    s = 1.0f / s;
    pOut->x = pV->x * s;
    pOut->y = pV->y * s;
    pOut->z = pV->z * s;
}

/* WHAT IT DOES: divide a vector in place by s. */
/* @implements 0x80224594 tgr BrVec3DivBy */
void BrVec3DivBy(BrVec3 *pV, float s)
{
    s = 1.0f / s;
    pV->x *= s;
    pV->y *= s;
    pV->z *= s;
}

/* WHAT IT DOES: copy a vector (destination first). */
/* @implements 0x802245D4 tgr BrVec3Copy */
void BrVec3Copy(BrVec3 *pDst, BrVec3 *pSrc)
{
    pDst->x = pSrc->x;
    pDst->y = pSrc->y;
    pDst->z = pSrc->z;
}

/* WHAT IT DOES: unit vector pointing from `from` to `to`.  When the two
 * points coincide it writes straight up the z axis (0,0,1) instead of
 * dividing by zero. */
/* @implements 0x802245F0 tgr BrVec3Direction */
void BrVec3Direction(BrVec3 *pOut, BrVec3 *pFrom, BrVec3 *pTo)
{
    float dx = pTo->x - pFrom->x;
    float dy = pTo->y - pFrom->y;
    float dz = pTo->z - pFrom->z;
    float len = sqrtf(dx * dx + dy * dy + dz * dz);
    float r;

    if (len != 0.0f) {
        r = 1.0f / len;
        pOut->x = dx * r;
        pOut->y = dy * r;
        pOut->z = dz * r;
    } else {
        pOut->x = 0.0f;
        pOut->y = 0.0f;
        pOut->z = 1.0f;
    }
}

/* WHAT IT DOES: out = v normalised to unit length; a zero vector becomes
 * (0,0,1). */
/* @implements 0x802246BC tgr BrVec3Normal */
void BrVec3Normal(BrVec3 *pOut, BrVec3 *pV)
{
    float len = sqrtf(pV->z * pV->z + (pV->x * pV->x + pV->y * pV->y));
    float r;

    if (len != 0.0f) {
        r = 1.0f / len;
        pOut->x = pV->x * r;
        pOut->y = pV->y * r;
        pOut->z = pV->z * r;
    } else {
        pOut->x = 0.0f;
        pOut->y = 0.0f;
        pOut->z = 1.0f;
    }
}

/* WHAT IT DOES: normalise a vector in place; a zero vector becomes (0,0,1). */
/* @implements 0x80224760 tgr BrVec3Normalise */
void BrVec3Normalise(BrVec3 *pV)
{
    float len = sqrtf(pV->z * pV->z + (pV->x * pV->x + pV->y * pV->y));
    float r;

    if (len != 0.0f) {
        r = 1.0f / len;
        pV->x *= r;
        pV->y *= r;
        pV->z *= r;
    } else {
        pV->x = 0.0f;
        pV->y = 0.0f;
        pV->z = 1.0f;
    }
}

/* WHAT IT DOES: out = a - b. */
/* @implements 0x80224808 tgr BrVec3Sub */
void BrVec3Sub(BrVec3 *pOut, BrVec3 *pA, BrVec3 *pB)
{
    pOut->x = pA->x - pB->x;
    pOut->y = pA->y - pB->y;
    pOut->z = pA->z - pB->z;
}

/* WHAT IT DOES: a -= b, in place. */
/* @implements 0x8022483C tgr BrVec3SubFrom */
void BrVec3SubFrom(BrVec3 *pA, BrVec3 *pB)
{
    pA->x -= pB->x;
    pA->y -= pB->y;
    pA->z -= pB->z;
}

/* WHAT IT DOES: add the x and y components only; z is left alone. */
/* @implements 0x80224870 tgr BrVec2Add */
void BrVec2Add(BrVec3 *pOut, BrVec3 *pA, BrVec3 *pB)
{
    pOut->x = pB->x + pA->x;
    pOut->y = pB->y + pA->y;
}

/* WHAT IT DOES: out = a + b. */
/* @implements 0x80224894 tgr BrVec3Add */
void BrVec3Add(BrVec3 *pOut, BrVec3 *pA, BrVec3 *pB)
{
    pOut->x = pB->x + pA->x;
    pOut->y = pB->y + pA->y;
    pOut->z = pB->z + pA->z;
}

/* WHAT IT DOES: a += b, in place. */
/* @implements 0x802248C8 tgr BrVec3AddTo */
void BrVec3AddTo(BrVec3 *pA, BrVec3 *pB)
{
    pA->x += pB->x;
    pA->y += pB->y;
    pA->z += pB->z;
}

/* WHAT IT DOES: linear interpolation -- t = 0 gives b, t = 1 gives a. */
/* @implements 0x802248FC tgr BrVec3Lerp */
void BrVec3Lerp(BrVec3 *pOut, BrVec3 *pA, BrVec3 *pB, float t)
{
    pOut->x = pB->x + (pA->x - pB->x) * t;
    pOut->y = pB->y + (pA->y - pB->y) * t;
    pOut->z = pB->z + (pA->z - pB->z) * t;
}

/* WHAT IT DOES: out = a + b * s -- step a point along a direction. */
/* @implements 0x8022494C tgr BrVec3MulAdd */
void BrVec3MulAdd(BrVec3 *pOut, BrVec3 *pA, BrVec3 *pB, float s)
{
    pOut->x = pB->x * s + pA->x;
    pOut->y = pB->y * s + pA->y;
    pOut->z = pB->z * s + pA->z;
}

/* WHAT IT DOES: a += b * s, in place. */
/* @implements 0x80224990 tgr BrVec3MulAddTo */
void BrVec3MulAddTo(BrVec3 *pA, BrVec3 *pB, float s)
{
    pA->x += pB->x * s;
    pA->y += pB->y * s;
    pA->z += pB->z * s;
}

/* WHAT IT DOES: the point halfway between a and b. */
/* @implements 0x802249D4 tgr BrVec3Midpoint */
void BrVec3Midpoint(BrVec3 *pOut, BrVec3 *pA, BrVec3 *pB)
{
    pOut->x = (pB->x + pA->x) * 0.5f;
    pOut->y = (pB->y + pA->y) * 0.5f;
    pOut->z = (pB->z + pA->z) * 0.5f;
}

/* WHAT IT DOES: set a vector to zero. */
/* @implements 0x80224A1C tgr BrVec3Zero */
void BrVec3Zero(BrVec3 *pV)
{
    pV->z = 0.0f;
    pV->y = 0.0f;
    pV->x = 0.0f;
}

/* WHAT IT DOES: distance between two points on the ground plane (x and y
 * only). */
/* @implements 0x80224A34 tgr BrVec3DistXY */
float BrVec3DistXY(BrVec3 *pA, BrVec3 *pB)
{
    float dx = pA->x - pB->x;
    float dy = pA->y - pB->y;
    return sqrtf(dx * dx + dy * dy);
}

/* WHAT IT DOES: distance between two points. */
/* @implements 0x80224A78 tgr BrVec3Dist */
float BrVec3Dist(BrVec3 *pA, BrVec3 *pB)
{
    float dx = pA->x - pB->x;
    float dy = pA->y - pB->y;
    float dz = pA->z - pB->z;
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

/* WHAT IT DOES: squared distance between two points -- for comparisons
 * that do not need the square root. */
/* @implements 0x80224ACC tgr BrVec3DistSq */
float BrVec3DistSq(BrVec3 *pA, BrVec3 *pB)
{
    float dx = pA->x - pB->x;
    float dy = pA->y - pB->y;
    float dz = pA->z - pB->z;
    return dx * dx + dy * dy + dz * dz;
}

/* WHAT IT DOES: length of a vector. */
/* @implements 0x80224B08 tgr BrVec3Length */
float BrVec3Length(BrVec3 *pV)
{
    return sqrtf(pV->z * pV->z + (pV->x * pV->x + pV->y * pV->y));
}

/* WHAT IT DOES: length of a vector's ground-plane (x, y) part. */
/* @implements 0x80224B48 tgr BrVec3LenXY */
float BrVec3LenXY(BrVec3 *pV)
{
    float y = pV->y;
    float x = pV->x;
    return sqrtf(y * y + x * x);
}
