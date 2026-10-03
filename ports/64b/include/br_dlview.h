/* br_dlview.h -- port: which view a display-list triangle belongs to.
 *
 * The screen map (platform/common/glide.c) places each kind its own way on a
 * window of any shape, as the wasm lane does (its native/render.m): 1 flat
 * 2D, drawn through an orthographic projection (w does not depend on the
 * position: the HUD, text); 2 the rear-view mirror, told by its viewport's
 * height -- BrFrameDraw draws it a quarter of its width tall, at most 62 of
 * 480 pixels, where the camera's own view is never under 120 (split screen
 * halves it); 0 the camera.  Not in the original. */
#ifndef BR_DLVIEW_H
#define BR_DLVIEW_H

#include <math.h>
#include "br_mat.h"        /* DAT_105ccd00, the projection the list loaded */
#include "br_platview.h"

static inline int br_dl_view(void)
{
    const float *m = (const float *)&DAT_105ccd00;
    if (m[3] == 0.0f && m[7] == 0.0f && m[11] == 0.0f)
        return 1;
    return fabsf(DAT_105ccfdc) < 48.0f ? 2 : 0;
}

/* a display-list triangle: its view, then Glide */
#define BR_DL_TRI(a, b, c)  (plat_glide_view(br_dl_view()), grDrawTriangle((a), (b), (c)))

#endif
