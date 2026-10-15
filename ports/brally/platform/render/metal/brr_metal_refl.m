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
#include "brr_metal_gi_src.h"
#include "brr_metal_lights.h"

static int s_hitcheck = -1;
static id<MTLBuffer> s_hcbuf, s_hcthis;   /* BR_LIGHT_HITCHECK's sums (a scratch buffer when off) */
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
                   id<MTLTexture> sky, id<MTLTexture> bE, const void *giu, size_t gisz, id<MTLBuffer> girr, id<MTLTexture> gdst,
                   id<MTLBuffer> lset, id<MTLBuffer> gcnt, id<MTLBuffer> glst, id<MTLBuffer> goff)
{
    id<MTLComputeCommandEncoder> e;
    id<MTLBlitCommandEncoder> b;
    if (s_rfail || !hdr || !sky || !bE)
        return;
    if (!s_rpipe) {
        NSError *err = nil;
        id<MTLLibrary> lib = brr_rt_library(dev, @BRR_LU_SRC BRR_WET_SRC BRR_ENV_SRC BRR_GI_DEFS BRR_LIGHTS_SRC BRR_GI_SRC BRR_REFL_SRC, &err);
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
            fprintf(stderr, "brr: reflections: %s\n", err ? [[err description] UTF8String] : "off: no hardware ray tracing");
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
        if (s_hitcheck < 0)
            s_hitcheck = getenv("BR_LIGHT_HITCHECK") != NULL;
        if (!s_hcbuf)
            s_hcbuf = [dev newBufferWithLength:64 options:MTLResourceStorageModeShared];
        if (s_hitcheck && s_frame % 120 == 60 && rt && rt->built) {   /* the hits' model against the frame where both see a point */
            id<MTLBuffer> hb = [dev newBufferWithLength:64 options:MTLResourceStorageModeShared];
            unsigned long fr = s_frame;
            memset([hb contents], 0, 64);
            s_hcthis = hb;
            rp.pad[0] = 1.0f;
            [cb addCompletedHandler:^(id<MTLCommandBuffer> c) {
                const int32_t *v = (const int32_t *)[hb contents];
                (void)c;
                if (v[4])
                    fprintf(stderr, "hitcheck: frame %lu, %d hits seen on screen: log2(model / frame) probes mean %+.3f |mean| %.3f, sky hemisphere mean %+.3f |mean| %.3f\n",
                            fr, v[4], v[0] / 1000.0 / v[4], v[1] / 1000.0 / v[4], v[2] / 1000.0 / v[4], v[3] / 1000.0 / v[4]);
                if (v[9])
                    fprintf(stderr, "hitcheck: frame %lu, %d of them in shade: probes mean %+.3f |mean| %.3f, sky hemisphere mean %+.3f |mean| %.3f\n",
                            fr, v[9], v[5] / 1000.0 / v[9], v[6] / 1000.0 / v[9], v[7] / 1000.0 / v[9], v[8] / 1000.0 / v[9]);
                if (v[13])
                    fprintf(stderr, "hitcheck: frame %lu, %d in shade: log2 probes at the hit / at the frame's pixel %+.3f, with the frame's normal %+.3f, normals' cosine %.3f\n",
                            fr, v[13], v[10] / 1000.0 / v[13], v[11] / 1000.0 / v[13], v[12] / 1000.0 / v[13]);
            }];
        } else
            s_hcthis = s_hcbuf;
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
    {   /* the probes' light for the hits (gu.n.x 0: off) */
        static const uint8_t zero[512];
        [e setBuffer:s_hcthis offset:0 atIndex:13];
        [e setBytes:giu && girr && gdst ? giu : zero length:giu && girr && gdst ? gisz : sizeof zero atIndex:12];
        [e setBuffer:girr ? girr : s_rnull offset:0 atIndex:18];
        [e setTexture:gdst ? gdst : s_rdst atIndex:9];
        [e setBuffer:lset && gcnt && glst ? lset : s_rnull offset:0 atIndex:14];
        [e setBuffer:lset && gcnt && glst ? gcnt : s_rnull offset:0 atIndex:15];
        [e setBuffer:lset && gcnt && glst ? glst : s_rnull offset:0 atIndex:16];
        [e setBuffer:goff ? goff : s_rnull offset:0 atIndex:17];
    }
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

void brr_suncheck_pass(id<MTLDevice> dev, id<MTLCommandBuffer> cb, const void *lu, size_t lusz, const BrrRtWorld *rt,
                       id<MTLTexture> pos, id<MTLTexture> nrm, id<MTLTexture> hdr, unsigned long frame)
{
    static id<MTLComputePipelineState> pipe;
    static id<MTLTexture> dst;
    static uint64_t tot[56];
    static int fail;
    id<MTLComputeCommandEncoder> e;
    id<MTLBlitCommandEncoder> b;
    id<MTLBuffer> cnt;
    if (fail || !rt || !rt->built || !rt->as || !hdr)
        return;
    if (!pipe) {
        NSError *err = nil;
        id<MTLLibrary> lib = brr_rt_library(dev, @BRR_LU_SRC BRR_SUNCHECK_SRC, &err);
        id<MTLFunction> f = lib ? [lib newFunctionWithName:@"svck"] : nil;
        pipe = f ? [dev newComputePipelineStateWithFunction:f error:&err] : nil;
        if (!pipe) {
            fprintf(stderr, "suncheck: %s\n", err ? [[err description] UTF8String] : "no pipeline");
            fail = 1;
            return;
        }
    }
    if (!dst || dst.width != hdr.width || dst.height != hdr.height) {
        MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:hdr.pixelFormat width:hdr.width
                                                                                     height:hdr.height mipmapped:NO];
        td.usage = MTLTextureUsageShaderWrite | MTLTextureUsageShaderRead;
        td.storageMode = MTLStorageModePrivate;
        dst = [dev newTextureWithDescriptor:td];
    }
    cnt = [dev newBufferWithLength:56 * sizeof(uint32_t) options:MTLResourceStorageModeShared];
    memset([cnt contents], 0, 56 * sizeof(uint32_t));
    e = [cb computeCommandEncoder];
    [e setComputePipelineState:pipe];
    [e setBytes:lu length:lusz atIndex:3];
    [e setAccelerationStructure:rt->as atBufferIndex:8];
    [e setTexture:pos atIndex:1];
    [e setTexture:nrm atIndex:2];
    [e setTexture:hdr atIndex:3];
    [e setTexture:dst atIndex:4];
    [e setBuffer:cnt offset:0 atIndex:13];
    [e dispatchThreads:MTLSizeMake(hdr.width, hdr.height, 1) threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
    [e endEncoding];
    b = [cb blitCommandEncoder];
    [b copyFromTexture:dst toTexture:hdr];
    [b endEncoding];
    [cb addCompletedHandler:^(id<MTLCommandBuffer> c) {
        static const char *band[7] = { "0-10", "10-25", "25-55", "55-110", "110-200", "200-300", "300+" };
        const uint32_t *v = (const uint32_t *)[cnt contents];
        int k;
        (void)c;
        for (k = 0; k < 56; k++)
            tot[k] += v[k];
        if (frame % 30 == 0)
            for (k = 0; k < 7; k++) {
                const uint64_t *t = &tot[8 * k];
                if (!t[1])
                    continue;
                fprintf(stderr, "suncheck: so far [%s m] %llu px, traced shadow %llu, missing %.1f%% (hit within 5 cm %llu, 25 cm %llu, 2 m %llu, farther %llu), extra %.1f%%\n",
                        band[k], (unsigned long long)t[0], (unsigned long long)t[1], 100.0 * (double)t[2] / (double)t[1],
                        (unsigned long long)t[4], (unsigned long long)t[5], (unsigned long long)t[6], (unsigned long long)t[7], 100.0 * (double)t[3] / (double)t[0]);
            }
    }];
}
