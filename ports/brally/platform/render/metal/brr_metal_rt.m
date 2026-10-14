/* brr_metal_rt.m: the track's ray-tracing world on the GPU (brr_metal_rt.h;
 * DESIGN.md 8.1): light_bvh.c's tree in four buffers for the compute
 * traversal, and where the device supports ray tracing, a primitive
 * acceleration structure over the same triangles, built and compacted once a
 * track generation. */
#import <Metal/Metal.h>
#include <mach/mach_time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "brr_metal_rt.h"

static double ms_now(void)
{
    static mach_timebase_info_data_t tb;
    if (!tb.denom)
        mach_timebase_info(&tb);
    return (double)mach_absolute_time() * tb.numer / tb.denom * 1e-6;
}

void brr_rt_release(BrrRtWorld *w)
{
    light_bvh_free(&w->bvh);
    w->node = w->tri = w->id = w->prim = w->leaf = w->acc = nil;
    w->as = nil;
    w->ntri = 0;
    w->built = 0;
}

/* the structure over t's triangles in their own order, so its primitive id is
 * the track's triangle number; built for tracing speed, then compacted */
static id<MTLAccelerationStructure> build_as(id<MTLDevice> dev, id<MTLCommandQueue> q, const LightTrack *t)
{
    MTLAccelerationStructureTriangleGeometryDescriptor *g = [MTLAccelerationStructureTriangleGeometryDescriptor descriptor];
    MTLPrimitiveAccelerationStructureDescriptor *d = [MTLPrimitiveAccelerationStructureDescriptor descriptor];
    MTLAccelerationStructureSizes sz;
    id<MTLAccelerationStructure> as, cas;
    id<MTLBuffer> vb, scratch, csize;
    id<MTLCommandBuffer> cb;
    id<MTLAccelerationStructureCommandEncoder> ae;
    uint32_t compacted;

    vb = [dev newBufferWithBytes:t->tri length:(size_t)t->ntri * 9 * sizeof(float) options:MTLResourceStorageModeShared];
    g.vertexBuffer = vb;
    g.vertexStride = 3 * sizeof(float);
    g.triangleCount = (NSUInteger)t->ntri;
    g.opaque = YES;
    d.geometryDescriptors = @[ g ];
    sz = [dev accelerationStructureSizesWithDescriptor:d];
    as = [dev newAccelerationStructureWithSize:sz.accelerationStructureSize];
    scratch = [dev newBufferWithLength:sz.buildScratchBufferSize options:MTLResourceStorageModePrivate];
    csize = [dev newBufferWithLength:sizeof(uint32_t) options:MTLResourceStorageModeShared];
    if (!vb || !as || !scratch || !csize)
        return nil;
    cb = [q commandBuffer];
    ae = [cb accelerationStructureCommandEncoder];
    [ae buildAccelerationStructure:as descriptor:d scratchBuffer:scratch scratchBufferOffset:0];
    [ae writeCompactedAccelerationStructureSize:as toBuffer:csize offset:0];
    [ae endEncoding];
    [cb commit];
    [cb waitUntilCompleted];
    if (cb.status != MTLCommandBufferStatusCompleted)
        return nil;
    memcpy(&compacted, [csize contents], sizeof compacted);
    if (!compacted || compacted >= sz.accelerationStructureSize)
        return as;
    cas = [dev newAccelerationStructureWithSize:compacted];
    if (!cas)
        return as;
    cb = [q commandBuffer];
    ae = [cb accelerationStructureCommandEncoder];
    [ae copyAndCompactAccelerationStructure:as toAccelerationStructure:cas];
    [ae endEncoding];
    [cb commit];
    [cb waitUntilCompleted];
    return cb.status == MTLCommandBufferStatusCompleted ? cas : as;
}

int brr_rt_update(BrrRtWorld *w, id<MTLDevice> dev, id<MTLCommandQueue> q, const LightTrack *t)
{
    double t0;
    size_t nb;
    if (!t || t->ntri <= 0 || !brr_rt_hardware(dev)) {   /* hardware ray tracing or none */
        brr_rt_release(w);
        return 0;
    }
    if (w->built && w->gen == t->gen && w->ntri == t->ntri)
        return 1;
    brr_rt_release(w);
    t0 = ms_now();
    if (!light_bvh_build(&w->bvh, t->tri, t->obj, t->ntri))
        return 0;
    w->bvh_ms = ms_now() - t0;
    nb = (size_t)w->bvh.nnode * sizeof(LightBvhNode);
    w->node = [dev newBufferWithBytes:w->bvh.node length:nb options:MTLResourceStorageModeShared];
    w->tri = [dev newBufferWithBytes:w->bvh.tri length:(size_t)w->bvh.ntri * 9 * sizeof(float) options:MTLResourceStorageModeShared];
    w->id = [dev newBufferWithBytes:w->bvh.id length:(size_t)w->bvh.ntri * sizeof(uint32_t) options:MTLResourceStorageModeShared];
    w->prim = [dev newBufferWithBytes:w->bvh.prim length:(size_t)w->bvh.ntri * sizeof(LightBvhPrim) options:MTLResourceStorageModeShared];
    {
        uint32_t *lm = malloc((size_t)w->bvh.ntri * sizeof *lm);
        int j;
        if (lm) {
            for (j = 0; j < w->bvh.ntri; j++)
                lm[w->bvh.id[j]] = (uint32_t)j;
            w->leaf = [dev newBufferWithBytes:lm length:(size_t)w->bvh.ntri * sizeof *lm options:MTLResourceStorageModeShared];
            free(lm);
        }
    }
    w->acc = [dev newBufferWithLength:(size_t)w->bvh.ntri * 16 options:MTLResourceStorageModeShared];
    if (w->acc)
        memset([w->acc contents], 0, (size_t)w->bvh.ntri * 16);
    if (!w->node || !w->tri || !w->id || !w->prim || !w->leaf || !w->acc) {
        brr_rt_release(w);
        return 0;
    }
    w->bvh_bytes = nb + (size_t)w->bvh.ntri * (9 * sizeof(float) + sizeof(uint32_t) + sizeof(LightBvhPrim));
    w->hw = 1;
    w->as_ms = 0.0;
    w->as_bytes = 0;
    {
        t0 = ms_now();
        w->as = build_as(dev, q, t);
        w->as_ms = ms_now() - t0;
        w->as_bytes = w->as ? w->as.size : 0;
        if (!w->as) {   /* the effects' kernels are built for the intersector here: no world without it */
            fprintf(stderr, "brr: ray tracing: the acceleration structure did not build\n");
            brr_rt_release(w);
            return 0;
        }
    }
    w->ntri = t->ntri;
    w->gen = t->gen;
    w->built = 1;
    return 1;
}

void brr_rt_bind(const BrrRtWorld *w, id<MTLComputeCommandEncoder> e, int base)
{
    [e setBuffer:w->node offset:0 atIndex:base];
    [e setBuffer:w->tri offset:0 atIndex:base + 1];
    [e setBuffer:w->id offset:0 atIndex:base + 2];
    [e setBuffer:w->prim offset:0 atIndex:base + 3];
    if (w->as)   /* the kernels' RT_AS(base + 4) */
        [e setAccelerationStructure:w->as atBufferIndex:base + 4];
}

/* hardware ray tracing: the intersector on Apple GPU family 9 and later
 * (M3, A17 Pro and on), which trace in dedicated units */
int brr_rt_hardware(id<MTLDevice> dev)
{
    if (!dev.supportsRaytracing)
        return 0;
    if (@available(macOS 10.15, iOS 13.0, *))
        return [dev supportsFamily:MTLGPUFamilyApple9];
    return 0;
}

id<MTLLibrary> brr_rt_library(id<MTLDevice> dev, NSString *src, NSError **err)
{
    MTLCompileOptions *o = [MTLCompileOptions new];
    NSString *all;
    int hw = brr_rt_hardware(dev);
    if (@available(macOS 15.0, iOS 18.0, *))
        o.mathMode = MTLMathModeSafe;
    else {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        o.fastMathEnabled = NO;     /* what mathMode replaced, before macOS 15 and iOS 18 */
#pragma clang diagnostic pop
    }
    if (!hw)   /* no hardware ray tracing: the effects that trace are off */
        return nil;
    all = [NSString stringWithFormat:@"#include <metal_stdlib>\nusing namespace metal;\n#define BRR_RT_HW 1\n%s%s%s%@",
           BRR_RT_SRC, BRR_RT_HW_SRC, BRR_RT_SEL_SRC, src ? src : @""];
    return [dev newLibraryWithSource:all options:o error:err];
}
