/* rm_view.h -- Remastered: how far a race view sees (the app's Remaster
 * Options, View Distance; BR_FLAG_VIEW_UNLIMITED).  Not in the original.
 *
 * The original cuts every view off at a fixed distance (DAT_100aa040, 400;
 * a split view 0.65 of it, the mirror 0.2 for its culling and 0.3 for its
 * lens): the camera's culling wedge (BrCamFrustumBuild, BrSpanBuildHull)
 * reaches no further, and the far plane (BrCamMatrixSetup) clips there, so
 * a landmark appears all at once as the wedge reaches it.  Unlimited puts
 * both past the whole world: its grid is 64 cells of 32 a side (2048, 2896
 * corner to corner).  The fog is the game's own either way: Glide's table
 * by distance, not by the far plane. */
#ifndef RM_VIEW_H
#define RM_VIEW_H

#include "br_flags.h"

#define RM_VIEW_FAR 4096.0f

/* the view's far distance: the game's, or at Unlimited past the world */
static inline float rm_view_far(float far_)
{
    return plat_flag(BR_FLAG_VIEW_UNLIMITED) ? RM_VIEW_FAR : far_;
}

#endif
