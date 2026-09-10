/* br_vecd.c -- double-precision vector math. See br_vecd.h.
 *
 * 0x10030640: the original multiplies y and z first, then x, and sums
 * (z*z + y*y) + x*x. Floating-point addition is not associative, so the
 * summation order is preserved exactly rather than written left to right.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "br_vecd.h"

#include <math.h>

/* WHAT IT DOES: the cross product in double precision -- the direction
 * perpendicular to two others, which is how surface normals and sideways
 * axes are built. The double-precision twin of BrVec3Cross; note the output
 * is the LAST argument here, not the first. Each component is stored the
 * moment it is computed, so pOut may not alias either input. */
/* @t4-pass 0x1001DD00 1 2026-09-10 probes 10 bytes 65 insns 25 regions 3 rows 4 census no  (hand, fn.py variants: four commutative operand orders, six TU positions -- position IS the lever, top-of-TU took REGNORM 8+8 to 0+0; every other slot is worse) */
/* @t4-pass 0x1001DD00 2 2026-09-10 probes 10 bytes 65 insns 25 regions 3 rows 4 census yes  (hand, fn.py variants: parens, negated form, element pointers, indexed out, value copies, zyx/yzx store order, cast, sub-temp, struct-then-copy -- all inert or worse; census below) */
/* @t3 0x1001DD00 2026-09-10 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 65/65 insns 25/25 rows 2+2 regions 3 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 1 2
 * CENSUS (all 25 instruction pairs, 2026-09-10): 23 of 25 are identical up to
 * ONE register relabel -- pA and pOut swap eax and edx (pB is ecx in both), so
 * every differing byte is a ModRM base field. The remaining pair is the
 * commutative fold on the x component's first product: orig `fld b.z / fmul
 * a.y`, ours `fld a.y / fmul b.z`, the crossed quad t3.py classify already
 * cancels. No stack slots, no spills, no uncompared code outside the 4 B the
 * key-3 walk skips. Dead probes: the two ledger lines above.
 * Do not reopen before the end-grind (CLAUDE.md rule 12). */
/* @implements 0x1001DD00 glide BrVec3dCross */
/* @implements 0x10030670 d3d BrVec3dCross */
void BrVec3dCross(const BrVec3d *pA, const BrVec3d *pB, BrVec3d *pOut)
{
    /* POSITION IS THE MATCH, not the spelling. At the head of the TU the body
     * is 65/65 B and 25/25 instructions with REGNORM 0+0; anywhere else below
     * it the register assignment rotates further and REGNORM goes to 4+4 or
     * 8+8. Orig fstp's each component as it is computed; named temps add
     * integer copies and bloat 65 B to 89 B. */
    pOut->x = pA->y * pB->z - pA->z * pB->y;
    pOut->y = pA->z * pB->x - pA->x * pB->z;
    pOut->z = pA->x * pB->y - pA->y * pB->x;
}

/* WHAT IT DOES: the dot product in double precision -- how much two
 * directions agree. Positive means roughly the same way, zero means at right
 * angles, negative means opposing. The double-precision twin of BrVec3Dot,
 * used by the camera/orientation code where the float version's last bits
 * are not good enough. */
/* @implements 0x10030640 d3d BrVec3dDot */
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

signed char BrPackNormalByte(double v)
{
    double t = floor(0.5 + 128.0 * v);

    if (t < -128.0)
        return -128;
    if (t > 127.0)
        return 127;
    return (signed char)t;
}
