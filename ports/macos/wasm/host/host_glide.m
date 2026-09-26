/* host_glide.m -- glide2x.dll on Metal (port code).
 *
 * The 38 Glide 2 entry points BRGlide.dll imports, drawn with Metal into a
 * 640x480 colour + depth target that grBufferSwap scales into the window.
 *
 *   geometry     Glide vertices are already in screen space: the vertex
 *                shader only maps pixels to NDC. Colour and the texture
 *                coordinates are interpolated linearly in screen space
 *                (center_no_perspective), as the Voodoo iterates them; the
 *                fragment shader divides sow/tow by oow per pixel.
 *   combine      the texture, colour and alpha combine units
 *                (func/factor/local/other/invert) in the fragment shader.
 *   depth        W-buffer (1/oow) or Z-buffer (ooz) written per fragment,
 *                with Glide's eight compare functions.
 *   fog          table fog on Glide's pseudo-log w scale.
 *   blend        Glide's blend factors -> Metal pipeline states (cached).
 *   textures     emulated TMU memory; the texture a grTexSource selects is
 *                decoded from it to RGBA8, re-decoded when downloads
 *                overwrite it.
 *   clip / cull  scissor rect / screen-space winding.
 *   LFB writes   written into the target between passes.
 */
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>
#include "host.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define W 640
#define H 480

CAMetalLayer *happ_metal_layer(void);   /* host_app.m; nil when headless */

typedef struct { float x, y, ooz, oow, r, g, b, a, sow, tow; } gv;
typedef struct {
    int cc_func, cc_fact, cc_local, cc_other, cc_inv;
    int ac_func, ac_fact, ac_local, ac_other, ac_inv;
    int tc_rfunc, tc_rfact, tc_afunc, tc_afact, tc_rinv, tc_ainv;
    int at_fn, at_ref, fogmode, dmode, use_tex, clear;
    float clear_depth, pad0;
    float cconst[4], fogcolor[4], clearcol[4];
    float su, sv, pad1, pad2;
    float fogtab[64];
} gu;

static const char *SHADER =
"#include <metal_stdlib>\n"
"using namespace metal;\n"
"struct GV { float x, y, ooz, oow, r, g, b, a, sow, tow; };\n"
"struct GU { int cc_func, cc_fact, cc_local, cc_other, cc_inv;\n"
"  int ac_func, ac_fact, ac_local, ac_other, ac_inv;\n"
"  int tc_rfunc, tc_rfact, tc_afunc, tc_afact, tc_rinv, tc_ainv;\n"
"  int at_fn, at_ref, fogmode, dmode, use_tex, clear;\n"
"  float clear_depth, pad0; float4 cconst, fogcolor, clearcol; float su, sv, pad1, pad2;\n"
"  float fogtab[64]; };\n"
"struct VO { float4 pos [[position]];\n"
"  float4 col [[center_no_perspective]]; float ooz [[center_no_perspective]];\n"
"  float oow [[center_no_perspective]]; float sow [[center_no_perspective]];\n"
"  float tow [[center_no_perspective]]; };\n"
"vertex VO vs(uint vid [[vertex_id]], const device GV *v [[buffer(0)]]) {\n"
"  GV g = v[vid]; VO o;\n"
"  o.pos = float4(g.x / 320.0 - 1.0, 1.0 - g.y / 240.0, 0.5, 1.0);\n"
"  o.col = float4(g.r, g.g, g.b, g.a); o.ooz = g.ooz; o.oow = g.oow; o.sow = g.sow; o.tow = g.tow;\n"
"  return o; }\n"
"float comb1(int f, float fac, float l, float la, float o) {\n"
"  switch (f) { case 0: return 0; case 1: return l; case 2: return la;\n"
"  case 3: return fac * o; case 4: return fac * o + l; case 5: return fac * o + la;\n"
"  case 6: return fac * (o - l); case 7: return fac * (o - l) + l; case 8: return fac * (o - l) + la;\n"
"  case 9: return -fac * l + l; case 16: return -fac * l + la; default: return l; } }\n"
"float factor(int f, float4 loc, float4 oth, float ta, float lcomp) {\n"
"  float v; switch (f & 7) { case 0: v = 0; break; case 1: v = lcomp; break;\n"
"  case 2: v = oth.a; break; case 3: v = loc.a; break; case 4: v = ta; break; default: v = 0; }\n"
"  if (f >= 8) v = 255.0 - v; return v / 255.0; }\n"
"bool cmpf(int f, float a, float b) {\n"
"  switch (f) { case 0: return false; case 1: return a < b; case 2: return a == b;\n"
"  case 3: return a <= b; case 4: return a > b; case 5: return a != b; case 6: return a >= b;\n"
"  default: return true; } }\n"
"float fogof(constant GU &u, float w) {\n"
"  if (w <= 1.0) return u.fogtab[0]; float prev = 0, pw = 1;\n"
"  for (int i = 0; i < 64; i++) { float tw = exp2(3.0 + float(i >> 2)) / float(8 - (i & 3));\n"
"    if (w <= tw) return prev + (u.fogtab[i] - prev) * (w - pw) / (tw - pw);\n"
"    prev = u.fogtab[i]; pw = tw; }\n"
"  return u.fogtab[63]; }\n"
"struct FO { float4 c [[color(0)]]; float d [[depth(any)]]; };\n"
"fragment FO fs(VO in [[stage_in]], constant GU &u [[buffer(0)]],\n"
"               texture2d<float> tex [[texture(0)]], sampler smp [[sampler(0)]]) {\n"
"  FO o;\n"
"  if (u.clear) { o.c = u.clearcol / 255.0; o.d = u.clear_depth; return o; }\n"
"  float depth = 0;\n"
"  if (u.dmode == 2 || u.dmode == 4) depth = in.oow != 0 ? (1.0 / in.oow) / 65536.0 : 1.0;\n"
"  else if (u.dmode) depth = in.ooz / 65536.0;\n"
"  o.d = clamp(depth, 0.0, 1.0);\n"
"  float4 it = in.col; float4 t = float4(255);\n"
"  if (u.use_tex && in.oow != 0) {\n"
"    float2 st = float2(in.sow, in.tow) / in.oow;\n"
"    float4 raw = tex.sample(smp, st * float2(u.su, u.sv)) * 255.0;\n"
"    float3 rc; for (int k = 0; k < 3; k++) {\n"
"      rc[k] = comb1(u.tc_rfunc, factor(u.tc_rfact, raw, float4(0), raw.a, 0), raw[k], raw.a, 0);\n"
"      if (u.tc_rinv) rc[k] = 255 - rc[k]; }\n"
"    float ra = comb1(u.tc_afunc, factor(u.tc_afact, raw, float4(0), raw.a, raw.a), raw.a, raw.a, 0);\n"
"    if (u.tc_ainv) ra = 255 - ra;\n"
"    t = clamp(float4(rc, ra), 0.0, 255.0); }\n"
"  float4 cc = u.cconst, loc, oth, outc;\n"
"  loc.rgb = u.cc_local == 1 ? cc.rgb : it.rgb;\n"
"  oth.rgb = u.cc_other == 1 ? t.rgb : (u.cc_other == 2 ? cc.rgb : it.rgb);\n"
"  loc.a = u.ac_local == 1 ? cc.a : it.a;\n"
"  oth.a = u.ac_other == 1 ? t.a : (u.ac_other == 2 ? cc.a : it.a);\n"
"  for (int k = 0; k < 3; k++) {\n"
"    float f = factor(u.cc_fact, loc, oth, t.a, loc[k]);\n"
"    outc[k] = comb1(u.cc_func, f, loc[k], loc.a, oth[k]);\n"
"    if (u.cc_inv) outc[k] = 255 - outc[k]; }\n"
"  float fa = factor(u.ac_fact, loc, oth, t.a, loc.a);\n"
"  outc.a = comb1(u.ac_func, fa, loc.a, loc.a, oth.a);\n"
"  if (u.ac_inv) outc.a = 255 - outc.a;\n"
"  outc = clamp(outc, 0.0, 255.0);\n"
"  if (!cmpf(u.at_fn, floor(outc.a), float(u.at_ref))) discard_fragment();\n"
"  if (u.fogmode & 1) {\n"
"    float w = in.oow != 0 ? 1.0 / in.oow : 65535.0;\n"
"    float k = fogof(u, w) / 255.0;\n"
"    outc.rgb = mix(outc.rgb, u.fogcolor.rgb, k); }\n"
"  o.c = outc / 255.0; return o; }\n"
"struct PO { float4 pos [[position]]; float2 uv; };\n"
"vertex PO pvs(uint vid [[vertex_id]]) {\n"
"  float2 p = float2((vid << 1) & 2, vid & 2); PO o;\n"
"  o.pos = float4(p * 2.0 - 1.0, 0, 1); o.uv = float2(p.x, 1.0 - p.y); return o; }\n"
"fragment float4 pfs(PO in [[stage_in]], texture2d<float> t [[texture(0)]]) {\n"
"  constexpr sampler s(filter::linear); return float4(t.sample(s, in.uv).rgb, 1); }\n";

static id<MTLDevice> g_dev;
static id<MTLCommandQueue> g_q;
static id<MTLLibrary> g_lib;
static id<MTLTexture> g_color, g_depth, g_white;
static id<MTLRenderPipelineState> g_present;
static id<MTLCommandBuffer> g_cb;
static id<MTLRenderCommandEncoder> g_enc;
static id<MTLBuffer> g_vbuf[3];
static int g_vbi;
static size_t g_voff;
static dispatch_semaphore_t g_inflight;
static NSMutableDictionary *g_pipes, *g_dss, *g_samplers;
static gu U;

static struct {
    int ab_rs, ab_rd, ab_as, ab_ad;
    int cull, dfunc, dmask, filter, clamp_s, clamp_t;
    int cx0, cy0, cx1, cy1;
    u32 tex_start, tex_large, tex_aspect, tex_fmt, tex_small;
    int colfmt, origin_ll;
} S;

static u8 g_tmem[4 << 20];
typedef struct { u32 start, end, fmt, large, aspect; __unsafe_unretained id<MTLTexture> t; } texent;
static NSMutableArray *g_texobjs;     /* keeps the textures alive */
static texent g_tex[512];
static int g_ntex;

static const int LODW[9] = { 256, 128, 64, 32, 16, 8, 4, 2, 1 };
static const int ASPX[7] = { 8, 4, 2, 1, 1, 1, 1 };
static const int ASPY[7] = { 1, 1, 1, 1, 2, 4, 8 };
static void lod_dims(int lod, int aspect, int *w, int *h)
{
    int s = (lod >= 0 && lod < 9) ? LODW[lod] : 1;
    int ax = (aspect >= 0 && aspect < 7) ? ASPX[aspect] : 1;
    int ay = (aspect >= 0 && aspect < 7) ? ASPY[aspect] : 1;
    *w = ax >= ay ? s : (s / ay > 0 ? s / ay : 1);
    *h = ay >= ax ? s : (s / ax > 0 ? s / ax : 1);
}
static int fmt_bpp(int f) { return f >= 8 ? 2 : 1; }
static u32 tex_bytes(int small, int large, int aspect, int fmt)
{
    u32 tot = 0;
    int l;
    for (l = large; l <= small; l++) {
        int w, h;
        lod_dims(l, aspect, &w, &h);
        tot += (u32)(w * h * fmt_bpp(fmt));
    }
    return (tot + 7) & ~7u;
}
static u32 to_argb(u32 c)
{
    switch (S.colfmt) {
    case 1: return (c & 0xFF00FF00u) | ((c & 0xFF) << 16) | ((c >> 16) & 0xFF);
    case 2: return (c >> 8) | (c << 24);
    case 3: return ((c & 0xFF) << 24) | ((c & 0xFF00) << 8) | ((c >> 8) & 0xFF00) | (c >> 24);
    default: return c;
    }
}
static void argb4(u32 c, float *o)
{
    o[0] = (float)((c >> 16) & 255); o[1] = (float)((c >> 8) & 255);
    o[2] = (float)(c & 255); o[3] = (float)(c >> 24);
}

/* ------------------------------------------------------------ device */
static void gl_setup(void)
{
    NSError *err = nil;
    MTLTextureDescriptor *td;
    MTLRenderPipelineDescriptor *pd;
    int i;
    if (g_dev) return;
    g_dev = MTLCreateSystemDefaultDevice();
    if (!g_dev) { fprintf(stderr, "no Metal device\n"); exit(1); }
    g_q = [g_dev newCommandQueue];
    g_lib = [g_dev newLibraryWithSource:[NSString stringWithUTF8String:SHADER] options:nil error:&err];
    if (!g_lib) { fprintf(stderr, "Glide shader: %s\n", err.localizedDescription.UTF8String); exit(1); }
    td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                            width:W height:H mipmapped:NO];
    td.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    td.storageMode = MTLStorageModeShared;
    g_color = [g_dev newTextureWithDescriptor:td];
    td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                                            width:W height:H mipmapped:NO];
    td.usage = MTLTextureUsageRenderTarget;
    td.storageMode = MTLStorageModePrivate;
    g_depth = [g_dev newTextureWithDescriptor:td];
    td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                            width:1 height:1 mipmapped:NO];
    g_white = [g_dev newTextureWithDescriptor:td];
    { u32 w = 0xFFFFFFFFu; [g_white replaceRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0 withBytes:&w bytesPerRow:4]; }
    for (i = 0; i < 3; i++)
        g_vbuf[i] = [g_dev newBufferWithLength:(16u << 20) options:MTLResourceStorageModeShared];
    g_inflight = dispatch_semaphore_create(3);
    g_pipes = [NSMutableDictionary new];
    g_dss = [NSMutableDictionary new];
    g_samplers = [NSMutableDictionary new];
    g_texobjs = [NSMutableArray new];
    pd = [MTLRenderPipelineDescriptor new];
    pd.vertexFunction = [g_lib newFunctionWithName:@"pvs"];
    pd.fragmentFunction = [g_lib newFunctionWithName:@"pfs"];
    pd.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
    g_present = [g_dev newRenderPipelineStateWithDescriptor:pd error:&err];
    if (!g_present) { fprintf(stderr, "present pipeline: %s\n", err.localizedDescription.UTF8String); exit(1); }
    {
        CAMetalLayer *l = happ_metal_layer();
        if (l) { l.device = g_dev; l.pixelFormat = MTLPixelFormatBGRA8Unorm; l.framebufferOnly = YES; }
    }
}

static MTLBlendFactor bf(int f, int isdst)
{
    switch (f) {
    case 0: return MTLBlendFactorZero;
    case 1: return MTLBlendFactorSourceAlpha;
    case 2: return isdst ? MTLBlendFactorSourceColor : MTLBlendFactorDestinationColor;
    case 3: return MTLBlendFactorDestinationAlpha;
    case 4: return MTLBlendFactorOne;
    case 5: return MTLBlendFactorOneMinusSourceAlpha;
    case 6: return isdst ? MTLBlendFactorOneMinusSourceColor : MTLBlendFactorOneMinusDestinationColor;
    case 7: return MTLBlendFactorOneMinusDestinationAlpha;
    case 15: return MTLBlendFactorSourceAlphaSaturated;
    default: return MTLBlendFactorOne;
    }
}
static id<MTLRenderPipelineState> gpipe(int rs, int rd, int as, int ad, int nocolor)
{
    NSNumber *k = @((rs << 24) | (rd << 16) | (as << 8) | ad | (nocolor << 30));
    id<MTLRenderPipelineState> p = g_pipes[k];
    if (!p) {
        NSError *err = nil;
        MTLRenderPipelineDescriptor *d = [MTLRenderPipelineDescriptor new];
        MTLRenderPipelineColorAttachmentDescriptor *c;
        d.vertexFunction = [g_lib newFunctionWithName:@"vs"];
        d.fragmentFunction = [g_lib newFunctionWithName:@"fs"];
        d.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
        c = d.colorAttachments[0];
        c.pixelFormat = MTLPixelFormatRGBA8Unorm;
        if (!(rs == 4 && rd == 0 && as == 4 && ad == 0)) {
            c.blendingEnabled = YES;
            c.sourceRGBBlendFactor = bf(rs, 0);
            c.destinationRGBBlendFactor = bf(rd, 1);
            c.sourceAlphaBlendFactor = bf(as, 0);
            c.destinationAlphaBlendFactor = bf(ad, 1);
        }
        p = [g_dev newRenderPipelineStateWithDescriptor:d error:&err];
        if (!p) { fprintf(stderr, "Glide pipeline: %s\n", err.localizedDescription.UTF8String); exit(1); }
        g_pipes[k] = p;
    }
    return p;
}
static id<MTLDepthStencilState> dss(int enabled, int fn, int mask)
{
    NSNumber *k = @((enabled << 8) | (fn << 1) | mask);
    id<MTLDepthStencilState> s = g_dss[k];
    if (!s) {
        static const MTLCompareFunction CF[8] = {
            MTLCompareFunctionNever, MTLCompareFunctionLess, MTLCompareFunctionEqual,
            MTLCompareFunctionLessEqual, MTLCompareFunctionGreater, MTLCompareFunctionNotEqual,
            MTLCompareFunctionGreaterEqual, MTLCompareFunctionAlways };
        MTLDepthStencilDescriptor *d = [MTLDepthStencilDescriptor new];
        d.depthCompareFunction = enabled ? CF[fn & 7] : MTLCompareFunctionAlways;
        d.depthWriteEnabled = enabled && mask;
        s = [g_dev newDepthStencilStateWithDescriptor:d];
        g_dss[k] = s;
    }
    return s;
}
static id<MTLSamplerState> samp(void)
{
    NSNumber *k = @((S.filter << 2) | (S.clamp_s << 1) | S.clamp_t);
    id<MTLSamplerState> s = g_samplers[k];
    if (!s) {
        MTLSamplerDescriptor *d = [MTLSamplerDescriptor new];
        d.minFilter = d.magFilter = S.filter ? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest;
        d.sAddressMode = S.clamp_s ? MTLSamplerAddressModeClampToEdge : MTLSamplerAddressModeRepeat;
        d.tAddressMode = S.clamp_t ? MTLSamplerAddressModeClampToEdge : MTLSamplerAddressModeRepeat;
        s = [g_dev newSamplerStateWithDescriptor:d];
        g_samplers[k] = s;
    }
    return s;
}

static void begin_pass(void)
{
    MTLRenderPassDescriptor *rp;
    if (g_enc) return;
    gl_setup();
    if (!g_cb) {
        dispatch_semaphore_wait(g_inflight, DISPATCH_TIME_FOREVER);
        g_cb = [g_q commandBuffer];
        g_vbi = (g_vbi + 1) % 3;
        g_voff = 0;
        {
            dispatch_semaphore_t sem = g_inflight;
            [g_cb addCompletedHandler:^(id<MTLCommandBuffer> b) { (void)b; dispatch_semaphore_signal(sem); }];
        }
    }
    rp = [MTLRenderPassDescriptor renderPassDescriptor];
    rp.colorAttachments[0].texture = g_color;
    rp.colorAttachments[0].loadAction = MTLLoadActionLoad;
    rp.colorAttachments[0].storeAction = MTLStoreActionStore;
    rp.depthAttachment.texture = g_depth;
    rp.depthAttachment.loadAction = MTLLoadActionLoad;
    rp.depthAttachment.storeAction = MTLStoreActionStore;
    g_enc = [g_cb renderCommandEncoderWithDescriptor:rp];
    [g_enc setViewport:(MTLViewport){ 0, 0, W, H, 0, 1 }];
}
static void end_pass(void)
{
    if (g_enc) { [g_enc endEncoding]; g_enc = nil; }
}
/* finish all GPU work: needed before the CPU touches the target */
static void flush_wait(void)
{
    end_pass();
    if (g_cb) { [g_cb commit]; [g_cb waitUntilCompleted]; g_cb = nil; }
}

static void set_scissor(void)
{
    int x0 = S.cx0 < 0 ? 0 : S.cx0, y0 = S.cy0 < 0 ? 0 : S.cy0;
    int x1 = S.cx1 > W ? W : S.cx1, y1 = S.cy1 > H ? H : S.cy1;
    if (x1 <= x0 || y1 <= y0) { x0 = y0 = 0; x1 = y1 = 1; }
    if (S.origin_ll) { int t = H - y1; y1 = H - y0; y0 = t; }
    [g_enc setScissorRect:(MTLScissorRect){ (NSUInteger)x0, (NSUInteger)y0,
                                            (NSUInteger)(x1 - x0), (NSUInteger)(y1 - y0) }];
}

/* ---------------------------------------------------------- textures */
static void decode(const u8 *src, int fmt, int w, int h, u32 *out)
{
    int i;
    for (i = 0; i < w * h; i++) {
        u32 r, g, b, a;
        if (fmt >= 8) {
            u16 x = (u16)(src[2 * i] | src[2 * i + 1] << 8);
            switch (fmt) {
            case 10: r = (x >> 11) * 255 / 31; g = ((x >> 5) & 63) * 255 / 63; b = (x & 31) * 255 / 31; a = 255; break;
            case 11: r = ((x >> 10) & 31) * 255 / 31; g = ((x >> 5) & 31) * 255 / 31; b = (x & 31) * 255 / 31; a = x & 0x8000 ? 255 : 0; break;
            case 12: r = ((x >> 8) & 15) * 17; g = ((x >> 4) & 15) * 17; b = (x & 15) * 17; a = ((x >> 12) & 15) * 17; break;
            case 13: r = g = b = x & 255; a = x >> 8; break;
            default: r = ((x >> 5) & 7) * 36; g = ((x >> 2) & 7) * 36; b = (x & 3) * 85; a = x >> 8; break;
            }
        } else {
            u8 x = src[i];
            switch (fmt) {
            case 0: r = (x >> 5) * 36; g = ((x >> 2) & 7) * 36; b = (x & 3) * 85; a = 255; break;
            case 2: r = g = b = 255; a = x; break;
            case 3: r = g = b = x; a = 255; break;
            case 4: r = g = b = (x & 15) * 17; a = (x >> 4) * 17; break;
            default: r = g = b = x; a = 255; break;
            }
        }
        out[i] = r | g << 8 | b << 16 | a << 24;
    }
}
static id<MTLTexture> cur_texture(void)
{
    int i, w, h;
    u32 n;
    MTLTextureDescriptor *td;
    id<MTLTexture> t;
    u32 *px;
    for (i = 0; i < g_ntex; i++)
        if (g_tex[i].t && g_tex[i].start == S.tex_start && g_tex[i].fmt == S.tex_fmt &&
            g_tex[i].large == S.tex_large && g_tex[i].aspect == S.tex_aspect)
            return g_tex[i].t;
    lod_dims((int)S.tex_large, (int)S.tex_aspect, &w, &h);
    n = (u32)(w * h * fmt_bpp((int)S.tex_fmt));
    if (S.tex_start + n > sizeof g_tmem) return g_white;
    td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                            width:(NSUInteger)w height:(NSUInteger)h mipmapped:NO];
    t = [g_dev newTextureWithDescriptor:td];
    px = malloc((size_t)(w * h * 4));
    decode(g_tmem + S.tex_start, (int)S.tex_fmt, w, h, px);
    [t replaceRegion:MTLRegionMake2D(0, 0, (NSUInteger)w, (NSUInteger)h) mipmapLevel:0 withBytes:px bytesPerRow:(NSUInteger)w * 4];
    free(px);
    for (i = 0; i < g_ntex && g_tex[i].t; i++) ;
    if (i == 512) { i = rand() % 512; [g_texobjs removeObject:g_tex[i].t]; }
    if (i == g_ntex) g_ntex++;
    [g_texobjs addObject:t];
    g_tex[i] = (texent){ S.tex_start, S.tex_start + n, S.tex_fmt, S.tex_large, S.tex_aspect, t };
    return t;
}

/* ------------------------------------------------------------- API */
void h_grGlideInit(void) {}
void h_grGlideShutdown(void) {}
u32 h_grSstQueryHardware(u32 p)
{
    u32 i;
    memset(W_P(p), 0, 0x60);
    HW32(p + 0, 1); HW32(p + 4, 0); HW32(p + 8, 4); HW32(p + 12, 2); HW32(p + 16, 1);
    for (i = 0; i < 3; i++) { HW32(p + 24 + i * 8, 1); HW32(p + 28 + i * 8, 4); }
    return 1;
}
void h_grSstSelect(u32 w) { (void)w; }
u32 h_grSstWinOpen(u32 hwnd, u32 res, u32 ref, u32 cfmt, u32 org, u32 ncol, u32 naux)
{
    (void)hwnd; (void)ref; (void)ncol; (void)naux;
    gl_setup();
    S.colfmt = (int)cfmt;
    S.origin_ll = org == 1;
    S.cx0 = 0; S.cy0 = 0; S.cx1 = W; S.cy1 = H;
    S.ab_rs = 4; S.ab_rd = 0; S.ab_as = 4; S.ab_ad = 0; S.dmask = 1; S.dfunc = 7;
    U.at_fn = 7; U.cc_func = 1; U.ac_func = 1; U.tc_rfunc = 1; U.tc_afunc = 1;
    HLOG("grSstWinOpen(res=%u colfmt=%u origin=%u)\n", res, cfmt, org);
    return 1;
}
void h_grSstWinClose(void) { flush_wait(); }
u32 h_grTexMinAddress(u32 t) { (void)t; return 0; }
u32 h_grTexMaxAddress(u32 t) { (void)t; return (4u << 20) - 0x20000u; }
u32 h_grTexCalcMemRequired(u32 s, u32 l, u32 a, u32 f) { return tex_bytes((int)s, (int)l, (int)a, (int)f); }
u32 h_grTexTextureMemRequired(u32 eo, u32 info)
{
    (void)eo;
    return tex_bytes((int)H32(info), (int)H32(info + 4), (int)H32(info + 8), (int)H32(info + 12));
}
void h_grTexDownloadMipMap(u32 tmu, u32 start, u32 eo, u32 info)
{
    u32 n = tex_bytes((int)H32(info), (int)H32(info + 4), (int)H32(info + 8), (int)H32(info + 12));
    u32 data = H32(info + 16), end;
    int i;
    (void)tmu; (void)eo;
    if (start >= sizeof g_tmem) return;
    if (start + n > sizeof g_tmem) n = (u32)sizeof g_tmem - start;
    if (data) memcpy(g_tmem + start, W_P(data), n);
    end = start + n;
    /* anything decoded from the bytes just overwritten is stale */
    for (i = 0; i < g_ntex; i++)
        if (g_tex[i].t && g_tex[i].start < end && start < g_tex[i].end) {
            [g_texobjs removeObject:g_tex[i].t];
            g_tex[i].t = nil;
        }
}
void h_grTexSource(u32 tmu, u32 start, u32 eo, u32 info)
{
    (void)tmu; (void)eo;
    S.tex_start = start; S.tex_small = H32(info); S.tex_large = H32(info + 4);
    S.tex_aspect = H32(info + 8); S.tex_fmt = H32(info + 12);
}
void h_grTexClampMode(u32 tmu, u32 s, u32 t) { (void)tmu; S.clamp_s = (int)s; S.clamp_t = (int)t; }
void h_grTexFilterMode(u32 tmu, u32 mn, u32 mg) { (void)tmu; (void)mn; S.filter = (int)mg; }
void h_grTexMipMapMode(u32 tmu, u32 m, u32 b) { (void)tmu; (void)m; (void)b; }
void h_grTexLodBiasValue(u32 tmu, f32 b) { (void)tmu; (void)b; }
void h_grTexCombine(u32 tmu, u32 rf, u32 rfa, u32 af, u32 afa, u32 ri, u32 ai)
{
    (void)tmu;
    U.tc_rfunc = (int)rf; U.tc_rfact = (int)rfa; U.tc_afunc = (int)af; U.tc_afact = (int)afa;
    U.tc_rinv = (int)ri; U.tc_ainv = (int)ai;
}
void h_grColorCombine(u32 f, u32 fa, u32 l, u32 o, u32 inv)
{ U.cc_func = (int)f; U.cc_fact = (int)fa; U.cc_local = (int)l; U.cc_other = (int)o; U.cc_inv = (int)inv; }
void h_grAlphaCombine(u32 f, u32 fa, u32 l, u32 o, u32 inv)
{ U.ac_func = (int)f; U.ac_fact = (int)fa; U.ac_local = (int)l; U.ac_other = (int)o; U.ac_inv = (int)inv; }
void h_grAlphaBlendFunction(u32 rs, u32 rd, u32 as, u32 ad)
{ S.ab_rs = (int)rs; S.ab_rd = (int)rd; S.ab_as = (int)as; S.ab_ad = (int)ad; }
void h_grAlphaTestFunction(u32 f) { U.at_fn = (int)f; }
void h_grAlphaTestReferenceValue(u32 v) { U.at_ref = (int)(v & 0xFF); }
void h_grConstantColorValue(u32 c) { argb4(to_argb(c), U.cconst); }
void h_grCullMode(u32 m) { S.cull = (int)m; }
void h_grDepthBufferMode(u32 m) { U.dmode = (int)m; }
void h_grDepthBufferFunction(u32 f) { S.dfunc = (int)f; }
void h_grDepthMask(u32 m) { S.dmask = (int)m; }
void h_grFogMode(u32 m) { U.fogmode = (int)m; }
void h_grFogColorValue(u32 c) { argb4(to_argb(c), U.fogcolor); }
void h_grFogTable(u32 p) { int i; for (i = 0; i < 64; i++) U.fogtab[i] = ((u8 *)W_P(p))[i]; }
void h_guFogGenerateLinear(u32 p, f32 nearw, f32 farw)
{
    int i;
    for (i = 0; i < 64; i++) {
        float w = powf(2.0f, 3.0f + (float)(i >> 2)) / (float)(8 - (i & 3));
        float f = farw == nearw ? 0.0f : (w - nearw) / (farw - nearw);
        f = f < 0 ? 0 : f > 1 ? 1 : f;
        ((u8 *)W_P(p))[i] = (u8)(f * 255.0f);
    }
}
void h_grClipWindow(u32 x0, u32 y0, u32 x1, u32 y1)
{ S.cx0 = (int)x0; S.cy0 = (int)y0; S.cx1 = (int)x1; S.cy1 = (int)y1; }
u32 h_grBufferNumPending(void) { return 0; }

static void draw(const gv *v, int n, int clear)
{
    size_t bytes = (size_t)n * sizeof(gv);
    id<MTLTexture> t = g_white;
    begin_pass();
    if (g_voff + bytes > (16u << 20)) {
        /* a vertex-heavy frame: flush and start over in the next buffer */
        end_pass();
        [g_cb commit];
        g_cb = nil;
        begin_pass();
    }
    memcpy((u8 *)g_vbuf[g_vbi].contents + g_voff, v, bytes);
    U.clear = clear;
    U.use_tex = !clear && (U.cc_other == 1 || U.ac_other == 1 || (U.cc_fact & 7) == 4 || (U.ac_fact & 7) == 4);
    if (U.use_tex) {
        int tw, th, md;
        t = cur_texture();
        lod_dims((int)S.tex_large, (int)S.tex_aspect, &tw, &th);
        md = tw > th ? tw : th;
        U.su = (float)md / (256.0f * (float)tw);
        U.sv = (float)md / (256.0f * (float)th);
    }
    [g_enc setRenderPipelineState:clear ? gpipe(4, 0, 4, 0, 0) : gpipe(S.ab_rs, S.ab_rd, S.ab_as, S.ab_ad, 0)];
    [g_enc setDepthStencilState:clear ? dss(1, 7, 1) : dss(U.dmode != 0, S.dfunc, S.dmask)];
    set_scissor();
    [g_enc setVertexBuffer:g_vbuf[g_vbi] offset:g_voff atIndex:0];
    [g_enc setFragmentBytes:&U length:sizeof U atIndex:0];
    [g_enc setFragmentTexture:t atIndex:0];
    [g_enc setFragmentSamplerState:samp() atIndex:0];
    [g_enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:(NSUInteger)n];
    g_voff += (bytes + 255) & ~(size_t)255;
}

void h_grBufferClear(u32 color, u32 alpha, u32 depth)
{
    gv q[6];
    float x0 = 0, y0 = 0, x1 = W, y1 = H;
    int i;
    argb4((to_argb(color) & 0x00FFFFFFu) | ((alpha & 0xFF) << 24), U.clearcol);
    U.clear_depth = (float)(depth & 0xFFFF) / 65536.0f;
    memset(q, 0, sizeof q);
    q[0].x = x0; q[0].y = y0; q[1].x = x1; q[1].y = y0; q[2].x = x0; q[2].y = y1;
    q[3].x = x1; q[3].y = y0; q[4].x = x1; q[4].y = y1; q[5].x = x0; q[5].y = y1;
    for (i = 0; i < 6; i++) q[i].oow = 1;
    draw(q, 6, 1);                 /* the scissor keeps it inside the clip window */
}

static int g_nshot;
static void shot(void)
{
    const char *dir = getenv("BR_SHOT_DIR");
    int every = getenv("BR_SHOT_EVERY") ? atoi(getenv("BR_SHOT_EVERY")) : 30;
    char p[1024];
    FILE *f;
    u32 *px;
    int i;
    if (!dir) return;
    if (every < 1) every = 1;
    if (g_nshot++ % every) return;
    flush_wait();
    px = malloc(W * H * 4);
    [g_color getBytes:px bytesPerRow:W * 4 fromRegion:MTLRegionMake2D(0, 0, W, H) mipmapLevel:0];
    snprintf(p, sizeof p, "%s/frame%05d.ppm", dir, g_nshot - 1);
    f = fopen(p, "wb");
    if (f) {
        fprintf(f, "P6\n%d %d\n255\n", W, H);
        for (i = 0; i < W * H; i++) { u8 c[3] = { (u8)px[i], (u8)(px[i] >> 8), (u8)(px[i] >> 16) }; fwrite(c, 1, 3, f); }
        fclose(f);
    }
    free(px);
}

void h_grBufferSwap(u32 interval)
{
    CAMetalLayer *l;
    (void)interval;
    begin_pass();
    end_pass();
    shot();
    l = happ_metal_layer();
    if (!g_cb) begin_pass(), end_pass();
    if (l) {
        id<CAMetalDrawable> d = [l nextDrawable];
        if (d) {
            MTLRenderPassDescriptor *rp = [MTLRenderPassDescriptor renderPassDescriptor];
            id<MTLRenderCommandEncoder> e;
            rp.colorAttachments[0].texture = d.texture;
            rp.colorAttachments[0].loadAction = MTLLoadActionClear;
            rp.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);
            rp.colorAttachments[0].storeAction = MTLStoreActionStore;
            e = [g_cb renderCommandEncoderWithDescriptor:rp];
            {
                double dw = d.texture.width, dh = d.texture.height, s = fmin(dw / W, dh / H);
                [e setViewport:(MTLViewport){ (dw - W * s) / 2, (dh - H * s) / 2, W * s, H * s, 0, 1 }];
            }
            [e setRenderPipelineState:g_present];
            [e setFragmentTexture:g_color atIndex:0];
            [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
            [e endEncoding];
            [g_cb presentDrawable:d];
        }
    }
    [g_cb commit];
    g_cb = nil;
    happ_pump(0);
}

/* LFB write: GR_LFB_SRC_FMT 565 0, 555 1, 1555 2, 888 4, 8888 5 */
u32 h_grLfbWriteRegion(u32 buf, u32 x, u32 y, u32 fmt, u32 w, u32 h, u32 stride, u32 data)
{
    u32 *px = malloc((size_t)w * h * 4), i, j;
    (void)buf;
    gl_setup();
    for (j = 0; j < h; j++) {
        const u8 *s = (const u8 *)W_P(data + j * stride);
        for (i = 0; i < w; i++) {
            u32 r, g, b, a = 255;
            if (fmt == 0) { u16 v = (u16)(s[2 * i] | s[2 * i + 1] << 8); r = (v >> 11) * 255 / 31; g = ((v >> 5) & 63) * 255 / 63; b = (v & 31) * 255 / 31; }
            else if (fmt == 1 || fmt == 2) { u16 v = (u16)(s[2 * i] | s[2 * i + 1] << 8); r = ((v >> 10) & 31) * 255 / 31; g = ((v >> 5) & 31) * 255 / 31; b = (v & 31) * 255 / 31; if (fmt == 2 && !(v & 0x8000)) a = 0; }
            else if (fmt == 4) { b = s[3 * i]; g = s[3 * i + 1]; r = s[3 * i + 2]; }
            else { b = s[4 * i]; g = s[4 * i + 1]; r = s[4 * i + 2]; a = s[4 * i + 3]; }
            px[j * w + i] = r | g << 8 | b << 16 | a << 24;
        }
    }
    flush_wait();
    {
        int x0 = (int)x, y0 = (int)y, ww = (int)w, hh = (int)h;
        if (x0 < W && y0 < H && x0 + ww > 0 && y0 + hh > 0) {
            int cx = x0 < 0 ? -x0 : 0, cy = y0 < 0 ? -y0 : 0;
            int rw = (x0 + ww > W ? W - x0 : ww) - cx, rh = (y0 + hh > H ? H - y0 : hh) - cy;
            [g_color replaceRegion:MTLRegionMake2D((NSUInteger)(x0 + cx), (NSUInteger)(y0 + cy), (NSUInteger)rw, (NSUInteger)rh)
                       mipmapLevel:0 withBytes:px + cy * w + cx bytesPerRow:w * 4];
        }
    }
    free(px);
    return 1;
}

static void load_vtx(u32 p, gv *v)
{
    const float *f = (const float *)W_P(p);
    v->x = f[0]; v->y = f[1];
    v->r = f[3]; v->g = f[4]; v->b = f[5];
    v->ooz = f[6]; v->a = f[7]; v->oow = f[8];
    v->sow = f[9]; v->tow = f[10];
    /* Glide apps snap vertices with a large bias; take it back off */
    if (v->x > 4096.0f) { v->x -= (float)(3 << 18); v->y -= (float)(3 << 18); }
    if (S.origin_ll) v->y = (float)H - v->y;
}
static int culled(const gv *a, const gv *b, const gv *c)
{
    float area = (b->x - a->x) * (c->y - a->y) - (c->x - a->x) * (b->y - a->y);
    if (area == 0) return 1;
    if (S.cull == 1 && area < 0) return 1;
    if (S.cull == 2 && area > 0) return 1;
    return 0;
}
void h_grDrawTriangle(u32 a, u32 b, u32 c)
{
    gv v[3];
    load_vtx(a, &v[0]); load_vtx(b, &v[1]); load_vtx(c, &v[2]);
    if (!culled(&v[0], &v[1], &v[2])) draw(v, 3, 0);
}
/* the game's GrVertex is 0x3C bytes (two TMUs): include/br_imgblit.h */
void h_grDrawPolygonVertexList(u32 n, u32 p)
{
    gv tri[3 * 64], v0, a, b;
    int k = 0;
    u32 i;
    if (n < 3) return;
    load_vtx(p, &v0);
    load_vtx(p + 60, &a);
    for (i = 2; i < n && k < 64; i++) {
        load_vtx(p + 60 * i, &b);
        if (!culled(&v0, &a, &b)) { tri[3 * k] = v0; tri[3 * k + 1] = a; tri[3 * k + 2] = b; k++; }
        a = b;
    }
    if (k) draw(tri, 3 * k, 0);
}
