/* WHAT IT DOES: works out the transform that undoes a given one -- how to get
 * from world space back into an object's own space, for instance.  It is the
 * Graphics Gems II affine inverse: the 3x3 determinant is summed as six
 * signed terms split into positive and negative accumulators, a transform
 * that is singular (determinant 0, or tiny relative to the size of its
 * terms) yields the identity, otherwise the adjugate is scaled by 1/det and
 * the translation row becomes -T * inverse(A).  It returns 1 either way, so
 * the caller cannot tell the two cases apart. */
/* @t3 0x10034B70 2026-09-27 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 694/696 insns 243/244 rows 6+5 regions 5 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 1 2
 * Residue: two x87 choices with the same arithmetic -- the ABS test's
 * `fcom` (original `fld st(0); fcomp`) and which of translation row 2's
 * products is formed first (a + b == b + a).  Dossier and dead list in the
 * header comment below.  Do not reopen before the end-grind (the project rules
 * rule 12). */
/* @t4-pass 0x10034B70 2 2026-09-27 probes 729 bytes 694 insns 243 regions 5 rows 11 census yes  (hand: every plain/helper/named-subset spelling of the three translation rows, 9^3, in the <windows.h> TU; none moves row 2 or the ABS dup) */
/* @t4-pass 0x10034B70 3 2026-09-27 probes 30 bytes 694 insns 243 regions 5 rows 11 census no  (hand: 12 ABS spellings -- macro, inline fn, if/negate, both ternaries, temp, !(x >= 0), 0 - x, *-1 -- and 18 C++ flag sets) */
/* @implements 0x10034B70 glide BrMtxInvert
 * @cpp_symbol _BrMtxInvert
 *
 * Hand-transcribed from the Glide bytes 2026-09-27.  A plain cdecl free
 * function, but a C++ translation unit.  The same source under the C front
 * end spills neither determinant minor (m11*m22, m12*m21): it keeps them with
 * `fst` and multiplies from the register, where the original `fstp`s them and
 * multiplies `m00` by the stack copy.  /TP gives the original's spill, and
 * with <windows.h> as the preamble all six determinant terms and the whole
 * adjugate come out in the original's operand order (C lane best 7+13
 * register-blind; here 1+2).
 *
 * The -T * inverse(A) row:
 *  - row 0 goes through an inline by-value dot helper.  The adjugate values
 *    are then the loaded operand (`fld out[2][0]; fld out[0][0]; fmul
 *    in[3][0]; fld out[1][0]`) and in[3][*] the memory one, as in the
 *    original.  Written flat, VC5 loads in[3][*] instead.
 *  - row 1 names only out[2][1]: the original loads out[0][1] and out[1][1]
 *    ahead of row 0's store and out[2][1] after it.
 *  - row 2 is written flat.
 *
 * RESIDUE, 694/696 B, 6 instructions in two places, no arithmetic
 * difference:
 *  - |det/(pos-neg)| is tested with `fcom` where the original has
 *    `fld st(0); fcomp` (2 B).  Inert: ABS as a macro, an inline function,
 *    if/negate, a ternary either way round, via temp/ratio, `!(x >= 0)`,
 *    `0 - x`, `*-1`, and folded into the condition.
 *  - in row 2 the original forms in[3][1]*out[1][2] first and ours forms
 *    in[3][0]*out[0][2] first.  The sum is the same (a + b == b + a).
 *    Inert: every row 0/1/2 spelling combination (plain, helper, named
 *    subsets: 729 per TU), parens, sign placement, named in[3][*], dead
 *    reads, temp names, param names/types, int/prototype/inline-function
 *    pads 0-160, include sets, and /Ox /Ob2 /G5 /Gi /GR /Gy- /Gf-.
 *    /Op /Oa /Ow /G6 /O1 are far worse.
 */
#define _CRTIMP __declspec(dllimport)

#include <windows.h>   /* the preamble is a codegen input: see the header */

extern "C" {

#include "slice2_21.h"

#define K_0        0.0f                     /* 0x100775F4 */
#define K_1        1.0f                     /* 0x100775F0 */
#define K_EPS_REL  1.0000000036274937e-15f  /* 0x10077604 */
#define BR_ABS(a)  (((a) < K_0) ? -(a) : (a))

/* in[3] . (x, y, z) -- the adjugate column arrives by value. */
static inline float BrDot3(float r0, float r1, float r2,
                           float x, float y, float z)
{
    return r0*x + r1*y + r2*z;
}

#define IN(r,c)  pM->m[r][c]
#define OUT(r,c) pOut->m[r][c]

int BrMtxInvert(BrMat4 *pOut, const BrMat4 *pM)
{
    float det_1;
    float pos, neg, temp, ratio;
    int i, j;
#define ACCUMULATE if (temp >= K_0) pos += temp; else neg += temp;

    pos = neg = K_0;
    temp =  IN(0,0) * IN(1,1) * IN(2,2); ACCUMULATE
    temp =  IN(0,1) * IN(1,2) * IN(2,0); ACCUMULATE
    temp =  IN(0,2) * IN(1,0) * IN(2,1); ACCUMULATE
    temp = -IN(0,2) * IN(1,1) * IN(2,0); ACCUMULATE
    temp = -IN(0,1) * IN(1,0) * IN(2,2); ACCUMULATE
    temp = -IN(0,0) * IN(1,2) * IN(2,1); ACCUMULATE
    det_1 = pos + neg;
    ratio = BR_ABS(det_1 / (pos - neg));

    if (det_1 == K_0 || ratio < K_EPS_REL) {
        for (i = 0; i < 4; i++)
            for (j = 0; j < 4; j++)
                if (i == j) OUT(i,j) = 1.0f; else OUT(i,j) = 0.0f;
        return 1;   /* the singular path reports success as well */
    }

    det_1 = K_1 / det_1;
    OUT(0,0) =   (IN(1,1) * IN(2,2) - IN(1,2) * IN(2,1)) * det_1;
    OUT(1,0) = - (IN(1,0) * IN(2,2) - IN(1,2) * IN(2,0)) * det_1;
    OUT(2,0) =   (IN(1,0) * IN(2,1) - IN(1,1) * IN(2,0)) * det_1;
    OUT(0,1) = - (IN(0,1) * IN(2,2) - IN(0,2) * IN(2,1)) * det_1;
    OUT(1,1) =   (IN(0,0) * IN(2,2) - IN(0,2) * IN(2,0)) * det_1;
    OUT(2,1) = - (IN(0,0) * IN(2,1) - IN(0,1) * IN(2,0)) * det_1;
    OUT(0,2) =   (IN(0,1) * IN(1,2) - IN(0,2) * IN(1,1)) * det_1;
    OUT(1,2) = - (IN(0,0) * IN(1,2) - IN(0,2) * IN(1,0)) * det_1;
    OUT(2,2) =   (IN(0,0) * IN(1,1) - IN(0,1) * IN(1,0)) * det_1;

    OUT(3,0) = - BrDot3(IN(3,0), IN(3,1), IN(3,2), OUT(0,0), OUT(1,0), OUT(2,0));
    {
        float c21 = OUT(2,1);
        OUT(3,1) = - (IN(3,0) * OUT(0,1) + IN(3,1) * OUT(1,1) + IN(3,2) * c21);
    }
    OUT(3,2) = - (IN(3,0) * OUT(0,2) + IN(3,1) * OUT(1,2) + IN(3,2) * OUT(2,2));

    OUT(0,3) = OUT(1,3) = OUT(2,3) = 0.0f;
    OUT(3,3) = 1.0f;
    return 1;
#undef ACCUMULATE
}

} /* extern "C" */
