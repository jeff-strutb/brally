/* br_platview.h: what the game's drawing tells the screen map (glide.c), so
 * a window of any shape is filled the way the wasm lane fills it. */
#ifndef BR_PLATVIEW_H
#define BR_PLATVIEW_H

#ifdef __cplusplus
extern "C" {
#endif

/* this frame is a race frame (BrFrameDraw): its views fill the window */
void plat_glide_wide(void);
/* the next draw's view: 0 the camera's perspective, 1 flat 2D (an
 * orthographic projection: the HUD, text), 2 the rear-view mirror.  Each
 * draw takes it and leaves 1, the default for what is not a display list. */
void plat_glide_view(int view);
/* how far a race view is widened (kx) or heightened (ky) past the game's
 * shape to fill the window: 1, 1 when it is not (BR_FLAG_ANY_ASPECT off) */
void plat_view_scale(float *kx, float *ky);

#ifdef __cplusplus
}
#endif
#endif
