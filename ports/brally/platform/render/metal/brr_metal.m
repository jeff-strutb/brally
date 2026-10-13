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
 * toolchain step.
 *
 * With BR_FLAG_LIGHTING (the Remastered lighting, brr_metal_lit.h and its
 * section below) a frame is recorded and replayed through the lighting's
 * passes instead; without it, nothing of that runs. */
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>
#import <ImageIO/ImageIO.h>
#include <TargetConditionals.h>
#include <simd/simd.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "brr.h"
#include "brr_metal_lights.h"
#include "brr_metal_lit.h"
#include "light_night.h"
#include "light_world.h"
#include "brr_metal_sky.h"
#include "brr_skymask.h"
#include "brr_metal_vol.h"
#include "brr_metal_rt.h"
#include "brr_metal_refl_api.h"
#include "brr_metal_gi.h"
#include "brr_shadow.h"
#include "brr_metal_taa_api.h"
#include "br_flags.h"
#include "light.h"
#include "light_cluster.h"

CAMetalLayer *host_macos_metal_layer(void);
void host_macos_layer_fit(int *w, int *h);   /* host_macos.m: drawable = the view in pixels */      /* host_macos.m */
/* without a window host (HOST=null: headless runs that take shots) there is
 * no layer: frames are drawn offscreen and never presented */
__attribute__((weak)) CAMetalLayer *host_macos_metal_layer(void) { return nil; }
__attribute__((weak)) void host_macos_layer_fit(int *w, int *h) { *w = *h = 0; }
#include "host.h"

/* ---- the shader -------------------------------------------------------------------- */
static NSString *const k_src = @
"#include <metal_stdlib>\n"
"using namespace metal;\n"
"struct V { packed_float2 pos; float z; float oow; packed_float4 col; packed_float2 st; packed_float4 wld; };\n"
"struct U {\n"
"  int cc_fn, cc_factor, cc_local, cc_other, cc_invert;\n"
"  int ac_fn, ac_factor, ac_local, ac_other, ac_invert;\n"
"  int tc_rgb_fn, tc_rgb_factor, tc_alpha_fn, tc_alpha_factor, tc_rgb_invert, tc_alpha_invert;\n"
"  int has_tex, atest_fn, fog_mode, depth_mode;\n"
"  float atest_ref, vw, vh, xf;\n"
"  float4 konst, fog_color;\n"
"  float fog[64];\n"
"  float glow, addk;\n"
"};\n"
"struct O { float4 pos [[position]]; float4 col [[center_no_perspective]]; float2 st; float oow [[center_no_perspective]]; float z [[center_no_perspective]]; float3 wp; float mat [[flat]]; };\n"
/* the screen map (glide.c): the game's pixels to the target through entry
   u.xf of this frame's table */
"vertex O vs(uint i [[vertex_id]], const device V *v [[buffer(0)]], constant U &u [[buffer(1)]],\n"
"            const device float4 *xt [[buffer(2)]]) {\n"
"  O o; V a = v[i]; float4 T = xt[int(u.xf)];\n"
"  float w = a.oow > 0 ? 1.0 / a.oow : 1.0;\n"
"  float2 ndc = float2(a.pos.x * T.x + T.y, a.pos.y * T.z + T.w);\n"
"  o.pos = float4(ndc * w, clamp(a.z, 0.0, 1.0) * w, w);\n"
"  o.col = clamp(a.col / 255.0, 0.0, 1.0); o.st = a.st; o.oow = a.oow; o.z = a.z; o.wp = a.wld.xyz; o.mat = a.wld.w;\n"
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
/* the game's own fragment, for the lit shaders (brr_metal_lit.h): the same
   equations as fs, with what they were made of */
"struct SH { float4 o; float4 tex; float4 it; float fog; float d; bool keep; };\n"
"static SH shade(O in, constant U &u, texture2d<float> t, sampler sm) {\n"
"  SH r; float4 tex = float4(0);\n"
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
"  r.keep = !(u.atest_fn != 7 && !cmpf(u.atest_fn, floor(o.a * 255.0 + 0.5), u.atest_ref));\n"
"  float f = 0;\n"
"  if (u.fog_mode == 1) f = it.a;\n"
"  else if (u.fog_mode == 2) f = fogt(u, in.oow > 0 ? 1.0 / in.oow : 65536.0);\n"
"  else if (u.fog_mode == 3) f = clamp(in.z, 0.0, 1.0);\n"
"  r.fog = f; r.tex = u.has_tex ? tex : float4(1); r.it = it; r.o = o;\n"
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

typedef struct V { float pos[2]; float z, oow; float col[4]; float st[2]; float wld[4]; } V;
typedef struct U {
    int32_t cc_fn, cc_factor, cc_local, cc_other, cc_invert;
    int32_t ac_fn, ac_factor, ac_local, ac_other, ac_invert;
    int32_t tc_rgb_fn, tc_rgb_factor, tc_alpha_fn, tc_alpha_factor, tc_rgb_invert, tc_alpha_invert;
    int32_t has_tex, atest_fn, fog_mode, depth_mode;
    float atest_ref, vw, vh, xf;
    float konst[4], fog_color[4];
    float fog[64];
    float glow, addk;    /* the lit shaders: lamplight glows (night); an additive draw's light, cd/m2 */
} U;

/* ---- state --------------------------------------------------------------------------- */
static id<MTLDevice> s_dev;
static id<MTLCommandQueue> s_q;
static id<MTLLibrary> s_lib;
static id<MTLTexture> s_col, s_dep;
static int s_fresh;                 /* the targets are new: the next pass clears them */
static id<MTLRenderPipelineState> s_pipes[16][16];     /* by blend src, dst */
static id<MTLRenderPipelineState> s_clear[2], s_blit, s_sharp, s_lfbpipe, s_lfbsharp;
static id<MTLBuffer> s_xtj;            /* the lit 3D's jittered copy of it (brr_metal_taa.m), while its passes draw */
static int s_nxf[3];
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
static int s_lw, s_lh, s_lup;       /* the lit 3D's internal size (target x brr_taa_scale), and through MetalFX's scaler */
static int s_dw, s_dh;              /* what op_draw's scissors are in: the lit 3D's size while it draws, else the target's */
static id<MTLTexture> s_ldep;       /* the lit 3D's depth when it is scaled (s_dep otherwise) */
static long s_gpu_last_us;          /* the last whole frame's GPU time, us (the dynamic resolution's input) */

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
static void lit_reset(void);
static void lit_emask(uint32_t tid, const uint8_t *rgba, int w, int h);
static id<MTLTexture> lit_skymask(uint32_t tid);

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
    lit_reset();                        /* the lit frame's targets follow, at the next lit frame */
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

/* ---- the Remastered lighting: a lit frame (BR_FLAG_LIGHTING) -------------------------
 * The game's draws are recorded as it makes them (their vertices go into the
 * frame's vertex buffer as always) and replayed at present in passes
 * (brr_metal_lit.h): the G-buffer, the sun's shadow map, the lighting with
 * the backdrop behind it, the blended 3D forward-lit in the game's order,
 * bloom, post, then everything else (2D, the mirror, LFB writes) over the
 * result as the game drew it. */
enum { K_OPQ, K_BLEND, K_BACK, K_OTHER, K_CLEAR, K_LFB };
typedef struct Op {
    int kind;
    U u;
    int src, dst, dmode, dfn, dmask, cull;
    int sc[4], has_sc;
    uint32_t tex;
    int texobj;                     /* the texture object as it was when drawn (s_frametex): the game
                                       replaces a texture's contents within a frame (the font pages) */
    int filt, cs, ct;
    int tw, th;                     /* the texture's size */
    long voff;
    int n;
    int lfb, sharp;                 /* K_LFB: its texture in s_lfbtex, sharp-bilinear */
    int colour, depthbuf;           /* K_CLEAR */
    float clear[4], depth;
} Op;
static Op *s_ops;
static int s_nops, s_capops;
static NSMutableArray *s_lfbtex, *s_frametex;
static int s_litf;                  /* this frame is lit (the flag, read when it began) */
static id<MTLTexture> s_galb, s_gpos, s_gnrm, s_hdr, s_shadow, s_shcache, s_bl[6];
static int s_nbl;
static id<MTLTexture> s_emask[TEX_MAX], s_black;
static id<MTLTexture> s_skym[TEX_MAX];   /* a backdrop texture's painted sky (brr_skymask.h), made on its first use as one */
static id<MTLTexture> s_brdfE;        /* the GGX lobe's albedo by (mu, roughness): the diffuse's coupling (brdf_lut) */
static id<MTLRenderPipelineState> s_lpg, s_lpb, s_lpl, s_lpf[16][16], s_lbd, s_lbu, s_lbua, s_lpp, s_shpipe, s_shcopy;
static id<MTLDepthStencilState> s_shds, s_nods, s_shcopyds;
static float *s_tris;               /* this frame's opaque 3D in world space */
static float *s_dyn;                /* the part of it not on the track: the moving casters (cars, start lights) */
static size_t s_ndyn, s_capdyn;
static BrrShCache s_shc[CASCADES];  /* each cascade's static cache (brr_shadow.h) */
static unsigned long s_shc_redraws[CASCADES], s_shc_restored, s_shc_tiled, s_shc_frames;
static id<MTLTexture> s_shtile[CASCADES];   /* each cascade's scratch tile for its moving casters (brr_shc_tile) */
static id<MTLBuffer> s_trkbuf;       /* the whole track's triangles (light_game_track): the static casters */
static unsigned s_trkgen;
static int s_trkn;
static float s_trklo[3], s_trkhi[3];
static BrrRtWorld s_rt;             /* the track for rays (brr_metal_rt_world) */
static size_t s_ntris, s_captris;
static id<MTLBuffer> s_hist[3];
static float s_ev = 1e9f;
#define LUM_BINS 512             /* the meter's histogram: 16 bins a stop over log2 cd/m2 -10..22 */

typedef struct LU { float sunvp[CASCADES][16]; float csplit[4], cfwd[4], ctexel[4], cbias[4]; float eye[4], sun[4], sunc[4], skyc[4], gndc[4], misc[4], wb[4], bg[4], fogc[4];
                   float cdepth[4], csoft[4], scr[16], vol[4], gi[4], wet[4]; } LU;   /* brr_metal_lit.h's LU, the same layout; vol.x: 1 when the volumetric fog replaces the game's (brr_metal_vol.h); gi: the irradiance probes' switch (brr_metal_gi.h) */
typedef struct AU { float sun[4], cam[4], atm[4]; } AU;
typedef struct KU { float xft[4], vpt[4], m0[4], m1[4], m2[4], off[4], eye[4], tgt[4], sun[4], ktop[4], cld[4]; } KU;

/* the sky per weather (brr_metal_sky.h): the Mie density (haze, fog), the
 * light at the top of the atmosphere (the sun's 128,000 lux, or a full moon's
 * 0.32 at night), the part of the sun the clouds let through, and the
 * clouds' brightening of the sky */
static const struct { float mie, top, through, skyk, cover, dens; } k_sky[5] = {
    /* sunny */ { 1.0f, 128000.0f, 1.00f, 1.0f, 0.25f, 0.03f },
    /* fog   */ { 30.0f, 128000.0f, 0.15f, 1.0f, 0.95f, 0.10f },
    /* rain  */ { 12.0f, 128000.0f, 0.05f, 0.6f, 1.00f, 0.16f },
    /* snow  */ { 8.0f, 128000.0f, 0.25f, 1.0f, 0.85f, 0.10f },
    /* night */ { 1.0f, 0.32f, 1.00f, 1.0f, 0.35f, 0.06f },
};   /* cover: the cloud layer's fBm coverage; dens: its extinction per metre at full density (brr_metal_clouds.h) */
static id<MTLTexture> s_atmT, s_atmM, s_atmS, s_atmI[3];
static id<MTLRenderPipelineState> s_patmT, s_patmM, s_patmS, s_patmI, s_psky, s_pbk2;
static float s_atm_mie = -1.0f;

/* the light per weather, in physical units: the sun's (or moon's)
 * illuminance and colour, the sky's and the ground's light on a surface
 * facing them, lux; the camera's EV100 range the meter keeps to (the "sunny
 * 16" rule puts a sunlit scene near 15; a headlamp-lit road near 7) and how
 * rough the ground is (wet: smooth) */
/* At night the directional light is the full moon (light_night.h): sun is its
 * illuminance facing it at elevation elev (light_moon_normal(42) = 0.213 lux;
 * 0.143 on level ground), its colour the sun's spectrum reddened by the
 * moon's albedo, about 4,100 K (linear sRGB, Kim et al.'s Planckian locus);
 * the sky starlight and airglow, about 0.001 lux, blue.  elev 0: the track's
 * sun direction as it is.  The meter's band (percentiles plo..phi) and its
 * compensation comp (stops brighter): at night the band is the scene's
 * middle (30th to 75th percentile), so the moonlit hills and road set the
 * exposure, not the headlamp pool and the lamps above it, and a stop under
 * the meter's mid-grey, so night reads as night (MEASUREMENTS.md). */
static const struct { float sun, sky, gnd, evlo, evhi, rough; float sunc[3], skyc[3], gndc[3]; float elev, plo, phi, comp; float wet, pud; } k_weather[5] = {   /* wet, pud: brr_metal_refl.h */
    /* sunny */ { 100000, 20000, 6000, 12.0f, 16.0f, 0.70f, { 1.00f, 0.95f, 0.88f }, { 0.55f, 0.72f, 1.00f }, { 0.75f, 0.72f, 0.62f }, 0, 0.05f, 0.95f, 1.5f, 0.0f, 0.0f },
    /* fog   */ {  12000, 16000, 6000, 10.5f, 15.0f, 0.55f, { 1.00f, 0.97f, 0.92f }, { 0.85f, 0.88f, 0.92f }, { 0.70f, 0.70f, 0.68f }, 0, 0.80f, 0.983f, 0, 0.0f, 0.0f },
    /* rain  */ {   6000, 11000, 3500,  9.5f, 14.5f, 0.25f, { 0.95f, 0.97f, 1.00f }, { 0.70f, 0.76f, 0.85f }, { 0.55f, 0.56f, 0.56f }, 0, 0.80f, 0.983f, 0, 1.0f, 0.6f },
    /* snow  */ {  25000, 22000, 16000, 11.5f, 15.5f, 0.55f, { 1.00f, 0.98f, 0.95f }, { 0.80f, 0.86f, 1.00f }, { 0.95f, 0.97f, 1.00f }, 0, 0.80f, 0.983f, 0, 0.5f, 0.3f },
    /* night */ {  0.213f, 0.001f, 0.02f, -6.0f, 4.0f, 0.60f, { 1.00f, 0.669f, 0.402f }, { 0.45f, 0.60f, 1.00f }, { 0.40f, 0.40f, 0.40f }, LIGHT_MOON_ELEV, 0.30f, 0.75f, -1.0f, 0.0f, 0.0f },
};
/* the grade per weather (saturation, contrast about mid-grey): sunny a
 * modest lift for the high-key day PBR Neutral's highlight desaturation
 * greys; the rest none */
static const float k_grade[5][2] = { { 1.12f, 1.05f }, { 1, 1 }, { 1, 1 }, { 1, 1 }, { 1, 1 } };
static LightGlow s_glow;   /* which textures the track's scene draws (light_night.h) */
static float s_eshare[LIGHT_GLOW_TEX], s_ecol[LIGHT_GLOW_TEX][3];   /* the share of a texture's texels that glow, their mean linear colour */

static id<MTLRenderPipelineState> lit_pipe(NSString *vs, NSString *fs, const MTLPixelFormat *f, int nf, int depth,
                                           int blend_src, int blend_dst)
{
    MTLRenderPipelineDescriptor *d = [MTLRenderPipelineDescriptor new];
    NSError *err = nil;
    id<MTLRenderPipelineState> p;
    int i;
    d.vertexFunction = [s_lib newFunctionWithName:vs];
    d.fragmentFunction = fs ? [s_lib newFunctionWithName:fs] : nil;
    for (i = 0; i < nf; i++)
        d.colorAttachments[i].pixelFormat = f[i];
    if (blend_src >= 0) {
        d.colorAttachments[0].blendingEnabled = YES;
        d.colorAttachments[0].sourceRGBBlendFactor = blend_src == 99 ? MTLBlendFactorOne : bf(blend_src, 1);
        d.colorAttachments[0].destinationRGBBlendFactor = blend_dst == 99 ? MTLBlendFactorOne : bf(blend_dst, 0);
        d.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorZero;
        d.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOne;
    }
    if (depth)
        d.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
    p = [s_dev newRenderPipelineStateWithDescriptor:d error:&err];
    if (!p)
        fprintf(stderr, "brr: metal lit pipeline %s: %s\n", [(fs ? fs : vs) UTF8String], [[err description] UTF8String]);
    return p;
}

static void lit_init(void)
{
    static const MTLPixelFormat gb[3] = { MTLPixelFormatRGBA8Unorm, MTLPixelFormatRGBA32Float, MTLPixelFormatRGBA16Float };
    static const MTLPixelFormat hf = MTLPixelFormatRGBA16Float, cf = MTLPixelFormatBGRA8Unorm;
    MTLDepthStencilDescriptor *dd = [MTLDepthStencilDescriptor new];
    MTLTextureDescriptor *td;
    static const uint8_t zero = 0;
    s_lpg = lit_pipe(@"vs", @"fsgb", gb, 3, 1, -1, -1);
    s_lpb = lit_pipe(@"vs", @"fsbk", &hf, 1, 0, -1, -1);
    s_lpl = lit_pipe(@"lvs", @"lfs", &hf, 1, 0, -1, -1);
    s_lbd = lit_pipe(@"lvs", @"bdn", &hf, 1, 0, -1, -1);
    s_lbu = lit_pipe(@"lvs", @"bup", &hf, 1, 0, -1, -1);
    s_lbua = lit_pipe(@"lvs", @"bup", &hf, 1, 0, 99, 99);          /* added onto the next level up */
    s_lpp = lit_pipe(@"lvs", @"pfs", &cf, 1, 0, -1, -1);
    s_shpipe = lit_pipe(@"svs", nil, NULL, 0, 1, -1, -1);
    s_shcopy = lit_pipe(@"lvs", @"scopy", NULL, 0, 1, -1, -1);
    {
        static const MTLPixelFormat f32 = MTLPixelFormatRGBA32Float;
        s_patmT = lit_pipe(@"lvs", @"atm_t", &hf, 1, 0, -1, -1);
        s_patmM = lit_pipe(@"lvs", @"atm_ms", &hf, 1, 0, -1, -1);
        s_patmS = lit_pipe(@"lvs", @"atm_sv", &hf, 1, 0, -1, -1);
        s_patmI = lit_pipe(@"lvs", @"atm_irr", &f32, 1, 0, -1, -1);
        s_psky = lit_pipe(@"lvs", @"skyfs", &hf, 1, 0, -1, -1);
        s_pbk2 = lit_pipe(@"vs", @"fsbk2", &hf, 1, 0, 1, 5);     /* source alpha, one minus source alpha */
    }
    dd.depthCompareFunction = MTLCompareFunctionLessEqual;
    dd.depthWriteEnabled = YES;
    s_shds = [s_dev newDepthStencilStateWithDescriptor:dd];
    dd = [MTLDepthStencilDescriptor new];
    dd.depthCompareFunction = MTLCompareFunctionAlways;
    dd.depthWriteEnabled = NO;
    s_nods = [s_dev newDepthStencilStateWithDescriptor:dd];
    dd = [MTLDepthStencilDescriptor new];
    dd.depthCompareFunction = MTLCompareFunctionAlways;
    dd.depthWriteEnabled = YES;
    s_shcopyds = [s_dev newDepthStencilStateWithDescriptor:dd];
    td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm width:1 height:1 mipmapped:NO];
    s_black = [s_dev newTextureWithDescriptor:td];
    [s_black replaceRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0 withBytes:&zero bytesPerRow:1];
    s_lfbtex = [NSMutableArray new];
    s_frametex = [NSMutableArray new];
    {   /* the specular lobe's albedo table, once (brr_metal_lit.h, brdf_lut then brdf_avg) */
        static const MTLPixelFormat rg = MTLPixelFormatRG16Float;
        id<MTLRenderPipelineState> p[2] = { lit_pipe(@"lvs", @"brdf_lut", &rg, 1, 0, -1, -1), lit_pipe(@"lvs", @"brdf_avg", &rg, 1, 0, -1, -1) };
        id<MTLCommandBuffer> cb = [s_q commandBuffer];
        id<MTLTexture> t[2];
        int k;
        td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:rg width:64 height:64 mipmapped:NO];
        td.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
        td.storageMode = MTLStorageModePrivate;
        for (k = 0; k < 2; k++)
            t[k] = [s_dev newTextureWithDescriptor:td];
        for (k = 0; k < 2; k++) {
            MTLRenderPassDescriptor *rp = [MTLRenderPassDescriptor renderPassDescriptor];
            id<MTLRenderCommandEncoder> e;
            rp.colorAttachments[0].texture = t[k];
            rp.colorAttachments[0].loadAction = MTLLoadActionDontCare;
            rp.colorAttachments[0].storeAction = MTLStoreActionStore;
            e = [cb renderCommandEncoderWithDescriptor:rp];
            [e setRenderPipelineState:p[k]];
            if (k)
                [e setFragmentTexture:t[0] atIndex:0];
            [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
            [e endEncoding];
        }
        [cb commit];
        s_brdfE = t[1];
    }
}

static id<MTLRenderPipelineState> lit_forward(int src, int dst)
{
    static const MTLPixelFormat hf = MTLPixelFormatRGBA16Float;
    src &= 15;
    dst &= 15;
    if (!s_lpf[src][dst])
        s_lpf[src][dst] = lit_pipe(@"vs", @"fsf", &hf, 1, 1, src, dst);
    return s_lpf[src][dst];
}

static id<MTLTexture> lit_tex(MTLPixelFormat f, int w, int h)
{
    MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:f width:(NSUInteger)(w < 1 ? 1 : w)
                                                                                 height:(NSUInteger)(h < 1 ? 1 : h) mipmapped:NO];
    td.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    td.storageMode = MTLStorageModePrivate;
    return [s_dev newTextureWithDescriptor:td];
}

/* the lit frame's targets at the target's size (remade when it changes) */
static void lit_reset(void);
static void lit_targets(void)
{
    int i, w, h, up, lw, lh;
    float sc = brr_taa_scale(s_dev, s_rw, s_rh, (float)__atomic_load_n(&s_gpu_last_us, __ATOMIC_RELAXED) / 1000.0f, &up);
    lw = (int)lroundf((float)s_rw * sc);
    lh = (int)lroundf((float)s_rh * sc);
    if (lw < 16 || lh < 16 || !up) {
        lw = s_rw;
        lh = s_rh;
    }
    if (s_galb && (lw != s_lw || lh != s_lh || up != s_lup))
        lit_reset();                    /* the scale moved: the lit 3D's targets again (and TAA's history) */
    if (s_galb)
        return;
    s_lw = lw;
    s_lh = lh;
    s_lup = up;
    s_galb = lit_tex(MTLPixelFormatRGBA8Unorm, s_lw, s_lh);
    s_gpos = lit_tex(MTLPixelFormatRGBA32Float, s_lw, s_lh);
    s_gnrm = lit_tex(MTLPixelFormatRGBA16Float, s_lw, s_lh);
    s_hdr = lit_tex(MTLPixelFormatRGBA16Float, s_lw, s_lh);
    s_ldep = s_dep;
    if (s_lw != s_rw || s_lh != s_rh) {   /* depth of its own, read by MetalFX's scaler */
        MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                                                                     width:(NSUInteger)s_lw height:(NSUInteger)s_lh mipmapped:NO];
        td.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
        td.storageMode = MTLStorageModePrivate;
        s_ldep = [s_dev newTextureWithDescriptor:td];
    }
    for (i = 0, w = s_rw / 2, h = s_rh / 2; i < 6 && w >= 4 && h >= 4; i++, w /= 2, h /= 2)
        s_bl[i] = lit_tex(MTLPixelFormatRGBA16Float, w, h);
    s_nbl = i;
    if (!s_shadow) {   /* the live cascades the lighting reads, and the static casters' cache (brr_shadow.h) */
        MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                                                                     width:SHADOW_SIZE height:SHADOW_SIZE mipmapped:NO];
        td.textureType = MTLTextureType2DArray;
        td.arrayLength = CASCADES;
        td.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
        td.storageMode = MTLStorageModePrivate;
        s_shadow = [s_dev newTextureWithDescriptor:td];
        s_shcache = [s_dev newTextureWithDescriptor:td];
        memset(s_shc, 0, sizeof s_shc);
    }
}

/* a texture's lamplight, for the glow at night: its warm, bright, saturated
 * texels (a lit window's yellow, a lamp, a lens), a mask beside it; none for
 * a texture without any.  A material property of the texture, decided once
 * when it is made. */
static void lit_emask(uint32_t tid, const uint8_t *rgba, int w, int h)
{
    uint8_t *m;
    int i, any = 0, ng = 0;
    double col[3] = { 0, 0, 0 };
    s_emask[tid] = nil;
    if (!(m = (uint8_t *)malloc((size_t)w * (size_t)h)))
        return;
    for (i = 0; i < w * h; i++) {
        const uint8_t *p = &rgba[4 * i];
        int r = p[0], g = p[1], b = p[2];
        m[i] = (uint8_t)(p[3] >= 128 && r >= 180 && g >= 128 && r - b >= 60 ? 255 : 0);
        any |= m[i];
        if (m[i]) {
            ng++;
            col[0] += pow(r / 255.0, 2.2);
            col[1] += pow(g / 255.0, 2.2);
            col[2] += pow(b / 255.0, 2.2);
        }
    }
    if (tid < LIGHT_GLOW_TEX) {
        s_eshare[tid] = w * h > 0 ? (float)ng / (float)(w * h) : 0.0f;
        for (i = 0; i < 3; i++)
            s_ecol[tid][i] = ng ? (float)(col[i] / ng) : 1.0f;
    }
    if (any) {
        MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm
                                                                                     width:(NSUInteger)w height:(NSUInteger)h mipmapped:NO];
        td.usage = MTLTextureUsageShaderRead;
        s_emask[tid] = [s_dev newTextureWithDescriptor:td];
        [s_emask[tid] replaceRegion:MTLRegionMake2D(0, 0, (NSUInteger)w, (NSUInteger)h) mipmapLevel:0 withBytes:m bytesPerRow:(NSUInteger)w];
    }
    free(m);
}

/* a backdrop texture's sky mask (brr_skymask.h): made once, the first time
 * the texture is drawn as the backdrop, from its texels; s_black (all
 * scenery, the painting kept) without one */
static id<MTLTexture> lit_skymask(uint32_t tid)
{
    id<MTLTexture> t;
    uint8_t *px, *m;
    int *sky, w, h;
    if (tid >= TEX_MAX || !(t = s_tex[tid]))
        return s_black;
    if (s_skym[tid])
        return s_skym[tid];
    w = (int)t.width;
    h = (int)t.height;
    px = malloc((size_t)w * h * 4);
    m = malloc((size_t)w * h);
    sky = malloc((size_t)w * sizeof(int));
    if (px && m && sky) {
        MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm
                                                                                     width:(NSUInteger)w height:(NSUInteger)h mipmapped:NO];
        [t getBytes:px bytesPerRow:(NSUInteger)w * 4 fromRegion:MTLRegionMake2D(0, 0, (NSUInteger)w, (NSUInteger)h) mipmapLevel:0];
        brr_skymask(px, w, h, sky, m);
        td.usage = MTLTextureUsageShaderRead;
        s_skym[tid] = [s_dev newTextureWithDescriptor:td];
        [s_skym[tid] replaceRegion:MTLRegionMake2D(0, 0, (NSUInteger)w, (NSUInteger)h) mipmapLevel:0 withBytes:m bytesPerRow:(NSUInteger)w];
    }
    free(px);
    free(m);
    free(sky);
    return s_skym[tid] ? s_skym[tid] : s_black;
}

static Op *op_new(int kind)
{
    Op *o;
    if (s_nops == s_capops) {
        int c = s_capops ? s_capops * 2 : 1024;
        Op *p = realloc(s_ops, (size_t)c * sizeof *p);
        if (!p)
            return NULL;
        s_ops = p;
        s_capops = c;
    }
    o = &s_ops[s_nops++];
    memset(o, 0, sizeof *o);
    o->kind = kind;
    return o;
}

/* the frame's map, scissor and buffers for one recorded draw */
static void op_draw(id<MTLRenderCommandEncoder> e, const Op *o, int with_depth)
{
    int dw = s_dw ? s_dw : s_rw, dh = s_dh ? s_dh : s_rh;
    if (o->has_sc && dw == s_rw && dh == s_rh)
        [e setScissorRect:(MTLScissorRect){ (NSUInteger)o->sc[0], (NSUInteger)o->sc[1], (NSUInteger)o->sc[2], (NSUInteger)o->sc[3] }];
    else if (o->has_sc) {   /* the lit 3D at its internal size: the scissor scaled with it, outward to whole pixels */
        int x0 = (int)floor((double)o->sc[0] * dw / s_rw), y0 = (int)floor((double)o->sc[1] * dh / s_rh);
        int x1 = (int)ceil((double)(o->sc[0] + o->sc[2]) * dw / s_rw), y1 = (int)ceil((double)(o->sc[1] + o->sc[3]) * dh / s_rh);
        x1 = x1 > dw ? dw : x1;
        y1 = y1 > dh ? dh : y1;
        [e setScissorRect:(MTLScissorRect){ (NSUInteger)x0, (NSUInteger)y0, (NSUInteger)(x1 > x0 ? x1 - x0 : 0), (NSUInteger)(y1 > y0 ? y1 - y0 : 0) }];
    } else
        [e setScissorRect:(MTLScissorRect){ 0, 0, (NSUInteger)dw, (NSUInteger)dh }];
    if (with_depth)
        [e setDepthStencilState:ds_for(o->dmode, o->dfn, o->dmask)];
    [e setVertexBuffer:s_xtj ? s_xtj : s_xt[s_vbi] offset:0 atIndex:2];
    [e setVertexBuffer:s_vb[s_vbi] offset:(NSUInteger)o->voff atIndex:0];
    [e setVertexBytes:&o->u length:sizeof o->u atIndex:1];
    [e setFragmentBytes:&o->u length:sizeof o->u atIndex:0];
    if (o->u.has_tex) {
        NSArray *pair = s_frametex[(NSUInteger)o->texobj];
        [e setFragmentTexture:pair[0] atIndex:0];
        [e setFragmentTexture:pair[1] atIndex:1];
        if (pair.count > 2)
            [e setFragmentTexture:pair[2] atIndex:2];
        [e setFragmentSamplerState:samp_for(o->filt, o->cs, o->ct) atIndex:0];
    } else {
        [e setFragmentTexture:s_black atIndex:1];
    }
    [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:(NSUInteger)o->n];
}

/* the track's ray-tracing world (DESIGN.md 8.1; brr_metal_rt.m): built the
 * first time a pass asks for it on a track, again when the track changes.
 * Nothing traces rays yet; GI and reflections will. */
const BrrRtWorld *brr_metal_rt_world(void)
{
    const LightTrack *t = light_game_track();
    unsigned gen = s_rt.gen;
    int was = s_rt.built;
    if (!s_dev || !brr_rt_update(&s_rt, s_dev, s_q, t))
        return NULL;
    if (!was || gen != s_rt.gen)
        fprintf(stderr, "brr: ray tracing world: %d triangles, BVH %d nodes %.1f ms %zu KB, %s %.1f ms %zu KB\n",
                s_rt.ntri, s_rt.bvh.nnode, s_rt.bvh_ms, s_rt.bvh_bytes >> 10,
                s_rt.as ? (s_rt.hw ? "hardware structure" : "structure (shader-traced)") : "no structure",
                s_rt.as_ms, s_rt.as_bytes >> 10);
    return &s_rt;
}

/* the exposure: the post pass bins the frame's luminance into a histogram
 * (16 bins a stop, so a bin centre is within 1/32 EV of what it holds; one
 * of three buffers, read back three frames later when its frame is
 * done), each sample weighted toward the frame's centre (1 at the edge, 4 at
 * the centre: light_meter_weight); the mean log luminance of the weather's
 * percentile band (by day 5th to 95th: a centre-weighted average, the
 * outliers trimmed; the old 80th to 98.3rd band metered on the brightest
 * fifth and darkened the rest) less its compensation sets the camera's EV100 as a light meter
 * does (sensor calibration K = 12.5), adapting over about a second (Lagarde
 * and de Rousiers 2014) */
static float lit_exposure(int w)
{
    uint32_t *c;
    unsigned ns = 0;
    float target;
    int i;
    if (!s_hist[0])
        for (i = 0; i < 3; i++)
            s_hist[i] = [s_dev newBufferWithLength:LUM_BINS * 4 options:MTLResourceStorageModeShared];
    c = (uint32_t *)[s_hist[s_vbi] contents];
    for (i = 0; i < LUM_BINS; i++)
        ns += c[i];
    target = light_meter_ev(c, LUM_BINS, k_weather[w].plo, k_weather[w].phi, k_weather[w].evlo, k_weather[w].evhi, k_weather[w].comp);
    memset(c, 0, LUM_BINS * 4);
    if (s_ev > 1e8f)
        s_ev = target;
    s_ev += (target - s_ev) * 0.05f;
    if (getenv("BR_LIGHT_STATS") && s_frame % 600 == 0)
        fprintf(stderr, "light: frame %lu weather %d, %u meter samples, target EV %.2f, EV %.2f\n", s_frame, w, ns, target, s_ev);
    return 1.0f / (1.2f * exp2f(s_ev));
}

/* the frame's moving casters: its opaque 3D less what lies on the track
 * (light_capture_ontrack: the cascades' static cache holds that already) */
static void lit_dynamic(void)
{
    size_t i;
    s_ndyn = 0;
    if (s_capdyn < s_ntris) {
        float *p = realloc(s_dyn, s_ntris * 3 * sizeof(float));
        if (!p)
            return;
        s_dyn = p;
        s_capdyn = s_ntris;
    }
    for (i = 0; i + 3 <= s_ntris; i += 3)
        if (!s_trkn || !light_capture_ontrack(&s_tris[3 * i])) {
            memcpy(&s_dyn[3 * s_ndyn], &s_tris[3 * i], 9 * sizeof(float));
            s_ndyn += 3;
        }
}

/* the sun's shadow cascades (DESIGN.md 7.4; the fit in brr_shadow.h): the
 * camera's view out to SHADOW_FAR in four, each window reaching back toward
 * the sun to the farthest caster (the whole track and this frame's moving
 * objects).  Fills the lighting's constants and the cascades. */
static int lit_cascades(const float sun[3], const float eye[3], LU *u, BrrCascade cs[CASCADES])
{
    LightCamera cam;
    float f[3] = { -sun[0], -sun[1], -sun[2] }, len, ray[4][3], fwd[3], lo = 1e30f;
    simd_float4x4 a, ia;
    size_t i;
    int k, c;
    if (!light_capture_camera(&cam) || (s_ntris < 3 && !s_trkn))
        return 0;
    /* the view's corner rays and its forward axis, from the inverse of view x projection */
    memcpy(&a, cam.vp, sizeof a);                /* row vectors stored row-major: a x p is clip */
    ia = simd_inverse(a);
    for (c = 0; c < 5; c++) {
        float x = c == 4 ? 0.0f : (c & 1 ? 1.0f : -1.0f), y = c == 4 ? 0.0f : (c & 2 ? 1.0f : -1.0f), d[3];
        simd_float4 p0 = simd_mul(ia, simd_make_float4(x, y, 0.0f, 1.0f)), p1 = simd_mul(ia, simd_make_float4(x, y, 0.5f, 1.0f));
        for (k = 0; k < 3; k++)
            d[k] = p1[k] / p1[3] - p0[k] / p0[3];
        len = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        if (!(len > 0))
            return 0;
        for (k = 0; k < 3; k++)
            (c == 4 ? fwd : ray[c])[k] = d[k] / len;
    }
    if (fwd[0] * ray[0][0] + fwd[1] * ray[0][1] + fwd[2] * ray[0][2] < 0)   /* the rays point away from the eye */
        for (k = 0; k < 3; k++) {
            fwd[k] = -fwd[k];
            for (c = 0; c < 4; c++)
                ray[c][k] = -ray[c][k];
        }
    /* the casters' reach toward the sun: the track's extent and the moving casters */
    for (c = 0; c < 8 && s_trkn; c++) {
        float p[3] = { c & 1 ? s_trkhi[0] : s_trklo[0], c & 2 ? s_trkhi[1] : s_trklo[1], c & 4 ? s_trkhi[2] : s_trklo[2] };
        lo = fminf(lo, p[0] * f[0] + p[1] * f[1] + p[2] * f[2]);
    }
    for (i = 0; i < s_ndyn; i++) {
        const float *p = &s_dyn[3 * i];
        lo = fminf(lo, p[0] * f[0] + p[1] * f[1] + p[2] * f[2]);
    }
    brr_cascade_fit(sun, eye, (const float (*)[3])ray, fwd, lo, SHADOW_SIZE, cs);
    for (k = 0; k < 3; k++)
        u->cfwd[k] = fwd[k];
    for (c = 0; c < CASCADES; c++) {
        memcpy(u->sunvp[c], cs[c].m, sizeof cs[c].m);
        u->csplit[c] = cs[c].split;
        u->ctexel[c] = cs[c].texel;
        u->cbias[c] = cs[c].bias;
        u->cdepth[c] = cs[c].depth;
    }
    u->csoft[0] = tanf(SUN_HALF_DEG * 3.14159265f / 180.0f);
    u->csoft[1] = PCSS_SEARCH_MAX;
    u->csoft[2] = PCSS_FILTER_MAX;
    return 1;
}

/* BR_LIGHT_GPU=1: each lit pass's GPU time, from Metal's timestamp counters
 * at the encoders' stage boundaries (the samples Apple GPUs take), summed per
 * pass over a second and printed as milliseconds a frame */
#define GPU_PASSES 10
static const char *const k_pass_names[GPU_PASSES] = { "sky tables", "g-buffer", "sun shadow", "lamp shadows", "lighting",
                                                     "transparent", "bloom down", "bloom up", "post", "2d" };
static id<MTLCounterSampleBuffer> s_gpuctr[3];
static int s_gpu_on = -1;
static double s_gpu_ms[GPU_PASSES], s_gpu_span, s_gpu_t0;
static long s_gpu_cb_us, s_gpu_cb_n;   /* the frames' whole GPU time (command buffer start to end) */
#define GPU_SPLITS 9
static const char *const k_split_names[GPU_SPLITS] = { "tables+gbuffer+sun", "lamp shadows", "gi update", "lighting", "reflections",
                                                      "fog", "transparent", "taa", "bloom+post+2d" };
static long s_split_us[GPU_SPLITS], s_split_n[GPU_SPLITS];
static unsigned s_gpu_frames;
static MTLTimestamp s_cpu0, s_gpu0;
static double s_tick_ns = 1.0;
static uint8_t s_gpu_used[3][GPU_PASSES];
static uint8_t s_gpu_frame_used[GPU_PASSES];

static id<MTLRenderCommandEncoder> lit_encoder(MTLRenderPassDescriptor *rp, int pass)
{
    if (s_gpu_on < 0) {
        s_gpu_on = getenv("BR_LIGHT_GPU") != NULL && [s_dev supportsCounterSampling:MTLCounterSamplingPointAtStageBoundary];
        if (s_gpu_on) {
            MTLCounterSampleBufferDescriptor *d = [MTLCounterSampleBufferDescriptor new];
            int k;
            for (id<MTLCounterSet> cs in s_dev.counterSets)
                if ([cs.name isEqualToString:MTLCommonCounterSetTimestamp])
                    d.counterSet = cs;
            d.sampleCount = 2 * 64;
            d.storageMode = MTLStorageModeShared;
            for (k = 0; k < 3 && d.counterSet; k++)
                s_gpuctr[k] = [s_dev newCounterSampleBufferWithDescriptor:d error:nil];
            if (!s_gpuctr[0])
                s_gpu_on = 0;
            else
                [s_dev sampleTimestamps:&s_cpu0 gpuTimestamp:&s_gpu0];
        }
    }
    if (s_gpu_on && pass >= 0 && pass < GPU_PASSES && !s_gpu_frame_used[pass]) {   /* a pass's first encoder */
        rp.sampleBufferAttachments[0].sampleBuffer = s_gpuctr[s_vbi];
        rp.sampleBufferAttachments[0].startOfVertexSampleIndex = (NSUInteger)(2 * pass);
        rp.sampleBufferAttachments[0].endOfVertexSampleIndex = MTLCounterDontSample;
        rp.sampleBufferAttachments[0].startOfFragmentSampleIndex = MTLCounterDontSample;
        rp.sampleBufferAttachments[0].endOfFragmentSampleIndex = (NSUInteger)(2 * pass + 1);
        s_gpu_frame_used[pass] = 1;
    }
    return [s_cmd renderCommandEncoderWithDescriptor:rp];
}

/* the samples of the frame that last used this slot (three frames ago, done) */
static void lit_gpu_read(void)
{
    int k;
    if (s_gpu_on <= 0)
        return;
    {
        NSData *d = [s_gpuctr[s_vbi] resolveCounterRange:NSMakeRange(0, 2 * GPU_PASSES)];
        const MTLCounterResultTimestamp *t = d ? (const MTLCounterResultTimestamp *)d.bytes : NULL;
        /* the GPU overlaps one pass's start with the last one's fragments
         * (the frame's first pass with the last frame's last), so a pass
         * costs from the later of its start and the previous pass's end to
         * its own end (the passes' ends follow the order they ran in; the
         * slots are read in frame order) */
        if (t) {
            static uint64_t prev;
            uint64_t first = 0, last = 0;
            for (k = 0; k < GPU_PASSES; k++) {
                uint64_t a0 = t[2 * k].timestamp, a1 = t[2 * k + 1].timestamp;
                if (!s_gpu_used[s_vbi][k] || a0 == MTLCounterErrorValue || a1 == MTLCounterErrorValue || a1 < a0)
                    continue;
                if (prev > a0)
                    a0 = prev < a1 ? prev : a1;
                s_gpu_ms[k] += (double)(a1 - a0) * s_tick_ns / 1e6;
                if (!first)
                    first = a0;
                last = prev = a1;
            }
            if (last > first)
                s_gpu_span += (double)(last - first) * s_tick_ns / 1e6;
            s_gpu_frames++;
        }
    }
    {
        double now = CACurrentMediaTime();
        if (!s_gpu_t0)
            s_gpu_t0 = now;
        if (now - s_gpu_t0 >= 1.0 && s_gpu_frames) {
            MTLTimestamp c1, g1;
            double total = 0;
            [s_dev sampleTimestamps:&c1 gpuTimestamp:&g1];   /* GPU ticks to nanoseconds */
            if (g1 > s_gpu0)
                s_tick_ns = (double)(c1 - s_cpu0) / (double)(g1 - s_gpu0);
            fprintf(stderr, "light gpu (ms a frame, %u frames):", s_gpu_frames);
            for (k = 0; k < GPU_PASSES; k++) {
                fprintf(stderr, " %s %.2f", k_pass_names[k], s_gpu_ms[k] / s_gpu_frames);
                total += s_gpu_ms[k] / s_gpu_frames;
            }
            {
                long us = __atomic_exchange_n(&s_gpu_cb_us, 0, __ATOMIC_RELAXED), n = __atomic_exchange_n(&s_gpu_cb_n, 0, __ATOMIC_RELAXED);
                fprintf(stderr, " | total %.2f, first start to last end %.2f, whole frame %.2f, %ux%u\n", total, s_gpu_span / s_gpu_frames,
                        n ? us / 1000.0 / n : 0.0, (unsigned)s_rw, (unsigned)s_rh);
                if (s_split_n[0]) {
                    fprintf(stderr, "light gpu modules (ms a frame):");
                    for (k = 0; k < GPU_SPLITS; k++) {
                        long su = __atomic_exchange_n(&s_split_us[k], 0, __ATOMIC_RELAXED), sn = __atomic_exchange_n(&s_split_n[k], 0, __ATOMIC_RELAXED);
                        fprintf(stderr, " %s %.2f", k_split_names[k], sn ? su / 1000.0 / sn : 0.0);
                    }
                    fprintf(stderr, "\n");
                }
            }
            memset(s_gpu_ms, 0, sizeof s_gpu_ms);
            s_gpu_span = 0;
            s_gpu_frames = 0;
            s_gpu_t0 = now;
        }
    }
}

/* ---- many lights (DESIGN.md 7.4): the clusters and the lamps' shadow atlas -------------
 * Each frame the scene's lights (light_scene.c) go into this frame's light set
 * (light_cluster.h's LightSet, which brr_metal_lights.h's lamps() reads): the
 * clusters' lists for the camera, and the shadow plan, whose tiles are drawn
 * into the atlas after the sun's cascades: each cleared, then the track's
 * triangles in the lamp's frustum (binned at load) and this frame's moving
 * objects (lit_dynamic's: the captured 3D not on the track).  The iPhone's atlas is a quarter of the Mac's. */
#if TARGET_OS_IPHONE
#define LS_ATLAS 2048
#else
#define LS_ATLAS 4096
#endif
#define LS_BUDGET ((long)LS_ATLAS * LS_ATLAS)     /* texels drawn a frame: the whole atlas */
static id<MTLBuffer> s_lset[3], s_lcib[3];
static int s_nlamp;                  /* lamps in this frame's set (the probes light their hits by them) */
static id<MTLTexture> s_latl;
static int s_latl_new;
static id<MTLRenderPipelineState> s_lcclr, s_lccas;
static id<MTLDepthStencilState> s_lcds_all, s_lcds_less;
static uint32_t *s_lcidx;               /* this frame's track casters, tile after tile */
static size_t s_lccap, s_lcn;
static size_t s_lctile[LC_LIGHTS][2];   /* each plan tile's casters: first, count */
static LightShadowPlan s_lplan;

/* the frame's lights, their clusters for the camera vp (NULL: no 3D this
 * frame, no lights) and the shadow plan, into this frame's light set */
static void lit_lights(const float eye[3], const float *vp)
{
    static const float ident[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
    LightSet *set;
    LightClusterStats st;
    int ids[LC_LIGHTS], n = 0, k;
    const LightTrack *t;
    if (!s_lset[0]) {
        MTLTextureDescriptor *td;
        MTLRenderPipelineDescriptor *pd;
        MTLDepthStencilDescriptor *dd;
        NSError *err = nil;
        for (k = 0; k < 3; k++)
            s_lset[k] = [s_dev newBufferWithLength:sizeof(LightSet) options:MTLResourceStorageModeShared];
        td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float width:LS_ATLAS height:LS_ATLAS
                                                            mipmapped:NO];
        td.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
        td.storageMode = MTLStorageModePrivate;
        s_latl = [s_dev newTextureWithDescriptor:td];
        s_latl_new = 1;
        light_shadow_reset();
        pd = [MTLRenderPipelineDescriptor new];
        pd.vertexFunction = [s_lib newFunctionWithName:@"lcclr"];
        pd.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
        s_lcclr = [s_dev newRenderPipelineStateWithDescriptor:pd error:&err];
        pd.vertexFunction = [s_lib newFunctionWithName:@"lcvs"];
        s_lccas = [s_dev newRenderPipelineStateWithDescriptor:pd error:&err];
        if (!s_lcclr || !s_lccas)
            fprintf(stderr, "brr: metal lamp shadow pipelines: %s\n", [[err description] UTF8String]);
        dd = [MTLDepthStencilDescriptor new];
        dd.depthCompareFunction = MTLCompareFunctionAlways;
        dd.depthWriteEnabled = YES;
        s_lcds_all = [s_dev newDepthStencilStateWithDescriptor:dd];
        dd.depthCompareFunction = MTLCompareFunctionLessEqual;
        s_lcds_less = [s_dev newDepthStencilStateWithDescriptor:dd];
    }
    set = (LightSet *)[s_lset[s_vbi] contents];
    if (vp)
        n = light_scene_lights(set->l, ids, LC_LIGHTS);
    s_nlamp = n;
    light_cluster_build(set, n, vp ? vp : ident, eye, &st);
    t = light_game_track();
    if (t)
        light_shadow_track(t->tri, t->ntri, t->gen);
    else
        light_shadow_track(NULL, 0, 0);
    light_shadow_plan(set, ids, &st, eye, s_dyn, (int)s_ndyn, LS_ATLAS, LS_BUDGET, &s_lplan);
    /* the track's casters of each tile drawn this frame */
    s_lcn = 0;
    for (k = 0; k < s_lplan.n; k++) {
        const LightShadowTile *tl = &s_lplan.t[k];
        size_t need = s_lcn + (size_t)(t ? t->ntri : 0) * 3;
        s_lctile[k][0] = s_lcn;
        s_lctile[k][1] = 0;
        if (!tl->render || !t || !s_trkn)
            continue;
        if (need > s_lccap) {
            size_t c = s_lccap ? s_lccap : 65536;
            uint32_t *q;
            while (c < need)
                c *= 2;
            if (!(q = realloc(s_lcidx, c * sizeof *q)))
                continue;
            s_lcidx = q;
            s_lccap = c;
        }
        s_lctile[k][1] = (size_t)light_shadow_track_casters(&set->s[tl->light], &set->l[tl->light], s_lcidx + s_lcn,
                                                            (int)(s_lccap - s_lcn));
        s_lcn += s_lctile[k][1];
    }
    if (s_lcn && (!s_lcib[s_vbi] || [s_lcib[s_vbi] length] < s_lcn * 4))
        s_lcib[s_vbi] = [s_dev newBufferWithLength:s_lccap * 4 options:MTLResourceStorageModeShared];
    if (s_lcn)
        memcpy([s_lcib[s_vbi] contents], s_lcidx, s_lcn * 4);
    if (getenv("BR_LIGHT_STATS") && s_frame % 600 == 0)
        fprintf(stderr, "lights: frame %lu, %d lights (%d seen), %ld cluster entries (longest list %d); lamp shadows %d tiles: "
                "%d drawn (%ld texels, %zu track triangles), %d cached, %d stale, %d unshadowed%s\n",
                s_frame, st.lights, st.visible, st.entries, st.maxlist, s_lplan.n, s_lplan.rendered, s_lplan.texels, s_lcn / 3,
                s_lplan.cached, s_lplan.stale, s_lplan.dropped, s_lplan.repacked ? ", repacked" : "");
}

/* the lamps' shadow tiles the plan draws this frame */
static void lit_lamp_shadows(void)
{
    const LightSet *set = (const LightSet *)[s_lset[s_vbi] contents];
    MTLRenderPassDescriptor *rp;
    id<MTLRenderCommandEncoder> e;
    id<MTLBuffer> dyn = nil;
    int k;
    for (k = 0; k < s_lplan.n && !s_lplan.t[k].render; k++)
        ;
    if (k == s_lplan.n)
        return;
    if (s_ndyn)
        dyn = [s_dev newBufferWithBytes:s_dyn length:s_ndyn * 3 * sizeof(float) options:MTLResourceStorageModeShared];
    rp = [MTLRenderPassDescriptor renderPassDescriptor];
    rp.depthAttachment.texture = s_latl;
    rp.depthAttachment.loadAction = s_latl_new ? MTLLoadActionClear : MTLLoadActionLoad;
    rp.depthAttachment.clearDepth = 1.0;
    rp.depthAttachment.storeAction = MTLStoreActionStore;
    s_latl_new = 0;
    e = lit_encoder(rp, 3);   /* lamp shadows */
    [e setCullMode:MTLCullModeNone];
    for (k = 0; k < s_lplan.n; k++) {
        const LightShadowTile *t = &s_lplan.t[k];
        if (!t->render)
            continue;
        [e setViewport:(MTLViewport){ t->x, t->y, t->size, t->size, 0.0, 1.0 }];
        [e setScissorRect:(MTLScissorRect){ (NSUInteger)t->x, (NSUInteger)t->y, (NSUInteger)t->size, (NSUInteger)t->size }];
        [e setRenderPipelineState:s_lcclr];
        [e setDepthStencilState:s_lcds_all];
        [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        [e setRenderPipelineState:s_lccas];
        [e setDepthStencilState:s_lcds_less];
        [e setVertexBytes:set->s[t->light].m length:sizeof set->s[t->light].m atIndex:1];
        if (s_lctile[k][1]) {
            [e setVertexBuffer:s_trkbuf offset:0 atIndex:0];
            [e drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:s_lctile[k][1] indexType:MTLIndexTypeUInt32
                         indexBuffer:s_lcib[s_vbi] indexBufferOffset:s_lctile[k][0] * 4];
        }
        if (dyn) {
            [e setVertexBuffer:dyn offset:0 atIndex:0];
            [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:s_ndyn];
        }
    }
    [e endEncoding];
}

/* diffuse GI (DESIGN.md 8.2; brr_metal_gi.h), on unless BR_LIGHT_GI=0 (the
 * project lead's choice, 2026-10-13, pending the tier decision): a 16 x 8 x 16 volume of probes 8 m apart
 * around the eye, 64 rays a probe a frame, its hits lit by the directional
 * light (sun or moon), the sky and the frame's lamps; u->gi.x on when it ran */
static BrrGi s_gi;
static void lit_gi(LU *u, const float eye[3])
{
    static int on = -1;
    const BrrRtWorld *w;
    float sky[3], gnd[3];
    int k;
    u->gi[0] = 0.0f;
    if (on < 0)
        on = !getenv("BR_LIGHT_GI") || atoi(getenv("BR_LIGHT_GI")) > 0;
    if (!on || !(w = brr_metal_rt_world()))
        return;
    if (!s_gi.ready && !brr_gi_init(&s_gi, s_dev, 16, 8, 16, 64)) {
        fprintf(stderr, "brr: irradiance probes: no pipelines, off\n");
        on = 0;
        return;
    }
    for (k = 0; k < 3; k++) {
        sky[k] = u->skyc[k] / 3.14159265f;      /* a miss's radiance: the sky's irradiance over pi */
        gnd[k] = u->gndc[k] / 3.14159265f;
    }
    brr_gi_frame(&s_gi, eye, 8.0f, u->sun, u->sunc, sky, gnd);
    brr_gi_encode(&s_gi, s_cmd, w, s_lset[s_vbi], s_nlamp);
    u->gi[0] = 1.0f;
}
/* the probes for the lighting's fragment stage: the volume at buffer idx,
 * irradiance and distance at textures 6 and 7 (placeholders when off) */
static void lit_gi_bind(id<MTLRenderCommandEncoder> e, int idx)
{
    static const BrrGiU zero;
    int on = s_gi.ready && s_gi.frame;
    [e setFragmentBytes:on ? &s_gi.u : &zero length:sizeof zero atIndex:(NSUInteger)idx];
    [e setFragmentTexture:on ? brr_gi_irr(&s_gi) : s_brdfE atIndex:6];
    [e setFragmentTexture:on ? brr_gi_dst(&s_gi) : s_brdfE atIndex:7];
}

/* BR_LIGHT_GPU=2 (diagnostic): the frame's work committed in pieces at its
 * modules' boundaries, each piece's GPU time (Metal's command buffer start and
 * end) summed per module and printed with the pass timer's line.  Changes the
 * frame's scheduling: for measuring only. */
static double s_split_end;   /* the previous piece's GPU end (completion handlers run in queue order) */
static void lit_split_add(id<MTLCommandBuffer> cb, int k)
{
    /* a piece costs from the later of its own start and the previous piece's
       end to its own end: a piece's start includes waiting behind the last */
    [cb addCompletedHandler:^(id<MTLCommandBuffer> c) {
        double t0 = c.GPUStartTime > s_split_end ? c.GPUStartTime : s_split_end, ms = (c.GPUEndTime - t0) * 1000.0;
        s_split_end = c.GPUEndTime;
        if (ms >= 0 && ms < 1000) {
            __atomic_add_fetch(&s_split_us[k], (long)(ms * 1000.0), __ATOMIC_RELAXED);
            __atomic_add_fetch(&s_split_n[k], 1, __ATOMIC_RELAXED);
        }
    }];
}
static void lit_split(int k)
{
    if (s_gpu_on <= 0 || !getenv("BR_LIGHT_GPU") || atoi(getenv("BR_LIGHT_GPU")) != 2)
        return;
    lit_split_add(s_cmd, k);
    [s_cmd commit];
    s_cmd = [s_q commandBuffer];
}

static void lit_present(void)
{
    LU u;
    /* the sun (or moon) direction, toward it, Z up: Coastline's, chosen
       2026-10-12 (high, behind the start: DESIGN.md 7.3's manifest will carry it per track) */
    float eye[3] = { 0, 0, 0 }, sun[3] = { -1.0f, -1.5f, 2.5f }, len;
    int w = light_game_weather(), k, i, first3d, have_cam, sky;
    LightCamera cam;
    KU ku;
    BrrCascade cs[CASCADES];
    id<MTLRenderCommandEncoder> e;
    MTLRenderPassDescriptor *rp;
    float taa_s[16];
    id<MTLTexture> hdr = s_hdr;
    id<MTLBuffer> xtj;
    s_xtj = nil;
    lit_targets();
    lit_gpu_read();
    memset(s_gpu_frame_used, 0, sizeof s_gpu_frame_used);
    memset(&u, 0, sizeof u);
    {   /* BR_LIGHT_SUN=x,y,z: the sun's direction (towards it; Z up), until the tracks' manifests carry it */
        const char *e2 = getenv("BR_LIGHT_SUN");
        if (e2)
            sscanf(e2, "%f,%f,%f", &sun[0], &sun[1], &sun[2]);
    }
    light_capture_eye(eye);
    if (k_weather[w].elev > 0) {   /* the moon: the sun's bearing, at its own elevation */
        float hz = sqrtf(sun[0] * sun[0] + sun[1] * sun[1]), el = k_weather[w].elev * 0.017453293f;
        if (hz > 0) {
            sun[0] = sun[0] / hz * cosf(el);
            sun[1] = sun[1] / hz * cosf(el);
        }
        sun[2] = sinf(el);
    }
    len = sqrtf(sun[0] * sun[0] + sun[1] * sun[1] + sun[2] * sun[2]);
    for (k = 0; k < 3; k++) {
        sun[k] /= len;
        u.eye[k] = eye[k];
        u.sun[k] = sun[k];
        u.sunc[k] = k_weather[w].sunc[k] * k_weather[w].sun;
        u.skyc[k] = k_weather[w].skyc[k] * k_weather[w].sky;
        u.gndc[k] = k_weather[w].gndc[k] * k_weather[w].gnd;
    }
    {   /* white balance: to the sun and sky together, or at night to the headlamps */
        static const float halogen[3] = { 1.00f, 0.83f, 0.62f };
        float wb[3], y;
        for (k = 0; k < 3; k++)
            wb[k] = w == 4 ? halogen[k] : k_weather[w].sunc[k] * k_weather[w].sun * 0.5f + k_weather[w].skyc[k] * k_weather[w].sky;
        y = 0.2126f * wb[0] + 0.7152f * wb[1] + 0.0722f * wb[2];
        for (k = 0; k < 3; k++)
            u.wb[k] = y / wb[k];
    }
    u.misc[0] = lit_exposure(w);
    u.misc[2] = k_weather[w].rough;
    u.wet[0] = k_weather[w].wet; u.wet[1] = k_weather[w].pud; u.wet[2] = (float)brr_refl_tier(); u.wet[3] = REFL_RT_ROUGH;
    u.bg[0] = 0.9f;                         /* the backdrop and fog: shown as the game drew them (pre-exposed) */
    u.bg[1] = s_nbl ? 1.0f / (float)s_nbl : 0.0f;
    u.bg[2] = k_grade[w][0];                /* the grade after the tonemap: saturation, contrast (brr_metal_lit.h) */
    u.bg[3] = k_grade[w][1];
    {
        const char *e2 = getenv("BR_LIGHT_VIEW");
        u.misc[3] = e2 ? (float)atoi(e2) : 0.0f;
    }
    {   /* the track's triangles, once a track: the shadows' static casters */
        const LightTrack *t = light_game_track();
        if (t && t->gen != s_trkgen && t->ntri) {
            s_trkbuf = [s_dev newBufferWithBytes:t->tri length:(size_t)t->ntri * 9 * sizeof(float) options:MTLResourceStorageModeShared];
            s_trkn = t->ntri * 3;
            s_trkgen = t->gen;
            memcpy(s_trklo, t->lo, sizeof s_trklo);
            memcpy(s_trkhi, t->hi, sizeof s_trkhi);
        } else if (!t) {
            s_trkn = 0;
        }
    }
    lit_dynamic();
    u.misc[1] = (float)lit_cascades(sun, eye, &u, cs);
    have_cam = light_capture_camera(&cam);
    if (have_cam && u.misc[1] > 0.5f && !getenv("BR_LIGHT_NOCONTACT")) {
        /* the contact shadows' map from world to the target's pixels: the
           main 3D view's screen map entry (its first opaque draw's), the game's
           viewport and its view x projection */
        int xf = 0;
        for (i = 0; i < s_nops; i++)
            if (s_ops[i].kind == K_OPQ) {
                xf = (int)s_ops[i].u.xf;
                break;
            }
        brr_screen_matrix(cam.vp, cam.sx, cam.ox, cam.sy, cam.oy, (float)light_glide_flip(),
                          (const float *)[s_xt[s_vbi] contents] + 4 * xf, (float)s_lw, (float)s_lh, u.scr);
        u.csoft[3] = CONTACT_LEN;
    }
    lit_lights(eye, have_cam ? cam.vp : NULL);
    u.vol[0] = have_cam && brr_vol_on(s_dev) ? 1.0f : 0.0f;   /* the volumetric fog in place of the game's */
    if (have_cam) {   /* TAA (brr_metal_taa.m): this frame's unjittered world-to-pixel matrix, and the jittered screen map */
        int xf = 0;
        for (i = 0; i < s_nops; i++)
            if (s_ops[i].kind == K_OPQ) {
                xf = (int)s_ops[i].u.xf;
                break;
            }
        brr_screen_matrix(cam.vp, cam.sx, cam.ox, cam.sy, cam.oy, (float)light_glide_flip(),
                          (const float *)[s_xt[s_vbi] contents] + 4 * xf, (float)s_lw, (float)s_lh, taa_s);
        s_xtj = brr_taa_begin(s_dev, s_xt[s_vbi], s_vbi, s_nxf[s_vbi], s_lw, s_lh, NULL);   /* jitter in the lit 3D's pixels */
    }
    sky = have_cam && !getenv("BR_LIGHT_NOSKY");
    if (sky) {
        /* the atmosphere's tables: transmittance and multiple scattering when the
           weather's haze changes, the sky view and its light every frame */
        AU a;
        float *ir;
        memset(&a, 0, sizeof a);
        memcpy(a.sun, sun, 12);
        a.cam[0] = fmaxf(eye[2], 0.0f) / 1000.0f + 0.002f;
        a.atm[0] = k_sky[w].mie;
        if (!s_atmT) {
            MTLTextureDescriptor *td;
            s_atmT = lit_tex(MTLPixelFormatRGBA16Float, 256, 64);
            s_atmM = lit_tex(MTLPixelFormatRGBA16Float, 32, 32);
            s_atmS = lit_tex(MTLPixelFormatRGBA16Float, 192, 108);
            for (k = 0; k < 3; k++) {
                td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float width:3 height:1 mipmapped:NO];
                td.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
                td.storageMode = MTLStorageModeShared;
                s_atmI[k] = [s_dev newTextureWithDescriptor:td];
            }
        }
        {   /* last use of this slot (three frames ago, done): the sky's light, the sun's
               transmittance at the camera, the horizon's light */
            float px[12];
            [s_atmI[s_vbi] getBytes:px bytesPerRow:48 fromRegion:MTLRegionMake2D(0, 0, 3, 1) mipmapLevel:0];
            ir = px;
            if (px[0] > 0.0f || px[4] > 0.0f) {
                for (k = 0; k < 3; k++) {
                    u.sunc[k] = k_sky[w].top * k_sky[w].through * ir[4 + k];
                    u.skyc[k] = k_sky[w].top * k_sky[w].skyk * ir[k];
                    u.fogc[k] = k_sky[w].top * k_sky[w].skyk * ir[8 + k];
                }
                for (k = 0; k < 3; k++)          /* the ground's light back up: albedo 0.2 of what falls on it */
                    u.gndc[k] = 0.2f * (u.sunc[k] * fmaxf(sun[2], 0.0f) + u.skyc[k]);
                u.fogc[3] = 1.0f;
            }
        }
        {
            id<MTLTexture> tg[4] = { s_atmT, s_atmM, s_atmS, s_atmI[s_vbi] };
            id<MTLRenderPipelineState> pp[4] = { s_patmT, s_patmM, s_patmS, s_patmI };
            for (k = s_atm_mie == a.atm[0] ? 2 : 0; k < 4; k++) {
                rp = [MTLRenderPassDescriptor renderPassDescriptor];
                rp.colorAttachments[0].texture = tg[k];
                rp.colorAttachments[0].loadAction = MTLLoadActionDontCare;
                rp.colorAttachments[0].storeAction = MTLStoreActionStore;
                e = lit_encoder(rp, 0);   /* sky tables */
                [e setRenderPipelineState:pp[k]];
                [e setFragmentBytes:&a length:sizeof a atIndex:0];
                [e setFragmentTexture:k == 3 ? s_atmS : s_atmT atIndex:0];
                [e setFragmentTexture:k == 3 ? s_atmT : s_atmM atIndex:1];
                [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
                [e endEncoding];
            }
            s_atm_mie = a.atm[0];
        }
        {   /* the sky's camera rays: the main 3D view's screen map, the game's viewport, the
               inverse of its view x projection's x, y and w columns */
            const float *m = cam.vp;
            double aa[3][3], det, inv[3][3];
            int r2, c2, xf = 0;
            for (i = 0; i < s_nops; i++)
                if (s_ops[i].kind == K_OPQ) {
                    xf = (int)s_ops[i].u.xf;
                    break;
                }
            memcpy(ku.xft, (const float *)[s_xt[s_vbi] contents] + 4 * xf, 16);
            ku.vpt[0] = cam.sx; ku.vpt[1] = cam.ox; ku.vpt[2] = cam.sy; ku.vpt[3] = cam.oy;
            for (r2 = 0; r2 < 3; r2++)
                for (c2 = 0; c2 < 3; c2++)
                    aa[r2][c2] = m[c2 * 4 + (r2 == 2 ? 3 : r2)];
            det = aa[0][0] * (aa[1][1] * aa[2][2] - aa[1][2] * aa[2][1]) - aa[0][1] * (aa[1][0] * aa[2][2] - aa[1][2] * aa[2][0])
                + aa[0][2] * (aa[1][0] * aa[2][1] - aa[1][1] * aa[2][0]);
            if (fabs(det) < 1e-30)
                det = 1e-30;
            inv[0][0] = (aa[1][1] * aa[2][2] - aa[1][2] * aa[2][1]) / det;
            inv[0][1] = (aa[0][2] * aa[2][1] - aa[0][1] * aa[2][2]) / det;
            inv[0][2] = (aa[0][1] * aa[1][2] - aa[0][2] * aa[1][1]) / det;
            inv[1][0] = (aa[1][2] * aa[2][0] - aa[1][0] * aa[2][2]) / det;
            inv[1][1] = (aa[0][0] * aa[2][2] - aa[0][2] * aa[2][0]) / det;
            inv[1][2] = (aa[0][2] * aa[1][0] - aa[0][0] * aa[1][2]) / det;
            inv[2][0] = (aa[1][0] * aa[2][1] - aa[1][1] * aa[2][0]) / det;
            inv[2][1] = (aa[0][1] * aa[2][0] - aa[0][0] * aa[2][1]) / det;
            inv[2][2] = (aa[0][0] * aa[1][1] - aa[0][1] * aa[1][0]) / det;
            for (c2 = 0; c2 < 3; c2++) {
                ku.m0[c2] = (float)inv[0][c2];
                ku.m1[c2] = (float)inv[1][c2];
                ku.m2[c2] = (float)inv[2][c2];
            }
            ku.off[0] = m[12]; ku.off[1] = m[13]; ku.off[2] = m[15];
            memcpy(ku.eye, eye, 12);
            ku.eye[3] = a.cam[0];
            ku.tgt[0] = (float)s_lw; ku.tgt[1] = (float)s_lh;
            ku.tgt[3] = (float)light_glide_flip();
            memcpy(ku.sun, sun, 12);
            ku.ktop[0] = k_sky[w].top;
            ku.ktop[1] = k_sky[w].through;
            ku.ktop[2] = k_sky[w].skyk;
            ku.ktop[3] = u.misc[3];            /* the debug view */
            ku.cld[0] = k_sky[w].cover;
            ku.cld[1] = k_sky[w].dens;
            ku.cld[2] = (float)fmod((double)s_frame / 60.0, 3600.0);   /* the drift's clock, seconds */
            ku.cld[3] = getenv("BR_LIGHT_NOCLOUDS") ? 0.0f : 1.0f;    /* the cloud layer's tier flag */
            if (getenv("BR_LIGHT_STATS") && s_frame % 600 == 0)
                fprintf(stderr, "sky: xft %g %g %g %g vpt %g %g %g %g m0 %g %g %g m1 %g %g %g m2 %g %g %g off %g %g %g eye %g %g %g tgt %g %g sun %g %g %g irr %g %g %g\n",
                        ku.xft[0], ku.xft[1], ku.xft[2], ku.xft[3], ku.vpt[0], ku.vpt[1], ku.vpt[2], ku.vpt[3],
                        ku.m0[0], ku.m0[1], ku.m0[2], ku.m1[0], ku.m1[1], ku.m1[2], ku.m2[0], ku.m2[1], ku.m2[2],
                        ku.off[0], ku.off[1], ku.off[2], ku.eye[0], ku.eye[1], ku.eye[2], ku.tgt[0], ku.tgt[1],
                        ku.sun[0], ku.sun[1], ku.sun[2], u.skyc[0], u.skyc[1], u.skyc[2]);
            ku.tgt[2] = u.misc[0];             /* the exposure: the sky is written pre-exposed */
        }
    }
    if (getenv("BR_LIGHT_BACKLOG") && s_frame == (unsigned long)atoi(getenv("BR_LIGHT_BACKLOG")))
        for (i = 0; i < s_nops; i++)
            if (s_ops[i].kind == K_BACK)
            {
                if (getenv("BR_LIGHT_TEXDUMP") && s_ops[i].u.has_tex) {   /* the texture's texels, RGBA, to DIR/<id>.rgba */
                    NSArray *pair = s_frametex[(NSUInteger)s_ops[i].texobj];
                    id<MTLTexture> t = pair[0];
                    size_t nb = t.width * t.height * 4;
                    uint8_t *px = malloc(nb);
                    char path[1024];
                    FILE *f;
                    [t getBytes:px bytesPerRow:t.width * 4 fromRegion:MTLRegionMake2D(0, 0, t.width, t.height) mipmapLevel:0];
                    snprintf(path, sizeof path, "%s/%u_%lux%lu.rgba", getenv("BR_LIGHT_TEXDUMP"), s_ops[i].tex,
                             (unsigned long)t.width, (unsigned long)t.height);
                    if ((f = fopen(path, "wb"))) {
                        fwrite(px, 1, nb, f);
                        fclose(f);
                    }
                    free(px);
                }
                fprintf(stderr, "backdrop op %d: tex %u %dx%d, %d vertices, blend %d/%d, scissor %d,%d %dx%d\n", i, s_ops[i].tex,
                        s_ops[i].tw, s_ops[i].th, s_ops[i].n, s_ops[i].src, s_ops[i].dst, s_ops[i].sc[0], s_ops[i].sc[1],
                        s_ops[i].sc[2], s_ops[i].sc[3]);
            }

    if (u.fogc[3] > 0.5f && w != 4) {  /* daylight white balance: to the sun's own light, as a camera's
                                          daylight setting: the sky stays blue, shade a little blue */
        float wb[3], y;
        for (k = 0; k < 3; k++)
            wb[k] = u.sunc[k] > 0.0f ? u.sunc[k] : u.skyc[k];
        y = 0.2126f * wb[0] + 0.7152f * wb[1] + 0.0722f * wb[2];
        for (k = 0; k < 3; k++)
            u.wb[k] = wb[k] > 0.0f ? y / wb[k] : 1.0f;
    }

    /* the G-buffer: the camera's opaque 3D */
    rp = [MTLRenderPassDescriptor renderPassDescriptor];
    {
        id<MTLTexture> g[3] = { s_galb, s_gpos, s_gnrm };
        for (k = 0; k < 3; k++) {
            rp.colorAttachments[k].texture = g[k];
            rp.colorAttachments[k].loadAction = MTLLoadActionClear;
            rp.colorAttachments[k].clearColor = MTLClearColorMake(0, 0, 0, 0);
            rp.colorAttachments[k].storeAction = MTLStoreActionStore;
        }
    }
    rp.depthAttachment.texture = s_ldep;
    rp.depthAttachment.loadAction = MTLLoadActionClear;
    rp.depthAttachment.clearDepth = 1.0;
    rp.depthAttachment.storeAction = MTLStoreActionStore;
    s_dw = s_lw;   /* the lit 3D draws at its internal size, through the post (op_draw's scissors) */
    s_dh = s_lh;
    e = lit_encoder(rp, 1);   /* g-buffer */
    [e setRenderPipelineState:s_lpg];
    for (i = 0; i < s_nops; i++)
        if (s_ops[i].kind == K_OPQ)
            op_draw(e, &s_ops[i], 1);
    [e endEncoding];

    /* the sun's shadow cascades (brr_shadow.h, brr_shc_plan): per cascade, the
       track drawn again into the static cache only when the cascade's matrix
       or the track changed; the live map restored from the cache where last
       frame's moving casters were (all of it after a redraw), by blit; this
       frame's moving casters composited over the cache in a scratch tile
       just their size, and blitted in.  A cascade with no moving casters,
       before or now, costs nothing.  The blits go in two encoders for all
       the cascades (an encoder costs about as much as the texels it saves),
       so a frame with movers in two cascades has four encoders, as many as
       drawing the four maps afresh, and moves a small part of the memory */
    if (u.misc[1] > 0.5f) {
        id<MTLBuffer> b = s_ndyn ? [s_dev newBufferWithBytes:s_dyn length:s_ndyn * 3 * sizeof(float) options:MTLResourceStorageModeShared] : nil;
        unsigned gen = s_trkn ? s_trkgen : 0xFFFFFFFFu;
        int c, dyn[CASCADES][4], rs[CASCADES][4], nrs = 0, ntile = 0;
        id<MTLBlitCommandEncoder> be;
        s_shc_frames++;
        for (c = 0; c < CASCADES; c++) {
            brr_shc_rect(cs[c].m, s_dyn, s_ndyn, SHADOW_SIZE, dyn[c]);
            if (!b)
                dyn[c][0] = dyn[c][2] = 0;
            ntile += dyn[c][0] < dyn[c][2];
            if (!brr_shc_plan(&s_shc[c], cs[c].m, gen, dyn[c], SHADOW_SIZE, rs[c]))
                continue;
            s_shc_redraws[c]++;
            rp = [MTLRenderPassDescriptor renderPassDescriptor];
            rp.depthAttachment.texture = s_shcache;
            rp.depthAttachment.slice = (NSUInteger)c;
            rp.depthAttachment.loadAction = MTLLoadActionClear;
            rp.depthAttachment.clearDepth = 1.0;
            rp.depthAttachment.storeAction = MTLStoreActionStore;
            e = lit_encoder(rp, 2);   /* sun shadow */
            if (s_trkn) {
                [e setRenderPipelineState:s_shpipe];
                [e setDepthStencilState:s_shds];
                [e setCullMode:MTLCullModeNone];
                [e setDepthBias:1.0f slopeScale:2.0f clamp:0.01f];
                [e setVertexBytes:cs[c].m length:sizeof cs[c].m atIndex:1];
                [e setVertexBuffer:s_trkbuf offset:0 atIndex:0];
                [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:(NSUInteger)s_trkn];
            }
            [e endEncoding];
        }
        for (c = 0; c < CASCADES; c++)
            nrs += rs[c][0] < rs[c][2];
        if (nrs) {
            be = [s_cmd blitCommandEncoder];
            for (c = 0; c < CASCADES; c++)
                if (rs[c][0] < rs[c][2]) {
                    s_shc_restored += (unsigned long)(rs[c][2] - rs[c][0]) * (unsigned long)(rs[c][3] - rs[c][1]);
                    [be copyFromTexture:s_shcache sourceSlice:(NSUInteger)c sourceLevel:0
                           sourceOrigin:MTLOriginMake((NSUInteger)rs[c][0], (NSUInteger)rs[c][1], 0)
                             sourceSize:MTLSizeMake((NSUInteger)(rs[c][2] - rs[c][0]), (NSUInteger)(rs[c][3] - rs[c][1]), 1)
                              toTexture:s_shadow destinationSlice:(NSUInteger)c destinationLevel:0
                      destinationOrigin:MTLOriginMake((NSUInteger)rs[c][0], (NSUInteger)rs[c][1], 0)];
                }
            [be endEncoding];
        }
        for (c = 0; c < CASCADES; c++) {
            int w = dyn[c][2] - dyn[c][0], h = dyn[c][3] - dyn[c][1], tb;
            uint32_t k[4] = { (uint32_t)c, (uint32_t)dyn[c][0], (uint32_t)dyn[c][1], 0 };
            float mc[16];
            if (w <= 0)
                continue;
            tb = brr_shc_tile(w, h, SHADOW_SIZE);
            if (!s_shtile[c] || (int)s_shtile[c].width < tb) {   /* grown as needed, never shrunk */
                MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                                                                             width:(NSUInteger)tb height:(NSUInteger)tb mipmapped:NO];
                td.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
                td.storageMode = MTLStorageModePrivate;
                s_shtile[c] = [s_dev newTextureWithDescriptor:td];
            }
            tb = (int)s_shtile[c].width;
            brr_shc_crop(cs[c].m, SHADOW_SIZE, dyn[c][0], dyn[c][1], tb, mc);
            s_shc_tiled += (unsigned long)w * (unsigned long)h;
            rp = [MTLRenderPassDescriptor renderPassDescriptor];
            rp.depthAttachment.texture = s_shtile[c];
            rp.depthAttachment.loadAction = MTLLoadActionDontCare;
            rp.depthAttachment.storeAction = MTLStoreActionStore;
            e = lit_encoder(rp, 2);   /* sun shadow */
            [e setScissorRect:(MTLScissorRect){ 0, 0, (NSUInteger)w, (NSUInteger)h }];
            [e setRenderPipelineState:s_shcopy];
            [e setDepthStencilState:s_shcopyds];
            [e setFragmentTexture:s_shcache atIndex:0];
            [e setFragmentBytes:k length:sizeof k atIndex:0];
            [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
            [e setRenderPipelineState:s_shpipe];
            [e setDepthStencilState:s_shds];
            [e setCullMode:MTLCullModeNone];
            [e setDepthBias:1.0f slopeScale:2.0f clamp:0.01f];
            [e setVertexBytes:mc length:sizeof mc atIndex:1];
            [e setVertexBuffer:b offset:0 atIndex:0];
            [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:s_ndyn];
            [e endEncoding];
        }
        if (ntile) {
            be = [s_cmd blitCommandEncoder];
            for (c = 0; c < CASCADES; c++)
                if (dyn[c][0] < dyn[c][2])
                    [be copyFromTexture:s_shtile[c] sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0)
                             sourceSize:MTLSizeMake((NSUInteger)(dyn[c][2] - dyn[c][0]), (NSUInteger)(dyn[c][3] - dyn[c][1]), 1)
                              toTexture:s_shadow destinationSlice:(NSUInteger)c destinationLevel:0
                      destinationOrigin:MTLOriginMake((NSUInteger)dyn[c][0], (NSUInteger)dyn[c][1], 0)];
            [be endEncoding];
        }
        if (getenv("BR_LIGHT_STATS") && s_frame % 600 == 0) {
            fprintf(stderr, "light: shadow cache over %lu frames: static redraws %lu %lu %lu %lu, texels a frame restored %.0f and composited %.0f,"
                            " %zu of %zu triangles moving casters\n", s_shc_frames, s_shc_redraws[0], s_shc_redraws[1],
                    s_shc_redraws[2], s_shc_redraws[3], (double)s_shc_restored / (double)s_shc_frames,
                    (double)s_shc_tiled / (double)s_shc_frames, s_ndyn / 3, s_ntris / 3);
            memset(s_shc_redraws, 0, sizeof s_shc_redraws);
            s_shc_restored = s_shc_tiled = s_shc_frames = 0;
        }
    }

    lit_split(0);
    lit_lamp_shadows();

    /* what the game drew before its 3D (its clears and background fills) is
       the background: behind the scene, not over it */
    for (first3d = 0; first3d < s_nops; first3d++)
        if (s_ops[first3d].kind == K_OPQ || s_ops[first3d].kind == K_BLEND || s_ops[first3d].kind == K_BACK)
            break;

    /* the lighting, in HDR: the background and backdrop, then every G-buffer pixel */
    xtj = s_xtj;
    s_xtj = nil;
    rp = [MTLRenderPassDescriptor renderPassDescriptor];
    rp.colorAttachments[0].texture = s_hdr;
    rp.colorAttachments[0].loadAction = MTLLoadActionClear;
    rp.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);
    for (i = 0; i < first3d; i++)
        if (s_ops[i].kind == K_CLEAR && s_ops[i].colour) {
            float c3[3];
            for (k = 0; k < 3; k++)
                c3[k] = (s_ops[i].clear[k] <= 0.04045f ? s_ops[i].clear[k] / 12.92f
                                                        : powf((s_ops[i].clear[k] + 0.055f) / 1.055f, 2.4f)) * u.bg[0];
            rp.colorAttachments[0].clearColor = MTLClearColorMake(c3[0], c3[1], c3[2], 1);
        }
    rp.colorAttachments[0].storeAction = MTLStoreActionStore;
    lit_split(1);
    lit_gi(&u, eye);                    /* the irradiance probes' update (brr_metal_gi.h) */
    lit_split(2);
    e = lit_encoder(rp, 4);   /* lighting */
    if (sky) {                          /* the sky, then the backdrop's scenery in front of it */
        [e setRenderPipelineState:s_psky];
        [e setFragmentBytes:&ku length:sizeof ku atIndex:0];
        [e setFragmentTexture:s_atmS atIndex:0];
        [e setFragmentTexture:s_atmT atIndex:1];
        [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        [e setRenderPipelineState:s_pbk2];
        for (i = 0; i < s_nops; i++)
            if (s_ops[i].kind == K_BACK) {
                [e setFragmentBytes:&u length:sizeof u atIndex:1];
                [e setFragmentBytes:&ku length:sizeof ku atIndex:2];
                op_draw(e, &s_ops[i], 0);
            }
    } else {
        [e setRenderPipelineState:s_lpb];
        for (i = 0; i < s_nops; i++)
            if (s_ops[i].kind == K_BACK || (s_ops[i].kind == K_OTHER && i < first3d)) {
                [e setFragmentBytes:&u length:sizeof u atIndex:1];
                op_draw(e, &s_ops[i], 0);
            }
    }
    [e setScissorRect:(MTLScissorRect){ 0, 0, (NSUInteger)s_lw, (NSUInteger)s_lh }];
    [e setRenderPipelineState:s_lpl];
    [e setFragmentBytes:&u length:sizeof u atIndex:0];
    [e setFragmentBuffer:s_lset[s_vbi] offset:0 atIndex:1];
    [e setFragmentTexture:s_galb atIndex:0];
    [e setFragmentTexture:s_gpos atIndex:1];
    [e setFragmentTexture:s_gnrm atIndex:2];
    [e setFragmentTexture:s_shadow atIndex:3];
    [e setFragmentTexture:s_latl atIndex:4];
    [e setFragmentTexture:s_brdfE atIndex:5];
    lit_gi_bind(e, 2);
    [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    [e endEncoding];
    lit_split(3);
    if (u.wet[2] > 0.5f)   /* reflections (brr_metal_refl.m) */
        brr_refl_pass(s_dev, s_cmd, &u, sizeof u, u.wet[2] > 1.5f ? brr_metal_rt_world() : NULL, s_galb, s_gpos, s_gnrm, s_hdr, sky ? s_atmS : nil, s_brdfE);

    lit_split(4);
    if (u.vol[0] > 0.5f) {   /* the volumetric fog over the lit opaque 3D, the sky and the backdrop */
        float vs[16];
        int xf = 0;
        for (i = 0; i < s_nops; i++)
            if (s_ops[i].kind == K_OPQ) {
                xf = (int)s_ops[i].u.xf;
                break;
            }
        brr_screen_matrix(cam.vp, cam.sx, cam.ox, cam.sy, cam.oy, (float)light_glide_flip(),
                          (const float *)[s_xt[s_vbi] contents] + 4 * xf, (float)s_lw, (float)s_lh, vs);
        brr_vol_frame(s_dev, s_lib, s_cmd, &u, sizeof u, s_lset[s_vbi], s_shadow, s_gpos, s_hdr, eye, vs, w, (double)s_frame / 60.0);
    }

    /* the blended 3D, forward-lit, in the game's order */
    s_xtj = xtj;
    lit_split(5);
    rp = [MTLRenderPassDescriptor renderPassDescriptor];
    rp.colorAttachments[0].texture = s_hdr;
    rp.colorAttachments[0].loadAction = MTLLoadActionLoad;
    rp.colorAttachments[0].storeAction = MTLStoreActionStore;
    rp.depthAttachment.texture = s_ldep;
    rp.depthAttachment.loadAction = MTLLoadActionLoad;
    rp.depthAttachment.storeAction = MTLStoreActionStore;
    e = lit_encoder(rp, 5);   /* transparent */
    for (i = 0; i < s_nops; i++)
        if (s_ops[i].kind == K_BLEND) {
            [e setRenderPipelineState:lit_forward(s_ops[i].src, s_ops[i].dst)];
            [e setFragmentBytes:&u length:sizeof u atIndex:1];
            [e setFragmentBuffer:s_lset[s_vbi] offset:0 atIndex:2];
            [e setFragmentTexture:s_shadow atIndex:3];
            [e setFragmentTexture:s_latl atIndex:4];
            [e setFragmentTexture:s_brdfE atIndex:5];
            lit_gi_bind(e, 3);
            brr_vol_bind(e, 4, 8);      /* the volumetric fog, when u.vol is on */
            op_draw(e, &s_ops[i], 1);
        }
    [e endEncoding];

    lit_split(6);
    s_dw = s_dh = 0;
    if (s_xtj) {   /* TAA: the resolved HDR goes on to bloom and post; scaled, MetalFX's upscale to the target's size */
        hdr = brr_taa_resolve(s_dev, s_cmd, s_hdr, s_gpos, s_ldep, taa_s, cam.vp, eye, s_rw, s_rh, s_lup);
        s_xtj = nil;
    }

    lit_split(7);
    /* bloom */
    for (k = 0; k < s_nbl; k++) {
        rp = [MTLRenderPassDescriptor renderPassDescriptor];
        rp.colorAttachments[0].texture = s_bl[k];
        rp.colorAttachments[0].loadAction = MTLLoadActionDontCare;
        rp.colorAttachments[0].storeAction = MTLStoreActionStore;
        e = lit_encoder(rp, 6);   /* bloom down */
        [e setRenderPipelineState:s_lbd];
        [e setFragmentTexture:k ? s_bl[k - 1] : hdr atIndex:0];
        [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        [e endEncoding];
    }
    for (k = s_nbl - 1; k > 0; k--) {
        rp = [MTLRenderPassDescriptor renderPassDescriptor];
        rp.colorAttachments[0].texture = s_bl[k - 1];
        rp.colorAttachments[0].loadAction = MTLLoadActionLoad;
        rp.colorAttachments[0].storeAction = MTLStoreActionStore;
        e = lit_encoder(rp, 7);   /* bloom up */
        [e setRenderPipelineState:s_lbua];
        [e setFragmentTexture:s_bl[k] atIndex:0];
        [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        [e endEncoding];
    }

    /* post, into the game's colour target */
    rp = [MTLRenderPassDescriptor renderPassDescriptor];
    rp.colorAttachments[0].texture = s_col;
    rp.colorAttachments[0].loadAction = MTLLoadActionDontCare;
    rp.colorAttachments[0].storeAction = MTLStoreActionStore;
    e = lit_encoder(rp, 8);   /* post */
    [e setRenderPipelineState:s_lpp];
    {
        LU pu = u;
        pu.misc[0] = u.misc[0];
        pu.bg[1] = u.bg[1];
        [e setFragmentBytes:&pu length:sizeof pu atIndex:0];
    }
    [e setFragmentTexture:hdr atIndex:0];
    [e setFragmentTexture:s_nbl ? s_bl[0] : hdr atIndex:1];
    [e setFragmentTexture:s_galb atIndex:2];
    [e setFragmentBuffer:s_hist[s_vbi] offset:0 atIndex:1];
    [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    [e endEncoding];

    memcpy(s_gpu_used[s_vbi], s_gpu_frame_used, sizeof s_gpu_frame_used);   /* set by the time 2d begins, below */
    s_gpu_used[s_vbi][9] = 1;
    /* the game's 2D, mirror and LFB writes over it, as it drew them */
    rp = [MTLRenderPassDescriptor renderPassDescriptor];
    rp.colorAttachments[0].texture = s_col;
    rp.colorAttachments[0].loadAction = MTLLoadActionLoad;
    rp.colorAttachments[0].storeAction = MTLStoreActionStore;
    rp.depthAttachment.texture = s_dep;
    rp.depthAttachment.loadAction = s_ldep != s_dep ? MTLLoadActionClear : MTLLoadActionLoad;   /* scaled: the 3D's depth is not at this size */
    rp.depthAttachment.clearDepth = 1.0;
    rp.depthAttachment.storeAction = MTLStoreActionStore;
    e = lit_encoder(rp, 9);   /* 2d */
    for (i = first3d; i < s_nops; i++) {
        const Op *o = &s_ops[i];
        if (o->kind == K_OTHER) {
            [e setRenderPipelineState:pipe_for(o->src, o->dst)];
            op_draw(e, o, 1);
        } else if (o->kind == K_CLEAR) {
            [e setRenderPipelineState:s_clear[o->colour ? 1 : 0]];
            [e setDepthStencilState:s_clear_ds[o->depthbuf ? 1 : 0]];
            [e setScissorRect:(MTLScissorRect){ 0, 0, (NSUInteger)s_rw, (NSUInteger)s_rh }];
            [e setVertexBuffer:s_vb[s_vbi] offset:(NSUInteger)o->voff atIndex:0];
            [e setFragmentBytes:o->clear length:sizeof o->clear atIndex:0];
            [e setFragmentBytes:&o->depth length:sizeof o->depth atIndex:1];
            [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
        } else if (o->kind == K_LFB) {
            [e setRenderPipelineState:o->sharp ? s_lfbsharp : s_lfbpipe];
            [e setDepthStencilState:s_clear_ds[0]];
            [e setScissorRect:(MTLScissorRect){ 0, 0, (NSUInteger)s_rw, (NSUInteger)s_rh }];
            [e setVertexBuffer:s_vb[s_vbi] offset:(NSUInteger)o->voff atIndex:0];
            [e setFragmentTexture:s_lfbtex[(NSUInteger)o->lfb] atIndex:0];
            [e setFragmentSamplerState:s_near atIndex:0];
            [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
        }
    }
    [e endEncoding];
}

static void lit_reset(void)
{
    int i;
    s_galb = s_gpos = s_gnrm = s_hdr = s_ldep = nil;
    brr_taa_reset();
    for (i = 0; i < 6; i++)
        s_bl[i] = nil;
    s_nbl = 0;
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
        s_lib = [s_dev newLibraryWithSource:[[[[k_src stringByAppendingString:@BRR_LIGHTS_SRC] stringByAppendingString:@BRR_LIT_SRC]
                                                stringByAppendingString:@BRR_SKY_SRC] stringByAppendingString:@BRR_VOL_SRC]
                                    options:nil error:&err];
        if (!s_lib) {
            fprintf(stderr, "brr: metal shader: %s\n", [[err description] UTF8String]);
            return 0;
        }
        {
            /* headless, BR_SIZE=WxH sets the targets' size (a window's drawable
             * sets it otherwise): shots and GPU timings at a display's size */
            const char *e = getenv("BR_SIZE");
            int tw = 0, th = 0;
            if (!host_macos_metal_layer() && e && sscanf(e, "%dx%d", &tw, &th) == 2 && tw > 0 && th > 0)
                targets(tw, th);
            else
                targets(width, height);
        }
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
        lit_init();
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
        s_skym[tid] = nil;
        lit_emask(tid, rgba, w, h);
        light_glow_new(&s_glow, tid);
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

/* the frame's command buffer and vertex buffer slot, begun on first use;
 * whether the frame is lit is decided here, once */
static void frame_begin(void)
{
    if (!s_cmd) {
        dispatch_semaphore_wait(s_inflight, DISPATCH_TIME_FOREVER);
        s_cmd = [s_q commandBuffer];
        {
            dispatch_semaphore_t sem = s_inflight;
            [s_cmd addCompletedHandler:^(id<MTLCommandBuffer> b) { (void)b; dispatch_semaphore_signal(sem); }];
        }
        s_vbi = (s_vbi + 1) % 3;
        s_vused = 0;
        s_litf = plat_flag(BR_FLAG_LIGHTING);
        s_nops = 0;
        s_ntris = 0;
        [s_lfbtex removeAllObjects];
        [s_frametex removeAllObjects];
    }
}

/* the frame's encoder into the offscreen target, begun on first use */
static id<MTLRenderCommandEncoder> enc(void)
{
    if (!s_enc) {
        MTLRenderPassDescriptor *rp = [MTLRenderPassDescriptor renderPassDescriptor];
        frame_begin();
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
        id<MTLRenderCommandEncoder> e = nil;
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
        frame_begin();
        if ((off = vb_take(sizeof q)) < 0)
            return;
        memcpy((char *)[s_vb[s_vbi] contents] + off, q, sizeof q);
        if (s_litf) {                   /* replayed after the lighting (lit_present) */
            Op *o = op_new(K_CLEAR);
            if (o) {
                o->voff = off;
                o->colour = colour;
                o->depthbuf = depthbuf;
                memcpy(o->clear, c, sizeof c);
                o->depth = depth;
            }
            return;
        }
        e = enc();
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
        frame_begin();
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
                o[i].wld[0] = v[i].wx;
                o[i].wld[1] = v[i].wy;
                o[i].wld[2] = v[i].wz;
                o[i].wld[3] = v[i].lit;
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
        if (s_litf) {                   /* recorded; replayed in the lighting's passes (lit_present) */
            int opaque = st->blend_rgb_src == 4 && st->blend_rgb_dst == 0;
            int kind = v[0].lit > 1.5f && v[0].lit < 2.5f ? K_BACK : v[0].lit > 0.5f ? (opaque ? K_OPQ : K_BLEND) : K_OTHER;
            Op *op = op_new(kind);
            if (!op)
                return;
            {
                int x0 = st->sx0 < 0 ? 0 : st->sx0, y0 = st->sy0 < 0 ? 0 : st->sy0;
                int x1 = st->sx1 > s_rw ? s_rw : st->sx1, y1 = st->sy1 > s_rh ? s_rh : st->sy1;
                if (x1 <= x0 || y1 <= y0) {
                    s_nops--;
                    return;
                }
                op->sc[0] = x0; op->sc[1] = y0; op->sc[2] = x1 - x0; op->sc[3] = y1 - y0;
                op->has_sc = 1;
            }
            u.glow = 0.0f;
            if (light_game_weather() == 4 && u.has_tex && (kind == K_OPQ || kind == K_BLEND) && n >= 3) {
                /* lamplight glows only in the track's own scene, never on a car (light_night.h) */
                const float tri[9] = { v[0].wx, v[0].wy, v[0].wz, v[1].wx, v[1].wy, v[1].wz, v[2].wx, v[2].wy, v[2].wz };
                light_glow_drawn(&s_glow, st->texture, light_capture_ontrack(tri));
                u.glow = light_glow_on(&s_glow, st->texture) ? 1.0f : 0.0f;
                if (u.glow > 0 && st->texture < LIGHT_GLOW_TEX && s_eshare[st->texture] > 0) {
                    /* the track's lights: its glowing triangles, once a track generation (light_world.c) */
                    const LightTrack *trk = light_game_track();
                    float eye[3] = { 0, 0, 0 }, *wt = malloc((size_t)(n / 3) * 9 * sizeof(float));
                    int q;
                    light_capture_eye(eye);
                    if (trk && wt) {
                        for (q = 0; q < n / 3 * 3; q++) {
                            wt[3 * q] = v[q].wx;
                            wt[3 * q + 1] = v[q].wy;
                            wt[3 * q + 2] = v[q].wz;
                        }
                        light_world_gather(trk->gen, wt, n / 3, s_eshare[st->texture], s_ecol[st->texture], eye);
                    }
                    free(wt);
                }
            }
            u.addk = kind == K_BLEND && st->blend_rgb_dst == 4 ? 100.0f : 0.0f;   /* dst ONE: light it adds */
            op->u = u;
            op->src = st->blend_rgb_src;
            op->dst = st->blend_rgb_dst;
            op->dmode = st->depth_mode;
            op->dfn = st->depth_fn;
            op->dmask = st->depth_mask;
            op->tex = st->texture;
            op->tw = st->tex_w;
            op->th = st->tex_h;
            if (u.has_tex) {            /* this draw's texture and glow mask, as they are now */
                id<MTLTexture> t = st->texture < TEX_MAX && s_tex[st->texture] ? s_tex[st->texture] : s_white;
                id<MTLTexture> m = st->texture < TEX_MAX && s_emask[st->texture] ? s_emask[st->texture] : s_black;
                op->texobj = (int)[s_frametex count];
                if (kind == K_BACK)     /* a backdrop's painted sky, for the physical sky through it */
                    [s_frametex addObject:@[ t, m, lit_skymask(st->texture) ]];
                else
                    [s_frametex addObject:@[ t, m ]];
            }
            op->filt = st->mag_filter || st->min_filter;
            op->cs = st->clamp_s;
            op->ct = st->clamp_t;
            op->voff = off;
            op->n = n;
            if (kind == K_OPQ) {        /* the shadow map's casters */
                if (s_ntris + (size_t)n > s_captris) {
                    size_t c = s_captris ? s_captris * 2 : 65536;
                    float *p;
                    while (c < s_ntris + (size_t)n)
                        c *= 2;
                    if ((p = realloc(s_tris, c * 3 * sizeof(float)))) {
                        s_tris = p;
                        s_captris = c;
                    }
                }
                if (s_ntris + (size_t)n <= s_captris)
                    for (i = 0; i < n; i++, s_ntris++) {
                        s_tris[3 * s_ntris + 0] = v[i].wx;
                        s_tris[3 * s_ntris + 1] = v[i].wy;
                        s_tris[3 * s_ntris + 2] = v[i].wz;
                    }
            }
            return;
        }
        e = enc();
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
        frame_begin();
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
        if (s_litf) {                   /* replayed after the lighting (lit_present) */
            Op *o = op_new(K_LFB);
            if (o) {
                o->voff = off;
                o->lfb = (int)[s_lfbtex count];
                o->sharp = !(d[2] - d[0] == w && d[3] - d[1] == h);
                [s_lfbtex addObject:t];
            }
            return;
        }
        e = enc();
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
        frame_begin();                          /* the frame's command buffer, and so its slot */
        memcpy([s_xt[s_vbi] contents], t, (size_t)n * 16);
        s_nxf[s_vbi] = n;
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

/* BR_STATS: once a second, the frames drawn and those the screen showed
 * (the drawable's presented time; in the iOS Simulator, which has none,
 * those handed to it) */
static volatile int s_shown;

static void stats(void)
{
    static int on = -1;
    static double t0;
    static unsigned long f0;
    static int s0;
    double now;
    if (on < 0)
        on = getenv("BR_STATS") != NULL;
    if (!on)
        return;
    now = CACurrentMediaTime();
    if (t0 == 0) {
        t0 = now;
        f0 = s_frame;
        s0 = s_shown;
    } else if (now - t0 >= 1.0) {
        int shown = s_shown;
        fprintf(stderr, "brr stats: %lu frames drawn, %d shown in %.3f s\n", s_frame - f0, shown - s0, now - t0);
        t0 = now;
        f0 = s_frame;
        s0 = shown;
    }
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
        frame_begin();
        if (s_litf) {
            lit_present();                     /* the recorded frame through the lighting's passes */
        } else {
            enc();                             /* a frame with nothing drawn still presents */
            [s_enc endEncoding];
            s_enc = nil;
        }
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
                if (getenv("BR_STATS")) {
#if TARGET_OS_SIMULATOR                 /* (no presented time there: each frame handed over) */
                    __atomic_add_fetch(&s_shown, 1, __ATOMIC_RELAXED);
#else
                    [dr addPresentedHandler:^(id<MTLDrawable> d) {
                        if (d.presentedTime > 0)
                            __atomic_add_fetch(&s_shown, 1, __ATOMIC_RELAXED);
                    }];
#endif
                }
                [s_cmd presentDrawable:dr];
            }
        }
        if (s_gpu_on > 0 && s_litf && getenv("BR_LIGHT_GPU") && atoi(getenv("BR_LIGHT_GPU")) == 2)
            lit_split_add(s_cmd, GPU_SPLITS - 1);
        if (s_gpu_on > 0)   /* the whole frame's GPU time, every pass (BR_LIGHT_GPU) */
            [s_cmd addCompletedHandler:^(id<MTLCommandBuffer> c) {
                double ms = (c.GPUEndTime - c.GPUStartTime) * 1000.0;
                if (ms > 0 && ms < 1000) {
                    __atomic_add_fetch(&s_gpu_cb_us, (long)(ms * 1000.0), __ATOMIC_RELAXED);
                    __atomic_add_fetch(&s_gpu_cb_n, 1, __ATOMIC_RELAXED);
                }
            }];
        if (s_litf && s_lup)   /* the lit frame's GPU time, start to end, for the dynamic resolution (brr_taa_scale) */
            [s_cmd addCompletedHandler:^(id<MTLCommandBuffer> c) {
                double ms = (c.GPUEndTime - c.GPUStartTime) * 1000.0;
                if (ms > 0 && ms < 1000)
                    __atomic_store_n(&s_gpu_last_us, (long)(ms * 1000.0), __ATOMIC_RELAXED);
            }];
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
        stats();
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
