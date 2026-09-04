/* br_sin.c -- geometry.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import
 * table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdint.h>

#ifdef BR_MATCHING_BUILD


/* ======================================================================
 * 1. x87 one-liners
 * ====================================================================== */

/* 0x10002240 */
/* WHAT IT DOES: the game's sine, a one-instruction wrapper round the
 * processor's own sine. Used throughout the physics and the camera work. */
/* @implements 0x10002240 d3d BrSinF */
#ifdef _MSC_VER
#pragma intrinsic(sin)
#endif
float BrSinF(float x)
{
    /* The original is fld [esp+4]; fsin; ret -- the x87 sine emitted inline,
     * with no call into the CRT.  `sinf` is a real library function in MSVC
     * 5.0 and is not on the intrinsic list; only the double `sin` is, so that
     * is what has to be written.  Same fix as BrSqrtF below.
     *
     * DEVIATION (port target only): `fsin` rounds at the x87's current
     * precision, 53-bit here (CRT control word 0x027F -- CONVENTIONS.md), and
     * is undefined for |x| >= 2^63, returning the operand untouched with C2
     * set.  Off-MSVC this is libm's double `sin` narrowed to float, which
     * shares the 53-bit working precision but not the 2^63 behaviour.  No
     * call site approaches that limit. */
    return (float)sin(x);
}

/* 0x10002250 */
/* WHAT IT DOES: the game's square root, a one-instruction wrapper round the
 * processor's own. Used everywhere a distance or a vector length is needed. */
/* @implements 0x10002250 d3d BrSqrtF */
#ifdef _MSC_VER
#pragma intrinsic(sqrt)
#endif
float BrSqrtF(float x)
{
    /* The original is three instructions -- fld [esp+4]; fsqrt; ret -- so the
     * square root is the x87 FSQRT emitted inline, not a call into the CRT.
     * `sqrtf` is a real function in MSVC 5.0's library; only the double
     * `sqrt` is on the intrinsic list, so that is what has to be written.
     * No narrowing store appears because without /Op MSVC leaves the result
     * in ST(0) at the x87's working precision. */
    return (float)sqrt(x);
}

#endif /* BR_MATCHING_BUILD */
