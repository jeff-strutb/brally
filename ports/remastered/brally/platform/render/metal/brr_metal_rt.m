/* brr_metal_rt.m: the track's ray-tracing world on the GPU (brr_metal_rt.h;
 * DESIGN.md 8.1): light_bvh.c's tree in four buffers for the compute
 * traversal, built once a track generation. */
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
    w->ntri = 0;
    w->built = 0;
}

int brr_rt_update(BrrRtWorld *w, id<MTLDevice> dev, id<MTLCommandQueue> q, const LightTrack *t)
{
    double t0;
    size_t nb;
    (void)q;
    if (!t || t->ntri <= 0) {
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
}

id<MTLLibrary> brr_rt_library(id<MTLDevice> dev, NSString *src, NSError **err)
{
    MTLCompileOptions *o = [MTLCompileOptions new];
    NSString *all;
    if (@available(macOS 15.0, iOS 18.0, *))
        o.mathMode = MTLMathModeSafe;
    else {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        o.fastMathEnabled = NO;     /* what mathMode replaced, before macOS 15 and iOS 18 */
#pragma clang diagnostic pop
    }
    all = [NSString stringWithFormat:@"#include <metal_stdlib>\nusing namespace metal;\n%s%s%@", BRR_RT_SRC, BRR_RT_SEL_SRC, src ? src : @""];
    return [dev newLibraryWithSource:all options:o error:err];
}
