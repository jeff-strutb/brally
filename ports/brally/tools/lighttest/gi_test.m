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
 *   fold      the GPU's fold of one frame's rays against this file's CPU fold
 *             of the same rays
 *   cost      a 16 x 8 x 16 volume, 64 rays a probe, over a 131k triangle
 *             terrain in sunlight: the GPU's time for one update, and memory
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
static void box(Mesh *m, float h, uint32_t alb, uint32_t emit)
{
    float H = 2 * h;
    for (int ax = 0; ax < 3; ax++)
        for (int sd = 0; sd < 2; sd++) {
            float o[3] = { -h, -h, -h }, e1[3] = { 0 }, e2[3] = { 0 };
            o[ax] = sd ? h : -h;
            e1[(ax + 1) % 3] = H; e2[(ax + 2) % 3] = H;
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
    [e setTexture:brr_gi_irr(g) atIndex:0];
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
/* one atlas (R texels a side, ch channels) folded from rays over prev */
static void cpu_fold(const BrrGiU *u, const float *ray, const float *prev, float *next, int W, int H, int R, int ch, int dist)
{
    int T = R + 2;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            int tx = x / T, ty = y / T, lx = x % T, ly = y % T, ix, iy, s[3], pl, k;
            float dir[3], acc[3] = { 0, 0, 0 }, ws = 0.0f;
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
                if (dist) { float w = powf(c, 50.0f), t = fminf(r[3], u->lim[0]); acc[0] += w * t; acc[1] += w * t * t; ws += w; }
                else { for (k = 0; k < 3; k++) acc[k] += c * r[k]; ws += c; }
            }
            if (ws <= 0.0f) { for (k = 0; k < ch; k++) o[k] = old[k]; continue; }
            int wv[3], fresh = u->pbase[3] == 0;
            for (k = 0; k < 3; k++) {
                wv[k] = u->base[k] + imod(s[k] - u->base[k], u->n[k]);
                if (wv[k] < u->pbase[k] || wv[k] >= u->pbase[k] + u->n[k]) fresh = 1;
            }
            float h = fresh ? 0.0f : u->par[0];
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
    float *irr = tex(brr_gi_irr(&g), 4), err = 0.0f, lo = 1e30f, hi = 0.0f;
    int W = (int)brr_gi_irr(&g).width, H = (int)brr_gi_irr(&g).height;
    for (int i = 0; i < W * H; i++)
        for (int k = 0; k < 3; k++) {
            float e = fabsf(irr[i * 4 + k] / want - 1.0f);
            err = fmaxf(err, e); lo = fminf(lo, irr[i * 4 + k]); hi = fmaxf(hi, irr[i * 4 + k]);
        }
    printf("furnace    a %.4f: E/(1-a) %.5f lux, probes %.5f .. %.5f, max rel err %.4f%%\n", a, want, lo, hi, err * 100.0f);
    check("furnace within 2%", err < 0.02f);
    /* the fold on the CPU from the same rays: one more frame */
    {
        float *prev = tex(brr_gi_irr(&g), 4), *pd = tex(brr_gi_dst(&g), 2);
        BrrGiU u;
        frame(&g, 2.0f, sun, zero);
        u = g.u;
        run(&g, &w, nil, 0);
        float *ray = [g.ray contents], *gi = tex(brr_gi_irr(&g), 4), *gd = tex(brr_gi_dst(&g), 2);
        int Wd = (int)brr_gi_dst(&g).width, Hd = (int)brr_gi_dst(&g).height;
        float *ci = malloc((size_t)W * H * 16), *cd = malloc((size_t)Wd * Hd * 8), di = 0.0f, dd = 0.0f;
        cpu_fold(&u, ray, prev, ci, W, H, BRR_GI_IRR, 4, 0);
        cpu_fold(&u, ray, pd, cd, Wd, Hd, BRR_GI_DST, 2, 1);
        for (int i = 0; i < W * H * 4; i++) di = fmaxf(di, fabsf(ci[i] - gi[i]));
        for (int i = 0; i < Wd * Hd * 2; i++) dd = fmaxf(dd, fabsf(cd[i] - gd[i]) / fmaxf(1.0f, fabsf(cd[i])));
        printf("fold       GPU vs CPU on the same rays: irradiance max abs diff %.3g lux (of %.3f), distance max rel diff %.3g\n", di, want, dd);
        check("fold irradiance", di < 1e-4f * want);
        check("fold distance", dd < 1e-4f);
        free(prev); free(pd); free(gi); free(gd); free(ci); free(cd);
    }
    free(irr);
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
    float *irr = tex(brr_gi_irr(&g), 4), err = 0.0f;
    int W = (int)brr_gi_irr(&g).width;
    /* the centre probe: grid (1, 1, 1), world index 0, slot 0, tile 0 */
    double a = 128.0 / 255.0, e0 = 0.0;
    for (int iy = 0; iy < BRR_GI_IRR; iy++)
        for (int ix = 0; ix < BRR_GI_IRR; ix++) {
            float d[3];
            unoct((ix + 0.5f) / BRR_GI_IRR * 2 - 1, (iy + 0.5f) / BRR_GI_IRR * 2 - 1, d);
            double ref = lamp_ref(3.0, a, I, d), got = irr[((iy + 1) * W + ix + 1) * 4];
            if (!ix && !iy) e0 = ref;
            err = fmaxf(err, (float)fabs(got / ref - 1.0));
        }
    printf("lamp       point lamp %.0f cd at the centre of a 6 m box, a %.4f: first bounce %.4f lux (texel 0), 64 texels max rel err %.3f%%\n",
           I, a, e0, err * 100.0f);
    check("lamp first bounce within 2%", err < 0.02f);
    free(irr);
    brr_gi_release(&g); brr_rt_release(&w);
}

static void leak(void)
{
    Mesh m = { 0 };
    BrrRtWorld w = { 0 };
    float sun[3] = { 0, 0, 1 }, zero[3] = { 0, 0, 0 };
    /* the wall at x = 1 (between probe planes x = 0 and 2); the emitter below the lit side */
    float wo[3] = { 1, -40, -40 }, we1[3] = { 0, 80, 0 }, we2[3] = { 0, 0, 80 };
    float eo[3] = { -30, -30, -3 }, ee1[3] = { 0, 60, 0 }, ee2[3] = { 30.5f, 0, 0 };
    quad(&m, wo, we1, we2, grey(77), 0);
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
    brr_gi_init(&g, s_dev, 16, 8, 16, 64);
    double best = 1e30, sum = 0.0;
    int nt = 0;
    for (int f = 0; f < 40; f++) {
        float eye[3] = { f * 0.5f, 0, 4 };
        brr_gi_frame(&g, eye, 8.0f, sun, sunc, sky, gnd);
        double ms = run(&g, &w, nil, 0);
        if (f >= 10) { best = fmin(best, ms); sum += ms; nt++; }
    }
    printf("cost       %d probes x %d rays over %d triangles: %.3f ms a frame (best %.3f), GPU memory %.2f MB (atlases x2 + rays), BVH %.2f MB\n",
           16 * 8 * 16, 64, m.n, sum / nt, best, g.bytes / 1048576.0, w.bvh_bytes / 1048576.0);
    brr_gi_release(&g); brr_rt_release(&w);
}

static const char *k_eval =
"kernel void gi_eval(constant GiU &u [[buffer(0)]], device const float4 *q [[buffer(1)]], device float4 *o [[buffer(2)]],\n"
"                    texture2d<float> irr [[texture(0)]], texture2d<float> dst [[texture(1)]], uint i [[thread_position_in_grid]]) {\n"
"  float3 p = q[2 * i].xyz, n = q[2 * i + 1].xyz; o[i] = float4(gi_irr(u, irr, dst, p, n, n), 0); }\n";

int main(void)
{
    @autoreleasepool {
        NSError *err = nil;
        s_dev = MTLCreateSystemDefaultDevice();
        s_q = [s_dev newCommandQueue];
        id<MTLLibrary> lib = brr_rt_library(s_dev, [NSString stringWithFormat:@"%s%s%s%s", BRR_GI_DEFS, BRR_LIGHTS_SRC, BRR_GI_SRC, k_eval], &err);
        if (!lib) { printf("FAIL compile: %s\n", [[err description] UTF8String]); return 1; }
        s_eval = [s_dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"gi_eval"] error:&err];
        check("GiU is 13 float4", sizeof(BrrGiU) == 13 * 16);
        furnace();
        lampbox();
        leak();
        cost();
        printf(s_fail ? "gi: FAIL\n" : "gi: all passed\n");
        return s_fail;
    }
}
