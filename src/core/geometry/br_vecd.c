/* br_vecd.c -- double-precision vector math. See br_vecd.h.
 *
 * 0x10030640: the original multiplies y and z first, then x, and sums
 * (z*z + y*y) + x*x. Floating-point addition is not associative, so the
 * summation order is preserved exactly rather than written left to right.
 */
#include "br_vecd.h"

#include <math.h>

double BrVec3dDot(const BrVec3d *pA, const BrVec3d *pB)
{
    double zz = pB->z * pA->z;
    double yy = pB->y * pA->y;
    double xx = pB->x * pA->x;
    return (zz + yy) + xx;
}

/* 0x100305B0 -- spills y and z to stack scratch then multiplies them back,
 * summing (z*z + y*y) + x*x, same ordering as the dot product. */
/* WHAT IT DOES: the squared length of a 3D vector in double precision,
 * summed in the original's own order because floating-point addition is not
 * associative and the order is visible in the last digit. */
/* @implements 0x100305B0 d3d BrVec3dLenSq */
double BrVec3dLenSq(const BrVec3d *pV)
{
    double zz = pV->z * pV->z;
    double yy = pV->y * pV->y;
    double xx = pV->x * pV->x;
    return (zz + yy) + xx;
}

double BrVec3dLen(const BrVec3d *pV)
{
    return sqrt(BrVec3dLenSq(pV));
}

/* 0x10030600 -- note the guard is an exact compare against 0.0, not an
 * epsilon. Very small non-zero lengths still divide, and can blow up. That is
 * the original behaviour and callers may depend on the resulting infinities
 * being visible rather than silently clamped. */
/* WHAT IT DOES: scales a double-precision 3D vector to unit length. The zero
 * guard is an exact test against zero, not a tolerance, so a very short but
 * non-zero vector still divides and can produce enormous results -- which is
 * the original's behaviour and callers may be relying on seeing it. */
/* @implements 0x10030600 d3d BrVec3dNormalise */
BrVec3d *BrVec3dNormalise(BrVec3d *pV)
{
    double len = BrVec3dLen(pV);

    if (len != 0.0) {
        pV->x /= len;
        pV->y /= len;
        pV->z /= len;
    }
    return pV;
}

/* 0x10030670 -- destination is the THIRD argument; see br_vecd.h. */
/* WHAT IT DOES: the cross product of two 3D vectors: the direction at right
 * angles to both, which is how the game gets a surface's facing from two of
 * its edges. Note the answer goes into the third argument, not the first. */
/* @implements 0x10030670 d3d BrVec3dCross */
void BrVec3dCross(const BrVec3d *pA, const BrVec3d *pB, BrVec3d *pOut)
{
    double x = pA->y * pB->z - pA->z * pB->y;
    double y = pA->z * pB->x - pA->x * pB->z;
    double z = pA->x * pB->y - pA->y * pB->x;
    pOut->x = x; pOut->y = y; pOut->z = z;
}

signed char BrPackNormalByte(double v)
{
    double t = floor(0.5 + 128.0 * v);

    if (t < -128.0)
        return -128;
    if (t > 127.0)
        return 127;
    return (signed char)t;
}
