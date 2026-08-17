/* br_collrespsolve.c -- see br_collrespsolve.h.  The OBB collision response.
 *
 * Transcribed from orig/BRGlide.dll and pinned to tools/x87emu.py golden
 * vectors.  Each function carries the address of what it is.
 */
#include <math.h>
#include <string.h>

#include "br_collrespsolve.h"

/* 0x117787F0..FC and 0x117781A0..A8. */
BrCrPlaneState g_brCrPlane;

/* 0x10077B84 (1/3), 0x10077AC8 (0.5) -- the two literals the mode-2 arm uses. */
#define BR_CR_THIRD  (1.0f / 3.0f)
#define BR_CR_HALF   0.5f

/* ------------------------------------------------------------------ *
 * 0x10067470 -- contact-plane resolver.
 *
 * WHAT IT DOES: turns one candidate contact into a plane and writes the plane
 * normal scaled by how far the point is from it.  There are two ways in.  The
 * common one (mode != 2) is handed the plane normal outright.  The box-face
 * one (mode == 2) has to CHOOSE the face: it averages the three box-space
 * triangle vertices and picks the axis on which that centroid is SMALLEST --
 * the face the triangle lies most flush against -- and uses that box face's
 * outward normal.  The N64 sibling calls these two paths "Cube Edge to
 * Triangle Face" and "Triangle Edge to CubeFace".
 *
 * Two things here are preserved quirks, both confirmed against the bytes:
 *
 *  - The sign of the chosen box normal is taken from the centroid's X
 *    component, NOT from the component on the winning axis.  The original
 *    carries cx on the FPU stack through the entire argmin tournament and
 *    signs by whatever is left in st0, which is always cx.  A NaN cx takes the
 *    negative arm (the compare's unordered result), so it maps to -0.5.
 *
 *  - The argmin is the original's exact >= tournament, so ties resolve the way
 *    its `fcom`/`jae`-shaped branches do (a tie keeps the earlier axis).
 * ------------------------------------------------------------------ */
/* @implements 0x10067470 glide BrCrPlaneResolve */
void BrCrPlaneResolve(const BrVec3 *pExt, const BrVec3 *pA, float planeD,
                      const BrVec3 *pEdgeN, const BrVec3 aVerts[3])
{
    float nx, ny, nz;     /* the plane normal V fed to the shared tail */
    float d, s;

    if (g_brCrPlane.modeFC == 2u) {
        /* --- mode 2: build the box-face normal from the triangle centroid. */
        float cx, cy, cz, sign;
        int   k;

        /* centroid, in the original's add order ((v0 + v2) + v1) * 1/3 */
        cx = ((aVerts[0].x + aVerts[2].x) + aVerts[1].x) * BR_CR_THIRD;
        cy = ((aVerts[0].y + aVerts[2].y) + aVerts[1].y) * BR_CR_THIRD;
        cz = ((aVerts[0].z + aVerts[2].z) + aVerts[1].z) * BR_CR_THIRD;

        /* argmin |c| via the exact >= tournament: |cx| vs |cy| first, then the
         * survivor's partner against |cz|.  k is the axis the normal lands on. */
        if (fabsf(cx) >= fabsf(cy))
            k = (fabsf(cy) >= fabsf(cz)) ? 2 : 1;
        else
            k = (fabsf(cx) >= fabsf(cz)) ? 2 : 0;

        /* sign from cx (the quirk); NaN cx -> -0.5 */
        sign = (cx < 0.0f || isnan(cx)) ? -BR_CR_HALF : BR_CR_HALF;

        nx = ny = nz = 0.0f;
        if (k == 0)      nx = sign;
        else if (k == 1) ny = sign;
        else             nz = sign;

        /* the tail's dot: for a one-axis normal this is exact regardless of
         * summation order. */
        d = (pA->y * ny + pA->z * nz) + pA->x * nx;

        /* side-effect state the walker reads next.  .x is scaled by ext.x*ext.y
         * (two stores in the original), .y/.z keep the raw +-0.5, and modeFC is
         * multiplied by ext.z as a FLOAT (its int mode value reinterpreted). */
        g_brCrPlane.normal.x = pExt->y * (pExt->x * nx);
        g_brCrPlane.normal.y = ny;
        g_brCrPlane.normal.z = nz;
        {
            float fc;
            memcpy(&fc, &g_brCrPlane.modeFC, sizeof fc);
            fc = pExt->z * fc;
            memcpy(&g_brCrPlane.modeFC, &fc, sizeof fc);
        }
    } else {
        /* --- mode != 2: the plane normal is handed in directly. */
        nx = pEdgeN->x; ny = pEdgeN->y; nz = pEdgeN->z;
        d = (pA->y * ny + pA->z * nz) + pA->x * nx;   /* dot(pA, pEdgeN) */
    }

    /* shared tail: out = (planeD - dot(pA, V)) * pA */
    s = planeD - d;
    g_brCrPlane.out.x = s * pA->x;
    g_brCrPlane.out.y = s * pA->y;
    g_brCrPlane.out.z = s * pA->z;
}
