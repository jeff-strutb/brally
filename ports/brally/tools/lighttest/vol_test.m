/* vol_test.m: the volumetric fog (brr_metal_vol.h) against closed forms and
 * its CPU reference; not the game.
 *
 *   homogeneous  a uniform medium lit by a uniform isotropic ambient,
 *                integrated over the grid's slices: transmittance against
 *                exp(-sigma_t d), in-scattering against
 *                L_amb albedo (1 - exp(-sigma_t d)), at every slice's far side
 *   steps        the energy-conserving step (Hillaire 2015) against the
 *                naive Riemann sum at coarse slices: a uniform fog and a
 *                height fog on a sloping ray, against a fine reference
 *   phase        Henyey-Greenstein integrates to 1 over the sphere
 *   gpu          the renderer's own kernels (the shader source as the
 *                renderer builds it, argv[1]): vol_inject against the CPU's
 *                cell (sun, sky, a lamp, height falloff; noise off, then its
 *                bounds with it on), the reprojection's lookup, vol_integrate
 *                against vol_step, vol_at's lookup at slice sides, and the
 *                homogeneous closed form through the GPU; the kernels' time
 *   beam         the lighthouse beam's single scattering (vol_beam): the
 *                march against a fine quadrature of the same integral over
 *                the whole ray for 8 rays crossing the beam at different
 *                distances and angles (< 2% at every jitter), zero for rays
 *                that miss the cone, the change for a 1 degree turn of the
 *                beam (< 5%), the shader's vol_beam against the C one, and
 *                the beam's luminance seen side-on (night's medium, the
 *                lighthouse's 2,000,000 cd)
 *
 * Build and run: ports/brally/tools/lighttest/vol.sh */
#import <Metal/Metal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "light.h"
#include "light_cluster.h"
#include "brr_metal_vol.h"

static int g_fail;
#define CHECK(c, ...)                                         \
    do {                                                      \
        if (!(c)) {                                           \
            g_fail++;                                         \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);       \
            printf(__VA_ARGS__);                              \
            printf("\n");                                     \
        }                                                     \
    } while (0)

/* brr_metal.m's LU, the same layout as the shaders' */
typedef struct LU { float sunvp[4][16]; float csplit[4], cfwd[4], ctexel[4], cbias[4]; float eye[4], sun[4], sunc[4], skyc[4], gndc[4], misc[4], wb[4], bg[4], fogc[4];
                   float cdepth[4], csoft[4], scr[16], vol[4]; } LU;

static void homogeneous(void)
{
    static const float sig[3] = { 0.026f, 0.004f, 0.0002f };
    float worst_t = 0, worst_s = 0, alb = 0.9f, L = 1000.0f;
    int c, k;
    for (c = 0; c < 3; c++) {
        float S = 0, T = 1;
        for (k = 0; k < VOL_NZ; k++) {
            float t0 = vol_t0(k), t1 = vol_t1(k), et = expf(-sig[c] * t1);
            vol_step(sig[c], sig[c] * alb * L, t1 - t0, &S, &T);
            worst_t = fmaxf(worst_t, fabsf(T - et) / et);
            if (1.0f - et > 1e-4f)
                worst_s = fmaxf(worst_s, fabsf(S - L * alb * (1.0f - et)) / (L * alb * (1.0f - et)));
        }
    }
    printf("homogeneous: transmittance max rel error %.2e, ambient in-scatter max rel error %.2e (64 slices to %g m; fog, rain, haze)\n",
           worst_t, worst_s, VOL_FAR);
    CHECK(worst_t < 0.005f, "transmittance %g", worst_t);
    CHECK(worst_s < 0.005f, "in-scatter %g", worst_s);
}

/* a reference by fine midpoint steps of the exact ODE solution per step (2e5 steps) */
static void steps(void)
{
    static const int ns[3] = { 8, 16, 64 };
    int case_, j;
    for (case_ = 0; case_ < 2; case_++) {
        /* case 0: uniform fog sigma 0.026, ambient light 1; case 1: height fog 0.026 thinning over 40 m on a ray rising 0.2 */
        float sg = 0.026f, alb = 0.98f, L = 1.0f;
        double Sref = 0, Tref = 1;
        long i, nf = 200000;
        for (i = 0; i < nf; i++) {
            double t = ((double)i + 0.5) * VOL_FAR / nf, ds = VOL_FAR / (double)nf;
            double st = case_ ? sg * exp(-fmax(0.2 * t, 0.0) / 40.0) : sg;
            double tr = exp(-st * ds);
            Sref += Tref * (st * alb * L) * (1.0 - tr) / st;
            Tref *= tr;
        }
        for (j = 0; j < 3; j++) {
            float Se = 0, Te = 1, Sn = 0, Tn = 1;
            int k, n = ns[j];
            for (k = 0; k < n; k++) {
                float t0 = k ? VOL_NEAR * powf(VOL_FAR / VOL_NEAR, (float)k / n) : 0.0f, t1 = VOL_NEAR * powf(VOL_FAR / VOL_NEAR, (float)(k + 1) / n);
                float tm = 0.5f * (t0 + t1), st = case_ ? sg * expf(-0.2f * tm / 40.0f) : sg;
                vol_step(st, st * alb * L, t1 - t0, &Se, &Te);
                vol_step_naive(st, st * alb * L, t1 - t0, &Sn, &Tn);
            }
            {
                double ee = fabs(Se - Sref) / Sref, en = fabs(Sn - Sref) / Sref;
                printf("steps: %s, %2d slices: in-scatter to %g m rel error energy-conserving %.2e, naive Riemann %.2e (%.0fx less)\n",
                       case_ ? "height fog " : "uniform fog", n, VOL_FAR, ee, en, en / fmax(ee, 1e-9));
                CHECK(ee < en, "the energy-conserving step no better at %d slices", n);
                if (!case_)
                    CHECK(ee < 1e-3, "uniform: %g", ee);
            }
        }
    }
}

static void phase(void)
{
    static const float g[5] = { 0.0f, 0.3f, VOL_G, 0.9f, -0.5f };
    int j;
    long i, n = 400000;
    for (j = 0; j < 5; j++) {
        double s = 0;
        for (i = 0; i < n; i++) {   /* over the cosine (the azimuth's 2 pi), midpoint */
            double c = -1.0 + 2.0 * ((double)i + 0.5) / n;
            s += 2.0 * 3.14159265358979 * vol_hg(g[j], (float)c) * 2.0 / n;
        }
        printf("phase: Henyey-Greenstein g %+.2f integrates to %.6f over the sphere\n", g[j], s);
        CHECK(fabs(s - 1.0) < 1e-3, "HG %g: %g", g[j], s);
    }
}

/* ---- the GPU ---- */

/* the test camera: the eye at E looking along +y, f pixels focal length, the target W x H */
#define TW 1280.0f
#define TH 720.0f
static const float E[3] = { 10.0f, -5.0f, 3.0f }, F = 700.0f;
static void screen_map(float s[16])
{
    /* pixel x = (f (x - Ex) + cx (y - Ey)) / (y - Ey), pixel y = (-f (z - Ez) + cy (y - Ey)) / (y - Ey) */
    float cx = TW / 2, cy = TH / 2;
    memset(s, 0, 16 * sizeof(float));
    s[0] = F; s[4] = cx; s[12] = -F * E[0] - cx * E[1];
    s[5] = cy; s[9] = -F; s[13] = F * E[2] - cy * E[1];
    s[7] = 1; s[15] = -E[1];
}


/* the cell the CPU expects (noise off): (scattered light, extinction) */
static void cpu_cell(const LU *u, const BrrVU *v, const LightSpot *lt, int x, int y, int z, float out[4])
{
    float px = ((float)x + 0.5f) / VOL_NX * TW, py = ((float)y + 0.5f) / VOL_NY * TH, d[3], len, t0, t1, t, p[3], st, L[3], c;
    int k;
    d[0] = (px - TW / 2) / F; d[1] = 1.0f; d[2] = -(py - TH / 2) / F;
    len = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    for (k = 0; k < 3; k++) d[k] /= len;
    t0 = z ? vol_dist((float)z) : 0.0f;
    t1 = vol_dist((float)z + 1.0f);
    t = t0 + (t1 - t0) * v->tgt[2];
    for (k = 0; k < 3; k++) p[k] = E[k] + d[k] * t;
    st = v->med[0] * vol_height(p[2], v->med[3], v->med[2]);
    c = u->sun[0] * d[0] + u->sun[1] * d[1] + u->sun[2] * d[2];
    for (k = 0; k < 3; k++) L[k] = u->sunc[k] * vol_hg(v->nse[3], c) + u->skyc[k] / 3.14159265f;
    if (lt) {   /* a point light: candela over the distance squared, in its range window */
        float q[3] = { lt->pos[0] - p[0], lt->pos[1] - p[1], lt->pos[2] - p[2] }, d2 = fmaxf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2], 0.01f), dl = sqrtf(d2);
        float wn = dl < lt->range ? 1.0f - (dl <= 0.7f * lt->range ? 0.0f : ({ float a = (dl - 0.7f * lt->range) / (0.3f * lt->range); a * a * (3 - 2 * a); })) : 0.0f;
        float cl = (q[0] * d[0] + q[1] * d[1] + q[2] * d[2]) / dl;
        for (k = 0; k < 3; k++) L[k] += lt->rgb[k] * lt->candela * wn / d2 * vol_hg(v->nse[3], cl);
    }
    for (k = 0; k < 3; k++) out[k] = fminf(st * v->med[1] * L[k] * u->misc[0], 60000.0f);
    out[3] = st;
}

static id<MTLTexture> tex3(id<MTLDevice> dev)
{
    MTLTextureDescriptor *td = [MTLTextureDescriptor new];
    td.textureType = MTLTextureType3D;
    td.pixelFormat = MTLPixelFormatRGBA32Float;
    td.width = VOL_NX; td.height = VOL_NY; td.depth = VOL_NZ;
    td.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
    td.storageMode = MTLStorageModeShared;
    return [dev newTextureWithDescriptor:td];
}
static void rd3(id<MTLTexture> t, float *px)
{
    [t getBytes:px bytesPerRow:VOL_NX * 16 bytesPerImage:VOL_NX * VOL_NY * 16
     fromRegion:MTLRegionMake3D(0, 0, 0, VOL_NX, VOL_NY, VOL_NZ) mipmapLevel:0 slice:0];
}
static void wr3(id<MTLTexture> t, const float *px)
{
    [t replaceRegion:MTLRegionMake3D(0, 0, 0, VOL_NX, VOL_NY, VOL_NZ) mipmapLevel:0 slice:0 withBytes:px
         bytesPerRow:VOL_NX * 16 bytesPerImage:VOL_NX * VOL_NY * 16];
}
#define IX(x, y, z) ((((size_t)(z) * VOL_NY + (y)) * VOL_NX + (x)) * 4)

static const char *k_at =
    "kernel void t_at(uint id [[thread_position_in_grid]], constant VU &v [[buffer(1)]], const device float4 *q [[buffer(2)]],\n"
    "                 device float4 *o [[buffer(3)]], texture3d<float> iv [[texture(0)]]) { o[id] = vol_at(v, iv, q[id].xy, q[id].z); }\n"
    /* an opaque surface (the full-screen pass's lookup, its world point read from a position target) and a
       blended one (fsf's, its colour c through the volume: c = 0 gives S, c = 1 gives T + S; additive: T alone)
       at the same pixel and distance; q: (pixel, distance, the CPU's texel) */
    "kernel void t_same(uint id [[thread_position_in_grid]], constant VU &v [[buffer(1)]], const device float4 *q [[buffer(2)]],\n"
    "                   device float4 *o [[buffer(3)]], texture3d<float> iv [[texture(0)]], texture2d<float> pos [[texture(1)]]) {\n"
    "  float3 eye = v.peye.xyz, wp = pos.read(uint2(id, 0)).xyz;\n"
    "  o[4 * id] = vol_pix(v, iv, q[id].xy, wp, eye);\n"
    "  o[4 * id + 1] = float4(vol_fwd(v, iv, q[id].xy, wp, eye, float3(0.0), false), 0.0);\n"
    "  o[4 * id + 2] = float4(vol_fwd(v, iv, q[id].xy, wp, eye, float3(1.0), false), 0.0);\n"
    "  o[4 * id + 3] = float4(vol_fwd(v, iv, q[id].xy, wp, eye, float3(1.0), true), 0.0); }\n";

static double run(id<MTLCommandQueue> q, id<MTLComputePipelineState> p, __strong id<MTLBuffer> b[4], __strong id<MTLTexture> t[3], MTLSize n, MTLSize g,
                  const LU *u, const BrrVU *v)
{
    id<MTLCommandBuffer> cb = [q commandBuffer];
    id<MTLComputeCommandEncoder> e = [cb computeCommandEncoder];
    int k;
    [e setComputePipelineState:p];
    if (u) [e setBytes:u length:sizeof *u atIndex:0];
    [e setBytes:v length:sizeof *v atIndex:1];
    for (k = 2; k < 4; k++)
        if (b[k]) [e setBuffer:b[k] offset:0 atIndex:(NSUInteger)k];
    for (k = 0; k < 3; k++)
        if (t[k]) [e setTexture:t[k] atIndex:(NSUInteger)k];
    [e dispatchThreads:n threadsPerThreadgroup:g];
    [e endEncoding];
    [cb commit];
    [cb waitUntilCompleted];
    return cb.GPUEndTime - cb.GPUStartTime;
}

static void gpu(const char *path)
{
    id<MTLDevice> dev = MTLCreateSystemDefaultDevice();
    NSError *err = nil;
    NSString *src = [NSString stringWithContentsOfFile:[NSString stringWithUTF8String:path] encoding:NSUTF8StringEncoding error:&err];
    id<MTLLibrary> lib;
    id<MTLComputePipelineState> pinj, pint, pat, psame;
    id<MTLCommandQueue> q;
    id<MTLTexture> shadow, cell, prev, integ;
    id<MTLBuffer> lsb, b[4] = { nil, nil, nil, nil };
    LU u;
    BrrVU v;
    LightSet *ls;
    float scr[16], *got, *want, *ref;
    size_t n4 = (size_t)VOL_NX * VOL_NY * VOL_NZ * 4;
    double tinj = 1e9, tint = 1e9;
    int x, y, z, k, rep;
    if (!dev || !src) {
        CHECK(0, "no device or no shader source %s", path);
        return;
    }
    lib = [dev newLibraryWithSource:[src stringByAppendingString:[NSString stringWithUTF8String:k_at]] options:nil error:&err];
    if (!lib) {
        CHECK(0, "the shader source: %s", [[err description] UTF8String]);
        return;
    }
    pinj = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"vol_inject"] error:&err];
    pint = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"vol_integrate"] error:&err];
    pat = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"t_at"] error:&err];
    psame = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"t_same"] error:&err];
    q = [dev newCommandQueue];
    {
        MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float width:16 height:16 mipmapped:NO];
        td.textureType = MTLTextureType2DArray;
        td.arrayLength = 4;
        td.usage = MTLTextureUsageShaderRead;
        td.storageMode = MTLStorageModePrivate;
        shadow = [dev newTextureWithDescriptor:td];
    }
    cell = tex3(dev); prev = tex3(dev); integ = tex3(dev);
    got = malloc(n4 * sizeof(float)); want = malloc(n4 * sizeof(float)); ref = malloc(n4 * sizeof(float));
    lsb = [dev newBufferWithLength:sizeof(LightSet) options:MTLResourceStorageModeShared];
    ls = (LightSet *)[lsb contents];
    memset(ls, 0, sizeof *ls);
    {   /* one point light in every cluster: 2,000 cd, warm, 40 m range, 20 m ahead and a little up */
        LightSpot *l = &ls->l[0];
        l->pos[0] = E[0] + 2; l->pos[1] = E[1] + 20; l->pos[2] = E[2] + 1;
        l->kind = LIGHT_POINT; l->candela = 2000; l->range = 40;
        l->rgb[0] = 1; l->rgb[1] = 0.8f; l->rgb[2] = 0.6f;
        for (k = 0; k < LC_N; k++) ls->cl[k] = 1;
        ls->idx[0] = 0;
    }
    memset(&u, 0, sizeof u);
    memcpy(u.eye, E, sizeof E);
    u.sun[0] = 0.3f; u.sun[1] = 0.8f; u.sun[2] = 0.52f;
    { float l = sqrtf(u.sun[0] * u.sun[0] + u.sun[1] * u.sun[1] + u.sun[2] * u.sun[2]); for (k = 0; k < 3; k++) u.sun[k] /= l; }
    for (k = 0; k < 3; k++) { u.sunc[k] = 12000.0f * (1.0f - 0.05f * k); u.skyc[k] = 16000.0f * (0.85f + 0.03f * k); }
    u.misc[0] = 1.0f / 3000.0f;           /* an exposure: pre-exposed values near 1 */
    u.misc[1] = 0.0f;                     /* no cascades: the sun unshadowed */
    memset(&v, 0, sizeof v);
    screen_map(scr);
    vol_inverse(scr, &v);
    v.grid[0] = VOL_NX; v.grid[1] = VOL_NY; v.grid[2] = VOL_NZ; v.grid[3] = VOL_FAR;
    v.tgt[0] = TW; v.tgt[1] = TH; v.tgt[2] = 0.37f; v.tgt[3] = VOL_NEAR;
    v.med[0] = k_vol[1].sigma; v.med[1] = k_vol[1].albedo; v.med[2] = k_vol[1].height; v.med[3] = E[2] - 2.0f;
    v.nse[3] = VOL_G;
    b[2] = lsb;

    /* the cells */
    for (rep = 0; rep < 5; rep++) {
        id<MTLTexture> t[3] = { shadow, prev, cell };
        tinj = fmin(tinj, run(q, pinj, b, t, MTLSizeMake(VOL_NX, VOL_NY, VOL_NZ), MTLSizeMake(8, 8, 4), &u, &v));
    }
    rd3(cell, got);
    {
        double md = 0, mr = 0;
        for (z = 0; z < VOL_NZ; z++)
            for (y = 0; y < VOL_NY; y++)
                for (x = 0; x < VOL_NX; x++) {
                    float c[4];
                    cpu_cell(&u, &v, &ls->l[0], x, y, z, c);
                    memcpy(&want[IX(x, y, z)], c, sizeof c);
                    for (k = 0; k < 4; k++) {
                        double d = fabs(got[IX(x, y, z) + k] - c[k]);
                        md = fmax(md, d);
                        mr = fmax(mr, d / fmax(fabs(c[k]), 1e-6));
                    }
                }
        printf("gpu: vol_inject against the CPU over %d cells (sun, sky, a lamp, height fog): max abs diff %.2e, max rel diff %.2e\n",
               VOL_NX * VOL_NY * VOL_NZ, md, mr);
        CHECK(mr < 2e-3, "inject rel %g", mr);
    }
    {   /* the noise on: extinction within its amplitude of the noiseless */
        float lo = 1e9f, hi = -1e9f;
        id<MTLTexture> t[3] = { shadow, prev, integ };
        v.nse[0] = 0.25f; v.nse[1] = 1.0f / 24.0f; v.nse[2] = 12.5f;
        run(q, pinj, b, t, MTLSizeMake(VOL_NX, VOL_NY, VOL_NZ), MTLSizeMake(8, 8, 4), &u, &v);
        rd3(integ, ref);
        for (size_t i = 3; i < n4; i += 4)
            if (want[i] > 1e-9f) {
                lo = fminf(lo, ref[i] / want[i]);
                hi = fmaxf(hi, ref[i] / want[i]);
            }
        printf("gpu: noise 0.25: extinction over the noiseless %.3f..%.3f\n", lo, hi);
        CHECK(lo >= 0.75f - 1e-3f && hi <= 1.25f + 1e-3f && hi - lo > 0.2f, "noise %g..%g", lo, hi);
        v.nse[0] = 0;
    }
    {   /* the reprojection: the eye still, last frame's cells a ramp of their own coordinates, history weight 1 */
        double md = 0;
        id<MTLTexture> t[3] = { shadow, prev, integ };
        for (z = 0; z < VOL_NZ; z++)
            for (y = 0; y < VOL_NY; y++)
                for (x = 0; x < VOL_NX; x++) {
                    float *p = &ref[IX(x, y, z)];
                    p[0] = ((float)x + 0.5f) / VOL_NX; p[1] = ((float)y + 0.5f) / VOL_NY; p[2] = ((float)z + 0.5f) / VOL_NZ; p[3] = 0;
                }
        wr3(prev, ref);
        memcpy(v.pscr, scr, sizeof scr);
        memcpy(v.peye, E, sizeof E);
        v.peye[3] = 1.0f;
        v.hist[0] = 1.0f;
        run(q, pinj, b, t, MTLSizeMake(VOL_NX, VOL_NY, VOL_NZ), MTLSizeMake(8, 8, 4), &u, &v);
        rd3(integ, got);
        for (z = 0; z < VOL_NZ; z++)
            for (y = 0; y < VOL_NY; y++)
                for (x = 0; x < VOL_NX; x++) {
                    float t0 = z ? vol_dist((float)z) : 0.0f, t1 = vol_dist((float)z + 1.0f), tt = t0 + (t1 - t0) * v.tgt[2];
                    float ez = fminf(fmaxf(vol_slice(tt) / VOL_NZ, 0.5f / VOL_NZ), 1.0f - 0.5f / VOL_NZ);
                    float e3[3] = { ((float)x + 0.5f) / VOL_NX, ((float)y + 0.5f) / VOL_NY, ez };
                    if (vol_slice(tt) / VOL_NZ < 0.0f)
                        continue;
                    for (k = 0; k < 3; k++)
                        md = fmax(md, fabs(got[IX(x, y, z) + k] - e3[k]));
                }
        printf("gpu: reprojection, a still eye: last frame's cell found at its own coordinates, max diff %.2e (of 1)\n", md);
        CHECK(md < 2e-3, "reprojection %g", md);
        memset(v.pscr, 0, sizeof v.pscr);
        v.peye[3] = 0;
        v.hist[0] = 0;
    }
    /* the integration, on the GPU's cells */
    for (rep = 0; rep < 5; rep++) {
        id<MTLTexture> t[3] = { cell, integ, nil };
        tint = fmin(tint, run(q, pint, b, t, MTLSizeMake(VOL_NX, VOL_NY, 1), MTLSizeMake(16, 8, 1), NULL, &v));
    }
    rd3(cell, want);
    rd3(integ, got);
    {
        double md = 0, mr = 0;
        for (y = 0; y < VOL_NY; y++)
            for (x = 0; x < VOL_NX; x++) {
                float S[3] = { 0, 0, 0 }, T[3] = { 1, 1, 1 };
                for (z = 0; z < VOL_NZ; z++) {
                    const float *c = &want[IX(x, y, z)];
                    for (k = 0; k < 3; k++) {
                        float Tk = T[k];
                        vol_step(fmaxf(c[3], 1e-7f), c[k], vol_t1(z) - vol_t0(z), &S[k], &Tk);
                        T[k] = Tk;
                    }
                    for (k = 0; k < 4; k++) {
                        float w = k < 3 ? S[k] : T[0], d = fabsf(got[IX(x, y, z) + k] - w);
                        md = fmax(md, d);
                        mr = fmax(mr, d / fmax(fabs(w), 1e-6));
                    }
                }
            }
        printf("gpu: vol_integrate against vol_step over %d columns: max abs diff %.2e, max rel diff %.2e\n", VOL_NX * VOL_NY, md, mr);
        CHECK(mr < 2e-3, "integrate rel %g", mr);
    }
    {   /* vol_at at slices' far sides, texel centres: the slice's own value */
        id<MTLBuffer> qb = [dev newBufferWithLength:256 * 16 options:MTLResourceStorageModeShared];
        id<MTLBuffer> ob = [dev newBufferWithLength:256 * 16 options:MTLResourceStorageModeShared];
        float *qq = [qb contents], *oo, md = 0;
        id<MTLTexture> t[3] = { integ, nil, nil };
        id<MTLBuffer> bb[4] = { nil, nil, qb, ob };
        for (k = 0; k < 256; k++) {
            int xx = (k * 37) % VOL_NX, yy = (k * 11) % VOL_NY, zz = 1 + k % (VOL_NZ - 1);
            qq[4 * k] = ((float)xx + 0.5f) / VOL_NX * TW; qq[4 * k + 1] = ((float)yy + 0.5f) / VOL_NY * TH;
            qq[4 * k + 2] = vol_t1(zz);
            qq[4 * k + 3] = (float)(IX(xx, yy, zz) / 4);
        }
        run(q, pat, bb, t, MTLSizeMake(256, 1, 1), MTLSizeMake(64, 1, 1), NULL, &v);
        oo = [ob contents];
        for (k = 0; k < 256; k++) {
            const float *w = &got[(size_t)qq[4 * k + 3] * 4];
            int j;
            for (j = 0; j < 4; j++)
                md = fmaxf(md, fabsf(oo[4 * k + j] - w[j]) / fmaxf(fabsf(w[j]), 1e-6f));
        }
        printf("gpu: vol_at at slices' far sides: max rel diff to the slice %.2e\n", md);
        CHECK(md < 5e-3, "vol_at %g", md);
    }
    {   /* every 3D surface the same fog: an opaque and a blended surface at the same pixel and distance take the
           same transmittance and in-scatter, at slices' far sides the CPU's integration (the slice's value);
           at any distance the same as each other; an additive one only the transmittance */
        enum { NQ = 512 };
        id<MTLBuffer> qb = [dev newBufferWithLength:NQ * 16 options:MTLResourceStorageModeShared];
        id<MTLBuffer> ob = [dev newBufferWithLength:NQ * 4 * 16 options:MTLResourceStorageModeShared];
        MTLTextureDescriptor *pd = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float width:NQ height:1 mipmapped:NO];
        id<MTLTexture> pos;
        float *qq = [qb contents], *oo, wpos[NQ * 4];
        double dop = 0, dtr = 0, dsame = 0, dadd = 0;
        id<MTLBuffer> bb[4] = { nil, nil, qb, ob };
        pd.usage = MTLTextureUsageShaderRead;
        pos = [dev newTextureWithDescriptor:pd];
        memcpy(v.peye, E, sizeof E);
        for (k = 0; k < NQ; k++) {
            int xx = (k * 37) % VOL_NX, yy = (k * 11) % VOL_NY, zz = 1 + k % (VOL_NZ - 1);
            float t = k < NQ / 2 ? vol_t1(zz) : 1.0f + (float)((k * 7919) % 2970) / 10.0f, a = (float)k * 0.731f, el = 0.3f * sinf((float)k);
            qq[4 * k] = ((float)xx + 0.5f) / VOL_NX * TW; qq[4 * k + 1] = ((float)yy + 0.5f) / VOL_NY * TH;
            qq[4 * k + 2] = t;
            qq[4 * k + 3] = (float)(IX(xx, yy, zz) / 4);
            wpos[4 * k] = E[0] + t * cosf(el) * cosf(a);
            wpos[4 * k + 1] = E[1] + t * cosf(el) * sinf(a);
            wpos[4 * k + 2] = E[2] + t * sinf(el);
            wpos[4 * k + 3] = 0;
        }
        [pos replaceRegion:MTLRegionMake2D(0, 0, NQ, 1) mipmapLevel:0 withBytes:wpos bytesPerRow:NQ * 16];
        {
            id<MTLTexture> t[3] = { integ, pos, nil };
            run(q, psame, bb, t, MTLSizeMake(NQ, 1, 1), MTLSizeMake(64, 1, 1), NULL, &v);
        }
        oo = [ob contents];
        for (k = 0; k < NQ; k++) {
            const float *op = &oo[16 * k], *f0 = &oo[16 * k + 4], *f1 = &oo[16 * k + 8], *fa = &oo[16 * k + 12];
            const float *w = &got[(size_t)qq[4 * k + 3] * 4];
            int j;
            for (j = 0; j < 3; j++) {
                float tr = f1[j] - f0[j];   /* the blended surface's transmittance and in-scatter */
                dsame = fmax(dsame, fmax(fabs(f0[j] - op[j]) / fmax(fabs(op[j]), 1.0), fabs(tr - op[3])));
                dadd = fmax(dadd, fabs(fa[j] - op[3]));
                if (k < NQ / 2) {
                    dop = fmax(dop, fmax(fabs(op[j] - w[j]) / fmax(fabs(w[j]), 1.0), fabs(op[3] - w[3])));
                    dtr = fmax(dtr, fmax(fabs(f0[j] - w[j]) / fmax(fabs(w[j]), 1.0), fabs(tr - w[3])));
                }
            }
        }
        printf("gpu: an opaque and a blended surface at the same pixel and distance: opaque against the CPU %.2e, blended against the CPU %.2e, "
               "blended against opaque %.2e (%d surfaces), additive's transmittance %.2e\n", dop, dtr, dsame, NQ, dadd);
        CHECK(dop < 1e-3 && dtr < 1e-3 && dsame < 1e-3 && dadd < 1e-3, "opaque/blended %g %g %g %g", dop, dtr, dsame, dadd);
        memset(v.peye, 0, sizeof v.peye);
    }
    {   /* the homogeneous closed form through the GPU: sun and lamp off, the sky an ambient of L = skyc / pi, no height falloff */
        LightSet *l2 = ls;
        double mt = 0, ms = 0;
        id<MTLTexture> t[3] = { shadow, prev, cell };
        id<MTLTexture> t2[3] = { cell, integ, nil };
        float L = 2000.0f, sg = k_vol[2].sigma, alb = k_vol[2].albedo;
        memset(l2->cl, 0, sizeof l2->cl);
        memset(u.sunc, 0, sizeof u.sunc);
        for (k = 0; k < 3; k++) u.skyc[k] = L * 3.14159265f;
        u.misc[0] = 1.0f / 1000.0f;
        v.med[0] = sg; v.med[1] = alb; v.med[2] = 1e9f;
        run(q, pinj, b, t, MTLSizeMake(VOL_NX, VOL_NY, VOL_NZ), MTLSizeMake(8, 8, 4), &u, &v);
        run(q, pint, b, t2, MTLSizeMake(VOL_NX, VOL_NY, 1), MTLSizeMake(16, 8, 1), NULL, &v);
        rd3(integ, got);
        for (z = 0; z < VOL_NZ; z++) {
            float et = expf(-sg * vol_t1(z)), es = L * u.misc[0] * alb * (1.0f - et);
            for (y = 0; y < VOL_NY; y += 7)
                for (x = 0; x < VOL_NX; x += 9) {
                    mt = fmax(mt, fabs(got[IX(x, y, z) + 3] - et) / et);
                    ms = fmax(ms, fabs(got[IX(x, y, z)] - es) / es);
                }
        }
        printf("gpu: homogeneous (rain, 1 km): transmittance max rel error %.2e, ambient in-scatter max rel error %.2e\n", mt, ms);
        CHECK(mt < 0.005 && ms < 0.005, "gpu homogeneous %g %g", mt, ms);
    }
    printf("gpu: %s: vol_inject %.3f ms, vol_integrate %.3f ms at %dx%dx%d (one lamp in every cluster, the sun unshadowed)\n",
           [[dev name] UTF8String], tinj * 1e3, tint * 1e3, VOL_NX, VOL_NY, VOL_NZ);
    free(got); free(want); free(ref);
}

/* the lighthouse: a lamp 30 m up, its axis turned phi degrees, 0.02 under the horizon (light_world.c) */
#define BEAM_CD 2000000.0f
static void beam_axis(float phi, float a[3])
{
    float r = phi * 0.017453293f, l;
    a[0] = cosf(r); a[1] = sinf(r); a[2] = -0.02f;
    l = sqrtf(vol_dot3(a, a));
    a[0] /= l; a[1] /= l; a[2] /= l;
}
/* a ray crossing the axis s metres out (offset off metres sideways), at
 * alpha degrees to it, from an eye back metres before the crossing */
static void beam_ray(const float A[3], const float a[3], float s, float off, float alpha, float back, float o[3], float d[3])
{
    float h[3] = { -a[1], a[0], 0.0f }, l = sqrtf(h[0] * h[0] + h[1] * h[1]), ca = cosf(alpha * 0.017453293f), sa = sinf(alpha * 0.017453293f);
    int k;
    h[0] /= l; h[1] /= l;
    for (k = 0; k < 3; k++) {
        float x = A[k] + a[k] * s + h[k] * off;
        d[k] = a[k] * ca + h[k] * sa;
        o[k] = x - d[k] * back;
    }
}
/* the reference: the integrand summed at n midpoints over the whole ray, no cone, no change of variable */
static double beam_ref(const float o[3], const float d[3], const float A[3], const float a[3], float st, float tmax, int n)
{
    double s = 0, dt = tmax / (double)n;
    int i;
    for (i = 0; i < n; i++)
        s += vol_beam_f(o, d, A, a, 0.99904822f, 0.99965732f, st, VOL_G, (float)((i + 0.5) * dt));
    return s * dt;
}
static const float k_bA[3] = { 0.0f, 0.0f, 30.0f };
static void beam(void)
{
    static const float cs[8][4] = {   /* s, sideways offset, alpha, eye back */
        { 30, 0.0f, 90, 100 }, { 100, 1.0f, 60, 150 }, { 200, -3.0f, 120, 80 }, { 300, 0.0f, 30, 200 },
        { 600, 8.0f, 150, 300 }, { 1000, 0.0f, 45, 100 }, { 1500, -20.0f, 75, 400 }, { 50, 0.5f, 20, 60 } };
    const float st = k_vol[4].sigma, cout = 0.99904822f, cin = 0.99965732f;
    float a[3], o[3], d[3], worst = 0, worst_turn = 0;
    int c, k;
    beam_axis(17.0f, a);
    for (c = 0; c < 8; c++) {
        double ref;
        beam_ray(k_bA, a, cs[c][0], cs[c][1], cs[c][2], cs[c][3], o, d);
        ref = beam_ref(o, d, k_bA, a, st, VOL_BEAM_SKY, 4000000);
        CHECK(ref > 0, "beam ray %d: the reference is zero", c);
        for (k = 0; k < 8; k++) {
            float m = vol_beam(o, d, k_bA, a, cout, cin, st, VOL_G, VOL_BEAM_SKY, (k + 0.5f) / 8.0f, VOL_BEAM_STEPS);
            float e = (float)(fabs(m - ref) / ref);
            worst = fmaxf(worst, e);
            CHECK(e < 0.02f, "beam ray %d jitter %d: march %g, quadrature %g (%.2f%%)", c, k, m, ref, e * 100);
        }
    }
    printf("beam: march (%d steps) against a %d-point quadrature, 8 rays x 8 jitters: max rel error %.3f%%\n",
           VOL_BEAM_STEPS, 4000000, worst * 100);
    {   /* rays that miss: beside the cone, behind the lamp, ending before it, from inside going out the back */
        float m[4];
        beam_ray(k_bA, a, 300, 0.0f, 90, 100, o, d);            /* 30 m over the axis at 300 m: outside 2.5 degrees (13 m) */
        o[2] += 30.0f;
        m[0] = vol_beam(o, d, k_bA, a, cout, cin, st, VOL_G, VOL_BEAM_SKY, 0.5f, VOL_BEAM_STEPS);
        beam_ray(k_bA, a, -200, 0.0f, 90, 100, o, d);           /* behind the lamp: the other nappe */
        m[1] = vol_beam(o, d, k_bA, a, cout, cin, st, VOL_G, VOL_BEAM_SKY, 0.5f, VOL_BEAM_STEPS);
        beam_ray(k_bA, a, 300, 0.0f, 90, 100, o, d);            /* a wall 50 m short of the beam */
        m[2] = vol_beam(o, d, k_bA, a, cout, cin, st, VOL_G, 50.0f, 0.5f, VOL_BEAM_STEPS);
        beam_ray(k_bA, a, 300, 0.0f, 90, 0.0f, o, d);           /* 50 m over the axis, going up and out */
        o[2] += 50.0f; d[0] = 0.3f * a[0]; d[1] = 0.3f * a[1]; d[2] = 0.954f;
        m[3] = vol_beam(o, d, k_bA, a, cout, cin, st, VOL_G, VOL_BEAM_SKY, 0.5f, VOL_BEAM_STEPS);
        for (k = 0; k < 4; k++)
            CHECK(m[k] == 0.0f, "beam miss %d: %g, not zero", k, m[k]);
        printf("beam: rays missing the cone (beside, behind the lamp, short of it, going away): %g %g %g %g\n", m[0], m[1], m[2], m[3]);
    }
    /* the beam turning past fixed rays (side-on and oblique, 100 to 400 m), 1 degree a frame over 16
       degrees, the ray crossing the whole cone throughout: the change a step under 5%, and the march's
       change the quadrature's (no banding: within 0.5% of it) */
    {
        static const float rs[3][3] = { { 100, 90, 100 }, { 300, 60, 150 }, { 400, 90, 200 } };
        float worst_track = 0;
        int r;
        for (r = 0; r < 3; r++) {
            float a0[3], prev = -1;
            double pref = -1;
            int deg;
            beam_axis(17.0f, a0);
            beam_ray(k_bA, a0, rs[r][0], 0.0f, rs[r][1], rs[r][2], o, d);
            for (deg = -8; deg <= 8; deg++) {
                float m;
                double ref;
                beam_axis(17.0f + deg, a);
                m = vol_beam(o, d, k_bA, a, cout, cin, st, VOL_G, VOL_BEAM_SKY, 0.5f, VOL_BEAM_STEPS);
                ref = beam_ref(o, d, k_bA, a, st, VOL_BEAM_SKY, 1000000);
                if (prev > 0 && m > 0) {
                    float e = fabsf(m - prev) / prev, tr = (float)fabs((m / prev) - (ref / pref));
                    worst_turn = fmaxf(worst_turn, e);
                    worst_track = fmaxf(worst_track, tr);
                    CHECK(e < 0.05f, "beam turn ray %d at %d degrees: %g after %g (%.1f%%)", r, deg, m, prev, e * 100);
                    CHECK(tr < 0.005f, "beam turn ray %d at %d degrees: the march's change off the quadrature's by %.2f%%", r, deg, tr * 100);
                }
                CHECK(m > 0, "beam turn ray %d at %d degrees: the ray no longer crosses the beam", r, deg);
                prev = m;
                pref = ref;
            }
        }
        printf("beam: turning 1 degree a step over 16 degrees past 3 rays: max change %.2f%%, the march's change off the quadrature's by at most %.3f%%\n",
               worst_turn * 100, worst_track * 100);
    }
    /* the beam's luminance side-on, an eye 100 m from the crossing: sigma_s I (the march) */
    {
        static const float dist[3] = { 100, 300, 1000 };
        beam_axis(17.0f, a);
        for (k = 0; k < 3; k++) {
            float m;
            beam_ray(k_bA, a, dist[k], 0.0f, 90, 100, o, d);
            m = vol_beam(o, d, k_bA, a, cout, cin, st, VOL_G, VOL_BEAM_SKY, 0.5f, VOL_BEAM_STEPS) * st * k_vol[4].albedo * BEAM_CD;
            printf("beam: side-on %4.0f m from the lighthouse, eye 100 m off: %.4f cd/m2 (a moonlit road 0.01)\n", dist[k], m);
        }
    }
}

/* the shader's vol_beam against the C reference, at the same rays and jitter */
static const char *k_beam =
"kernel void t_beam(device const float4 *in [[buffer(0)]], device float *out [[buffer(1)]], uint i [[thread_position_in_grid]]) {\n"
"  float4 o = in[i * 4], d = in[i * 4 + 1], A = in[i * 4 + 2], a = in[i * 4 + 3];\n"
"  out[i] = vol_beam(o.xyz, d.xyz, A.xyz, a.xyz, a.w, o.w, A.w, d.w, 3000.0, 0.5); }\n";
static void beam_gpu(const char *path)
{
    id<MTLDevice> dev = MTLCreateSystemDefaultDevice();
    NSError *err = nil;
    NSString *src = [NSString stringWithContentsOfFile:[NSString stringWithUTF8String:path] encoding:NSUTF8StringEncoding error:&err];
    id<MTLLibrary> lib;
    id<MTLComputePipelineState> pl;
    float in[8][16], want[8], *got, a[3], worst = 0;
    int c;
    if (!dev || !src)
        return;
    lib = [dev newLibraryWithSource:[src stringByAppendingString:[NSString stringWithUTF8String:k_beam]] options:nil error:&err];
    pl = lib ? [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"t_beam"] error:&err] : nil;
    if (!pl) {
        CHECK(0, "the beam kernel: %s", [[err description] UTF8String]);
        return;
    }
    beam_axis(17.0f, a);
    for (c = 0; c < 8; c++) {
        float o[3], d[3];
        beam_ray(k_bA, a, 30.0f + 180.0f * c, (float)(c % 3) - 1.0f, 25.0f + 18.0f * c, 120.0f, o, d);
        memcpy(in[c], o, 12); in[c][3] = 0.99965732f;
        memcpy(in[c] + 4, d, 12); in[c][7] = VOL_G;
        memcpy(in[c] + 8, k_bA, 12); in[c][11] = k_vol[4].sigma;
        memcpy(in[c] + 12, a, 12); in[c][15] = 0.99904822f;
        want[c] = vol_beam(o, d, k_bA, a, 0.99904822f, 0.99965732f, k_vol[4].sigma, VOL_G, 3000.0f, 0.5f, VOL_BEAM_STEPS);
    }
    {
        id<MTLCommandQueue> q = [dev newCommandQueue];
        id<MTLBuffer> bi = [dev newBufferWithBytes:in length:sizeof in options:MTLResourceStorageModeShared];
        id<MTLBuffer> bo = [dev newBufferWithLength:sizeof want options:MTLResourceStorageModeShared];
        id<MTLCommandBuffer> cb = [q commandBuffer];
        id<MTLComputeCommandEncoder> e = [cb computeCommandEncoder];
        [e setComputePipelineState:pl];
        [e setBuffer:bi offset:0 atIndex:0];
        [e setBuffer:bo offset:0 atIndex:1];
        [e dispatchThreads:MTLSizeMake(8, 1, 1) threadsPerThreadgroup:MTLSizeMake(8, 1, 1)];
        [e endEncoding];
        [cb commit];
        [cb waitUntilCompleted];
        got = (float *)[bo contents];
        for (c = 0; c < 8; c++) {
            float e2 = fabsf(got[c] - want[c]) / fmaxf(want[c], 1e-20f);
            CHECK(want[c] > 0 && e2 < 2e-3f, "beam gpu ray %d: shader %g, C %g", c, got[c], want[c]);
            worst = fmaxf(worst, e2);
        }
    }
    printf("beam: the shader's vol_beam against the C one, 8 rays: max rel difference %.2e\n", worst);
}

int main(int argc, char **argv)
{
    @autoreleasepool {
        homogeneous();
        steps();
        phase();
        beam();
        if (argc > 1) {
            gpu(argv[1]);
            beam_gpu(argv[1]);
        }
        printf("memory: cells 2 x %.1f MB + the integrated volume %.1f MB (RGBA16F, %dx%dx%d)\n",
               VOL_NX * VOL_NY * VOL_NZ * 8 / 1048576.0, VOL_NX * VOL_NY * VOL_NZ * 8 / 1048576.0, VOL_NX, VOL_NY, VOL_NZ);
    }
    printf(g_fail ? "vol_test: %d FAILED\n" : "vol_test: all passed\n", g_fail);
    return g_fail != 0;
}
