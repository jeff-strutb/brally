/* light_surf_shade.c: the Remastered textures' shading in C (light_surf.h
 * light_surf_shade): the reference the shaders follow line for line
 * (render/metal/brr_metal_surf.h, render/vulkan/shaders/surf.glsl) and the
 * software renderer's own.
 *
 * Where a GPU filters (16x anisotropic, trilinear) this takes one trilinear
 * level from the larger of the two gradients; everything that decides what is
 * drawn where (the regions, the variants' patches, the hex cells and their
 * turns, the borders) is the same arithmetic, and the noise is an integer hash,
 * bit-exact on every GPU and here. */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "light_surf.h"

static float fractf(float x) { return x - floorf(x); }
static float clampf(float x, float a, float b) { return x < a ? a : x > b ? b : x; }
static float smooth(float a, float b, float x) { float t = clampf((x - a) / (b - a), 0.0f, 1.0f); return t * t * (3.0f - 2.0f * t); }

/* the integer hash (shaders: surfhash): a cell's two numbers in 0..1 */
static uint32_t ihash(int32_t x, int32_t y)
{
    uint32_t h = (uint32_t)x * 0x8da6b343u ^ (uint32_t)y * 0xd8163841u;
    h ^= h >> 13;
    h *= 0x2c1b3c6du;
    h ^= h >> 12;
    h *= 0x297a2d39u;
    h ^= h >> 15;
    return h;
}
static void hash2(int32_t x, int32_t y, float r[2])
{
    uint32_t h = ihash(x, y);
    r[0] = (float)(h & 0xFFFFu) / 65536.0f;
    r[1] = (float)(h >> 16) / 65536.0f;
}
static float vhash(int32_t x, int32_t y) { return (float)(ihash(x, y) & 0xFFFFFFu) / 16777216.0f; }
static float vnoise(float x, float y)
{
    float fx = floorf(x), fy = floorf(y), u = x - fx, v = y - fy;
    int32_t ix = (int32_t)fx, iy = (int32_t)fy;
    u = u * u * (3.0f - 2.0f * u);
    v = v * v * (3.0f - 2.0f * v);
    return (vhash(ix, iy) * (1 - u) + vhash(ix + 1, iy) * u) * (1 - v) + (vhash(ix, iy + 1) * (1 - u) + vhash(ix + 1, iy + 1) * u) * v;
}
static float vfbm(float x, float y)
{
    return 0.5f * vnoise(x, y) + 0.3f * vnoise(x * 2.03f + 5.1f, y * 2.03f + 5.1f) + 0.2f * vnoise(x * 4.11f + 9.7f, y * 4.11f + 9.7f);
}

/* ---- the sets, sampled ---------------------------------------------------------------- */
/* level l, bilinear, wrapping, at uv (tiles), into o[ch] (0..1) */
static void bilin(int set, int k, int l, float u, float v, float *o)
{
    int ch = k ? 2 : 4, n, c;
    const uint8_t *px = light_surf_set_level(set, k, l, &n);
    if (!px) {
        for (c = 0; c < ch; c++)
            o[c] = 0.5f;
        return;
    }
    float x = u * n - 0.5f, y = v * n - 0.5f, fx = floorf(x), fy = floorf(y), ax = x - fx, ay = y - fy;
    int x0 = ((int)fx % n + n) % n, y0 = ((int)fy % n + n) % n, x1 = (x0 + 1) % n, y1 = (y0 + 1) % n;
    for (c = 0; c < ch; c++)
        o[c] = ((px[(y0 * n + x0) * ch + c] * (1 - ax) + px[(y0 * n + x1) * ch + c] * ax) * (1 - ay)
                + (px[(y1 * n + x0) * ch + c] * (1 - ax) + px[(y1 * n + x1) * ch + c] * ax) * ay) / 255.0f;
}

static void tri(int set, int k, float u, float v, const float dx[2], const float dy[2], float *o)
{
    int size = light_surf_sets_size(), ch = k ? 2 : 4, l0, c, s_levels = light_surf_set_levels();
    float d = fmaxf(sqrtf(dx[0] * dx[0] + dx[1] * dx[1]), sqrtf(dy[0] * dy[0] + dy[1] * dy[1])) * size;
    float lod = d > 1.0f ? log2f(d) : 0.0f, f, a[4], b[4];
    if (lod > s_levels - 1)
        lod = (float)(s_levels - 1);
    l0 = (int)lod;
    f = lod - l0;
    bilin(set, k, l0, u, v, a);
    if (f > 0.0f && l0 + 1 < s_levels) {
        bilin(set, k, l0 + 1, u, v, b);
        for (c = 0; c < ch; c++)
            a[c] = a[c] * (1 - f) + b[c] * f;
    }
    memcpy(o, a, sizeof(float) * ch);
}

/* ---- the shading (the shaders' surfhex, surfone, surfset, surfshade) -------------------- */
typedef struct { float alb[3], h, n[2]; } SurfS;
typedef struct { float alb[3], h, dn[3]; } SurfR;

/* a triangle grid cell's corners and weights (Heitz and Neyret 2018's TriangleGrid) */
static void trigrid(float u, float v, float w[3], int c[3][2])
{
    float sx = (u - 0.57735027f * v) * 2.0f, sy = 1.15470054f * v * 2.0f;
    int bx = (int)floorf(sx), by = (int)floorf(sy);
    float fx = sx - bx, fy = sy - by, z = 1.0f - fx - fy;
    if (z > 0.0f) {
        w[0] = z; w[1] = fy; w[2] = fx;
        c[0][0] = bx; c[0][1] = by; c[1][0] = bx; c[1][1] = by + 1; c[2][0] = bx + 1; c[2][1] = by;
    } else {
        w[0] = -z; w[1] = 1.0f - fy; w[2] = 1.0f - fx;
        c[0][0] = bx + 1; c[0][1] = by + 1; c[1][0] = bx + 1; c[1][1] = by; c[2][0] = bx; c[2][1] = by + 1;
    }
}

static SurfS hexsample(int set, float u, float v, const float dx[2], const float dy[2], float rot)
{
    float w[3], sum = 0;
    int c[3][2], i;
    SurfS o;
    memset(&o, 0, sizeof o);
    trigrid(u, v, w, c);
    for (i = 0; i < 3; i++) {
        w[i] = powf(w[i], 8.0f);
        sum += w[i];
    }
    for (i = 0; i < 3; i++) {
        float r[2], a, cs, sn, q[2], gx[2], gy[2], A[4], B[2], nt[2];
        w[i] /= sum;
        if (w[i] < 0.02f)
            continue;
        hash2(c[i][0], c[i][1], r);
        a = rot * r[0] * 6.2831853f;
        cs = cosf(a);
        sn = sinf(a);
        /* R = [[c, -s], [s, c]] (column-major float2x2(float2(c, s), float2(-s, c))) */
        q[0] = cs * u - sn * v + r[0] * 7.31f;
        q[1] = sn * u + cs * v + r[1] * 7.31f;
        gx[0] = cs * dx[0] - sn * dx[1]; gx[1] = sn * dx[0] + cs * dx[1];
        gy[0] = cs * dy[0] - sn * dy[1]; gy[1] = sn * dy[0] + cs * dy[1];
        tri(set, 0, q[0], q[1], gx, gy, A);
        tri(set, 1, q[0], q[1], gx, gy, B);
        nt[0] = B[0] * 2.0f - 1.0f;   /* OpenGL's green points up the image, against v */
        nt[1] = 1.0f - B[1] * 2.0f;
        /* back to the projection's axes: transpose(R) */
        o.n[0] += w[i] * (cs * nt[0] + sn * nt[1]);
        o.n[1] += w[i] * (-sn * nt[0] + cs * nt[1]);
        o.alb[0] += w[i] * A[0] * 2.0f;
        o.alb[1] += w[i] * A[1] * 2.0f;
        o.alb[2] += w[i] * A[2] * 2.0f;
        o.h += w[i] * A[3];
    }
    return o;
}

static SurfR surfone(int set, float k, const float sa[4], const float sc[4], const float p[3], const float n[3], const float dp0[3], const float dp1[3])
{
    float an[3], bw[3], tw = 0, alb[3] = { 0, 0, 0 }, h = 0, dn[3] = { 0, 0, 0 }, dpx[3], dpy[3];
    int i;
    SurfR o;
    for (i = 0; i < 3; i++) {
        an[i] = fabsf(n[i]);
        bw[i] = an[i] * an[i] * an[i] * an[i];
        dpx[i] = dp0[i] * k;
        dpy[i] = dp1[i] * k;
    }
    tw = bw[0] + bw[1] + bw[2];
    for (i = 0; i < 3; i++)
        bw[i] /= tw;
    tw = 0;
    if (bw[2] > 0.02f) {   /* from above (Z up): ground */
        float sg = n[2] >= 0.0f ? 1.0f : -1.0f, gx[2] = { dpx[0], dpx[1] * sg }, gy[2] = { dpy[0], dpy[1] * sg };
        SurfS s = hexsample(set, p[0] * k, p[1] * sg * k, gx, gy, sc[0]);
        for (i = 0; i < 3; i++)
            alb[i] += bw[2] * s.alb[i];
        h += bw[2] * s.h;
        dn[0] += bw[2] * s.n[0];
        dn[1] += bw[2] * sg * s.n[1];
        tw += bw[2];
    }
    if (bw[0] > 0.02f) {   /* from the side, along x */
        float sg = n[0] >= 0.0f ? 1.0f : -1.0f, gx[2] = { dpx[1] * sg, dpx[2] }, gy[2] = { dpy[1] * sg, dpy[2] };
        SurfS s = hexsample(set, p[1] * sg * k, p[2] * k, gx, gy, sc[0]);
        for (i = 0; i < 3; i++)
            alb[i] += bw[0] * s.alb[i];
        h += bw[0] * s.h;
        dn[1] += bw[0] * sg * s.n[0];
        dn[2] += bw[0] * s.n[1];
        tw += bw[0];
    }
    if (bw[1] > 0.02f) {   /* from the side, along y */
        float sg = n[1] >= 0.0f ? 1.0f : -1.0f, gx[2] = { -dpx[0] * sg, dpx[2] }, gy[2] = { -dpy[0] * sg, dpy[2] };
        SurfS s = hexsample(set, -p[0] * sg * k, p[2] * k, gx, gy, sc[0]);
        for (i = 0; i < 3; i++)
            alb[i] += bw[1] * s.alb[i];
        h += bw[1] * s.h;
        dn[0] += bw[1] * -sg * s.n[0];
        dn[2] += bw[1] * s.n[1];
        tw += bw[1];
    }
    for (i = 0; i < 3; i++) {
        o.alb[i] = 1.0f + (alb[i] / tw - 1.0f) * sa[2];
        o.dn[i] = dn[i] / tw * sa[3];
    }
    o.h = h / tw;
    return o;
}

/* water's moving ripples (the shaders' surfripple): four deep-water waves, their slope at p, time tm */
static void ripple(float px, float py, float tm, float sl[2])
{
    static const float a[4] = { 0.3f, 1.9f, 3.4f, 5.0f }, lw[4] = { 0.9f, 1.6f, 2.7f, 4.3f }, am[4] = { 0.012f, 0.02f, 0.03f, 0.045f };
    int i;
    sl[0] = sl[1] = 0.0f;
    for (i = 0; i < 4; i++) {
        float dx = cosf(a[i]), dy = sinf(a[i]), k = 6.2831853f / lw[i], w = sqrtf(9.81f * k), c = cosf(k * (dx * px + dy * py) - w * tm);
        sl[0] += am[i] * k * dx * c;
        sl[1] += am[i] * k * dy * c;
    }
}

/* a region's surface: its variants' patches (light_surf.h); water's ripples; a card's detail in its own space */
static SurfR surfset(const float sa[4], const float sv[4], const float sk[4], const float sc[4], const float p[3], const float n[3],
                     const float dp0[3], const float dp1[3], const float misc[4], const float st[2], const float ds0[2], const float ds1[2])
{
    SurfR o, A, B;
    int nv, sl[3], i, j;
    float kk[3], br[3], u, f, t, w = 0.1f, hk = 0.35f, wb;
    o.alb[0] = o.alb[1] = o.alb[2] = 1.0f;
    o.h = 0.5f;
    o.dn[0] = o.dn[1] = o.dn[2] = 0.0f;
    if (sa[0] == -3.0f) {   /* water, flat: its ripples */
        if (fabsf(n[2]) > 0.6f) {
            float sl[2];
            ripple(p[0], p[1], misc[1], sl);
            o.dn[0] = -sl[0] * sa[3];
            o.dn[1] = -sl[1] * sa[3];
        }
        return o;
    }
    if (sa[0] < 0.0f)
        return o;
    if (misc[2] > 0.0f) {   /* in the texture's own space (a foliage card) */
        float gx[2] = { ds0[0] * misc[2], ds0[1] * misc[2] }, gy[2] = { ds1[0] * misc[2], ds1[1] * misc[2] };
        SurfS s = hexsample((int)(sa[0] + 0.5f), st[0] * misc[2], st[1] * misc[2], gx, gy, sc[0]);
        for (i = 0; i < 3; i++)
            o.alb[i] = 1.0f + (s.alb[i] - 1.0f) * sa[2];
        o.h = s.h;
        return o;
    }
    nv = (int)(sv[2] + 0.5f);
    if (nv < 2)
        return surfone((int)(sa[0] + 0.5f), sa[1], sa, sc, p, n, dp0, dp1);
    sl[0] = (int)(sa[0] + 0.5f); sl[1] = (int)(fmaxf(sv[0], 0.0f) + 0.5f); sl[2] = (int)(fmaxf(sv[1], 0.0f) + 0.5f);
    kk[0] = sa[1]; kk[1] = sk[0]; kk[2] = sk[1];
    br[0] = 1.0f; br[1] = sk[2]; br[2] = sk[3];
    u = vfbm(p[0] * sv[3] + sc[3] * 37.0f, p[1] * sv[3] + sc[3] * 37.0f);
    f = clampf((u - 0.5f) * 2.6f + 0.5f, 0.0f, 1.0f) * (float)(nv - 1);
    i = (int)f < nv - 2 ? (int)f : nv - 2;
    t = f - (float)i;
    if (t < 0.5f - w - hk || t > 0.5f + w + hk) {
        j = t < 0.5f ? i : i + 1;
        o = surfone(sl[j], kk[j], sa, sc, p, n, dp0, dp1);
        for (i = 0; i < 3; i++)
            o.alb[i] *= br[j];
        return o;
    }
    A = surfone(sl[i], kk[i], sa, sc, p, n, dp0, dp1);
    B = surfone(sl[i + 1], kk[i + 1], sa, sc, p, n, dp0, dp1);
    wb = smooth(0.5f - w, 0.5f + w, t + (B.h - A.h) * hk);
    for (j = 0; j < 3; j++) {
        o.alb[j] = A.alb[j] * br[i] * (1 - wb) + B.alb[j] * br[i + 1] * wb;
        o.dn[j] = A.dn[j] * (1 - wb) + B.dn[j] * wb;
    }
    o.h = A.h * (1 - wb) + B.h * wb;
    return o;
}

static float lin1(float c) { return c <= 0.04045f ? c / 12.92f : powf((c + 0.055f) / 1.055f, 2.4f); }
static float srgb1(float c) { c = clampf(c, 0.0f, 1.0f); return c <= 0.0031308f ? 12.92f * c : 1.055f * powf(c, 1.0f / 2.4f) - 0.055f; }

/* the map's texel, nearest and bilinear (wrapping), as the shaders' samplers */
static void mapread(const light_surf_map *m, float s, float t, float near[4], float lin[4], float var[2])
{
    float x = s * m->w - 0.5f, y = t * m->h - 0.5f, fx = floorf(x), fy = floorf(y), ax = x - fx, ay = y - fy;
    int x0 = ((int)fx % m->w + m->w) % m->w, y0 = ((int)fy % m->h + m->h) % m->h, x1 = (x0 + 1) % m->w, y1 = (y0 + 1) % m->h, c;
    int nx = (((int)floorf(s * m->w)) % m->w + m->w) % m->w, ny = (((int)floorf(t * m->h)) % m->h + m->h) % m->h;
    for (c = 0; c < 4; c++) {
        near[c] = m->map[(ny * m->w + nx) * 4 + c] / 255.0f;
        lin[c] = ((m->map[(y0 * m->w + x0) * 4 + c] * (1 - ax) + m->map[(y0 * m->w + x1) * 4 + c] * ax) * (1 - ay)
                  + (m->map[(y1 * m->w + x0) * 4 + c] * (1 - ax) + m->map[(y1 * m->w + x1) * 4 + c] * ax) * ay) / 255.0f;
    }
    for (c = 0; c < 2; c++)
        var[c] = ((m->var[(y0 * m->w + x0) * 2 + c] * (1 - ax) + m->var[(y0 * m->w + x1) * 2 + c] * ax) * (1 - ay)
                  + (m->var[(y1 * m->w + x0) * 2 + c] * (1 - ax) + m->var[(y1 * m->w + x1) * 2 + c] * ax) * ay) / 255.0f;
}

void light_surf_shade(const LightSurfU *u, int entry, const float st[2], const float p[3], const float n[3],
                      const float dp0[3], const float dp1[3], const float ds0[2], const float ds1[2],
                      const float ref[3], const float vc[3], float out[3], float dn[3], float *alpha)
{
    const light_surf_map *m = light_surf_entry_map(entry);
    float P[4], L[4], V[2], t, hue[3] = { 1, 1, 1 }, ca[3], cb[3], orig[3];
    int lo, hi, k, texel = u->flags[2] > 0.5f;
    SurfR a;
    *alpha = -1.0f;
    if (!m || !light_surf_sets_ready()) {
        memcpy(out, ref, sizeof(float) * 3);
        dn[0] = dn[1] = dn[2] = 0;
        return;
    }
    for (k = 0; k < 3; k++)
        orig[k] = texel ? lin1(ref[k]) : ref[k];
    mapread(m, st[0], st[1], P, L, V);
    V[0] += 0.5f;
    V[1] += 0.5f;
    lo = (int)(P[0] * 255.0f + 0.5f);
    hi = (int)(P[1] * 255.0f + 0.5f);
    t = L[2];
    if (u->flags[1] > 0.5f && !texel) {   /* a layout's vertex colour hue (the lighting's delit) */
        float lv = 0.2126f * vc[0] + 0.7152f * vc[1] + 0.0722f * vc[2];
        for (k = 0; k < 3; k++)
            hue[k] = lin1(clampf((lv > 0.02f ? clampf(vc[k] / lv, 0.0f, 2.0f) : 1.0f) * 0.5f, 0.0f, 1.0f)) * 4.672f;
    }
    a = surfset(u->sa[lo], u->sv[lo], u->sk[lo], u->sc[lo], p, n, dp0, dp1, u->misc, st, ds0, ds1);
    if (u->misc[0] > 0.5f)   /* a crisp silhouette (the shaders widen the edge by 0.75 of a pixel's change; here 0.02) */
        *alpha = smooth(0.48f, 0.52f, L[3]);
    for (k = 0; k < 3; k++)
        ca[k] = u->sa[lo][0] == -1.0f ? orig[k] : u->flags[1] > 0.5f ? orig[k] * a.alb[k]   /* a layout: its own texels, the grain over them */
                : u->sb[lo][k] * (1.0f + (V[0] - 1.0f) * u->sb[lo][3]) * a.alb[k] * hue[k];
    memcpy(out, ca, sizeof ca);
    memcpy(dn, a.dn, sizeof a.dn);
    if (hi != lo) {
        float soft = fmaxf(u->sc[lo][1], u->sc[hi][1]), hk = fmaxf(u->sc[lo][2], u->sc[hi][2]);
        float fw = fmaxf(0.0f, fmaxf(soft, 1e-3f));   /* (the shaders add 0.75 of t's screen derivative; one pixel here is the map's) */
        if (t > 0.5f - fw - hk) {
            SurfR b = surfset(u->sa[hi], u->sv[hi], u->sk[hi], u->sc[hi], p, n, dp0, dp1, u->misc, st, ds0, ds1);
            float wb = smooth(0.5f - fw, 0.5f + fw, t + (b.h - a.h) * hk);
            for (k = 0; k < 3; k++) {
                cb[k] = u->sa[hi][0] == -1.0f ? orig[k] : u->flags[1] > 0.5f ? orig[k] * b.alb[k]
                        : u->sb[hi][k] * (1.0f + (V[1] - 1.0f) * u->sb[hi][3]) * b.alb[k] * hue[k];
                out[k] = ca[k] * (1 - wb) + cb[k] * wb;
                dn[k] = a.dn[k] * (1 - wb) + b.dn[k] * wb;
            }
        }
    }
    if (u->flags[1] < 0.5f) {   /* the ground's broad light and dark, +-8% over some 120 m */
        float g = 0.92f + 0.16f * vfbm(p[0] * (1.0f / 120.0f) + 3.1f, p[1] * (1.0f / 120.0f) + 3.1f);
        for (k = 0; k < 3; k++)
            out[k] *= g;
    }
    if (u->flags[3] == 1.0f)
        for (k = 0; k < 3; k++)
            out[k] = 0.18f * a.alb[k];
    if (u->flags[3] == 2.0f) {
        float id = u->sc[(hi != lo && t > 0.5f) ? hi : lo][3], hh = fractf(id * 0.618034f);
        for (k = 0; k < 3; k++)
            out[k] = 0.5f * clampf(fabsf(fractf(hh + (k == 0 ? 0.0f : k == 1 ? 0.6667f : 0.3333f)) * 6.0f - 3.0f) - 1.0f, 0.0f, 1.0f);
    }
    if (texel) {
        /* the game's own lighting: the relief as a light from above and a little to one side, as the
           1999 art paints its light, then the texel, sRGB */
        float l[3] = { 0.42f, 0.33f, 0.84f }, sh = 1.0f + 0.8f * (dn[0] * l[0] + dn[1] * l[1] + dn[2] * l[2]);
        sh = clampf(sh, 0.6f, 1.4f);
        for (k = 0; k < 3; k++)
            out[k] = srgb1(u->flags[3] > 0.5f ? out[k] : out[k] * sh);
    } else
        for (k = 0; k < 3; k++)
            out[k] = fminf(out[k], 0.9f);
}
