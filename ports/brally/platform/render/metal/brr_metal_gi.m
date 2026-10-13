/* brr_metal_gi.m: the irradiance probes' update on the GPU (brr_metal_gi.h;
 * DESIGN.md 8.2): pipelines, the ping-pong atlases, the volume's placement
 * and each frame's ray rotation. */
#import <Metal/Metal.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "brr_metal_gi.h"
#include "brr_metal_lights.h"

void brr_gi_release(BrrGi *g)
{
    g->trace = g->birr = g->bdst = nil;
    g->irr[0] = g->irr[1] = g->dst[0] = g->dst[1] = nil;
    g->grid = nil;
    g->ray = g->leaf = g->gcnt = g->glst = nil;
    g->ready = 0;
}

static id<MTLTexture> atlas(id<MTLDevice> dev, int w, int h, MTLPixelFormat f)
{
    MTLTextureDescriptor *d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:f width:(NSUInteger)w
                                                                                height:(NSUInteger)h mipmapped:NO];
    d.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
    d.storageMode = MTLStorageModeShared;     /* unified memory: the tests read it back */
    return [dev newTextureWithDescriptor:d];
}

int brr_gi_init(BrrGi *g, id<MTLDevice> dev, int nx, int ny, int nz, int nr)
{
    NSError *err = nil;
    id<MTLLibrary> lib;
    int k;
    memset(g, 0, sizeof *g);
    lib = brr_rt_library(dev, @BRR_GI_DEFS BRR_LIGHTS_SRC BRR_GI_SRC, &err);
    if (!lib)
        return 0;
    g->trace = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"gi_trace"] error:&err];
    g->birr = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"gi_blend_irr"] error:&err];
    g->bdst = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"gi_blend_dst"] error:&err];
    g->grid = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"gi_grid"] error:&err];
    g->gcnt = [dev newBufferWithLength:BRR_GI_GRID * BRR_GI_GRID * BRR_GI_GRID * 4 options:MTLResourceStorageModeShared];
    g->glst = [dev newBufferWithLength:BRR_GI_GRID * BRR_GI_GRID * BRR_GI_GRID * 32 * 2 options:MTLResourceStorageModeShared];
    for (k = 0; k < 2; k++) {
        g->irr[k] = atlas(dev, nx * nz * (BRR_GI_IRR + 2), ny * (BRR_GI_IRR + 2), MTLPixelFormatRGBA32Float);
        g->dst[k] = atlas(dev, nx * nz * (BRR_GI_DST + 2), ny * (BRR_GI_DST + 2), MTLPixelFormatRG32Float);
    }
    g->ray = [dev newBufferWithLength:(size_t)nx * ny * nz * nr * 16 options:MTLResourceStorageModeShared];
    if (!g->trace || !g->birr || !g->bdst || !g->grid || !g->gcnt || !g->glst || !g->irr[0] || !g->irr[1] || !g->dst[0] || !g->dst[1] || !g->ray) {
        brr_gi_release(g);
        return 0;
    }
    g->nx = nx; g->ny = ny; g->nz = nz; g->nr = nr;
    g->bytes = 2 * ((size_t)nx * nz * (BRR_GI_IRR + 2) * ny * (BRR_GI_IRR + 2) * 16
                    + (size_t)nx * nz * (BRR_GI_DST + 2) * ny * (BRR_GI_DST + 2) * 8)
             + (size_t)nx * ny * nz * nr * 16
             + (size_t)BRR_GI_GRID * BRR_GI_GRID * BRR_GI_GRID * (4 + 32 * 2);
    g->u.n[0] = nx; g->u.n[1] = ny; g->u.n[2] = nz; g->u.n[3] = nr;
    g->u.par[0] = 0.97f; g->u.par[1] = 0.3f; g->u.par[2] = 0.0f; g->u.par[3] = 0.3f;
    g->u.lim[0] = 200.0f; g->u.lim[1] = 1.0f; g->u.lim[3] = 1.0f;
    g->u.upd[0] = 4;
    g->u.gdim[3] = BRR_GI_CELL;
    g->ready = 1;
    return 1;
}

/* xorshift32: the rotation's own stream (not the game's random numbers) */
static float rnd(uint32_t *s)
{
    *s ^= *s << 13; *s ^= *s >> 17; *s ^= *s << 5;
    return (float)(*s >> 8) * (1.0f / 16777216.0f);
}

/* a uniformly random rotation (Shoemake's unit quaternion) as rows */
static void rotation(BrrGiU *u, uint32_t frame)
{
    uint32_t s = frame * 2654435761u + 0x9e3779b9u;
    float a, b, c, x, y, z, w;
    if (!s)
        s = 1;
    a = rnd(&s); b = rnd(&s) * 6.2831853f; c = rnd(&s) * 6.2831853f;
    x = sqrtf(1.0f - a) * sinf(b); y = sqrtf(1.0f - a) * cosf(b);
    z = sqrtf(a) * sinf(c); w = sqrtf(a) * cosf(c);
    u->rot0[0] = 1 - 2 * (y * y + z * z); u->rot0[1] = 2 * (x * y - z * w); u->rot0[2] = 2 * (x * z + y * w);
    u->rot1[0] = 2 * (x * y + z * w); u->rot1[1] = 1 - 2 * (x * x + z * z); u->rot1[2] = 2 * (y * z - x * w);
    u->rot2[0] = 2 * (x * z - y * w); u->rot2[1] = 2 * (y * z + x * w); u->rot2[2] = 1 - 2 * (x * x + y * y);
}

void brr_gi_frame(BrrGi *g, const float eye[3], float sp, const float sun[3], const float sunc[3],
                  const float sky[3], const float gnd[3])
{
    BrrGiU *u = &g->u;
    int k;
    memcpy(u->pbase, u->base, sizeof u->base);
    u->org[3] = sp;
    for (k = 0; k < 3; k++) {
        u->base[k] = (int)floorf((eye[k] - u->org[k]) / sp) - g->u.n[k] / 2;
        u->sun[k] = sun[k]; u->sunc[k] = sunc[k]; u->sky[k] = sky[k]; u->gnd[k] = gnd[k];
    }
    u->base[3] = (int)g->frame;
    u->upd[1] = (int)(g->frame % (uint32_t)(u->upd[0] > 0 ? u->upd[0] : 1));
    rotation(u, g->frame++);
}

/* the track's triangle -> its slot in the BVH's leaf order (the trace reads
 * the corners for the hit's normal there) */
static void leaf_map(BrrGi *g, id<MTLDevice> dev, const BrrRtWorld *w)
{
    uint32_t *m = malloc((size_t)w->bvh.ntri * sizeof *m);
    int j;
    if (!m)
        return;
    for (j = 0; j < w->bvh.ntri; j++)
        m[w->bvh.id[j]] = (uint32_t)j;
    g->leaf = [dev newBufferWithBytes:m length:(size_t)w->bvh.ntri * sizeof *m options:MTLResourceStorageModeShared];
    free(m);
    g->gen = w->gen;
}

/* the light grid over the volume and the rays' reach: cubes of 16 m, larger
 * when that would take more than BRR_GI_GRID an axis */
static void grid_place(BrrGiU *u)
{
    float sp = u->org[3], R = u->lim[0], lo[3], ext = 0.0f, cs;
    int k;
    for (k = 0; k < 3; k++) {
        lo[k] = u->org[k] + u->base[k] * sp - R;
        ext = fmaxf(ext, (u->n[k] - 1) * sp + 2.0f * R);
    }
    cs = fmaxf(16.0f, ext / BRR_GI_GRID * 1.0001f);
    for (k = 0; k < 3; k++) {
        float e = (u->n[k] - 1) * sp + 2.0f * R;
        int d = (int)ceilf(e / cs);
        u->gorg[k] = lo[k];
        u->gdim[k] = d < 1 ? 1 : d > BRR_GI_GRID ? BRR_GI_GRID : d;
    }
    u->gorg[3] = cs;
}

void brr_gi_encode(BrrGi *g, id<MTLCommandBuffer> cb, const BrrRtWorld *w, id<MTLBuffer> lset, int nlamp)
{
    id<MTLComputeCommandEncoder> e;
    int prev = g->cur, next = g->cur ^ 1, np = g->nx * g->ny * g->nz;
    if (!g->ready || !w || !w->built)
        return;
    if (!g->leaf || g->gen != w->gen)
        leaf_map(g, cb.device, w);
    g->u.lim[2] = (float)(lset ? (nlamp < 128 ? nlamp : 128) : 0);
    grid_place(&g->u);
    e = [cb computeCommandEncoder];
    if (lset) {
        [e setComputePipelineState:g->grid];
        [e setBytes:&g->u length:sizeof g->u atIndex:0];
        [e setBuffer:lset offset:0 atIndex:1];
        [e setBuffer:g->gcnt offset:0 atIndex:2];
        [e setBuffer:g->glst offset:0 atIndex:3];
        [e dispatchThreads:MTLSizeMake((NSUInteger)(g->u.gdim[0] * g->u.gdim[1] * g->u.gdim[2]), 1, 1)
            threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];
    }
    [e setComputePipelineState:g->trace];
    [e setBytes:&g->u length:sizeof g->u atIndex:0];
    [e setBuffer:g->ray offset:0 atIndex:1];
    [e setBuffer:g->leaf offset:0 atIndex:2];
    [e setBuffer:lset offset:0 atIndex:3];
    brr_rt_bind(w, e, 4);
    [e setBuffer:g->gcnt offset:0 atIndex:9];
    [e setBuffer:g->glst offset:0 atIndex:10];
    [e setTexture:g->irr[prev] atIndex:0];
    [e setTexture:g->dst[prev] atIndex:1];
    [e dispatchThreads:MTLSizeMake((NSUInteger)g->nr, (NSUInteger)np, 1) threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];
    [e setComputePipelineState:g->birr];
    [e setTexture:g->irr[prev] atIndex:0];
    [e setTexture:g->irr[next] atIndex:1];
    [e dispatchThreads:MTLSizeMake(g->irr[next].width, g->irr[next].height, 1) threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
    [e setComputePipelineState:g->bdst];
    [e setTexture:g->dst[prev] atIndex:0];
    [e setTexture:g->dst[next] atIndex:1];
    [e dispatchThreads:MTLSizeMake(g->dst[next].width, g->dst[next].height, 1) threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
    [e endEncoding];
    g->u.pbase[3] = 1;
    g->cur = next;
}

id<MTLTexture> brr_gi_irr(const BrrGi *g) { return g->irr[g->cur]; }
id<MTLTexture> brr_gi_dst(const BrrGi *g) { return g->dst[g->cur]; }
