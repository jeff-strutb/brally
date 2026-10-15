/* brr_metal_gi.m: the irradiance probes baked on the GPU (brr_metal_gi.h;
 * DESIGN.md 8.2): pipelines, the probes' placement over the track, the bake's
 * passes and its ray rotations. */
#import <Metal/Metal.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "brr_metal_gi.h"
#include "brr_metal_lights.h"

void brr_gi_release(BrrGi *g)
{
    g->trace = g->bsh = g->bdst = nil;
    g->sh = nil;
    g->dst[0] = g->dst[1] = nil;
    g->grid = nil;
    g->ray = g->leaf = g->gcnt = g->glst = g->off = nil;
    g->offreset = g->reloc = nil;
    g->ready = g->baked = 0;
}

static id<MTLTexture> atlas(id<MTLDevice> dev, int w, int h, MTLPixelFormat f)
{
    MTLTextureDescriptor *d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:f width:(NSUInteger)w
                                                                                height:(NSUInteger)h mipmapped:NO];
    d.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
    d.storageMode = MTLStorageModeShared;     /* unified memory: the tests read it back */
    return [dev newTextureWithDescriptor:d];
}

int brr_gi_init(BrrGi *g, id<MTLDevice> dev, int nr)
{
    NSError *err = nil;
    id<MTLLibrary> lib;
    memset(g, 0, sizeof *g);
    lib = brr_rt_library(dev, @BRR_GI_DEFS BRR_LIGHTS_SRC BRR_GI_SRC, &err);
    if (!lib)
        return 0;
    g->trace = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"gi_trace"] error:&err];
    g->bsh = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"gi_blend_sh"] error:&err];
    g->bdst = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"gi_blend_dst"] error:&err];
    g->grid = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"gi_grid"] error:&err];
    g->offreset = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"gi_offreset"] error:&err];
    g->reloc = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"gi_reloc"] error:&err];
    g->gcnt = [dev newBufferWithLength:BRR_GI_GRID * BRR_GI_GRID * BRR_GI_GRID * 4 options:MTLResourceStorageModeShared];
    g->glst = [dev newBufferWithLength:BRR_GI_GRID * BRR_GI_GRID * BRR_GI_GRID * 32 * 2 options:MTLResourceStorageModeShared];
    {   /* a black cube for the trace's sky slot when there is no skymap */
        MTLTextureDescriptor *td = [MTLTextureDescriptor textureCubeDescriptorWithPixelFormat:MTLPixelFormatRGBA16Float size:1 mipmapped:NO];
        static const uint16_t z4[4];
        int f;
        g->nocube = [dev newTextureWithDescriptor:td];
        for (f = 0; f < 6 && g->nocube; f++)
            [g->nocube replaceRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0 slice:(NSUInteger)f withBytes:z4 bytesPerRow:8 bytesPerImage:8];
    }
    if (!g->nocube || !g->trace || !g->bsh || !g->bdst || !g->grid || !g->gcnt || !g->glst || !g->offreset || !g->reloc) {
        brr_gi_release(g);
        return 0;
    }
    g->nr = nr;
    g->u.n[3] = nr;
    g->u.par[1] = 0.3f; g->u.par[2] = 0.0f; g->u.par[3] = 0.3f;
    g->u.lim[0] = 200.0f; g->u.lim[1] = 1.0f; g->u.lim[3] = 1.0f;
    g->u.gdim[3] = BRR_GI_CELL;
    g->keep = BRR_GI_KEEP;
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

/* the probes' grid points: the corners of every cell a surface passes
 * through, and of the cells next to it (a lookup's point leaves its surface
 * by up to 0.75 x 0.3 of a spacing toward the eye and its normal, and a
 * probe moved out of the solid by up to 0.45 of one).  A triangle is walked
 * at a quarter of a spacing (its corners, edges and inside).  Returns the
 * probes; *grid the dense table (~0u none, then the probe's number), *pts
 * each probe's grid point */
static int gi_place(const float *tri, int ntri, const float org[3], float sp, const int n[3], uint32_t **grid, int32_t **pts)
{
    size_t nd = (size_t)n[0] * n[1] * n[2], k;
    uint8_t *m = calloc(nd, 1);
    uint32_t *gt;
    int32_t *pt;
    int np = 0, t;
    if (!m)
        return -1;
    for (t = 0; t < ntri; t++) {
        const float *a = tri + 9 * t, *b = a + 3, *c = a + 6;
        float e = 0.0f;
        int i, j, s, q;
        for (q = 0; q < 3; q++) {
            float d1 = b[q] - a[q], d2 = c[q] - a[q], d3 = c[q] - b[q];
            e = fmaxf(e, fmaxf(fabsf(d1), fmaxf(fabsf(d2), fabsf(d3))));
        }
        s = (int)ceilf(e / (0.25f * sp));
        if (s < 1)
            s = 1;
        if (s > 4096)
            s = 4096;
        for (i = 0; i <= s; i++)
            for (j = 0; j <= s - i; j++) {
                float u = (float)i / s, v = (float)j / s, p[3];
                int c0[3], x, y, z;
                for (q = 0; q < 3; q++) {
                    p[q] = a[q] + (b[q] - a[q]) * u + (c[q] - a[q]) * v;
                    c0[q] = (int)floorf((p[q] - org[q]) / sp);
                }
                for (z = c0[2] - 1; z <= c0[2] + 2; z++)
                    for (y = c0[1] - 1; y <= c0[1] + 2; y++)
                        for (x = c0[0] - 1; x <= c0[0] + 2; x++)
                            if (x >= 0 && y >= 0 && z >= 0 && x < n[0] && y < n[1] && z < n[2])
                                m[(size_t)x + (size_t)n[0] * ((size_t)y + (size_t)n[1] * z)] = 1;
            }
    }
    for (k = 0; k < nd; k++)
        np += m[k];
    gt = malloc(nd * 4);
    pt = malloc((size_t)(np ? np : 1) * 16);
    if (!gt || !pt) {
        free(m); free(gt); free(pt);
        return -1;
    }
    np = 0;
    for (k = 0; k < nd; k++) {
        gt[k] = m[k] ? (uint32_t)np : 0xFFFFFFFFu;
        if (m[k]) {
            pt[4 * np] = (int32_t)(k % (size_t)n[0]);
            pt[4 * np + 1] = (int32_t)(k / (size_t)n[0] % (size_t)n[1]);
            pt[4 * np + 2] = (int32_t)(k / ((size_t)n[0] * n[1]));
            pt[4 * np + 3] = 0;
            np++;
        }
    }
    free(m);
    *grid = gt;
    *pts = pt;
    return np;
}

/* the light grid over the grid and the rays' reach: cubes of 16 m, larger
 * when that would take more than BRR_GI_GRID an axis */
static void grid_place(BrrGiU *u)
{
    float sp = u->org[3], R = u->lim[0], ext = 0.0f, cs;
    int k;
    for (k = 0; k < 3; k++)
        ext = fmaxf(ext, (u->n[k] - 1) * sp + 2.0f * R);
    cs = fmaxf(16.0f, ext / BRR_GI_GRID * 1.0001f);
    for (k = 0; k < 3; k++) {
        float e = (u->n[k] - 1) * sp + 2.0f * R;
        int d = (int)ceilf(e / cs);
        u->gorg[k] = u->org[k] - R;
        u->gdim[k] = d < 1 ? 1 : d > BRR_GI_GRID ? BRR_GI_GRID : d;
    }
    u->gorg[3] = cs;
}

static void grid_encode(BrrGi *g, id<MTLComputeCommandEncoder> e, const BrrGiU *u, id<MTLBuffer> lset)
{
    [e setComputePipelineState:g->grid];
    [e setBytes:u length:sizeof *u atIndex:0];
    [e setBuffer:lset offset:0 atIndex:1];
    [e setBuffer:g->gcnt offset:0 atIndex:2];
    [e setBuffer:g->glst offset:0 atIndex:3];
    [e dispatchThreads:MTLSizeMake((NSUInteger)(u->gdim[0] * u->gdim[1] * u->gdim[2]), 1, 1) threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];
}

void brr_gi_env(BrrGi *g, id<MTLTexture> spec, float lod)
{
    g->env = spec;
    g->u.sky[3] = spec ? 1.0f : 0.0f;
    g->u.gnd[3] = lod;
}

#define BAKE_ROWS 8   /* atlas rows a batch */
int brr_gi_bake(BrrGi *g, id<MTLCommandQueue> q, const BrrRtWorld *w, float sp, int passes, const float sun[3], const float sunc[3],
                const float sky[3], const float gnd[3], id<MTLBuffer> lset, int nlamp)
{
    id<MTLDevice> dev = q.device;
    BrrGiU *u = &g->u;
    const float *tri = w ? w->bvh.tri : NULL;
    uint32_t *gt = NULL;
    int32_t *pt = NULL;
    float lo[3] = { 1e30f, 1e30f, 1e30f }, hi[3] = { -1e30f, -1e30f, -1e30f };
    int k, i, np, T = BRR_GI_DST + 2, tpr = 16384 / (BRR_GI_DST + 2), rows, batch, pass, nr = g->nr;
    size_t nd, offn, gtn;
    float bounce = g->u.lim[3];   /* bounces past the first: as set (0: the first bounce alone) */
    CFAbsoluteTime t0 = CFAbsoluteTimeGetCurrent();
    if (!g->ready || !w || !w->built || !tri || w->bvh.ntri <= 0)
        return 0;
    if (!g->leaf || g->gen != w->gen)
        leaf_map(g, dev, w);
    for (i = 0; i < 9 * w->bvh.ntri; i++) {
        lo[i % 3] = fminf(lo[i % 3], tri[i]);
        hi[i % 3] = fmaxf(hi[i % 3], tri[i]);
    }
    for (k = 0; k < 3; k++) {
        u->org[k] = floorf(lo[k] / sp) * sp - sp;
        u->n[k] = (int)ceilf((hi[k] - u->org[k]) / sp) + 3;
        u->base[k] = 0;
    }
    u->org[3] = sp;
    np = gi_place(tri, w->bvh.ntri, u->org, sp, u->n, &gt, &pt);
    if (np <= 0)
        return 0;
    nd = (size_t)u->n[0] * u->n[1] * u->n[2];
    gtn = (nd + 3) / 4;                           /* the grid table in float4s */
    offn = (size_t)np + gtn + (size_t)np;         /* relocations, grid table, grid points */
    rows = (np + tpr - 1) / tpr;
    batch = tpr * BAKE_ROWS;
    if (g->np != np || !g->off || g->off.length < offn * 16) {
        g->off = [dev newBufferWithLength:offn * 16 options:MTLResourceStorageModeShared];
        g->sh = [dev newBufferWithLength:(size_t)np * 9 * 16 options:MTLResourceStorageModeShared];
        g->ray = [dev newBufferWithLength:(size_t)batch * nr * 16 options:MTLResourceStorageModeShared];
    }
    if (!g->ray)
        g->ray = [dev newBufferWithLength:(size_t)batch * nr * 16 options:MTLResourceStorageModeShared];
    for (k = 0; k < 2; k++)
        g->dst[k] = atlas(dev, tpr * T, ((rows + BAKE_ROWS - 1) / BAKE_ROWS * BAKE_ROWS) * T, MTLPixelFormatRG16Float);
    if (!g->off || !g->sh || !g->ray || !g->dst[0] || !g->dst[1]) {
        free(gt); free(pt);
        g->baked = 0;
        return 0;
    }
    memset([g->off contents], 0, offn * 16);
    memcpy((char *)[g->off contents] + (size_t)np * 16, gt, nd * 4);
    memcpy((char *)[g->off contents] + ((size_t)np + gtn) * 16, pt, (size_t)np * 16);
    memset([g->sh contents], 0, (size_t)np * 9 * 16);
    free(gt);
    free(pt);
    g->np = np;
    u->bk[0] = np; u->bk[1] = tpr;
    u->bx[0] = np; u->bx[1] = (int)((size_t)np + gtn);
    for (k = 0; k < 3; k++) {
        u->sun[k] = sun[k]; u->sunc[k] = sunc[k]; u->sky[k] = sky[k]; u->gnd[k] = gnd[k];
    }
    u->lim[2] = (float)(lset ? (nlamp < 128 ? nlamp : 128) : 0);
    u->bx[3] = 1;
    grid_place(u);
    g->cur = 0;
    for (pass = 0; pass < passes; pass++) {
        id<MTLCommandBuffer> cb = [q commandBuffer];
        id<MTLComputeCommandEncoder> e = [cb computeCommandEncoder];
        int prev = g->cur, next = g->cur ^ 1, b0;
        rotation(u, (uint32_t)pass);
        u->bx[2] = pass;
        u->par[0] = fminf((float)pass / (float)(pass + 1), g->keep);
        /* the first pass's hits take their direct light only: there are no probes to read yet (each
           would fall back to the open sky's light, and inside a tunnel that first bounce, carried
           through every later pass, lit it 10 to 40 times over) */
        u->lim[3] = pass > 0 ? bounce : 0.0f;
        if (pass == 0 && lset)
            grid_encode(g, e, u, lset);
        for (b0 = 0; b0 < np; b0 += batch) {
            u->bk[2] = b0;
            u->bk[3] = np - b0 < batch ? np - b0 : batch;
            [e setComputePipelineState:g->offreset];
            [e setBytes:u length:sizeof *u atIndex:0];
            [e setBuffer:g->off offset:0 atIndex:11];
            [e dispatchThreads:MTLSizeMake((NSUInteger)u->bk[3], 1, 1) threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];
            [e setComputePipelineState:g->trace];
            [e setBuffer:g->ray offset:0 atIndex:1];
            [e setBuffer:g->leaf offset:0 atIndex:2];
            [e setBuffer:lset offset:0 atIndex:3];
            brr_rt_bind(w, e, 4);
            [e setBuffer:g->gcnt offset:0 atIndex:9];
            [e setBuffer:g->glst offset:0 atIndex:10];
            [e setBuffer:g->sh offset:0 atIndex:12];
            [e setTexture:g->dst[prev] atIndex:1];
            [e setTexture:g->env ? g->env : g->nocube atIndex:2];
            [e dispatchThreads:MTLSizeMake((NSUInteger)nr, (NSUInteger)u->bk[3], 1) threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];
            [e setComputePipelineState:g->reloc];   /* where the next pass traces from (gi_reloc) */
            [e dispatchThreads:MTLSizeMake((NSUInteger)u->bk[3], 1, 1) threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];
            [e setComputePipelineState:g->bsh];
            [e dispatchThreads:MTLSizeMake((NSUInteger)u->bk[3], 1, 1) threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];
            [e setComputePipelineState:g->bdst];
            [e setTexture:g->dst[prev] atIndex:0];
            [e setTexture:g->dst[next] atIndex:1];
            [e dispatchThreads:MTLSizeMake((NSUInteger)(tpr * T), (NSUInteger)(((u->bk[3] + tpr - 1) / tpr) * T), 1)
                threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
            /* the encoder is serial: the next batch's trace reads this one's coefficients (one bounce sooner) */
        }
        [e endEncoding];
        [cb commit];
        [cb waitUntilCompleted];
        if (cb.status != MTLCommandBufferStatusCompleted) {
            g->baked = 0;
            return 0;
        }
        g->cur = next;
    }
    if (!g->keepray) {
        g->dst[g->cur ^ 1] = nil;   /* the spare atlas, only the bake's */
        g->ray = nil;
    }
    u->bx[3] = 0;
    u->lim[3] = bounce;
    g->bytes = offn * 16 + (size_t)np * 9 * 16 + (size_t)g->dst[g->cur].width * g->dst[g->cur].height * 4;
    g->ms = (CFAbsoluteTimeGetCurrent() - t0) * 1000.0;
    g->baked = 1;
    return 1;
}

id<MTLBuffer> brr_gi_sh(const BrrGi *g) { return g->sh; }
id<MTLTexture> brr_gi_dst(const BrrGi *g) { return g->dst[g->cur]; }
id<MTLBuffer> brr_gi_off(const BrrGi *g) { return g->off; }
