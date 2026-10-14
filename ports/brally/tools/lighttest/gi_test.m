/* gi_test.m: the irradiance probes (brr_metal_gi.h, brr_metal_gi.m) on this
 * Mac's GPU over synthetic worlds built by the renderer's own brr_rt_update.
 * Not the game.
 *
 *   furnace   a closed box of albedo a whose walls emit Le: every probe
 *             converges to pi Le / (1 - a), every bounce
 *   lamp      a point lamp at the box's centre, first bounce only: the
 *             centre probe's texels against a quadrature of the closed-form
 *             integrand over the walls
 *   leak      an emitter on one side of an opaque wall: irradiance sampled
 *             (gi_irr) on the dark side over that on the lit side, with and
 *             without the Chebyshev visibility; held under 1% from 0.3 m
 *             (closer than that the normal bias reaches through the wall,
 *             as in the paper)
 *   grid      the light grid: 128 lamps of random places and ranges binned on
 *             the GPU; every cube's list against the spheres of influence
 *             on the CPU (a lamp whose sphere misses the cube never listed,
 *             one that reaches it always, while the cube holds fewer than
 *             BRR_GI_CELL), and lit hits against every lamp of the set
 *   fold      the GPU's fold of one frame's rays against this file's CPU fold
 *             of the same rays
 *   cost      a 16 x 8 x 16 volume, 64 rays a probe, over a 131k triangle
 *             terrain in sunlight, without lamps and with 128 along 1 km:
 *             the GPU's time for one update (a quarter of the probes), and
 *             memory
 *
 * Usage: gi_test   (gi.sh) */
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "brr_metal_gi.h"
#include "brr_metal_lights.h"

static id<MTLDevice> s_dev;
static id<MTLCommandQueue> s_q;
static id<MTLComputePipelineState> s_eval;
static int s_fail;

/* ---- worlds ---------------------------------------------------------------- */
typedef struct Mesh { float *v; int n, cap; uint32_t *alb, *emit; } Mesh;

static void tri(Mesh *m, const float a[3], const float b[3], const float c[3], uint32_t alb, uint32_t emit)
{
    if (m->n == m->cap) {
        m->cap = m->cap ? m->cap * 2 : 64;
        m->v = realloc(m->v, (size_t)m->cap * 9 * sizeof(float));
        m->alb = realloc(m->alb, (size_t)m->cap * 4);
        m->emit = realloc(m->emit, (size_t)m->cap * 4);
    }
    memcpy(m->v + m->n * 9, a, 12); memcpy(m->v + m->n * 9 + 3, b, 12); memcpy(m->v + m->n * 9 + 6, c, 12);
    m->alb[m->n] = alb; m->emit[m->n] = emit; m->n++;
}
/* a quad o + s e1 + t e2, s, t in [0, 1] */
static void quad(Mesh *m, const float o[3], const float e1[3], const float e2[3], uint32_t alb, uint32_t emit)
{
    float a[3], b[3], c[3], d[3];
    for (int k = 0; k < 3; k++) { a[k] = o[k]; b[k] = o[k] + e1[k]; c[k] = o[k] + e1[k] + e2[k]; d[k] = o[k] + e2[k]; }
    tri(m, a, b, c, alb, emit); tri(m, a, c, d, alb, emit);
}
/* a closed box, its faces wound toward its inside (the track's faces wind
 * toward the space they are seen from: gi_fold's probe classification) */
static void box(Mesh *m, float h, uint32_t alb, uint32_t emit)
{
    float H = 2 * h;
    for (int ax = 0; ax < 3; ax++)
        for (int sd = 0; sd < 2; sd++) {
            float o[3] = { -h, -h, -h }, e1[3] = { 0 }, e2[3] = { 0 };
            o[ax] = sd ? h : -h;
            e1[(ax + (sd ? 2 : 1)) % 3] = H; e2[(ax + (sd ? 1 : 2)) % 3] = H;
            quad(m, o, e1, e2, alb, emit);
        }
}
static uint32_t grey(int g) { return (uint32_t)g | (uint32_t)g << 8 | (uint32_t)g << 16 | 0xff000000u; }

static int world(BrrRtWorld *w, Mesh *m, unsigned gen)
{
    LightTrack t;
    memset(&t, 0, sizeof t);
    t.tri = m->v; t.ntri = m->n; t.gen = gen;
    if (!brr_rt_update(w, s_dev, s_q, &t))
        return 0;
    LightBvhPrim *p = [w->prim contents];
    for (int i = 0; i < m->n; i++) { p[i].albedo = m->alb[i]; p[i].emit = m->emit[i]; }
    return 1;
}

/* ---- running ---------------------------------------------------------------- */
static double run(BrrGi *g, const BrrRtWorld *w, id<MTLBuffer> ls, int nl)
{
    id<MTLCommandBuffer> cb = [s_q commandBuffer];
    brr_gi_encode(g, cb, w, ls, nl);
    [cb commit];
    [cb waitUntilCompleted];
    if (cb.status != MTLCommandBufferStatusCompleted) { printf("FAIL command buffer: %s\n", [[cb.error description] UTF8String]); exit(1); }
    return (cb.GPUEndTime - cb.GPUStartTime) * 1e3;
}
static float *tex(id<MTLTexture> t, int ch)
{
    float *p = malloc(t.width * t.height * ch * sizeof(float));
    [t getBytes:p bytesPerRow:t.width * ch * sizeof(float) fromRegion:MTLRegionMake2D(0, 0, t.width, t.height) mipmapLevel:0];
    return p;
}
static void frame(BrrGi *g, float sp, const float sun[3], const float sunc[3])
{
    float eye[3] = { 0, 0, 0 }, z[3] = { 0, 0, 0 };
    brr_gi_frame(g, eye, sp, sun, sunc, z, z);
}
/* gi_irr at q (p, n pairs) */
static void eval(BrrGi *g, const float *q, int n, float *out)
{
    id<MTLBuffer> qb = [s_dev newBufferWithBytes:q length:(size_t)n * 32 options:MTLResourceStorageModeShared];
    id<MTLBuffer> ob = [s_dev newBufferWithLength:(size_t)n * 16 options:MTLResourceStorageModeShared];
    id<MTLCommandBuffer> cb = [s_q commandBuffer];
    id<MTLComputeCommandEncoder> e = [cb computeCommandEncoder];
    [e setComputePipelineState:s_eval];
    [e setBytes:&g->u length:sizeof g->u atIndex:0];
    [e setBuffer:qb offset:0 atIndex:1];
    [e setBuffer:ob offset:0 atIndex:2];
    [e setBuffer:brr_gi_off(g) offset:0 atIndex:3];
    [e setBuffer:brr_gi_sh(g) offset:0 atIndex:4];
    [e setTexture:brr_gi_dst(g) atIndex:1];
    [e dispatchThreads:MTLSizeMake((NSUInteger)n, 1, 1) threadsPerThreadgroup:MTLSizeMake(32, 1, 1)];
    [e endEncoding];
    [cb commit];
    [cb waitUntilCompleted];
    memcpy(out, [ob contents], (size_t)n * 16);
}

/* ---- the CPU's octahedral map and fold (brr_metal_gi.h's, in C) ---------------- */
static float snz(float v) { return v >= 0.0f ? 1.0f : -1.0f; }
static void unoct(float ex, float ey, float d[3])
{
    float v[3] = { ex, ey, 1.0f - fabsf(ex) - fabsf(ey) }, l;
    if (v[2] < 0.0f) { float x = v[0], y = v[1]; v[0] = (1.0f - fabsf(y)) * snz(x); v[1] = (1.0f - fabsf(x)) * snz(y); }
    l = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    d[0] = v[0] / l; d[1] = v[1] / l; d[2] = v[2] / l;
}
static void inner(int x, int y, int R, int *ix, int *iy)
{
    int E = R + 1;
    if ((x == 0 || x == E) && (y == 0 || y == E)) { x = x == 0 ? R : 1; y = y == 0 ? R : 1; }
    else if (y == 0 || y == E) { x = E - x; y = y == 0 ? 1 : R; }
    else if (x == 0 || x == E) { y = E - y; x = x == 0 ? 1 : R; }
    *ix = x - 1; *iy = y - 1;
}
static void rayd(const BrrGiU *u, int i, float d[3])
{
    float N = (float)u->n[3], z = 1.0f - (2.0f * i + 1.0f) / N, r = sqrtf(fmaxf(1.0f - z * z, 0.0f)), ph = i * 2.39996323f;
    float s[3] = { r * cosf(ph), r * sinf(ph), z };
    d[0] = u->rot0[0] * s[0] + u->rot0[1] * s[1] + u->rot0[2] * s[2];
    d[1] = u->rot1[0] * s[0] + u->rot1[1] * s[1] + u->rot1[2] * s[2];
    d[2] = u->rot2[0] * s[0] + u->rot2[1] * s[1] + u->rot2[2] * s[2];
}
static int imod(int a, int n) { return ((a % n) + n) % n; }
/* the irradiance's spherical harmonics on the CPU (brr_metal_gi_src.h gi_y9, gi_blend_sh) */
static void y9(const float d[3], double y[9])
{
    y[0] = 0.282095; y[1] = 0.488603 * d[1]; y[2] = 0.488603 * d[2]; y[3] = 0.488603 * d[0];
    y[4] = 1.092548 * d[0] * d[1]; y[5] = 1.092548 * d[1] * d[2]; y[6] = 0.315392 * (3.0 * d[2] * d[2] - 1.0);
    y[7] = 1.092548 * d[0] * d[2]; y[8] = 0.546274 * (d[0] * d[0] - d[1] * d[1]);
}
static double sh_eval(const float *sh, int slot, const float n[3], int ch)
{
    double y[9], e = 0.0;
    y9(n, y);
    for (int k = 0; k < 9; k++) e += sh[(9 * slot + k) * 4 + ch] * y[k];
    return e > 0.0 ? e : 0.0;
}
static void cpu_sh(const BrrGiU *u, const float *ray, const float *prev, float *next)
{
    int np = u->n[0] * u->n[1] * u->n[2], nr = u->n[3];
    for (int pl = 0; pl < np; pl++) {
        int s[3] = { pl % u->n[0], (pl / u->n[0]) % u->n[1], pl / (u->n[0] * u->n[1]) }, wv[3], fresh = u->pbase[3] == 0, nb = 0;
        int per = u->upd[0] > 1 ? u->upd[0] : 1;
        double c[9][3] = { { 0 } }, y[9];
        for (int k = 0; k < 3; k++) {
            wv[k] = u->base[k] + imod(s[k] - u->base[k], u->n[k]);
            if (wv[k] < u->pbase[k] || wv[k] >= u->pbase[k] + u->n[k]) fresh = 1;
        }
        memcpy(next + 36 * pl, prev + 36 * pl, 36 * sizeof(float));
        if (pl % per != u->upd[1] && !fresh) continue;
        for (int i = 0; i < nr; i++) {
            const float *r = ray + ((size_t)pl * nr + i) * 4;
            float d[3];
            rayd(u, i, d);
            y9(d, y);
            nb += r[3] < 0.0f;
            for (int k = 0; k < 9; k++)
                for (int q = 0; q < 3; q++) c[k][q] += r[q] * y[k];
        }
        double h = fresh ? 0.0 : pow(u->par[0], per), sc = 4.0 * M_PI / nr;
        for (int k = 0; k < 9; k++) {
            double A = k == 0 ? M_PI : k < 4 ? 2.0 * M_PI / 3.0 : M_PI / 4.0;
            for (int q = 0; q < 3; q++) {
                double nw = nb > 0.25 * nr ? 0.0 : c[k][q] * sc * A;
                next[(9 * pl + k) * 4 + q] = (float)(nb > 0.25 * nr ? 0.0 : nw + (prev[(9 * pl + k) * 4 + q] - nw) * h);
            }
            next[(9 * pl + k) * 4 + 3] = 0.0f;
        }
    }
}
/* one atlas (R texels a side, ch channels) folded from rays over prev */
static void cpu_fold(const BrrGiU *u, const float *ray, const float *prev, float *next, int W, int H, int R, int ch, int dist)
{
    int T = R + 2;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            int tx = x / T, ty = y / T, lx = x % T, ly = y % T, ix, iy, s[3], pl, k;
            float dir[3], acc[3] = { 0, 0, 0 }, ws = 0.0f;
            int nb = 0;
            const float *old = prev + ((size_t)y * W + x) * ch;
            float *o = next + ((size_t)y * W + x) * ch;
            s[0] = tx % u->n[0]; s[1] = ty; s[2] = tx / u->n[0];
            pl = s[0] + u->n[0] * (s[1] + u->n[1] * s[2]);
            inner(lx, ly, R, &ix, &iy);
            unoct((ix + 0.5f) / R * 2.0f - 1.0f, (iy + 0.5f) / R * 2.0f - 1.0f, dir);
            for (int i = 0; i < u->n[3]; i++) {
                const float *r = ray + ((size_t)pl * u->n[3] + i) * 4;
                float d[3], c;
                rayd(u, i, d);
                c = fmaxf(dir[0] * d[0] + dir[1] * d[1] + dir[2] * d[2], 0.0f);
                nb += r[3] < 0.0f;   /* a back face's hit (gi_trace) */
                if (dist) { float w = powf(c, 50.0f), t = fminf(fabsf(r[3]), u->lim[0]); acc[0] += w * t; acc[1] += w * t * t; ws += w; }
                else { for (k = 0; k < 3; k++) acc[k] += c * r[k]; ws += c; }
            }
            int wv[3], fresh = u->pbase[3] == 0, per = u->upd[0] > 1 ? u->upd[0] : 1;
            for (k = 0; k < 3; k++) {
                wv[k] = u->base[k] + imod(s[k] - u->base[k], u->n[k]);
                if (wv[k] < u->pbase[k] || wv[k] >= u->pbase[k] + u->n[k]) fresh = 1;
            }
            if (pl % per != u->upd[1] && !fresh) { for (k = 0; k < ch; k++) o[k] = old[k]; continue; }
            if (nb > 0.25f * u->n[3]) { for (k = 0; k < ch; k++) o[k] = k == 3 ? 1.0f : 0.0f; continue; }   /* inside the solid: off */
            if (ws <= 0.0f) { for (k = 0; k < ch; k++) o[k] = old[k]; continue; }
            float h = fresh ? 0.0f : powf(u->par[0], (float)per);
            for (k = 0; k < (dist ? 2 : 3); k++) {
                float nw = acc[k] / ws * (dist ? 1.0f : 3.14159265f);
                o[k] = nw + (old[k] - nw) * h;
            }
            if (!dist) o[3] = 1.0f;
        }
}

/* ---- the tests ---------------------------------------------------------------- */
static void check(const char *what, int ok) { if (!ok) { printf("FAIL %s\n", what); s_fail = 1; } }

static void furnace(void)
{
    Mesh m = { 0 };
    BrrRtWorld w = { 0 };
    BrrGi g;
    float sun[3] = { 0, 0, 1 }, zero[3] = { 0, 0, 0 };
    box(&m, 3.0f, grey(128), grey(255));
    world(&w, &m, 1);
    brr_gi_init(&g, s_dev, 3, 3, 3, 64);
    g.u.par[2] = 1.0f;                       /* the walls' radiance, cd/m2 */
    for (int f = 0; f < 800; f++) { frame(&g, 2.0f, sun, zero); run(&g, &w, nil, 0); }
    float a = 128.0f / 255.0f, want = 3.14159265f / (1.0f - a);
    int np = 27, NB = np * 36;
    float *irr = malloc((size_t)NB * 4), err = 0.0f, lo = 1e30f, hi = 0.0f;
    memcpy(irr, [brr_gi_sh(&g) contents], (size_t)NB * 4);
    for (int pl = 0; pl < np; pl++)   /* each probe's irradiance over 64 normals */
        for (int i = 0; i < 64; i++) {
            float z = 1.0f - (2.0f * i + 1.0f) / 64.0f, r = sqrtf(fmaxf(1.0f - z * z, 0.0f)), ph = i * 2.39996323f, n[3] = { r * cosf(ph), r * sinf(ph), z };
            for (int k = 0; k < 3; k++) {
                float v = (float)sh_eval(irr, pl, n, k), e = fabsf(v / want - 1.0f);
                err = fmaxf(err, e); lo = fminf(lo, v); hi = fmaxf(hi, v);
            }
        }
    printf("furnace    a %.4f: E/(1-a) %.5f lux, probes %.5f .. %.5f, max rel err %.4f%%\n", a, want, lo, hi, err * 100.0f);
    check("furnace within 2%", err < 0.02f);
    /* the fold on the CPU from the same rays: one more frame */
    {
        float *prev = malloc((size_t)NB * 4), *pd = tex(brr_gi_dst(&g), 2);
        memcpy(prev, [brr_gi_sh(&g) contents], (size_t)NB * 4);
        BrrGiU u;
        frame(&g, 2.0f, sun, zero);
        u = g.u;
        run(&g, &w, nil, 0);
        float *ray = [g.ray contents], *gi = [brr_gi_sh(&g) contents], *gd = tex(brr_gi_dst(&g), 2);
        int Wd = (int)brr_gi_dst(&g).width, Hd = (int)brr_gi_dst(&g).height;
        float *ci = malloc((size_t)NB * 4), *cd = malloc((size_t)Wd * Hd * 8), di = 0.0f, dd = 0.0f;
        cpu_sh(&u, ray, prev, ci);
        cpu_fold(&u, ray, pd, cd, Wd, Hd, BRR_GI_DST, 2, 1);
        for (int i = 0; i < NB; i++) di = fmaxf(di, fabsf(ci[i] - gi[i]));
        for (int i = 0; i < Wd * Hd * 2; i++) dd = fmaxf(dd, fabsf(cd[i] - gd[i]) / fmaxf(1.0f, fabsf(cd[i])));
        printf("fold       GPU vs CPU on the same rays: irradiance coefficients max abs diff %.3g lux (of %.3f), distance max rel diff %.3g\n", di, want, dd);
        check("fold irradiance", di < 1e-4f * want);
        check("fold distance", dd < 1e-4f);
        free(prev); free(pd); free(gd); free(ci); free(cd);
    }
    free(irr);
    brr_gi_release(&g); brr_rt_release(&w);
}

/* probe classification: a solid box (its faces wound outward, as the
 * track's hills and walls), 3 x 3 x 3 probes 2 m apart about it; the centre
 * probe inside sees only back faces and is off (its distances 0), every
 * other probe keeps its own; the fold on the CPU agrees */
static void buried(void)
{
    Mesh m = { 0 };
    BrrRtWorld w = { 0 };
    BrrGi g;
    float sun[3] = { 0, 0, 1 }, zero[3] = { 0, 0, 0 };
    for (int ax = 0; ax < 3; ax++)
        for (int sd = 0; sd < 2; sd++) {
            float o[3] = { -1, -1, -1 }, e1[3] = { 0 }, e2[3] = { 0 };
            o[ax] = sd ? 1 : -1;
            e1[(ax + (sd ? 1 : 2)) % 3] = 2; e2[(ax + (sd ? 2 : 1)) % 3] = 2;
            quad(&m, o, e1, e2, grey(128), grey(255));
        }
    world(&w, &m, 4);
    brr_gi_init(&g, s_dev, 3, 3, 3, 64);
    g.u.par[2] = 1.0f;
    for (int f = 0; f < 4; f++) { frame(&g, 2.0f, sun, zero); run(&g, &w, nil, 0); }
    float *pd = tex(brr_gi_dst(&g), 2);
    BrrGiU u;
    frame(&g, 2.0f, sun, zero);
    u = g.u;
    run(&g, &w, nil, 0);
    float *ray = [g.ray contents], *gd = tex(brr_gi_dst(&g), 2);
    int Wd = (int)brr_gi_dst(&g).width, Hd = (int)brr_gi_dst(&g).height, T = BRR_GI_DST + 2, off = 0, on = 0, cen = -1;
    float *cd = malloc((size_t)Wd * Hd * 8), dd = 0.0f;
    cpu_fold(&u, ray, pd, cd, Wd, Hd, BRR_GI_DST, 2, 1);
    for (int i = 0; i < Wd * Hd * 2; i++) dd = fmaxf(dd, fabsf(cd[i] - gd[i]) / fmaxf(1.0f, fabsf(cd[i])));
    for (int p = 0; p < 27; p++) {   /* each probe: off when its tile's distances are all 0 */
        int s[3] = { p % 3, (p / 3) % 3, p / 9 }, tx = s[0] + 3 * s[2], ty = s[1], z = 1;
        float pos[3];
        for (int k = 0; k < 3; k++) pos[k] = u.org[k] + (u.base[k] + imod(s[k] - u.base[k], 3)) * u.org[3];
        for (int y = 0; y < T; y++)
            for (int x = 0; x < T; x++)
                z &= gd[((size_t)(ty * T + y) * Wd + tx * T + x) * 2] == 0.0f;
        if (fabsf(pos[0]) < 0.5f && fabsf(pos[1]) < 0.5f && fabsf(pos[2]) < 0.5f) cen = z;
        else { off += z; on += !z; }
    }
    printf("buried     solid box: the probe inside %s; %d of %d outside it on; fold GPU vs CPU distance max rel diff %.3g\n",
           cen == 1 ? "off" : cen == 0 ? "ON" : "not in the volume", on, on + off, dd);
    check("buried probe off, the others on", cen == 1 && off == 0 && on == 26);
    check("buried fold", dd < 1e-4f);
    free(pd); free(gd); free(cd);
    brr_gi_release(&g); brr_rt_release(&w);
}

/* the sky by direction: open air (one far triangle so the world builds), a
 * cube of radiance a + b z (Metal's face axes; world z up is face +Z), a
 * probe's irradiance against the closed form pi a + (2 pi / 3) b n.z */
static void skycube(void)
{
    Mesh m = { 0 };
    BrrRtWorld w = { 0 };
    BrrGi g;
    const int N = 64;
    const float A = 1000.0f, B = 500.0f;
    float sun[3] = { 0, 0, 1 }, zero[3] = { 0, 0, 0 }, err = 0.0f;
    float ta[3] = { 900, 900, -900 }, tb[3] = { 901, 900, -900 }, tc[3] = { 900, 901, -900 };
    tri(&m, ta, tb, tc, grey(128), 0);
    world(&w, &m, 1);
    brr_gi_init(&g, s_dev, 3, 3, 3, 64);
    MTLTextureDescriptor *td = [MTLTextureDescriptor textureCubeDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float size:N mipmapped:NO];
    id<MTLTexture> cube = [s_dev newTextureWithDescriptor:td];
    float *px = malloc((size_t)N * N * 16);
    for (int f = 0; f < 6; f++) {
        for (int y = 0; y < N; y++)
            for (int x = 0; x < N; x++) {
                float u = 2.0f * (x + 0.5f) / N - 1.0f, v = 2.0f * (y + 0.5f) / N - 1.0f;
                float d[6][3] = { { 1, -v, -u }, { -1, -v, u }, { u, 1, v }, { u, -1, -v }, { u, -v, 1 }, { -u, -v, -1 } };
                float l = sqrtf(d[f][0] * d[f][0] + d[f][1] * d[f][1] + d[f][2] * d[f][2]), L = A + B * d[f][2] / l;
                float *q = px + ((size_t)y * N + x) * 4;
                q[0] = q[1] = q[2] = L; q[3] = 1.0f;
            }
        [cube replaceRegion:MTLRegionMake2D(0, 0, N, N) mipmapLevel:0 slice:(NSUInteger)f withBytes:px bytesPerRow:N * 16 bytesPerImage:(NSUInteger)N * N * 16];
    }
    free(px);
    {   /* the same field twice: from the uniform sky / ground blend (A + B z exactly), then from the cube;
           the probes' 8 x 8 octahedral texels hold either the same way (their bilinear lookup is off the
           closed form by up to 6% at oblique normals: the representation, which a CPU model reproduces) */
        float q[6][8] = { { 0, 0, 0, 0, 0, 0, 1, 0 }, { 0, 0, 0, 0, 0, 0, -1, 0 }, { 0, 0, 0, 0, 1, 0, 0, 0 },
                          { 0, 0, 0, 0, 0, -1, 0, 0 }, { 0, 0, 0, 0, 0.6f, 0, 0.8f, 0 }, { 0, 0, 0, 0, 0, 0.8f, -0.6f, 0 } };
        float ref[6][4], out[6][4], up[3] = { A + B, A + B, A + B }, dn[3] = { A - B, A - B, A - B }, eye[3] = { 0, 0, 0 }, dax = 0.0f;
        for (int f = 0; f < 800; f++) { brr_gi_frame(&g, eye, 2.0f, sun, zero, up, dn); run(&g, &w, nil, 0); }
        eval(&g, &q[0][0], 6, &ref[0][0]);
        {   /* the storage's error over the sphere: 256 normals (a Fibonacci set) at a probe against the closed form */
            enum { NS = 256 };
            float qs[NS][8], os[NS][4], worst = 0.0f, wz = 0.0f;
            for (int i = 0; i < NS; i++) {
                float z = 1.0f - (2.0f * i + 1.0f) / NS, r = sqrtf(fmaxf(1.0f - z * z, 0.0f)), ph = i * 2.39996323f;
                memset(qs[i], 0, sizeof qs[i]);
                qs[i][4] = r * cosf(ph); qs[i][5] = r * sinf(ph); qs[i][6] = z;
            }
            eval(&g, &qs[0][0], NS, &os[0][0]);
            for (int i = 0; i < NS; i++) {
                float e = fabsf(os[i][0] / (3.14159265f * A + 2.0943951f * B * qs[i][6]) - 1.0f);
                if (e > worst) { worst = e; wz = qs[i][6]; }
            }
            printf("storage    order-2 harmonics a probe: irradiance over 256 normals against the closed form, worst %.4f%% (at n.z %.2f)\n",
                   worst * 100.0f, wz);
            check("storage within 2% over the sphere", worst < 0.02f);
        }
        brr_gi_release(&g);   /* the cube's run on the same rays, frame for frame (the rotations follow the frame count) */
        brr_gi_init(&g, s_dev, 3, 3, 3, 64);
        brr_gi_env(&g, cube, 2.0f);
        for (int f = 0; f < 800; f++) { frame(&g, 2.0f, sun, zero); run(&g, &w, nil, 0); }
        eval(&g, &q[0][0], 6, &out[0][0]);
        for (int i = 0; i < 6; i++) {
            float want = 3.14159265f * A + 2.0943951f * B * q[i][6];
            err = fmaxf(err, fabsf(out[i][0] / ref[i][0] - 1.0f));
            if (i < 4)
                dax = fmaxf(dax, fabsf(out[i][0] / want - 1.0f));
        }
        printf("sky cube   radiance %g + %g z: up %.1f (closed form %.1f), down %.1f (%.1f); cube against the blend max rel diff %.3f%%, "
               "axes against the closed form %.2f%%\n", A, B, out[0][0], 3.14159265f * A + 2.0943951f * B, out[1][0],
               3.14159265f * A - 2.0943951f * B, err * 100.0f, dax * 100.0f);
        check("a miss sees the sky cube by direction: the cube as the analytic field, within 0.2%", err < 0.002f);
        check("sky cube: the axes within 1.5% of the closed form", dax < 0.015f);
    }
    brr_gi_release(&g); brr_rt_release(&w);
}

/* a lamp's first bounce: E(dir) at the box's centre, the lamp there too:
 * the walls' radiance a I cos / (pi r^2) times cos(dir, w) over their solid
 * angle, cos / r^2 dA, by the midpoint rule */
static double lamp_ref(double h, double a, double I, const float dir[3])
{
    const int N = 1200;
    double E = 0.0, dA = (2 * h / N) * (2 * h / N);
    for (int ax = 0; ax < 3; ax++)
        for (int sd = -1; sd <= 1; sd += 2)
            for (int j = 0; j < N; j++)
                for (int i = 0; i < N; i++) {
                    double p[3];
                    p[ax] = sd * h; p[(ax + 1) % 3] = -h + (i + 0.5) * 2 * h / N; p[(ax + 2) % 3] = -h + (j + 0.5) * 2 * h / N;
                    double r2 = p[0] * p[0] + p[1] * p[1] + p[2] * p[2], r = sqrt(r2), cy = h / r;
                    double cd = (dir[0] * p[0] + dir[1] * p[1] + dir[2] * p[2]) / r;
                    if (cd <= 0.0) continue;
                    E += a / M_PI * I * cy / r2 * cd * cy / r2 * dA;
                }
    return E;
}

static void lampbox(void)
{
    Mesh m = { 0 };
    BrrRtWorld w = { 0 };
    BrrGi g;
    float sun[3] = { 0, 0, 1 }, zero[3] = { 0, 0, 0 };
    id<MTLBuffer> ls = [s_dev newBufferWithLength:1 << 20 options:MTLResourceStorageModeShared];
    float *L = (float *)((char *)[ls contents] + 224);   /* LSet.l[0], after LCH */
    const float I = 100.0f;
    box(&m, 3.0f, grey(128), 0);
    world(&w, &m, 2);
    memset([ls contents], 0, 1 << 20);
    /* pos, kind 2 (a point), axis, cd, side, range, rgb, cosout */
    float rec[16] = { 0, 0, 0, 2.0f, 0, 0, 1, I, 1, 0, 0, 100.0f, 1, 1, 1, 0 };
    memcpy(L, rec, sizeof rec);
    brr_gi_init(&g, s_dev, 3, 3, 3, 256);
    g.u.lim[3] = 0.0f;                       /* the first bounce alone */
    for (int f = 0; f < 200; f++) { frame(&g, 2.0f, sun, zero); run(&g, &w, ls, 1); }
    const float *irr = [brr_gi_sh(&g) contents];
    float err = 0.0f, tr = 0.0f;
    /* the centre probe: grid (1, 1, 1), world index 0, slot 0.  Its reference: the walls' radiance at the
       centre (a / pi I h / r^3, r = h / max |w_i|) projected to order 2 on a fine Fibonacci set and convolved
       (the estimator's own expectation); its truncation against the exact first bounce (lamp_ref) apart */
    double a = 128.0 / 255.0, e0 = 0.0, cs[9] = { 0 }, y[9];
    const int NF = 200000;
    for (int i = 0; i < NF; i++) {
        float z = 1.0f - (2.0f * i + 1.0f) / NF, r = sqrtf(fmaxf(1.0f - z * z, 0.0f)), ph = i * 2.39996323f, d[3] = { r * cosf(ph), r * sinf(ph), z };
        double m = fmax(fabs(d[0]), fmax(fabs(d[1]), fabs(d[2]))), dist = 3.0 / m, Lw = a / M_PI * I * 3.0 / (dist * dist * dist);
        y9(d, y);
        for (int k = 0; k < 9; k++) cs[k] += Lw * y[k] * 4.0 * M_PI / NF;
    }
    for (int i = 0; i < 64; i++) {
        float z = 1.0f - (2.0f * i + 1.0f) / 64.0f, r = sqrtf(fmaxf(1.0f - z * z, 0.0f)), ph = i * 2.39996323f, d[3] = { r * cosf(ph), r * sinf(ph), z };
        double ref = 0.0, got = sh_eval(irr, 0, d, 0), ex = lamp_ref(3.0, a, I, d);
        y9(d, y);
        for (int k = 0; k < 9; k++) ref += cs[k] * (k == 0 ? M_PI : k < 4 ? 2.0 * M_PI / 3.0 : M_PI / 4.0) * y[k];
        if (!i) e0 = ex;
        err = fmaxf(err, (float)fabs(got / ref - 1.0));
        tr = fmaxf(tr, (float)fabs(ref / ex - 1.0));
    }
    printf("lamp       point lamp %.0f cd at the centre of a 6 m box, a %.4f: first bounce %.4f lux (normal 0); over 64 normals the probe against "
           "the field's order-2 projection max rel err %.3f%%, that projection's truncation against the exact bounce %.2f%%\n",
           I, a, e0, err * 100.0f, tr * 100.0f);
    check("lamp first bounce: the probe's coefficients within 0.2% of the field's", err < 0.002f);
    check("lamp truncation of order 2 within 3.5%", tr < 0.035f);
    brr_gi_release(&g); brr_rt_release(&w);
}

static void leak(void)
{
    Mesh m = { 0 };
    BrrRtWorld w = { 0 };
    float sun[3] = { 0, 0, 1 }, zero[3] = { 0, 0, 0 };
    /* the wall at x = 1 (between probe planes x = 0 and 2); the emitter below the lit side */
    float wo[3] = { 1.001f, -40, -40 }, we1[3] = { 0, 80, 0 }, we2[3] = { 0, 0, 80 };   /* the lit side's face (+x), 1 mm off the dark side's */
    float eo[3] = { -30, -30, -3 }, ee1[3] = { 30.5f, 0, 0 }, ee2[3] = { 0, 60, 0 };   /* facing up, toward the probes */
    float wo2[3] = { 1, 40, -40 }, we3[3] = { 0, -80, 0 };
    quad(&m, wo, we1, we2, grey(77), 0);
    quad(&m, wo2, we3, we2, grey(77), 0);   /* the wall's other side (-x, at x = 1): probes stand on both */
    quad(&m, eo, ee1, ee2, grey(1), grey(255));
    world(&w, &m, 3);
    for (int ch = 1; ch >= 0; ch--) {
        BrrGi g;
        brr_gi_init(&g, s_dev, 8, 4, 8, 64);
        g.u.par[2] = 1.0f; g.u.lim[1] = (float)ch;
        for (int f = 0; f < 300; f++) { frame(&g, 2.0f, sun, zero); run(&g, &w, nil, 0); }
        /* receivers facing down (toward the emitter's plane), d metres either side of the wall */
        float ds[4] = { 0.1f, 0.3f, 0.6f, 0.9f }, q[16 * 4], o[16 * 4], worst = 0.0f;
        int n = 0;
        for (int i = 0; i < 4; i++)
            for (int s = -1; s <= 1; s += 2) {
                float p[8] = { 1.0f + s * ds[i], 0.3f, -1.0f, 0, 0, 0, -1, 0 };
                memcpy(q + n * 8, p, 32); n++;
            }
        eval(&g, q, n, o);
        printf("leak       Chebyshev %s:", ch ? "on " : "off");
        for (int i = 0; i < 4; i++) {
            float lit = o[(2 * i) * 4], dark = o[(2 * i + 1) * 4], f = dark / fmaxf(lit, 1e-9f);
            printf("  %.1f m: %.4f/%.4f = %.2f%%", ds[i], dark, lit, f * 100.0f);
            if (ds[i] > 0.2f) worst = fmaxf(worst, f);
        }
        printf("  (worst from 0.3 m %.2f%%)\n", worst * 100.0f);
        if (ch)
            check("leak through the wall under 1% from 0.3 m with Chebyshev", worst < 0.01f);
        brr_gi_release(&g);
    }
    brr_rt_release(&w);
}

/* the light grid against the spheres on the CPU */
static void gridtest(void)
{
    Mesh m = { 0 };
    BrrRtWorld w = { 0 };
    BrrGi g;
    float sun[3] = { 0, 0, 1 }, zero[3] = { 0, 0, 0 };
    id<MTLBuffer> ls = [s_dev newBufferWithLength:sizeof(float) * 16 * 128 + 224 options:MTLResourceStorageModeShared];
    float *L = (float *)((char *)[ls contents] + 224);
    uint32_t r = 777u;
#define RN() ((float)((r = r * 1664525u + 1013904223u) >> 8) / 16777216.0f)
    memset([ls contents], 0, ls.length);
    for (int i = 0; i < 128; i++) {
        float rec[16] = { -300 + 600 * RN(), -300 + 600 * RN(), -100 + 200 * RN(), 2.0f, 0, 0, -1, 100 + 1000 * RN(),
                          1, 0, 0, 3 + 37 * RN(), 1, 1, 1, 0 };
        memcpy(L + i * 16, rec, sizeof rec);
    }
    box(&m, 3.0f, grey(128), 0);
    world(&w, &m, 5);
    brr_gi_init(&g, s_dev, 16, 8, 16, 64);
    frame(&g, 8.0f, sun, zero);
    run(&g, &w, ls, 128);
    const BrrGiU *u = &g.u;
    const uint32_t *cnt = [g.gcnt contents];
    const uint16_t *lst = [g.glst contents];
    int nc = u->gdim[0] * u->gdim[1] * u->gdim[2], wrong = 0, full = 0, listed = 0, checked = 0;
    for (int c = 0; c < nc; c++) {
        int cx = c % u->gdim[0], cy = (c / u->gdim[0]) % u->gdim[1], cz = c / (u->gdim[0] * u->gdim[1]);
        float lo[3] = { u->gorg[0] + cx * u->gorg[3], u->gorg[1] + cy * u->gorg[3], u->gorg[2] + cz * u->gorg[3] };
        int in[128] = { 0 };
        if (cnt[c] > BRR_GI_CELL) { wrong++; continue; }
        for (unsigned k = 0; k < cnt[c]; k++) in[lst[c * 32 + k]] = 1;
        listed += (int)cnt[c];
        if (cnt[c] == BRR_GI_CELL) { full++; }
        for (int i = 0; i < 128; i++) {
            const float *l = L + i * 16;
            double d2 = 0.0, R = l[11];
            for (int k = 0; k < 3; k++) {
                double q = l[k], e = q < lo[k] ? lo[k] - q : q > lo[k] + u->gorg[3] ? q - lo[k] - u->gorg[3] : 0.0;
                d2 += e * e;
            }
            double gap = sqrt(d2) - R;            /* < 0: the sphere reaches the cube */
            if (gap > 1e-3 && in[i]) wrong++;    /* misses it: never listed */
            if (gap < -1e-3 && !in[i] && cnt[c] < BRR_GI_CELL) wrong++;   /* reaches it: always */
            checked++;
        }
    }
    printf("grid       %d x %d x %d cubes of %.1f m, 128 lamps: %d lamp-cube pairs checked, %d listed, %d cubes full, %d wrong\n",
           u->gdim[0], u->gdim[1], u->gdim[2], u->gorg[3], checked, listed, full, wrong);
    check("grid lists exactly the lamps that reach a cube", wrong == 0 && listed > 0);
    brr_gi_release(&g); brr_rt_release(&w);
}

static void cost(void)
{
    Mesh m = { 0 };
    BrrRtWorld w = { 0 };
    BrrGi g;
    const int N = 256;
    const float S = 512.0f / N;
    float sun[3] = { 0.4f, 0.3f, 0.866f }, sunc[3] = { 100000, 95000, 90000 }, sky[3] = { 3000, 4000, 6000 }, gnd[3] = { 500, 500, 500 };
    for (int j = 0; j < N; j++)
        for (int i = 0; i < N; i++) {
            float x = -256 + i * S, y = -256 + j * S;
#define HT(X, Y) (6.0f * sinf((X) * 0.031f) * cosf((Y) * 0.027f) + 1.5f * sinf((X) * 0.17f + (Y) * 0.11f))
            float a[3] = { x, y, HT(x, y) }, b[3] = { x + S, y, HT(x + S, y) }, c[3] = { x + S, y + S, HT(x + S, y + S) }, d[3] = { x, y + S, HT(x, y + S) };
            tri(&m, a, b, c, grey(90), 0); tri(&m, a, c, d, grey(90), 0);
        }
    world(&w, &m, 4);
    /* the night's set: 128 lamps (the set's most) spread along a 1 km line
       through the volume, points and spots, 20 .. 60 m range */
    id<MTLBuffer> ls = [s_dev newBufferWithLength:sizeof(float) * 16 * 128 + 224 options:MTLResourceStorageModeShared];
    memset([ls contents], 0, ls.length);
    {
        float *L = (float *)((char *)[ls contents] + 224);
        uint32_t r = 12345u;
        for (int i = 0; i < 128; i++) {
            float x = -500.0f + 1000.0f * (i + 0.5f) / 128.0f, y = ((int)(r = r * 1664525u + 1013904223u) >> 8) / 8388608.0f * 30.0f;
            float rec[16] = { x, y, 5.0f, i & 1 ? 3.0f : 2.0f, 0, 0, -1, 3000.0f, 0.95f, 0, 0, 20.0f + (i % 5) * 10.0f, 1, 0.8f, 0.6f, 0.5f };
            memcpy(L + i * 16, rec, sizeof rec);
        }
    }
    for (int lit = 0; lit < 2; lit++) {
        brr_gi_init(&g, s_dev, 16, 8, 16, 64);
        double best = 1e30, sum = 0.0;
        int nt = 0;
        for (int f = 0; f < 40; f++) {
            float eye[3] = { f * 0.5f, 0, 4 };
            brr_gi_frame(&g, eye, 8.0f, sun, sunc, sky, gnd);
            double ms = run(&g, &w, lit ? ls : nil, lit ? 128 : 0);
            if (f >= 10) { best = fmin(best, ms); sum += ms; nt++; }
        }
        printf("cost       %d probes x %d rays over %d triangles, %d lamps: %.3f ms a frame (best %.3f), GPU memory %.2f MB (atlases x2 + rays), BVH %.2f MB\n",
               16 * 8 * 16, 64, m.n, lit ? 128 : 0, sum / nt, best, g.bytes / 1048576.0, w.bvh_bytes / 1048576.0);
        brr_gi_release(&g);
    }
    brr_rt_release(&w);
}

static const char *k_eval =
"kernel void gi_eval(constant GiU &u [[buffer(0)]], device const float4 *q [[buffer(1)]], device float4 *o [[buffer(2)]],\n"
"                    device const float4 *off [[buffer(3)]], device const float4 *sh [[buffer(4)]],\n"
"                    texture2d<float> dst [[texture(1)]], uint i [[thread_position_in_grid]]) {\n"
"  float3 p = q[2 * i].xyz, n = q[2 * i + 1].xyz; o[i] = float4(gi_irr(u, off, sh, dst, p, n, n), 0); }\n";

int main(void)
{
    @autoreleasepool {
        NSError *err = nil;
        s_dev = MTLCreateSystemDefaultDevice();
        s_q = [s_dev newCommandQueue];
        id<MTLLibrary> lib = brr_rt_library(s_dev, [NSString stringWithFormat:@"%s%s%s%s", BRR_GI_DEFS, BRR_LIGHTS_SRC, BRR_GI_SRC, k_eval], &err);
        if (!lib) { printf("FAIL compile: %s\n", [[err description] UTF8String]); return 1; }
        s_eval = [s_dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"gi_eval"] error:&err];
        check("GiU is 16 float4", sizeof(BrrGiU) == 16 * 16);
        furnace();
        buried();
        skycube();
        lampbox();
        leak();
        gridtest();
        cost();
        printf(s_fail ? "gi: FAIL\n" : "gi: all passed\n");
        return s_fail;
    }
}
