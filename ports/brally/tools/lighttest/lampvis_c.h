/* lampvis_c.h: the shader's lampvis (brr_metal_lights.h), in C, over a depth
 * atlas of asz x asz texels (row 0 at the top), for cluster_test.c and
 * metal_test.m: the same blocker search, mean blocker distance, penumbra
 * width and rotated Vogel disc of bilinear comparisons.  Needs light_cluster.h. */
#ifndef LAMPVIS_C_H
#define LAMPVIS_C_H

#include <math.h>

static void lv_clip(const LightShadowRec *r, const float p[3], float c[4])
{
    int k;
    for (k = 0; k < 4; k++)
        c[k] = r->m[k] * p[0] + r->m[4 + k] * p[1] + r->m[8 + k] * p[2] + r->m[12 + k];
}

static int lv_texel(int v, int asz) { return v < 0 ? 0 : v >= asz ? asz - 1 : v; }

/* the sampler's filtered comparison (linear, clamp to edge, less_equal) */
static float lv_cmp(const float *dep, int asz, float s, float t, float ref)
{
    float x = s * asz - 0.5f, y = t * asz - 0.5f, fx, fy, v = 0;
    int x0 = (int)floorf(x), y0 = (int)floorf(y), i, j;
    fx = x - x0;
    fy = y - y0;
    for (j = 0; j < 2; j++)
        for (i = 0; i < 2; i++) {
            float w = (i ? fx : 1 - fx) * (j ? fy : 1 - fy);
            v += w * (ref <= dep[lv_texel(y0 + j, asz) * asz + lv_texel(x0 + i, asz)] ? 1.0f : 0.0f);
        }
    return v;
}

/* point i of n of the Vogel disc, turned by rot (cos, sin) */
static void lv_vogel(int i, int n, const float rot[2], float o[2])
{
    float r = sqrtf((i + 0.5f) / n), t = i * 2.39996323f, vx = r * cosf(t), vy = r * sinf(t);
    o[0] = vx * rot[0] - vy * rot[1];
    o[1] = vx * rot[1] + vy * rot[0];
}

static float lv_clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

static float lampvis_c(const LightShadowRec *h, const float *dep, int asz, const float p[3], const float n[3], const float ld[3],
                       const float rot[2])
{
    float c0[4], c[4], q[3], gz[2], T, tx, nl, tn, nd[2], st[2], lo[2], hi[2], ts = 1.0f / asz, dr, zr, kt, rs, zb = 0, nb = 0, db, wr, vis = 0;
    int k, j;
    if (h->rect[3] < 0.5f)
        return 1;
    lv_clip(h, p, c0);
    if (c0[3] <= h->prm[3])
        return 1;
    T = h->rect[2] * asz;
    tx = 2.0f * c0[3] * h->prm[2] / T;
    nl = lv_clampf(n[0] * ld[0] + n[1] * ld[1] + n[2] * ld[2], 0.05f, 1.0f);
    tn = fminf(sqrtf(1 - nl * nl) / nl, 8.0f);
    for (k = 0; k < 3; k++)
        q[k] = p[k] + n[k] * tx * (1.0f + 2.0f * (1.0f - nl));
    lv_clip(h, q, c);
    nd[0] = c[0] / c[3];
    nd[1] = c[1] / c[3];
    if (c[3] <= h->prm[3] || fabsf(nd[0]) >= 1 || fabsf(nd[1]) >= 1)
        return 1;
    for (k = 0; k < 2; k++) {
        lo[k] = h->rect[k] + 0.5f * ts;
        hi[k] = h->rect[k] + h->rect[2] - 0.5f * ts;
    }
    st[0] = h->rect[0] + (nd[0] * 0.5f + 0.5f) * h->rect[2];
    st[1] = h->rect[1] + (0.5f - nd[1] * 0.5f) * h->rect[2];
    dr = fmaxf(c[3] - tx * (1 + tn), h->prm[3]);
    zr = h->prm[0] + h->prm[1] / dr;
    {                                  /* the receiver's plane: dz / dst from two points a texel off along it */
        float t1[3], t2[3], a1[4], a2[4], qa[3], qb[3], e1[2], e2[2], z1, z2, det, len, ax[3] = { 0, 0, 1 };
        if (fabsf(n[2]) >= 0.9f)
            ax[0] = 1, ax[2] = 0;
        t1[0] = n[1] * ax[2] - n[2] * ax[1];
        t1[1] = n[2] * ax[0] - n[0] * ax[2];
        t1[2] = n[0] * ax[1] - n[1] * ax[0];
        len = sqrtf(t1[0] * t1[0] + t1[1] * t1[1] + t1[2] * t1[2]);
        for (k = 0; k < 3; k++)
            t1[k] /= len;
        t2[0] = n[1] * t1[2] - n[2] * t1[1];
        t2[1] = n[2] * t1[0] - n[0] * t1[2];
        t2[2] = n[0] * t1[1] - n[1] * t1[0];
        for (k = 0; k < 3; k++) {
            qa[k] = q[k] + t1[k] * tx;
            qb[k] = q[k] + t2[k] * tx;
        }
        lv_clip(h, qa, a1);
        lv_clip(h, qb, a2);
        e1[0] = (a1[0] / a1[3] - nd[0]) * 0.5f * h->rect[2];
        e1[1] = (a1[1] / a1[3] - nd[1]) * -0.5f * h->rect[2];
        e2[0] = (a2[0] / a2[3] - nd[0]) * 0.5f * h->rect[2];
        e2[1] = (a2[1] / a2[3] - nd[1]) * -0.5f * h->rect[2];
        z1 = a1[2] / a1[3] - c[2] / c[3];
        z2 = a2[2] / a2[3] - c[2] / c[3];
        det = e1[0] * e2[1] - e1[1] * e2[0];
        gz[0] = gz[1] = 0;
        if (fabsf(det) > 1e-12f) {
            gz[0] = (z1 * e2[1] - z2 * e1[1]) / det;
            gz[1] = (z2 * e1[0] - z1 * e2[0]) / det;
        }
    }
    zr -= (fabsf(gz[0]) + fabsf(gz[1])) * 0.5f * ts;
    kt = 0.075f * T / (2.0f * h->prm[2]);
    rs = lv_clampf(kt * (1.0f / h->prm[3] - 1.0f / dr), 1.0f, 12.0f);
    for (k = 0; k < 16; k++) {         /* gather: the 2 x 2 texels the bilinear footprint at the point covers */
        float v[2], s, t;
        int x0, y0, a, b;
        lv_vogel(k, 16, rot, v);
        s = lv_clampf(st[0] + v[0] * rs * ts, lo[0], hi[0]);
        t = lv_clampf(st[1] + v[1] * rs * ts, lo[1], hi[1]);
        x0 = (int)floorf(s * asz - 0.5f);
        y0 = (int)floorf(t * asz - 0.5f);
        for (b = 0; b < 2; b++)
            for (a = 0; a < 2; a++) {
                float g = dep[lv_texel(y0 + b, asz) * asz + lv_texel(x0 + a, asz)];
                if (g < zr + gz[0] * (s - st[0]) + gz[1] * (t - st[1])) {
                    zb += h->prm[1] / (g - h->prm[0]);
                    nb += 1;
                }
            }
    }
    if (nb == 0)
        return 1;
    db = fmaxf(zb / nb, h->prm[3]);
    wr = lv_clampf(kt * (1.0f / db - 1.0f / dr), 0.5f, 16.0f);
    for (j = 0; j < 24; j++) {
        float v[2];
        lv_vogel(j, 24, rot, v);
        float s = lv_clampf(st[0] + v[0] * wr * ts, lo[0], hi[0]), t = lv_clampf(st[1] + v[1] * wr * ts, lo[1], hi[1]);
        vis += lv_cmp(dep, asz, s, t, zr + gz[0] * (s - st[0]) + gz[1] * (t - st[1]));
    }
    return vis / 24;
}

/* the penumbra scene: the lamp at (0, 0, 1) looking down x, a blocker plane at
 * x = db covering y >= 0, a receiving wall at x = dr; the width (10% to 90%)
 * of the shadow's edge on the wall for a lamp disc of radius R and a straight
 * edge: the disc's segment area (acos h - h sqrt(1 - h^2)) / pi is 0.1 at
 * h = 0.6870 */
static float lv_disc_h10(void)
{
    double lo = 0, hi = 1;
    int i;
    for (i = 0; i < 60; i++) {
        double m = 0.5 * (lo + hi), a = (acos(m) - m * sqrt(1 - m * m)) / 3.14159265358979;
        if (a > 0.1)
            lo = m;
        else
            hi = m;
    }
    return (float)(0.5 * (lo + hi));
}

/* the sweep across the wall: LV_NS points y = LV_Y0 + i LV_DY at x = dr, z = 1 */
#define LV_NS 701
#define LV_Y0 (-0.7f)
#define LV_DY 0.002f

/* the edge's width from 90% down to 10% (linear between sweep points), the
   largest rise along the sweep (the profile should only fall) */
static float lv_width(const float *v, float *rise)
{
    float y90 = 0, y10 = 0;
    int i, f90 = 0, f10 = 0;
    *rise = 0;
    for (i = 1; i < LV_NS; i++) {
        float a = v[i - 1], b = v[i], y = LV_Y0 + (i - 1) * LV_DY;
        *rise = fmaxf(*rise, b - a);
        if (!f90 && a >= 0.9f && b < 0.9f)
            y90 = y + LV_DY * (a - 0.9f) / (a - b), f90 = 1;
        if (!f10 && a >= 0.1f && b < 0.1f)
            y10 = y + LV_DY * (a - 0.1f) / (a - b), f10 = 1;
    }
    return f90 && f10 ? y10 - y90 : -1;
}

/* the penumbra scene's lamp: 1 m up looking down x */
static void lv_pen_lamp(LightSpot *l)
{
    memset(l, 0, sizeof *l);
    l->pos[2] = 1.0f;
    l->axis[0] = 1;
    l->side[1] = -1;
    l->kind = LIGHT_SPOT;
    l->candela = 1000;
    l->range = 60;
    l->cosout = cosf(50.0f * 3.14159265f / 180.0f);
    l->side[0] = cosf(40.0f * 3.14159265f / 180.0f);
}

#endif
