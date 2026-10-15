/* scene.h: synthetic cameras and lights for the light tests (cluster_test.c,
 * metal_test.m): no game, no data. */
#ifndef LIGHTTEST_SCENE_H
#define LIGHTTEST_SCENE_H

#include <math.h>
#include <stdint.h>
#include <string.h>

#include "light_cluster.h"

static uint64_t g_rng = 0x9E3779B97F4A7C15ull;
static double rnd(void)                     /* [0, 1) */
{
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 7;
    g_rng ^= g_rng << 17;
    return (double)(g_rng >> 11) / 9007199254740992.0;
}
static float rndf(float a, float b) { return a + (b - a) * (float)rnd(); }

static void nrm3(float v[3])
{
    float l = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (l > 0)
        v[0] /= l, v[1] /= l, v[2] /= l;
}
static void cross3(const float a[3], const float b[3], float o[3])
{
    o[0] = a[1] * b[2] - a[2] * b[1];
    o[1] = a[2] * b[0] - a[0] * b[2];
    o[2] = a[0] * b[1] - a[1] * b[0];
}

/* a camera as the game's (LightCamera.vp: row vectors, clip = [p 1] vp, w the
 * view depth, z in [0, 1] from near to far), Z up */
typedef struct Cam { float vp[16], eye[3], fwd[3], right[3], up[3], tx, ty; } Cam;
static void cam_make(Cam *c, const float eye[3], const float fwd[3], float vfov_deg, float aspect, float zn, float zf)
{
    float z[3] = { 0, 0, 1 }, A = zf / (zf - zn), B = -zn * zf / (zf - zn);
    int i;
    memcpy(c->eye, eye, 12);
    memcpy(c->fwd, fwd, 12);
    nrm3(c->fwd);
    cross3(c->fwd, z, c->right);
    nrm3(c->right);
    cross3(c->right, c->fwd, c->up);
    c->ty = tanf(vfov_deg * 0.5f * 3.14159265f / 180.0f);
    c->tx = c->ty * aspect;
    memset(c->vp, 0, sizeof c->vp);
    for (i = 0; i < 3; i++) {          /* coefficient of p_i in clip x, y, z, w */
        c->vp[i * 4 + 0] = c->right[i] / c->tx;
        c->vp[i * 4 + 1] = c->up[i] / c->ty;
        c->vp[i * 4 + 2] = A * c->fwd[i];
        c->vp[i * 4 + 3] = c->fwd[i];
    }
    for (i = 0; i < 4; i++) {
        float t = i == 0 ? c->right[0] * eye[0] + c->right[1] * eye[1] + c->right[2] * eye[2]
                : i == 1 ? c->up[0] * eye[0] + c->up[1] * eye[1] + c->up[2] * eye[2]
                         : c->fwd[0] * eye[0] + c->fwd[1] * eye[1] + c->fwd[2] * eye[2];
        c->vp[12 + i] = i == 0 ? -t / c->tx : i == 1 ? -t / c->ty : i == 2 ? -A * t + B : -t;
    }
}
/* the world point at screen (nx, ny) in [-1, 1] and view depth d */
static void cam_point(const Cam *c, float nx, float ny, float d, float p[3])
{
    int k;
    for (k = 0; k < 3; k++)
        p[k] = c->eye[k] + d * (c->fwd[k] + nx * c->tx * c->right[k] + ny * c->ty * c->up[k]);
}

static void light_colour(LightSpot *l, float r, float g, float b)
{
    float y = 0.2126f * r + 0.7152f * g + 0.0722f * b;
    l->rgb[0] = r / y;
    l->rgb[1] = g / y;
    l->rgb[2] = b / y;
}

/* a random light near the camera: a headlamp, a rear lamp, a point or a spot */
static void light_random(LightSpot *l, const Cam *c)
{
    float u = (float)rnd(), up[3] = { 0, 0, 1 };
    int k;
    memset(l, 0, sizeof *l);
    cam_point(c, rndf(-1.6f, 1.6f), rndf(-1.6f, 1.6f), rndf(-40.0f, 300.0f), l->pos);
    l->axis[0] = rndf(-1, 1);
    l->axis[1] = rndf(-1, 1);
    l->axis[2] = rndf(-0.6f, 0.6f);
    nrm3(l->axis);
    cross3(l->axis, up, l->side);
    nrm3(l->side);
    if (u < 0.3f) {
        l->kind = LIGHT_HEAD;
        l->candela = 20000;
        l->range = 150;
        l->cosout = 0.3420201f;
        light_colour(l, 1.0f, 0.83f, 0.62f);
    } else if (u < 0.5f) {
        l->kind = LIGHT_REAR;
        l->candela = rnd() < 0.5 ? 5 : 80;
        l->range = 25;
        l->cosout = 0;
        light_colour(l, 1.0f, 0.04f, 0.015f);
    } else if (u < 0.75f) {
        l->kind = LIGHT_POINT;
        l->candela = rndf(100, 5000);
        l->range = rndf(3, 60);
        for (k = 0; k < 3; k++)
            l->axis[k] = l->side[k] = 0;
        light_colour(l, 1.0f, 0.9f, 0.7f);
    } else {
        float out = rndf(4.0f, 85.0f) * 3.14159265f / 180.0f, in = out * rndf(0.3f, 0.95f);
        l->kind = LIGHT_SPOT;
        l->candela = rndf(1000, 2e5f);
        l->range = rndf(10, 200);
        l->cosout = cosf(out);
        memset(l->side, 0, sizeof l->side);
        l->side[0] = cosf(in);
        light_colour(l, 1.0f, 1.0f, 1.0f);
    }
}

/* a random point the light reaches (inside its range and cone), or not quite */
static void point_near(const LightSpot *l, float p[3])
{
    float d[3], r = l->range * (float)pow(rnd(), 0.5) * 1.02f;
    int k;
    for (;;) {
        d[0] = rndf(-1, 1);
        d[1] = rndf(-1, 1);
        d[2] = rndf(-1, 1);
        if (d[0] * d[0] + d[1] * d[1] + d[2] * d[2] <= 1.0f && d[0] * d[0] + d[1] * d[1] + d[2] * d[2] > 1e-6f)
            break;
    }
    nrm3(d);
    if (l->kind != LIGHT_POINT && rnd() < 0.8) {     /* mostly inside the cone (or just past its edge) */
        float t = acosf(fminf(fmaxf(l->cosout, 0.0f), 1.0f)) * rndf(0.0f, 1.05f), b[3], o[3], q[3] = { 0.3f, 0.5f, 0.8f };
        cross3(l->axis, q, b);
        nrm3(b);
        cross3(l->axis, b, o);
        {
            float ph = rndf(0, 6.2831853f);
            for (k = 0; k < 3; k++)
                d[k] = l->axis[k] * cosf(t) + sinf(t) * (b[k] * cosf(ph) + o[k] * sinf(ph));
        }
    }
    for (k = 0; k < 3; k++)
        p[k] = l->pos[k] + d[k] * r;
}

/* the 20-car night grid (DESIGN.md 7.4's stress case): two columns, 8 m
 * between rows, every car's two low beams on (DESIGN.md 7.2) and its two tail
 * lamps; the camera a chase camera 6 m behind and 2.2 m above the last car,
 * looking down the grid.  Returns the light count; ids car x 4 + lamp. */
static int grid_scene(LightSpot *ls, int *ids, Cam *c, float aspect)
{
    int car, j, n = 0;
    float eye[3] = { -6.0f, -2.0f, 2.2f }, fwd[3] = { 1.0f, 0.0f, -0.12f };
    cam_make(c, eye, fwd, 60.0f, aspect, 0.1f, 2000.0f);
    for (car = 0; car < 20; car++) {
        float cx = (float)((19 - car) / 2) * 8.0f, cy = car & 1 ? 2.0f : -2.0f;   /* row, column */
        for (j = 0; j < 4; j++) {
            LightSpot *l = &ls[n];
            memset(l, 0, sizeof *l);
            l->pos[0] = cx + (j < 2 ? 2.0f : -2.0f);
            l->pos[1] = cy + (j & 1 ? -0.7f : 0.7f);
            l->pos[2] = j < 2 ? 0.55f : 0.75f;
            l->axis[0] = j < 2 ? 1.0f : -1.0f;
            l->side[1] = j < 2 ? -1.0f : 1.0f;          /* to the car's right (forward x up) */
            if (j < 2) {
                l->kind = LIGHT_HEAD;
                l->candela = 20000;
                l->range = 150;
                l->cosout = 0.3420201f;
                light_colour(l, 1.0f, 0.83f, 0.62f);
            } else {
                l->kind = LIGHT_REAR;
                l->candela = 5;
                l->range = 25;
                l->cosout = 0;
                light_colour(l, 1.0f, 0.04f, 0.015f);
            }
            if (ids)
                ids[n] = car * 4 + j;
            n++;
        }
    }
    return n;
}

#endif
