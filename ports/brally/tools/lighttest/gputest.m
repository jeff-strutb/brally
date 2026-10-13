/* gputest.m: the sun's shadow functions as the game compiles them
 * (brr_metal_lit.h: BRR_LU_SRC, BRR_SHADOW_SRC), run on this Mac's GPU in
 * compute kernels over the synthetic scenes of scenes.h, against their CPU
 * twins (shadowcpu.h); and the static cache's restore pass (scopy) drawn
 * into a depth array and read back bit for bit.  No game code, no game
 * data.  ports/brally/tools/lighttest/run.sh builds and runs it. */
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <stdio.h>
#include <math.h>
#include "brr_metal_lit.h"
#include "scenes.h"

static int s_fail;
#define CHECK(c, ...) do { if (!(c)) { s_fail++; printf("  FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static id<MTLDevice> s_dev;
static id<MTLCommandQueue> s_q;
static id<MTLLibrary> s_lib;

static NSString *const k_test = @
"struct TV { float4 pos [[position]]; };\n"
"vertex TV tvs(uint i [[vertex_id]]) { float2 p = float2((i << 1) & 2, i & 2); TV o; o.pos = float4(p * 2.0 - 1.0, 0, 1); return o; }\n"
"kernel void ksize(device uint *o [[buffer(0)]]) { o[0] = sizeof(LU); }\n"
"kernel void kvis(constant LU &u [[buffer(0)]], const device float4 *rx [[buffer(1)]], device float *out [[buffer(2)]],\n"
"                 depth2d_array<float> sm [[texture(0)]], uint i [[thread_position_in_grid]]) {\n"
"  float4 r = rx[i]; out[i] = casvis(u, sm, r.xyz, float3(0, 0, 1), 0, float2(cos(r.w), sin(r.w))); }\n"
"kernel void kcon(constant LU &u [[buffer(0)]], const device float4 *nrm [[buffer(1)]], device float *out [[buffer(2)]],\n"
"                 texture2d<float> pos [[texture(0)]], uint2 g [[thread_position_in_grid]]) {\n"
"  uint w = pos.get_width(); if (g.x >= w || g.y >= pos.get_height()) return;\n"
"  float4 P = pos.read(g);\n"
"  out[g.y * w + g.x] = P.w == 1.0 ? contact(u, pos, P.xyz, nrm[g.y * w + g.x].xyz, ign(float2(g) + 0.5)) : 1.0; }\n"
/* the cost benchmark: per pixel of a G-buffer, the sun's visibility as the
   lighting pass computes it, the contact shadow, the previous commit's 3x3
   filter (copied here as the baseline), and the reads alone */
"static float casvis_old(constant LU &u, depth2d_array<float> sm, float3 p, float3 n, int k) {\n"
"  constexpr sampler cs(filter::linear, address::clamp_to_edge, compare_func::less_equal);\n"
"  float nl = clamp(dot(n, u.sun.xyz), 0.05, 1.0), tn = min(sqrt(1.0 - nl * nl) / nl, 8.0);\n"
"  float4 c = u.sunvp[k] * float4(p + n * u.ctexel[k] * (1.0 + 2.0 * (1.0 - nl)), 1.0); float2 st = float2(c.x * 0.5 + 0.5, 0.5 - c.y * 0.5);\n"
"  if (any(st <= 0.0) || any(st >= 1.0) || c.z >= 1.0) return 1.0;\n"
"  float2 tx = 1.0 / float2(sm.get_width(), sm.get_height()); float vis = 0.0;\n"
"  for (int y = -1; y <= 1; y++) for (int x = -1; x <= 1; x++) vis += sm.sample_compare(cs, st + float2(x, y) * tx, uint(k), c.z - u.cbias[k] * (1.0 + tn));\n"
"  return vis / 9.0;\n"
"}\n"
"static float sunvis_old(constant LU &u, depth2d_array<float> sm, float3 p, float3 n) {\n"
"  float d = dot(p - u.eye.xyz, u.cfwd.xyz); int k = 0;\n"
"  while (k < 3 && d > u.csplit[k]) k++;\n"
"  if (d > u.csplit[3]) return 1.0;\n"
"  float d0 = k ? u.csplit[k - 1] : 0.0, w = saturate((d - mix(d0, u.csplit[k], 0.9)) / max(0.1 * (u.csplit[k] - d0), 1e-3));\n"
"  float vis = casvis_old(u, sm, p, n, k);\n"
"  if (w > 0.0) vis = mix(vis, k < 3 ? casvis_old(u, sm, p, n, k + 1) : 1.0, w);\n"
"  return vis;\n"
"}\n"
"kernel void kbench(constant LU &u [[buffer(0)]], device float *out [[buffer(1)]], constant uint &mode [[buffer(2)]],\n"
"                   texture2d<float> pos [[texture(0)]], depth2d_array<float> sm [[texture(1)]], uint2 g [[thread_position_in_grid]]) {\n"
"  uint w = pos.get_width(); if (g.x >= w || g.y >= pos.get_height()) return;\n"
"  float4 P = pos.read(g); float j = ign(float2(g) + 0.5), r = 1.0; float3 n = float3(0, 0, 1);\n"
"  if (P.w == 1.0) {\n"
"    if (mode == 1) r = sunvis(u, sm, P.xyz, n, j * 6.2831853);\n"
"    else if (mode == 2) r = contact(u, pos, P.xyz, n, j);\n"
"    else if (mode == 3) r = sunvis_old(u, sm, P.xyz, n);\n"
"    else if (mode == 4) { r = dot(n, u.sun.xyz) > 0.0 ? sunvis(u, sm, P.xyz, n, j * 6.2831853) : 0.0; if (r > 0.0) r = min(r, contact(u, pos, P.xyz, n, j)); }\n"
"    else r = P.x;\n"
"  }\n"
"  out[g.y * w + g.x] = r; }\n";

static id<MTLComputePipelineState> kernel_named(NSString *n)
{
    NSError *err = nil;
    id<MTLComputePipelineState> p = [s_dev newComputePipelineStateWithFunction:[s_lib newFunctionWithName:n] error:&err];
    if (!p) {
        fprintf(stderr, "gputest: %s: %s\n", [n UTF8String], [[err description] UTF8String]);
        exit(2);
    }
    return p;
}

/* a private Depth32Float array, its layers filled from data (size x size each) */
static id<MTLTexture> depth_array(const float *data, int size, int layers)
{
    MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                                                                 width:(NSUInteger)size height:(NSUInteger)size mipmapped:NO];
    id<MTLTexture> t;
    id<MTLBuffer> b = [s_dev newBufferWithBytes:data length:(size_t)size * size * layers * 4 options:MTLResourceStorageModeShared];
    id<MTLCommandBuffer> cb = [s_q commandBuffer];
    id<MTLBlitCommandEncoder> be = [cb blitCommandEncoder];
    int l;
    td.textureType = MTLTextureType2DArray;
    td.arrayLength = (NSUInteger)layers;
    td.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    td.storageMode = MTLStorageModePrivate;
    t = [s_dev newTextureWithDescriptor:td];
    for (l = 0; l < layers; l++)
        [be copyFromBuffer:b sourceOffset:(NSUInteger)l * size * size * 4 sourceBytesPerRow:(NSUInteger)size * 4
              sourceBytesPerImage:(NSUInteger)size * size * 4 sourceSize:MTLSizeMake((NSUInteger)size, (NSUInteger)size, 1)
                        toTexture:t destinationSlice:(NSUInteger)l destinationLevel:0 destinationOrigin:MTLOriginMake(0, 0, 0)];
    [be endEncoding];
    [cb commit];
    [cb waitUntilCompleted];
    return t;
}

static void depth_read(id<MTLTexture> t, int size, int layers, float *out)
{
    id<MTLBuffer> b = [s_dev newBufferWithLength:(size_t)size * size * layers * 4 options:MTLResourceStorageModeShared];
    id<MTLCommandBuffer> cb = [s_q commandBuffer];
    id<MTLBlitCommandEncoder> be = [cb blitCommandEncoder];
    int l;
    for (l = 0; l < layers; l++)
        [be copyFromTexture:t sourceSlice:(NSUInteger)l sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0)
                 sourceSize:MTLSizeMake((NSUInteger)size, (NSUInteger)size, 1) toBuffer:b
          destinationOffset:(NSUInteger)l * size * size * 4 destinationBytesPerRow:(NSUInteger)size * 4
        destinationBytesPerImage:(NSUInteger)size * size * 4];
    [be endEncoding];
    [cb commit];
    [cb waitUntilCompleted];
    memcpy(out, [b contents], (size_t)size * size * layers * 4);
}

#define PEN_ROT 64
#define PEN_X0 (-0.6f)
#define PEN_DX 0.0005f
#define PEN_NX 2401

static void gpu_penumbra(void)
{
    static const float Ds[] = { 0.05f, 2.0f, 5.0f, 10.0f, 20.0f, 40.0f, 60.0f };
    id<MTLComputePipelineState> k = kernel_named(@"kvis");
    float *map = malloc((size_t)PEN_SIZE * PEN_SIZE * 4);
    size_t n = (size_t)PEN_NX * PEN_ROT;
    id<MTLBuffer> rx = [s_dev newBufferWithLength:n * 16 options:MTLResourceStorageModeShared];
    id<MTLBuffer> out = [s_dev newBufferWithLength:n * 4 options:MTLResourceStorageModeShared];
    double tanh_ = tan(SUN_HALF_DEG * 3.14159265358979 / 180.0), worst = 0;
    DepthMap dm = { map, PEN_SIZE };
    LU u;
    int di;
    size_t i;
    {
        float *r = (float *)[rx contents];
        for (i = 0; i < n; i++) {
            r[4 * i + 0] = PEN_X0 + PEN_DX * (float)(i / PEN_ROT);
            r[4 * i + 1] = 0.0037f;
            r[4 * i + 2] = 0.0f;
            r[4 * i + 3] = 6.2831853f * ((float)(i % PEN_ROT) + 0.5f) / PEN_ROT;
        }
    }
    printf("gpu penumbra: casvis on the GPU against its CPU twin, %zu receivers a blocker height\n", n);
    /* max |d|: the largest difference apart from "tip", the receivers where
       one side found a blocker in its search and the other did not (the
       hardware rounds sample positions to 1/256 texel) and took PCSS's
       fully-lit exit: only ever at the penumbra's outer tip */
    printf("  %6s %10s %10s %10s %6s %9s %9s %6s %8s\n", "D m", "max |d|", "mean |d|", "differ", "tip", "gpu w mm", "2Dtan mm", "drops", "max drop");
    for (di = 0; di < (int)(sizeof Ds / sizeof Ds[0]); di++) {
        id<MTLTexture> sm;
        id<MTLCommandBuffer> cb;
        id<MTLComputeCommandEncoder> ce;
        const float *g, *r = (const float *)[rx contents];
        double mx = 0, mean = 0, xa = 1e9, xb = 1e9, maxdrop = 0;
        long differ = 0, tip = 0, drops = 0;
        int ix, m;
        pen_setup(Ds[di], &u, map);
        sm = depth_array(map, PEN_SIZE, 1);
        cb = [s_q commandBuffer];
        ce = [cb computeCommandEncoder];
        [ce setComputePipelineState:k];
        [ce setBytes:&u length:sizeof u atIndex:0];
        [ce setBuffer:rx offset:0 atIndex:1];
        [ce setBuffer:out offset:0 atIndex:2];
        [ce setTexture:sm atIndex:0];
        [ce dispatchThreads:MTLSizeMake(n, 1, 1) threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];
        [ce endEncoding];
        [cb commit];
        [cb waitUntilCompleted];
        g = (const float *)[out contents];
        for (i = 0; i < n; i++) {
            float n3[3] = { 0, 0, 1 }, c = cpu_casvis(&u, &dm, &r[4 * i], n3, 0, r[4 * i + 3]);
            double d = fabs((double)g[i] - c);
            if (d > 0.01 && getenv("GPUTEST_DEBUG"))
                printf("    receiver x %.4f phi %.4f: gpu %.6f cpu %.6f\n", r[4 * i], r[4 * i + 3], g[i], c);
            mean += d;
            if (d > 1e-6) differ++;
            if (d > 2e-3)
                tip++;               /* PCSS's no-blocker exit taken by one and not the other (graded below) */
            else if (d > mx)
                mx = d;
            if (d > 2e-3 && (g[i] < 1.0f && c < 1.0f))
                CHECK(0, "D %.2f x %.4f: GPU %.6f and CPU %.6f differ where both found blockers", Ds[di], r[4 * i], g[i], c);
            if (d > 2e-3 && (g[i] < 0.95f || c < 0.95f))
                CHECK(0, "D %.2f x %.4f: GPU %.6f and CPU %.6f differ away from the penumbra's outer tip", Ds[di], r[4 * i], g[i], c);
        }
        for (m = 0; m < PEN_ROT; m++)            /* each turn of the disc alone: never darker further from the blocker */
            for (ix = 1; ix < PEN_NX; ix++) {
                double dd = g[(ix - 1) * PEN_ROT + m] - g[ix * PEN_ROT + m];
                if (dd > 1e-6) {
                    drops++;
                    if (dd > maxdrop) maxdrop = dd;
                }
            }
        for (ix = 0; ix < PEN_NX; ix++) {    /* the GPU's own penumbra width, as shadowtest measures the CPU's */
            double e = 0, x = PEN_X0 + PEN_DX * ix;
            for (m = 0; m < PEN_ROT; m++)
                e += g[ix * PEN_ROT + m];
            e /= PEN_ROT;
            if (e > 0 && xa > 1e8) xa = x;
            if (e >= 1.0 && xb > 1e8) xb = x;
        }
        printf("  %6.2f %10.2e %10.2e %10ld %6ld %9.1f %9.1f %6ld %8.4f\n", Ds[di], mx, mean / n, differ, tip, (xb - xa - PEN_TEXEL) * 1000,
               2 * Ds[di] * tanh_ * 1000, drops, maxdrop);
        if (mx > worst) worst = mx;
        CHECK(mx < 2e-3, "D %.2f: the GPU's casvis differs from the CPU's by %.4f", Ds[di], mx);
        CHECK(tip <= (long)(n / 10000), "D %.2f: %ld receivers took the no-blocker exit on one side only", Ds[di], tip);
        CHECK(maxdrop < 0.05, "D %.2f: the GPU's shadow darkens by %.4f moving away from the blocker", Ds[di], maxdrop);
        CHECK(fabs(xb - xa - PEN_TEXEL - 2 * Ds[di] * tanh_) <= PEN_TEXEL, "D %.2f: the GPU's penumbra is off by more than a texel", Ds[di]);
    }
    free(map);
}

static void gpu_contact(void)
{
    id<MTLComputePipelineState> k = kernel_named(@"kcon");
    float *pos = malloc(CON_W * CON_H * 16), *nrm = malloc(CON_W * CON_H * 16);
    id<MTLBuffer> nb, out;
    id<MTLTexture> pt;
    MTLTextureDescriptor *td;
    id<MTLCommandBuffer> cb;
    id<MTLComputeCommandEncoder> ce;
    const float *g;
    LU u;
    Cam c;
    int x, y, n = 0, differ = 0;
    double mx = 0;
    con_setup(&u, &c, pos, nrm);
    td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float width:CON_W height:CON_H mipmapped:NO];
    td.usage = MTLTextureUsageShaderRead;
    td.storageMode = MTLStorageModeShared;
    pt = [s_dev newTextureWithDescriptor:td];
    [pt replaceRegion:MTLRegionMake2D(0, 0, CON_W, CON_H) mipmapLevel:0 withBytes:pos bytesPerRow:CON_W * 16];
    nb = [s_dev newBufferWithBytes:nrm length:CON_W * CON_H * 16 options:MTLResourceStorageModeShared];
    out = [s_dev newBufferWithLength:CON_W * CON_H * 4 options:MTLResourceStorageModeShared];
    cb = [s_q commandBuffer];
    ce = [cb computeCommandEncoder];
    [ce setComputePipelineState:k];
    [ce setBytes:&u length:sizeof u atIndex:0];
    [ce setBuffer:nb offset:0 atIndex:1];
    [ce setBuffer:out offset:0 atIndex:2];
    [ce setTexture:pt atIndex:0];
    [ce dispatchThreads:MTLSizeMake(CON_W, CON_H, 1) threadsPerThreadgroup:MTLSizeMake(16, 16, 1)];
    [ce endEncoding];
    [cb commit];
    [cb waitUntilCompleted];
    g = (const float *)[out contents];
    for (y = 0; y < CON_H; y++)
        for (x = 0; x < CON_W; x++) {
            const float *P = &pos[4 * (y * CON_W + x)];
            float cv;
            double d;
            if (P[3] != 1.0f)
                continue;
            cv = cpu_contact(&u, pos, CON_W, CON_H, P, &nrm[4 * (y * CON_W + x)], cpu_ign(x + 0.5f, y + 0.5f));
            d = fabs((double)g[y * CON_W + x] - cv);
            n++;
            if (d > 1e-5) differ++;
            if (d > mx) mx = d;
        }
    printf("gpu contact: %d ground pixels, the GPU and the CPU differ at %d (largest %.2e)\n", n, differ, mx);
    CHECK(differ <= n / 1000, "the GPU's contact shadow differs from the CPU's at %d pixels", differ);
    free(pos);
    free(nrm);
}

/* the cost: the shadow functions over a 1920 x 1080 G-buffer of ground seen
 * from 2 m up (the sun overhead, a 10 m high blocker over half the map, so
 * half the ground is shadowed and half has no blocker in reach), four
 * 2048^2 cascades split at the fit's distances (so the blend bands are
 * there), timed by the command buffers' GPU start and end over 20 runs.  A
 * compute dispatch, not the lighting's fragment pass: the same per-pixel
 * work without the rest of that pass. */
static void gpu_cost(void)
{
    enum { W = 1920, H = 1080, S = 2048 };
    static const char *const names[5] = { "reads only", "sun, cascades, PCSS", "contact shadow", "sun, previous 3x3", "sun and contact" };
    static const int order[5] = { 0, 3, 1, 2, 4 };
    id<MTLComputePipelineState> k = kernel_named(@"kbench");
    float *pos = calloc((size_t)W * H, 16), *map = malloc((size_t)S * S * 4 * 4);
    float eye[3] = { 0.0f, -20.0f, 2.0f }, tgt[3] = { 0.0f, 10.0f, 0.0f }, rw = S * 0.05f * 0.5f;
    double base = 0, ms[5][41];
    id<MTLTexture> pt, sm;
    id<MTLBuffer> out = [s_dev newBufferWithLength:(size_t)W * H * 4 options:MTLResourceStorageModeShared];
    MTLTextureDescriptor *td;
    Cam c;
    LU u;
    int x, y, l, mi, run;
    memset(&c, 0, sizeof c);
    memset(&u, 0, sizeof u);
    cam_make(&c, eye, tgt, 0.9f, (float)W / H);
    c.sx = W / 2; c.ox = W / 2; c.sy = H / 2; c.oy = H / 2; c.flip = H;
    c.xft[0] = 2.0f / W; c.xft[1] = -1; c.xft[2] = -2.0f / H; c.xft[3] = 1;
    c.tw = W; c.th = H;
    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++) {
            float d[3], *P = &pos[4 * ((size_t)y * W + x)];
            cam_ray(&c, (float)x, (float)y, d);
            if (d[2] < -1e-4f) {
                float t = -eye[2] / d[2];
                P[0] = eye[0] + d[0] * t;
                P[1] = eye[1] + d[1] * t;
                P[2] = 0.0f;
                P[3] = 1.0f;
            }
        }
    for (l = 0; l < 4; l++)
        for (y = 0; y < S; y++)
            for (x = 0; x < S; x++)
                map[(size_t)l * S * S + (size_t)y * S + x] = x < S / 2 ? 90.0f / 101.0f : 100.0f / 101.0f;
    for (l = 0; l < 4; l++) {
        u.sunvp[l][0] = 1.0f / rw;
        u.sunvp[l][5] = 1.0f / rw;
        u.sunvp[l][10] = -1.0f / 101.0f;
        u.sunvp[l][13] = -30.0f / rw;
        u.sunvp[l][14] = 100.0f / 101.0f;
        u.sunvp[l][15] = 1.0f;
        u.ctexel[l] = 0.05f;
        u.cbias[l] = 0.05f / 101.0f;
        u.cdepth[l] = 101.0f;
    }
    u.csplit[0] = 24.4f; u.csplit[1] = 53.6f; u.csplit[2] = 110.0f; u.csplit[3] = 300.0f;
    u.csoft[0] = tanf(SUN_HALF_DEG * 3.14159265f / 180.0f);
    u.csoft[1] = PCSS_SEARCH_MAX;
    u.csoft[2] = PCSS_FILTER_MAX;
    u.csoft[3] = CONTACT_LEN;
    u.sun[2] = 1.0f;
    u.misc[1] = 1.0f;
    memcpy(u.eye, eye, 12);
    memcpy(u.cfwd, c.fwd, 12);
    brr_screen_matrix(c.vp, c.sx, c.ox, c.sy, c.oy, c.flip, c.xft, c.tw, c.th, u.scr);
    td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float width:W height:H mipmapped:NO];
    td.usage = MTLTextureUsageShaderRead;
    td.storageMode = MTLStorageModeShared;
    pt = [s_dev newTextureWithDescriptor:td];
    [pt replaceRegion:MTLRegionMake2D(0, 0, W, H) mipmapLevel:0 withBytes:pos bytesPerRow:W * 16];
    sm = depth_array(map, S, 4);
    printf("gpu cost: %dx%d G-buffer, four %d^2 cascades, %s, ms a frame (median of 41, the modes interleaved after 60 warm-up runs)\n",
           W, H, S, [[s_dev name] UTF8String]);
    for (run = -60; run < 41; run++)
        for (mi = 0; mi < 5; mi++) {
            uint32_t mode = (uint32_t)order[mi];
            id<MTLCommandBuffer> cb = [s_q commandBuffer];
            id<MTLComputeCommandEncoder> ce = [cb computeCommandEncoder];
            [ce setComputePipelineState:k];
            [ce setBytes:&u length:sizeof u atIndex:0];
            [ce setBuffer:out offset:0 atIndex:1];
            [ce setBytes:&mode length:4 atIndex:2];
            [ce setTexture:pt atIndex:0];
            [ce setTexture:sm atIndex:1];
            [ce dispatchThreads:MTLSizeMake(W, H, 1) threadsPerThreadgroup:MTLSizeMake(16, 16, 1)];
            [ce endEncoding];
            [cb commit];
            [cb waitUntilCompleted];
            if (run >= 0)
                ms[mode][run] = (cb.GPUEndTime - cb.GPUStartTime) * 1000.0;
        }
    for (mi = 0; mi < 5; mi++) {
        int mode = order[mi], a, b;
        double med;
        for (a = 1; a < 41; a++)            /* sorted, for the median */
            for (b = a; b > 0 && ms[mode][b - 1] > ms[mode][b]; b--) {
                double t = ms[mode][b];
                ms[mode][b] = ms[mode][b - 1];
                ms[mode][b - 1] = t;
            }
        med = ms[mode][20];
        if (mode == 0)
            base = med;
        printf("  %-22s %6.3f ms (%6.3f over the reads alone; quartiles %.3f..%.3f)\n", names[mode], med, med - base, ms[mode][10], ms[mode][30]);
    }
    free(pos);
    free(map);
}

/* the moving casters' tile against drawing them straight into the slice: a
 * cascade's cache (random depths) and a dozen triangles; the game's way
 * (brr_metal.m: the cache blitted in, the tile composited with scopy and the
 * cropped matrix, blitted over) and the direct way (the cache blitted in,
 * the triangles drawn over the whole slice), both with the game's depth
 * bias; every texel compared */
static void gpu_tile(void)
{
    enum { S = SHADOW_SIZE, L = 4, NT = 12, SL = 2 };
    float *a = malloc((size_t)S * S * L * 4), *ra = malloc((size_t)S * S * L * 4), *rb = malloc((size_t)S * S * L * 4), tri[NT * 9];
    float m[16] = { 1.0f / 40, 0, 0, 0, 0, 1.0f / 40, 0, 0, 0, 0, -0.02f, 0, -0.1f, 0.2f, 0.9f, 1 }, mc[16];
    id<MTLTexture> cache, live, ref, tile;
    id<MTLRenderPipelineState> sp, cp;
    id<MTLDepthStencilState> lessq, always;
    id<MTLBuffer> vb;
    MTLRenderPipelineDescriptor *pd = [MTLRenderPipelineDescriptor new];
    MTLDepthStencilDescriptor *dd = [MTLDepthStencilDescriptor new];
    MTLTextureDescriptor *td;
    NSError *err = nil;
    unsigned seed = 99;
    int dyn[4], tb, w, h, i, x, y, l, way, outside = 0, inside = 0, rectn;
    double mx = 0;
    for (i = 0; i < S * S * L; i++) {
        seed = seed * 1664525u + 1013904223u;
        a[i] = 0.6f + 0.4f * (float)(seed >> 8) / 16777216.0f;
    }
    for (i = 0; i < NT * 9; i++) {             /* triangles a few metres across around (5, -8), 5..25 m up */
        seed = seed * 1664525u + 1013904223u;
        tri[i] = (float)(seed >> 8) / 16777216.0f;
        tri[i] = i % 3 == 0 ? 5.0f + (tri[i] - 0.5f) * 8.0f : i % 3 == 1 ? -8.0f + (tri[i] - 0.5f) * 6.0f : 5.0f + tri[i] * 20.0f;
    }
    brr_shc_rect(m, tri, NT * 3, S, dyn);
    w = dyn[2] - dyn[0];
    h = dyn[3] - dyn[1];
    tb = brr_shc_tile(w, h, S);
    brr_shc_crop(m, S, dyn[0], dyn[1], tb, mc);
    cache = depth_array(a, S, L);
    live = depth_array(a, S, L);
    ref = depth_array(a, S, L);
    td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float width:(NSUInteger)tb height:(NSUInteger)tb mipmapped:NO];
    td.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    td.storageMode = MTLStorageModePrivate;
    tile = [s_dev newTextureWithDescriptor:td];
    vb = [s_dev newBufferWithBytes:tri length:sizeof tri options:MTLResourceStorageModeShared];
    pd.vertexFunction = [s_lib newFunctionWithName:@"svs"];
    pd.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
    sp = [s_dev newRenderPipelineStateWithDescriptor:pd error:&err];
    pd.vertexFunction = [s_lib newFunctionWithName:@"tvs"];
    pd.fragmentFunction = [s_lib newFunctionWithName:@"scopy"];
    cp = [s_dev newRenderPipelineStateWithDescriptor:pd error:&err];
    if (!sp || !cp) {
        fprintf(stderr, "gputest: tile pipelines: %s\n", [[err description] UTF8String]);
        exit(2);
    }
    dd.depthCompareFunction = MTLCompareFunctionLessEqual;
    dd.depthWriteEnabled = YES;
    lessq = [s_dev newDepthStencilStateWithDescriptor:dd];
    dd.depthCompareFunction = MTLCompareFunctionAlways;
    always = [s_dev newDepthStencilStateWithDescriptor:dd];
    for (way = 0; way < 2; way++) {
        id<MTLCommandBuffer> cb = [s_q commandBuffer];
        MTLRenderPassDescriptor *rp = [MTLRenderPassDescriptor renderPassDescriptor];
        id<MTLRenderCommandEncoder> e;
        id<MTLBlitCommandEncoder> be;
        if (way == 0) {                      /* the game's: tile, then blit */
            uint32_t k[4] = { SL, (uint32_t)dyn[0], (uint32_t)dyn[1], 0 };
            rp.depthAttachment.texture = tile;
            rp.depthAttachment.loadAction = MTLLoadActionDontCare;
            rp.depthAttachment.storeAction = MTLStoreActionStore;
            e = [cb renderCommandEncoderWithDescriptor:rp];
            [e setScissorRect:(MTLScissorRect){ 0, 0, (NSUInteger)w, (NSUInteger)h }];
            [e setRenderPipelineState:cp];
            [e setDepthStencilState:always];
            [e setFragmentTexture:cache atIndex:0];
            [e setFragmentBytes:k length:sizeof k atIndex:0];
            [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
            [e setRenderPipelineState:sp];
            [e setDepthStencilState:lessq];
            [e setCullMode:MTLCullModeNone];
            [e setDepthBias:1.0f slopeScale:2.0f clamp:0.01f];
            [e setVertexBytes:mc length:sizeof mc atIndex:1];
            [e setVertexBuffer:vb offset:0 atIndex:0];
            [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:NT * 3];
            [e endEncoding];
            be = [cb blitCommandEncoder];
            [be copyFromTexture:tile sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0) sourceSize:MTLSizeMake((NSUInteger)w, (NSUInteger)h, 1)
                      toTexture:live destinationSlice:SL destinationLevel:0 destinationOrigin:MTLOriginMake((NSUInteger)dyn[0], (NSUInteger)dyn[1], 0)];
            [be endEncoding];
        } else {                             /* straight into the slice */
            rp.depthAttachment.texture = ref;
            rp.depthAttachment.slice = SL;
            rp.depthAttachment.loadAction = MTLLoadActionLoad;
            rp.depthAttachment.storeAction = MTLStoreActionStore;
            e = [cb renderCommandEncoderWithDescriptor:rp];
            [e setRenderPipelineState:sp];
            [e setDepthStencilState:lessq];
            [e setCullMode:MTLCullModeNone];
            [e setDepthBias:1.0f slopeScale:2.0f clamp:0.01f];
            [e setVertexBytes:m length:sizeof m atIndex:1];
            [e setVertexBuffer:vb offset:0 atIndex:0];
            [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:NT * 3];
            [e endEncoding];
        }
        [cb commit];
        [cb waitUntilCompleted];
    }
    depth_read(live, S, L, ra);
    depth_read(ref, S, L, rb);
    rectn = 0;
    for (l = 0; l < L; l++)
        for (y = 0; y < S; y++)
            for (x = 0; x < S; x++) {
                size_t o = (size_t)l * S * S + (size_t)y * S + x;
                int inr = l == SL && x >= dyn[0] && x < dyn[2] && y >= dyn[1] && y < dyn[3];
                rectn += inr && ra[o] < a[o];
                if (!memcmp(&ra[o], &rb[o], 4))
                    continue;
                if (!inr)
                    outside++;
                else {
                    inside++;
                    if (fabs((double)ra[o] - rb[o]) > mx) mx = fabs((double)ra[o] - rb[o]);
                }
            }
    {   /* one texel in depth: the bias the lighting compares with (40 m half-width, 2048 texels, 0.02 a metre) */
        double texd = 2.0 * 40.0 / S * 0.02;
        printf("gpu tile: a %dx%d rectangle in a %d tile; %d texels where the triangles are nearer than the cache;"
               " against drawing them straight in: %d texels differ inside, by %.2e at most (%.2f%% of one texel's depth), %d outside\n",
               w, h, tb, rectn, inside, mx, 100.0 * mx / texd, outside);
        CHECK(outside == 0, "the tile path changed %d texels outside its rectangle", outside);
        CHECK(rectn > 1000, "the test triangles hardly show (%d texels)", rectn);
        /* the rasteriser snaps corners to 1/256 texel; the cropped matrix moves
           them by about 1e-5 texel, so a snap can fall the other way: depth then
           differs by the triangle's slope over 1/256 texel.  A covered texel
           changing (a difference near the cache's own depths) would be a fault */
        CHECK(mx < 0.02 * texd, "the tile path differs from drawing straight in by %.2e (%.1f%% of a texel's depth)", mx, 100.0 * mx / texd);
    }
    free(a);
    free(ra);
    free(rb);
}

/* the shadow update's cost: a height field of 11,250 triangles over 80 m (as
 * many as Coastline's track) and 36 moving triangles (three cars' boxes) in a
 * 300 x 200 texel corner; drawn the old way (each of the four maps cleared,
 * the track and the movers drawn: four encoders) against the cache's steady
 * state with movers in two and in four cascades (one blit encoder restoring
 * last frame's rectangles, a tile pass each, one blit encoder putting the
 * tiles in), and a frame that redraws one cascade's cache.  Median and
 * minimum of 41 after 100 warm-up runs: the minimum is the work itself when
 * other work shares the GPU */
static void gpu_update_cost(void)
{
    enum { S = SHADOW_SIZE, L = 4, G = 75 };
    static const char *const names[5] = { "old: four maps drawn afresh", "cache: movers in 2 cascades", "cache: movers in 4 cascades",
                                          "cache: no movers", "cache: 1 cascade redrawn" };
    float *trk = malloc((size_t)G * G * 2 * 9 * sizeof(float)), mov[36 * 9], *z = malloc((size_t)(G + 1) * (G + 1) * sizeof(float));
    float m[16] = { 1.0f / 40, 0, 0, 0, 0, 1.0f / 40, 0, 0, 0, 0, -0.02f, 0, 0, 0, 0.9f, 1 }, mc[16];
    id<MTLTexture> live, cache, tile[L];
    id<MTLBuffer> tb, mb;
    id<MTLRenderPipelineState> sp, cp;
    id<MTLDepthStencilState> lessq, always;
    MTLRenderPipelineDescriptor *pd = [MTLRenderPipelineDescriptor new];
    MTLDepthStencilDescriptor *dd = [MTLDepthStencilDescriptor new];
    NSError *err = nil;
    unsigned seed = 5;
    int i, x, y, n = 0, dyn[4], tsz, mode, run;
    double v[5][41];
    float *zero = calloc((size_t)S * S * L, 4);
    for (i = 0; i < (G + 1) * (G + 1); i++) {
        seed = seed * 1664525u + 1013904223u;
        z[i] = 5.0f * (float)(seed >> 8) / 16777216.0f;
    }
    for (y = 0; y < G; y++)
        for (x = 0; x < G; x++) {
            float p[4][3];
            int c, q;
            for (c = 0; c < 4; c++) {
                int xx = x + (c & 1), yy = y + (c >> 1);
                p[c][0] = -40.0f + 80.0f * xx / G;
                p[c][1] = -40.0f + 80.0f * yy / G;
                p[c][2] = z[yy * (G + 1) + xx];
            }
            for (q = 0; q < 3; q++) memcpy(&trk[9 * n + 3 * q], p[q == 0 ? 0 : q == 1 ? 1 : 3], 12);
            n++;
            for (q = 0; q < 3; q++) memcpy(&trk[9 * n + 3 * q], p[q == 0 ? 0 : q == 1 ? 3 : 2], 12);
            n++;
        }
    for (i = 0; i < 36 * 9; i++) {
        seed = seed * 1664525u + 1013904223u;
        mov[i] = (float)(seed >> 8) / 16777216.0f;
        mov[i] = i % 3 == 0 ? -30.0f + mov[i] * 11.0f : i % 3 == 1 ? 30.0f - mov[i] * 7.0f : 6.0f + mov[i] * 2.0f;
    }
    brr_shc_rect(m, mov, 36 * 3, S, dyn);
    tsz = brr_shc_tile(dyn[2] - dyn[0], dyn[3] - dyn[1], S);
    brr_shc_crop(m, S, dyn[0], dyn[1], tsz, mc);
    live = depth_array(zero, S, L);
    cache = depth_array(zero, S, L);
    for (i = 0; i < L; i++) {
        MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                                                                     width:(NSUInteger)tsz height:(NSUInteger)tsz mipmapped:NO];
        td.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
        td.storageMode = MTLStorageModePrivate;
        tile[i] = [s_dev newTextureWithDescriptor:td];
    }
    tb = [s_dev newBufferWithBytes:trk length:(size_t)n * 36 options:MTLResourceStorageModeShared];
    mb = [s_dev newBufferWithBytes:mov length:sizeof mov options:MTLResourceStorageModeShared];
    pd.vertexFunction = [s_lib newFunctionWithName:@"svs"];
    pd.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
    sp = [s_dev newRenderPipelineStateWithDescriptor:pd error:&err];
    pd.vertexFunction = [s_lib newFunctionWithName:@"tvs"];
    pd.fragmentFunction = [s_lib newFunctionWithName:@"scopy"];
    cp = [s_dev newRenderPipelineStateWithDescriptor:pd error:&err];
    dd.depthCompareFunction = MTLCompareFunctionLessEqual;
    dd.depthWriteEnabled = YES;
    lessq = [s_dev newDepthStencilStateWithDescriptor:dd];
    dd.depthCompareFunction = MTLCompareFunctionAlways;
    always = [s_dev newDepthStencilStateWithDescriptor:dd];
    for (run = -100; run < 41; run++)
        for (mode = 0; mode < 5; mode++) {
            id<MTLCommandBuffer> cb = [s_q commandBuffer];
            id<MTLRenderCommandEncoder> e;
            id<MTLBlitCommandEncoder> be;
            MTLRenderPassDescriptor *rp;
            int c, k = mode == 1 ? 2 : mode == 2 ? 4 : mode == 4 ? 1 : 0;
            if (mode == 0 || mode == 4)
                for (c = 0; c < (mode == 0 ? L : 1); c++) {     /* a map drawn afresh: the old frame, or a cache redrawn */
                    rp = [MTLRenderPassDescriptor renderPassDescriptor];
                    rp.depthAttachment.texture = mode == 0 ? live : cache;
                    rp.depthAttachment.slice = (NSUInteger)c;
                    rp.depthAttachment.loadAction = MTLLoadActionClear;
                    rp.depthAttachment.clearDepth = 1.0;
                    rp.depthAttachment.storeAction = MTLStoreActionStore;
                    e = [cb renderCommandEncoderWithDescriptor:rp];
                    [e setRenderPipelineState:sp];
                    [e setDepthStencilState:lessq];
                    [e setCullMode:MTLCullModeNone];
                    [e setDepthBias:1.0f slopeScale:2.0f clamp:0.01f];
                    [e setVertexBytes:m length:sizeof m atIndex:1];
                    [e setVertexBuffer:tb offset:0 atIndex:0];
                    [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:(NSUInteger)n * 3];
                    if (mode == 0) {
                        [e setVertexBuffer:mb offset:0 atIndex:0];
                        [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:36 * 3];
                    }
                    [e endEncoding];
                }
            if (mode == 4) {                 /* the redrawn cascade restored whole */
                be = [cb blitCommandEncoder];
                [be copyFromTexture:cache sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0) sourceSize:MTLSizeMake(S, S, 1)
                          toTexture:live destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0, 0, 0)];
                [be endEncoding];
            }
            if (mode >= 1 && k) {
                if (mode != 4) {
                    be = [cb blitCommandEncoder];
                    for (c = 0; c < k; c++)
                        [be copyFromTexture:cache sourceSlice:(NSUInteger)c sourceLevel:0 sourceOrigin:MTLOriginMake((NSUInteger)dyn[0], (NSUInteger)dyn[1], 0)
                                 sourceSize:MTLSizeMake((NSUInteger)(dyn[2] - dyn[0]), (NSUInteger)(dyn[3] - dyn[1]), 1)
                                  toTexture:live destinationSlice:(NSUInteger)c destinationLevel:0
                          destinationOrigin:MTLOriginMake((NSUInteger)dyn[0], (NSUInteger)dyn[1], 0)];
                    [be endEncoding];
                }
                for (c = 0; c < k; c++) {
                    uint32_t kk[4] = { (uint32_t)c, (uint32_t)dyn[0], (uint32_t)dyn[1], 0 };
                    rp = [MTLRenderPassDescriptor renderPassDescriptor];
                    rp.depthAttachment.texture = tile[c];
                    rp.depthAttachment.loadAction = MTLLoadActionDontCare;
                    rp.depthAttachment.storeAction = MTLStoreActionStore;
                    e = [cb renderCommandEncoderWithDescriptor:rp];
                    [e setScissorRect:(MTLScissorRect){ 0, 0, (NSUInteger)(dyn[2] - dyn[0]), (NSUInteger)(dyn[3] - dyn[1]) }];
                    [e setRenderPipelineState:cp];
                    [e setDepthStencilState:always];
                    [e setFragmentTexture:cache atIndex:0];
                    [e setFragmentBytes:kk length:sizeof kk atIndex:0];
                    [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
                    [e setRenderPipelineState:sp];
                    [e setDepthStencilState:lessq];
                    [e setCullMode:MTLCullModeNone];
                    [e setDepthBias:1.0f slopeScale:2.0f clamp:0.01f];
                    [e setVertexBytes:mc length:sizeof mc atIndex:1];
                    [e setVertexBuffer:mb offset:0 atIndex:0];
                    [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:36 * 3];
                    [e endEncoding];
                }
                be = [cb blitCommandEncoder];
                for (c = 0; c < k; c++)
                    [be copyFromTexture:tile[c] sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0)
                             sourceSize:MTLSizeMake((NSUInteger)(dyn[2] - dyn[0]), (NSUInteger)(dyn[3] - dyn[1]), 1)
                              toTexture:live destinationSlice:(NSUInteger)c destinationLevel:0
                      destinationOrigin:MTLOriginMake((NSUInteger)dyn[0], (NSUInteger)dyn[1], 0)];
                [be endEncoding];
            }
            [cb commit];
            [cb waitUntilCompleted];
            if (run >= 0)
                v[mode][run] = mode == 3 ? 0.0 : (cb.GPUEndTime - cb.GPUStartTime) * 1000.0;
        }
    printf("gpu shadow update: %d track triangles, 36 moving in a %dx%d rectangle (%d tile), %s\n", n, dyn[2] - dyn[0], dyn[3] - dyn[1], tsz,
           [[s_dev name] UTF8String]);
    for (mode = 0; mode < 5; mode++) {
        int a2, b2;
        for (a2 = 1; a2 < 41; a2++)
            for (b2 = a2; b2 > 0 && v[mode][b2 - 1] > v[mode][b2]; b2--) {
                double t = v[mode][b2];
                v[mode][b2] = v[mode][b2 - 1];
                v[mode][b2 - 1] = t;
            }
        printf("  %-30s median %.3f ms, min %.3f ms\n", names[mode], v[mode][20], v[mode][0]);
    }
    free(trk);
    free(z);
    free(zero);
}

/* the restore: cache -> live by blit, a region of one slice and the whole of
 * another (brr_metal.m: last frame's rectangle, or all after a redraw);
 * every texel compared as bits */
static void gpu_restore(void)
{
    enum { S = 256, L = 4 };
    float *a = malloc(S * S * L * 4), *b = malloc(S * S * L * 4), *r = malloc(S * S * L * 4);
    id<MTLTexture> cache, live;
    id<MTLCommandBuffer> cb;
    id<MTLBlitCommandEncoder> be;
    unsigned seed = 7;
    int i, bad = 0, keptbad = 0, x, y, l;
    static const int rect[4] = { 37, 50, 37 + 141, 50 + 99 };
    for (i = 0; i < S * S * L; i++) {
        seed = seed * 1664525u + 1013904223u;
        a[i] = (float)(seed >> 8) / 16777216.0f;
        seed = seed * 1664525u + 1013904223u;
        b[i] = (float)(seed >> 8) / 16777216.0f;
    }
    cache = depth_array(a, S, L);
    live = depth_array(b, S, L);
    cb = [s_q commandBuffer];
    be = [cb blitCommandEncoder];
    [be copyFromTexture:cache sourceSlice:2 sourceLevel:0 sourceOrigin:MTLOriginMake((NSUInteger)rect[0], (NSUInteger)rect[1], 0)
             sourceSize:MTLSizeMake((NSUInteger)(rect[2] - rect[0]), (NSUInteger)(rect[3] - rect[1]), 1)
              toTexture:live destinationSlice:2 destinationLevel:0 destinationOrigin:MTLOriginMake((NSUInteger)rect[0], (NSUInteger)rect[1], 0)];
    [be copyFromTexture:cache sourceSlice:1 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0) sourceSize:MTLSizeMake(S, S, 1)
              toTexture:live destinationSlice:1 destinationLevel:0 destinationOrigin:MTLOriginMake(0, 0, 0)];
    [be endEncoding];
    [cb commit];
    [cb waitUntilCompleted];
    depth_read(live, S, L, r);
    for (l = 0; l < L; l++)
        for (y = 0; y < S; y++)
            for (x = 0; x < S; x++) {
                size_t o = (size_t)l * S * S + (size_t)y * S + x;
                int inr = l == 1 || (l == 2 && x >= rect[0] && x < rect[2] && y >= rect[1] && y < rect[3]);
                if (inr && memcmp(&r[o], &a[o], 4))
                    bad++;
                if (!inr && memcmp(&r[o], &b[o], 4))
                    keptbad++;
            }
    printf("gpu restore: %d texels copied from the cache not bit-exact, %d texels outside the restore changed\n", bad, keptbad);
    CHECK(bad == 0 && keptbad == 0, "the restore is not an exact copy of its region");
    free(a);
    free(b);
    free(r);
}

/* the renderer's whole shader source (run.sh extracts it as brr_open builds
 * it) compiled as brr_open compiles it, by the system's Metal compiler, and
 * the functions the lit frame makes pipelines of looked up */
static void whole_source(const char *path)
{
    static NSString *const fns[] = { @"vs", @"fs", @"fsgb", @"fsbk", @"lvs", @"lfs", @"fsf", @"bdn", @"bup", @"pfs", @"svs", @"scopy",
                                     @"atm_t", @"atm_ms", @"atm_sv", @"atm_irr", @"skyfs", @"fsbk2" };
    NSError *err = nil;
    NSString *src = [NSString stringWithContentsOfFile:[NSString stringWithUTF8String:path] encoding:NSUTF8StringEncoding error:&err];
    id<MTLLibrary> lib;
    unsigned i, missing = 0;
    if (!src) {
        CHECK(0, "cannot read %s", path);
        return;
    }
    lib = [s_dev newLibraryWithSource:src options:nil error:&err];
    if (!lib) {
        CHECK(0, "the renderer's shader source does not compile: %s", [[err description] UTF8String]);
        return;
    }
    for (i = 0; i < sizeof fns / sizeof fns[0]; i++)
        if (![lib newFunctionWithName:fns[i]]) {
            missing++;
            printf("  missing function %s\n", [fns[i] UTF8String]);
        }
    printf("whole shader source (%s): compiled by the system's Metal compiler, %s; %u of %u lit-frame functions found\n", path,
           err ? "with diagnostics" : "no diagnostics", (unsigned)(sizeof fns / sizeof fns[0]) - missing, (unsigned)(sizeof fns / sizeof fns[0]));
    if (err)
        printf("%s\n", [[err description] UTF8String]);
    CHECK(missing == 0, "%u shader functions missing", missing);
}

int main(int argc, char **argv)
{
    @autoreleasepool {
        NSError *err = nil;
        NSString *src;
        s_dev = MTLCreateSystemDefaultDevice();
        if (!s_dev) {
            fprintf(stderr, "gputest: no Metal device\n");
            return 2;
        }
        s_q = [s_dev newCommandQueue];
        if (argc > 1)
            whole_source(argv[1]);
        src = [@"#include <metal_stdlib>\nusing namespace metal;\n" BRR_LU_SRC BRR_SHADOW_SRC stringByAppendingString:k_test];
        s_lib = [s_dev newLibraryWithSource:src options:nil error:&err];
        if (!s_lib) {
            fprintf(stderr, "gputest: shader: %s\n", [[err description] UTF8String]);
            return 2;
        }
        printf("gputest on %s\n", [[s_dev name] UTF8String]);
        {
            id<MTLBuffer> o = [s_dev newBufferWithLength:4 options:MTLResourceStorageModeShared];
            id<MTLCommandBuffer> cb = [s_q commandBuffer];
            id<MTLComputeCommandEncoder> ce = [cb computeCommandEncoder];
            [ce setComputePipelineState:kernel_named(@"ksize")];
            [ce setBuffer:o offset:0 atIndex:0];
            [ce dispatchThreads:MTLSizeMake(1, 1, 1) threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
            [ce endEncoding];
            [cb commit];
            [cb waitUntilCompleted];
            printf("LU: the shader's %u bytes, the C struct's %zu\n", *(uint32_t *)[o contents], sizeof(LU));
            CHECK(*(uint32_t *)[o contents] == sizeof(LU), "LU differs between the shader and C");
        }
        gpu_penumbra();
        gpu_contact();
        gpu_restore();
        gpu_tile();
        if (!getenv("GPUTEST_NOCOST"))
        {
            gpu_cost();
            gpu_update_cost();
        }
        printf(s_fail ? "gputest: %d FAILED\n" : "gputest: all passed\n", s_fail);
    }
    return s_fail != 0;
}
