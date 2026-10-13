/* brr_metal_refl.m: the reflection pass (brr_metal_refl.h, DESIGN.md 8.4):
 * its compute pipeline, compiled with the BVH's traversal (brr_rt_library),
 * and its dispatch after the deferred lighting: the trace at half
 * resolution into a half-size target (reflk), then the composite (reflc)
 * reading the lit target and adding the upsampled reflections into a copy,
 * the copy blitted back. */
#import <Metal/Metal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "brr_metal_lit.h"
#include "brr_metal_rt.h"
#include "brr_metal_refl_api.h"

static id<MTLComputePipelineState> s_rpipe, s_cpipe;
static id<MTLTexture> s_rdst, s_rhalf;
static id<MTLBuffer> s_rnull;
static int s_rfail;

int brr_refl_tier(void)
{
    const char *e = getenv("BR_REFL");
    return e ? atoi(e) : 2;
}

void brr_refl_pass(id<MTLDevice> dev, id<MTLCommandBuffer> cb, const void *lu, size_t lusz, const BrrRtWorld *rt,
                   id<MTLTexture> alb, id<MTLTexture> pos, id<MTLTexture> nrm, id<MTLTexture> hdr,
                   id<MTLTexture> sky, id<MTLTexture> bE)
{
    id<MTLComputeCommandEncoder> e;
    id<MTLBlitCommandEncoder> b;
    if (s_rfail || !hdr || !sky || !bE)
        return;
    if (!s_rpipe) {
        NSError *err = nil;
        id<MTLLibrary> lib = brr_rt_library(dev, @BRR_LU_SRC BRR_WET_SRC BRR_REFL_SRC, &err);
        id<MTLFunction> f = lib ? [lib newFunctionWithName:@"reflk"] : nil, fc = lib ? [lib newFunctionWithName:@"reflc"] : nil;
        s_rpipe = f ? [dev newComputePipelineStateWithFunction:f error:&err] : nil;
        s_cpipe = s_rpipe && fc ? [dev newComputePipelineStateWithFunction:fc error:&err] : nil;
        if (!s_rpipe || !s_cpipe) {
            fprintf(stderr, "brr: reflections: %s\n", lib ? [[err description] UTF8String] : "off: no hardware ray tracing");
            s_rfail = 1;
            return;
        }
        s_rnull = [dev newBufferWithLength:64 options:MTLResourceStorageModePrivate];
    }
    if (!s_rdst || s_rdst.width != hdr.width || s_rdst.height != hdr.height) {
        MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:hdr.pixelFormat width:hdr.width
                                                                                     height:hdr.height mipmapped:NO];
        td.usage = MTLTextureUsageShaderWrite | MTLTextureUsageShaderRead;
        td.storageMode = MTLStorageModePrivate;
        s_rdst = [dev newTextureWithDescriptor:td];
        td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA16Float width:(hdr.width + 1) / 2
                                                               height:(hdr.height + 1) / 2 mipmapped:NO];
        td.usage = MTLTextureUsageShaderWrite | MTLTextureUsageShaderRead;
        td.storageMode = MTLStorageModePrivate;
        s_rhalf = [dev newTextureWithDescriptor:td];
    }
    e = [cb computeCommandEncoder];
    [e setComputePipelineState:s_rpipe];
    if (rt && rt->built) {
        [e setBuffer:rt->node offset:0 atIndex:0];
        [e setBuffer:rt->tri offset:0 atIndex:1];
        [e setBuffer:rt->id offset:0 atIndex:2];
    } else {
        int k;
        for (k = 0; k < 3; k++)
            [e setBuffer:s_rnull offset:0 atIndex:(NSUInteger)k];
    }
    if (rt && rt->built && rt->as)   /* the misses traced in hardware (RT_AS(8)) */
        [e setAccelerationStructure:rt->as atBufferIndex:8];
    {   /* no world yet: nothing traced (LU.wet, the last field: w the roughness below which misses trace) */
        uint8_t ul[1024];
        if (!(rt && rt->built && rt->as) && lusz <= sizeof ul) {
            float zero = 0.0f;
            memcpy(ul, lu, lusz);
            memcpy(ul + lusz - sizeof(float), &zero, sizeof zero);
            [e setBytes:ul length:lusz atIndex:3];
        } else
            [e setBytes:lu length:lusz atIndex:3];
    }
    [e setTexture:alb atIndex:0];
    [e setTexture:pos atIndex:1];
    [e setTexture:nrm atIndex:2];
    [e setTexture:hdr atIndex:3];
    [e setTexture:s_rhalf atIndex:4];
    [e setTexture:sky atIndex:5];
    [e dispatchThreads:MTLSizeMake(s_rhalf.width, s_rhalf.height, 1) threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
    /* the composite: the same encoder (LU at 3, the G-buffer and the lit target bound) */
    [e setComputePipelineState:s_cpipe];
    [e setTexture:s_rdst atIndex:4];
    [e setTexture:bE atIndex:6];
    [e setTexture:s_rhalf atIndex:7];
    [e dispatchThreads:MTLSizeMake(hdr.width, hdr.height, 1) threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
    [e endEncoding];
    b = [cb blitCommandEncoder];
    [b copyFromTexture:s_rdst toTexture:hdr];
    [b endEncoding];
}
