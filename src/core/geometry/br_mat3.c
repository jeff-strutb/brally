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
