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
                if (hist[y][x][3] >= TAA_PHASES) {
                    lo[k] = fmaxf(lo[k], m1[k] - TAA_GAMMA * sd); hi[k] = fminf(hi[k], m1[k] + TAA_GAMMA * sd);
                }
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

/* (6) the lit frame's scaled path through brr_taa_resolve: a 3024 x 1964
 * target (a 14" Retina) at the default scale and at the controller's ends,
 * synthetic colour, depth (Depth32Float, cleared to 1, less-than: not
 * reversed) and G-buffer positions; the output at the target's size, the
 * command buffer completes without an error */
static id<MTLTexture> synth(id<MTLDevice> dev, MTLPixelFormat f, int w, int h, MTLTextureUsage use)
{
    MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:f width:(NSUInteger)w height:(NSUInteger)h mipmapped:NO];
    td.usage = use;
    td.storageMode = MTLStorageModePrivate;
    return [dev newTextureWithDescriptor:td];
}

static void metalfx_upscale(id<MTLDevice> dev)
{
    static const float sc[3] = { 0.67f, 0.5f, 1.0f };
    const int tw = 3024, th = 1964;
    float S[16], vp[16], e[3] = { 0, 0, 5 }, t[3] = { 50, 0, 0 }, xft[4] = { 2.0f / 640.0f, -1.0f, -2.0f / 480.0f, 1.0f };
    int k, up = 0;
    float d = brr_taa_scale(dev, tw, th, 0.0f, &up);
    check(fabsf(d - 0.67f) < 1e-6f && up, "default scale on a 3024x1964 target (over 2.5 MP)", "%.2f, through MetalFX %.0f", d, up);
    d = brr_taa_scale(dev, 1920, 1080, 0.0f, &up);
    check(d == 1.0f && !up, "scale on a 1920x1080 target (2.1 MP)", "%.2f, through MetalFX %.0f", d, up);
    camvp(e, t, 1.0f, (float)tw / th, vp);
    for (k = 0; k < 3; k++) {
        int lw = (int)lroundf(tw * sc[k]), lh = (int)lroundf(th * sc[k]), f;
        id<MTLCommandQueue> q = [dev newCommandQueue];
        id<MTLCommandBuffer> cb = nil;
        id<MTLTexture> hdr = synth(dev, MTLPixelFormatRGBA16Float, lw, lh, MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead);
        id<MTLTexture> gpos = synth(dev, MTLPixelFormatRGBA32Float, lw, lh, MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead);
        id<MTLTexture> dep = synth(dev, MTLPixelFormatDepth32Float, lw, lh, MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead);
        id<MTLBuffer> xt = [dev newBufferWithBytes:xft length:sizeof xft options:MTLResourceStorageModeShared];
        id<MTLTexture> out = nil;
        double ms = 0;
        brr_screen_matrix(vp, 320, 320, 240, 240, 0, xft, (float)lw, (float)lh, S);
        brr_taa_reset();
        for (f = 0; f < 4; f++) {
            MTLRenderPassDescriptor *rp = [MTLRenderPassDescriptor renderPassDescriptor];
            cb = [q commandBuffer];
            rp.colorAttachments[0].texture = hdr;
            rp.colorAttachments[0].loadAction = MTLLoadActionClear;
            rp.colorAttachments[0].clearColor = MTLClearColorMake(0.2 * f, 0.5, 0.8, 1);
            rp.colorAttachments[0].storeAction = MTLStoreActionStore;
            rp.colorAttachments[1].texture = gpos;
            rp.colorAttachments[1].loadAction = MTLLoadActionClear;
            rp.colorAttachments[1].clearColor = MTLClearColorMake(40, 3, 1, 1);
            rp.colorAttachments[1].storeAction = MTLStoreActionStore;
            rp.depthAttachment.texture = dep;
            rp.depthAttachment.loadAction = MTLLoadActionClear;
            rp.depthAttachment.clearDepth = 0.5 + 0.1 * f;
            rp.depthAttachment.storeAction = MTLStoreActionStore;
            [[cb renderCommandEncoderWithDescriptor:rp] endEncoding];
            brr_taa_begin(dev, xt, 0, 1, lw, lh, NULL);
            out = brr_taa_resolve(dev, cb, hdr, gpos, dep, S, vp, e, tw, th, 1);
            [cb commit];
            [cb waitUntilCompleted];
            if (cb.status != MTLCommandBufferStatusCompleted)
                break;
            ms = (cb.GPUEndTime - cb.GPUStartTime) * 1000.0;
        }
        {
            char what[200];
            snprintf(what, sizeof what, "MetalFX upscale %dx%d -> %dx%d (scale %.2f): output at the target's size, 4 frames complete", lw, lh, tw, th, sc[k]);
            check(out && (int)out.width == tw && (int)out.height == th && cb.status == MTLCommandBufferStatusCompleted, what,
                  "status %.0f, last frame %.3f ms GPU", (double)cb.status, ms);
        }
    }
    brr_taa_reset();
}

/* (5) the lit 3D at an internal size: the screen map (brr_screen_matrix) at
 * the internal size takes the game's viewport corners to the internal
 * target's corners, and a world point to its full-size pixel x the scale */
static void scaled_map(void)
{
    static const int T[3][2] = { { 3024, 1964 }, { 2560, 1600 }, { 1920, 1080 } };
    static const float S[3] = { 0.67f, 0.5f, 0.85f };
    float e[3] = { 10, -40, 12 }, t[3] = { 60, 30, 0 }, vp[16];
    float xft[4] = { 2.0f / 640.0f, -1.0f, -2.0f / 480.0f, 1.0f };   /* the game's 640 x 480 to ndc, rows down */
    double worst_c = 0, worst_p = 0, worst_m = 0;
    int a, b, k, q;
    camvp(e, t, 1.0f, 640.0f / 480.0f, vp);
    for (a = 0; a < 3; a++)
        for (b = 0; b < 3; b++) {
            float tw = (float)T[a][0], th = (float)T[a][1];
            float lw = roundf(tw * S[b]), lh = roundf(th * S[b]), Sf[16], Si[16];
            brr_screen_matrix(vp, 320.0f, 320.0f, 240.0f, 240.0f, 0.0f, xft, tw, th, Sf);
            brr_screen_matrix(vp, 320.0f, 320.0f, 240.0f, 240.0f, 0.0f, xft, lw, lh, Si);
            /* the viewport's corners (clip x / w, y / w = +-1: the game's x = +-sx + ox) through the
               internal matrix: a clip point [cx w, cy w, *, w] gives (A cx w + B w, C cy w + D w, w),
               the matrix's rows being A vp0 + B vp3 and C vp1 + D vp3 */
            for (k = 0; k < 4; k++) {
                float cx = (k & 1) ? 1.0f : -1.0f, cy = (k & 2) ? 1.0f : -1.0f, ww = 7.0f, cc[4], hh[4];
                double ex = cx > 0 ? lw : 0.0, ey = cy > 0 ? lh : 0.0;   /* the game's rows run down: its y = sy + oy is the bottom */
                /* recover the rows' action on clip space: S = M vp with M's rows (A,0,0,B), (0,C,0,D), (0,0,0,0), (0,0,0,1) */
                float A = 0.5f * lw * xft[0] * 320.0f, B = 0.5f * lw * (1.0f + xft[0] * 320.0f + xft[1]);
                float C = -0.5f * lh * xft[2] * 240.0f, D = 0.5f * lh * (1.0f - xft[2] * 240.0f - xft[3]);
                cc[0] = cx * ww; cc[1] = cy * ww; cc[2] = 0.0f; cc[3] = ww;
                hh[0] = A * cc[0] + B * cc[3];
                hh[1] = C * cc[1] + D * cc[3];
                hh[3] = cc[3];
                worst_c = fmax(worst_c, fabs(hh[0] / hh[3] - ex) + fabs(hh[1] / hh[3] - ey));
            }
            {   /* the matrix built is M vp: check M against S on vp's columns */
                int i;
                float A = 0.5f * lw * xft[0] * 320.0f, B = 0.5f * lw * (1.0f + xft[0] * 320.0f + xft[1]);
                float C = -0.5f * lh * xft[2] * 240.0f, D = 0.5f * lh * (1.0f - xft[2] * 240.0f - xft[3]);
                for (i = 0; i < 4; i++)
                    worst_m = fmax(worst_m, fabs(Si[4 * i] - (A * vp[4 * i] + B * vp[4 * i + 3])) +
                                            fabs(Si[4 * i + 1] - (C * vp[4 * i + 1] + D * vp[4 * i + 3])));
            }
            /* world points across the view: internal pixel = full pixel x (internal / full size) */
            for (q = 0; q < 64; q++) {
                float p[3] = { 20.0f + 7.0f * (float)(q % 8), -10.0f + 9.0f * (float)(q / 8), (float)(q % 5) }, f[2], g[2];
                if (!brr_taa_reproject(Sf, p, 1.0f, f) || !brr_taa_reproject(Si, p, 1.0f, g))
                    continue;
                worst_p = fmax(worst_p, fmax(fabs(g[0] - f[0] * lw / tw), fabs(g[1] - f[1] * lh / th)));
            }
        }
    check(worst_c < 1e-3, "scaled screen map: the game's viewport corners on the internal target's corners (3 targets x 3 scales, float32 rounding)",
          "max error %.3g px (limit %.0g)", worst_c, 1e-3);
    check(worst_m < 1e-3, "scaled screen map: the matrix built = the viewport map above x the view projection", "max error %.2g (limit %.0g)", worst_m, 1e-3);
    check(worst_p < 0.01, "scaled screen map: a world point at internal pixel = full pixel x scale (64 points, 9 sizes)",
          "max error %.4f px (limit %.2f)", worst_p, 0.01);
}

int main(void)
{
    @autoreleasepool {
        id<MTLDevice> dev = MTLCreateSystemDefaultDevice();
        int i, n, ok;
        double mx = 0, my = 0, worst = 0;
        float jit[2], ndc[2];
        scaled_map();
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
                for (k = 0; k < 3; k++) q[k] = e1[k] + d[k] / l * 1e7f;
                brr_taa_reproject(S0, d, 0, a);
                byhand(vp0, xft, 320, 320, -240, 240, 1280, 960, q, b);
                errs = fmax(errs, fmax(fabs(a[0] - b[0]), fabs(a[1] - b[1])));
            }
            check(ok && errs < 0.05, "sky directions through last frame's camera (at infinity vs 1e7 out: 1e5 has 0.06 px of parallax between the cameras)", "max error %.4f px (limit %.2f)", errs, 0.05);
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
        {   /* the cost: the resolve at 1920x1080 on synthetic textures (history valid, every pixel reprojected) */
            NSError *err = nil;
            id<MTLLibrary> lib = [dev newLibraryWithSource:@BRR_TAA_SRC options:nil error:&err];
            MTLRenderPipelineDescriptor *pd = [MTLRenderPipelineDescriptor new];
            id<MTLCommandQueue> q = [dev newCommandQueue];
            MTLTextureDescriptor *td;
            id<MTLTexture> t[5];
            static const MTLPixelFormat f[5] = { MTLPixelFormatRGBA16Float, MTLPixelFormatRGBA16Float, MTLPixelFormatR32Float,
                                                 MTLPixelFormatRGBA32Float, MTLPixelFormatRGBA16Float };
            float u[44];
            double best = 1e9;
            int r;
            pd.vertexFunction = [lib newFunctionWithName:@"taa_vs"];
            pd.fragmentFunction = [lib newFunctionWithName:@"taa_resolve"];
            pd.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA16Float;
            pd.colorAttachments[1].pixelFormat = MTLPixelFormatR32Float;
            id<MTLRenderPipelineState> ps = [dev newRenderPipelineStateWithDescriptor:pd error:&err];
            for (i = 0; i < 5; i++) {
                td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:f[i] width:1920 height:1080 mipmapped:NO];
                td.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
                td.storageMode = MTLStorageModePrivate;
                t[i] = [dev newTextureWithDescriptor:td];
            }
            memset(u, 0, sizeof u);
            u[0] = 1; u[5] = 1; u[15] = 1; u[40] = TAA_ALPHA; u[43] = TAA_GAMMA;
            for (r = 0; r < 20; r++) {
                id<MTLCommandBuffer> cb = [q commandBuffer];
                MTLRenderPassDescriptor *rp = [MTLRenderPassDescriptor renderPassDescriptor];
                id<MTLRenderCommandEncoder> e;
                rp.colorAttachments[0].texture = t[4];
                rp.colorAttachments[1].texture = t[2];
                rp.colorAttachments[0].loadAction = rp.colorAttachments[1].loadAction = MTLLoadActionDontCare;
                e = [cb renderCommandEncoderWithDescriptor:rp];
                [e setRenderPipelineState:ps];
                [e setFragmentBytes:u length:sizeof u atIndex:0];
                [e setFragmentTexture:t[0] atIndex:0];
                [e setFragmentTexture:t[1] atIndex:1];
                [e setFragmentTexture:t[2] atIndex:2];
                [e setFragmentTexture:t[3] atIndex:3];
                [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
                [e endEncoding];
                [cb commit];
                [cb waitUntilCompleted];
                if (r > 2) best = fmin(best, (cb.GPUEndTime - cb.GPUStartTime) * 1000.0);
            }
            printf("INFO  resolve at 1920x1080 on %s: %.3f ms GPU (best of 17); history 2 x (8 + 4) bytes a pixel = %.1f MB\n",
                   [[dev name] UTF8String], best, 2.0 * 12 * 1920 * 1080 / 1048576.0);
        }
        /* (4) MetalFX */
        {
            int sup = brr_taa_metalfx_supported(dev);
            printf("INFO  MetalFX temporal scaler on %s: %s\n", [[dev name] UTF8String], sup ? "supported" : "not supported (the own TAA is used)");
            if (sup) {
                id fx = brr_taa_metalfx_make(dev, 858, 483, 1280, 720);
                id fx1 = brr_taa_metalfx_make(dev, 1280, 720, 1280, 720);
                check(fx != nil && fx1 != nil, "MetalFX scaler made (0.67 internal scale, and 1:1)", "%.0f %.0f", fx != nil, fx1 != nil);
                metalfx_upscale(dev);
            }
        }
        printf("%s\n", fails ? "TAA: FAIL" : "TAA: all pass");
        return fails != 0;
    }
}
