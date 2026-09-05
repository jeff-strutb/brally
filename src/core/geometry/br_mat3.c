/* br_mat3.c -- geometry.
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

#include "slice3_44.h"   /* BrMat3 / BrMat4 / BrVec3, for the routines moved
                          * here out of src/core/slice3_44.c                */

#ifdef BR_MATCHING_BUILD


/* 0x10074B20 */
/* WHAT IT DOES: subtracts one 3x3 matrix from another, all nine elements. An
 * earlier reading of this had it covering only three, described as a
 * faithfully preserved bug of the original; that was a misreading, and the
 * note in the code explains how it was caught. */
/* @implements 0x10074B20 d3d BrMat3Sub */
void BrMat3Sub(float *pOut, const float *pA, const float *pB)
{
    int i, j;

    /* NINE elements: a 3x3 matrix subtract, nested 3x3 so the cursor
     * walks continuously (the outer body reloads the pA/pOut offsets
     * but does not reset the pB walker).
     *
     * This was previously modelled as three subtractions performed three
     * times, and recorded as a PRESERVED BUG of the original -- "only the
     * inner loop advances any pointer, and the outer one resets it".
     *
     * The outer loop does NOT reset it. The reset `mov eax, edi` sits at
     * 0x1006DD98 and the outer loop's target is 0x1006DD9A -- one instruction
     * LATER. So the cursor advances continuously across all three passes and
     * the function covers nine floats, exactly once each.
     *
     * 0x10065C80 settles it independently: it passes a 3x3 identity scaled by
     * 1/mass, which is meaningless to a routine that only touches three
     * elements.
     *
     * Worth naming the shape of the old error, because it is the kind that
     * survives review: a misreading dressed as a faithfully preserved bug.
     * It looks like diligence, it explains an oddity, and nobody re-derives
     * it. A claim that the original is wrong should be held to a HIGHER
     * standard than a claim that it is right, not a lower one. */
    for (i = 0; i < 3; ++i)
        for (j = 0; j < 3; ++j)
            pOut[3 * i + j] = pA[3 * i + j] - pB[3 * i + j];
}

#endif /* BR_MATCHING_BUILD */

/* 0x10074830 */
/* WHAT IT DOES: rotates a 3D vector by a 3x3 matrix. Used all through the
 * car physics, where the same quantity has to be moved between the world's
 * frame of reference and the car's own. */
/* @implements 0x1006DA90 glide BrMat3MulVec3 */
void BrMat3MulVec3(BrVec3 *pOut, const BrMat3 *pM, const BrVec3 *pV)
{
    const float *v = &pV->x;
    float *o = &pOut->x;
    int i, k;

    for (i = 0; i < 3; i++) {
        /* the original zeroes the destination slot and then reloads it once
         * per term, so every partial sum is rounded to float */
        o[i] = 0.0f;
        for (k = 0; k < 3; k++)
            o[i] += pM->m[3 * i + k] * v[k];
    }
}

/* 0x100749D0 */
/* WHAT IT DOES: builds the little 3x3 matrix that stands in for a cross
 * product: multiplying a vector by it gives the same answer as crossing it
 * with the vector this was built from. The physics uses it to turn "a force
 * applied at this offset" into a twist. */
/* @implements 0x100749D0 d3d BrMat3Skew */
/* @n64 0x80258DB0 exact */
void BrMat3Skew(BrMat3 *pOut, const BrVec3 *pV)
{
    /* The three diagonal zeros are written FIRST, and in DESCENDING order --
     * `xor ecx,ecx; [eax+0x20]; [eax+0x10]; [eax]` -- not interleaved in index
     * order with the rest. Interleaved, the zero register has to stay live
     * across the whole body, so VC5 spills into esi and the function grows a
     * push/pop pair (59 -> 61 bytes). Grouped, the zero dies before pV is
     * loaded and ecx is reused for pV, which is what the original does.
     * Ascending order for the group is 7 bytes off; descending is exact. */
    pOut->m[8] =  0.0f;
    pOut->m[4] =  0.0f;
    pOut->m[0] =  0.0f;
    pOut->m[1] = -pV->z;
    pOut->m[2] =  pV->y;
    pOut->m[3] =  pV->z;
    pOut->m[5] = -pV->x;
    pOut->m[6] = -pV->y;
    pOut->m[7] =  pV->x;
}

/* 0x10074A90 */
/* WHAT IT DOES: pulls the rotation part out of a full 4x4 transform,
 * dropping the translation and leaving a compact 3x3. */
/* @implements 0x10074A90 d3d BrMat4ToMat3 */
/* @n64 0x80258E64 located */
void BrMat4ToMat3(BrMat3 *pOut, const BrMat4 *pSrc)
{
    int i, j;

    for (i = 0; i < 3; ++i)
        for (j = 0; j < 3; ++j)
            pOut->m[3 * i + j] = pSrc->m[i][j];
}

/* 0x10074A50 */
/* WHAT IT DOES: the same extraction but flipped along the diagonal, which
 * for a pure rotation is the same as reversing it -- so this gives the
 * matrix that undoes the transform's rotation. */
/* @implements 0x10074A50 d3d BrMat4ToMat3Transposed */
/* @n64 0x80258EAC located */
void BrMat4ToMat3Transposed(BrMat3 *pOut, const BrMat4 *pSrc)
{
    int i, j;

    for (i = 0; i < 3; ++i)
        for (j = 0; j < 3; ++j)
            pOut->m[i + 3 * j] = pSrc->m[i][j];
}

/* 0x10074A10 */
/* WHAT IT DOES: extracts the rotation part of a 4x4 transform twice at once,
 * once flipped and once straight, because the physics needs both to move
 * quantities into the body's frame and back out again. */
/* @implements 0x10074A10 d3d BrMat4ToMat3Both */
/* @n64 0x80258E04 located */
void BrMat4ToMat3Both(BrMat3 *pTransposed, BrMat3 *pStraight,
                      const BrMat4 *pSrc)
{
    int i, j;

    for (i = 0; i < 3; ++i) {
        for (j = 0; j < 3; ++j) {
            /* ONE chained assignment, not two statements.  The original loads
             * the source element once and spends it twice -- `fld [ecx]`,
             * `fst [edx]` (transposed, kept), `fstp [eax-4]` (straight) -- and
             * that fst/fstp pair is what a chain compiles to.  Two separate
             * assignments from the same expression let VC5 copy through
             * integer registers instead, which costs three extra
             * instructions. */
            pStraight->m[3 * i + j] = pTransposed->m[i + 3 * j] = pSrc->m[i][j];
        }
    }
}

/* 0x10074B70 */
/* WHAT IT DOES: builds the matrix that takes a point out of a box's own frame
 * and into the world: the box's rotation transposed, each column stretched by
 * that axis's scale, and the translation carried across as the source's
 * position rotated back through the transpose and negated. Callers hand it
 * the SAME frame as source and destination -- see include/br_collresp.h --
 * so the two stores per element are not a redundant pair. */
/* @implements 0x1006DDD0 glide BrMat4BuildScaledTransposed */
void BrMat4BuildScaledTransposed(const BrMat4 *pS, BrMat4 *pOut,
                                 const BrVec3 *pScale)
{
    BrVec3 negT;
    int i, k;

    for (i = 0; i < 3; ++i) {
        for (k = 0; k < 3; ++k) {
            /* TWO statements, and the compound `*=` is the second of them.
             * The plain store must really happen because pScale may alias
             * pOut, which is why the original reads the scale AFTER the
             * store; the `*=` then reuses the value it just stored (`fst`,
             * not `fstp`) instead of reloading it. */
            pOut->m[i][k] = pS->m[k][i];
            pOut->m[i][k] *= (&pScale->x)[k];
        }
        pOut->m[i][3] = 0.0f;
    }

    pOut->m[3][3] = 1.0f;
    negT.x = -pS->m[3][0];
    negT.y = -pS->m[3][1];
    negT.z = -pS->m[3][2];
    BrMat4MulVec3Transposed((BrVec3 *)&pOut->m[3][0], pOut, &negT);
}
