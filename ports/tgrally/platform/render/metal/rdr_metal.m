/* rdr_metal.m: the renderer interface (render/rdr.h) on Metal (macOS).
 *
 * The same rules as the software renderer (render/soft/rdr_soft.c), on the
 * GPU: frames are drawn offscreen at the N64's framebuffer size with a depth
 * buffer, the colour combiner and the tiles' wrap, mirror and mask rules are
 * evaluated in the fragment shader from the RDP state of each draw, alpha
 * compare discards, and the blender's modes are fixed-function blends.  A
 * finished frame is scaled into the window's Metal layer (the host's) at the
 * N64's 4:3, letterboxed. */
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <simd/simd.h>
#include <stdlib.h>
#include <string.h>
#include "../rdr.h"

CAMetalLayer *host_macos_metal_layer(void);
void host_macos_layer_fit(int *w, int *h);

static const char *k_shader =
"#include <metal_stdlib>\n"
"using namespace metal;\n"
"struct VIn { float4 pos [[attribute(0)]]; float2 st [[attribute(1)]]; float4 rgba [[attribute(2)]]; };\n"
"struct VOut { float4 pos [[position]]; float2 st; float4 rgba; };\n"
"struct Tile { float4 org; int4 size; int4 wrap; int4 cl; };\n"
"struct U { int4 cc[4]; float4 prim, env, fog, blendc; float plod; int cycle, filter, fog_blend;\n"
"           int alpha_cmp, ntex, balpha, lodn; float2 fb; Tile tile[10]; };\n"
"vertex VOut vs(VIn v [[stage_in]], constant U &u [[buffer(1)]]) {\n"
"  VOut o; float w = v.pos.w;\n"
"  o.pos = float4(v.pos.x / u.fb.x * 2.0 - w, w - v.pos.y / u.fb.y * 2.0, (v.pos.z + w) * 0.5, w);\n"
"  o.st = v.st; o.rgba = v.rgba; return o; }\n"
"int wrapi(int i, int n, int clampv, int mirror, int mask, int cw) {\n"
"  if (clampv != 0 && cw > 0) i = clamp(i, 0, cw - 1);\n"
"  if (mask > 0) { int m = ((i % mask) + mask) % mask;\n"
"    if (mirror != 0 && (((i < 0 ? -i - 1 : i) / mask) & 1) != 0) m = mask - 1 - m;\n"
"    return min(m, n - 1); }\n"
"  return clamp(i, 0, n - 1); }\n"
"float4 texel(texture2d<float> t, constant Tile &tl, float2 st, int filter) {\n"
"  float x = st.x * tl.org.z - tl.org.x, y = st.y * tl.org.w - tl.org.y;\n"
"  int w = tl.size.x, h = tl.size.y;\n"
"  if (filter == 0) {\n"
"    int ix = wrapi(int(floor(x)), w, tl.wrap.x, tl.wrap.z, tl.size.z, tl.cl.x);\n"
"    int iy = wrapi(int(floor(y)), h, tl.wrap.y, tl.wrap.w, tl.size.w, tl.cl.y);\n"
"    return t.read(uint2(ix, iy)); }\n"
"  x -= 0.5; y -= 0.5; float fx = x - floor(x), fy = y - floor(y); int x0 = int(floor(x)), y0 = int(floor(y));\n"
"  int xa = wrapi(x0, w, tl.wrap.x, tl.wrap.z, tl.size.z, tl.cl.x), xb = wrapi(x0 + 1, w, tl.wrap.x, tl.wrap.z, tl.size.z, tl.cl.x);\n"
"  int ya = wrapi(y0, h, tl.wrap.y, tl.wrap.w, tl.size.w, tl.cl.y), yb = wrapi(y0 + 1, h, tl.wrap.y, tl.wrap.w, tl.size.w, tl.cl.y);\n"
"  float4 a = mix(t.read(uint2(xa, ya)), t.read(uint2(xb, ya)), fx);\n"
"  float4 b = mix(t.read(uint2(xa, yb)), t.read(uint2(xb, yb)), fx);\n"
"  return mix(a, b, fy); }\n"
"float4 inp(int c, float4 comb, float4 t0, float4 t1, float4 sh, constant U &u, float lfrac) {\n"
"  switch (c) {\n"
"  case 0: return comb; case 1: return t0; case 2: return t1; case 3: return u.prim; case 4: return sh;\n"
"  case 5: return u.env; case 6: return float4(1.0); case 8: return float4(0.5);\n"
"  case 11: return float4(comb.a); case 12: return float4(t0.a); case 13: return float4(t1.a);\n"
"  case 14: return float4(u.prim.a); case 15: return float4(sh.a); case 16: return float4(u.env.a);\n"
"  case 17: return float4(lfrac);\n"
"  case 18: return float4(u.plod);\n"
"  default: return float4(0.0); } }\n"
"fragment float4 fs(VOut i [[stage_in]], constant U &u [[buffer(1)]],\n"
"                   texture2d<float> t0t [[texture(0)]], texture2d<float> t1t [[texture(1)]],\n"
"                   array<texture2d<float>, 8> lt [[texture(2)]]) {\n"
"  float4 t0 = float4(0.0), t1 = float4(0.0), comb = float4(0.0); float lfrac = 0.0;\n"
"  if (u.lodn > 0) {\n"
"    float2 dx = dfdx(i.st), dy = dfdy(i.st);\n"
"    float lod = max(max(abs(dx.x), abs(dx.y)), max(abs(dy.x), abs(dy.y)));\n"
"    int level = 0;\n"
"    if (lod >= 1.0) { level = min(int(floor(log2(lod))), 7); lfrac = min(lod / exp2(float(level)) - 1.0, 1.0); }\n"
"    int a = min(level, u.lodn - 1), b = min(level + 1, u.lodn - 1);\n"
"    if (level >= u.lodn - 1) lfrac = 1.0;\n"
"    t0 = texel(lt[a], u.tile[2 + a], i.st, u.filter);\n"
"    t1 = texel(lt[b], u.tile[2 + b], i.st, u.filter);\n"
"  } else {\n"
"  if ((u.ntex & 1) != 0) t0 = texel(t0t, u.tile[0], i.st, u.filter);\n"
"  if ((u.ntex & 2) != 0) t1 = texel(t1t, u.tile[1], i.st, u.filter);\n"
"  }\n"
"  for (int c = 0; c < u.cycle; c++) {\n"
"    int4 r = u.cc[c * 2], a = u.cc[c * 2 + 1];\n"
"    float3 rgb = (inp(r.x, comb, t0, t1, i.rgba, u, lfrac).rgb - inp(r.y, comb, t0, t1, i.rgba, u, lfrac).rgb) *\n"
"                 inp(r.z, comb, t0, t1, i.rgba, u, lfrac).rgb + inp(r.w, comb, t0, t1, i.rgba, u, lfrac).rgb;\n"
"    float al = (inp(a.x, comb, t0, t1, i.rgba, u, lfrac).a - inp(a.y, comb, t0, t1, i.rgba, u, lfrac).a) *\n"
"               inp(a.z, comb, t0, t1, i.rgba, u, lfrac).a + inp(a.w, comb, t0, t1, i.rgba, u, lfrac).a;\n"
"    comb = clamp(float4(rgb, al), 0.0, 1.0); }\n"
"  if (u.alpha_cmp == 1 && comb.a < u.blendc.a) discard_fragment();\n"
"  if (u.alpha_cmp == 3 && comb.a < 0.5) discard_fragment();\n"
"  if (u.alpha_cmp == 4 && comb.a < 1.0 / 255.0) discard_fragment();\n"
"  if (u.fog_blend != 0) comb.rgb = mix(comb.rgb, u.fog.rgb, i.rgba.a);\n"
"  if (u.balpha == 1) comb.a = u.fog.a; else if (u.balpha == 2) comb.a = i.rgba.a;\n"
"  return comb; }\n"
"struct BOut { float4 pos [[position]]; float2 uv; };\n"
"vertex BOut bvs(uint vid [[vertex_id]], constant float4 &r [[buffer(0)]]) {\n"
"  float2 p = float2((vid & 1) ? 1.0 : 0.0, (vid & 2) ? 1.0 : 0.0);\n"
"  BOut o; o.pos = float4(r.x + p.x * r.z, r.y + p.y * r.w, 0.0, 1.0); o.uv = float2(p.x, 1.0 - p.y); return o; }\n"
"fragment float4 bfs(BOut i [[stage_in]], texture2d<float> t [[texture(0)]]) {\n"
"  constexpr sampler s(filter::linear); return t.sample(s, i.uv); }\n";

typedef struct { simd_float4 org; simd_int4 size; simd_int4 wrap; simd_int4 cl; } TileU;
typedef struct {
    simd_int4 cc[4];
    simd_float4 prim, env, fog, blendc;
    float plod;
    int cycle, filter, fog_blend;
    int alpha_cmp, ntex, balpha, lodn;
    simd_float2 fb;
    TileU tile[10];
} Uniforms;

static id<MTLDevice> s_dev;
static id<MTLCommandQueue> s_q;
static id<MTLRenderPipelineState> s_pipe[4], s_blit;
static id<MTLDepthStencilState> s_ds[2][2][2];       /* [test][write][decal] */
static id<MTLTexture> s_color, s_depth, s_dummy;
static id<MTLCommandBuffer> s_cb;
static id<MTLRenderCommandEncoder> s_enc;
static int s_fb_w, s_fb_h, s_first = 1;
static NSMutableArray *s_tex;                         /* handle -> texture (NSNull: free) */
static uint32_t *s_pixels;
static int s_px_w, s_px_h;

static CAMetalLayer *s_layer;

int rdr_presents(void) { return 1; }

/* the window's Metal layer, taken on the main thread when the window opens */
void rdr_window(void)
{
    s_layer = host_macos_metal_layer();
}

int rdr_init(void)
{
    @autoreleasepool {
        NSError *err = nil;
        id<MTLLibrary> lib;
        MTLRenderPipelineDescriptor *d;
        MTLVertexDescriptor *vd;
        int b, t, w, k;
        s_dev = MTLCreateSystemDefaultDevice();
        if (!s_dev)
            return 0;
        s_q = [s_dev newCommandQueue];
        lib = [s_dev newLibraryWithSource:[NSString stringWithUTF8String:k_shader] options:nil error:&err];
        if (!lib) {
            NSLog(@"rdr_metal: %@", err);
            return 0;
        }
        vd = [MTLVertexDescriptor vertexDescriptor];
        vd.attributes[0].format = MTLVertexFormatFloat4;
        vd.attributes[0].offset = offsetof(RdrVtx, x);
        vd.attributes[1].format = MTLVertexFormatFloat2;
        vd.attributes[1].offset = offsetof(RdrVtx, s);
        vd.attributes[2].format = MTLVertexFormatFloat4;
        vd.attributes[2].offset = offsetof(RdrVtx, r);
        vd.layouts[0].stride = sizeof(RdrVtx);
        for (b = 0; b < 4; b++) {                     /* opaque, alpha, add, keep */
            d = [MTLRenderPipelineDescriptor new];
            d.vertexFunction = [lib newFunctionWithName:@"vs"];
            d.fragmentFunction = [lib newFunctionWithName:@"fs"];
            d.vertexDescriptor = vd;
            d.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
            d.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
            if (b == RDR_BLEND_ALPHA || b == RDR_BLEND_ADD) {
                d.colorAttachments[0].blendingEnabled = YES;
                d.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
                d.colorAttachments[0].destinationRGBBlendFactor =
                    b == RDR_BLEND_ADD ? MTLBlendFactorOne : MTLBlendFactorOneMinusSourceAlpha;
                d.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorZero;
                d.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOne;
            }
            if (b == RDR_BLEND_MEM)
                d.colorAttachments[0].writeMask = MTLColorWriteMaskNone;
            s_pipe[b] = [s_dev newRenderPipelineStateWithDescriptor:d error:&err];
            if (!s_pipe[b]) {
                NSLog(@"rdr_metal: %@", err);
                return 0;
            }
        }
        d = [MTLRenderPipelineDescriptor new];
        d.vertexFunction = [lib newFunctionWithName:@"bvs"];
        d.fragmentFunction = [lib newFunctionWithName:@"bfs"];
        d.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
        s_blit = [s_dev newRenderPipelineStateWithDescriptor:d error:&err];
        for (t = 0; t < 2; t++)
            for (w = 0; w < 2; w++)
                for (k = 0; k < 2; k++) {
                    MTLDepthStencilDescriptor *dd = [MTLDepthStencilDescriptor new];
                    dd.depthCompareFunction = t ? (k ? MTLCompareFunctionLessEqual : MTLCompareFunctionLess)
                                                : MTLCompareFunctionAlways;
                    dd.depthWriteEnabled = w ? YES : NO;
                    s_ds[t][w][k] = [s_dev newDepthStencilStateWithDescriptor:dd];
                }
        {
            MTLTextureDescriptor *td =
                [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:1 height:1
                                                               mipmapped:NO];
            uint32_t z = 0;
            s_dummy = [s_dev newTextureWithDescriptor:td];
            [s_dummy replaceRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0 withBytes:&z bytesPerRow:4];
        }
        s_tex = [NSMutableArray arrayWithObject:[NSNull null]];   /* handle 0: none */
        return 1;
    }
}

static void begin_pass(MTLLoadAction color, MTLLoadAction depth)
{
    MTLRenderPassDescriptor *p = [MTLRenderPassDescriptor renderPassDescriptor];
    p.colorAttachments[0].texture = s_color;
    p.colorAttachments[0].loadAction = color;
    p.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);
    p.colorAttachments[0].storeAction = MTLStoreActionStore;
    p.depthAttachment.texture = s_depth;
    p.depthAttachment.loadAction = depth;
    p.depthAttachment.clearDepth = 1.0;
    p.depthAttachment.storeAction = MTLStoreActionStore;
    s_enc = [s_cb renderCommandEncoderWithDescriptor:p];
    [s_enc setDepthClipMode:MTLDepthClipModeClamp];   /* the RSP clips on w, not on z */
    [s_enc setViewport:(MTLViewport){ 0, 0, s_fb_w, s_fb_h, 0, 1 }];
}

void rdr_frame_begin(int fb_w, int fb_h)
{
    @autoreleasepool {
        if (!s_color || fb_w != s_fb_w || fb_h != s_fb_h) {
            MTLTextureDescriptor *td =
                [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm width:fb_w
                                                                  height:fb_h mipmapped:NO];
            td.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
            td.storageMode = MTLStorageModePrivate;
            s_color = [s_dev newTextureWithDescriptor:td];
            td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float width:fb_w
                                                                   height:fb_h mipmapped:NO];
            td.usage = MTLTextureUsageRenderTarget;
            td.storageMode = MTLStorageModePrivate;
            s_depth = [s_dev newTextureWithDescriptor:td];
            s_fb_w = fb_w;
            s_fb_h = fb_h;
            s_first = 1;
        }
        s_cb = [s_q commandBuffer];
        begin_pass(s_first ? MTLLoadActionClear : MTLLoadActionLoad, MTLLoadActionClear);
        s_first = 0;
    }
}

int rdr_texture(const uint8_t *rgba, int w, int h)
{
    @autoreleasepool {
        MTLTextureDescriptor *td =
            [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:w height:h
                                                           mipmapped:NO];
        id<MTLTexture> t = [s_dev newTextureWithDescriptor:td];
        NSUInteger i;
        [t replaceRegion:MTLRegionMake2D(0, 0, w, h) mipmapLevel:0 withBytes:rgba bytesPerRow:(NSUInteger)w * 4];
        for (i = 1; i < [s_tex count]; i++)
            if (s_tex[i] == [NSNull null]) {
                s_tex[i] = t;
                return (int)i;
            }
        [s_tex addObject:t];
        return (int)[s_tex count] - 1;
    }
}

void rdr_texture_free(int tex)
{
    if (tex > 0 && (NSUInteger)tex < [s_tex count])
        s_tex[tex] = [NSNull null];
}

static void uniforms(const RdrState *st, Uniforms *u)
{
    int c, k;
    memset(u, 0, sizeof *u);
    for (c = 0; c < 2; c++) {
        u->cc[c * 2] = (simd_int4){ st->cc.rgb[c][0], st->cc.rgb[c][1], st->cc.rgb[c][2], st->cc.rgb[c][3] };
        u->cc[c * 2 + 1] = (simd_int4){ st->cc.a[c][0], st->cc.a[c][1], st->cc.a[c][2], st->cc.a[c][3] };
    }
    u->prim = (simd_float4){ st->prim[0], st->prim[1], st->prim[2], st->prim[3] };
    u->env = (simd_float4){ st->env[0], st->env[1], st->env[2], st->env[3] };
    u->fog = (simd_float4){ st->fog[0], st->fog[1], st->fog[2], st->fog[3] };
    u->blendc = (simd_float4){ st->blend[0], st->blend[1], st->blend[2], st->blend[3] };
    u->plod = st->prim_lod_frac;
    u->cycle = st->cycle;
    u->filter = st->filter;
    u->fog_blend = st->fog_blend;
    u->alpha_cmp = st->alpha_compare;
    u->balpha = st->blend_alpha;
    u->fb = (simd_float2){ (float)s_fb_w, (float)s_fb_h };
    u->lodn = st->lod_levels;
    for (k = 0; k < 10; k++) {
        const RdrTile *t = k < 2 ? &st->tile[k] : &st->lod[k - 2];
        if (k >= 2 && k - 2 >= st->lod_levels)
            break;
        if (!t->tex)
            continue;
        if (k < 2)
            u->ntex |= 1 << k;
        u->tile[k].org = (simd_float4){ t->s0, t->t0, t->sscale, t->tscale };
        u->tile[k].size = (simd_int4){ t->w, t->h, t->mask_s, t->mask_t };
        u->tile[k].wrap = (simd_int4){ t->clamp_s, t->clamp_t, t->mirror_s, t->mirror_t };
        u->tile[k].cl = (simd_int4){ t->clamp_w, t->clamp_h, 0, 0 };
    }
}

static void draw(const RdrState *st, const RdrVtx *v, int n, int depth)
{
    Uniforms u;
    int k;
    uniforms(st, &u);
    [s_enc setRenderPipelineState:s_pipe[st->blend_mode & 3]];
    [s_enc setDepthStencilState:depth ? s_ds[st->z_test != 0][st->z_write != 0][st->z_decal != 0] : s_ds[0][0][0]];
    {
        int x0 = st->scissor[0] < 0 ? 0 : st->scissor[0], y0 = st->scissor[1] < 0 ? 0 : st->scissor[1];
        int x1 = st->scissor[2] > s_fb_w ? s_fb_w : st->scissor[2], y1 = st->scissor[3] > s_fb_h ? s_fb_h : st->scissor[3];
        if (x1 <= x0 || y1 <= y0)
            return;
        [s_enc setScissorRect:(MTLScissorRect){ (NSUInteger)x0, (NSUInteger)y0, (NSUInteger)(x1 - x0),
                                                (NSUInteger)(y1 - y0) }];
    }
    for (k = 0; k < 10; k++) {
        int h = k < 2 ? st->tile[k].tex : (k - 2 < st->lod_levels ? st->lod[k - 2].tex : 0);
        id t = h && (NSUInteger)h < [s_tex count] ? s_tex[h] : nil;
        [s_enc setFragmentTexture:(t && t != [NSNull null]) ? t : s_dummy atIndex:k];
    }
    [s_enc setVertexBytes:&u length:sizeof u atIndex:1];
    [s_enc setFragmentBytes:&u length:sizeof u atIndex:1];
    if ((size_t)n * sizeof(RdrVtx) <= 4096) {
        [s_enc setVertexBytes:v length:(NSUInteger)n * sizeof(RdrVtx) atIndex:0];
    } else {
        id<MTLBuffer> b = [s_dev newBufferWithBytes:v length:(NSUInteger)n * sizeof(RdrVtx)
                                            options:MTLResourceStorageModeShared];
        [s_enc setVertexBuffer:b offset:0 atIndex:0];
    }
    [s_enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:(NSUInteger)n];
}

void rdr_triangles(const RdrState *st, const RdrVtx *v, int n)
{
    if (s_enc)
        draw(st, v, n, 1);
}

void rdr_rect(const RdrState *st, float x0, float y0, float x1, float y1, float s, float t, float dsdx,
              float dtdy, int fill, const float rgba[4])
{
    RdrVtx q[6];
    RdrState f;
    float xs[2] = { x0, x1 }, ys[2] = { y0, y1 };
    static const int order[6][2] = { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
    int i;
    if (!s_enc)
        return;
    f = *st;
    if (fill == 1) {                                  /* a fill colour: the combiner passes it */
        memset(&f.cc, 0, sizeof f.cc);
        for (i = 0; i < 3; i++)
            f.cc.rgb[0][i] = f.cc.a[0][i] = RDR_CC_ZERO;
        f.cc.rgb[0][3] = f.cc.a[0][3] = RDR_CC_PRIM;
        memcpy(f.prim, rgba, sizeof f.prim);
        f.cycle = 1;
        f.tile[0].tex = f.tile[1].tex = 0;
        f.blend_mode = RDR_BLEND_OPAQUE;
        f.alpha_compare = 0;
        f.fog_blend = 0;
    }
    for (i = 0; i < 6; i++) {
        RdrVtx *p = &q[i];
        memset(p, 0, sizeof *p);
        p->x = xs[order[i][0]];
        p->y = ys[order[i][1]];
        p->z = -1;
        p->w = 1;
        p->s = s + (p->x - x0) * dsdx;
        p->t = t + (p->y - y0) * dtdy;
    }
    draw(&f, q, 6, 0);
}

void rdr_clear_depth(void)
{
    if (!s_enc)
        return;
    [s_enc endEncoding];
    begin_pass(MTLLoadActionLoad, MTLLoadActionClear);
}

void rdr_frame_end(void)
{
    @autoreleasepool {
        CAMetalLayer *ml = s_layer;
        id<CAMetalDrawable> dr;
        int dw = 0, dh = 0;
        if (!s_enc)
            return;
        [s_enc endEncoding];
        s_enc = nil;
        if (ml) {
            host_macos_layer_fit(&dw, &dh);
            if ([ml device] != s_dev)
                [ml setDevice:s_dev];
            dr = [ml nextDrawable];
            if (dr && dw > 0 && dh > 0) {
                MTLRenderPassDescriptor *p = [MTLRenderPassDescriptor renderPassDescriptor];
                id<MTLRenderCommandEncoder> e;
                float sx = 1, sy = 1;
                simd_float4 r;
                if (dw * 3 > dh * 4)                  /* letterbox to 4:3 */
                    sx = (float)(dh * 4) / (float)(dw * 3);
                else
                    sy = (float)(dw * 3) / (float)(dh * 4);
                r = (simd_float4){ -sx, -sy, 2 * sx, 2 * sy };
                p.colorAttachments[0].texture = dr.texture;
                p.colorAttachments[0].loadAction = MTLLoadActionClear;
                p.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);
                p.colorAttachments[0].storeAction = MTLStoreActionStore;
                e = [s_cb renderCommandEncoderWithDescriptor:p];
                [e setRenderPipelineState:s_blit];
                [e setVertexBytes:&r length:sizeof r atIndex:0];
                [e setFragmentTexture:s_color atIndex:0];
                [e drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
                [e endEncoding];
                [s_cb presentDrawable:dr];
            }
        }
        [s_cb commit];
        s_cb = nil;
    }
}

/* the last frame read back (for screenshots only: it waits for the GPU) */
const uint32_t *rdr_frame_pixels(int *w, int *h)
{
    @autoreleasepool {
        id<MTLCommandBuffer> cb;
        id<MTLBlitCommandEncoder> bl;
        id<MTLBuffer> buf;
        if (!s_color) {
            *w = *h = 0;
            return NULL;
        }
        buf = [s_dev newBufferWithLength:(NSUInteger)s_fb_w * s_fb_h * 4 options:MTLResourceStorageModeShared];
        cb = [s_q commandBuffer];
        bl = [cb blitCommandEncoder];
        [bl copyFromTexture:s_color sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0)
                 sourceSize:MTLSizeMake(s_fb_w, s_fb_h, 1) toBuffer:buf destinationOffset:0
        destinationBytesPerRow:(NSUInteger)s_fb_w * 4 destinationBytesPerImage:(NSUInteger)s_fb_w * s_fb_h * 4];
        [bl endEncoding];
        [cb commit];
        [cb waitUntilCompleted];
        if (s_px_w * s_px_h < s_fb_w * s_fb_h)
            s_pixels = (uint32_t *)realloc(s_pixels, (size_t)s_fb_w * s_fb_h * 4);
        memcpy(s_pixels, [buf contents], (size_t)s_fb_w * s_fb_h * 4);   /* BGRA = 0xAARRGGBB little-endian */
        s_px_w = *w = s_fb_w;
        s_px_h = *h = s_fb_h;
        return s_pixels;
    }
}
