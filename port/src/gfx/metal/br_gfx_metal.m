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

#include <stdlib.h>
#include <string.h>

#define BR_GFX_MAX_TEXTURES 1024

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
