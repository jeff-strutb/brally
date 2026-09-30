/* host_glide.m -- glide2x.dll on Metal (port code).
 *
 * The 38 Glide 2 entry points BRGlide.dll imports, drawn with Metal into a
 * colour + depth target the size of the window's 4:3 area (640x480 headless,
 * or BR_RES=WxH; at most BR_MAXPIX pixels), re-sized at any swap when the
 * window changes; grBufferSwap copies it into the window.  The game's
 * coordinates stay 640x480 throughout; only the target is larger.
 *
 *   geometry     Glide vertices are already in screen space: the vertex
 *                shader only maps pixels to NDC.  The native renderer's
 *                triangles (native/render.m, hglide_tri_h) arrive in clip
 *                space instead, and the GPU projects and clips them.
 *                Colour and the texture
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
 *   LFB writes   drawn as textured quads, in order with everything else.
 *   batching     consecutive triangles that share every piece of state are
 *                one draw.
 */
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>
#include "host.h"
#include <stdarg.h>
#include <execinfo.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define W 640
#define H 480
static int RW = W, RH = H;     /* the render target, in pixels */

CAMetalLayer *happ_metal_layer(void);   /* host_app.m; nil when headless */

typedef struct { float x, y, ooz, oow, r, g, b, a, sow, tow; } gv;
typedef struct {
    int cc_func, cc_fact, cc_local, cc_other, cc_inv;
    int ac_func, ac_fact, ac_local, ac_other, ac_inv;
    int tc_rfunc, tc_rfact, tc_afunc, tc_afact, tc_rinv, tc_ainv;
    int at_fn, at_ref, fogmode, dmode, use_tex, clear;
    float clear_depth, pad0;
    float cconst[4], fogcolor[4], clearcol[4];
    float su, sv, lodbias, pad2;
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
"  float clear_depth, pad0; float4 cconst, fogcolor, clearcol; float su, sv, lodbias, pad2;\n"
"  float fogtab[64]; };\n"
"struct VO { float4 pos [[position]];\n"
"  float4 col [[center_no_perspective]]; float ooz [[center_no_perspective]];\n"
"  float oow [[center_no_perspective]]; float sow [[center_no_perspective]];\n"
"  float tow [[center_no_perspective]];\n"
"  float pw, ps, pt; float4 pcol; int pm [[flat]];   /* pm: w, s, t, colour perspective-correct (vsc) */\n"
"  float3 wp; float fl [[flat]]; };   /* fx G-buffer: world position, 1 main 3D / 2 sky / 3 other view */\n"
"vertex VO vs(uint vid [[vertex_id]], const device GV *v [[buffer(0)]]) {\n"
"  GV g = v[vid]; VO o;\n"
"  o.pos = float4(g.x / 320.0 - 1.0, 1.0 - g.y / 240.0, 0.5, 1.0);\n"
"  o.col = float4(g.r, g.g, g.b, g.a); o.ooz = g.ooz; o.oow = g.oow; o.sow = g.sow; o.tow = g.tow;\n"
"  o.pw = 1; o.ps = 0; o.pt = 0; o.pcol = 0; o.pm = 0; o.wp = 0; o.fl = 0;\n"
"  return o; }\n"
"/* clip-space corners from the native renderer: the GPU divides by w; 1/w,\n"
"   s/w and t/w are carried unperspective, the values the Voodoo iterates */\n"
"struct CV { float x, y, z, w, r, g, b, a, s, t, wx, wy, wz, fl; };\n"
"vertex VO vsc(uint vid [[vertex_id]], const device CV *v [[buffer(0)]]) {\n"
"  CV g = v[vid]; VO o; float q = 1.0 / g.w;\n"
"  o.pos = float4(g.x, g.y, g.z, g.w);\n"
"  o.col = float4(g.r, g.g, g.b, g.a); o.ooz = 0; o.oow = q; o.sow = g.s * q; o.tow = g.t * q;\n"
"  o.pw = g.w; o.ps = g.s; o.pt = g.t; o.pcol = o.col; o.pm = 1; o.wp = float3(g.wx, g.wy, g.wz); o.fl = g.fl;\n"
"  return o; }\n"
"/* the same corners drawn without the depth buffer: the no-Z vertex routines\n"
"   overwrite 1/w with 1/65535 after projecting, so the card writes the far\n"
"   depth and maps the texture without perspective */\n"
"vertex VO vscn(uint vid [[vertex_id]], const device CV *v [[buffer(0)]]) {\n"
"  CV g = v[vid]; VO o; float q = 1.0 / 65535.0;\n"
"  o.pos = float4(g.x, g.y, g.z, g.w);\n"
"  o.col = float4(g.r, g.g, g.b, g.a); o.ooz = 0; o.oow = q; o.sow = g.s * q; o.tow = g.t * q;\n"
"  o.pw = 1; o.ps = 0; o.pt = 0; o.pcol = 0; o.pm = 0; o.wp = 0; o.fl = g.fl == 1.0 ? 2.0 : 0.0;\n"
"  return o; }\n"
"struct BV { float2 p; float2 uv; };\n"
"struct BO { float4 pos [[position]]; float2 uv; };\n"
"vertex BO bvs(uint vid [[vertex_id]], const device BV *v [[buffer(0)]]) {\n"
"  BO o; o.pos = float4(v[vid].p, 0, 1); o.uv = v[vid].uv; return o; }\n"
"fragment float4 bfs(BO in [[stage_in]], texture2d<float> t [[texture(0)]]) {\n"
"  /* sharp bilinear: whole texels at any scale, blended only across the one\n"
"     target pixel a texel edge falls in (plain bilinear at 1:1) */\n"
"  constexpr sampler s(filter::linear, address::clamp_to_edge);\n"
"  float2 ts = float2(t.get_width(), t.get_height()), px = in.uv * ts - 0.5;\n"
"  float2 i = floor(px), f = px - i, d = max(fwidth(px), float2(1e-5));\n"
"  f = clamp((f - 0.5) / d + 0.5, 0.0, 1.0);\n"
"  return t.sample(s, (i + 0.5 + f) / ts); }\n"
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
"struct FO { float4 c [[color(0)]]; float4 n [[color(1)]]; float4 g [[color(2)]]; float d [[depth(any)]]; };\n"
"/* The Voodoo's 16-bit W-buffer word: 4-bit exponent, 12-bit mantissa of\n"
"   1/w as a .32 fraction.  Depth is stored and compared at this precision,\n"
"   which is what lets a second pass over the same polygon (the car shadows\n"
"   use grDepthBufferFunction(EQUAL)) hit every pixel the first one wrote. */\n"
"uint wfloat(float oow) {\n"
"  if (oow >= 1.0) return 0;\n"
"  if (oow <= 0.0) return 0xFFFF;\n"
"  uint t = uint(min(oow * 4294967296.0, 4294967295.0));\n"
"  if (t == 0) return 0xFFFF;\n"
"  int e = int(clz(t));\n"
"  uint m = e <= 19 ? (~t >> uint(19 - e)) : (~t << uint(e - 19));\n"
"  uint w = (uint(e) << 12) | (m & 0xFFF);\n"
"  return w < 0xFFFF ? w + 1 : w; }\n"
"fragment FO fs(VO vin [[stage_in]], constant GU &u [[buffer(0)]],\n"
"               texture2d<float> tex [[texture(0)]], sampler smp [[sampler(0)]]) {\n"
"  FO o; VO in = vin;\n"
"  /* clip-space corners: w, s and t come perspective-correct, so 1/w, s/w and\n"
"     t/w are exact at every pixel, clipped by the GPU or not */\n"
"  if (in.pm) { in.oow = 1.0 / in.pw; in.sow = in.ps * in.oow; in.tow = in.pt * in.oow; in.col = in.pcol; }\n"
"  o.n = 0; o.g = 0;\n"
"  /* a clear is background: the sky, where the game draws none (night, storm) */\n"
"  if (u.clear) { o.c = u.clearcol / 255.0; o.d = u.clear_depth; o.n = float4(o.c.rgb, 1); o.g = float4(0, 0, 0, 3); return o; }\n"
"  float depth = 0;\n"
"  if (u.dmode == 2 || u.dmode == 4) depth = float(wfloat(in.oow)) / 65536.0;\n"
"  else if (u.dmode) depth = floor(clamp(in.ooz, 0.0, 65535.0)) / 65536.0;\n"
"  o.d = depth;\n"
"  float4 it = in.col; float4 t = float4(255);\n"
"  if (u.use_tex && in.oow != 0) {\n"
"    float2 st = float2(in.sow, in.tow) / in.oow;\n"
"    float4 raw = tex.sample(smp, st * float2(u.su, u.sv), bias(u.lodbias)) * 255.0;\n"
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
"  o.c = outc / 255.0;\n"
"  /* fx G-buffer (host_fx.m); the pipeline decides what reaches it */\n"
"  if (vin.fl == 1.0) {\n"
"    float3 nn = cross(dfdy(vin.wp), dfdx(vin.wp)); float kf = 0;\n"
"    if (u.fogmode & 1) { float w = in.oow != 0 ? 1.0 / in.oow : 65535.0; kf = fogof(u, w) / 255.0; }\n"
"    o.n = float4(length(nn) > 0 ? normalize(nn) : float3(0, 0, 1), 1); o.g = float4(vin.wp, 1.0 + 0.9 * kf);\n"
"  } else if (vin.fl == 2.0) { o.n = float4(o.c.rgb, 1); o.g = float4(0, 0, 0, 3); }   /* the sky keeps its own colour, for host_fx.m */\n"
"  else o.n = float4(0, 0, 0, o.c.a);   /* 2D: blended ones scale the scene coverage by 1 - a */\n"
"  return o; }\n"
"struct PO { float4 pos [[position]]; float2 uv; };\n"
"vertex PO pvs(uint vid [[vertex_id]]) {\n"
"  float2 p = float2((vid << 1) & 2, vid & 2); PO o;\n"
"  o.pos = float4(p * 2.0 - 1.0, 0, 1); o.uv = float2(p.x, 1.0 - p.y); return o; }\n"
"fragment float4 pfs(PO in [[stage_in]], texture2d<float> t [[texture(0)]]) {\n"
"  constexpr sampler s(filter::linear); return float4(t.sample(s, in.uv).rgb, 1); }\n";

static id<MTLDevice> g_dev;
static id<MTLCommandQueue> g_q;
static id<MTLLibrary> g_lib;
static id<MTLTexture> g_color, g_depth, g_white, g_gn, g_gp;
static int g_fx_fresh = 1;
int hfx_on(void);
void hfx_tick(void);
void hfx_shadow_batch(id<MTLBuffer> buf, size_t off, int n, id<MTLTexture> tex, id<MTLSamplerState> smp,
                      int at_fn, int at_ref, int use_tex, float su, float sv);
id<MTLTexture> hfx_run(id<MTLDevice> dev, id<MTLCommandBuffer> cb, id<MTLTexture> col, id<MTLTexture> gn,
                       id<MTLTexture> gp, int w, int h, int origin_ll, const float *fogc);
static id<MTLTexture> g_fxout;
static id<MTLRenderPipelineState> g_present, g_blit;
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
    int cull, dfunc, dmask, filter, minfilt, mipmode, lodblend, clamp_s, clamp_t;
    int cx0, cy0, cx1, cy1;
    u32 tex_start, tex_large, tex_aspect, tex_fmt, tex_small;
    int colfmt, origin_ll;
} S;

static u8 g_tmem[4 << 20];
typedef struct { u32 start, end, fmt, large, small, aspect; __unsafe_unretained id<MTLTexture> t, tm; } texent;
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
/* The target follows the window: its 4:3 area at the drawable's pixel size.
 * Called at setup and at every swap, the one point where no frame is being
 * drawn; the game clears what it draws each frame. */
static void size_targets(void)
{
    int w = W, h = H;
    const char *e = getenv("BR_RES");
    CAMetalLayer *l = happ_metal_layer();
    MTLTextureDescriptor *td;
    if (e && sscanf(e, "%dx%d", &w, &h) == 2 && w >= 64 && h >= 48)
        ;
    else if (l) {
        CGSize d = l.drawableSize;
        double k = fmin(d.width / W, d.height / H);
        w = W; h = H;
        if (k > 0.1) { w = (int)lround(W * k); h = (int)lround(H * k); }
        /* at most BR_MAXPIX pixels (default 8M): a 5K screen's full-screen
         * 4320x3240 cost 13-14 ms of GPU a frame, 3200x2400 about 8; above
         * the cap the present scales the picture up to the screen */
        {
            const char *m = getenv("BR_MAXPIX");
            double cap = m ? atof(m) : 8.0e6;
            if (cap >= 640.0 * 480.0 && (double)w * h > cap) {
                w = (int)floor(sqrt(cap * 4.0 / 3.0));
                w -= w % 4;
                h = w * 3 / 4;
            }
        }
    } else {
        w = W; h = H;
    }
    if (g_color && w == RW && h == RH) return;
    RW = w; RH = h;
    td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                            width:(NSUInteger)RW height:(NSUInteger)RH mipmapped:NO];
    td.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    td.storageMode = MTLStorageModeShared;
    g_color = [g_dev newTextureWithDescriptor:td];
    td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                                            width:(NSUInteger)RW height:(NSUInteger)RH mipmapped:NO];
    td.usage = MTLTextureUsageRenderTarget;
    td.storageMode = MTLStorageModePrivate;
    g_depth = [g_dev newTextureWithDescriptor:td];
    td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA16Float
                                                            width:(NSUInteger)RW height:(NSUInteger)RH mipmapped:NO];
    td.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    td.storageMode = MTLStorageModePrivate;
    g_gn = [g_dev newTextureWithDescriptor:td];
    td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float
                                                            width:(NSUInteger)RW height:(NSUInteger)RH mipmapped:NO];
    td.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    td.storageMode = MTLStorageModePrivate;
    g_gp = [g_dev newTextureWithDescriptor:td];
    g_fx_fresh = 1;
    HLOG("render target %dx%d\n", RW, RH);
}

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
    size_targets();
    td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                            width:1 height:1 mipmapped:NO];
    g_white = [g_dev newTextureWithDescriptor:td];
    { u32 w = 0xFFFFFFFFu; [g_white replaceRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0 withBytes:&w bytesPerRow:4]; }
    for (i = 0; i < 3; i++)
        g_vbuf[i] = [g_dev newBufferWithLength:(16u << 20) options:MTLResourceStorageModeShared];
    /* two frames in flight, and two drawables below: every frame queued
     * ahead of the display is ~17 ms more between moving the mouse and
     * seeing the game's cursor move; three made the pointer feel laggy. */
    g_inflight = dispatch_semaphore_create(2);
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
    pd = [MTLRenderPipelineDescriptor new];
    pd.vertexFunction = [g_lib newFunctionWithName:@"bvs"];
    pd.fragmentFunction = [g_lib newFunctionWithName:@"bfs"];
    pd.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
    pd.colorAttachments[1].pixelFormat = MTLPixelFormatRGBA16Float;
    pd.colorAttachments[1].writeMask = MTLColorWriteMaskNone;
    pd.colorAttachments[2].pixelFormat = MTLPixelFormatRGBA32Float;
    pd.colorAttachments[2].writeMask = MTLColorWriteMaskNone;
    pd.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
    g_blit = [g_dev newRenderPipelineStateWithDescriptor:pd error:&err];
    if (!g_blit) { fprintf(stderr, "blit pipeline: %s\n", err.localizedDescription.UTF8String); exit(1); }
    {
        CAMetalLayer *l = happ_metal_layer();
        if (l) { l.device = g_dev; l.pixelFormat = MTLPixelFormatBGRA8Unorm; l.framebufferOnly = YES; l.maximumDrawableCount = 2; }
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
static id<MTLRenderPipelineState> gpipe(int rs, int rd, int as, int ad, int nocolor, int kind)
{
    NSNumber *k = @(((long)kind << 32) | (rs << 24) | (rd << 16) | (as << 8) | ad | (nocolor << 30));
    id<MTLRenderPipelineState> p = g_pipes[k];
    if (!p) {
        NSError *err = nil;
        MTLRenderPipelineDescriptor *d = [MTLRenderPipelineDescriptor new];
        MTLRenderPipelineColorAttachmentDescriptor *c;
        d.vertexFunction = [g_lib newFunctionWithName:kind == 2 ? @"vscn" : kind ? @"vsc" : @"vs"];
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
        /* fx G-buffer: opaque draws write it; blended 3D leaves it; blended
         * 2D scales the scene coverage (normal.a) by 1 - its alpha */
        d.colorAttachments[1].pixelFormat = MTLPixelFormatRGBA16Float;
        d.colorAttachments[2].pixelFormat = MTLPixelFormatRGBA32Float;
        if (c.blendingEnabled) {
            d.colorAttachments[2].writeMask = MTLColorWriteMaskNone;
            if (kind) d.colorAttachments[1].writeMask = MTLColorWriteMaskNone;
            else {
                MTLRenderPipelineColorAttachmentDescriptor *n = d.colorAttachments[1];
                n.blendingEnabled = YES;
                n.sourceRGBBlendFactor = MTLBlendFactorZero; n.destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
                n.sourceAlphaBlendFactor = MTLBlendFactorZero; n.destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
            }
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
    NSNumber *k = @((S.lodblend << 6) | ((S.mipmode != 0) << 5) | (S.minfilt << 3) | (S.filter << 2) | (S.clamp_s << 1) | S.clamp_t);
    id<MTLSamplerState> s = g_samplers[k];
    if (!s) {
        MTLSamplerDescriptor *d = [MTLSamplerDescriptor new];
        d.magFilter = S.filter ? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest;
        d.minFilter = S.minfilt ? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest;
        /* GR_MIPMAP_DISABLE samples the large level only; NEAREST (and its
         * dithered form) picks one level; lodBlend is trilinear */
        d.mipFilter = !S.mipmode ? MTLSamplerMipFilterNotMipmapped
                    : S.lodblend ? MTLSamplerMipFilterLinear : MTLSamplerMipFilterNearest;
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
    rp.colorAttachments[1].texture = g_gn;
    rp.colorAttachments[2].texture = g_gp;
    rp.colorAttachments[1].loadAction = rp.colorAttachments[2].loadAction = g_fx_fresh ? MTLLoadActionClear : MTLLoadActionLoad;
    rp.colorAttachments[1].clearColor = rp.colorAttachments[2].clearColor = MTLClearColorMake(0, 0, 0, 0);
    rp.colorAttachments[1].storeAction = rp.colorAttachments[2].storeAction = MTLStoreActionStore;
    g_fx_fresh = 0;
    g_enc = [g_cb renderCommandEncoderWithDescriptor:rp];
    [g_enc setViewport:(MTLViewport){ 0, 0, RW, RH, 0, 1 }];
}

/* The pending run of triangles that share every piece of state. */
static struct {
    int n, kind, shadow;
    size_t off;
    id<MTLRenderPipelineState> pipe;
    id<MTLDepthStencilState> ds;
    id<MTLTexture> tex;
    id<MTLSamplerState> smp;
    MTLScissorRect sc;
    gu u;
} B;
static unsigned g_st_draws;
static void flush_batch(void)
{
    if (!B.n) return;
    [g_enc setRenderPipelineState:B.pipe];
    [g_enc setDepthStencilState:B.ds];
    [g_enc setScissorRect:B.sc];
    [g_enc setVertexBuffer:g_vbuf[g_vbi] offset:B.off atIndex:0];
    [g_enc setFragmentBytes:&B.u length:sizeof B.u atIndex:0];
    [g_enc setFragmentTexture:B.tex atIndex:0];
    [g_enc setFragmentSamplerState:B.smp atIndex:0];
    [g_enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:(NSUInteger)B.n];
    if (B.shadow)
        hfx_shadow_batch(g_vbuf[g_vbi], B.off, B.n, B.tex, B.smp, B.u.at_fn, B.u.at_ref, B.u.use_tex, B.u.su, B.u.sv);
    B.n = 0;
    B.pipe = nil; B.ds = nil; B.tex = nil; B.smp = nil;
    g_voff = (g_voff + 255) & ~(size_t)255;
    g_st_draws++;
}
static void end_pass(void)
{
    if (g_enc) { flush_batch(); [g_enc endEncoding]; g_enc = nil; }
}
/* finish all GPU work: needed before the CPU touches the target */
static void flush_wait(void)
{
    end_pass();
    if (g_cb) { [g_cb commit]; [g_cb waitUntilCompleted]; g_cb = nil; }
    else if (g_q) {
        /* nothing open: frames already committed may still be drawing; an
         * empty buffer behind them on the queue completes after they do */
        id<MTLCommandBuffer> b = [g_q commandBuffer];
        [b commit]; [b waitUntilCompleted];
    }
}

/* The clip window, in 640x480 units, as a scissor on the target. */
static MTLScissorRect scissor_rect(void)
{
    int x0 = S.cx0 < 0 ? 0 : S.cx0, y0 = S.cy0 < 0 ? 0 : S.cy0;
    int x1 = S.cx1 > W ? W : S.cx1, y1 = S.cy1 > H ? H : S.cy1;
    int px0, py0, px1, py1;
    if (x1 <= x0 || y1 <= y0) return (MTLScissorRect){ 0, 0, 1, 1 };
    if (S.origin_ll) { int t = H - y1; y1 = H - y0; y0 = t; }
    px0 = x0 * RW / W; px1 = x1 * RW / W; py0 = y0 * RH / H; py1 = y1 * RH / H;
    if (px1 <= px0 || py1 <= py0) return (MTLScissorRect){ 0, 0, 1, 1 };
    return (MTLScissorRect){ (NSUInteger)px0, (NSUInteger)py0,
                             (NSUInteger)(px1 - px0), (NSUInteger)(py1 - py0) };
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
/* The texture at tex_start as a Metal texture with every mip level the
 * application downloaded (large LOD first, each level packed after the
 * previous one, as grTexDownloadMipMap lays them out). */
static id<MTLTexture> cur_texture(void)
{
    int i, w, h, l, nlev, small = (int)S.tex_small < (int)S.tex_large ? (int)S.tex_large : (int)S.tex_small;
    u32 n, off;
    MTLTextureDescriptor *td;
    id<MTLTexture> t;
    u32 *px;
    for (i = 0; i < g_ntex; i++)
        if (g_tex[i].t && g_tex[i].start == S.tex_start && g_tex[i].fmt == S.tex_fmt &&
            g_tex[i].large == S.tex_large && g_tex[i].small == (u32)small && g_tex[i].aspect == S.tex_aspect)
            return g_tex[i].t;
    n = tex_bytes(small, (int)S.tex_large, (int)S.tex_aspect, (int)S.tex_fmt);
    if (S.tex_start + n > sizeof g_tmem) return g_white;
    nlev = small - (int)S.tex_large + 1;
    lod_dims((int)S.tex_large, (int)S.tex_aspect, &w, &h);
    td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                            width:(NSUInteger)w height:(NSUInteger)h mipmapped:nlev > 1];
    if (nlev > 1) td.mipmapLevelCount = (NSUInteger)nlev;
    t = [g_dev newTextureWithDescriptor:td];
    px = malloc((size_t)(w * h * 4));
    for (l = 0, off = S.tex_start; l < nlev; l++) {
        int lw, lh;
        lod_dims((int)S.tex_large + l, (int)S.tex_aspect, &lw, &lh);
        decode(g_tmem + off, (int)S.tex_fmt, lw, lh, px);
        [t replaceRegion:MTLRegionMake2D(0, 0, (NSUInteger)lw, (NSUInteger)lh) mipmapLevel:(NSUInteger)l
               withBytes:px bytesPerRow:(NSUInteger)lw * 4];
        off += (u32)(lw * lh * fmt_bpp((int)S.tex_fmt));
    }
    free(px);
    for (i = 0; i < g_ntex && g_tex[i].t; i++) ;
    if (i == 512) { i = rand() % 512; [g_texobjs removeObject:g_tex[i].t]; if (g_tex[i].tm) [g_texobjs removeObject:g_tex[i].tm]; }
    if (i == g_ntex) g_ntex++;
    [g_texobjs addObject:t];
    g_tex[i] = (texent){ S.tex_start, S.tex_start + n, S.tex_fmt, S.tex_large, (u32)small, S.tex_aspect, t, nil };
    return t;
}

/* Remastered (host_fx.m) samples 3D textures with anisotropic trilinear
 * filtering, as a modern renderer would: the game's own mip chain where it
 * downloaded one, else a full chain generated once from its single level.
 * The Original look never sees either. */
static id<MTLTexture> fx_texture(id<MTLTexture> t)
{
    int i;
    for (i = 0; i < g_ntex; i++) if (g_tex[i].t == t) break;
    if (i == g_ntex) return t;
    if (!g_tex[i].tm) {
        id<MTLTexture> m = t;
        if (t.mipmapLevelCount == 1 && (t.width > 1 || t.height > 1)) {
            MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                           width:t.width height:t.height mipmapped:YES];
            id<MTLCommandBuffer> b = [g_q commandBuffer];
            id<MTLBlitCommandEncoder> e;
            m = [g_dev newTextureWithDescriptor:td];
            e = [b blitCommandEncoder];
            [e copyFromTexture:t sourceSlice:0 sourceLevel:0 toTexture:m destinationSlice:0 destinationLevel:0 sliceCount:1 levelCount:1];
            [e generateMipmapsForTexture:m];
            [e endEncoding];
            [b commit];
            [g_texobjs addObject:m];
        }
        g_tex[i].tm = m;
    }
    return g_tex[i].tm;
}
static id<MTLSamplerState> fx_samp(void)
{
    NSNumber *k = @((1 << 7) | (S.clamp_s << 1) | S.clamp_t);
    id<MTLSamplerState> s = g_samplers[k];
    if (!s) {
        MTLSamplerDescriptor *d = [MTLSamplerDescriptor new];
        d.magFilter = d.minFilter = MTLSamplerMinMagFilterLinear;
        d.mipFilter = MTLSamplerMipFilterLinear;
        d.maxAnisotropy = 16;
        d.sAddressMode = S.clamp_s ? MTLSamplerAddressModeClampToEdge : MTLSamplerAddressModeRepeat;
        d.tAddressMode = S.clamp_t ? MTLSamplerAddressModeClampToEdge : MTLSamplerAddressModeRepeat;
        s = [g_dev newSamplerStateWithDescriptor:d];
        g_samplers[k] = s;
    }
    return s;
}

/* BR_GLLOG=path: with BR_TRACE_FRAMES, the Glide call stream of the traced
 * frames, one call per line, vertices expanded -- the same text brbox's
 * recorder (tools/brbox_imports.py on_glide) is formatted to, so the port's
 * rendering can be diffed against the original's call by call. */
static FILE *g_gllog;
static void gllog(const char *fmt, ...)
{
    static int init;
    va_list ap;
    if (!w_tracing) return;
    if (!init) { const char *p = getenv("BR_GLLOG"); init = 1; if (p) g_gllog = fopen(p, "w"); if (g_gllog) setvbuf(g_gllog, NULL, _IONBF, 0); }
    if (!g_gllog) return;
    va_start(ap, fmt); vfprintf(g_gllog, fmt, ap); va_end(ap);
    fputc('\n', g_gllog);
}
static void gllog_vtx(const char *what, u32 p)
{
    const float *f = (const float *)W_P(p);
    if (!w_tracing || !g_gllog) return;
    fprintf(g_gllog, "  %s %.4g %.4g z%.4g w%.4g st %.4g %.4g rgba %.4g %.4g %.4g %.4g\n", what,
            f[0], f[1], f[6], f[8], f[9], f[10], f[3], f[4], f[5], f[7]);
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
    if (w_tracing && g_gllog && data) {
        u32 hsh = 2166136261u, k; const u8 *q = (const u8 *)W_P(data);
        for (k = 0; k < n; k++) hsh = (hsh ^ q[k]) * 16777619u;
        gllog("grTexDownloadMipMap start %u n %u fmt %u hash %08X", start, n, H32(info + 12), hsh);
    }
    if (start >= sizeof g_tmem) return;
    if (start + n > sizeof g_tmem) n = (u32)sizeof g_tmem - start;
    if (data) memcpy(g_tmem + start, W_P(data), n);
    end = start + n;
    /* anything decoded from the bytes just overwritten is stale */
    for (i = 0; i < g_ntex; i++)
        if (g_tex[i].t && g_tex[i].start < end && start < g_tex[i].end) {
            [g_texobjs removeObject:g_tex[i].t];
            if (g_tex[i].tm) [g_texobjs removeObject:g_tex[i].tm];
            g_tex[i].t = nil; g_tex[i].tm = nil;
        }
}
void h_grTexSource(u32 tmu, u32 start, u32 eo, u32 info)
{
    gllog("grTexSource start %u fmt %u large %u aspect %u", start, H32(info + 12), H32(info + 4), H32(info + 8));
    (void)tmu; (void)eo;
    S.tex_start = start; S.tex_small = H32(info); S.tex_large = H32(info + 4);
    S.tex_aspect = H32(info + 8); S.tex_fmt = H32(info + 12);
}
void h_grTexClampMode(u32 tmu, u32 s, u32 t) { (void)tmu; S.clamp_s = (int)s; S.clamp_t = (int)t; }
void h_grTexFilterMode(u32 tmu, u32 mn, u32 mg) { (void)tmu; S.minfilt = (int)mn; S.filter = (int)mg; }
void h_grTexMipMapMode(u32 tmu, u32 m, u32 b) { (void)tmu; S.mipmode = (int)m; S.lodblend = b != 0; }
/* Glide 2 packs the bias into the TMU's 6-bit two's-complement quarter-LOD
 * field: (int)((bias + .125) / .25) & 0x3F, unclamped in release builds.
 * The x86 float->int conversion gives 0x80000000 when out of range -- and
 * the game does pass out-of-range values (0x10028420 converts its -8 field
 * as unsigned, so 4294967288 * 0.25), which the card therefore sees as 0. */
void h_grTexLodBiasValue(u32 tmu, f32 b)
{
    double q = ((double)b + 0.125) / 0.25;
    int v = (q >= -2147483648.0 && q < 2147483648.0) ? (int)q : (int)0x80000000u;
    v &= 0x3F;
    if (v & 0x20) v -= 64;
    (void)tmu;
    U.lodbias = (float)v * 0.25f;
}
void h_grTexCombine(u32 tmu, u32 rf, u32 rfa, u32 af, u32 afa, u32 ri, u32 ai)
{
    gllog("grTexCombine %u %u %u %u %u %u", rf, rfa, af, afa, ri, ai);
    (void)tmu;
    U.tc_rfunc = (int)rf; U.tc_rfact = (int)rfa; U.tc_afunc = (int)af; U.tc_afact = (int)afa;
    U.tc_rinv = (int)ri; U.tc_ainv = (int)ai;
}
void h_grColorCombine(u32 f, u32 fa, u32 l, u32 o, u32 inv)
{ gllog("grColorCombine %u %u %u %u %u", f, fa, l, o, inv); U.cc_func = (int)f; U.cc_fact = (int)fa; U.cc_local = (int)l; U.cc_other = (int)o; U.cc_inv = (int)inv; }
void h_grAlphaCombine(u32 f, u32 fa, u32 l, u32 o, u32 inv)
{ gllog("grAlphaCombine %u %u %u %u %u", f, fa, l, o, inv); U.ac_func = (int)f; U.ac_fact = (int)fa; U.ac_local = (int)l; U.ac_other = (int)o; U.ac_inv = (int)inv; }
void h_grAlphaBlendFunction(u32 rs, u32 rd, u32 as, u32 ad)
{ gllog("grAlphaBlendFunction %u %u %u %u", rs, rd, as, ad); S.ab_rs = (int)rs; S.ab_rd = (int)rd; S.ab_as = (int)as; S.ab_ad = (int)ad; }
void h_grAlphaTestFunction(u32 f) { U.at_fn = (int)f; }
void h_grAlphaTestReferenceValue(u32 v) { U.at_ref = (int)(v & 0xFF); }
void h_grConstantColorValue(u32 c) { gllog("grConstantColorValue %08X", c); argb4(to_argb(c), U.cconst); }
void h_grCullMode(u32 m) { S.cull = (int)m; }
void h_grDepthBufferMode(u32 m) { gllog("grDepthBufferMode %u", m); U.dmode = (int)m; }
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
{ gllog("grClipWindow %u %u %u %u", x0, y0, x1, y1); S.cx0 = (int)x0; S.cy0 = (int)y0; S.cx1 = (int)x1; S.cy1 = (int)y1; }
u32 h_grBufferNumPending(void) { return 0; }

/* BR_PICK=x,y: with BR_TRACE_FRAMES, every draw inside the traced frames
 * that covers framebuffer pixel (x,y), with the state it was drawn under. */
static void pick(const gv *v, int n)
{
    static int init, px = -1, py = -1;
    int i;
    if (!init) { const char *e = getenv("BR_PICK"); init = 1; if (e) sscanf(e, "%d,%d", &px, &py); }
    if (px < 0 || !w_tracing) return;
    for (i = 0; i + 2 < n; i += 3) {
        const gv *a = &v[i], *b = &v[i + 1], *c = &v[i + 2];
        float x = (float)px + 0.5f, y = (float)py + 0.5f;
        float d1 = (b->x - a->x) * (y - a->y) - (b->y - a->y) * (x - a->x);
        float d2 = (c->x - b->x) * (y - b->y) - (c->y - b->y) * (x - b->x);
        float d3 = (a->x - c->x) * (y - c->y) - (a->y - c->y) * (x - c->x);
        if ((d1 < 0 || d2 < 0 || d3 < 0) && (d1 > 0 || d2 > 0 || d3 > 0)) continue;
        fprintf(stderr, "pick: tex start %u fmt %u large %u small %u aspect %u mip %d/%d bias %g | cc %d %d %d %d %d ac %d %d %d %d %d tc %d %d %d %d | ab %d %d %d %d at %d/%d | filt %d clamp %d%d | dm %d df %d mask %d | cconst %g %g %g %g\n"
                "pick:   (%g,%g w%g s%g t%g a%g) (%g,%g w%g s%g t%g a%g) (%g,%g w%g s%g t%g a%g)\n",
                S.tex_start, S.tex_fmt, S.tex_large, S.tex_small, S.tex_aspect, S.mipmode, S.lodblend, U.lodbias,
                U.cc_func, U.cc_fact, U.cc_local, U.cc_other, U.cc_inv,
                U.ac_func, U.ac_fact, U.ac_local, U.ac_other, U.ac_inv,
                U.tc_rfunc, U.tc_rfact, U.tc_afunc, U.tc_afact,
                S.ab_rs, S.ab_rd, S.ab_as, S.ab_ad, U.at_fn, U.at_ref,
                S.filter, S.clamp_s, S.clamp_t, U.dmode, S.dfunc, S.dmask,
                U.cconst[0], U.cconst[1], U.cconst[2], U.cconst[3],
                a->x, a->y, a->oow, a->sow, a->tow, a->a, b->x, b->y, b->oow, b->sow, b->tow, b->a,
                c->x, c->y, c->oow, c->sow, c->tow, c->a);
        { int k; fprintf(stderr, "pick:   tmem"); for (k = 0; k < 32; k++) fprintf(stderr, " %02X", g_tmem[S.tex_start + k]); fputc('\n', stderr); }
        return;
    }
}

/* v: n corners of `kind` 0 (gv, screen space) or 1 (clip space, 10 floats
 * too).  Appended to the pending run when every piece of state matches. */
static void draw(const void *v, int n, int clear, int kind)
{
    size_t bytes = (size_t)n * (kind ? 14 * sizeof(float) : sizeof(gv));
    id<MTLTexture> t = g_white;
    id<MTLRenderPipelineState> pipe;
    id<MTLDepthStencilState> ds;
    id<MTLSamplerState> smp;
    MTLScissorRect sc;
    if (!clear && !kind) pick((const gv *)v, n);
    begin_pass();
    if (g_voff + bytes + 256 > (16u << 20)) {
        /* a vertex-heavy frame: flush and start over in the next buffer */
        end_pass();
        [g_cb commit];
        g_cb = nil;
        begin_pass();
    }
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
    pipe = clear ? gpipe(4, 0, 4, 0, 0, 0) : gpipe(S.ab_rs, S.ab_rd, S.ab_as, S.ab_ad, 0, kind);
    ds = clear ? dss(1, 7, 1) : dss(U.dmode != 0, S.dfunc, S.dmask);
    smp = samp();
    if (kind && U.use_tex && hfx_on()) { t = fx_texture(t); smp = fx_samp(); }
    sc = scissor_rect();
    if (B.n && (B.kind != kind || B.pipe != pipe || B.ds != ds || B.tex != t || B.smp != smp ||
                memcmp(&B.sc, &sc, sizeof sc) || memcmp(&B.u, &U, sizeof U)))
        flush_batch();
    if (!B.n) {
        B.off = g_voff; B.kind = kind; B.pipe = pipe; B.ds = ds; B.tex = t; B.smp = smp;
        B.shadow = kind == 1 && S.ab_rs == 4 && S.ab_rd == 0 && S.dmask && U.dmode && hfx_on();
        B.sc = sc; B.u = U;
    }
    memcpy((u8 *)g_vbuf[g_vbi].contents + g_voff, v, bytes);
    g_voff += bytes;
    B.n += n;
}

/* A native mesh drawn in display-list order (host_car.m): the pending run is
 * drawn first, then the frame's encoder is handed over with what the mesh
 * needs to match the scene -- the clip window, the origin, the fog.  The
 * caller sets every piece of encoder state it uses and leaves the viewport
 * and cull mode as it found them; flush_batch sets the rest for the next run. */
id<MTLRenderCommandEncoder> hglide_native_pass(id<MTLDevice> *dev, MTLScissorRect *sc, int *origin_ll,
                                               int *fogmode, float *fogcolor, float *fogtab)
{
    begin_pass();
    flush_batch();
    *dev = g_dev;
    *sc = scissor_rect();
    *origin_ll = S.origin_ll;
    *fogmode = U.fogmode;
    memcpy(fogcolor, U.fogcolor, sizeof U.fogcolor);
    memcpy(fogtab, U.fogtab, sizeof U.fogtab);
    return g_enc;
}

void h_grBufferClear(u32 color, u32 alpha, u32 depth)
{
    gllog("grBufferClear %08X %u %u", color, alpha, depth);
    gv q[6];
    float x0 = 0, y0 = 0, x1 = W, y1 = H;
    int i;
    argb4((to_argb(color) & 0x00FFFFFFu) | ((alpha & 0xFF) << 24), U.clearcol);
    U.clear_depth = (float)(depth & 0xFFFF) / 65536.0f;
    memset(q, 0, sizeof q);
    q[0].x = x0; q[0].y = y0; q[1].x = x1; q[1].y = y0; q[2].x = x0; q[2].y = y1;
    q[3].x = x1; q[3].y = y0; q[4].x = x1; q[4].y = y1; q[5].x = x0; q[5].y = y1;
    for (i = 0; i < 6; i++) q[i].oow = 1;
    draw(q, 6, 1, 0);              /* the scissor keeps it inside the clip window */
}

/* The current colour target as a binary PPM. */
void hglide_shot(const char *path)
{
    FILE *f;
    u32 *px;
    int i;
    flush_wait();
    px = malloc((size_t)RW * RH * 4);
    [(g_fxout ? g_fxout : g_color) getBytes:px bytesPerRow:(NSUInteger)RW * 4 fromRegion:MTLRegionMake2D(0, 0, (NSUInteger)RW, (NSUInteger)RH) mipmapLevel:0];
    f = fopen(path, "wb");
    if (f) {
        fprintf(f, "P6\n%d %d\n255\n", RW, RH);
        for (i = 0; i < RW * RH; i++) { u8 c[3] = { (u8)px[i], (u8)(px[i] >> 8), (u8)(px[i] >> 16) }; fwrite(c, 1, 3, f); }
        fclose(f);
    }
    if (g_fxout && getenv("BR_FX_SHOTC")) {       /* the game's own colour beside it */
        char q[1024];
        snprintf(q, sizeof q, "%s.c.ppm", path);
        [g_color getBytes:px bytesPerRow:(NSUInteger)RW * 4 fromRegion:MTLRegionMake2D(0, 0, (NSUInteger)RW, (NSUInteger)RH) mipmapLevel:0];
        f = fopen(q, "wb");
        if (f) {
            fprintf(f, "P6\n%d %d\n255\n", RW, RH);
            for (i = 0; i < RW * RH; i++) { u8 c[3] = { (u8)px[i], (u8)(px[i] >> 8), (u8)(px[i] >> 16) }; fwrite(c, 1, 3, f); }
            fclose(f);
        }
    }
    free(px);
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
    px = malloc((size_t)RW * RH * 4);
    [(g_fxout ? g_fxout : g_color) getBytes:px bytesPerRow:(NSUInteger)RW * 4 fromRegion:MTLRegionMake2D(0, 0, (NSUInteger)RW, (NSUInteger)RH) mipmapLevel:0];
    snprintf(p, sizeof p, "%s/frame%05d.ppm", dir, g_nshot - 1);
    f = fopen(p, "wb");
    if (f) {
        fprintf(f, "P6\n%d %d\n255\n", RW, RH);
        for (i = 0; i < RW * RH; i++) { u8 c[3] = { (u8)px[i], (u8)(px[i] >> 8), (u8)(px[i] >> 16) }; fwrite(c, 1, 3, f); }
        fclose(f);
    }
    free(px);
}

static void glstat_swap(void);
/* The frame loop (native/frame.m) presents, to time and log each frame;
 * without it, a plain present. */
__attribute__((weak)) void hframe_present(id<MTLCommandBuffer> cb, id<CAMetalDrawable> d)
{
    [cb presentDrawable:d];
}
static unsigned g_swaps;
unsigned hglide_swaps(void) { return g_swaps; }
void h_grBufferSwap(u32 interval)
{
    CAMetalLayer *l;
    (void)interval;
    begin_pass();
    end_pass();
    glstat_swap();
    if (getenv("BR_SWAPLOG")) {                 /* present-to-present interval, ms */
        static double last;
        double t = CACurrentMediaTime() * 1000.0;
        if (last) fprintf(stderr, "swap: %.2f\n", t - last);
        last = t;
    }
    gllog("grBufferSwap");
    g_fxout = nil;
    hfx_tick();
    if (hfx_on()) {
        float fc[3] = { powf(U.fogcolor[0] / 255.0f, 2.2f), powf(U.fogcolor[1] / 255.0f, 2.2f), powf(U.fogcolor[2] / 255.0f, 2.2f) };
        if (!g_cb) begin_pass(), end_pass();
        g_fxout = hfx_run(g_dev, g_cb, g_color, g_gn, g_gp, RW, RH, S.origin_ll, fc);
    }
    { void hcar_frame_end(id<MTLCommandBuffer> cb, id<MTLTexture> pic);
      if (!g_cb) begin_pass(), end_pass();
      hcar_frame_end(g_cb, g_fxout ? g_fxout : g_color); }
    g_fx_fresh = 1;
    g_swaps++;
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
                double dw = d.texture.width, dh = d.texture.height, s = fmin(dw / RW, dh / RH);
                [e setViewport:(MTLViewport){ floor((dw - RW * s) / 2), floor((dh - RH * s) / 2), RW * s, RH * s, 0, 1 }];
            }
            [e setRenderPipelineState:g_present];
            [e setFragmentTexture:g_fxout ? g_fxout : g_color atIndex:0];
            [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
            [e endEncoding];
            hframe_present(g_cb, d);
        }
    }
    [g_cb commit];
    g_cb = nil;
    size_targets();
    happ_pump(0);
}

/* LFB write: GR_LFB_SRC_FMT 565 0, 555 1, 1555 2, 888 4, 8888 5 */
u32 h_grLfbWriteRegion(u32 buf, u32 x, u32 y, u32 fmt, u32 w, u32 h, u32 stride, u32 data)
{
    u32 *px = malloc((size_t)w * h * 4), i, j;
    (void)buf;
    gllog("grLfbWriteRegion buf %u x %u y %u fmt %u w %u h %u stride %u", buf, x, y, fmt, w, h, stride);
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
    if (w && h) {
        /* the region as a texture, copied onto its 640x480 rectangle */
        MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                      width:w height:h mipmapped:NO];
        id<MTLTexture> t = [g_dev newTextureWithDescriptor:td];
        float x0 = (float)(int)x / (W / 2) - 1, x1 = (float)((int)x + (int)w) / (W / 2) - 1;
        float y0 = 1 - (float)(int)y / (H / 2), y1 = 1 - (float)((int)y + (int)h) / (H / 2);
        float q[6][4] = { { x0, y0, 0, 0 }, { x1, y0, 1, 0 }, { x0, y1, 0, 1 },
                          { x1, y0, 1, 0 }, { x1, y1, 1, 1 }, { x0, y1, 0, 1 } };
        [t replaceRegion:MTLRegionMake2D(0, 0, w, h) mipmapLevel:0 withBytes:px bytesPerRow:w * 4];
        begin_pass();
        flush_batch();
        [g_enc setRenderPipelineState:g_blit];
        [g_enc setDepthStencilState:dss(0, 7, 0)];
        [g_enc setScissorRect:(MTLScissorRect){ 0, 0, (NSUInteger)RW, (NSUInteger)RH }];
        [g_enc setVertexBytes:q length:sizeof q atIndex:0];
        [g_enc setFragmentTexture:t atIndex:0];
        [g_enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
        g_st_draws++;
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
    /* Glide judges winding in the coordinates the application passed.
     * load_vtx has already turned y over for a lower-left origin, which
     * reverses every triangle's sign; undo that here.  Culling the other
     * way dropped the sky, the sea and the near road. */
    if (S.origin_ll) area = -area;
    if (area == 0) return 1;
    if (S.cull == 1 && area < 0) return 1;
    if (S.cull == 2 && area > 0) return 1;
    return 0;
}
/* BR_GLSTAT: per-swap counts of triangles submitted and culled, to stderr
 * every 60 swaps -- tells "the game drew nothing" from "Metal dropped it". */
static unsigned g_st_tri, g_st_cull, g_st_poly, g_st_swaps;
static int g_st_on = -1;
static void glstat_swap(void)
{
    if (g_st_on < 0) g_st_on = getenv("BR_GLSTAT") != NULL;
    if (!g_st_on) return;
    if (++g_st_swaps % 60 == 0)
        { extern unsigned hdx_mouse_polls;
          fprintf(stderr, "glstat: swap %u: tri %u culled %u poly %u draws %u mouse polls %u (last 60 swaps)\n",
                  g_st_swaps, g_st_tri, g_st_cull, g_st_poly, g_st_draws, hdx_mouse_polls);
          hdx_mouse_polls = 0; }
    if (g_st_swaps % 60 == 0) g_st_tri = g_st_cull = g_st_poly = g_st_draws = 0;
}

void h_grDrawTriangle(u32 a, u32 b, u32 c)
{
    gv v[3];
    static int dumpn = -1;
    if (dumpn < 0) dumpn = getenv("BR_VTXDUMP") ? atoi(getenv("BR_VTXDUMP")) : 0;
    if (dumpn > 0 && w_tracing) {   /* with BR_TRACE_FRAMES: inside the traced frames */
        const float *fa = (const float *)W_P(a), *fb = (const float *)W_P(b), *fc = (const float *)W_P(c);
        fprintf(stderr, "vtx: [%08X %08X %08X] (%g,%g,w%g,s%g,t%g) (%g,%g,w%g,s%g,t%g) (%g,%g,w%g,s%g,t%g) tex=%d fmt=%u\n", a, b, c,
                fa[0], fa[1], fa[8], fa[9], fa[10], fb[0], fb[1], fb[8], fb[9], fb[10], fc[0], fc[1], fc[8], fc[9], fc[10],
                (U.cc_other == 1 || U.ac_other == 1 || (U.cc_fact & 7) == 4 || (U.ac_fact & 7) == 4), S.tex_fmt);
        if (dumpn == 1) {
            void *bt[8]; int k, nb = backtrace(bt, 8);
            char **sy = backtrace_symbols(bt, nb);
            for (k = 0; k < nb; k++) fprintf(stderr, "vtx-bt: %s\n", sy[k]);
        }
        dumpn--;
    }
    gllog("grDrawTriangle"); gllog_vtx("v", a); gllog_vtx("v", b); gllog_vtx("v", c);
    load_vtx(a, &v[0]); load_vtx(b, &v[1]); load_vtx(c, &v[2]);
    g_st_tri++;
    if (!culled(&v[0], &v[1], &v[2])) draw(v, 3, 0, 0);
    else g_st_cull++;
}

/* A triangle from the native renderer (native/render.m).  Each corner is
 * ten floats: X, Y, Z, W, r, g, b, a, s, t, where (X/W, Y/W) is the Glide
 * screen position the game would have computed, Z the clip-space z (-W..W
 * inside the near and far planes), and s, t the texture coordinates before
 * the divide.  The GPU divides, clips and interpolates; culling is Glide's
 * rule on the same winding, tested on the homogeneous corners (the sign of
 * the determinant is the screen-space winding of the part in front of the
 * eye, so a triangle through the near plane culls as the original's clipped
 * polygon did).  `noz`: the corners came from a no-Z vertex routine, which
 * sets 1/w to 1/65535 for the card (see vscn). */
void hglide_tri_h(const float *a, const float *b, const float *c, int noz)
{
    const float *p[3] = { a, b, c };
    float v[3][14], det;
    int i;
    det = a[0] * (b[1] * c[3] - c[1] * b[3]) - b[0] * (a[1] * c[3] - c[1] * a[3]) +
          c[0] * (a[1] * b[3] - b[1] * a[3]);
    g_st_tri++;
    if (det == 0 || (S.cull == 1 && det < 0) || (S.cull == 2 && det > 0)) { g_st_cull++; return; }
    if (a[3] > 0 && b[3] > 0 && c[3] > 0) {     /* BR_PICK sees them in screen space */
        gv g[3];
        for (i = 0; i < 3; i++) {
            memset(&g[i], 0, sizeof g[i]);
            g[i].x = p[i][0] / p[i][3];
            g[i].y = S.origin_ll ? (float)H - p[i][1] / p[i][3] : p[i][1] / p[i][3];
            g[i].oow = noz ? 1.0f / 65535.0f : 1.0f / p[i][3]; g[i].sow = p[i][8] * g[i].oow; g[i].tow = p[i][9] * g[i].oow;
            g[i].a = p[i][7];
        }
        pick(g, 3);
    }
    for (i = 0; i < 3; i++) {
        const float *q = p[i];
        v[i][0] = q[0] / (W / 2) - q[3];
        v[i][1] = S.origin_ll ? q[1] / (H / 2) - q[3] : q[3] - q[1] / (H / 2);
        v[i][2] = (q[2] + q[3]) * 0.5f;
        v[i][3] = q[3];
        memcpy(&v[i][4], &q[4], 10 * sizeof(float));
    }
    draw(v, 3, 0, noz ? 2 : 1);
}
/* the game's GrVertex is 0x3C bytes (two TMUs): include/br_imgblit.h
 *
 * Still live with the native triangle leaves: the textured-rectangle command
 * (0xE3/0xE4, HUD and text quads) clips a rectangle that crosses a screen
 * edge with the game's CPU clipper, which draws the result here. */
void h_grDrawPolygonVertexList(u32 n, u32 p)
{
    gv tri[3 * 64], v0, a, b;
    int k = 0;
    u32 i;
    if (n < 3) return;
    g_st_poly++;
    gllog("grDrawPolygonVertexList %u", n);
    { u32 k; for (k = 0; k < n && k < 16; k++) gllog_vtx("v", p + 60 * k); }
    if (w_tracing && getenv("BR_VTXDUMP")) {
        const float *f = (const float *)W_P(p);
        fprintf(stderr, "poly n%u: (%g,%g,oow%g,s%g,t%g) rgba(%g,%g,%g,%g) cc_f%d cc_fa%d cc_o%d ac_o%d fmt%u start%u\n", n,
                f[0], f[1], f[8], f[9], f[10], f[3], f[4], f[5], f[7],
                U.cc_func, U.cc_fact, U.cc_other, U.ac_other, S.tex_fmt, S.tex_start);
    }
    load_vtx(p, &v0);
    load_vtx(p + 60, &a);
    for (i = 2; i < n && k < 64; i++) {
        load_vtx(p + 60 * i, &b);
        if (!culled(&v0, &a, &b)) { tri[3 * k] = v0; tri[3 * k + 1] = a; tri[3 * k + 2] = b; k++; }
        a = b;
    }
    if (k) draw(tri, 3 * k, 0, 0);
}
