/* brr_metal_refl.m: the reflection pass (brr_metal_refl.h, DESIGN.md 8.4):
 * its compute pipeline, compiled with the BVH's traversal (brr_rt_library),
 * and its dispatch after the deferred lighting: the trace at half
 * resolution into a half-size target (reflk), its temporal accumulation
 * into a half-size history (reflt, two targets taking turns), then the
 * composite (reflc) reading the lit target and adding the history, filtered
 * and upsampled, into a copy, the copy blitted back.  The copy stays as the
 * last frame's lit target that reflk reads for the BVH's hits on screen. */
#import <Metal/Metal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "brr_metal_lit.h"
#include "brr_metal_rt.h"
#include "brr_metal_refl_api.h"

static id<MTLComputePipelineState> s_rpipe, s_cpipe, s_tpipe, s_lpipe, s_apipe;
static id<MTLTexture> s_rdst, s_rhalf, s_rhist[2];
static float s_pscr[16];
static unsigned s_frame, s_valid;
static id<MTLBuffer> s_rnull;
static int s_rfail;

int brr_refl_tier(void)
{
    const char *e = getenv("BR_REFL");
    return e ? atoi(e) : 2;
}

void brr_refl_pass(id<MTLDevice> dev, id<MTLCommandBuffer> cb, const void *lu, size_t lusz, size_t scroff, const BrrRtWorld *rt,
                   id<MTLTexture> alb, id<MTLTexture> pos, id<MTLTexture> nrm, id<MTLTexture> hdr,
                   id<MTLTexture> sky, id<MTLTexture> bE)
{
    id<MTLComputeCommandEncoder> e;
    id<MTLBlitCommandEncoder> b;
    if (s_rfail || !hdr || !sky || !bE)
        return;
    if (!s_rpipe) {
        NSError *err = nil;
        id<MTLLibrary> lib = brr_rt_library(dev, @BRR_LU_SRC BRR_WET_SRC BRR_ENV_SRC BRR_REFL_SRC, &err);
        id<MTLFunction> f = lib ? [lib newFunctionWithName:@"reflk"] : nil, fc = lib ? [lib newFunctionWithName:@"reflc"] : nil,
                        ft = lib ? [lib newFunctionWithName:@"reflt"] : nil;
        s_rpipe = f ? [dev newComputePipelineStateWithFunction:f error:&err] : nil;
        s_cpipe = s_rpipe && fc ? [dev newComputePipelineStateWithFunction:fc error:&err] : nil;
        s_tpipe = s_cpipe && ft ? [dev newComputePipelineStateWithFunction:ft error:&err] : nil;
        if (lib) {   /* the track's albedo from the frames (talb_learn, talb_resolve) */
            id<MTLFunction> fl = [lib newFunctionWithName:@"talb_learn"], fr = [lib newFunctionWithName:@"talb_resolve"];
            s_lpipe = fl ? [dev newComputePipelineStateWithFunction:fl error:&err] : nil;
            s_apipe = fr ? [dev newComputePipelineStateWithFunction:fr error:&err] : nil;
        }
        if (!s_rpipe || !s_cpipe || !s_tpipe) {
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
        s_rhist[0] = [dev newTextureWithDescriptor:td];
        s_rhist[1] = [dev newTextureWithDescriptor:td];
        s_valid = 0;   /* no history, no last frame at this size */
    }
    {   /* the reprojection: the last frame's scr (LU.scr at scroff) */
        struct { float pscr[16]; uint32_t frame; float valid, pad[2]; } rp;
        memcpy(rp.pscr, s_pscr, sizeof rp.pscr);
        rp.frame = s_frame;
        rp.valid = s_valid ? 1.0f : 0.0f;
        rp.pad[0] = rp.pad[1] = 0.0f;
        e = [cb computeCommandEncoder];
        [e setBytes:&rp length:sizeof rp atIndex:4];
        if (scroff + sizeof s_pscr <= lusz)
            memcpy(s_pscr, (const uint8_t *)lu + scroff, sizeof s_pscr);
    }
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
    [e setBytes:lu length:lusz atIndex:3];   /* LU.wet.w, the roughness below which misses trace: 0 with no world (brr_metal.m) */
    [e setTexture:alb atIndex:0];
    [e setTexture:pos atIndex:1];
    if (rt && rt->built && rt->as && s_lpipe && s_apipe) {   /* the track's albedo learned from this frame, for the hits below and GI's */
        uint32_t n = (uint32_t)rt->bvh.ntri;
        [e setComputePipelineState:s_lpipe];
        [e setBuffer:rt->acc offset:0 atIndex:11];
        [e dispatchThreads:MTLSizeMake((alb.width + 3) / 4, (alb.height + 3) / 4, 1) threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
        [e setComputePipelineState:s_apipe];
        [e setBytes:&n length:sizeof n atIndex:0];
        [e setBuffer:rt->prim offset:0 atIndex:9];
        [e dispatchThreads:MTLSizeMake(n, 1, 1) threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];
        [e setBuffer:rt->node offset:0 atIndex:0];
        [e setBuffer:rt->leaf offset:0 atIndex:10];
        [e setComputePipelineState:s_rpipe];
    } else {
        [e setBuffer:rt && rt->built ? rt->prim : s_rnull offset:0 atIndex:9];
        [e setBuffer:rt && rt->built ? rt->leaf : s_rnull offset:0 atIndex:10];
    }
    [e setTexture:nrm atIndex:2];
    [e setTexture:hdr atIndex:3];
    [e setTexture:s_rhalf atIndex:4];
    [e setTexture:sky atIndex:5];
    [e setTexture:s_rdst atIndex:6];
    [e dispatchThreads:MTLSizeMake(s_rhalf.width, s_rhalf.height, 1) threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
    /* the temporal accumulation: the last frame's history into this frame's */
    {
        id<MTLTexture> hs = s_rhist[s_frame & 1], ho = s_rhist[(s_frame & 1) ^ 1];
        [e setComputePipelineState:s_tpipe];
        [e setTexture:s_rhalf atIndex:3];
        [e setTexture:hs atIndex:4];
        [e setTexture:ho atIndex:5];
        [e dispatchThreads:MTLSizeMake(s_rhalf.width, s_rhalf.height, 1) threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
        [e setTexture:hdr atIndex:3];
        [e setTexture:sky atIndex:5];
        [e setTexture:ho atIndex:7];
    }
    /* the composite: the same encoder (LU at 3, the G-buffer and the lit target bound) */
    [e setComputePipelineState:s_cpipe];
    [e setTexture:s_rdst atIndex:4];
    [e setTexture:bE atIndex:6];
    [e dispatchThreads:MTLSizeMake(hdr.width, hdr.height, 1) threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
    [e endEncoding];
    b = [cb blitCommandEncoder];
    [b copyFromTexture:s_rdst toTexture:hdr];
    [b endEncoding];
    s_frame++;
    s_valid = 1;
}
