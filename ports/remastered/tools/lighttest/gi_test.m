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
 *   fold      the GPU's fold of a bake's last pass's rays against this
 *             file's CPU fold of the same rays
 *   buried    a probe inside a solid box off, every one outside it on
 *   cost      a bake over a 131k triangle terrain 512 m square in sunlight,
 *             without lamps and with 128 along 1 km: its probes, its time and
 *             its memory
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
static double bake(BrrGi *g, const BrrRtWorld *w, float sp, int passes, const float sun[3], const float sunc[3],
                   const float sky[3], const float gnd[3], id<MTLBuffer> ls, int nl)
{
    if (!brr_gi_bake(g, s_q, w, sp, passes, sun, sunc, sky, gnd, ls, nl)) { printf("FAIL the bake\n"); exit(1); }
    return g->ms;
}
/* the distance atlas (half floats, 2 a texel) as floats */
static float *tex(id<MTLTexture> t)
{
    size_t n = t.width * t.height * 2;
    __fp16 *h = malloc(n * 2);
    float *p = malloc(n * sizeof(float));
    [t getBytes:h bytesPerRow:t.width * 4 fromRegion:MTLRegionMake2D(0, 0, t.width, t.height) mipmapLevel:0];
    for (size_t i = 0; i < n; i++) p[i] = (float)h[i];
    free(h);
    return p;
}
/* the probe at grid point gp (its number, -1 none), a probe's grid point and place */
static int probe_at(const BrrGi *g, const int gp[3])
{
    const BrrGiU *u = &g->u;
    const uint32_t *t = (const uint32_t *)((const char *)[g->off contents] + (size_t)u->bx[0] * 16);
    for (int k = 0; k < 3; k++) if (gp[k] < 0 || gp[k] >= u->n[k]) return -1;
    uint32_t v = t[gp[0] + u->n[0] * (gp[1] + u->n[1] * gp[2])];
    return v == 0xFFFFFFFFu ? -1 : (int)v;
}
static void probe_pos(const BrrGi *g, int s, float p[3])
{
    const int32_t *t = (const int32_t *)((const char *)[g->off contents] + (size_t)g->u.bx[1] * 16);
    for (int k = 0; k < 3; k++) p[k] = g->u.org[k] + t[4 * s + k] * g->u.org[3];
}
/* the probe at the grid point nearest a world point */
static int probe_near(const BrrGi *g, const float p[3])
{
    int gp[3];
    for (int k = 0; k < 3; k++) gp[k] = (int)lroundf((p[k] - g->u.org[k]) / g->u.org[3]);
    return probe_at(g, gp);
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
/* the last pass's batch (bk.z from bk.w probes) on the CPU: its probes'
 * coefficients folded over prev; the others kept */
static void cpu_sh(const BrrGiU *u, const float *ray, const float *prev, float *next)
{
    int np = u->bk[0], nr = u->n[3];
    memcpy(next, prev, (size_t)np * 36 * sizeof(float));
    for (int pl = u->bk[2]; pl < u->bk[2] + u->bk[3]; pl++) {
        int bi = pl - u->bk[2], nb = 0;
        double c[9][3] = { { 0 } }, y[9];
        for (int i = 0; i < nr; i++) {
            const float *r = ray + ((size_t)bi * nr + i) * 4;
            float d[3];
            rayd(u, i, d);
            y9(d, y);
            nb += r[3] < 0.0f;
            for (int k = 0; k < 9; k++)
                for (int q = 0; q < 3; q++) c[k][q] += r[q] * y[k];
        }
        double h = u->bx[2] == 0 ? 0.0 : u->par[0], sc = 4.0 * M_PI / nr;
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
/* the distance atlas (R texels a side, 2 channels) folded from the batch's
 * rays over prev; a texel of a tile outside the batch -1 (the GPU leaves it) */
static void cpu_fold(const BrrGiU *u, const float *ray, const float *prev, float *next, int W, int H, int R)
{
    int T = R + 2;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            int tx = x / T, ty = y / T, lx = x % T, ly = y % T, ix, iy, pl = tx + u->bk[1] * ty, bi = pl - u->bk[2], k, nb = 0;
            float dir[3], acc[2] = { 0, 0 }, ws = 0.0f;
            const float *old = prev + ((size_t)y * W + x) * 2;
            float *o = next + ((size_t)y * W + x) * 2;
            if (tx >= u->bk[1] || bi < 0 || bi >= u->bk[3]) { o[0] = o[1] = -1.0f; continue; }
            inner(lx, ly, R, &ix, &iy);
            unoct((ix + 0.5f) / R * 2.0f - 1.0f, (iy + 0.5f) / R * 2.0f - 1.0f, dir);
            for (int i = 0; i < u->n[3]; i++) {
                const float *r = ray + ((size_t)bi * u->n[3] + i) * 4;
                float d[3], c, w, t;
                rayd(u, i, d);
                c = fmaxf(dir[0] * d[0] + dir[1] * d[1] + dir[2] * d[2], 0.0f);
                nb += r[3] < 0.0f;   /* a back face's hit (gi_trace) */
                w = powf(c, 50.0f); t = fminf(fabsf(r[3]), u->lim[0]);
                acc[0] += w * t; acc[1] += w * t * t; ws += w;
            }
            if (nb > 0.25f * u->n[3]) { o[0] = o[1] = 0.0f; continue; }   /* inside the solid: off */
            if (ws <= 0.0f) { o[0] = old[0]; o[1] = old[1]; continue; }
            float h = u->bx[2] == 0 ? 0.0f : u->par[0];
            for (k = 0; k < 2; k++) {
                float nw = acc[k] / ws;
                o[k] = nw + (old[k] - nw) * h;
            }
        }
}
/* the CPU fold against the GPU's inside the batch: the max relative difference (the atlas is
 * half floats, 11 bits: the GPU rounds the old value read and the new one written, so up to 2 of
 * its units in the last place, 2^-10 each from 1 to 2) */
static float fold_diff(const float *cd, const float *gd, int n)
{
    float dd = 0.0f;
    for (int i = 0; i < n; i++)
        if (cd[i] >= 0.0f)
            dd = fmaxf(dd, fabsf(cd[i] - gd[i]) / fmaxf(1.0f, fabsf(cd[i])));
    return dd;
}

/* ---- the tests ---------------------------------------------------------------- */
static void check(const char *what, int ok) { if (!ok) { printf("FAIL %s\n", what); s_fail = 1; } }

/* a bake of passes, then the same bake one pass longer with its last pass's rays kept: the
   GPU's fold of that pass against the CPU's from the shorter bake's state (the bake is
   deterministic: the rotations follow the pass); returns the irradiance and distance diffs */
static void foldcheck(BrrGi *g, const BrrRtWorld *w, float sp, int passes, const float sun[3], const float sunc[3],
                      const float sky[3], const float gnd[3], id<MTLBuffer> ls, int nl, float *di, float *dd)
{
    bake(g, w, sp, passes, sun, sunc, sky, gnd, ls, nl);
    int np = g->np, NB = np * 36;
    float *prev = malloc((size_t)NB * 4), *pd = tex(brr_gi_dst(g));
    memcpy(prev, [brr_gi_sh(g) contents], (size_t)NB * 4);
    g->keepray = 1;
    bake(g, w, sp, passes + 1, sun, sunc, sky, gnd, ls, nl);
    g->keepray = 0;
    BrrGiU u = g->u;
    float *ray = [g->ray contents], *gi = [brr_gi_sh(g) contents], *gd = tex(brr_gi_dst(g));
    int Wd = (int)brr_gi_dst(g).width, Hd = (int)brr_gi_dst(g).height;
    float *ci = malloc((size_t)NB * 4), *cd = malloc((size_t)Wd * Hd * 8);
    cpu_sh(&u, ray, prev, ci);
    cpu_fold(&u, ray, pd, cd, Wd, Hd, BRR_GI_DST);
    *di = 0.0f;
    for (int i = 0; i < NB; i++) *di = fmaxf(*di, fabsf(ci[i] - gi[i]));
    *dd = fold_diff(cd, gd, Wd * Hd * 2);
    free(prev); free(pd); free(gd); free(ci); free(cd);
}

static void furnace(void)
{
    Mesh m = { 0 };
    BrrRtWorld w = { 0 };
    BrrGi g;
    float sun[3] = { 0, 0, 1 }, zero[3] = { 0, 0, 0 };
    box(&m, 3.0f, grey(128), grey(255));
    world(&w, &m, 1);
    brr_gi_init(&g, s_dev, 64);
    g.u.par[2] = 1.0f;                       /* the walls' radiance, cd/m2 */
    g.keep = 0.97f;
    bake(&g, &w, 2.0f, 300, sun, zero, zero, zero, nil, 0);
    float a = 128.0f / 255.0f, want = 3.14159265f / (1.0f - a);
    const float *irr = [brr_gi_sh(&g) contents];
    float err = 0.0f, lo = 1e30f, hi = 0.0f;
    int nin = 0;
    for (int pl = 0; pl < g.np; pl++) {   /* each probe inside the box: its irradiance over 64 normals */
        float p[3];
        probe_pos(&g, pl, p);
        if (fabsf(p[0]) >= 3.0f || fabsf(p[1]) >= 3.0f || fabsf(p[2]) >= 3.0f) continue;
        nin++;
        for (int i = 0; i < 64; i++) {
            float z = 1.0f - (2.0f * i + 1.0f) / 64.0f, r = sqrtf(fmaxf(1.0f - z * z, 0.0f)), ph = i * 2.39996323f, n[3] = { r * cosf(ph), r * sinf(ph), z };
            for (int k = 0; k < 3; k++) {
                float v = (float)sh_eval(irr, pl, n, k), e = fabsf(v / want - 1.0f);
                err = fmaxf(err, e); lo = fminf(lo, v); hi = fmaxf(hi, v);
            }
        }
    }
    printf("furnace    a %.4f: E/(1-a) %.5f lux, %d probes inside (of %d), %.5f .. %.5f, max rel err %.4f%%\n", a, want, nin, g.np, lo, hi, err * 100.0f);
    check("furnace: 27 probes inside, within 2%", nin == 27 && err < 0.02f);
    {
        float di, dd;
        foldcheck(&g, &w, 2.0f, 6, sun, zero, zero, zero, nil, 0, &di, &dd);
        printf("fold       GPU vs CPU on the same rays: irradiance coefficients max abs diff %.3g lux (of %.3f), distance max rel diff %.3g (half floats)\n", di, want, dd);
        check("fold irradiance", di < 1e-4f * want);
        check("fold distance", dd < 2e-3f);
    }
    brr_gi_release(&g); brr_rt_release(&w);
}

/* probe classification: a solid box 2 m across (its faces wound outward, as
 * the track's hills and walls), probes 2 m apart about it; the probe at its
 * centre sees only back faces and is off (its distances 0), every other
 * probe keeps its own; the fold on the CPU agrees */
static void buried(void)
{
    Mesh m = { 0 };
    BrrRtWorld w = { 0 };
    BrrGi g;
    float sun[3] = { 0, 0, 1 }, zero[3] = { 0, 0, 0 }, di, dd;
    for (int ax = 0; ax < 3; ax++)
        for (int sd = 0; sd < 2; sd++) {
            float o[3] = { -1, -1, -1 }, e1[3] = { 0 }, e2[3] = { 0 };
            o[ax] = sd ? 1 : -1;
            e1[(ax + (sd ? 1 : 2)) % 3] = 2; e2[(ax + (sd ? 2 : 1)) % 3] = 2;
            quad(&m, o, e1, e2, grey(128), grey(255));
        }
    world(&w, &m, 4);
    brr_gi_init(&g, s_dev, 64);
    g.u.par[2] = 1.0f;
    foldcheck(&g, &w, 2.0f, 4, sun, zero, zero, zero, nil, 0, &di, &dd);
    float *gd = tex(brr_gi_dst(&g));
    int Wd = (int)brr_gi_dst(&g).width, T = BRR_GI_DST + 2, off = 0, on = 0, cen = -1;
    for (int p = 0; p < g.np; p++) {   /* each probe: off when its tile's distances are all 0 */
        int tx = p % g.u.bk[1], ty = p / g.u.bk[1], z = 1;
        float pos[3];
        probe_pos(&g, p, pos);
        for (int y = 0; y < T; y++)
            for (int x = 0; x < T; x++)
                z &= gd[((size_t)(ty * T + y) * Wd + tx * T + x) * 2] == 0.0f;
        if (fabsf(pos[0]) < 0.5f && fabsf(pos[1]) < 0.5f && fabsf(pos[2]) < 0.5f) cen = z;
        else { off += z; on += !z; }
    }
    printf("buried     solid box: the probe inside %s; %d of %d outside it on; fold GPU vs CPU distance max rel diff %.3g\n",
           cen == 1 ? "off" : cen == 0 ? "ON" : "not placed", on, on + off, dd);
    check("buried probe off, the others on", cen == 1 && off == 0 && on > 0);
    check("buried fold", dd < 2e-3f);
    free(gd);
    brr_gi_release(&g); brr_rt_release(&w);
}

/* the sky by direction: open air (a 1 cm triangle so the world builds and
 * the probes are placed about it), a cube of radiance a + b z (Metal's face
 * axes; world z up is face +Z), irradiance in the cell beside the triangle
 * against the closed form pi a + (2 pi / 3) b n.z */
static void skycube(void)
{
    Mesh m = { 0 };
    BrrRtWorld w = { 0 };
    BrrGi g;
    const int N = 64;
    const float A = 1000.0f, B = 500.0f;
    float sun[3] = { 0, 0, 1 }, zero[3] = { 0, 0, 0 }, err = 0.0f;
    float ta[3] = { 900, 900, -900 }, tb[3] = { 900.01f, 900, -900 }, tc[3] = { 900, 900.01f, -900 }, P[3] = { 901, 901, -899 };
    tri(&m, ta, tb, tc, grey(128), 0);
    world(&w, &m, 1);
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
    {   /* the same field twice: from the uniform sky / ground blend (A + B z exactly), then from the cube */
        float q[6][8] = { { 0, 0, 0, 0, 0, 0, 1, 0 }, { 0, 0, 0, 0, 0, 0, -1, 0 }, { 0, 0, 0, 0, 1, 0, 0, 0 },
                          { 0, 0, 0, 0, 0, -1, 0, 0 }, { 0, 0, 0, 0, 0.6f, 0, 0.8f, 0 }, { 0, 0, 0, 0, 0, 0.8f, -0.6f, 0 } };
        float ref[6][4], out[6][4], up[3] = { A + B, A + B, A + B }, dn[3] = { A - B, A - B, A - B }, dax = 0.0f;
        for (int i = 0; i < 6; i++) memcpy(q[i], P, 12);
        brr_gi_init(&g, s_dev, 64);
        g.keep = 0.99f;
        bake(&g, &w, 2.0f, 400, sun, zero, up, dn, nil, 0);
        eval(&g, &q[0][0], 6, &ref[0][0]);
        {   /* the storage's error over the sphere: 256 normals (a Fibonacci set) against the closed form */
            enum { NS = 256 };
            float qs[NS][8], os[NS][4], worst = 0.0f, wz = 0.0f;
            for (int i = 0; i < NS; i++) {
                float z = 1.0f - (2.0f * i + 1.0f) / NS, r = sqrtf(fmaxf(1.0f - z * z, 0.0f)), ph = i * 2.39996323f;
                memset(qs[i], 0, sizeof qs[i]);
                memcpy(qs[i], P, 12);
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
        brr_gi_release(&g);   /* the cube's bake on the same rays, pass for pass (the rotations follow the pass) */
        brr_gi_init(&g, s_dev, 64);
        g.keep = 0.99f;
        brr_gi_env(&g, cube, 2.0f);
        bake(&g, &w, 2.0f, 400, sun, zero, zero, zero, nil, 0);
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
    brr_gi_init(&g, s_dev, 256);
    g.u.lim[3] = 0.0f;                       /* the first bounce alone */
    g.keep = 0.97f;
    bake(&g, &w, 2.0f, 200, sun, zero, zero, zero, ls, 1);
    const float *irr = [brr_gi_sh(&g) contents];
    const float c0[3] = { 0, 0, 0 };
    int cs0 = probe_near(&g, c0);
    float err = 0.0f, tr = 0.0f;
    /* the centre probe (cs0).  Its reference: the walls' radiance at the
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
        double ref = 0.0, got = cs0 >= 0 ? sh_eval(irr, cs0, d, 0) : 0.0, ex = lamp_ref(3.0, a, I, d);
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
        brr_gi_init(&g, s_dev, 64);
        g.u.par[2] = 1.0f; g.u.lim[1] = (float)ch;
        g.keep = 0.97f;
        bake(&g, &w, 2.0f, 300, sun, zero, zero, zero, nil, 0);
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
    brr_gi_init(&g, s_dev, 64);
    bake(&g, &w, 8.0f, 1, sun, zero, zero, zero, ls, 128);
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
        double best = 1e30;
        brr_gi_init(&g, s_dev, 64);
        for (int f = 0; f < 3; f++)
            best = fmin(best, bake(&g, &w, 8.0f, 12, sun, sunc, sky, gnd, lit ? ls : nil, lit ? 128 : 0));
        printf("cost       a bake: %d probes x 64 rays x 12 passes over %d triangles, %d lamps: %.1f ms (best of 3, CPU placement included), "
               "memory %.2f MB, BVH %.2f MB\n", g.np, m.n, lit ? 128 : 0, best, g.bytes / 1048576.0, w.bvh_bytes / 1048576.0);
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
        check("GiU is 18 float4", sizeof(BrrGiU) == 18 * 16);
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
