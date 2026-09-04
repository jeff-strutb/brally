/* br_tri.c -- geometry: triangle predicates.
 *
 * Responsibility: positions, orientations and the arithmetic that moves them.
 * This module answers questions about a triangle in space -- so far, whether a
 * point lies inside one when viewed from a given side. The collision grid asks
 * it once per candidate surface, so it is spatial arithmetic and not part of
 * any one caller's subsystem.
 *
 * Moved out of src/core/slice1_06.c (an address batch) unchanged. The
 * preamble below is carried over verbatim from that file, including the
 * matching-build renames: they decide the set of names the translation unit
 * sees, and trimming them changes the compiler's view of the code.
 */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
/* The original BrOptSave takes no arguments (loose globals in, packed
 * array out); hide the header's port prototype behind a rename so the
 * matching twin can define the real symbol -- the slice5_63.c caller keeps
 * the port signature (cdecl, extra args harmless at run time). */
#define BrOptSave   BrOptSave_hdr
#define BrOptAvailB BrOptAvailB_hdr
#ifdef BR_MATCHING_BUILD
/* The original BrNameListInit is a thiscall ctor with no stack args (vtbl
 * and fill string are fixed); hide the port's 3-arg prototype. */
#define BrNameListInit BrNameListInit_port
#include "slice1_06.h"
#undef BrNameListInit
#else
#include "slice1_06.h"
#endif
#undef BrOptSave
#undef BrOptAvailB
#else
#include "slice1_06.h"
#endif

#include <stdlib.h>
#include <string.h>

/* ==========================================================================
 * 0x1003B940
 * ========================================================================== */

/* The threshold is the float at 0x1008F62C, which is 0.0f. The original
 * compares with `fcomp` and then tests C0 alone, so "not less than" is the
 * accepting condition and an unordered compare (NaN) sets C0 and rejects. */
#define BR06_TRI_EPS 0.0f

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
