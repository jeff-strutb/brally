/* br_gfx_metal.m -- Metal backend for br_gfx.h.
 *
 * Shaders are compiled at runtime from source with newLibraryWithSource:, so
 * this needs no offline Metal toolchain (which is not installed on this
 * machine and would otherwise be an extra dependency).
 *
 * The render target uses MTLStorageModeShared so it can be read back directly
 * on Apple Silicon, which is what makes headless verification possible.
 */
#import <Metal/Metal.h>
#import <Foundation/Foundation.h>

#include "../br_gfx.h"
#include "../br_gfx3d.h"

#include <stdlib.h>
#include <string.h>

#define BR_GFX_MAX_TEXTURES 1024

/* --- 3D path (br_gfx3d.h) ------------------------------------------------
 * Sizes, and why they are these sizes.  The handle table is linear and short
 * because 0xDC's low 24 bits are a texture NAME, and the Glide build only
 * ever registers a handful (br_font.c: two, one per glyph size).  The batch
 * arrays grow. */
#define BR_GFX3D_MAX_TEXMAP 256

/* 0xDC's payload is 24 bits and ZERO IS A VALID TEXTURE NAME -- it is the
 * first entry of the array the loader appends to (br_tex3d.h).  So "no
 * texture bound" cannot be spelled 0; it is spelled with a value the 24-bit
 * field cannot hold. */
#define BR_GFX3D_TEX_NONE 0xFFFFFFFFu

/* One batched vertex.  x,y are screen pixels exactly as br_dl.c's transform
 * left them (quarter-pixel snapped, top-left origin); z,w are clip-space so
 * the rasteriser interpolates s,t perspective-correctly. */
typedef struct BrGfx3dVtx {
    float x, y, z, w;
    float r, g, b, a;
    float s, t;
} BrGfx3dVtx;

/* One flush: a run of vertices sharing a complete pipeline state. */
typedef struct BrGfx3dDraw {
    uint32_t first, count;
    uint8_t  combine, blend, z, fZWrite, fDecal;
    /* A RECTANGLE carries its own 0..1 corners; a TRIANGLE carries the raw
     * N64 Vtx coordinates and needs the per-texture scale applied.  The two
     * cannot share a draw, and this is what keeps them apart. */
    uint8_t  fUnitUv;
    uint32_t tex;
    float    konst[4];
} BrGfx3dDraw;

typedef struct BrGfx3dTexMap {
    uint32_t  handle;
    BrTexture tex;
    float     scale[2];     /* 0x118ED1A4 / 0x118ED1A8, normalised */
} BrGfx3dTexMap;

static char g_szError[512];

const char *BrGfxLastError(void)
{
    return g_szError[0] ? g_szError : NULL;
}

static void set_error(const char *pszFmt, ...)
{
    va_list ap;
    va_start(ap, pszFmt);
    vsnprintf(g_szError, sizeof(g_szError), pszFmt, ap);
    va_end(ap);
}

struct BrGfx {
    id<MTLDevice>              device;
    id<MTLCommandQueue>        queue;
    id<MTLRenderPipelineState> pipeline;
    id<MTLSamplerState>        sampler;
    id<MTLTexture>             target;
    id<MTLCommandBuffer>       cmd;
    id<MTLRenderCommandEncoder> enc;
    id<MTLTexture>             textures[BR_GFX_MAX_TEXTURES];
    uint32_t                   cTextures;
    uint32_t                   width, height;

    /* ---- the 3D path; all of it inert until BrGfx3dInit runs ---------- */
    int                        f3d;              /* pipelines built        */
    id<MTLRenderPipelineState> a3dPipe[BR_DL_CC__COUNT][BR_GFX3D_BLEND__COUNT];
    id<MTLDepthStencilState>   a3dDepth[BR_GFX3D_Z__COUNT][2];
    id<MTLTexture>             depthTarget;
    id<MTLTexture>             whiteTex;
    id<MTLSamplerState>        sampler3d;

    BrGfx3dTexMap              aTexMap[BR_GFX3D_MAX_TEXMAP];
    uint32_t                   cTexMap;

    BrDl                      *p3dDl;            /* for fill/prim colour   */

    /* pending Glide-equivalent state, exactly the set 0x1001E7A0 and
     * 0x10021270 between them maintain */
    BrDlCombine                stCombine;
    BrGfx3dBlend               stBlend;
    BrGfx3dZ                   stZ;
    int                        stZWrite;
    int                        stDecal;
    int                        stXluLatch;       /* g_5D17D4               */
    int                        stAlphaConst;     /* grAlphaCombine variant  */
    int                        stUnitUv;         /* rect vs triangle UVs    */
    float                      stKonst[4];       /* grConstantColorValue   */
    uint32_t                   stTex;

    int                        fDepthTest;       /* NOT from the original  */

    BrGfx3dVtx                *aVtx3d;
    uint32_t                   cVtx3d, cVtx3dMax;
    BrGfx3dDraw               *aDraw3d;
    uint32_t                   cDraw3d, cDraw3dMax;
    uint32_t                   iBatchFirst;
    int                        fBatchOpen;
    BrGfx3dDraw                openState3d;      /* state the run opened in */

    float                      clear3d[4];
    BrGfx3dStats               stats3d;
};

/* Quad in pixel space; the vertex shader maps it to clip space using the
 * target size pushed as a uniform. Top-left origin, so Y is flipped. */
static NSString *const kShaderSource = @
"#include <metal_stdlib>\n"
"using namespace metal;\n"
"struct VSIn  { float2 pos; float2 uv; };\n"
"struct VSOut { float4 pos [[position]]; float2 uv; };\n"
"vertex VSOut v_main(uint vid [[vertex_id]],\n"
"                    constant VSIn *verts [[buffer(0)]],\n"
"                    constant float2 &target [[buffer(1)]]) {\n"
"    VSOut o;\n"
"    float2 p = verts[vid].pos / target;      // 0..1\n"
"    p.y = 1.0 - p.y;                          // top-left origin\n"
"    o.pos = float4(p * 2.0 - 1.0, 0.0, 1.0);\n"
"    o.uv  = verts[vid].uv;\n"
"    return o;\n"
"}\n"
"fragment float4 f_main(VSOut in [[stage_in]],\n"
"                       texture2d<float> tex [[texture(0)]],\n"
"                       sampler smp [[sampler(0)]]) {\n"
"    return tex.sample(smp, in.uv);\n"
"}\n";

BrGfx *BrGfxCreate(uint32_t width, uint32_t height)
{
    g_szError[0] = '\0';
    if (width == 0 || height == 0) {
        set_error("BrGfxCreate: zero dimension");
        return NULL;
    }

    BrGfx *g = (BrGfx *)calloc(1, sizeof(BrGfx));
    if (g == NULL)
        return NULL;
    g->width = width;
    g->height = height;

    g->device = MTLCreateSystemDefaultDevice();
    if (g->device == nil) {
        set_error("no Metal device");
        free(g);
        return NULL;
    }
    g->queue = [g->device newCommandQueue];

    NSError *err = nil;
    id<MTLLibrary> lib = [g->device newLibraryWithSource:kShaderSource
                                                 options:nil
                                                   error:&err];
    if (lib == nil) {
        set_error("shader compile failed: %s",
                  err ? [[err localizedDescription] UTF8String] : "?");
        free(g);
        return NULL;
    }

    MTLRenderPipelineDescriptor *pd = [MTLRenderPipelineDescriptor new];
    pd.vertexFunction   = [lib newFunctionWithName:@"v_main"];
    pd.fragmentFunction = [lib newFunctionWithName:@"f_main"];
    pd.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
    pd.colorAttachments[0].blendingEnabled = YES;
    pd.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    pd.colorAttachments[0].destinationRGBBlendFactor =
        MTLBlendFactorOneMinusSourceAlpha;

    g->pipeline = [g->device newRenderPipelineStateWithDescriptor:pd error:&err];
    if (g->pipeline == nil) {
        set_error("pipeline failed: %s",
                  err ? [[err localizedDescription] UTF8String] : "?");
        free(g);
        return NULL;
    }

    MTLSamplerDescriptor *sd = [MTLSamplerDescriptor new];
    sd.minFilter = MTLSamplerMinMagFilterNearest;   /* period-correct */
    sd.magFilter = MTLSamplerMinMagFilterNearest;
    g->sampler = [g->device newSamplerStateWithDescriptor:sd];

    MTLTextureDescriptor *td = [MTLTextureDescriptor
        texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                     width:width
                                    height:height
                                 mipmapped:NO];
    td.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    td.storageMode = MTLStorageModeShared;          /* readable on the CPU */
    g->target = [g->device newTextureWithDescriptor:td];

    g->cTextures = 1;                                /* handle 0 stays invalid */
    return g;
}

void BrGfxDestroy(BrGfx *g)
{
    if (g == NULL)
        return;
    for (uint32_t i = 0; i < g->cTextures; i++)
        g->textures[i] = nil;
    free(g->aVtx3d);
    free(g->aDraw3d);
    free(g);
}

BrTexture BrGfxCreateTexture(BrGfx *g, uint32_t width, uint32_t height,
                             const uint8_t *pRgba)
{
    if (g == NULL || width == 0 || height == 0)
        return 0;
    if (g->cTextures >= BR_GFX_MAX_TEXTURES) {
        set_error("texture table full");
        return 0;
    }

    MTLTextureDescriptor *td = [MTLTextureDescriptor
        texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                     width:width
                                    height:height
                                 mipmapped:NO];
    id<MTLTexture> t = [g->device newTextureWithDescriptor:td];
    if (t == nil) {
        set_error("texture allocation failed");
        return 0;
    }
    [t replaceRegion:MTLRegionMake2D(0, 0, width, height)
         mipmapLevel:0
           withBytes:pRgba
         bytesPerRow:width * 4];

    BrTexture h = g->cTextures++;
    g->textures[h] = t;
    return h;
}

void BrGfxBeginFrame(BrGfx *g, float r, float gr, float b, float a)
{
    if (g == NULL)
        return;
    MTLRenderPassDescriptor *rp = [MTLRenderPassDescriptor renderPassDescriptor];
    rp.colorAttachments[0].texture     = g->target;
    rp.colorAttachments[0].loadAction  = MTLLoadActionClear;
    rp.colorAttachments[0].storeAction = MTLStoreActionStore;
    rp.colorAttachments[0].clearColor  = MTLClearColorMake(r, gr, b, a);

    g->cmd = [g->queue commandBuffer];
    g->enc = [g->cmd renderCommandEncoderWithDescriptor:rp];
    [g->enc setRenderPipelineState:g->pipeline];
    [g->enc setFragmentSamplerState:g->sampler atIndex:0];
}

void BrGfxDrawTexture(BrGfx *g, BrTexture tex,
                      float x, float y, float w, float h)
{
    if (g == NULL || g->enc == nil || tex == 0 || tex >= g->cTextures)
        return;

    const float verts[6][4] = {
        { x,     y,     0.0f, 0.0f },
        { x + w, y,     1.0f, 0.0f },
        { x,     y + h, 0.0f, 1.0f },
        { x + w, y,     1.0f, 0.0f },
        { x + w, y + h, 1.0f, 1.0f },
        { x,     y + h, 0.0f, 1.0f },
    };
    const float target[2] = { (float)g->width, (float)g->height };

    [g->enc setVertexBytes:verts length:sizeof(verts) atIndex:0];
    [g->enc setVertexBytes:target length:sizeof(target) atIndex:1];
    [g->enc setFragmentTexture:g->textures[tex] atIndex:0];
    [g->enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
}

void BrGfxEndFrame(BrGfx *g)
{
    if (g == NULL || g->enc == nil)
        return;
    [g->enc endEncoding];
    [g->cmd commit];
    [g->cmd waitUntilCompleted];
    g->enc = nil;
    g->cmd = nil;
}

int BrGfxReadPixels(BrGfx *g, uint8_t *pRgbaOut)
{
    if (g == NULL || pRgbaOut == NULL)
        return 1;
    [g->target getBytes:pRgbaOut
            bytesPerRow:g->width * 4
             fromRegion:MTLRegionMake2D(0, 0, g->width, g->height)
            mipmapLevel:0];
    return 0;
}

/* ------------------------------------------------------------------------
 * Windowed presentation (AppKit + CAMetalLayer).
 *
 * Kept in the backend because the swapchain is inherently backend-specific.
 * The portable core never sees NSWindow or CAMetalLayer.
 * ---------------------------------------------------------------------- */
#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>

static NSWindow     *g_window;
static CAMetalLayer *g_layer;
static BOOL          g_quit;

@interface BrWindowDelegate : NSObject <NSWindowDelegate>
@end
@implementation BrWindowDelegate
- (BOOL)windowShouldClose:(NSWindow *)sender { g_quit = YES; return YES; }
@end
static BrWindowDelegate *g_delegate;

int BrGfxOpenWindow(BrGfx *g, const char *pszTitle)
{
    if (g == NULL)
        return 1;
    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        NSRect frame = NSMakeRect(0, 0, g->width, g->height);
        g_window = [[NSWindow alloc]
            initWithContentRect:frame
                      styleMask:(NSWindowStyleMaskTitled |
                                 NSWindowStyleMaskClosable |
                                 NSWindowStyleMaskMiniaturizable)
                        backing:NSBackingStoreBuffered
                          defer:NO];
        [g_window setTitle:[NSString stringWithUTF8String:pszTitle]];
        [g_window center];

        g_layer = [CAMetalLayer layer];
        g_layer.device = g->device;
        g_layer.pixelFormat = MTLPixelFormatRGBA8Unorm;
        g_layer.framebufferOnly = NO;
        g_layer.drawableSize = CGSizeMake(g->width, g->height);

        NSView *view = [g_window contentView];
        [view setWantsLayer:YES];
        [view setLayer:g_layer];

        g_delegate = [BrWindowDelegate new];
        [g_window setDelegate:g_delegate];
        [g_window makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];
        [NSApp finishLaunching];
    }
    return 0;
}

/* The key queue.
 *
 * A RING, not a single latched key: a fast typist (or a scripted test that
 * feeds several keys between frames) must not lose any, and the menu's
 * selection moves one step per key, so dropping one silently gives the wrong
 * final index -- which is exactly the sort of thing that looks like a bug in
 * the ported navigation code and is not. Overflow drops the NEWEST key rather
 * than overwriting the oldest, so an overrun cannot reorder what did arrive.
 *
 * Backend-local and only touched on the thread that pumps events, which is
 * the thread that polls. */
#define BR_KEYQ 32
static BrKey g_aKeyQ[BR_KEYQ];
static int   g_iKeyHead, g_iKeyTail;

static void BrKeyPush(BrKey k)
{
    int next = (g_iKeyTail + 1) % BR_KEYQ;
    if (next == g_iKeyHead)
        return;                         /* full: drop the newest */
    g_aKeyQ[g_iKeyTail] = k;
    g_iKeyTail = next;
}

BrKey BrGfxPollKey(BrGfx *g)
{
    BrKey k;
    (void)g;
    if (g_iKeyHead == g_iKeyTail)
        return BR_KEY_NONE;
    k = g_aKeyQ[g_iKeyHead];
    g_iKeyHead = (g_iKeyHead + 1) % BR_KEYQ;
    return k;
}

/* macOS virtual key codes. Listed rather than #defined from a header because
 * AppKit does not ship portable names for them. */
#define BR_VK_RETURN   36
#define BR_VK_LBRACKET 33
#define BR_VK_RBRACKET 30
#define BR_VK_ESCAPE   53
#define BR_VK_SPACE    49
#define BR_VK_KPENTER  76
#define BR_VK_LEFT    123
#define BR_VK_RIGHT   124
#define BR_VK_DOWN    125
#define BR_VK_UP      126

int BrGfxPumpEvents(BrGfx *g)
{
    @autoreleasepool {
        NSEvent *ev;
        while ((ev = [NSApp nextEventMatchingMask:NSEventMaskAny
                                        untilDate:nil
                                           inMode:NSDefaultRunLoopMode
                                          dequeue:YES]) != nil) {
            if ([ev type] == NSEventTypeKeyDown && ![ev isARepeat]) {
                switch ([ev keyCode]) {
                case BR_VK_UP:     BrKeyPush(BR_KEY_UP);       break;
                case BR_VK_LEFT:   BrKeyPush(BR_KEY_UP);       break;
                case BR_VK_DOWN:   BrKeyPush(BR_KEY_DOWN);     break;
                case BR_VK_RIGHT:  BrKeyPush(BR_KEY_DOWN);     break;
                case BR_VK_RETURN: BrKeyPush(BR_KEY_ACTIVATE); break;
                case BR_VK_KPENTER:BrKeyPush(BR_KEY_ACTIVATE); break;
                case BR_VK_SPACE:  BrKeyPush(BR_KEY_ACTIVATE); break;
                /* Escape is BACK, not quit. The window's close button and
                 * Cmd-Q still quit; a menu that exits the process when the
                 * user asks to go back is not a menu. */
                case BR_VK_ESCAPE: BrKeyPush(BR_KEY_BACK);     break;
                /* Harness-only screen paging; see the enum. */
                case BR_VK_LBRACKET: BrKeyPush(BR_KEY_PREV_SCREEN); break;
                case BR_VK_RBRACKET: BrKeyPush(BR_KEY_NEXT_SCREEN); break;
                default: break;
                }
            }
            [NSApp sendEvent:ev];
        }
    }
    return g_quit ? 0 : 1;
}

void BrGfxPresent(BrGfx *g)
{
    if (g == NULL || g_layer == nil)
        return;
    @autoreleasepool {
        id<CAMetalDrawable> drawable = [g_layer nextDrawable];
        if (drawable == nil)
            return;
        id<MTLCommandBuffer> cb = [g->queue commandBuffer];
        id<MTLBlitCommandEncoder> blit = [cb blitCommandEncoder];
        [blit copyFromTexture:g->target
                  sourceSlice:0 sourceLevel:0
                 sourceOrigin:MTLOriginMake(0, 0, 0)
                   sourceSize:MTLSizeMake(g->width, g->height, 1)
                    toTexture:[drawable texture]
             destinationSlice:0 destinationLevel:0
            destinationOrigin:MTLOriginMake(0, 0, 0)];
        [blit endEncoding];
        [cb presentDrawable:drawable];
        [cb commit];
    }
}

/* ========================================================================
 * THE 3D PATH -- br_gfx3d.h
 *
 * This is the consumer side of br_dl.h's BrDlSink, and it is the same seam
 * BRD3D.dll and BRGlide.dll already sit on: one interpreter, one command
 * set, two backends.  Everything below is either (a) a transcription of
 * 0x1001E7A0 / 0x10021270 into Metal state objects, marked with its address,
 * or (b) marked DEVIATION.
 * ===================================================================== */

/* The shader.  ONE vertex function and ONE fragment function; the ten
 * combiner rows are specialisations of the fragment function through a
 * function constant, so the "shader cache" is a compile-time array and not a
 * runtime dictionary.  That is only legitimate because the combiner set is
 * closed -- see br_gfx3d.h. */
static NSString *const k3dShaderSource = @
"#include <metal_stdlib>\n"
"using namespace metal;\n"
"constant int kCombine [[function_constant(0)]];\n"
"// SCALAR MEMBERS ON PURPOSE. A `float4` member carries 16-byte alignment,\n"
"// so a {float4,float4,float2} struct is 48 bytes while the C BrGfx3dVtx is\n"
"// 40 -- the buffer would be read at the wrong stride and every vertex past\n"
"// the first would be garbage. Scalars give the same layout on both sides.\n"
"struct V3In  { float x, y, z, w; float r, g, b, a; float s, t; };\n"
"struct V3Out { float4 pos [[position]]; float4 rgba; float2 st; };\n"
"vertex V3Out v3_main(uint vid [[vertex_id]],\n"
"                     const device V3In *v [[buffer(0)]],\n"
"                     constant float2 &target [[buffer(1)]],\n"
"                     constant float2 &texScale [[buffer(2)]]) {\n"
"    V3Out o;\n"
"    float w = v[vid].w;\n"
"    // screen pixels, top-left origin, straight out of br_dl.c's transform\n"
"    float2 ndc = float2(v[vid].x / target.x * 2.0 - 1.0,\n"
"                        1.0 - v[vid].y / target.y * 2.0);\n"
"    o.pos  = float4(ndc * w, v[vid].z, w);\n"
"    o.rgba = float4(v[vid].r, v[vid].g, v[vid].b, v[vid].a);\n"
"    // s,t are the RAW N64 Vtx coordinates (S10.5 texels); texScale is the\n"
"    // per-texture value 0x100284E0 installs. The rasteriser does the\n"
"    // perspective divide itself -- see br3d_put.\n"
"    o.st   = float2(v[vid].s, v[vid].t) * texScale;\n"
"    return o;\n"
"}\n"
"fragment float4 f3_main(V3Out in [[stage_in]],\n"
"                        texture2d<float> tex [[texture(0)]],\n"
"                        sampler smp [[sampler(0)]],\n"
"                        constant float4 &konst [[buffer(0)]]) {\n"
"    float4 t = tex.sample(smp, in.st);\n"
"    float4 s = in.rgba;\n"
"    // grColorCombine's ten argument tuples, by row. See br_gfx3d.h.\n"
"    // grColorCombine sets RGB ONLY. Alpha is grAlphaCombine's, and the\n"
"    // one factor that varies there is folded into konst.a by the caller.\n"
"    float  oa = t.a * konst.a;\n"
"    if (kCombine == 1)                       // 1.0 * TEXTURE\n"
"        return float4(t.rgb, oa);\n"
"    if (kCombine == 2)                       // FUNCTION_LOCAL, ITERATED\n"
"        return float4(s.rgb, oa);\n"
"    if (kCombine == 7)                       // BLEND, TEXTURE_ALPHA\n"
"        return float4(mix(konst.rgb, t.rgb, t.a), oa);\n"
"    if (kCombine >= 3 && kCombine <= 6)      // 1.0 * CONSTANT\n"
"        return float4(konst.rgb, oa);\n"
"    if (kCombine == 9)\n"
"        return float4(konst.rgb, oa);\n"
"    return float4(t.rgb * s.rgb, oa);        // default and decal: modulate\n"
"}\n";

/* grAlphaBlendFunction's three tuples, as Metal blend factors. */
static void br3d_blend(MTLRenderPipelineColorAttachmentDescriptor *ca,
                       BrGfx3dBlend b)
{
    switch (b) {
    case BR_GFX3D_BLEND_ALPHA:                  /* (1,5,4,0) */
        ca.blendingEnabled = YES;
        ca.sourceRGBBlendFactor        = MTLBlendFactorSourceAlpha;
        ca.destinationRGBBlendFactor   = MTLBlendFactorOneMinusSourceAlpha;
        ca.sourceAlphaBlendFactor      = MTLBlendFactorOne;
        ca.destinationAlphaBlendFactor = MTLBlendFactorZero;
        break;
    case BR_GFX3D_BLEND_ADD:                    /* (1,4,4,0) */
        ca.blendingEnabled = YES;
        ca.sourceRGBBlendFactor        = MTLBlendFactorSourceAlpha;
        ca.destinationRGBBlendFactor   = MTLBlendFactorOne;
        ca.sourceAlphaBlendFactor      = MTLBlendFactorOne;
        ca.destinationAlphaBlendFactor = MTLBlendFactorZero;
        break;
    case BR_GFX3D_BLEND_OPAQUE:                 /* (4,0,4,0) */
    default:
        ca.blendingEnabled = NO;
        break;
    }
}

static MTLCompareFunction br3d_cmp(BrGfx3dZ z)
{
    switch (z) {
    case BR_GFX3D_Z_LESS:   return MTLCompareFunctionLess;      /* GR_CMP_LESS   */
    case BR_GFX3D_Z_EQUAL:  return MTLCompareFunctionEqual;     /* GR_CMP_EQUAL  */
    case BR_GFX3D_Z_LEQUAL: return MTLCompareFunctionLessEqual; /* GR_CMP_LEQUAL */
    case BR_GFX3D_Z_ALWAYS:
    default:                return MTLCompareFunctionAlways;
    }
}

int BrGfx3dInit(BrGfx *g)
{
    NSError *err = nil;
    id<MTLLibrary> lib;
    int ic, ib, iz, iw;

    if (g == NULL)
        return 1;
    if (g->f3d)
        return 0;

    lib = [g->device newLibraryWithSource:k3dShaderSource options:nil error:&err];
    if (lib == nil) {
        set_error("3d shader compile failed: %s",
                  err ? [[err localizedDescription] UTF8String] : "?");
        return 1;
    }

    /* THE STATIC ARRAY.  BR_DL_CC__COUNT x BR_GFX3D_BLEND__COUNT states,
     * every one of them built here and none of them built later. */
    for (ic = 0; ic < BR_DL_CC__COUNT; ic++) {
        MTLFunctionConstantValues *fc = [MTLFunctionConstantValues new];
        int combine = ic;
        id<MTLFunction> fs;

        [fc setConstantValue:&combine type:MTLDataTypeInt atIndex:0];
        fs = [lib newFunctionWithName:@"f3_main" constantValues:fc error:&err];
        if (fs == nil) {
            set_error("3d fragment specialisation %d failed: %s", ic,
                      err ? [[err localizedDescription] UTF8String] : "?");
            return 1;
        }
        for (ib = 0; ib < BR_GFX3D_BLEND__COUNT; ib++) {
            MTLRenderPipelineDescriptor *pd = [MTLRenderPipelineDescriptor new];
            pd.vertexFunction   = [lib newFunctionWithName:@"v3_main"];
            pd.fragmentFunction = fs;
            pd.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
            br3d_blend(pd.colorAttachments[0], (BrGfx3dBlend)ib);
            pd.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
            g->a3dPipe[ic][ib] =
                [g->device newRenderPipelineStateWithDescriptor:pd error:&err];
            if (g->a3dPipe[ic][ib] == nil) {
                set_error("3d pipeline %d/%d failed: %s", ic, ib,
                          err ? [[err localizedDescription] UTF8String] : "?");
                return 1;
            }
        }
    }

    for (iz = 0; iz < BR_GFX3D_Z__COUNT; iz++)
        for (iw = 0; iw < 2; iw++) {
            MTLDepthStencilDescriptor *dd = [MTLDepthStencilDescriptor new];
            dd.depthCompareFunction = br3d_cmp((BrGfx3dZ)iz);
            dd.depthWriteEnabled    = iw ? YES : NO;
            g->a3dDepth[iz][iw] = [g->device newDepthStencilStateWithDescriptor:dd];
        }

    {
        MTLTextureDescriptor *td = [MTLTextureDescriptor
            texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                         width:g->width
                                        height:g->height
                                     mipmapped:NO];
        td.usage       = MTLTextureUsageRenderTarget;
        td.storageMode = MTLStorageModePrivate;
        g->depthTarget = [g->device newTextureWithDescriptor:td];
    }

    /* An unbound texture must not blank the model: the combiner rows that
     * multiply by TEXTURE need a 1.0 texel, and nothing in the port yet
     * explains how a real texture reaches 0xDC. */
    {
        static const uint8_t white[4] = { 255, 255, 255, 255 };
        MTLTextureDescriptor *td = [MTLTextureDescriptor
            texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                         width:1 height:1 mipmapped:NO];
        g->whiteTex = [g->device newTextureWithDescriptor:td];
        [g->whiteTex replaceRegion:MTLRegionMake2D(0, 0, 1, 1)
                       mipmapLevel:0 withBytes:white bytesPerRow:4];
    }

    {
        MTLSamplerDescriptor *sd = [MTLSamplerDescriptor new];
        sd.minFilter = MTLSamplerMinMagFilterNearest;
        sd.magFilter = MTLSamplerMinMagFilterNearest;
        sd.sAddressMode = MTLSamplerAddressModeRepeat;
        sd.tAddressMode = MTLSamplerAddressModeRepeat;
        g->sampler3d = [g->device newSamplerStateWithDescriptor:sd];
    }

    /* The state the interpreter starts in: grDepthMask(1) is the first thing
     * 0x10021270 does on every call, and nothing has issued a combine or a
     * render mode yet. */
    g->stCombine   = BR_DL_CC_DEFAULT;
    g->stBlend     = BR_GFX3D_BLEND_OPAQUE;
    g->stZ         = BR_GFX3D_Z_LESS;
    g->stZWrite    = 1;
    g->stDecal     = 0;
    g->stXluLatch  = 0;
    g->stAlphaConst = 0;
    g->stTex       = BR_GFX3D_TEX_NONE;
    g->stKonst[0]  = g->stKonst[1] = g->stKonst[2] = g->stKonst[3] = 1.0f;
    g->fDepthTest  = 1;

    g->f3d = 1;
    return 0;
}

/* ---------------------------------------------------------------------
 * batching
 * ------------------------------------------------------------------ */

static int br3d_reserve(BrGfx *g, uint32_t need)
{
    if (g->cVtx3d + need > g->cVtx3dMax) {
        uint32_t n = g->cVtx3dMax ? g->cVtx3dMax * 2 : 4096;
        BrGfx3dVtx *p;
        while (n < g->cVtx3d + need)
            n *= 2;
        p = (BrGfx3dVtx *)realloc(g->aVtx3d, (size_t)n * sizeof(*p));
        if (p == NULL)
            return 1;
        g->aVtx3d = p;
        g->cVtx3dMax = n;
    }
    if (g->cDraw3d + 1 > g->cDraw3dMax) {
        uint32_t n = g->cDraw3dMax ? g->cDraw3dMax * 2 : 256;
        BrGfx3dDraw *p =
            (BrGfx3dDraw *)realloc(g->aDraw3d, (size_t)n * sizeof(*p));
        if (p == NULL)
            return 1;
        g->aDraw3d = p;
        g->cDraw3dMax = n;
    }
    return 0;
}

/* Snapshot the live Glide-equivalent state as a draw record.
 *
 * The fourth component of `konst` is NOT the constant colour's alpha in the
 * general case, and this is the subtlety the whole alpha model turns on:
 * grColorCombine sets RGB only.  Alpha comes from grAlphaCombine, which
 * 0x10021270 sets to (SCALE_OTHER, ONE, CONSTANT, TEXTURE) in six of its
 * arms -- alpha == texture alpha -- and to (SCALE_OTHER, TEXTURE_ALPHA,
 * CONSTANT, CONSTANT) in exactly two: modes 2 and 0x0D1849D8, where alpha ==
 * texture alpha * constant alpha.  Folding that one factor into konst.a lets
 * the shader write `t.a * konst.a` for every row and stay right, instead of
 * needing a second dimension of pipeline states. */
static void br3d_snapshot(BrGfx *g, BrGfx3dDraw *d)
{
    d->combine = (uint8_t)g->stCombine;
    d->blend   = (uint8_t)g->stBlend;
    d->z       = (uint8_t)(g->fDepthTest ? g->stZ : BR_GFX3D_Z_ALWAYS);
    d->fZWrite = (uint8_t)(g->fDepthTest ? g->stZWrite : 0);
    d->fDecal  = (uint8_t)g->stDecal;
    d->fUnitUv = (uint8_t)g->stUnitUv;
    d->tex     = g->stTex;
    d->konst[0] = g->stKonst[0];
    d->konst[1] = g->stKonst[1];
    d->konst[2] = g->stKonst[2];
    d->konst[3] = g->stAlphaConst ? g->stKonst[3] : 1.0f;
}

/* Close the open run of vertices as one draw call.
 *
 * The record is the state the run OPENED in, not the state now: by the time
 * a run is closed the interpreter has usually already executed the command
 * that closed it, so reading live state here draws every batch with the
 * NEXT batch's pipeline.  That is an off-by-one that looks entirely correct
 * on data which only ever uses one state -- which is exactly what the retail
 * car models do -- and only shows up when two states are in play. */
static void br3d_flush(BrGfx *g)
{
    BrGfx3dDraw *d;

    if (!g->fBatchOpen || g->cVtx3d == g->iBatchFirst) {
        g->fBatchOpen = 0;
        return;
    }
    d = &g->aDraw3d[g->cDraw3d++];
    *d = g->openState3d;
    d->first = g->iBatchFirst;
    d->count = g->cVtx3d - g->iBatchFirst;
    g->fBatchOpen = 0;
    g->stats3d.cDraws++;
}

/* Called before appending: if the open run was drawn under a different
 * state, close it first.  This is the whole batching rule. */
static void br3d_want(BrGfx *g)
{
    BrGfx3dDraw now;

    memset(&now, 0, sizeof(now));
    br3d_snapshot(g, &now);
    if (g->fBatchOpen) {
        BrGfx3dDraw open = g->openState3d;
        open.first = now.first = 0;
        open.count = now.count = 0;
        if (memcmp(&open, &now, sizeof(now)) == 0)
            return;
        br3d_flush(g);
        g->stats3d.cStateChanges++;
    }
    g->iBatchFirst = g->cVtx3d;
    g->fBatchOpen  = 1;
    g->openState3d = now;
}

/* ---------------------------------------------------------------------
 * the sink -- br_dl.h's BrDlSink, implemented
 * ------------------------------------------------------------------ */

/* The colour slots carry the Vtx's trailing three bytes scaled by 1/128 and
 * are therefore in [-1,1]: br_dl.c's file header says the LIGHTING PASS HAS
 * NOT BEEN FOUND, so nothing has overwritten them.  Folding to [0,1] is the
 * same thing br_ras_tri does, and it is done here for the same reason and in
 * the same way -- so that the two rasterisers can be compared without the
 * comparison being dominated by a colour convention neither of them owns. */
static float br3d_fold(float v)
{
    v = v * 0.5f + 0.5f;
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    return v;
}

static void br3d_put(BrGfx *g, const BrDlVtx *v)
{
    BrGfx3dVtx *o = &g->aVtx3d[g->cVtx3d++];
    o->x = v->x;
    o->y = v->y;
    /* Clip space, undivided.  br_dl.c keeps cz and cw beside the screen
     * coordinates, so the depth the RDP would have used is available without
     * reconstructing it: (cz + cw)/2 over cw lands in [0,1], which is Metal's
     * depth range and also the range grDepthBufferFunction is comparing in. */
    o->z = (v->cz + v->cw) * 0.5f;
    o->w = v->cw;
    o->r = br3d_fold(v->r);
    o->g = br3d_fold(v->g);
    o->b = br3d_fold(v->b);
    /* GrVertex.a at +0x1C is never written by the transform (0x10021A20); the
     * triangle routines 0x10021C70 / 0x100221D0 have not been read, so where
     * it comes from is unknown.  Forced opaque.  DEVIATION. */
    o->a = 1.0f;
    /* THE RAW COORDINATES, not tmu0.
     *
     * br_dl.c keeps the pre-divide s and t beside the Glide-shaped tmu0,
     * which holds s/w and t/w because grDrawTriangle wants them that way --
     * Glide interpolates s/w and 1/w itself.  A GPU does the same divide in
     * hardware from `pos.w`, so handing it s/w as a varying divides twice
     * and every texture comes out smeared by a factor of w.  The clipper
     * interpolates s and t (they are two of its nine attributes) and
     * br_dl_finish_vtx recomputes tmu0 from them, so the raw pair is
     * maintained on the clipped path as well. */
    o->s = v->s;
    o->t = v->t;
}

static void br3d_tri(void *pUser, const BrDlVtx *a, const BrDlVtx *b,
                     const BrDlVtx *c)
{
    BrGfx *g = (BrGfx *)pUser;

    /* 0x1001EA80 (G_SETPRIMCOLOR) ends in grConstantColorValue(w1), so the
     * primitive colour IS the Glide constant colour.  BrDlSink has no hook
     * for it, and the original reads renderer state at draw time anyway, so
     * the value is picked up here.  A combiner row that sets its own
     * constant (rows 3, 6 and 9) has already overwritten stKonst and wins
     * until the next G_SETCOMBINE, which is the original's ordering too. */
    if (g->p3dDl != NULL && g->stCombine != BR_DL_CC_TEX_SHADE_C1 &&
        g->stCombine != BR_DL_CC_TEX_SHADE_CW &&
        g->stCombine != BR_DL_CC_TEX_SHADE_C0) {
        g->stKonst[0] = g->p3dDl->prim[0];
        g->stKonst[1] = g->p3dDl->prim[1];
        g->stKonst[2] = g->p3dDl->prim[2];
        g->stKonst[3] = g->p3dDl->prim[3];
    }

    if (br3d_reserve(g, 3) != 0)
        return;
    br3d_want(g);
    br3d_put(g, a);
    br3d_put(g, b);
    br3d_put(g, c);
    g->stats3d.cTri++;
    g->stats3d.cVerts += 3;
    g->stats3d.aCombineUse[g->stCombine]++;
    g->stats3d.aBlendUse[g->stBlend]++;
    g->stats3d.aZUse[g->stZ]++;
}

/* 0x1001E7A0.  Three of the ten rows call grConstantColorValue; the rest
 * leave the constant alone.  br_dl.c has already done the classification --
 * that chain of exact compares is the interpreter's, not the backend's. */
static void br3d_combine(void *pUser, BrDlCombine id, uint32_t w0, uint32_t w1)
{
    BrGfx *g = (BrGfx *)pUser;
    uint32_t i;

    for (i = 0; i < g->stats3d.cSeenCombine; i++)
        if (g->stats3d.aSeenCombine[i][0] == w0 &&
            g->stats3d.aSeenCombine[i][1] == w1)
            break;
    if (i == g->stats3d.cSeenCombine && i < BR_GFX3D_MAX_SEEN) {
        g->stats3d.aSeenCombine[i][0] = w0;
        g->stats3d.aSeenCombine[i][1] = w1;
        g->stats3d.cSeenCombine++;
    }

    g->stCombine = id;
    /* 0x1001E8FB: the DECAL row alone swaps the triangle routine at
     * 0x100A9A68 between 0x10021C70 and 0x100221D0.  Neither has been read
     * (1019 and 1059 bytes), so what the second one does differently is not
     * known.  A decal is a coplanar overlay, so this backend gives it a
     * depth bias and says so.  DEVIATION, and a guess -- but a guess that
     * cannot silently corrupt anything else, because it is confined to one
     * combiner row. */
    g->stDecal = (id == BR_DL_CC_DECAL) ? 1 : 0;

    /* grConstantColorValue is a GrColor_t in R,G,B,A byte order: 0x1001EA80
     * passes G_SETPRIMCOLOR's w1 through unchanged, and br_dl.c's unpack
     * reads that same word as R,G,B,A. */
    if (id == BR_DL_CC_TEX_SHADE_C1) {          /* 0x1001E7FB: 0x000000FF */
        g->stKonst[0] = 0.0f; g->stKonst[1] = 0.0f;
        g->stKonst[2] = 0.0f; g->stKonst[3] = 1.0f;
    } else if (id == BR_DL_CC_TEX_SHADE_CW) {   /* 0x1001E859: -1         */
        g->stKonst[0] = g->stKonst[1] = g->stKonst[2] = g->stKonst[3] = 1.0f;
    } else if (id == BR_DL_CC_TEX_SHADE_C0) {   /* 0x1001E8DB: 0          */
        g->stKonst[0] = g->stKonst[1] = g->stKonst[2] = g->stKonst[3] = 0.0f;
    }
    g->stats3d.cCombineCmds++;
}

/* 0x10021270, transcribed.  Every arm below is one compare in that routine,
 * in the routine's own order, and the value each arm installs is the
 * argument the arm passes to grAlphaBlendFunction / grDepthBufferFunction /
 * grDepthMask.  Arms that change neither leave the state alone -- that is
 * the original's behaviour, not an omission. */
static void br3d_rendermode(void *pUser, uint32_t mode)
{
    BrGfx *g = (BrGfx *)pUser;
    uint32_t i;

    for (i = 0; i < g->stats3d.cSeenMode; i++)
        if (g->stats3d.aSeenMode[i] == mode)
            break;
    if (i == g->stats3d.cSeenMode && i < BR_GFX3D_MAX_SEEN)
        g->stats3d.aSeenMode[g->stats3d.cSeenMode++] = mode;

    g->stats3d.cModeCmds++;
    g->stZWrite = 1;                                    /* grDepthMask(1)   */

    if (mode == 0x00504F50u) {                          /* 0x10021282       */
        if (g->stXluLatch == 0) {
            g->stBlend = BR_GFX3D_BLEND_ALPHA;
            g->stAlphaConst = 0;                        /* (3,8,1,1,0)      */
        }
        g->stZ = BR_GFX3D_Z_EQUAL;                      /* ZMODE_DEC        */
    } else if (mode == 0x00000004u) {                   /* 0x100212E8       */
        if (g->stXluLatch == 0) {
            g->stBlend = BR_GFX3D_BLEND_ADD;
            g->stAlphaConst = 0;
        }
        g->stZ = BR_GFX3D_Z_EQUAL;
    } else if (mode == 0x0C184240u) {                   /* 0x10021351       */
        /* alpha test reference 0x80 only; no blend, no depth change. */
    } else if (mode == 0x00504240u) {                   /* 0x10021377       */
        if (g->stXluLatch == 0) {
            g->stBlend = BR_GFX3D_BLEND_ALPHA;
            g->stAlphaConst = 0;
        }
    } else if (mode == 0x00000003u) {                   /* 0x100213DB       */
        g->stXluLatch = 0;                              /* g_5D17D4 = 0     */
    } else if (mode == 0x00000001u) {                   /* 0x100213EE       */
        g->stZ = BR_GFX3D_Z_LEQUAL;
        g->stZWrite = 0;                                /* grDepthMask(0)   */
    } else if (mode == 0x00000000u) {                   /* 0x10021410       */
        g->stZ = BR_GFX3D_Z_LESS;
    } else if (mode == 0x00000002u) {                   /* 0x10021424       */
        g->stBlend = BR_GFX3D_BLEND_ADD;
        g->stAlphaConst = 1;                            /* (3,4,1,2,0)      */
        g->stZWrite = 0;
        g->stXluLatch = 1;
    } else if (mode == 0x0D1849D8u) {                   /* 0x10021451       */
        g->stBlend = BR_GFX3D_BLEND_ALPHA;              /* ZMODE_XLU        */
        g->stAlphaConst = 1;                            /* (3,4,1,2,0)      */
        g->stZWrite = 0;
        g->stXluLatch = 1;
    } else if ((mode & 0x00001800u) != 0u) {            /* 0x1002149A       */
        /* `test ah,0x18` -- CVG_X_ALPHA (0x1000) or the ZMODE high bit
         * (0x800), i.e. XLU or DEC. */
        g->stats3d.cModeUnrecognised++;
        if ((mode & 0x00010000u) == 0u) {               /* 0x100214A3       */
            if (g->stXluLatch == 0) {
                g->stBlend = BR_GFX3D_BLEND_ALPHA;
                g->stAlphaConst = 0;
            }
            g->stZWrite = 0;
        }
    } else {                                            /* 0x10021524       */
        g->stats3d.cModeUnrecognised++;
        if (g->stXluLatch == 0) {
            g->stBlend = BR_GFX3D_BLEND_OPAQUE;
            g->stAlphaConst = 0;
        }
    }
}

static void br3d_bind(void *pUser, uint32_t handle)
{
    BrGfx *g = (BrGfx *)pUser;
    g->stTex = handle;
    g->stats3d.cBinds++;
}

/* 0xDD re-aims one texture at a new address (the Glide one-texture scheme
 * br_font.c documents).  There is no path yet from a display-list address to
 * pixels, so the address is recorded by rebinding the handle and nothing
 * more.  Deliberately not faked. */
static void br3d_retarget(void *pUser, uint32_t handle, uint32_t addr)
{
    BrGfx *g = (BrGfx *)pUser;
    (void)addr;
    g->stTex = handle;
}

static void br3d_rect(void *pUser, int fTextured, int tile,
                      int32_t ulx, int32_t uly, int32_t lrx, int32_t lry)
{
    BrGfx *g = (BrGfx *)pUser;
    float col[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    BrDlCombine save = g->stCombine;
    BrGfx3dZ    saveZ = g->stZ;
    int         saveW = g->stZWrite;
    int i;

    (void)tile;
    if (br3d_reserve(g, 6) != 0)
        return;

    if (!fTextured && g->p3dDl != NULL) {
        /* DEVIATION: 0xF7's payload format has not been read -- br_dl.c
         * stores the raw word and nothing decodes it.  Taken as one RGBA5551
         * texel, the usual N64 fill value, and flagged so that a later pass
         * that does read the handler knows this is a guess. */
        uint32_t v = g->p3dDl->fillColour & 0xFFFFu;
        col[0] = (float)((v >> 11) & 0x1Fu) / 31.0f;
        col[1] = (float)((v >> 6) & 0x1Fu) / 31.0f;
        col[2] = (float)((v >> 1) & 0x1Fu) / 31.0f;
        col[3] = (float)(v & 1u);
    }

    /* A rectangle is screen space: no perspective, no depth.
     * NOTE THE ENUMERATOR NAMES: BR_DL_CC_SHADE is the row Glide renders as
     * `1.0 * TEXTURE` and BR_DL_CC_TEX is the row it renders as the ITERATED
     * colour -- the two names in br_dl.h are swapped (br_gfx3d.h shows the
     * decode).  A textured rect therefore wants ..._SHADE. */
    g->stCombine = fTextured ? BR_DL_CC_SHADE : BR_DL_CC_TEX;
    g->stZ       = BR_GFX3D_Z_ALWAYS;
    g->stZWrite  = 0;
    g->stUnitUv  = 1;
    br3d_want(g);
    {
        const float xs[6] = { (float)ulx, (float)lrx, (float)ulx,
                              (float)lrx, (float)lrx, (float)ulx };
        const float ys[6] = { (float)uly, (float)uly, (float)lry,
                              (float)uly, (float)lry, (float)lry };
        const float us[6] = { 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f };
        const float vs[6] = { 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f };
        for (i = 0; i < 6; i++) {
            BrGfx3dVtx *o = &g->aVtx3d[g->cVtx3d++];
            o->x = xs[i]; o->y = ys[i];
            o->z = 0.0f;  o->w = 1.0f;
            o->r = col[0]; o->g = col[1]; o->b = col[2]; o->a = col[3];
            o->s = us[i]; o->t = vs[i];
        }
    }
    br3d_flush(g);
    g->stUnitUv  = 0;
    g->stCombine = save;
    g->stZ = saveZ;
    g->stZWrite = saveW;
    g->stats3d.cRects++;
}

void BrGfx3dAttach(BrGfx *g, BrDl *pDl)
{
    if (g == NULL || pDl == NULL)
        return;
    if (BrGfx3dInit(g) != 0)
        return;
    g->p3dDl = pDl;
    memset(&pDl->sink, 0, sizeof(pDl->sink));
    pDl->sink.pUser         = g;
    pDl->sink.pfnTri        = br3d_tri;
    pDl->sink.pfnCombine    = br3d_combine;
    pDl->sink.pfnRenderMode = br3d_rendermode;
    pDl->sink.pfnBindTexture= br3d_bind;
    pDl->sink.pfnRetarget   = br3d_retarget;
    pDl->sink.pfnRect       = br3d_rect;
}

void BrGfx3dMapTexture(BrGfx *g, uint32_t handle, BrTexture tex,
                       float scaleS, float scaleT)
{
    uint32_t i;
    if (g == NULL || handle == BR_GFX3D_TEX_NONE)
        return;
    for (i = 0; i < g->cTexMap; i++)
        if (g->aTexMap[i].handle == handle) {
            g->aTexMap[i].tex = tex;
            g->aTexMap[i].scale[0] = scaleS;
            g->aTexMap[i].scale[1] = scaleT;
            return;
        }
    if (g->cTexMap >= BR_GFX3D_MAX_TEXMAP)
        return;
    g->aTexMap[g->cTexMap].handle = handle;
    g->aTexMap[g->cTexMap].tex    = tex;
    g->aTexMap[g->cTexMap].scale[0] = scaleS;
    g->aTexMap[g->cTexMap].scale[1] = scaleT;
    g->cTexMap++;
}

void BrGfx3dSetDepthTest(BrGfx *g, int fEnable)
{
    if (g != NULL)
        g->fDepthTest = fEnable ? 1 : 0;
}

const BrGfx3dStats *BrGfx3dGetStats(const BrGfx *g)
{
    return (g != NULL) ? &g->stats3d : NULL;
}

/* ---------------------------------------------------------------------
 * the frame
 *
 * Vertices accumulate on the CPU for the whole frame and the pass is
 * encoded in one go at the end.  That is not laziness: a batch flushed into
 * a live encoder would have to write into a buffer the GPU may still be
 * reading, and the alternative (a buffer per flush) allocates once per state
 * change.  Recording draw RANGES and replaying them keeps one allocation and
 * keeps `cDraws` an exact count of state changes.
 * ------------------------------------------------------------------ */

void BrGfx3dBeginFrame(BrGfx *g, float r, float gr, float b, float a)
{
    if (g == NULL || BrGfx3dInit(g) != 0)
        return;
    g->clear3d[0] = r; g->clear3d[1] = gr;
    g->clear3d[2] = b; g->clear3d[3] = a;
    g->cVtx3d = 0;
    g->cDraw3d = 0;
    g->fBatchOpen = 0;
    g->iBatchFirst = 0;
    memset(&g->stats3d, 0, sizeof(g->stats3d));
}

void BrGfx3dEndFrame(BrGfx *g)
{
    MTLRenderPassDescriptor *rp;
    id<MTLCommandBuffer> cb;
    id<MTLRenderCommandEncoder> enc;
    id<MTLBuffer> vb = nil;
    float target[2];
    uint32_t i;

    if (g == NULL || !g->f3d)
        return;
    br3d_flush(g);

    rp = [MTLRenderPassDescriptor renderPassDescriptor];
    rp.colorAttachments[0].texture     = g->target;
    rp.colorAttachments[0].loadAction  = MTLLoadActionClear;
    rp.colorAttachments[0].storeAction = MTLStoreActionStore;
    rp.colorAttachments[0].clearColor  =
        MTLClearColorMake(g->clear3d[0], g->clear3d[1],
                          g->clear3d[2], g->clear3d[3]);
    rp.depthAttachment.texture     = g->depthTarget;
    rp.depthAttachment.loadAction  = MTLLoadActionClear;
    rp.depthAttachment.storeAction = MTLStoreActionDontCare;
    rp.depthAttachment.clearDepth  = 1.0;

    cb  = [g->queue commandBuffer];
    enc = [cb renderCommandEncoderWithDescriptor:rp];

    if (g->cVtx3d > 0) {
        vb = [g->device newBufferWithBytes:g->aVtx3d
                                    length:(NSUInteger)g->cVtx3d * sizeof(BrGfx3dVtx)
                                   options:MTLResourceStorageModeShared];
    }
    target[0] = (float)g->width;
    target[1] = (float)g->height;

    for (i = 0; vb != nil && i < g->cDraw3d; i++) {
        const BrGfx3dDraw *d = &g->aDraw3d[i];
        id<MTLTexture> t = g->whiteTex;
        /* Identity unless a MAPPED handle is in play on a TRIANGLE batch.
         * An unmapped handle samples the 1x1 white texel, where the scale
         * cannot matter; a rectangle already has 0..1 corners. */
        float texScale[2] = { 1.0f, 1.0f };
        uint32_t k;

        for (k = 0; k < g->cTexMap; k++)
            if (g->aTexMap[k].handle == d->tex &&
                g->aTexMap[k].tex != 0 && g->aTexMap[k].tex < g->cTextures) {
                t = g->textures[g->aTexMap[k].tex];
                if (!d->fUnitUv) {
                    texScale[0] = g->aTexMap[k].scale[0];
                    texScale[1] = g->aTexMap[k].scale[1];
                }
                break;
            }

        [enc setRenderPipelineState:g->a3dPipe[d->combine][d->blend]];
        [enc setDepthStencilState:g->a3dDepth[d->z][d->fZWrite ? 1 : 0]];
        /* The decal row's separate triangle routine, approximated. */
        [enc setDepthBias:(d->fDecal ? -2.0f : 0.0f)
               slopeScale:(d->fDecal ? -1.0f : 0.0f)
                    clamp:0.0f];
        [enc setVertexBuffer:vb offset:0 atIndex:0];
        [enc setVertexBytes:target length:sizeof(target) atIndex:1];
        [enc setVertexBytes:texScale length:sizeof(texScale) atIndex:2];
        [enc setFragmentBytes:d->konst length:sizeof(d->konst) atIndex:0];
        [enc setFragmentTexture:t atIndex:0];
        [enc setFragmentSamplerState:g->sampler3d atIndex:0];
        [enc drawPrimitives:MTLPrimitiveTypeTriangle
                vertexStart:d->first
                vertexCount:d->count];
    }

    [enc endEncoding];
    [cb commit];
    [cb waitUntilCompleted];
}
