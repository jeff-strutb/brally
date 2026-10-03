/* brr_metal.m: the renderer backend on Metal (macOS).
 *
 * The game's frame is drawn at Glide's size into an offscreen colour and
 * depth target, then scaled into the window's CAMetalLayer at present,
 * aspect kept. Glide's model maps onto the GPU directly:
 *
 *   combiners   the fragment shader evaluates grColorCombine, grAlphaCombine
 *               and TMU0's grTexCombine from their own parameters, the same
 *               equations as the software backend (brr_soft.c), plus the
 *               alpha test and table fog
 *   blending    the pipeline's blend factors; the board keeps no destination
 *               alpha, so DST_ALPHA reads as one
 *   depth       a depth-stencil state per (mode, function, mask)
 *   scissor     grClipWindow
 *   texturing   perspective-correct (w = 1/oow), colour screen-linear as on
 *               the Voodoo
 *
 * The shader source is compiled at start-up, so the build needs no Metal
 * toolchain step. */
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>
#import <ImageIO/ImageIO.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "brr.h"

CAMetalLayer *host_macos_metal_layer(void);
void host_macos_layer_fit(int *w, int *h);   /* host_macos.m: drawable = the view in pixels */      /* host_macos.m */
#include "host.h"

/* ---- the shader -------------------------------------------------------------------- */
static NSString *const k_src = @
"#include <metal_stdlib>\n"
"using namespace metal;\n"
"struct V { packed_float2 pos; float z; float oow; packed_float4 col; packed_float2 st; };\n"
"struct U {\n"
"  int cc_fn, cc_factor, cc_local, cc_other, cc_invert;\n"
"  int ac_fn, ac_factor, ac_local, ac_other, ac_invert;\n"
"  int tc_rgb_fn, tc_rgb_factor, tc_alpha_fn, tc_alpha_factor, tc_rgb_invert, tc_alpha_invert;\n"
"  int has_tex, atest_fn, fog_mode, depth_mode;\n"
"  float atest_ref, vw, vh, xf;\n"
"  float4 konst, fog_color;\n"
"  float fog[64];\n"
"};\n"
"struct O { float4 pos [[position]]; float4 col [[center_no_perspective]]; float2 st; float oow [[center_no_perspective]]; float z [[center_no_perspective]]; };\n"
/* the screen map (glide.c): the game's pixels to the target through entry
   u.xf of this frame's table */
"vertex O vs(uint i [[vertex_id]], const device V *v [[buffer(0)]], constant U &u [[buffer(1)]],\n"
"            const device float4 *xt [[buffer(2)]]) {\n"
"  O o; V a = v[i]; float4 T = xt[int(u.xf)];\n"
"  float w = a.oow > 0 ? 1.0 / a.oow : 1.0;\n"
"  float2 ndc = float2(a.pos.x * T.x + T.y, a.pos.y * T.z + T.w);\n"
"  o.pos = float4(ndc * w, clamp(a.z, 0.0, 1.0) * w, w);\n"
"  o.col = clamp(a.col / 255.0, 0.0, 1.0); o.st = a.st; o.oow = a.oow; o.z = a.z;\n"
"  return o;\n"
"}\n"
"static float comb(int fn, float f, float l, float la, float ot, int inv) {\n"
"  float r;\n"
"  switch (fn) {\n"
"  case 0x0: r = 0; break; case 0x1: r = l; break; case 0x2: r = la; break;\n"
"  case 0x3: r = f * ot; break; case 0x4: r = f * ot + l; break; case 0x5: r = f * ot + la; break;\n"
"  case 0x6: r = f * (ot - l); break; case 0x7: r = f * (ot - l) + l; break;\n"
"  case 0x8: r = f * (ot - l) + la; break; case 0x9: r = -f * l + l; break;\n"
"  case 0x10: r = -f * l + la; break; default: r = l; break;\n"
"  }\n"
"  r = clamp(r, 0.0, 1.0);\n"
"  return inv ? 1.0 - r : r;\n"
"}\n"
"static float fac(int k, float l, float la, float oa, float ta) {\n"
"  switch (k) {\n"
"  case 0: return 0; case 1: return l; case 2: return oa; case 3: return la; case 4: return ta;\n"
"  case 5: return 0; case 8: return 1; case 9: return 1 - l; case 10: return 1 - oa;\n"
"  case 11: return 1 - la; case 12: return 1 - ta; case 13: return 1; default: return 0;\n"
"  }\n"
"}\n"
"static bool cmpf(int fn, float a, float b) {\n"
"  switch (fn) { case 0: return false; case 1: return a < b; case 2: return a == b; case 3: return a <= b;\n"
"  case 4: return a > b; case 5: return a != b; case 6: return a >= b; default: return true; }\n"
"}\n"
"static float fogw(int i) { return pow(2.0, 3.0 + float(i >> 2)) / float(8 - (i & 3)); }\n"
"static float fogt(constant U &u, float w) {\n"
"  if (w <= fogw(0)) return u.fog[0];\n"
"  for (int i = 0; i < 63; i++) { float w0 = fogw(i), w1 = fogw(i + 1);\n"
"    if (w < w1) { float k = (w - w0) / (w1 - w0); return u.fog[i] * (1 - k) + u.fog[i + 1] * k; } }\n"
"  return u.fog[63];\n"
"}\n"
"/* The Voodoo's 16-bit W-buffer word: 4-bit exponent, 12-bit mantissa of\n"
"   1/w as a .32 fraction.  Depth is stored and compared at this precision,\n"
"   which is what lets a second pass over the same polygon (the car shadow\n"
"   is drawn with grDepthBufferFunction(EQUAL)) hit every pixel the first\n"
"   one wrote; the Z-buffer keeps ooz's 16 integer bits. */\n"
"static uint wfloat(float oow) {\n"
"  if (oow >= 1.0) return 0;\n"
"  if (oow <= 0.0) return 0xFFFF;\n"
"  uint t = uint(min(oow * 4294967296.0, 4294967040.0));\n"
"  if (t == 0) return 0xFFFF;\n"
"  int e = int(clz(t));\n"
"  uint m = e <= 19 ? (~t >> uint(19 - e)) : (~t << uint(e - 19));\n"
"  uint w = (uint(e) << 12) | (m & 0xFFF);\n"
"  return w < 0xFFFF ? w + 1 : w;\n"
"}\n"
"struct FO { float4 c [[color(0)]]; float d [[depth(any)]]; };\n"
"fragment FO fs(O in [[stage_in]], constant U &u [[buffer(0)]], texture2d<float> t [[texture(0)]], sampler sm [[sampler(0)]]) {\n"
"  float4 tex = float4(0);\n"
"  if (u.has_tex) {\n"
"    float4 tx = t.sample(sm, in.st);\n"
"    tex.r = comb(u.tc_rgb_fn, fac(u.tc_rgb_factor, tx.r, tx.a, 0, tx.a), tx.r, tx.a, 0, u.tc_rgb_invert);\n"
"    tex.g = comb(u.tc_rgb_fn, fac(u.tc_rgb_factor, tx.g, tx.a, 0, tx.a), tx.g, tx.a, 0, u.tc_rgb_invert);\n"
"    tex.b = comb(u.tc_rgb_fn, fac(u.tc_rgb_factor, tx.b, tx.a, 0, tx.a), tx.b, tx.a, 0, u.tc_rgb_invert);\n"
"    tex.a = comb(u.tc_alpha_fn, fac(u.tc_alpha_factor, tx.a, tx.a, 0, tx.a), tx.a, tx.a, 0, u.tc_alpha_invert);\n"
"  }\n"
"  float4 it = in.col, k = u.konst;\n"
"  float4 cl = u.cc_local == 1 ? k : it;\n"
"  float4 co = u.cc_other == 1 ? tex : (u.cc_other == 2 ? k : it);\n"
"  float al = u.ac_local == 1 ? k.a : it.a;\n"
"  float ao = u.ac_other == 1 ? tex.a : (u.ac_other == 2 ? k.a : it.a);\n"
"  float4 o;\n"
"  o.a = comb(u.ac_fn, fac(u.ac_factor, al, al, ao, tex.a), al, al, ao, u.ac_invert);\n"
"  o.r = comb(u.cc_fn, fac(u.cc_factor, cl.r, al, co.a, tex.a), cl.r, al, co.r, u.cc_invert);\n"
"  o.g = comb(u.cc_fn, fac(u.cc_factor, cl.g, al, co.a, tex.a), cl.g, al, co.g, u.cc_invert);\n"
"  o.b = comb(u.cc_fn, fac(u.cc_factor, cl.b, al, co.a, tex.a), cl.b, al, co.b, u.cc_invert);\n"
"  if (u.atest_fn != 7 && !cmpf(u.atest_fn, floor(o.a * 255.0 + 0.5), u.atest_ref)) discard_fragment();\n"
"  float f = 0;\n"
"  if (u.fog_mode == 1) f = it.a;\n"
"  else if (u.fog_mode == 2) f = fogt(u, in.oow > 0 ? 1.0 / in.oow : 65536.0);\n"
"  else if (u.fog_mode == 3) f = clamp(in.z, 0.0, 1.0);\n"
"  if (f > 0) o.rgb += (u.fog_color.rgb - o.rgb) * f;\n"
"  FO r; r.c = o;\n"
"  r.d = (u.depth_mode == 2 || u.depth_mode == 4) ? float(wfloat(in.oow)) / 65536.0 : floor(clamp(in.z * 65535.0, 0.0, 65535.0)) / 65536.0;\n"
"  return r;\n"
"}\n"
/* the clear: a colour and a depth over a rectangle */
"struct CO { float4 pos [[position]]; };\n"
"vertex CO cvs(uint i [[vertex_id]], const device float4 *v [[buffer(0)]]) { CO o; o.pos = v[i]; return o; }\n"
"struct CF { float4 c [[color(0)]]; float d [[depth(any)]]; };\n"
"fragment CF cfs(CO in [[stage_in]], constant float4 &c [[buffer(0)]], constant float &d [[buffer(1)]]) { CF o; o.c = c; o.d = d; return o; }\n"
/* textured rectangles: the LFB writes into the frame, the frame into the window */
"struct BO { float4 pos [[position]]; float2 uv; };\n"
"vertex BO bvs(uint i [[vertex_id]], const device float4 *v [[buffer(0)]]) { BO o; o.pos = float4(v[i].xy, 0, 1); o.uv = v[i].zw; return o; }\n"
"fragment float4 bfs(BO in [[stage_in]], texture2d<float> t [[texture(0)]], sampler sm [[sampler(0)]]) { return float4(t.sample(sm, in.uv).rgb, 1); }\n"
/* the frame into the window, sharp bilinear: whole texels at any scale,
   blended only across the one window pixel a texel edge falls in */
"fragment float4 sfs(BO in [[stage_in]], texture2d<float> t [[texture(0)]]) {\n"
"  constexpr sampler s(filter::linear, address::clamp_to_edge);\n"
"  float2 ts = float2(t.get_width(), t.get_height()), px = in.uv * ts - 0.5;\n"
"  float2 i = floor(px), f = px - i, d = max(fwidth(px), float2(1e-5));\n"
"  f = clamp((f - 0.5) / d + 0.5, 0.0, 1.0);\n"
"  return float4(t.sample(s, (i + 0.5 + f) / ts).rgb, 1); }\n";

typedef struct V { float pos[2]; float z, oow; float col[4]; float st[2]; } V;
typedef struct U {
    int32_t cc_fn, cc_factor, cc_local, cc_other, cc_invert;
    int32_t ac_fn, ac_factor, ac_local, ac_other, ac_invert;
    int32_t tc_rgb_fn, tc_rgb_factor, tc_alpha_fn, tc_alpha_factor, tc_rgb_invert, tc_alpha_invert;
    int32_t has_tex, atest_fn, fog_mode, depth_mode;
    float atest_ref, vw, vh, xf;
    float konst[4], fog_color[4];
    float fog[64];
} U;

/* ---- state --------------------------------------------------------------------------- */
static id<MTLDevice> s_dev;
static id<MTLCommandQueue> s_q;
static id<MTLLibrary> s_lib;
static id<MTLTexture> s_col, s_dep;
static int s_fresh;                 /* the targets are new: the next pass clears them */
static id<MTLRenderPipelineState> s_pipes[16][16];     /* by blend src, dst */
static id<MTLRenderPipelineState> s_clear[2], s_blit, s_sharp, s_lfbpipe, s_lfbsharp;
static id<MTLBuffer> s_xt[3];           /* each frame's map table (glide.c), with its vertex buffer */
static id<MTLDepthStencilState> s_ds[2][8][2];         /* depth on, function, write */
static id<MTLDepthStencilState> s_clear_ds[2];
static id<MTLSamplerState> s_samp[2][2][2];            /* filter, clamp s, clamp t */
static id<MTLSamplerState> s_near;
static id<MTLTexture> s_white;                       /* a texture the game named but never made */
static id<MTLCommandBuffer> s_cmd;
static id<MTLRenderCommandEncoder> s_enc;
static id<MTLBuffer> s_vb[3];
static int s_vbi;
static size_t s_vused;
#define VB_SIZE (8u << 20)
static int s_w, s_h;                /* the game's frame: Glide's coordinates */
static int s_rw, s_rh;              /* the targets drawn into: the window's pixels, letterboxed */

#define TEX_MAX 4096
static id<MTLTexture> s_tex[TEX_MAX];
static uint32_t s_next_id = 1;
static unsigned long s_frame;
static uint8_t *s_shot_rgba;
static long s_shot_frame = -1;
static char s_shot_path[1024];
static dispatch_semaphore_t s_inflight;
static int s_offscreen;            /* BR_VCLOCK: frames never wait for the display */

static MTLBlendFactor bf(int k, int src)
{
    switch (k) {
    case 0:   return MTLBlendFactorZero;
    case 1:   return MTLBlendFactorSourceAlpha;
    case 2:   return src ? MTLBlendFactorDestinationColor : MTLBlendFactorSourceColor;
    case 3:   return MTLBlendFactorOne;                 /* no destination alpha: it reads as one */
    case 4:   return MTLBlendFactorOne;
    case 5:   return MTLBlendFactorOneMinusSourceAlpha;
    case 6:   return src ? MTLBlendFactorOneMinusDestinationColor : MTLBlendFactorOneMinusSourceColor;
    case 7:   return MTLBlendFactorZero;
    case 0xF: return MTLBlendFactorZero;         /* min(src alpha, 1 - dst alpha), dst alpha one */
    default:  return MTLBlendFactorOne;
    }
}

static id<MTLRenderPipelineState> pipe_for(int src, int dst)
{
    src &= 15;
    dst &= 15;
    if (!s_pipes[src][dst]) {
        MTLRenderPipelineDescriptor *d = [MTLRenderPipelineDescriptor new];
        NSError *err = nil;
        d.vertexFunction = [s_lib newFunctionWithName:@"vs"];
        d.fragmentFunction = [s_lib newFunctionWithName:@"fs"];
        d.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
        d.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
        d.colorAttachments[0].writeMask = MTLColorWriteMaskRed | MTLColorWriteMaskGreen | MTLColorWriteMaskBlue;
        if (!(src == 4 && dst == 0)) {
            d.colorAttachments[0].blendingEnabled = YES;
            d.colorAttachments[0].sourceRGBBlendFactor = bf(src, 1);
            d.colorAttachments[0].destinationRGBBlendFactor = bf(dst, 0);
            d.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorZero;
            d.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOne;
        }
        s_pipes[src][dst] = [s_dev newRenderPipelineStateWithDescriptor:d error:&err];
        if (!s_pipes[src][dst])
            fprintf(stderr, "brr: metal pipeline: %s\n", [[err description] UTF8String]);
    }
    return s_pipes[src][dst];
}

static MTLCompareFunction cmpfn(int fn)
{
    static const MTLCompareFunction k[8] = {
        MTLCompareFunctionNever, MTLCompareFunctionLess, MTLCompareFunctionEqual, MTLCompareFunctionLessEqual,
        MTLCompareFunctionGreater, MTLCompareFunctionNotEqual, MTLCompareFunctionGreaterEqual, MTLCompareFunctionAlways,
    };
    return k[fn & 7];
}

static id<MTLDepthStencilState> ds_for(int on, int fn, int write)
{
    on = on != 0;
    write = write != 0;
    fn &= 7;
    if (!s_ds[on][fn][write]) {
        MTLDepthStencilDescriptor *d = [MTLDepthStencilDescriptor new];
        d.depthCompareFunction = on ? cmpfn(fn) : MTLCompareFunctionAlways;
        d.depthWriteEnabled = on && write;
        s_ds[on][fn][write] = [s_dev newDepthStencilStateWithDescriptor:d];
    }
    return s_ds[on][fn][write];
}

static id<MTLSamplerState> samp_for(int filter, int cs, int ct)
{
    filter = filter != 0;
    cs = cs != 0;
    ct = ct != 0;
    if (!s_samp[filter][cs][ct]) {
        MTLSamplerDescriptor *d = [MTLSamplerDescriptor new];
        d.minFilter = d.magFilter = filter ? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest;
        d.sAddressMode = cs ? MTLSamplerAddressModeClampToEdge : MTLSamplerAddressModeRepeat;
        d.tAddressMode = ct ? MTLSamplerAddressModeClampToEdge : MTLSamplerAddressModeRepeat;
        s_samp[filter][cs][ct] = [s_dev newSamplerStateWithDescriptor:d];
    }
    return s_samp[filter][cs][ct];
}

static id<MTLRenderPipelineState> simple_pipe(NSString *vs, NSString *fs, int colour, int depth)
{
    MTLRenderPipelineDescriptor *d = [MTLRenderPipelineDescriptor new];
    NSError *err = nil;
    id<MTLRenderPipelineState> p;
    d.vertexFunction = [s_lib newFunctionWithName:vs];
    d.fragmentFunction = [s_lib newFunctionWithName:fs];
    d.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
    if (!colour)
        d.colorAttachments[0].writeMask = MTLColorWriteMaskNone;
    if (depth)
        d.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
    p = [s_dev newRenderPipelineStateWithDescriptor:d error:&err];
    if (!p)
        fprintf(stderr, "brr: metal pipeline %s: %s\n", [fs UTF8String], [[err description] UTF8String]);
    return p;
}

/* the colour and depth targets at w x h pixels.  The game draws in the
 * resolution it opened (its Glide coordinates); the targets take the
 * window's size, so geometry, text and textures are rasterised and filtered
 * at the window's resolution rather than drawn at that size and enlarged
 * (as the wasm lane draws) */
static void targets(int w, int h)
{
    MTLTextureDescriptor *td;
    td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                            width:(NSUInteger)w height:(NSUInteger)h mipmapped:NO];
    td.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    td.storageMode = MTLStorageModeShared;
    s_col = [s_dev newTextureWithDescriptor:td];
    td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                                            width:(NSUInteger)w height:(NSUInteger)h mipmapped:NO];
    td.usage = MTLTextureUsageRenderTarget;
    td.storageMode = MTLStorageModePrivate;
    s_dep = [s_dev newTextureWithDescriptor:td];
    s_rw = w;
    s_rh = h;
    s_fresh = 1;
    free(s_shot_rgba);
    s_shot_rgba = NULL;
}

/* the target size for a drawable of dw x dh: the drawable's, one target
 * pixel per window pixel, as the original drew at the resolution the player
 * chose; glide.c's screen map places the game's picture on it */
static void target_size(double dw, double dh, int *w, int *h)
{
    *w = (int)lround(dw);
    *h = (int)lround(dh);
    if (*w < 1) *w = 1;
    if (*h < 1) *h = 1;
}

int brr_open(int width, int height)
{
    @autoreleasepool {
        NSError *err = nil;
        MTLTextureDescriptor *td;
        CAMetalLayer *ml;
        int i;
        s_w = width;
        s_h = height;
        s_dev = MTLCreateSystemDefaultDevice();
        if (!s_dev)
            return 0;
        s_q = [s_dev newCommandQueue];
        s_lib = [s_dev newLibraryWithSource:k_src options:nil error:&err];
        if (!s_lib) {
            fprintf(stderr, "brr: metal shader: %s\n", [[err description] UTF8String]);
            return 0;
        }
        targets(width, height);
        for (i = 0; i < 3; i++) {
            s_vb[i] = [s_dev newBufferWithLength:VB_SIZE options:MTLResourceStorageModeShared];
            s_xt[i] = [s_dev newBufferWithLength:BRR_XF_MAX * 16 options:MTLResourceStorageModeShared];
            {                                   /* until a frame sets it: the identity */
                float *t = (float *)[s_xt[i] contents];
                int k;
                for (k = 0; k < BRR_XF_MAX; k++) {
                    t[4 * k + 0] = 2.0f / width;  t[4 * k + 1] = -1;
                    t[4 * k + 2] = -2.0f / height; t[4 * k + 3] = 1;
                }
            }
        }
        s_clear[0] = simple_pipe(@"cvs", @"cfs", 0, 1);
        s_clear[1] = simple_pipe(@"cvs", @"cfs", 1, 1);
        s_lfbpipe = simple_pipe(@"bvs", @"bfs", 1, 1);
        s_lfbsharp = simple_pipe(@"bvs", @"sfs", 1, 1);
        s_blit = simple_pipe(@"bvs", @"bfs", 1, 0);
        s_sharp = simple_pipe(@"bvs", @"sfs", 1, 0);
        for (i = 0; i < 2; i++) {
            MTLDepthStencilDescriptor *d = [MTLDepthStencilDescriptor new];
            d.depthCompareFunction = MTLCompareFunctionAlways;
            d.depthWriteEnabled = i;
            s_clear_ds[i] = [s_dev newDepthStencilStateWithDescriptor:d];
        }
        {
            MTLSamplerDescriptor *d = [MTLSamplerDescriptor new];
            d.minFilter = d.magFilter = MTLSamplerMinMagFilterNearest;
            s_near = [s_dev newSamplerStateWithDescriptor:d];
        }
        {
            static const uint32_t one = 0xFFFFFFFFu;
            td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                    width:1 height:1 mipmapped:NO];
            s_white = [s_dev newTextureWithDescriptor:td];
            [s_white replaceRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0 withBytes:&one bytesPerRow:4];
        }
        s_inflight = dispatch_semaphore_create(3);
        s_offscreen = getenv("BR_VCLOCK") != NULL;
        ml = host_macos_metal_layer();
        if (ml) {
            /* presents keep to the display's refresh, as the board's
             * buffer swap kept to the monitor's */
            [ml setDevice:s_dev];
            [ml setMaximumDrawableCount:3];
        }
        {
            const char *e = getenv("BR_SHOT");
            if (e && sscanf(e, "%ld:%1023s", &s_shot_frame, s_shot_path) != 2)
                s_shot_frame = -1;
        }
        fprintf(stderr, "brr: metal renderer %dx%d on %s\n", width, height, [[s_dev name] UTF8String]);
    }
    return 1;
}

void brr_close(void) {}

uint32_t brr_texture(uint32_t tid, const uint8_t *rgba, int w, int h)
{
    @autoreleasepool {
        MTLTextureDescriptor *td;
        id<MTLTexture> t;
        if (tid == 0) {
            for (tid = s_next_id; tid < TEX_MAX && s_tex[tid]; tid++)
                ;
            if (tid >= TEX_MAX)
                for (tid = 1; tid < TEX_MAX && s_tex[tid]; tid++)
                    ;
            if (tid >= TEX_MAX)
                return 0;
            s_next_id = tid + 1;
        }
        if (tid >= TEX_MAX || w <= 0 || h <= 0)
            return 0;
        td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                width:w height:h mipmapped:NO];
        td.usage = MTLTextureUsageShaderRead;
        t = [s_dev newTextureWithDescriptor:td];
        [t replaceRegion:MTLRegionMake2D(0, 0, w, h) mipmapLevel:0 withBytes:rgba bytesPerRow:(NSUInteger)w * 4];
        s_tex[tid] = t;
        return tid;
    }
}

void brr_texture_free(uint32_t id)
{
    if (id && id < TEX_MAX) {
        s_tex[id] = nil;
        if (id < s_next_id)
            s_next_id = id;
    }
}

/* the frame's encoder into the offscreen target, begun on first use */
static id<MTLRenderCommandEncoder> enc(void)
{
    if (!s_enc) {
        MTLRenderPassDescriptor *rp = [MTLRenderPassDescriptor renderPassDescriptor];
        if (!s_cmd) {
            dispatch_semaphore_wait(s_inflight, DISPATCH_TIME_FOREVER);
            s_cmd = [s_q commandBuffer];
            {
                dispatch_semaphore_t sem = s_inflight;
                [s_cmd addCompletedHandler:^(id<MTLCommandBuffer> b) { (void)b; dispatch_semaphore_signal(sem); }];
            }
            s_vbi = (s_vbi + 1) % 3;
            s_vused = 0;
        }
        rp.colorAttachments[0].texture = s_col;
        rp.colorAttachments[0].loadAction = s_fresh ? MTLLoadActionClear : MTLLoadActionLoad;
        rp.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);
        rp.colorAttachments[0].storeAction = MTLStoreActionStore;
        rp.depthAttachment.texture = s_dep;
        rp.depthAttachment.loadAction = s_fresh ? MTLLoadActionClear : MTLLoadActionLoad;
        rp.depthAttachment.clearDepth = 1.0;
        s_fresh = 0;
        rp.depthAttachment.storeAction = MTLStoreActionStore;
        s_enc = [s_cmd renderCommandEncoderWithDescriptor:rp];
    }
    return s_enc;
}

/* the draw's scissor, in target pixels (glide.c); 0 when it is empty */
static int scissor(id<MTLRenderCommandEncoder> e, const brr_state *st)
{
    int x0 = st->sx0 < 0 ? 0 : st->sx0, y0 = st->sy0 < 0 ? 0 : st->sy0;
    int x1 = st->sx1 > s_rw ? s_rw : st->sx1, y1 = st->sy1 > s_rh ? s_rh : st->sy1;
    if (x1 <= x0 || y1 <= y0)
        return 0;
    [e setScissorRect:(MTLScissorRect){ (NSUInteger)x0, (NSUInteger)y0, (NSUInteger)(x1 - x0), (NSUInteger)(y1 - y0) }];
    return 1;
}

/* room for n bytes in this frame's vertex buffer: the offset, or -1 */
static long vb_take(size_t n)
{
    size_t o = (s_vused + 15) & ~(size_t)15;
    if (o + n > VB_SIZE)
        return -1;
    s_vused = o + n;
    return (long)o;
}

void brr_clear(uint32_t argb, float depth, int colour, int depthbuf, const brr_state *clip)
{
    @autoreleasepool {
        id<MTLRenderCommandEncoder> e = enc();
        float x0 = (float)(clip->sx0 < 0 ? 0 : clip->sx0), y0 = (float)(clip->sy0 < 0 ? 0 : clip->sy0);
        float x1 = (float)(clip->sx1 > s_rw ? s_rw : clip->sx1), y1 = (float)(clip->sy1 > s_rh ? s_rh : clip->sy1);
        float c[4] = { ((argb >> 16) & 0xFF) / 255.0f, ((argb >> 8) & 0xFF) / 255.0f, (argb & 0xFF) / 255.0f, 1 };
        float q[6][4];
        long off;
        int i;
        static const int ix[6] = { 0, 1, 2, 2, 1, 3 };
        float px[4][2] = { { x0, y0 }, { x1, y0 }, { x0, y1 }, { x1, y1 } };
        if ((!colour && !depthbuf) || x1 <= x0 || y1 <= y0)
            return;
        for (i = 0; i < 6; i++) {
            q[i][0] = px[ix[i]][0] / s_rw * 2 - 1;
            q[i][1] = 1 - px[ix[i]][1] / s_rh * 2;
            q[i][2] = 0;
            q[i][3] = 1;
        }
        if ((off = vb_take(sizeof q)) < 0)
            return;
        memcpy((char *)[s_vb[s_vbi] contents] + off, q, sizeof q);
        [e setRenderPipelineState:s_clear[colour ? 1 : 0]];
        [e setDepthStencilState:s_clear_ds[depthbuf ? 1 : 0]];
        [e setScissorRect:(MTLScissorRect){ 0, 0, (NSUInteger)s_rw, (NSUInteger)s_rh }];
        [e setVertexBuffer:s_vb[s_vbi] offset:(NSUInteger)off atIndex:0];
        [e setFragmentBytes:c length:sizeof c atIndex:0];
        [e setFragmentBytes:&depth length:sizeof depth atIndex:1];
        [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
    }
}

void brr_draw(const brr_state *st, const brr_vertex *v, int n)
{
    @autoreleasepool {
        id<MTLRenderCommandEncoder> e;
        U u;
        long off;
        int i;
        if (n < 3)
            return;
        e = enc();
        if ((off = vb_take((size_t)n * sizeof(V))) < 0)
            return;
        {
            V *o = (V *)((char *)[s_vb[s_vbi] contents] + off);
            for (i = 0; i < n; i++) {
                o[i].pos[0] = v[i].x;
                o[i].pos[1] = v[i].y;
                o[i].z = v[i].z;
                o[i].oow = v[i].oow;
                o[i].col[0] = v[i].r;
                o[i].col[1] = v[i].g;
                o[i].col[2] = v[i].b;
                o[i].col[3] = v[i].a;
                o[i].st[0] = v[i].s;
                o[i].st[1] = v[i].t;
            }
        }
        memset(&u, 0, sizeof u);
        u.cc_fn = st->cc_fn; u.cc_factor = st->cc_factor; u.cc_local = st->cc_local;
        u.cc_other = st->cc_other; u.cc_invert = st->cc_invert;
        u.ac_fn = st->ac_fn; u.ac_factor = st->ac_factor; u.ac_local = st->ac_local;
        u.ac_other = st->ac_other; u.ac_invert = st->ac_invert;
        u.tc_rgb_fn = st->tc_rgb_fn; u.tc_rgb_factor = st->tc_rgb_factor;
        u.tc_alpha_fn = st->tc_alpha_fn; u.tc_alpha_factor = st->tc_alpha_factor;
        u.tc_rgb_invert = st->tc_rgb_invert; u.tc_alpha_invert = st->tc_alpha_invert;
        u.has_tex = st->texture != 0;
        u.atest_fn = st->atest_fn;
        u.atest_ref = st->atest_ref;
        u.fog_mode = st->fog_mode & 0xFF;
        u.depth_mode = st->depth_mode;
        u.vw = (float)s_w;
        u.vh = (float)s_h;
        u.xf = (float)(st->xf >= 0 && st->xf < BRR_XF_MAX ? st->xf : 0);
        u.konst[0] = ((st->constant >> 16) & 0xFF) / 255.0f;
        u.konst[1] = ((st->constant >> 8) & 0xFF) / 255.0f;
        u.konst[2] = (st->constant & 0xFF) / 255.0f;
        u.konst[3] = (st->constant >> 24) / 255.0f;
        u.fog_color[0] = ((st->fog_color >> 16) & 0xFF) / 255.0f;
        u.fog_color[1] = ((st->fog_color >> 8) & 0xFF) / 255.0f;
        u.fog_color[2] = (st->fog_color & 0xFF) / 255.0f;
        for (i = 0; i < 64; i++)
            u.fog[i] = st->fog_table[i] / 255.0f;
        [e setRenderPipelineState:pipe_for(st->blend_rgb_src, st->blend_rgb_dst)];
        [e setDepthStencilState:ds_for(st->depth_mode, st->depth_fn, st->depth_mask)];
        if (!scissor(e, st))
            return;
        [e setVertexBuffer:s_xt[s_vbi] offset:0 atIndex:2];
        [e setVertexBuffer:s_vb[s_vbi] offset:(NSUInteger)off atIndex:0];
        [e setVertexBytes:&u length:sizeof u atIndex:1];
        [e setFragmentBytes:&u length:sizeof u atIndex:0];
        if (u.has_tex) {
            id<MTLTexture> t = st->texture < TEX_MAX && s_tex[st->texture] ? s_tex[st->texture] : s_white;
            [e setFragmentTexture:t atIndex:0];
            [e setFragmentSamplerState:samp_for(st->mag_filter || st->min_filter, st->clamp_s, st->clamp_t) atIndex:0];
        }
        [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:(NSUInteger)n];
    }
}

/* LFB writes: the pixels into a texture of their own, drawn over target
 * rectangle d (glide.c's screen map), sharp bilinear when scaled */
void brr_lfb_write(int x, int y, int w, int h, const uint16_t *p, int stride, const int d[4])
{
    @autoreleasepool {
        uint32_t *px;
        float q[6][4];
        long off;
        int i, j;
        static const int ix[6] = { 0, 1, 2, 2, 1, 3 };
        id<MTLRenderCommandEncoder> e;
        MTLTextureDescriptor *td;
        id<MTLTexture> t;
        (void)x; (void)y;
        if (w <= 0 || h <= 0 || d[2] <= d[0] || d[3] <= d[1])
            return;
        px = (uint32_t *)malloc((size_t)w * (size_t)h * 4);
        if (!px)
            return;
        for (j = 0; j < h; j++) {
            const uint16_t *r = (const uint16_t *)((const uint8_t *)p + (size_t)j * (size_t)stride);
            for (i = 0; i < w; i++) {
                uint32_t c = r[i];
                uint32_t R = (c >> 11) * 255 / 31, G = ((c >> 5) & 63) * 255 / 63, B = (c & 31) * 255 / 31;
                px[(size_t)j * (size_t)w + (size_t)i] = 0xFF000000u | R << 16 | G << 8 | B;
            }
        }
        td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                                width:(NSUInteger)w height:(NSUInteger)h mipmapped:NO];
        td.usage = MTLTextureUsageShaderRead;
        t = [s_dev newTextureWithDescriptor:td];
        [t replaceRegion:MTLRegionMake2D(0, 0, (NSUInteger)w, (NSUInteger)h) mipmapLevel:0
               withBytes:px bytesPerRow:(NSUInteger)w * 4];
        free(px);
        e = enc();
        {
            float px4[4][4] = { { (float)d[0], (float)d[1], 0, 0 }, { (float)d[2], (float)d[1], 1, 0 },
                                { (float)d[0], (float)d[3], 0, 1 }, { (float)d[2], (float)d[3], 1, 1 } };
            for (i = 0; i < 6; i++) {
                q[i][0] = px4[ix[i]][0] / s_rw * 2 - 1;
                q[i][1] = 1 - px4[ix[i]][1] / s_rh * 2;
                q[i][2] = px4[ix[i]][2];
                q[i][3] = px4[ix[i]][3];
            }
        }
        if ((off = vb_take(sizeof q)) < 0)
            return;
        memcpy((char *)[s_vb[s_vbi] contents] + off, q, sizeof q);
        [e setRenderPipelineState:d[2] - d[0] == w && d[3] - d[1] == h ? s_lfbpipe : s_lfbsharp];
        [e setDepthStencilState:s_clear_ds[0]];
        [e setScissorRect:(MTLScissorRect){ 0, 0, (NSUInteger)s_rw, (NSUInteger)s_rh }];
        [e setVertexBuffer:s_vb[s_vbi] offset:(NSUInteger)off atIndex:0];
        [e setFragmentTexture:t atIndex:0];
        [e setFragmentSamplerState:s_near atIndex:0];
        [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
    }
}

void brr_target(int *w, int *h)
{
    *w = s_rw;
    *h = s_rh;
}

/* this frame's map table: into the buffer its draws read, before it is committed */
void brr_xf(const float (*t)[4], int n)
{
    @autoreleasepool {
        if (n > BRR_XF_MAX)
            n = BRR_XF_MAX;
        enc();                                  /* the frame's command buffer, and so its slot */
        memcpy([s_xt[s_vbi] contents], t, (size_t)n * 16);
    }
}

/* the frame read back (BGRA) to a PNG */
static int write_shot(const char *path)
{
    CGColorSpaceRef cs;
    CGContextRef cg;
    CGImageRef img;
    CGImageDestinationRef dst;
    CFURLRef url;
    int ok = 0;
    if (!s_shot_rgba)
        return 0;
    cs = CGColorSpaceCreateDeviceRGB();
    cg = CGBitmapContextCreate(s_shot_rgba, (size_t)s_rw, (size_t)s_rh, 8, (size_t)s_rw * 4, cs,
                               kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Little);
    img = CGBitmapContextCreateImage(cg);
    url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)path, (CFIndex)strlen(path), false);
    dst = CGImageDestinationCreateWithURL(url, CFSTR("public.png"), 1, NULL);
    if (dst && img) {
        CGImageDestinationAddImage(dst, img, NULL);
        ok = CGImageDestinationFinalize(dst);
    }
    if (dst) CFRelease(dst);
    CFRelease(url);
    if (img) CGImageRelease(img);
    CGContextRelease(cg);
    CGColorSpaceRelease(cs);
    return ok;
}

void brr_present(void)
{
    @autoreleasepool {
        /* a window that cannot be seen gets no refresh, and its next
         * drawable would hold the game up to a second: the frame is drawn
         * offscreen as always but not shown */
        CAMetalLayer *ml = s_offscreen || !host_window_visible() ? nil : host_macos_metal_layer();
        id<CAMetalDrawable> dr;
        int want_shot = (long)(s_frame + 1) == s_shot_frame;
        enc();                                 /* a frame with nothing drawn still presents */
        [s_enc endEncoding];
        s_enc = nil;
        if (ml) {
            int fw, fh;
            host_macos_layer_fit(&fw, &fh);
            dr = [ml nextDrawable];
            if (dr) {
                MTLRenderPassDescriptor *rp = [MTLRenderPassDescriptor renderPassDescriptor];
                id<MTLRenderCommandEncoder> e;
                double dw = (double)[dr texture].width, dh = (double)[dr texture].height;
                /* the target, at its own shape (the screen map already placed
                 * the game's picture on it), fitted to the drawable */
                double k = fmin(dw / s_rw, dh / s_rh), qw = s_rw * k / dw, qh = s_rh * k / dh;
                float q[6][4];
                static const int ix[6] = { 0, 1, 2, 2, 1, 3 };
                float c4[4][4] = { { (float)-qw, (float)qh, 0, 0 }, { (float)qw, (float)qh, 1, 0 },
                                   { (float)-qw, (float)-qh, 0, 1 }, { (float)qw, (float)-qh, 1, 1 } };
                int i;
                long off;
                for (i = 0; i < 6; i++)
                    memcpy(q[i], c4[ix[i]], sizeof q[i]);
                rp.colorAttachments[0].texture = [dr texture];
                rp.colorAttachments[0].loadAction = MTLLoadActionClear;
                rp.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);
                rp.colorAttachments[0].storeAction = MTLStoreActionStore;
                e = [s_cmd renderCommandEncoderWithDescriptor:rp];
                if ((off = vb_take(sizeof q)) >= 0) {
                    memcpy((char *)[s_vb[s_vbi] contents] + off, q, sizeof q);
                    [e setRenderPipelineState:s_sharp];
                    [e setVertexBuffer:s_vb[s_vbi] offset:(NSUInteger)off atIndex:0];
                    [e setFragmentTexture:s_col atIndex:0];
                    [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
                }
                [e endEncoding];
                [s_cmd presentDrawable:dr];
            }
        }
        [s_cmd commit];
        if (want_shot) {
            [s_cmd waitUntilCompleted];
            if (!s_shot_rgba)
                s_shot_rgba = (uint8_t *)malloc((size_t)s_rw * (size_t)s_rh * 4);
            [s_col getBytes:s_shot_rgba bytesPerRow:(NSUInteger)s_rw * 4
                 fromRegion:MTLRegionMake2D(0, 0, (NSUInteger)s_rw, (NSUInteger)s_rh) mipmapLevel:0];
            if (write_shot(s_shot_path))
                fprintf(stderr, "brr: frame %ld -> %s\n", s_shot_frame, s_shot_path);
        }
        s_cmd = nil;
        s_frame++;
        if (ml) {                              /* the next frame at the window's size */
            int w, h;
            CGSize ds = [ml drawableSize];
            target_size(ds.width, ds.height, &w, &h);
            if (w != s_rw || h != s_rh)
                targets(w, h);
        }
    }
}

int brr_shot(const char *path)
{
    @autoreleasepool {
        id<MTLCommandBuffer> c = [s_q commandBuffer];
        [c commit];
        [c waitUntilCompleted];                /* everything queued before it is done */
        if (!s_shot_rgba)
            s_shot_rgba = (uint8_t *)malloc((size_t)s_rw * (size_t)s_rh * 4);
        [s_col getBytes:s_shot_rgba bytesPerRow:(NSUInteger)s_rw * 4
             fromRegion:MTLRegionMake2D(0, 0, (NSUInteger)s_rw, (NSUInteger)s_rh) mipmapLevel:0];
        return write_shot(path);
    }
}
