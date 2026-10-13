/* shadowcpu.h: the sun's shadow functions of brr_metal_lit.h (BRR_SHADOW_SRC)
 * on the CPU, step for step as the shader does them: the hardware's gather
 * and filtered comparison (a bilinear footprint, clamp to edge) written out.
 * shadowtest.c checks them against the physics; gputest.m checks the shader
 * against them. */
#ifndef SHADOWCPU_H
#define SHADOWCPU_H

#include <math.h>
#include "../../platform/render/metal/brr_shadow.h"

/* brr_metal.m's LU, the same layout (brr_metal_lit.h's MSL struct) */
typedef struct LU { float sunvp[CASCADES][16]; float csplit[4], cfwd[4], ctexel[4], cbias[4]; float eye[4], sun[4], sunc[4], skyc[4], gndc[4], misc[4], wb[4], bg[4], fogc[4];
                   float cdepth[4], csoft[4], scr[16], vol[4], gi[4]; } LU;

typedef struct DepthMap { const float *d; int size; } DepthMap;   /* one layer, size x size, rows from the top */

static float cpu_fract(float x) { return x - floorf(x); }
static float cpu_ign(float x, float y) { return cpu_fract(52.9829189f * cpu_fract(x * 0.06711056f + y * 0.00583715f)); }
static void cpu_vogel(int i, int n, const float rot[2], float o[2])
{
    float r = sqrtf(((float)i + 0.5f) / (float)n), t = (float)i * 2.39996323f, v[2];
    v[0] = r * cosf(t);
    v[1] = r * sinf(t);
    o[0] = v[0] * rot[0] - v[1] * rot[1];
    o[1] = v[0] * rot[1] + v[1] * rot[0];
}
static float cpu_texel(const DepthMap *m, int x, int y)
{
    x = x < 0 ? 0 : x >= m->size ? m->size - 1 : x;
    y = y < 0 ? 0 : y >= m->size ? m->size - 1 : y;
    return m->d[(size_t)y * (size_t)m->size + (size_t)x];
}
/* the bilinear footprint of (s, t): its corner texel and the fractions */
static void cpu_foot(const DepthMap *m, float s, float t, int *x0, int *y0, float *fx, float *fy)
{
    float x = s * (float)m->size - 0.5f, y = t * (float)m->size - 0.5f;
    *x0 = (int)floorf(x);
    *y0 = (int)floorf(y);
    *fx = x - (float)*x0;
    *fy = y - (float)*y0;
}
static void cpu_gather(const DepthMap *m, float s, float t, float g[4])
{
    int x0, y0;
    float fx, fy;
    cpu_foot(m, s, t, &x0, &y0, &fx, &fy);
    g[0] = cpu_texel(m, x0, y0 + 1);
    g[1] = cpu_texel(m, x0 + 1, y0 + 1);
    g[2] = cpu_texel(m, x0 + 1, y0);
    g[3] = cpu_texel(m, x0, y0);
}
/* compare_func::less_equal: lit where the reference is at or before the texel */
static float cpu_cmp(const DepthMap *m, float s, float t, float ref)
{
    int x0, y0;
    float fx, fy;
    cpu_foot(m, s, t, &x0, &y0, &fx, &fy);
    return (1 - fx) * (1 - fy) * (ref <= cpu_texel(m, x0, y0) ? 1.0f : 0.0f)
         + fx * (1 - fy) * (ref <= cpu_texel(m, x0 + 1, y0) ? 1.0f : 0.0f)
         + (1 - fx) * fy * (ref <= cpu_texel(m, x0, y0 + 1) ? 1.0f : 0.0f)
         + fx * fy * (ref <= cpu_texel(m, x0 + 1, y0 + 1) ? 1.0f : 0.0f);
}
static float cpu_clamp(float x, float a, float b) { return x < a ? a : x > b ? b : x; }

/* casvis, cascade k (its map m), the disc turned by phi (sunvis's cos, sin) */
static float cpu_casvis(const LU *u, const DepthMap *m, const float p[3], const float n[3], int k, float phi)
{
    float rot[2] = { cosf(phi), sinf(phi) };
    const float *M = u->sunvp[k];
    float nl = cpu_clamp(n[0] * u->sun[0] + n[1] * u->sun[1] + n[2] * u->sun[2], 0.05f, 1.0f), tn = fminf(sqrtf(1.0f - nl * nl) / nl, 8.0f);
    float off = u->ctexel[k] * (1.0f + 2.0f * (1.0f - nl)), q[3], c[3], st[2], tx, zr, tpd, rs, zb = 0, nb = 0, wr, vis = 0;
    int i, j;
    for (i = 0; i < 3; i++)
        q[i] = p[i] + n[i] * off;
    for (i = 0; i < 3; i++)
        c[i] = M[i] * q[0] + M[4 + i] * q[1] + M[8 + i] * q[2] + M[12 + i];
    st[0] = c[0] * 0.5f + 0.5f;
    st[1] = 0.5f - c[1] * 0.5f;
    if (st[0] <= 0 || st[1] <= 0 || st[0] >= 1 || st[1] >= 1 || c[2] >= 1)
        return 1.0f;
    tx = 1.0f / (float)m->size;
    zr = c[2] - u->cbias[k] * (1.0f + tn);
    tpd = u->cdepth[k] * u->csoft[0] / u->ctexel[k];
    rs = cpu_clamp(c[2] * tpd, 1.0f, u->csoft[1]);
    for (i = 0; i < 16; i++) {
        float o[2], g[4];
        cpu_vogel(i, 16, rot, o);
        cpu_gather(m, st[0] + o[0] * (rs * tx), st[1] + o[1] * (rs * tx), g);
        for (j = 0; j < 4; j++)
            if (g[j] < zr) {
                zb += g[j];
                nb += 1.0f;
            }
    }
    if (nb == 0.0f)
        return 1.0f;
    wr = fminf((c[2] - zb / nb) * tpd, u->csoft[2]);
    for (i = 0; i < 24; i++) {
        float o[2];
        cpu_vogel(i, 24, rot, o);
        vis += cpu_cmp(m, st[0] + o[0] * (wr * tx), st[1] + o[1] * (wr * tx), zr);
    }
    return vis / 24.0f;
}

/* contact, over a G-buffer of world positions (w x h, rows from the top) */
static float cpu_contact(const LU *u, const float *pos, int w, int h, const float p[3], const float n[3], float j)
{
    int i, k, ns;
    float h0[4], h1[4], e[3], lpx;
    if (u->csoft[3] <= 0.0f || n[0] * u->sun[0] + n[1] * u->sun[1] + n[2] * u->sun[2] <= 0.0f)
        return 1.0f;
    for (k = 0; k < 3; k++)
        e[k] = p[k] + u->sun[k] * u->csoft[3];
    for (k = 0; k < 4; k++) {
        h0[k] = u->scr[k] * p[0] + u->scr[4 + k] * p[1] + u->scr[8 + k] * p[2] + u->scr[12 + k];
        h1[k] = u->scr[k] * e[0] + u->scr[4 + k] * e[1] + u->scr[8 + k] * e[2] + u->scr[12 + k];
    }
    if (h0[3] > 1e-6f && h1[3] > 1e-6f) {
        float dx = h1[0] / h1[3] - h0[0] / h0[3], dy = h1[1] / h1[3] - h0[1] / h0[3];
        lpx = sqrtf(dx * dx + dy * dy);
    } else
        lpx = 16.0f;
    if (lpx < 1.0f)
        return 1.0f;
    ns = (int)fminf(ceilf(lpx), 16.0f);
    for (i = 0; i < ns; i++) {
        float t = ((float)i + j) / (float)ns * u->csoft[3], q[3], hh[4], s[2], dq = 0, ds = 0, up = 0;
        const float *P;
        for (k = 0; k < 3; k++)
            q[k] = p[k] + u->sun[k] * t;
        for (k = 0; k < 4; k++)
            hh[k] = u->scr[k] * q[0] + u->scr[4 + k] * q[1] + u->scr[8 + k] * q[2] + u->scr[12 + k];
        if (hh[3] <= 1e-6f)
            return 1.0f;
        s[0] = hh[0] / hh[3];
        s[1] = hh[1] / hh[3];
        if (s[0] < 0 || s[1] < 0 || s[0] >= (float)w || s[1] >= (float)h)
            return 1.0f;
        P = &pos[4 * ((size_t)(unsigned)s[1] * (size_t)w + (size_t)(unsigned)s[0])];
        if (P[0] == 0 && P[1] == 0 && P[2] == 0)
            continue;
        for (k = 0; k < 3; k++) {
            dq += (q[k] - u->eye[k]) * u->cfwd[k];
            ds += (P[k] - u->eye[k]) * u->cfwd[k];
            up += (P[k] - p[k]) * n[k];
        }
        if (ds < dq && dq - ds < 0.0625f + 0.004f * dq && up > 0.02f + 0.002f * dq)
            return cpu_clamp((t / u->csoft[3] - 0.75f) * 4.0f, 0.0f, 1.0f);
    }
    return 1.0f;
}

#endif
