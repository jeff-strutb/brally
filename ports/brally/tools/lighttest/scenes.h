/* scenes.h: the synthetic scenes the shadow tests share (shadowtest.c on the
 * CPU, gputest.m on the GPU): no game data, every number analytic. */
#ifndef SCENES_H
#define SCENES_H

#include <stdlib.h>
#include <string.h>
#include "shadowcpu.h"

#define PEN_SIZE 512             /* the penumbra scene's map: 512 texels of 2 cm (10.24 m) */
#define PEN_TEXEL 0.02f
#define PEN_TOP 100.0f           /* metres: the window reaches back toward the sun to here */
#define PEN_RANGE 101.0f         /* its depth: from 100 m up down to 1 m below the ground */

/* the sun straight overhead; a flat receiver at z = 0; a flat blocker at
 * height D over the half-plane x < 0 (the texels left of the map's middle).
 * The map holds what an orthographic sun map of it holds: depth (100 - z) /
 * 101 of the nearest surface toward the sun */
static void pen_setup(float D, LU *u, float *map)
{
    float rw = PEN_SIZE * PEN_TEXEL * 0.5f;
    int x, y;
    memset(u, 0, sizeof *u);
    u->sunvp[0][0] = 1.0f / rw;
    u->sunvp[0][5] = 1.0f / rw;
    u->sunvp[0][10] = -1.0f / PEN_RANGE;
    u->sunvp[0][14] = PEN_TOP / PEN_RANGE;
    u->sunvp[0][15] = 1.0f;
    u->ctexel[0] = PEN_TEXEL;
    u->cbias[0] = PEN_TEXEL / PEN_RANGE;
    u->cdepth[0] = PEN_RANGE;
    u->csoft[0] = tanf(SUN_HALF_DEG * 3.14159265f / 180.0f);
    u->csoft[1] = PCSS_SEARCH_MAX;
    u->csoft[2] = PCSS_FILTER_MAX;
    u->sun[2] = 1.0f;
    u->misc[1] = 1.0f;
    u->csplit[0] = u->csplit[1] = u->csplit[2] = u->csplit[3] = 1e9f;
    for (y = 0; y < PEN_SIZE; y++)
        for (x = 0; x < PEN_SIZE; x++)
            map[y * PEN_SIZE + x] = x < PEN_SIZE / 2 ? (PEN_TOP - D) / PEN_RANGE : PEN_TOP / PEN_RANGE;
}

/* ---- a camera as the game has it ---------------------------------------- */
typedef struct Cam {
    float vp[16];                 /* row vectors: clip = [p 1] vp */
    float sx, ox, sy, oy, flip;   /* the viewport, Glide's rows */
    float xft[4];                 /* the screen map entry */
    float tw, th;                 /* the target */
    float eye[3], fwd[3];
} Cam;

static void v3norm(float v[3]) { float l = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); v[0] /= l; v[1] /= l; v[2] /= l; }
static void v3cross(const float a[3], const float b[3], float o[3]) { o[0] = a[1] * b[2] - a[2] * b[1]; o[1] = a[2] * b[0] - a[0] * b[2]; o[2] = a[0] * b[1] - a[1] * b[0]; }

/* a perspective camera at eye looking at tgt, Z up, the vertical field fovy
 * (radians), the game's clip conventions: x right, y up, w the depth */
static void cam_make(Cam *c, const float eye[3], const float tgt[3], float fovy, float aspect)
{
    float f[3] = { tgt[0] - eye[0], tgt[1] - eye[1], tgt[2] - eye[2] }, up[3] = { 0, 0, 1 }, r[3], v[3];
    float fy = 1.0f / tanf(fovy * 0.5f), fx = fy / aspect, a = 1.0f, b = -0.1f;
    int i;
    v3norm(f);
    v3cross(f, up, r);
    v3norm(r);
    v3cross(r, f, v);
    memcpy(c->eye, eye, 12);
    memcpy(c->fwd, f, 12);
    for (i = 0; i < 3; i++) {
        c->vp[4 * i + 0] = r[i] * fx;
        c->vp[4 * i + 1] = v[i] * fy;
        c->vp[4 * i + 2] = f[i] * a;
        c->vp[4 * i + 3] = f[i];
    }
    c->vp[12] = -(eye[0] * c->vp[0] + eye[1] * c->vp[4] + eye[2] * c->vp[8]);
    c->vp[13] = -(eye[0] * c->vp[1] + eye[1] * c->vp[5] + eye[2] * c->vp[9]);
    c->vp[14] = -(eye[0] * c->vp[2] + eye[1] * c->vp[6] + eye[2] * c->vp[10]) + b;
    c->vp[15] = -(eye[0] * c->vp[3] + eye[1] * c->vp[7] + eye[2] * c->vp[11]);
}

/* the sky's camray (brr_metal_sky.h) and its inverse matrix (brr_metal.m's
 * sky block), transcribed: the ray through target pixel (px, py) */
static void cam_ray(const Cam *c, float px, float py, float d[3])
{
    double aa[3][3], det, inv[3][3], cc[3], p[3];
    float ndc[2], g[2];
    int r2, c2;
    for (r2 = 0; r2 < 3; r2++)
        for (c2 = 0; c2 < 3; c2++)
            aa[r2][c2] = c->vp[c2 * 4 + (r2 == 2 ? 3 : r2)];
    det = aa[0][0] * (aa[1][1] * aa[2][2] - aa[1][2] * aa[2][1]) - aa[0][1] * (aa[1][0] * aa[2][2] - aa[1][2] * aa[2][0])
        + aa[0][2] * (aa[1][0] * aa[2][1] - aa[1][1] * aa[2][0]);
    inv[0][0] = (aa[1][1] * aa[2][2] - aa[1][2] * aa[2][1]) / det;
    inv[0][1] = (aa[0][2] * aa[2][1] - aa[0][1] * aa[2][2]) / det;
    inv[0][2] = (aa[0][1] * aa[1][2] - aa[0][2] * aa[1][1]) / det;
    inv[1][0] = (aa[1][2] * aa[2][0] - aa[1][0] * aa[2][2]) / det;
    inv[1][1] = (aa[0][0] * aa[2][2] - aa[0][2] * aa[2][0]) / det;
    inv[1][2] = (aa[0][2] * aa[1][0] - aa[0][0] * aa[1][2]) / det;
    inv[2][0] = (aa[1][0] * aa[2][1] - aa[1][1] * aa[2][0]) / det;
    inv[2][1] = (aa[0][1] * aa[2][0] - aa[0][0] * aa[2][1]) / det;
    inv[2][2] = (aa[0][0] * aa[1][1] - aa[0][1] * aa[1][0]) / det;
    ndc[0] = (px + 0.5f) / c->tw * 2.0f - 1.0f;
    ndc[1] = 1.0f - (py + 0.5f) / c->th * 2.0f;
    g[0] = (ndc[0] - c->xft[1]) / c->xft[0];
    g[1] = (ndc[1] - c->xft[3]) / c->xft[2];
    if (c->flip > 0.0f)
        g[1] = c->flip - g[1];
    cc[0] = (g[0] - c->ox) / c->sx - c->vp[12];
    cc[1] = (g[1] - c->oy) / c->sy - c->vp[13];
    cc[2] = 1.0 - c->vp[15];
    for (r2 = 0; r2 < 3; r2++)
        p[r2] = inv[r2][0] * cc[0] + inv[r2][1] * cc[1] + inv[r2][2] * cc[2];
    for (r2 = 0; r2 < 3; r2++)
        d[r2] = (float)(p[r2] - c->eye[r2]);
    v3norm(d);
}

/* a ray against an axis-aligned box: the entry distance, or -1 */
static float box_hit(const float o[3], const float d[3], const float lo[3], const float hi[3], int *face)
{
    float t0 = -1e30f, t1 = 1e30f;
    int k, f = -1;
    for (k = 0; k < 3; k++) {
        float a, b;
        if (fabsf(d[k]) < 1e-12f) {
            if (o[k] < lo[k] || o[k] > hi[k])
                return -1.0f;
            continue;
        }
        a = (lo[k] - o[k]) / d[k];
        b = (hi[k] - o[k]) / d[k];
        if (a > b) { float s = a; a = b; b = s; }
        if (a > t0) { t0 = a; f = k * 2 + (d[k] > 0 ? 0 : 1); }
        if (b < t1) t1 = b;
    }
    if (t0 > t1 || t1 < 0)
        return -1.0f;
    if (face)
        *face = f;
    return t0 > 0 ? t0 : 0.0f;
}

/* the contact scene: a block 30 x 30 cm, 12 cm tall, on flat ground, the sun
 * 33 degrees up behind it (its shadow falls toward the camera), the camera
 * 3 m off, 1.2 m up; a G-buffer of world positions (w: 1 where the ground)
 * and normals, ray traced through the game's camera conventions */
#define CON_W 320
#define CON_H 240
static const float k_box_lo[3] = { -0.15f, -0.15f, 0.0f }, k_box_hi[3] = { 0.15f, 0.15f, 0.12f };
static void con_setup(LU *u, Cam *c, float *pos, float *nrm)
{
    float eye[3] = { 0.4f, -3.0f, 1.2f }, tgt[3] = { 0.0f, -0.2f, 0.05f }, sun[3] = { 0.25f, 0.8f, 0.55f };
    int x, y, k;
    memset(u, 0, sizeof *u);
    memset(c, 0, sizeof *c);
    cam_make(c, eye, tgt, 0.8f, (float)CON_W / CON_H);
    c->sx = 160; c->ox = 160; c->sy = 120; c->oy = 120; c->flip = 240;      /* the game's rows run up */
    c->xft[0] = 2.0f / 320; c->xft[1] = -1; c->xft[2] = -2.0f / 240; c->xft[3] = 1;
    c->tw = CON_W; c->th = CON_H;
    v3norm(sun);
    for (k = 0; k < 3; k++) {
        u->sun[k] = sun[k];
        u->eye[k] = eye[k];
        u->cfwd[k] = c->fwd[k];
    }
    u->csoft[3] = CONTACT_LEN;
    brr_screen_matrix(c->vp, c->sx, c->ox, c->sy, c->oy, c->flip, c->xft, c->tw, c->th, u->scr);
    for (y = 0; y < CON_H; y++)
        for (x = 0; x < CON_W; x++) {
            float d[3], tb, tg = -1, *P = &pos[4 * (y * CON_W + x)], *N = &nrm[4 * (y * CON_W + x)];
            int face = -1;
            cam_ray(c, (float)x, (float)y, d);
            tb = box_hit(eye, d, k_box_lo, k_box_hi, &face);
            if (d[2] < 0)
                tg = -eye[2] / d[2];
            memset(P, 0, 16);
            memset(N, 0, 16);
            if (tb > 0 && (tg < 0 || tb < tg)) {
                for (k = 0; k < 3; k++)
                    P[k] = eye[k] + d[k] * tb;
                N[face / 2] = face & 1 ? 1.0f : -1.0f;
            } else if (tg > 0) {
                for (k = 0; k < 3; k++)
                    P[k] = eye[k] + d[k] * tg;
                P[2] = 0.0f;
                P[3] = 1.0f;
                N[2] = 1.0f;
            }
        }
}

#endif
