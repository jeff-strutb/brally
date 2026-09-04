/* br_camera.c -- the wedge of the world the camera can see.
 *
 * RESPONSIBILITY: what is in the world and where -- the view volume other
 * code asks "is this worth drawing?" against.
 *
 * Moved here out of src/core/slice2_19.c (an address batch, not a module).
 * The camera globals it fills stay defined there; slice2_19.h declares them.
 */
#include "slice2_19.h"

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

#ifdef BR_MATCHING_BUILD
    /* Braced: /Od otherwise peepholes `a = ...; b = a * ...` into one x87
     * chain (fst keeps a on the stack); the original stores and reloads. */
    { a = BrSub10002240(a2) * a3; }
    { b = a * a5 / a4; }
#else
    a = BrSub10002240(a2) * a3;
    b = a * a5 / a4;
#endif
    if (g_BrCamMode == 2)
        b = b / g_BrK08F514;

    BrVec3Copy(&g_BrCamEye, &pCam->eye);

    /* out = a + b*s, so centre = eye + fwd*a3. */
    BrVec3MulAdd(&g_BrCamCentre, &g_BrCamEye, &pCam->fwd, a3);

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
    BrVec3Lerp(&g_BrCamCorner1, &g_BrCamCorner1, &g_BrCamEye, 0.75f);
    BrVec3Lerp(&g_BrCamCorner2, &g_BrCamCorner2, &g_BrCamEye, 0.75f);
    BrVec3Lerp(&g_BrCamCorner0, &g_BrCamCorner0, &g_BrCamEye, 0.75f);
    BrVec3Lerp(&g_BrCamCorner3, &g_BrCamCorner3, &g_BrCamEye, 0.75f);

    g_BrCamDist  = a3;
    g_BrCamFovIn = a2;
}
