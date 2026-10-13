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
/* the race's weather: 0 sunny, 1 fog, 2 rain, 3 snow, 4 night (MEASUREMENTS.md) */
int  light_game_weather(void);

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
/* 1 when the triangle has a world position (the camera's view or the
 * mirror's, not the backdrop): world[3][3] then holds its corners */
int  light_capture_tri(int view, const LightVtx *a, const LightVtx *b, const LightVtx *c,
                       uint32_t texture, uint32_t state, float world[3][3]);
/* the frame is finished (grBufferSwap) */
void light_capture_swap(void);
/* 1 while the capture records (the flag, or BR_LIGHT_CAPTURE=1) */
int  light_capture_on(void);
/* the main camera's position this frame, from its view x projection; 0 when
 * no 3D was drawn */
int  light_capture_eye(float eye[3]);

#ifdef __cplusplus
}
#endif
#endif
