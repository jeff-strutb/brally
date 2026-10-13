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
 * Remastered lighting (BR_FLAG_LIGHTING; docs/brally/remaster/lighting): the
 * game's frame is drawn exactly as above, and each triangle of the camera's
 * 3D view also writes a G-buffer beside it (its albedo with the game's baked
 * light taken out, its world position and surface direction; every other
 * draw marks its pixels as the game's).  At present the sun's shadow map is
 * drawn from the frame's 3D triangles and one lighting pass relights the 3D
 * pixels in linear HDR (physical units, a sun and a sky), exposes them and
 * tonemaps them (Khronos PBR Neutral); the HUD, text and mirror keep the
 * game's pixels.  Off: nothing of this runs. */
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>
#import <ImageIO/ImageIO.h>
#include <TargetConditionals.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "brr.h"
#include "br_flags.h"
#include "light.h"

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
"};\n"
"struct O { float4 pos [[position]]; float4 col [[center_no_perspective]]; float2 st; float oow [[center_no_perspective]]; float z [[center_no_perspective]]; float3 wp; float lit [[flat]]; };\n"
/* the screen map (glide.c): the game's pixels to the target through entry
   u.xf of this frame's table */
"vertex O vs(uint i [[vertex_id]], const device V *v [[buffer(0)]], constant U &u [[buffer(1)]],\n"
"            const device float4 *xt [[buffer(2)]]) {\n"
"  O o; V a = v[i]; float4 T = xt[int(u.xf)];\n"
"  float w = a.oow > 0 ? 1.0 / a.oow : 1.0;\n"
"  float2 ndc = float2(a.pos.x * T.x + T.y, a.pos.y * T.z + T.w);\n"
"  o.pos = float4(ndc * w, clamp(a.z, 0.0, 1.0) * w, w);\n"
"  o.col = clamp(a.col / 255.0, 0.0, 1.0); o.st = a.st; o.oow = a.oow; o.z = a.z;\n"
"  o.wp = a.wld.xyz; o.lit = a.wld.w;\n"
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
"  r.fog = f; r.tex = u.has_tex ? tex : float4(1); r.it = it;\n"
"  if (f > 0) o.rgb += (u.fog_color.rgb - o.rgb) * f;\n"
"  r.o = o;\n"
"  r.d = (u.depth_mode == 2 || u.depth_mode == 4) ? float(wfloat(in.oow)) / 65536.0 : floor(clamp(in.z * 65535.0, 0.0, 65535.0)) / 65536.0;\n"
"  return r;\n"
"}\n"
"fragment FO fs(O in [[stage_in]], constant U &u [[buffer(0)]], texture2d<float> t [[texture(0)]], sampler sm [[sampler(0)]]) {\n"
"  SH h = shade(in, u, t, sm);\n"
"  if (!h.keep) discard_fragment();\n"
"  FO r; r.c = h.o; r.d = h.d; return r;\n"
"}\n"
/* the Remastered G-buffer beside the game's colour: albedo (the texture
   times the vertex colour's hue: its brightness is the game's baked light,
   which the lighting replaces; a = 1 marks a 3D pixel to relight), the world
   position (w the game's fog colour as 0xRRGGBB) and the surface's
   direction (w the game's fog amount) */
"struct GO { float4 c [[color(0)]]; float4 alb [[color(1)]]; float4 pos [[color(2)]]; float4 nrm [[color(3)]]; float d [[depth(any)]]; };\n"
"fragment GO fsg(O in [[stage_in]], constant U &u [[buffer(0)]], texture2d<float> t [[texture(0)]], sampler sm [[sampler(0)]]) {\n"
"  SH h = shade(in, u, t, sm);\n"
"  if (!h.keep) discard_fragment();\n"
"  GO r; r.c = h.o; r.d = h.d;\n"
"  if (in.lit > 0.5) {\n"
"    float3 vc = h.it.rgb, lin = pow(max(h.tex.rgb, 0.0), 2.2);\n"
"    float lv = dot(vc, float3(0.2126, 0.7152, 0.0722));\n"
"    float3 hue = lv > 0.02 ? clamp(vc / lv, 0.0, 2.0) : float3(1);\n"
"    float3 alb = clamp(lin * pow(hue, 2.2) * 0.85, 0.0, 0.9);\n"
"    r.alb = float4(alb, 1);\n"
"    float3 n = normalize(cross(dfdx(in.wp), dfdy(in.wp)));\n"
"    uint fc = (uint(u.fog_color.r * 255.0) << 16) | (uint(u.fog_color.g * 255.0) << 8) | uint(u.fog_color.b * 255.0);\n"
"    r.pos = float4(in.wp, float(fc)); r.nrm = float4(n, h.fog);\n"
"  } else { r.alb = float4(0); r.pos = float4(0); r.nrm = float4(0); }\n"
"  return r;\n"
"}\n"
/* the sun's shadow map: the frame's 3D triangles from the sun */
"struct SO { float4 pos [[position]]; };\n"
"vertex SO svs(uint i [[vertex_id]], const device packed_float3 *v [[buffer(0)]], constant float4x4 &m [[buffer(1)]]) {\n"
"  SO o; o.pos = m * float4(float3(v[i]), 1.0); return o; }\n"
/* the lighting: linear HDR in physical units (lux), a physical camera's
   exposure, Khronos PBR Neutral, sRGB; only where the G-buffer marks 3D */
"struct LU { float4x4 sunvp; float4 eye; float4 sun; float4 sunc; float4 skyc; float4 gndc; float4 misc; };\n"
"struct LO { float4 pos [[position]]; float2 uv; };\n"
"vertex LO lvs(uint i [[vertex_id]]) { float2 p = float2((i << 1) & 2, i & 2); LO o; o.pos = float4(p * 2.0 - 1.0, 0, 1); o.uv = float2(p.x, 1.0 - p.y); return o; }\n"
"static float3 neutral(float3 c) {\n"
"  const float start = 0.8 - 0.04, desat = 0.15;\n"
"  float x = min(c.r, min(c.g, c.b)); float off = x < 0.08 ? x - 6.25 * x * x : 0.04; c -= off;\n"
"  float p = max(c.r, max(c.g, c.b)); if (p < start) return c;\n"
"  float d = 1.0 - start, np = 1.0 - d * d / (p + d - start); c *= np / p;\n"
"  float g = 1.0 - 1.0 / (desat * (p - np) + 1.0); return mix(c, float3(np), g); }\n"
"static float srgb(float x) { return x <= 0.0031308 ? 12.92 * x : 1.055 * pow(x, 1.0 / 2.4) - 0.055; }\n"
"fragment float4 lfs(LO in [[stage_in]], constant LU &u [[buffer(0)]], texture2d<float> alb [[texture(0)]],\n"
"                    texture2d<float> pos [[texture(1)]], texture2d<float> nrm [[texture(2)]], depth2d<float> sm [[texture(3)]],\n"
"                    device atomic_uint *lum [[buffer(1)]]) {\n"
"  uint2 px = uint2(in.pos.xy); float4 a = alb.read(px);\n"
"  if (a.a < 0.5) discard_fragment();\n"
"  float4 P = pos.read(px), N4 = nrm.read(px); float3 p = P.xyz, n = N4.xyz, v = normalize(u.eye.xyz - p);\n"
"  if (dot(n, v) < 0) n = -n;\n"
"  float3 l = u.sun.xyz; float ndl = max(dot(n, l), 0.0);\n"
"  float vis = 1.0;\n"
"  if (u.misc.y > 0.5 && ndl > 0) {\n"
"    constexpr sampler cs(filter::linear, address::clamp_to_edge, compare_func::less_equal);\n"
"    float4 c = u.sunvp * float4(p + n * 0.05, 1.0); float2 st = float2(c.x * 0.5 + 0.5, 0.5 - c.y * 0.5);\n"
"    if (all(st > 0.0) && all(st < 1.0) && c.z < 1.0) { vis = 0; float2 tx = 1.0 / float2(sm.get_width(), sm.get_height());\n"
"      for (int y = -1; y <= 1; y++) for (int x = -1; x <= 1; x++) vis += sm.sample_compare(cs, st + float2(x, y) * tx, c.z - 0.0008);\n"
"      vis /= 9.0; } }\n"
"  float3 sky = mix(u.gndc.rgb, u.skyc.rgb, n.z * 0.5 + 0.5);\n"
"  float3 h = normalize(l + v); float nh = max(dot(n, h), 0.0), r2 = 0.6 * 0.6;\n"
"  float dd = nh * nh * (r2 - 1.0) + 1.0, D = r2 / (3.14159 * dd * dd);\n"
"  float F = 0.04 + 0.96 * pow(1.0 - max(dot(h, v), 0.0), 5.0);\n"
"  float3 Lo = a.rgb / 3.14159 * (u.sunc.rgb * ndl * vis + sky) + u.sunc.rgb * (D * F * 0.25 * ndl * vis * u.misc.z);\n"
"  int dbg = int(u.misc.w);\n"
"  if (dbg == 1) return float4(pow(a.rgb, 1.0 / 2.2), 1);\n"
"  if (dbg == 2) return float4(n * 0.5 + 0.5, 1);\n"
"  if (dbg == 3) return float4(float3(vis), 1);\n"
"  if (dbg == 4) return float4(float3(ndl), 1);\n"
"  Lo *= u.gndc.w > 0 ? float3(u.sunc.w, u.skyc.w, u.gndc.w) : float3(1);   /* the camera's white balance */\n"
"  float Y = dot(Lo, float3(0.2126, 0.7152, 0.0722));\n"
"  if (((px.x | px.y) & 3) == 0) {   /* every 16th pixel: the frame's mean log luminance, for the exposure */\n"
"    atomic_fetch_add_explicit(&lum[0], uint(clamp(log2(max(Y, 1e-4)) + 20.0, 0.0, 60.0) * 256.0), memory_order_relaxed);\n"
"    atomic_fetch_add_explicit(&lum[1], 1u, memory_order_relaxed); }\n"
"  float3 c = neutral(Lo * u.misc.x);\n"
"  uint fc = uint(P.w); float3 fogc = float3((fc >> 16) & 255, (fc >> 8) & 255, fc & 255) / 255.0;\n"
"  float3 o = float3(srgb(c.r), srgb(c.g), srgb(c.b));\n"
"  return float4(mix(o, fogc, N4.w), 1);\n"
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

/* ---- the Remastered lighting (BR_FLAG_LIGHTING) ------------------------------------- */
static int s_litf;                  /* this frame is lit: its encoder has the G-buffer too */
static id<MTLTexture> s_galb, s_gpos, s_gnrm, s_shadow;
/* [variant][src][dst]: 0 a 3D draw (G-buffer written when opaque), 1 any other
   (marks its pixels as the game's) */
static id<MTLRenderPipelineState> s_gpipes[2][16][16];
static id<MTLRenderPipelineState> s_gclear[2], s_glfb, s_glfbsharp, s_shpipe, s_lpipe;
static id<MTLDepthStencilState> s_shds;
static float *s_tris;               /* this frame's 3D triangles in world space, for the shadow map */
static size_t s_ntris, s_captris;
#define SHADOW_SIZE 2048

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

static void gbuffer_formats(MTLRenderPipelineDescriptor *d, int write)
{
    d.colorAttachments[1].pixelFormat = MTLPixelFormatRGBA8Unorm;
    d.colorAttachments[2].pixelFormat = MTLPixelFormatRGBA32Float;
    d.colorAttachments[3].pixelFormat = MTLPixelFormatRGBA16Float;
    d.colorAttachments[1].writeMask = write ? MTLColorWriteMaskAll : MTLColorWriteMaskNone;
    d.colorAttachments[2].writeMask = write == 2 ? MTLColorWriteMaskAll : MTLColorWriteMaskNone;
    d.colorAttachments[3].writeMask = write == 2 ? MTLColorWriteMaskAll : MTLColorWriteMaskNone;
}

/* a lit frame's pipeline: a 3D draw writes the whole G-buffer when it is
 * opaque and leaves it when it blends (smoke, the game's painted car
 * shadow); every other draw marks its pixels as the game's */
static id<MTLRenderPipelineState> gpipe_for(int lit, int src, int dst)
{
    int v = lit ? 0 : 1;
    src &= 15;
    dst &= 15;
    if (!s_gpipes[v][src][dst]) {
        MTLRenderPipelineDescriptor *d = [MTLRenderPipelineDescriptor new];
        NSError *err = nil;
        int opaque = src == 4 && dst == 0;
        d.vertexFunction = [s_lib newFunctionWithName:@"vs"];
        d.fragmentFunction = [s_lib newFunctionWithName:@"fsg"];
        d.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
        d.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
        d.colorAttachments[0].writeMask = MTLColorWriteMaskRed | MTLColorWriteMaskGreen | MTLColorWriteMaskBlue;
        if (!opaque) {
            d.colorAttachments[0].blendingEnabled = YES;
            d.colorAttachments[0].sourceRGBBlendFactor = bf(src, 1);
            d.colorAttachments[0].destinationRGBBlendFactor = bf(dst, 0);
            d.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorZero;
            d.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOne;
        }
        gbuffer_formats(d, lit ? (opaque ? 2 : 0) : 1);
        s_gpipes[v][src][dst] = [s_dev newRenderPipelineStateWithDescriptor:d error:&err];
        if (!s_gpipes[v][src][dst])
            fprintf(stderr, "brr: metal lit pipeline: %s\n", [[err description] UTF8String]);
    }
    return s_gpipes[v][src][dst];
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

static id<MTLRenderPipelineState> simple_pipe_g(NSString *vs, NSString *fs, int colour, int depth, int gbuf)
{
    MTLRenderPipelineDescriptor *d = [MTLRenderPipelineDescriptor new];
    NSError *err = nil;
    id<MTLRenderPipelineState> p;
    d.vertexFunction = [s_lib newFunctionWithName:vs];
    d.fragmentFunction = [s_lib newFunctionWithName:fs];
    d.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
    if (gbuf)
        gbuffer_formats(d, 0);
    if (!colour)
        d.colorAttachments[0].writeMask = MTLColorWriteMaskNone;
    if (depth)
        d.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
    p = [s_dev newRenderPipelineStateWithDescriptor:d error:&err];
    if (!p)
        fprintf(stderr, "brr: metal pipeline %s: %s\n", [fs UTF8String], [[err description] UTF8String]);
    return p;
}

static id<MTLRenderPipelineState> simple_pipe(NSString *vs, NSString *fs, int colour, int depth)
{
    return simple_pipe_g(vs, fs, colour, depth, 0);
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
    s_galb = s_gpos = s_gnrm = nil;     /* the G-buffer follows, made at the next lit frame */
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
        s_gclear[0] = simple_pipe_g(@"cvs", @"cfs", 0, 1, 1);
        s_gclear[1] = simple_pipe_g(@"cvs", @"cfs", 1, 1, 1);
        s_glfb = simple_pipe_g(@"bvs", @"bfs", 1, 1, 1);
        s_glfbsharp = simple_pipe_g(@"bvs", @"sfs", 1, 1, 1);
        {
            MTLRenderPipelineDescriptor *d = [MTLRenderPipelineDescriptor new];
            MTLDepthStencilDescriptor *dd = [MTLDepthStencilDescriptor new];
            d.vertexFunction = [s_lib newFunctionWithName:@"svs"];
            d.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
            s_shpipe = [s_dev newRenderPipelineStateWithDescriptor:d error:&err];
            d = [MTLRenderPipelineDescriptor new];
            d.vertexFunction = [s_lib newFunctionWithName:@"lvs"];
            d.fragmentFunction = [s_lib newFunctionWithName:@"lfs"];
            d.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
            s_lpipe = [s_dev newRenderPipelineStateWithDescriptor:d error:&err];
            if (!s_shpipe || !s_lpipe)
                fprintf(stderr, "brr: metal lighting pipelines: %s\n", [[err description] UTF8String]);
            dd.depthCompareFunction = MTLCompareFunctionLessEqual;
            dd.depthWriteEnabled = YES;
            s_shds = [s_dev newDepthStencilStateWithDescriptor:dd];
        }
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
            s_litf = plat_flag(BR_FLAG_LIGHTING);
            s_ntris = 0;
            if (s_litf && !s_galb) {
                MTLTextureDescriptor *td;
                MTLPixelFormat f[3] = { MTLPixelFormatRGBA8Unorm, MTLPixelFormatRGBA32Float, MTLPixelFormatRGBA16Float };
                id<MTLTexture> __strong *t[3] = { &s_galb, &s_gpos, &s_gnrm };
                int k;
                for (k = 0; k < 3; k++) {
                    td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:f[k] width:(NSUInteger)s_rw
                                                                           height:(NSUInteger)s_rh mipmapped:NO];
                    td.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
                    td.storageMode = MTLStorageModePrivate;
                    *t[k] = [s_dev newTextureWithDescriptor:td];
                }
            }
        }
        rp.colorAttachments[0].texture = s_col;
        rp.colorAttachments[0].loadAction = s_fresh ? MTLLoadActionClear : MTLLoadActionLoad;
        rp.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);
        rp.colorAttachments[0].storeAction = MTLStoreActionStore;
        if (s_litf) {
            id<MTLTexture> g[3] = { s_galb, s_gpos, s_gnrm };
            int k;
            for (k = 0; k < 3; k++) {
                rp.colorAttachments[1 + k].texture = g[k];
                rp.colorAttachments[1 + k].loadAction = MTLLoadActionClear;
                rp.colorAttachments[1 + k].clearColor = MTLClearColorMake(0, 0, 0, 0);
                rp.colorAttachments[1 + k].storeAction = MTLStoreActionStore;
            }
        }
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
        [e setRenderPipelineState:s_litf ? s_gclear[colour ? 1 : 0] : s_clear[colour ? 1 : 0]];
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
                o[i].wld[0] = v[i].wx;
                o[i].wld[1] = v[i].wy;
                o[i].wld[2] = v[i].wz;
                o[i].wld[3] = s_litf ? v[i].lit : 0.0f;
            }
        }
        if (s_litf && v[0].lit > 0.5f && st->blend_rgb_src == 4 && st->blend_rgb_dst == 0) {
            if (s_ntris + (size_t)n > s_captris) {          /* the shadow map's triangles */
                size_t c = s_captris ? s_captris * 2 : 65536;
                while (c < s_ntris + (size_t)n)
                    c *= 2;
                float *p = realloc(s_tris, c * 3 * sizeof(float));
                if (p) {
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
        [e setRenderPipelineState:s_litf ? gpipe_for(v[0].lit > 0.5f, st->blend_rgb_src, st->blend_rgb_dst)
                                         : pipe_for(st->blend_rgb_src, st->blend_rgb_dst)];
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
        if (s_litf)
            [e setRenderPipelineState:d[2] - d[0] == w && d[3] - d[1] == h ? s_glfb : s_glfbsharp];
        else
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

/* ---- the Remastered lighting at the end of a lit frame ---------------------------------- */
typedef struct LU { float sunvp[16]; float eye[4], sun[4], sunc[4], skyc[4], gndc[4], misc[4]; } LU;

/* the light per weather, in physical units: the sun's (or moon's)
 * illuminance and colour, the sky's and the ground's light on a surface
 * facing them, lux; the camera's exposure value at ISO 100 (the "sunny 16"
 * rule puts a sunlit scene near 15) and how glossy the ground is (wet) */
static const struct { float sun, sky, gnd, ev, spec; float sunc[3], skyc[3], gndc[3]; } k_weather[5] = {
    /* sunny */ { 100000, 20000, 6000, 14.8f, 0.15f, { 1.00f, 0.95f, 0.88f }, { 0.55f, 0.72f, 1.00f }, { 0.75f, 0.72f, 0.62f } },
    /* fog   */ {  12000, 16000, 6000, 13.0f, 0.30f, { 1.00f, 0.97f, 0.92f }, { 0.85f, 0.88f, 0.92f }, { 0.70f, 0.70f, 0.68f } },
    /* rain  */ {   6000, 11000, 3500, 12.3f, 1.00f, { 0.95f, 0.97f, 1.00f }, { 0.70f, 0.76f, 0.85f }, { 0.55f, 0.56f, 0.56f } },
    /* snow  */ {  25000, 22000, 16000, 13.8f, 0.30f, { 1.00f, 0.98f, 0.95f }, { 0.80f, 0.86f, 1.00f }, { 0.95f, 0.97f, 1.00f } },
    /* night */ {   0.30f, 0.08f, 0.02f, -2.2f, 0.20f, { 0.75f, 0.82f, 1.00f }, { 0.35f, 0.45f, 0.75f }, { 0.40f, 0.40f, 0.40f } },
};

/* the exposure: the frame's mean log luminance, measured by the lighting
 * pass into one of three buffers (read back three frames later, when its
 * frame is done), sets the camera's EV100 as a light meter does (sensor
 * calibration K = 12.5), adapting smoothly (Lagarde and de Rousiers 2014) */
static id<MTLBuffer> s_lum[3];
static float s_ev = 1e9f;

static void relight(void)
{
    LU u;
    float eye[3] = { 0, 0, 0 }, sun[3] = { 3.0f, 1.0f, 1.1f }, len;
    int w = light_game_weather(), k;
    memset(&u, 0, sizeof u);
    light_capture_eye(eye);
    len = sqrtf(sun[0] * sun[0] + sun[1] * sun[1] + sun[2] * sun[2]);
    for (k = 0; k < 3; k++) {
        sun[k] /= len;
        u.eye[k] = eye[k];
        u.sun[k] = sun[k];
        u.sunc[k] = k_weather[w].sunc[k] * k_weather[w].sun;
        u.skyc[k] = k_weather[w].skyc[k] * k_weather[w].sky;
        u.gndc[k] = k_weather[w].gndc[k] * k_weather[w].gnd;
    }
    {   /* white balance: the camera balanced to the sun and sky together, as a camera's auto white balance */
        float wb[3], y;
        for (k = 0; k < 3; k++)
            wb[k] = k_weather[w].sunc[k] * k_weather[w].sun * 0.5f + k_weather[w].skyc[k] * k_weather[w].sky;
        y = 0.2126f * wb[0] + 0.7152f * wb[1] + 0.0722f * wb[2];
        u.sunc[3] = y / wb[0];
        u.skyc[3] = y / wb[1];
        u.gndc[3] = y / wb[2];
    }
    {
        id<MTLBuffer> b;
        uint32_t *c;
        float target = k_weather[w].ev;
        if (!s_lum[0])
            for (k = 0; k < 3; k++)
                s_lum[k] = [s_dev newBufferWithLength:8 options:MTLResourceStorageModeShared];
        b = s_lum[s_vbi];
        c = (uint32_t *)[b contents];
        if (c[1] > 0) {
            float lavg = exp2f((float)c[0] / 256.0f / (float)c[1] - 20.0f);
            target = log2f(fmaxf(lavg, 1e-4f) * 100.0f / 12.5f);
            target = fminf(fmaxf(target, k_weather[w].ev - 3.0f), k_weather[w].ev + 3.0f);
        }
        c[0] = c[1] = 0;
        if (s_ev > 1e8f)
            s_ev = target;
        s_ev += (target - s_ev) * 0.05f;                     /* about a second to adapt at 60 frames */
        u.misc[0] = 1.0f / (1.2f * powf(2.0f, s_ev));
    }
    u.misc[2] = k_weather[w].spec;
    {   /* BR_LIGHT_VIEW: 1 albedo, 2 normals, 3 sun shadow, 4 the sun's angle (the debug view) */
        const char *e = getenv("BR_LIGHT_VIEW");
        u.misc[3] = e ? (float)atoi(e) : 0.0f;
    }
    if (!s_shadow) {
        MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                                                                     width:SHADOW_SIZE height:SHADOW_SIZE mipmapped:NO];
        td.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
        td.storageMode = MTLStorageModePrivate;
        s_shadow = [s_dev newTextureWithDescriptor:td];
    }
    if (s_ntris >= 3) {
        /* the sun's view: an orthographic box around the frame's 3D, looking along the light */
        float f[3] = { -sun[0], -sun[1], -sun[2] }, up[3] = { 0, 0, 1 }, r[3], v[3], lo[3] = { 1e30f, 1e30f, 1e30f },
              hi[3] = { -1e30f, -1e30f, -1e30f }, m[16];
        size_t i;
        r[0] = f[1] * up[2] - f[2] * up[1]; r[1] = f[2] * up[0] - f[0] * up[2]; r[2] = f[0] * up[1] - f[1] * up[0];
        len = sqrtf(r[0] * r[0] + r[1] * r[1] + r[2] * r[2]);
        for (k = 0; k < 3; k++) r[k] /= len;
        v[0] = r[1] * f[2] - r[2] * f[1]; v[1] = r[2] * f[0] - r[0] * f[2]; v[2] = r[0] * f[1] - r[1] * f[0];
        for (i = 0; i < s_ntris; i++) {
            const float *p = &s_tris[3 * i];
            float q[3] = { p[0] * r[0] + p[1] * r[1] + p[2] * r[2], p[0] * v[0] + p[1] * v[1] + p[2] * v[2],
                           p[0] * f[0] + p[1] * f[1] + p[2] * f[2] };
            for (k = 0; k < 3; k++) {
                lo[k] = fminf(lo[k], q[k]);
                hi[k] = fmaxf(hi[k], q[k]);
            }
        }
        lo[2] -= 50.0f;                                      /* casters just off the frame's edge */
        hi[2] += 1.0f;
        {
            float sx = 2.0f / (hi[0] - lo[0] + 1e-3f), sy = 2.0f / (hi[1] - lo[1] + 1e-3f), sz = 1.0f / (hi[2] - lo[2]);
            float ox = -(hi[0] + lo[0]) * 0.5f * sx, oy = -(hi[1] + lo[1]) * 0.5f * sy, oz = -lo[2] * sz;
            /* column-major: column j holds output row coefficients */
            m[0] = r[0] * sx; m[4] = r[1] * sx; m[8]  = r[2] * sx; m[12] = ox;
            m[1] = v[0] * sy; m[5] = v[1] * sy; m[9]  = v[2] * sy; m[13] = oy;
            m[2] = f[0] * sz; m[6] = f[1] * sz; m[10] = f[2] * sz; m[14] = oz;
            m[3] = 0;         m[7] = 0;         m[11] = 0;         m[15] = 1;
        }
        memcpy(u.sunvp, m, sizeof m);
        {
            MTLRenderPassDescriptor *rp = [MTLRenderPassDescriptor renderPassDescriptor];
            id<MTLRenderCommandEncoder> e;
            id<MTLBuffer> b = [s_dev newBufferWithBytes:s_tris length:s_ntris * 3 * sizeof(float)
                                                options:MTLResourceStorageModeShared];
            rp.depthAttachment.texture = s_shadow;
            rp.depthAttachment.loadAction = MTLLoadActionClear;
            rp.depthAttachment.clearDepth = 1.0;
            rp.depthAttachment.storeAction = MTLStoreActionStore;
            e = [s_cmd renderCommandEncoderWithDescriptor:rp];
            [e setRenderPipelineState:s_shpipe];
            [e setDepthStencilState:s_shds];
            [e setCullMode:MTLCullModeNone];
            [e setDepthBias:1.0f slopeScale:2.0f clamp:0.01f];
            [e setVertexBuffer:b offset:0 atIndex:0];
            [e setVertexBytes:m length:sizeof m atIndex:1];
            [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:s_ntris];
            [e endEncoding];
        }
        u.misc[1] = 1.0f;
    }
    {
        MTLRenderPassDescriptor *rp = [MTLRenderPassDescriptor renderPassDescriptor];
        id<MTLRenderCommandEncoder> e;
        rp.colorAttachments[0].texture = s_col;
        rp.colorAttachments[0].loadAction = MTLLoadActionLoad;
        rp.colorAttachments[0].storeAction = MTLStoreActionStore;
        e = [s_cmd renderCommandEncoderWithDescriptor:rp];
        [e setRenderPipelineState:s_lpipe];
        [e setFragmentBytes:&u length:sizeof u atIndex:0];
        [e setFragmentTexture:s_galb atIndex:0];
        [e setFragmentTexture:s_gpos atIndex:1];
        [e setFragmentTexture:s_gnrm atIndex:2];
        [e setFragmentTexture:s_shadow atIndex:3];
        [e setFragmentBuffer:s_lum[s_vbi] offset:0 atIndex:1];
        [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        [e endEncoding];
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
        enc();                                 /* a frame with nothing drawn still presents */
        [s_enc endEncoding];
        s_enc = nil;
        if (s_litf && s_galb)
            relight();
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
