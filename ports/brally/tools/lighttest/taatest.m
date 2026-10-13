/* taatest.m: the lit frame's TAA (brr_metal_taa.h, .m) against closed forms
 * and a CPU reference; no game.  PASS or FAIL per check, with its numbers. */
#import <Metal/Metal.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "brr_shadow.h"
#include "brr_metal_taa.h"
#include "brr_metal_taa_api.h"

static int fails;
static void check(int ok, const char *what, const char *fmt, double a, double b)
{
    printf("%s  %s: ", ok ? "PASS" : "FAIL", what);
    printf(fmt, a, b);
    printf("\n");
    fails += !ok;
}

/* row-vector view x projection: look from e at t, z up, vertical fov, aspect */
static void camvp(const float e[3], const float t[3], float fov, float asp, float vp[16])
{
    float f[3], r[3], u[3], l, n = 1.0f, fa = 5000.0f, ys = 1.0f / tanf(fov * 0.5f), xs = ys / asp;
    float V[16], P[16];
    int i, j, k;
    for (k = 0; k < 3; k++) f[k] = t[k] - e[k];
    l = sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
    for (k = 0; k < 3; k++) f[k] /= l;
    r[0] = f[1]; r[1] = -f[0]; r[2] = 0; l = sqrtf(r[0] * r[0] + r[1] * r[1]);
    r[0] /= l; r[1] /= l;
    u[0] = r[1] * f[2] - r[2] * f[1]; u[1] = r[2] * f[0] - r[0] * f[2]; u[2] = r[0] * f[1] - r[1] * f[0];
    memset(V, 0, sizeof V);
    for (k = 0; k < 3; k++) {     /* clip = [p 1] V: rows are the world axes */
        V[4 * k + 0] = r[k]; V[4 * k + 1] = u[k]; V[4 * k + 2] = f[k];
    }
    V[12] = -(r[0] * e[0] + r[1] * e[1] + r[2] * e[2]);
    V[13] = -(u[0] * e[0] + u[1] * e[1] + u[2] * e[2]);
    V[14] = -(f[0] * e[0] + f[1] * e[1] + f[2] * e[2]);
    V[15] = 1;
    memset(P, 0, sizeof P);
    P[0] = xs; P[5] = ys; P[10] = fa / (fa - n); P[11] = 1; P[14] = -n * fa / (fa - n);
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++) {
            vp[4 * i + j] = 0;
            for (k = 0; k < 4; k++) vp[4 * i + j] += V[4 * i + k] * P[4 * k + j];
        }
}

/* the same chain written out by hand: clip, viewport, screen map, target pixels */
static void byhand(const float vp[16], const float xft[4], float sx, float ox, float sy, float oy, float tw, float th,
                   const float p[3], float out[2])
{
    float c[4], gx, gy, nx, ny;
    int j;
    for (j = 0; j < 4; j++) c[j] = p[0] * vp[j] + p[1] * vp[4 + j] + p[2] * vp[8 + j] + vp[12 + j];
    gx = c[0] / c[3] * sx + ox;
    gy = c[1] / c[3] * sy + oy;
    nx = gx * xft[0] + xft[1];
    ny = gy * xft[2] + xft[3];
    out[0] = (nx * 0.5f + 0.5f) * tw;
    out[1] = (0.5f - ny * 0.5f) * th;
}

#define W 8
#define H 4
static float edge_cur(int x, const float jit[2], float xe, float A, float B)
{
    return (float)x + 0.5f - jit[0] < xe ? A : B;
}

/* the resolve on the CPU, the shader's maths with the C helpers (static camera: history at the texel) */
static void cpu_resolve(float cur[H][W], float hist[H][W][4], int reset, float out[H][W][4], int karis)
{
    int x, y, i, j, k;
    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++) {
            float c[3], m1[3] = { 0 }, m2[3] = { 0 }, lo[3] = { 1e30f, 1e30f, 1e30f }, hi[3] = { -1e30f, -1e30f, -1e30f };
            float hy[3], hc[3], h[3], o[3], a, n;
            c[0] = c[1] = c[2] = cur[y][x];
            for (j = -1; j <= 1; j++)
                for (i = -1; i <= 1; i++) {
                    int xx = x + i < 0 ? 0 : x + i >= W ? W - 1 : x + i, yy = y + j < 0 ? 0 : y + j >= H ? H - 1 : y + j;
                    float v[3] = { cur[yy][xx], cur[yy][xx], cur[yy][xx] }, yc[3];
                    brr_ycocg(v, yc);
                    for (k = 0; k < 3; k++) {
                        m1[k] += yc[k]; m2[k] += yc[k] * yc[k];
                        lo[k] = fminf(lo[k], yc[k]); hi[k] = fmaxf(hi[k], yc[k]);
                    }
                }
            for (k = 0; k < 3; k++) {
                float sd;
                m1[k] /= 9; m2[k] /= 9; sd = sqrtf(fmaxf(m2[k] - m1[k] * m1[k], 0));
                lo[k] = fmaxf(lo[k], m1[k] - TAA_GAMMA * sd); hi[k] = fminf(hi[k], m1[k] + TAA_GAMMA * sd);
            }
            if (reset) {
                for (k = 0; k < 3; k++) out[y][x][k] = c[k];
                out[y][x][3] = 1;
                continue;
            }
            brr_ycocg(hist[y][x], hy);
            brr_taa_clip(hy, lo, hi, hc);
            brr_rgb(hc, h);
            n = hist[y][x][3];
            a = fmaxf(1.0f / (n + 1.0f), TAA_ALPHA);
            brr_taa_blend(c, h, a, karis, o);
            for (k = 0; k < 3; k++) out[y][x][k] = o[k];
            out[y][x][3] = fminf(n + 1, 255);
        }
}

static id<MTLTexture> mk(id<MTLDevice> d, MTLPixelFormat f, int rt)
{
    MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:f width:W height:H mipmapped:NO];
    td.usage = MTLTextureUsageShaderRead | (rt ? MTLTextureUsageRenderTarget : 0);
    td.storageMode = MTLStorageModeShared;
    return [d newTextureWithDescriptor:td];
}

static void gpu_converge(id<MTLDevice> dev, int karis, float A, float B, float xe)
{
    NSError *err = nil;
    id<MTLLibrary> lib = [dev newLibraryWithSource:@BRR_TAA_SRC options:nil error:&err];
    MTLRenderPipelineDescriptor *pd = [MTLRenderPipelineDescriptor new];
    id<MTLRenderPipelineState> ps;
    id<MTLCommandQueue> q = [dev newCommandQueue];
    id<MTLTexture> cur = mk(dev, MTLPixelFormatRGBA32Float, 0), gpos = mk(dev, MTLPixelFormatRGBA32Float, 0);
    id<MTLTexture> th[2] = { mk(dev, MTLPixelFormatRGBA32Float, 1), mk(dev, MTLPixelFormatRGBA32Float, 1) };
    id<MTLTexture> td[2] = { mk(dev, MTLPixelFormatR32Float, 1), mk(dev, MTLPixelFormatR32Float, 1) };
    static float g[H][W][4], cpuh[H][W][4], cpuo[H][W][4], gh[H][W][4], cf[H][W];
    float u[16 + 16 + 4 + 4 + 4];
    int fr, x, y, k, p = 0;
    double maxd = 0, maxc = 0;
    char what[160];
    pd.vertexFunction = [lib newFunctionWithName:@"taa_vs"];
    pd.fragmentFunction = [lib newFunctionWithName:@"taa_resolve"];
    pd.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA32Float;
    pd.colorAttachments[1].pixelFormat = MTLPixelFormatR32Float;
    ps = [dev newRenderPipelineStateWithDescriptor:pd error:&err];
    if (!ps) { printf("FAIL  taa pipeline: %s\n", [[err description] UTF8String]); fails++; return; }
    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++) {
            g[y][x][0] = x + 0.5f; g[y][x][1] = y + 0.5f; g[y][x][2] = 1; g[y][x][3] = 0;
        }
    [gpos replaceRegion:MTLRegionMake2D(0, 0, W, H) mipmapLevel:0 withBytes:g bytesPerRow:W * 16];
    memset(u, 0, sizeof u);
    u[0] = 1; u[5] = 1; u[15] = 1;               /* S: world (x, y) is the pixel */
    u[32] = 0; u[33] = 0; u[34] = -10;           /* eye */
    for (fr = 0; fr < 16; fr++) {
        float jit[2], ndc[2];
        static float c4[H][W][4];
        id<MTLCommandBuffer> cb = [q commandBuffer];
        MTLRenderPassDescriptor *rp = [MTLRenderPassDescriptor renderPassDescriptor];
        id<MTLRenderCommandEncoder> e;
        brr_taa_jitter((unsigned)fr, W, H, jit, ndc);
        for (y = 0; y < H; y++)
            for (x = 0; x < W; x++) {
                cf[y][x] = edge_cur(x, jit, xe, A, B);
                c4[y][x][0] = c4[y][x][1] = c4[y][x][2] = cf[y][x]; c4[y][x][3] = 1;
            }
        [cur replaceRegion:MTLRegionMake2D(0, 0, W, H) mipmapLevel:0 withBytes:c4 bytesPerRow:W * 16];
        u[36] = jit[0]; u[37] = jit[1];
        u[40] = TAA_ALPHA; u[41] = fr == 0; u[42] = karis; u[43] = TAA_GAMMA;
        rp.colorAttachments[0].texture = th[p];
        rp.colorAttachments[1].texture = td[p];
        e = [cb renderCommandEncoderWithDescriptor:rp];
        [e setRenderPipelineState:ps];
        [e setFragmentBytes:u length:sizeof u atIndex:0];
        [e setFragmentTexture:cur atIndex:0];
        [e setFragmentTexture:th[p ^ 1] atIndex:1];
        [e setFragmentTexture:td[p ^ 1] atIndex:2];
        [e setFragmentTexture:gpos atIndex:3];
        [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        [e endEncoding];
        [cb commit];
        [cb waitUntilCompleted];
        cpu_resolve(cf, cpuh, fr == 0, cpuo, karis);
        memcpy(cpuh, cpuo, sizeof cpuh);
        [th[p] getBytes:gh bytesPerRow:W * 16 fromRegion:MTLRegionMake2D(0, 0, W, H) mipmapLevel:0];
        p ^= 1;
    }
    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++)
            for (k = 0; k < 3; k++)
                maxd = fmax(maxd, fabs(gh[y][x][k] - cpuh[y][x][k]));
    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++) {
            /* the pixel's coverage by the A side, recovered from the resolve */
            float f = fminf(fmaxf(xe - x, 0), 1), v = gh[y][x][0], fe;
            if (karis) {   /* Karis's fixed point: v = (f a' A + (1-f) b' B) / (f a' + (1-f) b'), a' = 1/(1+A) */
                float a1 = 1 / (1 + A), b1 = 1 / (1 + B);
                fe = (v - B) * b1 / ((A - v) * a1 + (v - B) * b1);
            } else {
                fe = (v - B) / (A - B);
            }
            maxc = fmax(maxc, fabs(fe - f));
        }
    snprintf(what, sizeof what, "GPU resolve = CPU reference over 16 frames (static jittered edge, karis %d)", karis);
    check(maxd < 1e-4, what, "max |gpu - cpu| %.2g (limit %.0g)", maxd, 1e-4);
    snprintf(what, sizeof what, "edge converges to its analytic coverage after 16 frames (karis %d)", karis);
    check(maxc < 0.02, what, "max coverage error %.4f (limit %.2f)", maxc, 0.02);
}

int main(void)
{
    @autoreleasepool {
        id<MTLDevice> dev = MTLCreateSystemDefaultDevice();
        int i, n, ok;
        double mx = 0, my = 0, worst = 0;
        float jit[2], ndc[2];
        /* (1) Halton and the jitter */
        for (n = 0; n < TAA_PHASES; n++) {
            brr_taa_jitter((unsigned)n, 640, 480, jit, ndc);
            mx += jit[0]; my += jit[1];
            worst = fmax(worst, fmax(fabs(jit[0]), fabs(jit[1])));
            if (fabsf(ndc[0] - jit[0] * 2 / 640) > 1e-9f || fabsf(ndc[1] + jit[1] * 2 / 480) > 1e-9f)
                worst = 9;
        }
        mx /= TAA_PHASES; my /= TAA_PHASES;
        check(fabs(mx) < 0.035 && fabs(my) < 0.035, "jitter mean over 16 phases ~0", "mean x %.4f y %.4f", mx, my);
        check(worst < 0.5, "jitter inside the pixel, ndc = 2 px / size (y flipped)", "max |offset| %.4f px (limit %.1f)", worst, 0.5);
        {   /* coverage: every 4x4 sub-cell of the pixel holds one of 16 phases */
            int cell[16] = { 0 }, c = 0;
            for (n = 0; n < 16; n++) {
                brr_taa_jitter((unsigned)n, 1, 1, jit, ndc);
                cell[(int)((jit[0] + 0.5f) * 4) + 4 * (int)((jit[1] + 0.5f) * 4)]++;
            }
            for (i = 0; i < 16; i++) c += cell[i] > 0;
            check(c >= 14, "16 phases cover the pixel (4x4 cells hit)", "%.0f of %.0f cells", c, 16);
        }
        /* (2) reprojection through two synthetic cameras */
        {
            float e0[3] = { 0, -30, 5 }, t0[3] = { 0, 0, 2 }, e1[3] = { 3, -25, 5.5f }, t1[3] = { 1, 0, 2 };
            float vp0[16], vp1[16], S0[16], S1[16], F1[16], Fi1[16], xft[4] = { 2.0f / 640, -1.0f, -2.0f / 480, 1.0f };
            double err = 0, errs = 0;
            camvp(e0, t0, 1.0f, 4.0f / 3, vp0);
            camvp(e1, t1, 1.1f, 4.0f / 3, vp1);
            brr_screen_matrix(vp0, 320, 320, -240, 240, 0, xft, 1280, 960, S0);
            brr_screen_matrix(vp1, 320, 320, -240, 240, 0, xft, 1280, 960, S1);
            for (i = 0; i < 200; i++) {
                float p[3] = { (float)(i % 20) - 10, (float)(i / 20) * 2, (float)(i % 7) * 0.5f }, a[2], b[2];
                if (!brr_taa_reproject(S0, p, 1, a)) continue;
                byhand(vp0, xft, 320, 320, -240, 240, 1280, 960, p, b);
                err = fmax(err, fmax(fabs(a[0] - b[0]), fabs(a[1] - b[1])));
            }
            check(err < 0.01, "world points through last frame's camera = by hand", "max error %.5f px (limit %.2f)", err, 0.01);
            /* the sky: a pixel's ray now, through last frame's camera at infinity, against a point 1e5 out */
            brr_taa_full(S1, vp1, F1);
            ok = brr_inv4(F1, Fi1);
            for (i = 0; i < 50; i++) {
                float d[3], q[3], a[2], b[2], l;
                int k;
                brr_taa_ray(Fi1, 100.0f + 20 * i, 200.0f + 10 * i, d);
                l = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
                for (k = 0; k < 3; k++) q[k] = e1[k] + d[k] / l * 1e5f;
                brr_taa_reproject(S0, d, 0, a);
                byhand(vp0, xft, 320, 320, -240, 240, 1280, 960, q, b);
                errs = fmax(errs, fmax(fabs(a[0] - b[0]), fabs(a[1] - b[1])));
            }
            check(ok && errs < 0.05, "sky directions through last frame's camera (at infinity vs 1e5 out)", "max error %.4f px (limit %.2f)", errs, 0.05);
        }
        /* (3) variance clipping */
        {
            float lo[3] = { 0.2f, -0.05f, -0.05f }, hi[3] = { 0.6f, 0.05f, 0.05f }, ghost[3] = { 2.0f, 0.3f, 0.0f }, good[3] = { 0.4f, 0.01f, -0.02f }, o[3];
            brr_taa_clip(ghost, lo, hi, o);
            check(o[0] <= 0.6f + 1e-5f && fabsf(o[1]) <= 0.05f + 1e-5f && o[0] < 2.0f, "a ghost outside the box is clipped to it", "Y %.3f -> %.3f", 2.0, o[0]);
            brr_taa_clip(good, lo, hi, o);
            check(fabsf(o[0] - good[0]) + fabsf(o[1] - good[1]) + fabsf(o[2] - good[2]) < 1e-6f, "a valid history inside the box is kept", "Y %.3f -> %.3f", good[0], o[0]);
        }
        /* (dynamic resolution) a heavy then light load: down to the floor, back to 1 */
        {
            BrrDynRes d = { 0, 0, 0 };
            float s = 1;
            for (i = 0; i < 200; i++) s = brr_taa_dynres(&d, 22.0f * s * s, 16.6f, 0.5f);
            float s1 = s;
            for (i = 0; i < 400; i++) s = brr_taa_dynres(&d, 12.0f * s * s, 16.6f, 0.5f);
            check(s1 < 0.9f && 22.0f * s1 * s1 < 16.6f * 1.08f && s == 1.0f, "dynamic resolution holds 16.6 ms and returns to full size",
                  "scale under load %.2f, after %.2f", s1, s);
        }
        if (!dev) { printf("SKIP  GPU checks: no Metal device\n"); return fails != 0; }
        gpu_converge(dev, 0, 1.0f, 0.0f, 3.3f);
        gpu_converge(dev, 1, 1.0f, 0.0f, 3.3f);
        /* (4) MetalFX */
        {
            int sup = brr_taa_metalfx_supported(dev);
            printf("INFO  MetalFX temporal scaler on %s: %s\n", [[dev name] UTF8String], sup ? "supported" : "not supported (the own TAA is used)");
            if (sup) {
                id fx = brr_taa_metalfx_make(dev, 858, 483, 1280, 720);
                id fx1 = brr_taa_metalfx_make(dev, 1280, 720, 1280, 720);
                check(fx != nil && fx1 != nil, "MetalFX scaler made (0.67 internal scale, and 1:1)", "%.0f %.0f", fx != nil, fx1 != nil);
            }
        }
        printf("%s\n", fails ? "TAA: FAIL" : "TAA: all pass");
        return fails != 0;
    }
}
