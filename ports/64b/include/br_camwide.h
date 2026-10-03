/* br_camwide.h -- port: the race camera widened to fill a window of any shape
 * (br_cammatrix.c, br_camera.c).  Not in the original. */
#ifndef BR_CAMWIDE_H
#define BR_CAMWIDE_H

#include "slice2_19.h"
#include "br_platview.h"

/* BrCamMatrixSetup for a race view stretched over the window (glide.c's
 * screen map): the view keeps the game's vertical angle and sees more to
 * the sides in a wide window (Hor+), keeps its horizontal angle and sees
 * more above and below in a tall one (Vert+).  As BrCamMatrixSetup when the
 * view is not widened. */
void BrCamMatrixSetupWide(const BrCamBasis *pCam, float angle, float far_, float w, float h);
/* after BrCamFrustumBuild: the culling wedge grown to the same widened view,
 * so the edges of a wide view are drawn, not left empty */
void BrCamFrustumWiden(void);

#endif
