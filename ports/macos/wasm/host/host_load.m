/* host_load.m -- what the Remastered loaders share (port code).
 *
 * Every Remastered asset is loaded off the render thread and in parallel:
 * a PNG decodes on whichever core is free, a big CPU pass over its pixels
 * splits by rows, and the mip chains the GPU builds for many textures go in
 * one command buffer with one wait, not a queue and a wait per texture.
 */
#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <ImageIO/ImageIO.h>
#include "host.h"
#include <stdlib.h>
#include <string.h>

/* An 8-bit PNG as tightly packed RGBA, straight alpha as stored (three
 * channels get alpha 255).  *spp: the file's samples per pixel.  NULL when
 * missing or not 8-bit RGB(A).  Safe on any thread. */
u8 *hload_png(const char *path, int *w, int *h, int *spp)
{
    NSData *data = [NSData dataWithContentsOfFile:@(path) options:NSDataReadingMappedIfSafe error:nil];
    NSDictionary *opt = @{ (id)kCGImageSourceShouldCache: @NO };
    CGImageSourceRef src;
    CGImageRef img;
    CFDataRef raw;
    const u8 *s;
    size_t bpr, bpp, x, y, iw, ih;
    CGImageAlphaInfo ai;
    u8 *px;
    if (!data) return NULL;
    src = CGImageSourceCreateWithData((__bridge CFDataRef)data, (__bridge CFDictionaryRef)opt);
    if (!src) return NULL;
    img = CGImageSourceCreateImageAtIndex(src, 0, (__bridge CFDictionaryRef)opt);
    CFRelease(src);
    if (!img) return NULL;
    bpp = CGImageGetBitsPerPixel(img);
    ai = CGImageGetAlphaInfo(img);
    /* what NSBitmapImageRep hands back for these files, byte for byte:
     * 8-bit, interleaved RGB or RGBA, alpha last, not floating point */
    if (CGImageGetBitsPerComponent(img) != 8 || (bpp != 24 && bpp != 32) ||
        (CGImageGetBitmapInfo(img) & (kCGBitmapFloatComponents | kCGBitmapByteOrderMask)) ||
        (bpp == 32 && ai != kCGImageAlphaLast && ai != kCGImageAlphaPremultipliedLast) ||
        (bpp == 24 && ai != kCGImageAlphaNone)) {
        CGImageRelease(img);
        return NULL;
    }
    iw = CGImageGetWidth(img); ih = CGImageGetHeight(img); bpr = CGImageGetBytesPerRow(img);
    raw = CGDataProviderCopyData(CGImageGetDataProvider(img));
    CGImageRelease(img);
    if (!raw) return NULL;
    s = CFDataGetBytePtr(raw);
    px = malloc(iw * ih * 4);
    if (bpp == 32)
        for (y = 0; y < ih; y++) memcpy(px + y * iw * 4, s + y * bpr, iw * 4);
    else
        for (y = 0; y < ih; y++)
            for (x = 0; x < iw; x++) {
                const u8 *q = s + y * bpr + x * 3;
                u8 *d = px + (y * iw + x) * 4;
                d[0] = q[0]; d[1] = q[1]; d[2] = q[2]; d[3] = 255;
            }
    CFRelease(raw);
    *w = (int)iw; *h = (int)ih; *spp = (int)(bpp / 8);
    return px;
}

/* f(i) for i in [0, n), across the cores */
void hload_for(int n, void (^f)(int i))
{
    if (n <= 0) return;
    if (n == 1) { f(0); return; }
    dispatch_apply((size_t)n, DISPATCH_APPLY_AUTO, ^(size_t i) { f((int)i); });
}

/* rows [0, h) in bands of 32, across the cores */
void hload_rows(int h, void (^f)(int y0, int y1))
{
    hload_for((h + 31) / 32, ^(int b) { f(b * 32, b * 32 + 32 > h ? h : b * 32 + 32); });
}

/* A batch of textures whose mip chains the GPU builds together: the caller
 * owns it (an NSMutableArray), adds from any thread, and flushes it once. */
static id<MTLCommandQueue> g_q;
static dispatch_once_t g_once;

void hload_mips(NSMutableArray *batch, id<MTLTexture> t)
{
    if (!t) return;
    @synchronized (batch) { [batch addObject:t]; }
}

/* build the batch's mip chains; returns when the GPU has */
void hload_flush(NSMutableArray *batch)
{
    id<MTLCommandBuffer> cb;
    id<MTLBlitCommandEncoder> b;
    if (!batch.count) return;
    dispatch_once(&g_once, ^{ g_q = [((id<MTLTexture>)batch[0]).device newCommandQueue]; });
    cb = [g_q commandBuffer];
    b = [cb blitCommandEncoder];
    for (id<MTLTexture> t in batch) [b generateMipmapsForTexture:t];
    [b endEncoding];
    [cb commit];
    [cb waitUntilCompleted];
    [batch removeAllObjects];
}
