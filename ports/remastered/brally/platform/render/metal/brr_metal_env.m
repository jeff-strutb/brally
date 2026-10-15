/* brr_metal_env.m: the sky's environment on the GPU (brr_metal_env.h): the
 * skymap's radiance into a cube, its mips, and the GGX-prefiltered specular. */
#import <Metal/Metal.h>
#include <stdio.h>
#include <stdlib.h>
#include "brr_metal_env.h"
#include "brr_metal_env_api.h"

static id<MTLRenderPipelineState> env_pipe(id<MTLDevice> dev, id<MTLLibrary> lib, NSString *fs, MTLPixelFormat pf)
{
    MTLRenderPipelineDescriptor *d = [MTLRenderPipelineDescriptor new];
    NSError *err = nil;
    id<MTLRenderPipelineState> p;
    d.vertexFunction = [lib newFunctionWithName:@"lvs"];
    d.fragmentFunction = [lib newFunctionWithName:fs];
    d.colorAttachments[0].pixelFormat = pf;
    p = d.vertexFunction && d.fragmentFunction ? [dev newRenderPipelineStateWithDescriptor:d error:&err] : nil;
    if (!p)
        fprintf(stderr, "brr: env pipeline %s: %s\n", [fs UTF8String], err ? [[err description] UTF8String] : "no function");
    return p;
}

/* one full-screen draw into face f, mip m of cube t */
static void env_draw(id<MTLCommandBuffer> cb, id<MTLRenderPipelineState> p, id<MTLTexture> t, int f, int m,
                     id<MTLTexture> src, const float prm[4])
{
    MTLRenderPassDescriptor *rp = [MTLRenderPassDescriptor renderPassDescriptor];
    id<MTLRenderCommandEncoder> e;
    rp.colorAttachments[0].texture = t;
    rp.colorAttachments[0].slice = (NSUInteger)f;
    rp.colorAttachments[0].level = (NSUInteger)m;
    rp.colorAttachments[0].loadAction = MTLLoadActionDontCare;
    rp.colorAttachments[0].storeAction = MTLStoreActionStore;
    e = [cb renderCommandEncoderWithDescriptor:rp];
    [e setRenderPipelineState:p];
    [e setFragmentTexture:src atIndex:0];
    [e setFragmentBytes:prm length:16 atIndex:0];
    [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    [e endEncoding];
}

int brr_env_build(BrrEnv *e, id<MTLDevice> dev, id<MTLLibrary> lib, id<MTLCommandQueue> q, const LightSky *s)
{
    static id<MTLRenderPipelineState> peq, ppf;
    MTLTextureDescriptor *td;
    id<MTLTexture> eq, cube, spec;
    id<MTLCommandBuffer> cb;
    id<MTLBlitCommandEncoder> be;
    int f, m, csz = 256;
    if (!s || !s->rgb)
        return -1;
    while (csz < ENV_CUBE && csz * 4 < s->w)   /* a quarter turn of the map a face: 2048 from 8K, 1024 from 4K */
        csz *= 2;
    if (!peq)
        peq = env_pipe(dev, lib, @"env_eq2cube", MTLPixelFormatRGB9E5Float);
    if (!ppf)
        ppf = env_pipe(dev, lib, @"env_prefilter", MTLPixelFormatRGBA16Float);
    if (!peq || !ppf)
        return -1;
    /* the map, half float (an 8K map: 268 MB) for the cube's pass alone, filled
       a band of rows at a time */
    td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA16Float width:(NSUInteger)s->w
                                                           height:(NSUInteger)s->h mipmapped:NO];
    td.usage = MTLTextureUsageShaderRead;
    td.storageMode = MTLStorageModeShared;
    eq = [dev newTextureWithDescriptor:td];
    if (!eq)
        return -1;
    {
        const int band = 64;
        __fp16 *hp = malloc((size_t)s->w * band * 8);
        int y0, y, x, c;
        if (!hp)
            return -1;
        for (y0 = 0; y0 < s->h; y0 += band) {
            int nb = s->h - y0 < band ? s->h - y0 : band;
            for (y = 0; y < nb; y++)
                for (x = 0; x < s->w; x++) {
                    const float *p = s->rgb + ((size_t)(y0 + y) * s->w + x) * 3;
                    __fp16 *q = hp + ((size_t)y * s->w + x) * 4;
                    for (c = 0; c < 3; c++)
                        q[c] = (__fp16)fminf(p[c], 65000.0f);
                    q[3] = (__fp16)1.0f;
                }
            [eq replaceRegion:MTLRegionMake2D(0, (NSUInteger)y0, (NSUInteger)s->w, (NSUInteger)nb) mipmapLevel:0 withBytes:hp
                  bytesPerRow:(NSUInteger)s->w * 8];
        }
        free(hp);
    }
    td = [MTLTextureDescriptor textureCubeDescriptorWithPixelFormat:MTLPixelFormatRGB9E5Float size:(NSUInteger)csz mipmapped:YES];
    td.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    td.storageMode = MTLStorageModePrivate;
    cube = [dev newTextureWithDescriptor:td];
    td = [MTLTextureDescriptor textureCubeDescriptorWithPixelFormat:MTLPixelFormatRGBA16Float size:ENV_SPEC mipmapped:YES];
    td.mipmapLevelCount = ENV_SPEC_MIPS;
    td.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    td.storageMode = MTLStorageModePrivate;
    spec = [dev newTextureWithDescriptor:td];
    if (!eq || !cube || !spec)
        return -1;
    cb = [q commandBuffer];
    for (f = 0; f < 6; f++) {
        const float prm[4] = { (float)f, (float)csz, s->rot, 0.0f };
        env_draw(cb, peq, cube, f, 0, eq, prm);
    }
    be = [cb blitCommandEncoder];
    [be generateMipmapsForTexture:cube];
    [be endEncoding];
    for (m = 0; m < ENV_SPEC_MIPS; m++)
        for (f = 0; f < 6; f++) {
            const float prm[4] = { (float)f, (float)(ENV_SPEC >> m), (float)m / (float)(ENV_SPEC_MIPS - 1), (float)csz };
            env_draw(cb, ppf, spec, f, m, cube, prm);
        }
    [cb commit];
    [cb waitUntilCompleted];
    if (cb.status != MTLCommandBufferStatusCompleted) {
        fprintf(stderr, "brr: env build failed: %s\n", cb.error ? [[cb.error description] UTF8String] : "?");
        return -1;
    }
    e->cube = cube;
    e->spec = spec;
    e->rot = s->rot;
    e->ready = 1;
    return 0;
}
