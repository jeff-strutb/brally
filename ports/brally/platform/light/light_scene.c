/* light_scene.c: the scene's lights, from the game's state (read-only).
 *
 * The cars' lamps (DESIGN.md 7.2), for every car in the race, player and
 * computer alike:
 *
 *   headlights   on at night, in fog and in rain: a low beam of about 20,000
 *                cd at its hot spot, halogen white (about 3,300 K), its
 *                pattern the regulation's asymmetric one (UN ECE R112: a
 *                sharp horizontal cut-off, stepping up 15 degrees on the kerb
 *                side, the hot spot just under the horizon; the shader draws
 *                it from the lamp's own frame)
 *   rear lamps   tail lamps (about 5 cd, red) whenever the headlights are on;
 *                brake lamps (about 80 cd) while the car brakes as its physics
 *                applies it: brake torque held (the pedal, the handbrake, the
 *                grid hold) or drive torque against the direction of travel;
 *                reversing lamps (about 300 cd, white) while it drives
 *                backwards (control word 0x20000, negative drive torque)
 *
 * Lamp positions: the front and rear faces of the car model's extent (its
 * full-detail body), as the lamps sit on a rally car; the lenses themselves
 * are the next refinement (DESIGN.md 2.4). */
#include <math.h>
#include <string.h>

#include "light.h"
#include "light_world.h"
#include <time.h>

#define MAX_CARS 20

static void norm(float v[3])
{
    float l = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (l > 0)
        v[0] /= l, v[1] /= l, v[2] /= l;
}

int light_scene_spots(LightSpot *out, int max)
{
    return light_scene_lights(out, NULL, max);
}

int light_scene_lights(LightSpot *out, int *id, int max)
{
    LightCar cars[MAX_CARS];
    int n = light_game_cars(cars, MAX_CARS), w = light_game_weather(), i, k = 0, j;
    int heads = w == 1 || w == 2 || w == 4;
    for (i = 0; i < n; i++) {
        const LightCar *c = &cars[i];
        float f[3], s[3], u[3], fl, vf;
        int braking, reversing;
        if (!c->has_box)
            continue;
        memcpy(f, c->fwd, 12);
        memcpy(u, c->up, 12);
        norm(f);
        norm(u);
        s[0] = f[1] * u[2] - f[2] * u[1];      /* to the car's right: forward x up */
        s[1] = f[2] * u[0] - f[0] * u[2];
        s[2] = f[0] * u[1] - f[1] * u[0];
        fl = sqrtf(c->fwd[0] * c->fwd[0] + c->fwd[1] * c->fwd[1] + c->fwd[2] * c->fwd[2]);
        vf = (c->vel[0] * c->fwd[0] + c->vel[1] * c->fwd[1] + c->vel[2] * c->fwd[2]) / (fl > 0 ? fl : 1);
        braking = c->brake < 0.0f || (fabsf(vf) > 0.3f && c->drive * vf < 0.0f);
        reversing = (c->ctl & 0x20000u) && c->drive < 0.0f && vf <= 0.3f;
        for (j = 0; j < 4 && k < max; j++) {
            /* lamp j: 0 front left, 1 front right, 2 rear left, 3 rear right, in the
               model's frame (x forward, y its second row, z up) */
            float hw = (c->hi[1] - c->lo[1]) * 0.5f * 0.72f, yc = (c->hi[1] + c->lo[1]) * 0.5f;
            float lx = j < 2 ? c->hi[0] - 0.08f : c->lo[0] + 0.05f;
            float ly = yc + (j & 1 ? -hw : hw);
            /* a headlamp 0.65 m above the model's lowest point (its wheels' contact): UN ECE
               R48 6.2.4 mounts it 0.5 to 1.2 m up; 0.36 of the body's height put it at
               about 0.49 m, under the minimum, so the cut-off (0.57 degrees down) left a
               car ahead lit only along its bumper */
            float lz = c->lo[2] + (j < 2 ? fminf(0.65f, (c->hi[2] - c->lo[2]) * 0.8f) : (c->hi[2] - c->lo[2]) * 0.48f);
            LightSpot *o = &out[k];
            int a;
            float cd;
            if (j < 2 ? !heads : !(heads || braking || reversing))
                continue;
            for (a = 0; a < 3; a++)
                o->pos[a] = c->pos[a] + c->fwd[a] * lx + c->side[a] * ly + c->up[a] * lz;
            if (j < 2) {
                o->kind = LIGHT_HEAD;
                memcpy(o->axis, f, 12);
                memcpy(o->side, s, 12);
                o->candela = 20000.0f;
                o->range = 150.0f;
                o->cosout = 0.3420201f;    /* 70 degrees: past it the pattern is under 1% of its peak */
                o->rgb[0] = 1.00f; o->rgb[1] = 0.83f; o->rgb[2] = 0.62f;
            } else {
                o->kind = LIGHT_REAR;
                for (a = 0; a < 3; a++) {
                    o->axis[a] = -f[a];
                    o->side[a] = -s[a];
                }
                cd = reversing ? 300.0f : braking ? 80.0f : 5.0f;
                o->candela = cd;
                o->range = 25.0f;
                o->cosout = 0.0f;          /* the half space behind the car */
                if (reversing) {
                    o->rgb[0] = 1.0f; o->rgb[1] = 0.95f; o->rgb[2] = 0.9f;
                } else {
                    o->rgb[0] = 1.0f; o->rgb[1] = 0.04f; o->rgb[2] = 0.015f;
                }
            }
            {   /* the colour at luminance 1: candela carry the brightness */
                float y = 0.2126f * o->rgb[0] + 0.7152f * o->rgb[1] + 0.0722f * o->rgb[2];
                o->rgb[0] /= y; o->rgb[1] /= y; o->rgb[2] /= y;
            }
            if (id)
                id[k] = i * 4 + j;
            k++;
        }
    }
    if (w == 4 && k < max) {   /* the track's own lights at night (light_world.c): ids from 1000 */
        float bp[4][3], bx[4][3];
        int m, q, nb;
        light_world_refresh();
        m = light_world_spots(out + k, max - k, 0.0f);
        for (q = 0; q < m; q++)
            if (id)
                id[k + q] = 1000 + q;
        k += m;
        nb = light_game_beams(bp, bx, 4);   /* the lighthouses, as the race turns them: ids from 2000 */
        for (q = 0; q < nb && k < max; q++, k++) {
            light_world_beam(&out[k], bp[q], bx[q]);
            if (id)
                id[k] = 2000 + q;
        }
    }
    return k;
}
