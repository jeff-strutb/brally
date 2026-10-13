/* brr_metal_taa.m: the lit frame's temporal anti-aliasing (brr_metal_taa.h)
 * and MetalFX's temporal scaler where the device has one.
 *
 * BR_LIGHT_TAA: 0 off, 1 the port's own TAA (the default), 2 MetalFX's
 * temporal scaler (at the target's size for now: input = output), the own
 * TAA where MetalFX is absent (before macOS 13 / iOS 16, or a device it
 * does not support). */
#import <Metal/Metal.h>
#if __has_include(<MetalFX/MetalFX.h>)
#import <MetalFX/MetalFX.h>
#define BRR_HAVE_METALFX 1
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "brr_metal_taa.h"
#include "brr_metal_taa_api.h"

typedef struct TU { float S[16], Fi[16], eye[4], jit[4], k[4]; } TU;

static id<MTLLibrary> s_tlib;
static id<MTLRenderPipelineState> s_tres, s_tmot;
static id<MTLTexture> s_th[2], s_td[2], s_tmv;
static id<MTLBuffer> s_txt[3];
static long s_tw;
static int s_tptr, s_tmode = -1, s_thave;
static unsigned s_tn;
static float s_tjit[2], s_tS[16];
#ifdef BRR_HAVE_METALFX
static id s_fx;                         /* id<MTLFXTemporalScaler> */
#endif

static int s_fxok(void)
{
#ifdef BRR_HAVE_METALFX
    return s_fx != nil;
#else
    return 0;
#endif
}

int brr_taa_mode(void)
{
    if (s_tmode < 0) {
        const char *e = getenv("BR_LIGHT_TAA");
        s_tmode = e ? atoi(e) : 1;
    }
    return s_tmode;
}

int brr_taa_metalfx_supported(id<MTLDevice> dev)
{
#ifdef BRR_HAVE_METALFX
    if (@available(macOS 13.0, iOS 16.0, *))
        return [MTLFXTemporalScalerDescriptor supportsDevice:dev] ? 1 : 0;
#endif
    (void)dev;
    return 0;
}

/* a temporal scaler from in x in to out x out; nil where there is none */
id brr_taa_metalfx_make(id<MTLDevice> dev, int iw, int ih, int ow, int oh)
{
#ifdef BRR_HAVE_METALFX
    if (@available(macOS 13.0, iOS 16.0, *)) {
        MTLFXTemporalScalerDescriptor *d;
        if (!brr_taa_metalfx_supported(dev))
            return nil;
        d = [MTLFXTemporalScalerDescriptor new];
        d.colorTextureFormat = MTLPixelFormatRGBA16Float;
        d.depthTextureFormat = MTLPixelFormatDepth32Float;
        d.motionTextureFormat = MTLPixelFormatRG16Float;
        d.outputTextureFormat = MTLPixelFormatRGBA16Float;
        d.inputWidth = (NSUInteger)iw;
        d.inputHeight = (NSUInteger)ih;
        d.outputWidth = (NSUInteger)ow;
        d.outputHeight = (NSUInteger)oh;
        d.autoExposureEnabled = NO;     /* the HDR is pre-exposed already */
        return [d newTemporalScalerWithDevice:dev];
    }
#endif
    (void)dev; (void)iw; (void)ih; (void)ow; (void)oh;
    return nil;
}

static id<MTLTexture> tex(id<MTLDevice> dev, MTLPixelFormat f, int w, int h)
{
    MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:f width:(NSUInteger)w height:(NSUInteger)h mipmapped:NO];
    td.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
    td.storageMode = MTLStorageModePrivate;
    return [dev newTextureWithDescriptor:td];
}

static int taa_init(id<MTLDevice> dev)
{
    NSError *err = nil;
    MTLRenderPipelineDescriptor *d;
    if (s_tres)
        return 1;
    s_tlib = [dev newLibraryWithSource:@BRR_TAA_SRC options:nil error:&err];
    if (!s_tlib) {
        fprintf(stderr, "brr: taa shaders: %s\n", [[err description] UTF8String]);
        return 0;
    }
    d = [MTLRenderPipelineDescriptor new];
    d.vertexFunction = [s_tlib newFunctionWithName:@"taa_vs"];
    d.fragmentFunction = [s_tlib newFunctionWithName:@"taa_resolve"];
    d.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA16Float;
    d.colorAttachments[1].pixelFormat = MTLPixelFormatR32Float;
    s_tres = [dev newRenderPipelineStateWithDescriptor:d error:&err];
    d = [MTLRenderPipelineDescriptor new];
    d.vertexFunction = [s_tlib newFunctionWithName:@"taa_vs"];
    d.fragmentFunction = [s_tlib newFunctionWithName:@"taa_motion"];
    d.colorAttachments[0].pixelFormat = MTLPixelFormatRG16Float;
    s_tmot = [dev newRenderPipelineStateWithDescriptor:d error:&err];
    return s_tres && s_tmot;
}

id<MTLBuffer> brr_taa_begin(id<MTLDevice> dev, id<MTLBuffer> xt, int slot, int nxf, int tw, int th, float *jit_px)
{
    float ndc[2];
    float *t;
    int i;
    if (!brr_taa_mode() || !taa_init(dev))
        return nil;
    brr_taa_jitter(s_tn, (float)tw, (float)th, s_tjit, ndc);
    if (!s_txt[slot])
        s_txt[slot] = [dev newBufferWithLength:xt.length options:MTLResourceStorageModeShared];
    memcpy([s_txt[slot] contents], [xt contents], (size_t)nxf * 16);
    t = (float *)[s_txt[slot] contents];
    for (i = 0; i < nxf; i++) {
        t[4 * i + 1] += ndc[0];
        t[4 * i + 3] += ndc[1];
    }
    if (jit_px) {
        jit_px[0] = s_tjit[0];
        jit_px[1] = s_tjit[1];
    }
    return s_txt[slot];
}

id<MTLTexture> brr_taa_resolve(id<MTLDevice> dev, id<MTLCommandBuffer> cb, id<MTLTexture> hdr, id<MTLTexture> gpos,
                               id<MTLTexture> depth, const float S[16], const float vp[16], const float eye[3],
                               int ow, int oh, int fx)
{
    TU u;
    float F[16], cen[2] = { 0, 0 }, ctr[3];
    int w = (int)hdr.width, h = (int)hdr.height, k, reset;
    long key = ((long)w * 65536 + h) * 4 + (fx ? 2 : 0) + (ow != w || oh != h);
    MTLRenderPassDescriptor *rp;
    id<MTLRenderCommandEncoder> e;
    if (!brr_taa_mode() || !s_tres)
        return hdr;
    s_tn++;
    if (!s_th[0] || s_tw != key || (int)s_th[0].width != ow || (int)s_th[0].height != oh) {
        /* the output (history) at the target's size, motion and the view
           distances at the input's; MetalFX from input to output */
        for (k = 0; k < 2; k++) {
            s_th[k] = tex(dev, MTLPixelFormatRGBA16Float, ow, oh);
            s_td[k] = tex(dev, MTLPixelFormatR32Float, w, h);
        }
        s_tmv = tex(dev, MTLPixelFormatRG16Float, w, h);
        s_tw = key;
        s_thave = 0;
#ifdef BRR_HAVE_METALFX
        s_fx = brr_taa_mode() == 2 || fx ? brr_taa_metalfx_make(dev, w, h, ow, oh) : nil;
#endif
    }
    if (!s_fxok() && (ow != w || oh != h))
        return hdr;                     /* no scaler for a scaled frame: the caller does not ask for one (brr_taa_scale) */
    memset(&u, 0, sizeof u);
    memcpy(u.S, s_tS, sizeof u.S);
    brr_taa_full(S, vp, F);
    brr_inv4(F, u.Fi);
    for (k = 0; k < 3; k++)
        u.eye[k] = eye[k];
    u.jit[0] = s_tjit[0];
    u.jit[1] = s_tjit[1];
    /* a camera cut: the world point under the screen's centre, a long way down
       the view, moves more than TAA_CUT_PX between the frames */
    brr_taa_ray(u.Fi, 0.5f * (float)w, 0.5f * (float)h, ctr);
    reset = !s_thave || !brr_taa_reproject(s_tS, ctr, 0.0f, cen) ||
            fabsf(cen[0] - 0.5f * (float)w) + fabsf(cen[1] - 0.5f * (float)h) > TAA_CUT_PX;
    u.k[0] = TAA_ALPHA;
    u.k[1] = reset ? 1.0f : 0.0f;
    u.k[2] = 1.0f;
    u.k[3] = TAA_GAMMA;
    memcpy(s_tS, S, sizeof s_tS);
    s_thave = 1;
#ifdef BRR_HAVE_METALFX
    if (s_fx) {
        if (@available(macOS 13.0, iOS 16.0, *)) {
            id<MTLFXTemporalScaler> fx = s_fx;
            rp = [MTLRenderPassDescriptor renderPassDescriptor];
            rp.colorAttachments[0].texture = s_tmv;
            rp.colorAttachments[0].loadAction = MTLLoadActionDontCare;
            rp.colorAttachments[0].storeAction = MTLStoreActionStore;
            e = [cb renderCommandEncoderWithDescriptor:rp];
            [e setRenderPipelineState:s_tmot];
            [e setFragmentBytes:&u length:sizeof u atIndex:0];
            [e setFragmentTexture:gpos atIndex:0];
            [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
            [e endEncoding];
            fx.colorTexture = hdr;
            fx.depthTexture = depth;
            fx.motionTexture = s_tmv;
            fx.outputTexture = s_th[s_tptr];
            fx.jitterOffsetX = -s_tjit[0];
            fx.jitterOffsetY = -s_tjit[1];
            fx.motionVectorScaleX = 1.0f;
            fx.motionVectorScaleY = 1.0f;
            fx.reset = reset ? YES : NO;
            fx.depthReversed = NO;      /* the lit 3D's depth: cleared to 1, less-than (near 0, far 1) */
            [fx encodeToCommandBuffer:cb];
            id<MTLTexture> out = s_th[s_tptr];
            s_tptr ^= 1;
            return out;
        }
    }
#else
    (void)depth;
#endif
    rp = [MTLRenderPassDescriptor renderPassDescriptor];
    rp.colorAttachments[0].texture = s_th[s_tptr];
    rp.colorAttachments[1].texture = s_td[s_tptr];
    for (k = 0; k < 2; k++) {
        rp.colorAttachments[k].loadAction = MTLLoadActionDontCare;
        rp.colorAttachments[k].storeAction = MTLStoreActionStore;
    }
    e = [cb renderCommandEncoderWithDescriptor:rp];
    [e setRenderPipelineState:s_tres];
    [e setFragmentBytes:&u length:sizeof u atIndex:0];
    [e setFragmentTexture:hdr atIndex:0];
    [e setFragmentTexture:s_th[s_tptr ^ 1] atIndex:1];
    [e setFragmentTexture:s_td[s_tptr ^ 1] atIndex:2];
    [e setFragmentTexture:gpos atIndex:3];
    [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    [e endEncoding];
    {
        id<MTLTexture> out = s_th[s_tptr];
        s_tptr ^= 1;
        return out;
    }
}

/* the lit 3D's internal scale for a target tw x th: 1 without MetalFX (or
 * with TAA off); BR_LIGHT_SCALE fixes it (0.5 to 1); otherwise, on a target
 * over 2.5 MP, from 0.67, the dynamic resolution controller's (16.6 ms, 0.5
 * to 1) fed with the last whole frame's GPU time (gpu_ms <= 0: none yet).
 * *up: 1 when the frame goes through MetalFX's scaler (at any scale, so a
 * move to 1.0 keeps the same look) */
float brr_taa_scale(id<MTLDevice> dev, int tw, int th, float gpu_ms, int *up)
{
    static int env = -1, sup = -1;
    static float fixed;
    static BrrDynRes d;
    const char *e;
    *up = 0;
    if (!brr_taa_mode())
        return 1.0f;
    if (sup < 0)
        sup = brr_taa_metalfx_supported(dev);
    if (env < 0) {
        e = getenv("BR_LIGHT_SCALE");
        env = e != NULL;
        fixed = e ? fminf(1.0f, fmaxf(0.5f, (float)atof(e))) : 1.0f;
    }
    if (!sup)
        return 1.0f;
    if (env) {
        *up = fixed < 1.0f || brr_taa_mode() == 2;
        return fixed;
    }
    if ((double)tw * (double)th <= 2.5e6)
        return 1.0f;
    *up = 1;
    if (d.scale <= 0.0f) {
        d.scale = 0.67f;
        d.ms = 16.6f;
    }
    if (gpu_ms > 0.0f)
        brr_taa_dynres(&d, gpu_ms, 16.6f, 0.5f);
    return d.scale;
}

void brr_taa_reset(void)
{
    int k;
    for (k = 0; k < 2; k++)
        s_th[k] = s_td[k] = nil;
    s_tmv = nil;
    s_thave = 0;
#ifdef BRR_HAVE_METALFX
    s_fx = nil;
#endif
}
