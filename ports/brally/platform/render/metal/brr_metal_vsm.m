/* brr_metal_vsm.m: the sun's virtual shadow map on the GPU (brr_vsm.h: the
 * map, its kernels and lookup).  Once a track (or a new sun): the sun's
 * axes, the depth range, the light-space grid of the track's triangles and
 * the track map; the page table emptied and every pool slot freed.  Each
 * frame: the windows placed about the camera, then the marking, the
 * release, the allocation, the binning, the pages drawn into the scratch and
 * copied into the pool, and the moving casters' dynamic pages drawn.
 *
 *   BR_LIGHT_VSMLOG=1   once a second: pages drawn, deferred, refused (the
 *                       pool full), free, dynamic pages, binned triangles */
#import <Metal/Metal.h>
#include <TargetConditionals.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "brr_metal_vsm_api.h"

#if TARGET_OS_IPHONE
#define POOL_ROWS VSM_PY_SMALL
#else
#define POOL_ROWS VSM_PY
#endif
#define POOL_SLOTS (VSM_PX * POOL_ROWS)
#define DYN_SLOTS (VSM_DYNX * VSM_DYNX)
#define SCR_SLOTS (VSM_SCRX * VSM_SCRY)

typedef struct { uint32_t nrend, nwant, nfail, nfree, ndyn, nbin, pad0, pad1; uint32_t atrk[4], adyn[4], afloor[4]; uint32_t dlev[8], pad2[4]; uint32_t acopy[4]; } VsmStats;   /* VSS */
typedef struct { float lo[2]; int32_t nx, ny; } VsmGrid;   /* VSG */

static int s_fail;
static id<MTLComputePipelineState> s_pmark, s_prel, s_palloc, s_pbin, s_pbinargs, s_pcopy, s_pdynmark, s_pdynargs;
static id<MTLRenderPipelineState> s_ptrk, s_pdyn, s_ptmap;
static id<MTLDepthStencilState> s_ds;
static id<MTLBuffer> s_ent, s_tag, s_need, s_dyn, s_fst, s_fcnt, s_rl, s_st[3], s_bin, s_used, s_dl, s_dc;
static id<MTLBuffer> s_gstart, s_glist, s_gtb;
static id<MTLTexture> s_pool, s_scr, s_dpool, s_tmap;
static VsmU s_u;
static VsmGrid s_g;
static unsigned s_gen = 0xFFFFFFFFu, s_fr;
static float s_sun[3];
static int s_ready, s_log = -1, s_slot;

static id<MTLComputePipelineState> kern(id<MTLDevice> dev, id<MTLLibrary> lib, NSString *name)
{
    NSError *err = nil;
    id<MTLFunction> f = [lib newFunctionWithName:name];
    id<MTLComputePipelineState> p = f ? [dev newComputePipelineStateWithFunction:f error:&err] : nil;
    if (!p) {
        fprintf(stderr, "brr: shadow map kernel %s: %s\n", [name UTF8String], err ? [[err description] UTF8String] : "missing");
        s_fail = 1;
    }
    return p;
}

static id<MTLRenderPipelineState> depthpipe(id<MTLDevice> dev, id<MTLLibrary> lib, NSString *vs)
{
    MTLRenderPipelineDescriptor *d = [MTLRenderPipelineDescriptor new];
    NSError *err = nil;
    id<MTLRenderPipelineState> p;
    d.vertexFunction = [lib newFunctionWithName:vs];
    d.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
    p = d.vertexFunction ? [dev newRenderPipelineStateWithDescriptor:d error:&err] : nil;
    if (!p) {
        fprintf(stderr, "brr: shadow map pipeline %s: %s\n", [vs UTF8String], err ? [[err description] UTF8String] : "missing");
        s_fail = 1;
    }
    return p;
}

static id<MTLTexture> tex(id<MTLDevice> dev, MTLPixelFormat f, int w, int h, MTLTextureUsage us)
{
    MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:f width:(NSUInteger)w height:(NSUInteger)h mipmapped:NO];
    td.usage = us;
    td.storageMode = MTLStorageModePrivate;
    return [dev newTextureWithDescriptor:td];
}

static int init(id<MTLDevice> dev, id<MTLLibrary> lib)
{
    MTLDepthStencilDescriptor *dd;
    int k;
    if (s_pmark || s_fail)
        return !s_fail;
    s_pmark = kern(dev, lib, @"vs_mark");
    s_prel = kern(dev, lib, @"vs_release");
    s_palloc = kern(dev, lib, @"vs_alloc");
    s_pbin = kern(dev, lib, @"vs_bin");
    s_pbinargs = kern(dev, lib, @"vs_binargs");
    s_pcopy = kern(dev, lib, @"vs_copy");
    s_pdynmark = kern(dev, lib, @"vs_dynmark");
    s_pdynargs = kern(dev, lib, @"vs_dynargs");
    s_ptrk = depthpipe(dev, lib, @"vs_trk");
    s_pdyn = depthpipe(dev, lib, @"vs_dyn");
    s_ptmap = depthpipe(dev, lib, @"svs");
    if (s_fail)
        return 0;
    if (s_palloc.maxTotalThreadsPerThreadgroup < 1024) {
        fprintf(stderr, "brr: shadow map: the allocation needs 1024 threads a group, the device gives %lu\n", (unsigned long)s_palloc.maxTotalThreadsPerThreadgroup);
        s_fail = 1;
        return 0;
    }
    dd = [MTLDepthStencilDescriptor new];
    dd.depthCompareFunction = MTLCompareFunctionLessEqual;
    dd.depthWriteEnabled = YES;
    s_ds = [dev newDepthStencilStateWithDescriptor:dd];
    s_ent = [dev newBufferWithLength:VSM_ENTRIES * 4 options:MTLResourceStorageModeShared];
    s_tag = [dev newBufferWithLength:VSM_ENTRIES * 4 options:MTLResourceStorageModeShared];
    s_need = [dev newBufferWithLength:VSM_ENTRIES * 4 options:MTLResourceStorageModeShared];
    s_dyn = [dev newBufferWithLength:VSM_ENTRIES * 4 options:MTLResourceStorageModeShared];
    s_fst = [dev newBufferWithLength:POOL_SLOTS * 4 options:MTLResourceStorageModeShared];
    s_fcnt = [dev newBufferWithLength:16 options:MTLResourceStorageModeShared];
    s_rl = [dev newBufferWithLength:SCR_SLOTS * 4 options:MTLResourceStorageModePrivate];
    for (k = 0; k < 3; k++)
        s_st[k] = [dev newBufferWithLength:sizeof(VsmStats) options:MTLResourceStorageModeShared];
    s_bin = [dev newBufferWithLength:(NSUInteger)VSM_BINCAP * 4 options:MTLResourceStorageModePrivate];
    s_used = [dev newBufferWithLength:16 options:MTLResourceStorageModePrivate];
    s_dl = [dev newBufferWithLength:DYN_SLOTS * 4 options:MTLResourceStorageModePrivate];
    s_dc = [dev newBufferWithLength:64 options:MTLResourceStorageModeShared];
    s_pool = tex(dev, MTLPixelFormatR32Float, VSM_PX * VSM_SLOT, POOL_ROWS * VSM_SLOT, MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite);
    s_scr = tex(dev, MTLPixelFormatDepth32Float, VSM_SCRX * VSM_SLOT, VSM_SCRY * VSM_SLOT, MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead);
    s_dpool = tex(dev, MTLPixelFormatDepth32Float, VSM_DYNX * VSM_SLOT, VSM_DYNX * VSM_SLOT, MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead);
    s_tmap = tex(dev, MTLPixelFormatDepth32Float, VSM_TMAP, VSM_TMAP, MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead);
    if (!s_ent || !s_tag || !s_need || !s_dyn || !s_fst || !s_fcnt || !s_rl || !s_bin || !s_used || !s_dl || !s_dc
        || !s_pool || !s_scr || !s_dpool || !s_tmap) {
        fprintf(stderr, "brr: shadow map: out of memory\n");
        s_fail = 1;
        return 0;
    }
    return 1;
}

/* the sun's axes: f away from the sun, r across it level with the ground
 * (any horizontal axis with the sun overhead), v = r x f (brr_shadow.h
 * brr_cascade_fit's) */
static void axes(const float sun[3], float r[3], float v[3], float f[3])
{
    float len;
    int k;
    for (k = 0; k < 3; k++)
        f[k] = -sun[k];
    r[0] = f[1];
    r[1] = -f[0];
    r[2] = 0.0f;
    len = sqrtf(r[0] * r[0] + r[1] * r[1]);
    if (len < 1e-4f) {
        r[0] = 1.0f; r[1] = 0.0f; len = 1.0f;
    }
    r[0] /= len;
    r[1] /= len;
    v[0] = r[1] * f[2] - r[2] * f[1];
    v[1] = r[2] * f[0] - r[0] * f[2];
    v[2] = r[0] * f[1] - r[1] * f[0];
}

/* once a track or sun: the axes, the depth range, the grid, the table and
 * the free stack; the track map drawn into cb */
static int track(id<MTLDevice> dev, id<MTLCommandBuffer> cb, const BrrVsmFrame *in)
{
    const LightTrack *t = in->trk;
    float r[3], v[3], f[3], lo[3] = { 1e30f, 1e30f, 1e30f }, hi[3] = { -1e30f, -1e30f, -1e30f }, *tb;
    int i, k, n = t->ntri, *cnt, *start, *list, nc, total;
    axes(in->sun, r, v, f);
    tb = malloc((size_t)n * 4 * sizeof *tb);
    if (!tb)
        return 0;
    for (i = 0; i < n; i++) {
        float b[4] = { 1e30f, 1e30f, -1e30f, -1e30f };
        for (k = 0; k < 3; k++) {
            const float *p = &t->tri[9 * i + 3 * k];
            float X = p[0] * r[0] + p[1] * r[1] + p[2] * r[2], Y = p[0] * v[0] + p[1] * v[1] + p[2] * v[2], Z = p[0] * f[0] + p[1] * f[1] + p[2] * f[2];
            b[0] = fminf(b[0], X); b[1] = fminf(b[1], Y); b[2] = fmaxf(b[2], X); b[3] = fmaxf(b[3], Y);
            lo[2] = fminf(lo[2], Z); hi[2] = fmaxf(hi[2], Z);
        }
        memcpy(&tb[4 * i], b, sizeof b);
        lo[0] = fminf(lo[0], b[0]); lo[1] = fminf(lo[1], b[1]); hi[0] = fmaxf(hi[0], b[2]); hi[1] = fmaxf(hi[1], b[3]);
    }
    /* the grid: each triangle in every cell its box meets */
    s_g.lo[0] = lo[0] - 1.0f;
    s_g.lo[1] = lo[1] - 1.0f;
    s_g.nx = (int)((hi[0] - s_g.lo[0]) / VSM_CELL) + 2;
    s_g.ny = (int)((hi[1] - s_g.lo[1]) / VSM_CELL) + 2;
    nc = s_g.nx * s_g.ny;
    cnt = calloc((size_t)nc + 1, sizeof *cnt);
    start = malloc(((size_t)nc + 1) * sizeof *start);
    if (!cnt || !start) {
        free(tb); free(cnt); free(start);
        return 0;
    }
    for (total = 0, i = 0; i < n; i++) {
        int x0 = (int)((tb[4 * i] - s_g.lo[0]) / VSM_CELL), x1 = (int)((tb[4 * i + 2] - s_g.lo[0]) / VSM_CELL);
        int y0 = (int)((tb[4 * i + 1] - s_g.lo[1]) / VSM_CELL), y1 = (int)((tb[4 * i + 3] - s_g.lo[1]) / VSM_CELL), x, y;
        for (y = y0; y <= y1; y++)
            for (x = x0; x <= x1; x++)
                cnt[y * s_g.nx + x]++, total++;
    }
    list = malloc((size_t)(total ? total : 1) * sizeof *list);
    if (!list) {
        free(tb); free(cnt); free(start);
        return 0;
    }
    start[0] = 0;
    for (i = 0; i < nc; i++)
        start[i + 1] = start[i] + cnt[i];
    memset(cnt, 0, (size_t)nc * sizeof *cnt);
    for (i = 0; i < n; i++) {
        int x0 = (int)((tb[4 * i] - s_g.lo[0]) / VSM_CELL), x1 = (int)((tb[4 * i + 2] - s_g.lo[0]) / VSM_CELL);
        int y0 = (int)((tb[4 * i + 1] - s_g.lo[1]) / VSM_CELL), y1 = (int)((tb[4 * i + 3] - s_g.lo[1]) / VSM_CELL), x, y;
        for (y = y0; y <= y1; y++)
            for (x = x0; x <= x1; x++) {
                int c = y * s_g.nx + x;
                list[start[c] + cnt[c]++] = i;
            }
    }
    s_gstart = [dev newBufferWithBytes:start length:((size_t)nc + 1) * sizeof *start options:MTLResourceStorageModeShared];
    s_glist = [dev newBufferWithBytes:list length:(size_t)(total ? total : 1) * sizeof *list options:MTLResourceStorageModeShared];
    s_gtb = [dev newBufferWithBytes:tb length:(size_t)n * 4 * sizeof *tb options:MTLResourceStorageModeShared];
    free(tb); free(cnt); free(start); free(list);
    if (!s_gstart || !s_glist || !s_gtb)
        return 0;
    /* the constants that hold for the track */
    memset(&s_u, 0, sizeof s_u);
    for (k = 0; k < 3; k++) {
        s_u.r[k] = r[k];
        s_u.v[k] = v[k];
        s_u.f[k] = f[k];
    }
    lo[2] -= 20.0f;   /* room for what moves over the track (a car on a crest, the start lights) */
    hi[2] += 20.0f;
    s_u.z[0] = lo[2];
    s_u.z[1] = 1.0f / (hi[2] - lo[2]);
    s_u.z[2] = hi[2] - lo[2];
    s_u.z[3] = VSM_T0;
    {
        float side = fmaxf(hi[0] - lo[0], hi[1] - lo[1]) + 4.0f;
        s_u.tm[0] = lo[0] - 2.0f;
        s_u.tm[1] = lo[1] - 2.0f;
        s_u.tm[2] = side;
        s_u.tm[3] = side / (float)VSM_TMAP;
    }
    /* the table empty, every slot free */
    {
        uint32_t *e = [s_ent contents], *g = [s_tag contents], *nd = [s_need contents], *d = [s_dyn contents], *fs = [s_fst contents];
        int32_t *fc = [s_fcnt contents];
        for (i = 0; i < VSM_ENTRIES; i++) {
            e[i] = VSM_NONE;
            g[i] = 0;
            nd[i] = 0;
            d[i] = 0xFFFFFFFFu;
        }
        for (i = 0; i < POOL_SLOTS; i++)
            fs[i] = (uint32_t)i;
        fc[0] = POOL_SLOTS;
    }
    /* the track map: every triangle, ortho over the track (svs: clip = m x p, m column-major) */
    {
        float m[16] = { 0 }, s2 = 2.0f / s_u.tm[2];
        MTLRenderPassDescriptor *rp = [MTLRenderPassDescriptor renderPassDescriptor];
        id<MTLRenderCommandEncoder> e;
        for (k = 0; k < 3; k++) {
            m[4 * k + 0] = r[k] * s2;
            m[4 * k + 1] = -v[k] * s2;
            m[4 * k + 2] = f[k] * s_u.z[1];
        }
        m[12] = -s_u.tm[0] * s2 - 1.0f;
        m[13] = s_u.tm[1] * s2 + 1.0f;
        m[14] = -s_u.z[0] * s_u.z[1];
        m[15] = 1.0f;
        rp.depthAttachment.texture = s_tmap;
        rp.depthAttachment.loadAction = MTLLoadActionClear;
        rp.depthAttachment.clearDepth = 1.0;
        rp.depthAttachment.storeAction = MTLStoreActionStore;
        e = [cb renderCommandEncoderWithDescriptor:rp];
        [e setRenderPipelineState:s_ptmap];
        [e setDepthStencilState:s_ds];
        [e setCullMode:MTLCullModeNone];
        [e setVertexBuffer:in->trkbuf offset:0 atIndex:0];
        [e setVertexBytes:m length:sizeof m atIndex:1];
        [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:(NSUInteger)n * 3];
        [e endEncoding];
    }
    s_gen = t->gen;
    memcpy(s_sun, in->sun, sizeof s_sun);
    return 1;
}

/* BR_LIGHT_VSMTIME=1 (diagnostic): each pass's GPU time from timestamps at
 * its encoder's start and end (the stage boundaries Apple GPUs sample),
 * averaged over 60 frames */
#define TM_PASSES 8
static const char *const k_tm_names[TM_PASSES] = { "mark", "release", "alloc", "bin", "dynamic mark", "draw", "copy", "dynamic draw" };
static int s_tm = -1;
static id<MTLCounterSampleBuffer> s_tmbuf[3];
static int s_tmslot;
static double s_tmsum[TM_PASSES];   /* GPU ticks */
static unsigned s_tmn;
static MTLTimestamp s_tmc0, s_tmg0;   /* the CPU's and the GPU's clocks together, for ticks to nanoseconds */
static id<MTLDevice> s_tmdev;
static void tm_begin(id<MTLDevice> dev)
{
    if (s_tm < 0) {
        s_tm = getenv("BR_LIGHT_VSMTIME") != NULL && [dev supportsCounterSampling:MTLCounterSamplingPointAtStageBoundary];
        if (s_tm) {
            MTLCounterSampleBufferDescriptor *d = [MTLCounterSampleBufferDescriptor new];
            int k;
            for (id<MTLCounterSet> cs in dev.counterSets)
                if ([cs.name isEqualToString:MTLCommonCounterSetTimestamp])
                    d.counterSet = cs;
            d.sampleCount = 2 * TM_PASSES;
            d.storageMode = MTLStorageModeShared;
            for (k = 0; k < 3 && d.counterSet; k++)
                s_tmbuf[k] = [dev newCounterSampleBufferWithDescriptor:d error:nil];
            if (!s_tmbuf[2])
                s_tm = 0;
            else {
                s_tmdev = dev;
                [dev sampleTimestamps:&s_tmc0 gpuTimestamp:&s_tmg0];
            }
        }
    }
    s_tmslot = (s_tmslot + 1) % 3;
}
static id<MTLComputeCommandEncoder> tm_compute(id<MTLCommandBuffer> cb, int k)
{
    MTLComputePassDescriptor *d = [MTLComputePassDescriptor computePassDescriptor];
    if (s_tm > 0) {
        d.sampleBufferAttachments[0].sampleBuffer = s_tmbuf[s_tmslot];
        d.sampleBufferAttachments[0].startOfEncoderSampleIndex = (NSUInteger)(2 * k);
        d.sampleBufferAttachments[0].endOfEncoderSampleIndex = (NSUInteger)(2 * k + 1);
    }
    return [cb computeCommandEncoderWithDescriptor:d];
}
static id<MTLRenderCommandEncoder> tm_render(id<MTLCommandBuffer> cb, MTLRenderPassDescriptor *rp, int k)
{
    if (s_tm > 0) {
        rp.sampleBufferAttachments[0].sampleBuffer = s_tmbuf[s_tmslot];
        rp.sampleBufferAttachments[0].startOfVertexSampleIndex = (NSUInteger)(2 * k);
        rp.sampleBufferAttachments[0].endOfVertexSampleIndex = MTLCounterDontSample;
        rp.sampleBufferAttachments[0].startOfFragmentSampleIndex = MTLCounterDontSample;
        rp.sampleBufferAttachments[0].endOfFragmentSampleIndex = (NSUInteger)(2 * k + 1);
    }
    return [cb renderCommandEncoderWithDescriptor:rp];
}
static void tm_end(id<MTLCommandBuffer> cb)
{
    id<MTLCounterSampleBuffer> b;
    if (s_tm <= 0)
        return;
    b = s_tmbuf[s_tmslot];
    [cb addCompletedHandler:^(id<MTLCommandBuffer> c) {
        NSData *d = [b resolveCounterRange:NSMakeRange(0, 2 * TM_PASSES)];
        const MTLCounterResultTimestamp *t = d ? (const MTLCounterResultTimestamp *)d.bytes : NULL;
        int k;
        (void)c;
        if (!t)
            return;
        for (k = 0; k < TM_PASSES; k++)
            if (t[2 * k + 1].timestamp > t[2 * k].timestamp && t[2 * k].timestamp != MTLCounterErrorValue)
                s_tmsum[k] += (double)(t[2 * k + 1].timestamp - t[2 * k].timestamp);
        if (++s_tmn == 60) {
            MTLTimestamp c1, g1;
            double ns;
            [s_tmdev sampleTimestamps:&c1 gpuTimestamp:&g1];
            ns = g1 > s_tmg0 ? (double)(c1 - s_tmc0) / (double)(g1 - s_tmg0) : 1.0;
            fprintf(stderr, "vsmtime (ms a frame):");
            for (k = 0; k < TM_PASSES; k++) {
                fprintf(stderr, " %s %.3f", k_tm_names[k], s_tmsum[k] * ns / 60.0 / 1e6);
                s_tmsum[k] = 0.0;
            }
            fprintf(stderr, "\n");
            s_tmn = 0;
        }
    }];
}

int brr_vsm_frame(id<MTLDevice> dev, id<MTLLibrary> lib, id<MTLCommandBuffer> cb, const BrrVsmFrame *in)
{
    id<MTLComputeCommandEncoder> ce;
    id<MTLRenderCommandEncoder> re;
    id<MTLBlitCommandEncoder> be;
    MTLRenderPassDescriptor *rp;
    id<MTLBuffer> st;
    int L, k;
    s_ready = 0;
    if (!in->trk || !in->trk->ntri || !in->trkbuf || !in->gpos || !in->gnrm || !init(dev, lib))
        return 0;
    if (s_gen != in->trk->gen || memcmp(s_sun, in->sun, sizeof s_sun) != 0)
        if (!track(dev, cb, in))
            return 0;
    /* this frame's constants: the windows about the camera */
    s_fr++;
    for (k = 0; k < 3; k++)
        s_u.eye[k] = in->eye[k];
    s_u.eye[3] = in->pixang;
    {
        float X = in->eye[0] * s_u.r[0] + in->eye[1] * s_u.r[1] + in->eye[2] * s_u.r[2];
        float Y = in->eye[0] * s_u.v[0] + in->eye[1] * s_u.v[1] + in->eye[2] * s_u.v[2];
        for (L = 0; L < VSM_LEVELS; L++) {
            float pw = (float)VSM_PAGE * VSM_T0 * (float)(1 << L);
            s_u.org[2 * L] = (int32_t)floorf(X / pw) - VSM_TAB / 2;
            s_u.org[2 * L + 1] = (int32_t)floorf(Y / pw) - VSM_TAB / 2;
        }
    }
    s_u.lod[0] = VSM_LOD;
    s_u.lod[1] = VSM_BAND;
    s_u.lod[2] = (float)(VSM_SCRX * VSM_SLOT);
    s_u.lod[3] = (float)(VSM_SCRY * VSM_SLOT);
    s_u.dsz[0] = s_u.dsz[1] = (float)(VSM_DYNX * VSM_SLOT);
    s_u.dsz[2] = (float)(VSM_PX * VSM_SLOT);
    s_u.dsz[3] = (float)(POOL_ROWS * VSM_SLOT);
    s_u.cnt[0] = s_fr;
    s_u.cnt[1] = SCR_SLOTS;
    s_u.cnt[2] = DYN_SLOTS;
    s_u.cnt[3] = 0;
    s_u.pool[0] = VSM_PX;
    s_u.pool[1] = POOL_ROWS;
    s_u.pool[2] = VSM_DYNX;
    s_u.pool[3] = VSM_SCRX;
    s_u.on[0] = 1.0f;
    s_slot = (s_slot + 1) % 3;
    st = s_st[s_slot];
    tm_begin(dev);
    be = [cb blitCommandEncoder];
    [be fillBuffer:s_used range:NSMakeRange(0, 16) value:0];
    [be fillBuffer:s_dc range:NSMakeRange(0, 64) value:0];
    [be endEncoding];
    /* marking */
    ce = tm_compute(cb, 0);
    [ce setComputePipelineState:s_pmark];
    [ce setBytes:&s_u length:sizeof s_u atIndex:0];
    [ce setBytes:in->sbias length:16 atIndex:1];
    [ce setBuffer:s_need offset:0 atIndex:2];
    [ce setTexture:in->gpos atIndex:0];
    [ce setTexture:in->gnrm atIndex:1];
    [ce dispatchThreads:MTLSizeMake(in->gpos.width, in->gpos.height, 1) threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
    [ce endEncoding];
    /* release, allocation */
    for (k = 1; k <= 2; k++) {
        ce = tm_compute(cb, k);
        [ce setBytes:&s_u length:sizeof s_u atIndex:0];
        [ce setBuffer:s_ent offset:0 atIndex:1];
        [ce setBuffer:s_tag offset:0 atIndex:2];
        [ce setBuffer:s_need offset:0 atIndex:3];
        [ce setBuffer:s_dyn offset:0 atIndex:4];
        [ce setBuffer:s_fcnt offset:0 atIndex:5];
        [ce setBuffer:s_fst offset:0 atIndex:6];
        [ce setBuffer:s_rl offset:0 atIndex:7];
        [ce setBuffer:st offset:0 atIndex:8];
        if (k == 1) {
            [ce setComputePipelineState:s_prel];
            [ce dispatchThreads:MTLSizeMake(VSM_ENTRIES, 1, 1) threadsPerThreadgroup:MTLSizeMake(256, 1, 1)];
        } else {
            [ce setComputePipelineState:s_palloc];
            [ce dispatchThreadgroups:MTLSizeMake(1, 1, 1) threadsPerThreadgroup:MTLSizeMake(1024, 1, 1)];
        }
        [ce endEncoding];
    }
    /* the pages' triangles */
    ce = tm_compute(cb, 3);
    [ce setComputePipelineState:s_pbin];
    [ce setBytes:&s_u length:sizeof s_u atIndex:0];
    [ce setBuffer:s_gstart offset:0 atIndex:1];
    [ce setBuffer:s_glist offset:0 atIndex:2];
    [ce setBuffer:s_gtb offset:0 atIndex:3];
    [ce setBytes:&s_g length:sizeof s_g atIndex:4];
    [ce setBuffer:s_used offset:0 atIndex:5];
    [ce setBuffer:s_bin offset:0 atIndex:6];
    [ce setBuffer:s_rl offset:0 atIndex:7];
    [ce setBuffer:st offset:0 atIndex:8];
    [ce dispatchThreadgroups:MTLSizeMake(SCR_SLOTS, 1, 1) threadsPerThreadgroup:MTLSizeMake(128, 1, 1)];
    [ce setComputePipelineState:s_pbinargs];
    [ce dispatchThreads:MTLSizeMake(1, 1, 1) threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
    [ce endEncoding];
    /* the moving casters' pages */
    {
        uint32_t kk[4] = { DYN_SLOTS, in->movercap, in->nfloor, 0 };
        ce = tm_compute(cb, 4);
        [ce setBytes:&s_u length:sizeof s_u atIndex:0];
        [ce setBuffer:st offset:0 atIndex:8];
        if (in->movers && in->moverargs) {
            [ce setComputePipelineState:s_pdynmark];
            [ce setBuffer:in->movers offset:0 atIndex:1];
            [ce setBuffer:s_need offset:0 atIndex:3];
            [ce setBuffer:s_dyn offset:0 atIndex:4];
            [ce setBuffer:s_dc offset:0 atIndex:5];
            [ce setBuffer:s_dl offset:0 atIndex:6];
            [ce setBuffer:in->moverargs offset:0 atIndex:9];
            [ce setBytes:kk length:sizeof kk atIndex:10];
            [ce dispatchThreads:MTLSizeMake((NSUInteger)in->movercap / 3 * VSM_LEVELS, 1, 1) threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];
        } else
            kk[1] = 0;
        [ce setComputePipelineState:s_pdynargs];
        [ce setBuffer:s_dc offset:0 atIndex:5];
        [ce setBuffer:in->moverargs ? in->moverargs : s_dc offset:0 atIndex:9];
        [ce setBytes:kk length:sizeof kk atIndex:10];
        [ce dispatchThreads:MTLSizeMake(1, 1, 1) threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
        [ce endEncoding];
    }
    /* the pages drawn into the scratch */
    rp = [MTLRenderPassDescriptor renderPassDescriptor];
    rp.depthAttachment.texture = s_scr;
    rp.depthAttachment.loadAction = MTLLoadActionClear;
    rp.depthAttachment.clearDepth = 1.0;
    rp.depthAttachment.storeAction = MTLStoreActionStore;
    re = tm_render(cb, rp, 5);
    [re setRenderPipelineState:s_ptrk];
    [re setDepthStencilState:s_ds];
    [re setCullMode:MTLCullModeNone];
    [re setVertexBuffer:s_bin offset:0 atIndex:0];
    [re setVertexBuffer:in->trkbuf offset:0 atIndex:1];
    [re setVertexBytes:&s_u length:sizeof s_u atIndex:2];
    [re setVertexBuffer:s_rl offset:0 atIndex:3];
    [re drawPrimitives:MTLPrimitiveTypeTriangle indirectBuffer:st indirectBufferOffset:offsetof(VsmStats, atrk)];
    [re endEncoding];
    /* into the pool */
    ce = tm_compute(cb, 6);
    [ce setComputePipelineState:s_pcopy];
    [ce setBytes:&s_u length:sizeof s_u atIndex:0];
    [ce setBuffer:s_ent offset:0 atIndex:1];
    [ce setBuffer:s_rl offset:0 atIndex:7];
    [ce setBuffer:st offset:0 atIndex:8];
    [ce setTexture:s_scr atIndex:0];
    [ce setTexture:s_pool atIndex:1];
    [ce dispatchThreadgroupsWithIndirectBuffer:st indirectBufferOffset:offsetof(VsmStats, acopy) threadsPerThreadgroup:MTLSizeMake(16, 16, 1)];   /* the pages drawn alone (vs_binargs) */
    [ce endEncoding];
    /* the moving casters' pages */
    rp = [MTLRenderPassDescriptor renderPassDescriptor];
    rp.depthAttachment.texture = s_dpool;
    rp.depthAttachment.loadAction = MTLLoadActionClear;
    rp.depthAttachment.clearDepth = 1.0;
    rp.depthAttachment.storeAction = MTLStoreActionStore;
    re = tm_render(cb, rp, 7);
    [re setRenderPipelineState:s_pdyn];
    [re setDepthStencilState:s_ds];
    [re setCullMode:MTLCullModeNone];
    [re setVertexBytes:&s_u length:sizeof s_u atIndex:2];
    [re setVertexBuffer:s_dl offset:0 atIndex:3];
    if (in->movers && in->moverargs) {
        [re setVertexBuffer:in->movers offset:0 atIndex:0];
        [re drawPrimitives:MTLPrimitiveTypeTriangle indirectBuffer:st indirectBufferOffset:offsetof(VsmStats, adyn)];
    }
    if (in->floors && in->nfloor) {
        [re setVertexBuffer:in->floors offset:0 atIndex:0];
        [re drawPrimitives:MTLPrimitiveTypeTriangle indirectBuffer:st indirectBufferOffset:offsetof(VsmStats, afloor)];
    }
    [re endEncoding];
    tm_end(cb);
    if (s_log < 0)
        s_log = getenv("BR_LIGHT_VSMLOG") != NULL;
    if (s_log) {   /* every frame's counts; the worst of each over the last 60 frames printed */
        unsigned fr = s_fr;
        [cb addCompletedHandler:^(id<MTLCommandBuffer> c) {
            static uint32_t mr, mw, mf, mfree = 0xFFFFFFFFu, md, mb;
            const VsmStats *v = (const VsmStats *)[st contents];
            (void)c;
            mr = v->nrend > mr ? v->nrend : mr;
            mw = v->nwant > mw ? v->nwant : mw;
            mf = v->nfail > mf ? v->nfail : mf;
            mfree = v->nfree < mfree ? v->nfree : mfree;
            md = v->ndyn > md ? v->ndyn : md;
            mb = v->nbin > mb ? v->nbin : mb;
            if (fr % 60 == 0) {
                fprintf(stderr, "vsm: frames to %u, the most in one: %u pages drawn, %u deferred, %u refused (pool full), %u dynamic pages (of %d), %u triangles binned; "
                        "fewest free %u of %d; this frame's dynamic pages by level %u %u %u %u %u %u %u %u\n", fr, mr, mw, mf, md, DYN_SLOTS, mb, mfree, POOL_SLOTS,
                        v->dlev[0], v->dlev[1], v->dlev[2], v->dlev[3], v->dlev[4], v->dlev[5], v->dlev[6], v->dlev[7]);
                mr = mw = mf = md = mb = 0;
                mfree = 0xFFFFFFFFu;
            }
        }];
    }
    s_ready = 1;
    return 1;
}

const VsmU *brr_vsm_u(void)
{
    return &s_u;
}

void brr_vsm_bind(id<MTLRenderCommandEncoder> e, int ub, int eb, int db, int pt, int dt, int tt)
{
    static VsmU off;
    [e setFragmentBytes:s_ready ? &s_u : &off length:sizeof s_u atIndex:(NSUInteger)ub];
    if (!s_ent)
        return;
    [e setFragmentBuffer:s_ent offset:0 atIndex:(NSUInteger)eb];
    [e setFragmentBuffer:s_dyn offset:0 atIndex:(NSUInteger)db];
    [e setFragmentTexture:s_pool atIndex:(NSUInteger)pt];
    [e setFragmentTexture:s_dpool atIndex:(NSUInteger)dt];
    [e setFragmentTexture:s_tmap atIndex:(NSUInteger)tt];
}

id<MTLTexture> brr_vsm_tmap(void)
{
    return s_tmap;
}
