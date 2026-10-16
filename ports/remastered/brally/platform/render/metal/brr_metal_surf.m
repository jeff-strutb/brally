/* brr_metal_surf.m: the Remastered textures' GPU side on Metal (brr_metal_surf_api.h).
 *
 * The sets (light_surf_sets_load: 2048 on the Mac, 1024 on the iPhone) into
 * two texture arrays, RGBA8 (the albedo's detail, the height) and RG8 (the
 * normal's x and y), each slice blitted from a staging buffer, then their mip
 * chains (the mean of each 2 x 2: the detail averages toward 1 and the
 * normals shorten with distance, which is the detail's fade); the CPU's
 * texels freed.  Each map (light_surf_entry_map) as RGBA8 and RG8 textures,
 * made on first use. */
#import <Metal/Metal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <TargetConditionals.h>

#include "brr_metal_surf_api.h"

#define ENTRIES 4096

static id<MTLDevice> s_dev;
static id<MTLCommandQueue> s_q;
static __strong id<MTLTexture> s_map[ENTRIES], s_var[ENTRIES];
static id<MTLTexture> s_seta, s_setb, s_dummy, s_dummya, s_dummyb;
static int s_up;                      /* the sets: 0 not asked, 1 reading, 2 on the GPU */

void brr_surf_init(id<MTLDevice> dev, id<MTLCommandQueue> q)
{
    MTLTextureDescriptor *td;
    uint8_t z[4] = { 128, 128, 128, 255 };
    if (s_dev)
        return;
    s_dev = dev;
    s_q = q;
    td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:1 height:1 mipmapped:NO];
    s_dummy = [dev newTextureWithDescriptor:td];
    [s_dummy replaceRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0 withBytes:z bytesPerRow:4];
    td.textureType = MTLTextureType2DArray;
    td.arrayLength = 1;
    s_dummya = [dev newTextureWithDescriptor:td];
    [s_dummya replaceRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0 slice:0 withBytes:z bytesPerRow:4 bytesPerImage:4];
    td.pixelFormat = MTLPixelFormatRG8Unorm;
    s_dummyb = [dev newTextureWithDescriptor:td];
    [s_dummyb replaceRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0 slice:0 withBytes:z bytesPerRow:2 bytesPerImage:2];
}

static void upload_sets(void)
{
    int size = light_surf_sets_size(), i, k;
    MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                  width:(NSUInteger)size height:(NSUInteger)size mipmapped:YES];
    id<MTLCommandBuffer> cb = [s_q commandBuffer];
    id<MTLBlitCommandEncoder> bl = [cb blitCommandEncoder];
    td.textureType = MTLTextureType2DArray;
    td.arrayLength = (NSUInteger)k_light_surf_nset;
    td.usage = MTLTextureUsageShaderRead;
    td.storageMode = MTLStorageModePrivate;
    s_seta = [s_dev newTextureWithDescriptor:td];
    td.pixelFormat = MTLPixelFormatRG8Unorm;
    s_setb = [s_dev newTextureWithDescriptor:td];
    for (i = 0; i < k_light_surf_nset; i++)
        for (k = 0; k < 2; k++) {
            int bpp = k ? 2 : 4;
            id<MTLBuffer> st = [s_dev newBufferWithBytes:k ? light_surf_set_b(i) : light_surf_set_a(i)
                                                  length:(NSUInteger)size * size * bpp options:MTLResourceStorageModeShared];
            [bl copyFromBuffer:st sourceOffset:0 sourceBytesPerRow:(NSUInteger)size * bpp sourceBytesPerImage:(NSUInteger)size * size * bpp
                    sourceSize:MTLSizeMake((NSUInteger)size, (NSUInteger)size, 1) toTexture:k ? s_setb : s_seta destinationSlice:(NSUInteger)i
              destinationLevel:0 destinationOrigin:MTLOriginMake(0, 0, 0)];
        }
    [bl generateMipmapsForTexture:s_seta];
    [bl generateMipmapsForTexture:s_setb];
    [bl endEncoding];
    [cb commit];
    [cb waitUntilCompleted];   /* (once a run: the staging buffers' texels copied before they are freed) */
    light_surf_sets_release();
    if (getenv("BR_LIGHT_STATS"))
        fprintf(stderr, "brr: surface sets on the GPU: %d at %d\n", k_light_surf_nset, size);
}

void brr_surf_frame(void)
{
    if (!s_dev)
        return;
    if (s_up == 0) {
#if TARGET_OS_IPHONE
        light_surf_sets_load(1024);
#else
        light_surf_sets_load(2048);
#endif
        s_up = 1;
    }
    if (s_up == 1 && light_surf_sets_ready()) {
        upload_sets();
        s_up = 2;
    }
}

static int entry_made(int entry)
{
    const light_surf_map *m;
    if (entry < 0 || entry >= ENTRIES)
        return 0;
    if (s_map[entry])
        return 1;
    if (!(m = light_surf_entry_map(entry)))
        return 0;
    {
        MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                      width:(NSUInteger)m->w height:(NSUInteger)m->h mipmapped:NO];
        id<MTLTexture> mt, vt;
        td.usage = MTLTextureUsageShaderRead;
        mt = [s_dev newTextureWithDescriptor:td];
        [mt replaceRegion:MTLRegionMake2D(0, 0, (NSUInteger)m->w, (NSUInteger)m->h) mipmapLevel:0 withBytes:m->map bytesPerRow:(NSUInteger)m->w * 4];
        td.pixelFormat = MTLPixelFormatRG8Unorm;
        vt = [s_dev newTextureWithDescriptor:td];
        [vt replaceRegion:MTLRegionMake2D(0, 0, (NSUInteger)m->w, (NSUInteger)m->h) mipmapLevel:0 withBytes:m->var bytesPerRow:(NSUInteger)m->w * 2];
        s_map[entry] = mt;
        s_var[entry] = vt;
    }
    return 1;
}

int brr_surf_ready(int entry)
{
    return s_up == 2 && entry_made(entry);
}

void brr_surf_bind_at(id<MTLRenderCommandEncoder> e, int entry, int at)
{
    int ok = entry >= 0 && brr_surf_ready(entry);
    [e setFragmentTexture:ok ? s_map[entry] : s_dummy atIndex:(NSUInteger)at];
    [e setFragmentTexture:ok ? s_var[entry] : s_dummy atIndex:(NSUInteger)at + 1];
    [e setFragmentTexture:s_up == 2 ? s_seta : s_dummya atIndex:(NSUInteger)at + 2];
    [e setFragmentTexture:s_up == 2 ? s_setb : s_dummyb atIndex:(NSUInteger)at + 3];
}

void brr_surf_bind(id<MTLRenderCommandEncoder> e, int entry) { brr_surf_bind_at(e, entry, 11); }
