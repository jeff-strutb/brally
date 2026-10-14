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
/* the track's sky environment, as the game picks its sky texture by the chosen
 * track (mirrors share their base track's): 0 desert, 1 mountain, 2 coast,
 * 3 mine, 4 amazon, 5 race, 6 bonus */
int  light_game_sky_env(void);
void light_game_sun(float v[3]);   /* the game's sun, toward it, unit, Z up (its car light) */
int  light_game_track_id(void);           /* the chosen track, as the game numbers it */
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
/* where a car's lamps sit in its own frame: a headlamp 0.65 m over the
   model's lowest point (UN ECE R48 6.2.4: 0.5 to 1.2 m), a rear lamp at 0.48
   of its height (j 0 front, 1 rear); their half-span across, 0.72 of the
   body's half-width */
static inline float light_car_lamp_z(const LightCar *c, int rear)
{
    float h = c->hi[2] - c->lo[2];
    return c->lo[2] + (rear ? h * 0.48f : (0.65f < h * 0.8f ? 0.65f : h * 0.8f));
}
/* a car's floor pan, metres over its model's lowest point (its wheels' contact): a rally
   car's ground clearance, 0.15 to 0.25 m */
#define LIGHT_CAR_FLOOR 0.2f
static inline float light_car_lamp_hw(const LightCar *c) { return (c->hi[1] - c->lo[1]) * 0.5f * 0.72f; }
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
/* 1 when world point p lies inside car c's model box (lo..hi in its own
 * frame: fwd, side, up about pos) grown by margin on every side */
static inline int light_car_inbox(const LightCar *c, const float p[3], float margin)
{
    float d[3] = { p[0] - c->pos[0], p[1] - c->pos[1], p[2] - c->pos[2] };
    const float *ax[3] = { c->fwd, c->side, c->up };
    int a;
    if (!c->has_box)
        return 0;
    for (a = 0; a < 3; a++) {
        float l = d[0] * ax[a][0] + d[1] * ax[a][1] + d[2] * ax[a][2];
        if (l < c->lo[a] - margin || l > c->hi[a] + margin)
            return 0;
    }
    return 1;
}
#define LIGHT_CAR_MARGIN 0.3f
/* 1 when a world triangle (3 corners x xyz) is car paint: its centroid in
 * a car's box (light_car_inbox, LIGHT_CAR_MARGIN) and the triangle not on
 * the track (light_capture_ontrack).  Glass and wheels are not told apart
 * yet: everything of the car's model is its paint */
int  light_capture_oncar(const float tri[9]);
/* the game's pixel rows as Glide takes them: its height when the origin is
 * the lower left (y up, as the game's viewport works), else 0 (glide.c) */
int  light_glide_flip(void);

#ifdef __cplusplus
}
#endif
#endif
