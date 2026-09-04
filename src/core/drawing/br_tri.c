/* br_tri.c -- drawing.
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


/* WHAT IT DOES: test whether a point lies inside a triangle, given a
 * reference direction that says which side of the triangle counts as the
 * front. Walks the three edges and requires the point to be on the inward
 * side of every one. Used by the collision grid to decide whether a wheel is
 * over a given piece of track surface. */
/* @implements 0x1003B940 d3d BrTriContainsPoint */
/* @n64 0x8022591C located */
int BrTriContainsPoint(const BrVec3 *pPt, const BrVec3 *pA, const BrVec3 *pB,
                       const BrVec3 *pC, const BrVec3 *pRef)
{
    /* SIX distinct Vec3 locals (frame 0x48), not three reused ones: a
     * fresh edge per test, a shared n, and two point-deltas.  Reusing
     * edge/toPt shrinks the frame to 0x24 and shifts every slot. */
    BrVec3 n, toPtB, edge1, edge2, edge3, toPtA;

    /* Edge A->B, paired with pPt - pB. */
    BrVec3Sub(&edge1, pB, pA);
    BrVec3Sub(&toPtB, pPt, pB);
    BrVec3Cross(&n, &edge1, &toPtB);
    if (!(BrVec3Dot(&n, pRef) >= BR06_TRI_EPS)) {
        return 0;
    }

    /* Edge B->C, paired with the same pPt - pB kept live. */
    BrVec3Sub(&edge2, pC, pB);
    BrVec3Cross(&n, &edge2, &toPtB);
    if (!(BrVec3Dot(&n, pRef) >= BR06_TRI_EPS)) {
        return 0;
    }

    /* Edge C->A, paired with pPt - pA. */
    BrVec3Sub(&edge3, pA, pC);
    BrVec3Sub(&toPtA, pPt, pA);
    BrVec3Cross(&n, &edge3, &toPtA);
    if (!(BrVec3Dot(&n, pRef) >= BR06_TRI_EPS)) {
        return 0;
    }

    return 1;
}

#endif /* BR_MATCHING_BUILD */
