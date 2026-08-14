/* br_vecd.h -- double-precision 3-vector, decompiled from BRD3D.dll.
 *
 * The engine has a second vector type using 8-byte components (the original
 * loads with `fld qword` at strides 0/8/0x10, versus `fld dword` at 0/4/8 in
 * br_vec.h). Almost certainly the physics/simulation path, where single
 * precision would drift. Kept as a distinct type rather than folded into
 * BrVec3 -- conflating them would silently change the arithmetic.
 */
#ifndef BR_VECD_H
#define BR_VECD_H

typedef struct BrVec3d { double x, y, z; } BrVec3d;

/* 0x10030640  returns a . b   (double precision) */
double BrVec3dDot(const BrVec3d *pA, const BrVec3d *pB);

/* 0x100305B0  returns |v|^2 */
double BrVec3dLenSq(const BrVec3d *pV);
/* 0x100305F0  returns |v| */
double BrVec3dLen(const BrVec3d *pV);

/* 0x10030600  normalise IN PLACE; returns pV.
 * If the length is exactly 0.0 the vector is left untouched (the original
 * compares against a 0.0 constant at 0x1008F478 and skips the divides), so a
 * zero vector stays zero instead of producing NaNs. */
BrVec3d *BrVec3dNormalise(BrVec3d *pV);

/* 0x10030670  out = a x b
 *
 * WARNING: the OUTPUT IS THE THIRD ARGUMENT here, unlike BrVec3Cross in
 * br_vec.h which takes the destination first. Verified from the original:
 * edx=arg1=a, ecx=arg2=b, eax=arg3=out. Do not "harmonise" these. */
void BrVec3dCross(const BrVec3d *pA, const BrVec3d *pB, BrVec3d *pOut);

/* 0x10030DE0  pack a normalised value into a signed byte.
 *
 *     clamp(floor(0.5 + 128 * v), -128, 127)
 *
 * The constants are -128.0 at 0x1008F4C0 and 0.5 at 0x1008F4C8; the original
 * computes `0.5 - (v * -128.0)` via fsubr, which is the same thing. Rounding
 * is floor-of-(x+0.5), i.e. halves go up, NOT banker's rounding.
 *
 * This is N64 vertex-normal packing: a unit component in [-1,1] becomes an
 * s8. Note 1.0 maps to 128 and therefore clamps to 127, so the encoding is
 * very slightly asymmetric -- faithful to the original. */
signed char BrPackNormalByte(double v);

#endif /* BR_VECD_H */
