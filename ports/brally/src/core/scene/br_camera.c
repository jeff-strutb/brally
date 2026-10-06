/* br_camera.c -- the wedge of the world the camera can see.
 *
 * RESPONSIBILITY: what is in the world and where -- the view volume other
 * code asks "is this worth drawing?" against.
 *
 * Moved here out of src/brally/core/slice2_19.c (an address batch, not a module).
 * The camera globals it fills stay defined there; slice2_19.h declares them.
 */
#include "br_vec.h"   /* br_globals: its objects */
#include "slice2_19.h"
#include "br_camwide.h"

/* 0x10033CB1 */
/* WHAT IT DOES: works out the wedge of the world the camera can currently
 * see -- the four corners of the view at a given distance ahead, pulled back
 * to three-quarters of that distance -- so that other code can ask whether
 * something is worth drawing. In one of the game's modes the view is squeezed
 * to half its height, which is what a split screen needs. */
/* @implements 0x10033CB1 d3d BrCamFrustumBuild */
void BrCamFrustumBuild(const BrCamBasis *pCam, float a2, float a3,
                       float a4, float a5)
{
    float a, b;

    /* Braced: /Od otherwise peepholes `a = ...; b = a * ...` into one x87
     * chain (fst keeps a on the stack); the original stores and reloads. */
    { a = BrSinF(a2) * a3; }
    { b = a * a5 / a4; }
    if (g_BrCamMode == 2)
        b = b / g_BrK08F514;

    BrVec3Copy(&(*(BrVec3 *)&g_aBrSpanPt), &pCam->eye);

    /* out = a + b*s, so centre = eye + fwd*a3. */
    BrVec3MulAdd(&g_BrCamCentre, &(*(BrVec3 *)&g_aBrSpanPt), &pCam->fwd, a3);

    BrVec3Scale(&g_BrCamExtentR, &pCam->right, a);
    BrVec3Scale(&g_BrCamExtentU, &pCam->up,    b);

    BrVec3Copy(&g_BrCamCentreCopy, &g_BrCamCentre);

    BrVec3Add   (&g_BrCamCorner0, &g_BrCamCentre, &g_BrCamExtentR);
    BrVec3AddTo (&g_BrCamCorner0, &g_BrCamExtentU);   /* C + R + U */

    BrVec3Add     (&g_BrCamCorner3, &g_BrCamCentre, &g_BrCamExtentR);
    BrVec3SubFrom (&g_BrCamCorner3, &g_BrCamExtentU); /* C + R - U */

    BrVec3Sub   (&g_BrCamCorner1, &g_BrCamCentre, &g_BrCamExtentR);
    BrVec3AddTo (&g_BrCamCorner1, &g_BrCamExtentU);   /* C - R + U */

    BrVec3Sub     (&g_BrCamCorner2, &g_BrCamCentre, &g_BrCamExtentR);
    BrVec3SubFrom (&g_BrCamCorner2, &g_BrCamExtentU); /* C - R - U */

    /* (c - eye)*0.75 + eye, in the original's order. */
    BrVec3Lerp(&g_BrCamCorner1, &g_BrCamCorner1, &(*(BrVec3 *)&g_aBrSpanPt), 0.75f);
    BrVec3Lerp(&g_BrCamCorner2, &g_BrCamCorner2, &(*(BrVec3 *)&g_aBrSpanPt), 0.75f);
    BrVec3Lerp(&g_BrCamCorner0, &g_BrCamCorner0, &(*(BrVec3 *)&g_aBrSpanPt), 0.75f);
    BrVec3Lerp(&g_BrCamCorner3, &g_BrCamCorner3, &(*(BrVec3 *)&g_aBrSpanPt), 0.75f);

    g_BrCamDist  = a3;
    g_BrCamFovIn = a2;
}

/* Port: the culling wedge for a race view widened to fill the window
 * (br_camwide.h, as the wasm lane's native/aspect.m): BrCamFrustumBuild's,
 * its half-width grown by kx and half-height by ky, the four corners rebuilt
 * as it builds them (centre +- right +- up, pulled three-quarters of the way
 * back toward the eye). */
void BrCamFrustumWiden(void)
{
    static const float sr[4] = { 1, -1, -1, 1 }, su[4] = { 1, 1, -1, -1 };
    BrVec3 *corner[4] = { &g_BrCamCorner0, &g_BrCamCorner1, &g_BrCamCorner2, &g_BrCamCorner3 };
    const BrVec3 *eye = (const BrVec3 *)&g_aBrSpanPt[0];
    float kx, ky;
    int i;
    plat_view_scale(&kx, &ky);
    if (kx <= 1.0f && ky <= 1.0f)
        return;
    g_BrCamExtentR.x *= kx; g_BrCamExtentR.y *= kx; g_BrCamExtentR.z *= kx;
    g_BrCamExtentU.x *= ky; g_BrCamExtentU.y *= ky; g_BrCamExtentU.z *= ky;
    for (i = 0; i < 4; i++) {
        BrVec3 v;
        v.x = g_BrCamCentre.x + sr[i] * g_BrCamExtentR.x + su[i] * g_BrCamExtentU.x;
        v.y = g_BrCamCentre.y + sr[i] * g_BrCamExtentR.y + su[i] * g_BrCamExtentU.y;
        v.z = g_BrCamCentre.z + sr[i] * g_BrCamExtentR.z + su[i] * g_BrCamExtentU.z;
        corner[i]->x = (v.x - eye->x) * 0.75f + eye->x;
        corner[i]->y = (v.y - eye->y) * 0.75f + eye->y;
        corner[i]->z = (v.z - eye->z) * 0.75f + eye->z;
    }
}
