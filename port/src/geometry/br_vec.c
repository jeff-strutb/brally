/* br_vec.c -- vector math decompiled from BRD3D.dll. See br_vec.h.
 *
 * Each function below is a direct transcription of the x87 sequence at the
 * noted address. The originals write results in an order dictated by the FPU
 * stack (e.g. cross stores y, z, then x); that ordering is irrelevant to
 * callers, so the C is written in the natural order instead. Cross uses a
 * temporary because the original also buffers a component -- aliasing out
 * with a or b is therefore safe, exactly as in the original.
 */
#include "br_vec.h"

#include <math.h>

void BrVec3Cross(BrVec3 *pOut, const BrVec3 *pA, const BrVec3 *pB)
{
    float x = pA->y * pB->z - pA->z * pB->y;
    float y = pA->z * pB->x - pA->x * pB->z;
    float z = pA->x * pB->y - pA->y * pB->x;
    pOut->x = x; pOut->y = y; pOut->z = z;
}

float BrVec3Dot(const BrVec3 *pA, const BrVec3 *pB)
{
    return pA->x * pB->x + pA->y * pB->y + pA->z * pB->z;
}

void BrVec3Sub(BrVec3 *pOut, const BrVec3 *pA, const BrVec3 *pB)
{
    pOut->x = pA->x - pB->x;
    pOut->y = pA->y - pB->y;
    pOut->z = pA->z - pB->z;
}

void BrVec3AddTo(BrVec3 *pA, const BrVec3 *pB)
{
    pA->x += pB->x;
    pA->y += pB->y;
    pA->z += pB->z;
}

void BrVec3Scale(BrVec3 *pOut, const BrVec3 *pV, float s)
{
    pOut->x = pV->x * s;
    pOut->y = pV->y * s;
    pOut->z = pV->z * s;
}

void BrVec3ScaleBy(BrVec3 *pV, float s)
{
    pV->x *= s;
    pV->y *= s;
    pV->z *= s;
}

void BrVec3MulAdd(BrVec3 *pOut, const BrVec3 *pA, const BrVec3 *pB, float s)
{
    pOut->x = pA->x + pB->x * s;
    pOut->y = pA->y + pB->y * s;
    pOut->z = pA->z + pB->z * s;
}

void BrVec3MulAddTo(BrVec3 *pA, const BrVec3 *pB, float s)
{
    pA->x += pB->x * s;
    pA->y += pB->y * s;
    pA->z += pB->z * s;
}

void BrVec3Lerp(BrVec3 *pOut, const BrVec3 *pA, const BrVec3 *pB, float t)
{
    pOut->x = (pA->x - pB->x) * t + pB->x;
    pOut->y = (pA->y - pB->y) * t + pB->y;
    pOut->z = (pA->z - pB->z) * t + pB->z;
}

void BrVec3Negate(BrVec3 *pOut, const BrVec3 *pV)
{
    pOut->x = -pV->x;
    pOut->y = -pV->y;
    pOut->z = -pV->z;
}

void BrVec3Add(BrVec3 *pOut, const BrVec3 *pA, const BrVec3 *pB)
{
    pOut->x = pA->x + pB->x;
    pOut->y = pA->y + pB->y;
    pOut->z = pA->z + pB->z;
}

void BrVec3SubFrom(BrVec3 *pA, const BrVec3 *pB)
{
    pA->x -= pB->x;
    pA->y -= pB->y;
    pA->z -= pB->z;
}

/* The original loads the constant 1.0f (at 0x1008F628), divides once, then
 * multiplies each component -- one divide instead of three. Reproduced
 * faithfully because it is also observably different from three divides in
 * the low bits, and physics code may depend on that. */
void BrVec3Div(BrVec3 *pOut, const BrVec3 *pV, float s)
{
    float r = 1.0f / s;
    pOut->x = pV->x * r;
    pOut->y = pV->y * r;
    pOut->z = pV->z * r;
}

void BrVec3DivBy(BrVec3 *pV, float s)
{
    float r = 1.0f / s;
    pV->x *= r;
    pV->y *= r;
    pV->z *= r;
}

/* 0.5f constant lives at 0x1008F638. */
void BrVec3Midpoint(BrVec3 *pOut, const BrVec3 *pA, const BrVec3 *pB)
{
    pOut->x = (pA->x + pB->x) * 0.5f;
    pOut->y = (pA->y + pB->y) * 0.5f;
    pOut->z = (pA->z + pB->z) * 0.5f;
}

void BrVec3Zero(BrVec3 *pV)
{
    pV->x = 0.0f;
    pV->y = 0.0f;
    pV->z = 0.0f;
}

/* The original spills dx and dy to stack scratch and multiplies them back in,
 * which is just how MSVC scheduled the x87 stack; the arithmetic is a plain
 * squared distance. */
float BrVec3DistSq(const BrVec3 *pA, const BrVec3 *pB)
{
    float dx = pA->x - pB->x;
    float dy = pA->y - pB->y;
    float dz = pA->z - pB->z;
    return dx * dx + dy * dy + dz * dz;
}

/* 0x1003B0E0 -- identical body to BrVec3DistSq, then tail-calls the fsqrt
 * wrapper at 0x10002250 (`fld [esp+4]; fsqrt; ret`). Summation order is
 * (dz*dz + dy*dy) + dx*dx, matching DistSq. */
/* WHAT IT DOES: the straight-line distance between two points in 3D. One of
 * the most-used routines in the game -- how far a car is from anything. */
/* @implements 0x1003B0E0 d3d BrVec3Dist */
float BrVec3Dist(const BrVec3 *pA, const BrVec3 *pB)
{
    return sqrtf(BrVec3DistSq(pA, pB));
}

/* 0x1003B170 -- the original copies y and z through stack scratch, then sums
 * and rounds to float32 before calling the fsqrt wrapper. The intermediate
 * round is observable, so it is spelled out rather than folded. */
float BrVec3Length(const BrVec3 *pV)
{
    float sum = pV->x * pV->x + pV->y * pV->y + pV->z * pV->z;
    return sqrtf(sum);
}
