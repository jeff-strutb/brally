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
/* a car as the race has it: its frame (rows of its body matrix), velocity,
 * the drive torque (negative: driving backwards) and brake torque (negative
 * while held) its physics applies, its control word (0x20000: reverse), and
 * its model's extent in its own frame */
typedef struct LightCar {
    float fwd[3], side[3], up[3], pos[3], vel[3];
    float drive, brake;
    uint32_t ctl;
    float lo[3], hi[3];
    int has_box;
} LightCar;
int  light_game_cars(LightCar *out, int max);
/* the track: every scene object's triangles in world space (as the
 * capture's), rebuilt when the race's object table changes (gen counts the
 * rebuilds) */
typedef struct LightTrack {
    float *tri;              /* ntri x 3 corners x xyz */
    int *obj;                /* the object each triangle belongs to */
    int ntri, nobj, nbill;    /* nbill: objects with no rotation */
    unsigned gen;
    float lo[3], hi[3];
} LightTrack;
const LightTrack *light_game_track(void);

/* ---- the scene's lights (light_scene.c) ------------------------------------------
 * every light this frame in world space, physical units: candela along the
 * light's axis, its colour (luminance 1), its frame (axis, side, up) and
 * kind.  Every light reaches no further than its range; a cone's (every kind
 * but a point) no wider than its outer angle, cosout = its cosine, 0 for a
 * hemisphere (cones wider than that are not supported: cosout >= 0).
 *
 *   LIGHT_HEAD   a headlamp's low beam (the shader's UN ECE R112 pattern)
 *   LIGHT_REAR   a rear lamp's broad lobe
 *   LIGHT_POINT  the same in every direction (axis, side unused)
 *   LIGHT_SPOT   candela on the axis inside the inner cone (cosine side[0]),
 *                falling to none at the outer
 *   LIGHT_RECT, LIGHT_TUBE   reserved for the area lights (DESIGN.md 7.1):
 *                the culling bounds them by the sphere of their range about
 *                pos (light_cluster.c: lc_bounds), which their range has to
 *                cover from every point of the emitter */
enum { LIGHT_HEAD, LIGHT_REAR, LIGHT_POINT, LIGHT_SPOT, LIGHT_RECT, LIGHT_TUBE };
typedef struct LightSpot {
    float pos[3];
    float kind;
    float axis[3];
    float candela;
    float side[3];
    float range;
    float rgb[3];
    float cosout;
} LightSpot;
int  light_scene_spots(LightSpot *out, int max);
/* the same, with each light's identity (stable from frame to frame: car x 4
 * + lamp), for the shadow cache */
int  light_scene_lights(LightSpot *out, int *id, int max);

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
 * mirror's, not the backdrop): world[3][3] then holds its corners; 2 for the
 * main view's backdrop; 0 otherwise */
int  light_capture_tri(int view, const LightVtx *a, const LightVtx *b, const LightVtx *c,
                       uint32_t texture, uint32_t state, float world[3][3]);
/* the frame is finished (grBufferSwap) */
void light_capture_swap(void);
/* 1 while the capture records (the flag, or BR_LIGHT_CAPTURE=1) */
int  light_capture_on(void);
/* the main camera's position this frame, from its view x projection; 0 when
 * no 3D was drawn */
int  light_capture_eye(float eye[3]);
/* the main camera this frame (its view x projection and viewport); 0 when no 3D was drawn */
int  light_capture_camera(LightCamera *c);
/* 1 when a world triangle (3 corners x xyz) lies on one of the track's
 * triangles (light_game_track): the track's shadow already holds it */
int  light_capture_ontrack(const float tri[9]);
/* the game's pixel rows as Glide takes them: its height when the origin is
 * the lower left (y up, as the game's viewport works), else 0 (glide.c) */
int  light_glide_flip(void);

#ifdef __cplusplus
}
#endif
#endif
