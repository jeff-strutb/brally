/* light.h: the Remastered lighting pipeline (docs/brally/remaster/lighting/
 * DESIGN.md), behind BR_FLAG_LIGHTING.  Platform code: it reads the game's
 * state and never writes it. */
#ifndef BR_LIGHT_H
#define BR_LIGHT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- the game's state (light_game.c, built with the core's flags) ---------- */
typedef struct LightCamera {
    float vp[16];            /* view x projection, row vectors: clip = [x y z 1] vp */
    float sx, ox, sy, oy;    /* clip to the game's pixels: x = cx / cw * sx + ox */
} LightCamera;
void light_game_camera(LightCamera *c);

/* ---- the capture (light_capture.c) --------------------------------------------
 * glide.c hands over every display-list triangle of the camera's view (0) or
 * the rear-view mirror's (2) as Glide receives it: the game's pixels, 1/w,
 * colour and texture coordinates; the capture rebuilds where it is in the
 * world. */
typedef struct LightVtx {
    float x, y;              /* the game's pixels */
    float oow;               /* 1/w */
    float r, g, b, a;        /* 0..255 */
    float s, t;              /* texels (s/w, t/w already divided back by 1/w) */
} LightVtx;
void light_capture_tri(int view, const LightVtx *a, const LightVtx *b, const LightVtx *c,
                       uint32_t texture, uint32_t state);
/* the frame is finished (grBufferSwap) */
void light_capture_swap(void);
/* 1 while the capture records (the flag, or BR_LIGHT_CAPTURE=1) */
int  light_capture_on(void);

#ifdef __cplusplus
}
#endif
#endif
