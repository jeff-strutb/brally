/* metal_test.m: the lights' shader code (brr_metal_lights.h, the renderer's
 * own source) run on the GPU against the CPU reference (light_cluster.c), on
 * synthetic scenes; not the game.
 *
 *   clusters   random cameras, lights and points: the shader's cluster for a
 *              point is the CPU's; every light the shader finds reaching a
 *              point (lamp x lwin > 0, all lights tried) is in the shader's
 *              walk of the point's cluster list
 *   shadow     a lamp's tile drawn by the renderer's atlas shaders (lcclr,
 *              lcvs) into a Depth32Float atlas, its depth against a ray cast,
 *              and the shader's lampvis on the ground behind a board
 *   grid       the 20-car night grid at 1920 x 1080: the lamps' light per
 *              pixel through the shader's lamps() (clustered, shadowed) and by
 *              brute force, GPU time each, and the atlas's drawing time
 *
 * Build and run: ports/brally/tools/lighttest/run.sh gpu */
#import <Metal/Metal.h>
#include <stdio.h>
#include <stdlib.h>

#include "scene.h"
#include "brr_metal_lights.h"
#include "lampvis_c.h"

static int g_fail;
#define LAMP_TEST_HEIGHT 0.65f   /* light_scene.c: a headlamp 0.65 m up (UN ECE R48 6.2.4: 0.5 to 1.2 m) */
#define CHECK(c, ...)                                                                    \
    do {                                                                                 \
        if (!(c)) {                                                                      \
            g_fail++;                                                                    \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                                  \
            printf(__VA_ARGS__);                                                         \
            printf("\n");                                                                \
        }                                                                                \
    } while (0)

/* a brdf of the lighting's signature (brr_metal_lit.h's takes its albedo table
   bE), which lamps() calls: this test compares lamps() with every light through
   the same function, so a plain GGX and Lambert stands in and bE goes unread */
#define BRDF_SRC \
"static float3 brdf(float3 n, float3 v, float3 l, float3 alb, float rough, texture2d<float> bE) {\n" \
"  float3 h = normalize(l + v); float nl = max(dot(n, l), 0.0), nv = max(dot(n, v), 1e-3), nh = max(dot(n, h), 0.0);\n" \
"  float a2 = max(rough * rough, 2e-3); a2 *= a2;\n" \
"  float d = nh * nh * (a2 - 1.0) + 1.0, D = a2 / (3.14159 * d * d);\n" \
"  float Vs = 0.5 / max(nl * sqrt(nv * nv * (1.0 - a2) + a2) + nv * sqrt(nl * nl * (1.0 - a2) + a2), 1e-4);\n" \
"  float F = 0.04 + 0.96 * pow(1.0 - max(dot(h, v), 0.0), 5.0);\n" \
"  return (alb / 3.14159 * (1.0 - F) + D * Vs * F) * nl;\n" \
"}\n"

static NSString *const k_test_src = @
"#include <metal_stdlib>\n"
"using namespace metal;\n"
BRR_LIGHTS_SRC
BRDF_SRC
"kernel void ksz(device uint *o [[buffer(0)]]) { o[0] = sizeof(LSet); o[1] = sizeof(LS); o[2] = sizeof(LCH); o[3] = sizeof(LSH); }\n"
"static bool reach(LS l, float3 p) { float3 L = float3(l.pos) - p; float d2 = max(dot(L, L), 0.01), dl = sqrt(d2); float3 ld = L / dl;\n"
"  return lamp(l, -ld) * lwin(l, -ld, dl) > 0.0; }\n"
"kernel void kcl(const device LSet &s [[buffer(0)]], const device float4 *pts [[buffer(1)]], device uint *out [[buffer(2)]],\n"
"                uint g [[thread_position_in_grid]]) {\n"
"  float3 p = pts[g].xyz; uint c = lc_index(s.h, p), e = s.cl[c];\n"
"  uint vc[4] = { 0, 0, 0, 0 }, va[4] = { 0, 0, 0, 0 };\n"
"  for (uint k = 0; k < (e & 255u); k++) { uint i = s.idx[(e >> 8) + k]; if (reach(s.l[i], p)) vc[i >> 5] |= 1u << (i & 31u); }\n"
"  for (uint i = 0; i < uint(s.h.eye.w); i++) if (reach(s.l[i], p)) va[i >> 5] |= 1u << (i & 31u);\n"
"  out[g * 9] = c; for (int k = 0; k < 4; k++) { out[g * 9 + 1 + k] = vc[k]; out[g * 9 + 5 + k] = va[k]; }\n"
"}\n"
"kernel void ksh(const device LSet &s [[buffer(0)]], depth2d<float> atl [[texture(0)]], const device float4 *pts [[buffer(1)]],\n"
"                device float *out [[buffer(2)]], uint g [[thread_position_in_grid]]) {\n"
"  float3 p = pts[g].xyz; LS l = s.l[0];\n"
"  out[g] = lampvis(s, atl, 0, p, float3(0, 0, 1), normalize(float3(l.pos) - p), float2(1, 0));\n"
"}\n"
"kernel void kpen(const device LSet &s [[buffer(0)]], depth2d<float> atl [[texture(0)]], const device float4 *pts [[buffer(1)]],\n"
"                 device float *out [[buffer(2)]], constant float2 &rot [[buffer(3)]], uint g [[thread_position_in_grid]]) {\n"
"  float3 p = pts[g].xyz; LS l = s.l[0];\n"
"  out[g] = lampvis(s, atl, 0, p, float3(-1, 0, 0), normalize(float3(l.pos) - p), rot);\n"
"}\n"
"kernel void kbox(const device LSet &s [[buffer(0)]], depth2d<float> atl [[texture(0)]], const device float4 *pts [[buffer(1)]],\n"
"                 device float2 *out [[buffer(2)]], uint g [[thread_position_in_grid]]) {\n"
"  float3 p = pts[g].xyz, n = float3(-1, 0, 0); LS l = s.l[0]; float3 L = float3(l.pos) - p; float d2 = dot(L, L), dl = sqrt(d2); float3 ld = L / dl;\n"
"  out[g] = float2(lamp(l, -ld) * lwin(l, -ld, dl) / d2 * max(dot(n, ld), 0.0), lampvis(s, atl, 0, p, n, ld, float2(1, 0)));\n"
"}\n"
"struct CU { float4 eye, fwd, right, up, tan; };\n"
/* the ground under the camera's pixel, lit by the lamps: 0 clustered (the
   renderer's lamps()), 1 every light (unshadowed: a set with no tiles) */
"kernel void klit(const device LSet &s [[buffer(0)]], depth2d<float> atl [[texture(0)]], texture2d<float> bE [[texture(1)]], constant CU &cu [[buffer(1)]],\n"
"                 constant uint &mode [[buffer(2)]], device float4 *out [[buffer(3)]], uint2 g [[thread_position_in_grid]]) {\n"
"  uint W = uint(cu.tan.z), H = uint(cu.tan.w); if (g.x >= W || g.y >= H) return;\n"
"  float2 nd = float2((g.x + 0.5) / W * 2.0 - 1.0, 1.0 - (g.y + 0.5) / H * 2.0);\n"
"  float3 d = cu.fwd.xyz + nd.x * cu.tan.x * cu.right.xyz + nd.y * cu.tan.y * cu.up.xyz;\n"
"  if (d.z >= 0.0) { out[g.y * W + g.x] = float4(0); return; }\n"
"  float3 p = cu.eye.xyz + d * (-cu.eye.z / d.z), n = float3(0, 0, 1), v = normalize(cu.eye.xyz - p), alb = float3(0.3);\n"
"  float3 Lo = float3(0);\n"
"  if (mode == 0) Lo = lamps(s, atl, p, n, v, alb, 0.6, bE, 0.0);\n"
"  else {\n"
"    for (uint i = 0; i < uint(s.h.eye.w); i++) {\n"
"      LS l = s.l[i]; float3 L = float3(l.pos) - p; float d2 = max(dot(L, L), 0.01), dl = sqrt(d2); float3 ld = L / dl;\n"
"      if (dot(n, ld) <= 0.0) continue;\n"
"      float E = lamp(l, -ld) * lwin(l, -ld, dl) / d2;\n"
"      if (E <= 0.0) continue;\n"
"      Lo += float3(l.rgb) * E * lampvis(s, atl, i, p, n, ld, float2(1, 0)) * brdf(n, v, ld, alb, 0.6, bE);\n"
"    }\n"
"  }\n"
"  out[g.y * W + g.x] = float4(Lo, 1);\n"
"}\n";

static id<MTLDevice> g_dev;
static id<MTLCommandQueue> g_q;
static id<MTLLibrary> g_lib;
static id<MTLTexture> g_be;         /* the brdf's table argument (unread by the stand-in) */
static id<MTLRenderPipelineState> g_pclr, g_pcas;
static id<MTLDepthStencilState> g_dsall, g_dsless;

static id<MTLComputePipelineState> kernel_named(NSString *n)
{
    NSError *err = nil;
    id<MTLComputePipelineState> k = [g_dev newComputePipelineStateWithFunction:[g_lib newFunctionWithName:n] error:&err];
    if (!k) {
        printf("kernel %s: %s\n", [n UTF8String], [[err description] UTF8String]);
        exit(1);
    }
    return k;
}

static id<MTLRenderPipelineState> depth_pipe(NSString *vs)
{
    MTLRenderPipelineDescriptor *d = [MTLRenderPipelineDescriptor new];
    NSError *err = nil;
    id<MTLRenderPipelineState> p;
    d.vertexFunction = [g_lib newFunctionWithName:vs];
    d.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
    p = [g_dev newRenderPipelineStateWithDescriptor:d error:&err];
    if (!p) {
        printf("pipeline %s: %s\n", [vs UTF8String], [[err description] UTF8String]);
        exit(1);
    }
    return p;
}

static LightSet g_set;
static LightShadowRec g_rec_cpu;

/* ---- clusters ------------------------------------------------------------------- */
static void test_clusters(void)
{
    id<MTLComputePipelineState> k = kernel_named(@"kcl");
    id<MTLBuffer> sb = [g_dev newBufferWithLength:sizeof(LightSet) options:MTLResourceStorageModeShared];
    const int npts = 65536;
    id<MTLBuffer> pb = [g_dev newBufferWithLength:npts * 16 options:MTLResourceStorageModeShared];
    id<MTLBuffer> ob = [g_dev newBufferWithLength:npts * 36 options:MTLResourceStorageModeShared];
    long pts = 0, reach = 0, fn = 0, cldiff = 0, cpudiff = 0, tiny = 0, gpuonly = 0;
    int t;
    for (t = 0; t < 100; t++) {
        Cam c;
        float eye[3] = { rndf(-1000, 1000), rndf(-1000, 1000), rndf(0.3f, 30.0f) }, fwd[3] = { rndf(-1, 1), rndf(-1, 1), rndf(-0.5f, 0.3f) };
        int n = t % 2 ? LC_LIGHTS : 1 + (int)(rnd() * LC_LIGHTS), i, j;
        float (*P)[4] = (float (*)[4])[pb contents];
        const uint32_t *o = (const uint32_t *)[ob contents];
        cam_make(&c, eye, fwd, rndf(35, 95), 16.0f / 9.0f, 0.1f, 3000.0f);
        for (i = 0; i < n; i++)
            light_random(&g_set.l[i], &c);
        light_cluster_build(&g_set, n, c.vp, c.eye, NULL);
        memcpy([sb contents], &g_set, sizeof g_set);
        for (i = 0; i < npts; i++) {
            float w;
            do {
                if (i & 1)
                    cam_point(&c, rndf(-1.0f, 1.0f), rndf(-1.0f, 1.0f), 0.05f * powf(2000.0f / 0.05f, (float)rnd()), P[i]);
                else
                    point_near(&g_set.l[(int)(rnd() * n)], P[i]);
                w = c.vp[3] * P[i][0] + c.vp[7] * P[i][1] + c.vp[11] * P[i][2] + c.vp[15];
            } while (!(w > 0.0f));
            P[i][3] = 1;
        }
        {
            id<MTLCommandBuffer> cb = [g_q commandBuffer];
            id<MTLComputeCommandEncoder> e = [cb computeCommandEncoder];
            [e setComputePipelineState:k];
            [e setBuffer:sb offset:0 atIndex:0];
            [e setBuffer:pb offset:0 atIndex:1];
            [e setBuffer:ob offset:0 atIndex:2];
            [e dispatchThreads:MTLSizeMake(npts, 1, 1) threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];
            [e endEncoding];
            [cb commit];
            [cb waitUntilCompleted];
        }
        for (i = 0; i < npts; i++) {
            const uint32_t *r = &o[9 * i];
            int ccpu = light_cluster_of(&g_set, P[i]);
            pts++;
            cldiff += (int)r[0] != ccpu;
            for (j = 0; j < 4; j++) {
                uint32_t all = r[5 + j], via = r[1 + j], cpu = 0;
                int b;
                for (b = 0; b < 32 && j * 32 + b < n; b++)
                    if (light_influence(&g_set.l[j * 32 + b], P[i]) > 0.0f)
                        cpu |= 1u << b;
                fn += __builtin_popcount(all & ~via);
                for (b = 0; b < 32 && j * 32 + b < n; b++)
                    if (((all ^ cpu) >> b) & 1) {
                        if ((all >> b) & 1)
                            gpuonly++;
                        else if (light_influence(&g_set.l[j * 32 + b], P[i]) < 1e-30f)
                            tiny++;
                    }
                reach += __builtin_popcount(all);
                cpudiff += __builtin_popcount(all ^ cpu);
            }
        }
    }
    printf("gpu clusters: 100 cameras, %ld points: %ld (point, light) pairs reaching on the GPU, %ld missing from the "
           "cluster walk; the shader's cluster differs from the CPU's at %ld points (a boundary, by rounding)\n",
           pts, reach, fn, cldiff);
    printf("  GPU and CPU reach disagree on %ld pairs (%.3f%%): %ld where the CPU's light is under 1e-30 cd (the GPU flushes "
           "it to zero), %ld where the GPU reaches and the CPU does not (a range or cone edge, by rounding: the margin's case), "
           "%ld other\n", cpudiff, 100.0 * cpudiff / (reach ? reach : 1), tiny, gpuonly, cpudiff - tiny - gpuonly);
    CHECK(fn == 0, "%ld lights reach a point on the GPU but its cluster walk misses them", fn);
}

/* ---- shadows -------------------------------------------------------------------- */
static void draw_tiles(id<MTLCommandBuffer> cb, id<MTLTexture> atl, const LightShadowPlan *p, id<MTLBuffer> vb, int nv, int clear)
{
    MTLRenderPassDescriptor *rp = [MTLRenderPassDescriptor renderPassDescriptor];
    id<MTLRenderCommandEncoder> e;
    int i;
    rp.depthAttachment.texture = atl;
    rp.depthAttachment.loadAction = clear ? MTLLoadActionClear : MTLLoadActionLoad;
    rp.depthAttachment.clearDepth = 1.0;
    rp.depthAttachment.storeAction = MTLStoreActionStore;
    e = [cb renderCommandEncoderWithDescriptor:rp];
    [e setCullMode:MTLCullModeNone];
    for (i = 0; i < p->n; i++) {
        const LightShadowTile *t = &p->t[i];
        if (!t->render)
            continue;
        [e setViewport:(MTLViewport){ t->x, t->y, t->size, t->size, 0.0, 1.0 }];
        [e setScissorRect:(MTLScissorRect){ (NSUInteger)t->x, (NSUInteger)t->y, (NSUInteger)t->size, (NSUInteger)t->size }];
        [e setRenderPipelineState:g_pclr];
        [e setDepthStencilState:g_dsall];
        [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        [e setRenderPipelineState:g_pcas];
        [e setDepthStencilState:g_dsless];
        [e setVertexBuffer:vb offset:0 atIndex:0];
        [e setVertexBytes:g_set.s[t->light].m length:64 atIndex:1];
        [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:(NSUInteger)nv];
    }
    [e endEncoding];
}

static id<MTLTexture> atlas_tex(int size)
{
    MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                                                                  width:(NSUInteger)size height:(NSUInteger)size mipmapped:NO];
    td.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    td.storageMode = MTLStorageModePrivate;
    return [g_dev newTextureWithDescriptor:td];
}

static void test_shadow(void)
{
    /* cluster_test.c's occluder scene: a spot 1 m up looking down x, a 2 x 2 m
       board at x = 10, the ground; a 512 tile at (512, 256) of a 1024 atlas */
    static const float tris[4][9] = { { 10, -1, 0, 10, 1, 0, 10, 1, 2 }, { 10, -1, 0, 10, 1, 2, 10, -1, 2 },
                                      { -5, -60, 0, 80, -60, 0, 80, 60, 0 }, { -5, -60, 0, 80, 60, 0, -5, 60, 0 } };
    const int asz = 1024, tsz = 512, tx0 = 512, ty0 = 256, gw = 540, gh = 240;
    id<MTLTexture> atl = atlas_tex(asz);
    id<MTLBuffer> vb = [g_dev newBufferWithBytes:tris length:sizeof tris options:MTLResourceStorageModeShared];
    id<MTLBuffer> sb = [g_dev newBufferWithLength:sizeof(LightSet) options:MTLResourceStorageModeShared];
    id<MTLBuffer> rb = [g_dev newBufferWithLength:(NSUInteger)tsz * tsz * 4 options:MTLResourceStorageModeShared];
    id<MTLBuffer> pb = [g_dev newBufferWithLength:(NSUInteger)gw * gh * 16 options:MTLResourceStorageModeShared];
    id<MTLBuffer> ob = [g_dev newBufferWithLength:(NSUInteger)gw * gh * 4 options:MTLResourceStorageModeShared];
    LightSpot *l = &g_set.l[0];
    LightShadowPlan p;
    int x, y, n = 0, nshad = 0, nlit = 0, bad = 0;
    double maxdd = 0, sumdd = 0;
    long ndd = 0;
    memset(&g_set.h, 0, sizeof g_set.h);
    memset(l, 0, sizeof *l);
    l->pos[2] = 1.0f;
    l->axis[0] = 1;
    l->side[1] = -1;
    l->kind = LIGHT_SPOT;
    l->candela = 1000;
    l->range = 60;
    l->cosout = cosf(50.0f * 3.14159265f / 180.0f);
    l->side[0] = cosf(40.0f * 3.14159265f / 180.0f);
    light_shadow_matrix(l, &g_set.s[0]);
    g_rec_cpu = g_set.s[0];
    g_set.s[0].rect[0] = (float)tx0 / asz;
    g_set.s[0].rect[1] = (float)ty0 / asz;
    g_set.s[0].rect[2] = (float)tsz / asz;
    g_set.s[0].rect[3] = 1;
    g_set.h.atl[0] = asz;
    g_set.h.atl[1] = 1.0f / asz;
    g_set.h.eye[3] = 1;
    memcpy([sb contents], &g_set, sizeof g_set);
    memset(&p, 0, sizeof p);
    p.n = 1;
    p.t[0].light = 0;
    p.t[0].x = tx0;
    p.t[0].y = ty0;
    p.t[0].size = tsz;
    p.t[0].render = 1;
    {
        float (*P)[4] = (float (*)[4])[pb contents];
        id<MTLCommandBuffer> cb = [g_q commandBuffer];
        id<MTLBlitCommandEncoder> b;
        id<MTLComputeCommandEncoder> e;
        for (y = 0; y < gh; y++)
            for (x = 0; x < gw; x++) {
                float *q = P[y * gw + x];
                q[0] = 1.0f + x * 0.1f;
                q[1] = -12.0f + y * 0.1f;
                q[2] = 0;
                q[3] = 1;
            }
        draw_tiles(cb, atl, &p, vb, 12, 1);
        b = [cb blitCommandEncoder];
        [b copyFromTexture:atl sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(tx0, ty0, 0) sourceSize:MTLSizeMake(tsz, tsz, 1)
                  toBuffer:rb destinationOffset:0 destinationBytesPerRow:tsz * 4 destinationBytesPerImage:(NSUInteger)tsz * tsz * 4];
        [b endEncoding];
        e = [cb computeCommandEncoder];
        [e setComputePipelineState:kernel_named(@"ksh")];
        [e setBuffer:sb offset:0 atIndex:0];
        [e setTexture:atl atIndex:0];
        [e setBuffer:pb offset:0 atIndex:1];
        [e setBuffer:ob offset:0 atIndex:2];
        [e dispatchThreads:MTLSizeMake((NSUInteger)gw * gh, 1, 1) threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];
        [e endEncoding];
        [cb commit];
        [cb waitUntilCompleted];
    }
    /* the drawn depth against a ray cast through each texel's centre (where it hits a surface) */
    {
        const float *dep = (const float *)[rb contents];
        const LightShadowRec *r = &g_rec_cpu;
        for (y = 0; y < tsz; y++)
            for (x = 0; x < tsz; x++) {
                float nx = (x + 0.5f) / tsz * 2 - 1, ny = 1 - (y + 0.5f) / tsz * 2, dir[3], best = 1e30f;
                int k, t;
                for (k = 0; k < 3; k++)
                    dir[k] = r->m[k * 4 + 3] + nx * r->m[k * 4 + 0] * r->prm[2] * r->prm[2] + ny * r->m[k * 4 + 1] * r->prm[2] * r->prm[2];
                for (t = 0; t < 4; t++) {
                    const float *v = tris[t];
                    float e1[3], e2[3], pv[3], tv[3], qv[3], det, a, bb, s;
                    for (k = 0; k < 3; k++) {
                        e1[k] = v[3 + k] - v[k];
                        e2[k] = v[6 + k] - v[k];
                        tv[k] = l->pos[k] - v[k];
                    }
                    cross3(dir, e2, pv);
                    det = e1[0] * pv[0] + e1[1] * pv[1] + e1[2] * pv[2];
                    if (fabsf(det) < 1e-12f)
                        continue;
                    a = (tv[0] * pv[0] + tv[1] * pv[1] + tv[2] * pv[2]) / det;
                    cross3(tv, e1, qv);
                    bb = (dir[0] * qv[0] + dir[1] * qv[1] + dir[2] * qv[2]) / det;
                    s = (e2[0] * qv[0] + e2[1] * qv[1] + e2[2] * qv[2]) / det;
                    if (a < 0 || bb < 0 || a + bb > 1 || s <= r->prm[3] || s >= l->range)
                        continue;
                    best = fminf(best, s);
                }
                if (best < 1e29f) {     /* compare as distances: depth back to metres */
                    float dg = r->prm[1] / (dep[y * tsz + x] - r->prm[0]), dd = fabsf(dg - best) / best;
                    if (dd < 0.05f) {     /* texels on a silhouette edge differ by a surface: not the raster */
                        maxdd = fmax(maxdd, dd);
                        sumdd += dd;
                        ndd++;
                    }
                }
            }
    }
    {
        const float *v = (const float *)[ob contents];
        for (y = 0; y < gh; y++)
            for (x = 0; x < gw; x++) {
                float px = 1.0f + x * 0.1f, py = -12.0f + y * 0.1f, yc = fabsf(py) * 10.0f / px, texel = 2.0f * 10.0f * g_rec_cpu.prm[2] / tsz, pt[3] = { px, py, 0 };
                int sh = px > 10.0f && yc <= 1.0f;
                if (light_influence(l, pt) <= 0.0f || (px > 10.0f && fabsf(yc - 1.0f) < 3.0f * texel) || fabsf(px - 10.0f) < 1.0f)
                    continue;
                n++;
                if (sh) {
                    nshad++;
                    bad += v[y * gw + x] > 0.05f;
                } else {
                    nlit++;
                    bad += v[y * gw + x] < 0.95f;
                }
            }
    }
    printf("gpu shadow: tile drawn by lcclr/lcvs, depth against a ray cast at %ld texel centres: mean %.2e, largest %.2e "
           "(relative distance); lampvis on %d ground points (%d behind the board, %d open): %d misclassified\n",
           ndd, sumdd / (ndd ? ndd : 1), maxdd, n, nshad, nlit, bad);
    CHECK(ndd > 100000 && maxdd < 1e-3, "drawn depth off the ray cast: %g", maxdd);
    CHECK(bad == 0 && nshad > 1000 && nlit > 1000, "%d ground points shadowed wrongly", bad);
}

/* the soft shadow's edge drawn by the casters' pipelines: a blocker plane at
   x = db over y >= 0, a wall at x = 10; the shader's lampvis along a line
   across the edge against its C twin over the same drawn tile, and its
   10%..90% width (averaged over 16 turns) against the lamp disc's */
static void test_shadow_penumbra(void)
{
    const int asz = 1024, tsz = 512, tx0 = 512, ty0 = 256, nrot = 16, npt = LV_NS + 2;
    const float dr = 10.0f, dbs[2] = { 2.0f, 5.0f };
    LightSpot *l = &g_set.l[0];
    LightShadowPlan p;
    id<MTLTexture> atl = atlas_tex(asz);
    id<MTLBuffer> sb = [g_dev newBufferWithLength:sizeof(LightSet) options:MTLResourceStorageModeShared];
    id<MTLBuffer> rb = [g_dev newBufferWithLength:(NSUInteger)tsz * tsz * 4 options:MTLResourceStorageModeShared];
    id<MTLBuffer> pb = [g_dev newBufferWithLength:(NSUInteger)npt * 16 options:MTLResourceStorageModeShared];
    id<MTLBuffer> ob = [g_dev newBufferWithLength:(NSUInteger)npt * nrot * 4 options:MTLResourceStorageModeShared];
    float *dep = malloc((size_t)asz * asz * sizeof(float)), h10 = lv_disc_h10();
    int bi, k, i, y;
    memset(&g_set.h, 0, sizeof g_set.h);
    lv_pen_lamp(l);
    light_shadow_matrix(l, &g_set.s[0]);
    g_set.s[0].rect[0] = (float)tx0 / asz;
    g_set.s[0].rect[1] = (float)ty0 / asz;
    g_set.s[0].rect[2] = (float)tsz / asz;
    g_set.s[0].rect[3] = 1;
    g_set.h.atl[0] = asz;
    g_set.h.atl[1] = 1.0f / asz;
    g_set.h.eye[3] = 1;
    memcpy([sb contents], &g_set, sizeof g_set);
    memset(&p, 0, sizeof p);
    p.n = 1;
    p.t[0].x = tx0;
    p.t[0].y = ty0;
    p.t[0].size = tsz;
    p.t[0].render = 1;
    {
        float (*P)[4] = (float (*)[4])[pb contents];
        for (i = 0; i < npt; i++) {
            P[i][0] = dr;
            P[i][1] = i < LV_NS ? LV_Y0 + i * LV_DY : i == LV_NS ? -1.5f : 1.5f;
            P[i][2] = 1;
            P[i][3] = 1;
        }
    }
    for (bi = 0; bi < 2; bi++) {
        float db = dbs[bi], tris[4][9] = { { db, 0, -20, db, 20, -20, db, 20, 20 }, { db, 0, -20, db, 20, 20, db, 0, 20 },
                                           { dr, -30, -30, dr, 30, -30, dr, 30, 30 }, { dr, -30, -30, dr, 30, 30, dr, -30, 30 } };
        float avg[LV_NS], geo = 2 * h10 * 0.075f * (dr - db) / db, texel = 2 * dr * g_set.s[0].prm[2] / tsz, w, rise, ra, maxd = 0;
        int exact_bad = 0;
        id<MTLBuffer> vb = [g_dev newBufferWithBytes:tris length:sizeof tris options:MTLResourceStorageModeShared];
        id<MTLCommandBuffer> cb = [g_q commandBuffer];
        id<MTLBlitCommandEncoder> b;
        id<MTLComputeCommandEncoder> e;
        const float *gv = (const float *)[ob contents];
        draw_tiles(cb, atl, &p, vb, 12, 1);
        b = [cb blitCommandEncoder];
        [b copyFromTexture:atl sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(tx0, ty0, 0) sourceSize:MTLSizeMake(tsz, tsz, 1)
                  toBuffer:rb destinationOffset:0 destinationBytesPerRow:tsz * 4 destinationBytesPerImage:(NSUInteger)tsz * tsz * 4];
        [b endEncoding];
        e = [cb computeCommandEncoder];
        [e setComputePipelineState:kernel_named(@"kpen")];
        [e setBuffer:sb offset:0 atIndex:0];
        [e setTexture:atl atIndex:0];
        [e setBuffer:pb offset:0 atIndex:1];
        for (k = 0; k < nrot; k++) {
            float rot[2] = { cosf(k * 6.2831853f / nrot), sinf(k * 6.2831853f / nrot) };
            [e setBuffer:ob offset:(NSUInteger)k * npt * 4 atIndex:2];
            [e setBytes:rot length:8 atIndex:3];
            [e dispatchThreads:MTLSizeMake((NSUInteger)npt, 1, 1) threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];
        }
        [e endEncoding];
        [cb commit];
        [cb waitUntilCompleted];
        for (i = 0; i < asz * asz; i++)
            dep[i] = 1.0f;
        for (y = 0; y < tsz; y++)
            memcpy(dep + (size_t)(ty0 + y) * asz + tx0, (const float *)[rb contents] + (size_t)y * tsz, (size_t)tsz * 4);
        memset(avg, 0, sizeof avg);
        rise = 0;
        for (k = 0; k < nrot; k++) {
            float rot[2] = { cosf(k * 6.2831853f / nrot), sinf(k * 6.2831853f / nrot) }, nn[3] = { -1, 0, 0 }, ri;
            const float *v = gv + (size_t)k * npt;
            for (i = 0; i < npt; i++) {
                float pt[3] = { dr, i < LV_NS ? LV_Y0 + i * LV_DY : i == LV_NS ? -1.5f : 1.5f, 1.0f }, ld[3], dl;
                ld[0] = -dr;
                ld[1] = -pt[1];
                ld[2] = 0;
                dl = sqrtf(dr * dr + pt[1] * pt[1]);
                ld[0] /= dl;
                ld[1] /= dl;
                maxd = fmaxf(maxd, fabsf(v[i] - lampvis_c(&g_set.s[0], dep, asz, pt, nn, ld, rot)));
                if (i < LV_NS)
                    avg[i] += v[i] / nrot;
            }
            exact_bad += (v[LV_NS] != 1.0f) + (v[LV_NS + 1] != 0.0f);
            lv_width(v, &ri);
            rise = fmaxf(rise, ri);
        }
        w = lv_width(avg, &ra);
        printf("gpu penumbra: blocker %.0f m, wall %.0f m: 10-90%% width %.4f m, the lamp disc's %.4f m, off by %.2f texel; "
               "largest rise %.2g; %d of %d clear points not exactly 1 / 0; GPU against the C twin: largest difference %.4f\n",
               db, dr, w, geo, fabsf(w - geo) / texel, rise, exact_bad, 2 * nrot, maxd);
        CHECK(w > 0 && fabsf(w - geo) <= texel, "gpu blocker %.0f m: penumbra %.4f m, geometric %.4f m", db, w, geo);
        CHECK(rise <= 1e-3f, "gpu blocker %.0f m: the edge rises by %g", db, rise);
        CHECK(exact_bad == 0, "gpu blocker %.0f m: %d clear points not 1 / 0", db, exact_bad);
        CHECK(maxd <= 0.02f, "gpu blocker %.0f m: lampvis off its C twin by %g", db, maxd);
    }
    free(dep);
}

/* ---- a car ahead in a headlamp ---------------------------------------------------- */
static void box_tris(float x0, float x1, float y0, float y1, float z0, float z1, float *t)
{
    float c[8][3];
    static const int f[12][3] = { { 0, 1, 3 }, { 0, 3, 2 }, { 4, 6, 7 }, { 4, 7, 5 }, { 0, 4, 5 }, { 0, 5, 1 },
                                  { 2, 3, 7 }, { 2, 7, 6 }, { 0, 2, 6 }, { 0, 6, 4 }, { 1, 5, 7 }, { 1, 7, 3 } };
    int i, k, j;
    for (i = 0; i < 8; i++)
        c[i][0] = i & 4 ? x1 : x0, c[i][1] = i & 2 ? y1 : y0, c[i][2] = i & 1 ? z1 : z0;
    for (i = 0; i < 12; i++)
        for (k = 0; k < 3; k++)
            for (j = 0; j < 3; j++)
                t[i * 9 + k * 3 + j] = c[f[i][k]][j];
}

/* the rear of a car-sized box (4 x 1.7 x 1.25 m, 0.15 m off the ground) D m ahead of a
   headlamp at height hl, lit three ways: the pattern alone (no tile), through a freshly
   drawn tile, and through a stale tile (light_shadow_plan's over-budget path: last
   frame's depth with last frame's transform) after lamp and car both moved dx forward:
   the rear face's mean illuminance, lux, and the lit fraction of it */
static void box_case(float D, float hl, float dx, double *E, double *vf, double *vs)
{
    const int asz = 1024, tsz = 512, np = 32 * 24;
    float tris[108];
    id<MTLTexture> atl = atlas_tex(asz);
    id<MTLBuffer> sb = [g_dev newBufferWithLength:sizeof(LightSet) options:MTLResourceStorageModeShared];
    id<MTLBuffer> pb = [g_dev newBufferWithLength:(NSUInteger)np * 16 options:MTLResourceStorageModeShared];
    id<MTLBuffer> ob = [g_dev newBufferWithLength:(NSUInteger)np * 8 options:MTLResourceStorageModeShared];
    id<MTLBuffer> vb;
    LightSpot *l = &g_set.l[0];
    LightShadowPlan p;
    int pass, i, j;
    box_tris(D, D + 4.0f, -0.85f, 0.85f, 0.15f, 1.40f, tris);
    vb = [g_dev newBufferWithBytes:tris length:sizeof tris options:MTLResourceStorageModeShared];
    for (pass = 0; pass < 3; pass++) {
        float sh = pass == 2 ? dx : 0.0f;
        memset(&g_set.h, 0, sizeof g_set.h);
        memset(l, 0, sizeof *l);
        l->pos[2] = hl;
        l->axis[0] = 1;
        l->side[1] = -1;
        l->kind = LIGHT_HEAD;
        l->candela = 20000;
        l->range = 150;
        l->cosout = 0.3420201f;
        l->rgb[0] = l->rgb[1] = l->rgb[2] = 1;
        light_shadow_matrix(l, &g_set.s[0]);   /* the tile's transform: the lamp where the tile was drawn */
        l->pos[0] = sh;                        /* the stale case: lamp and car have moved on since */
        g_set.s[0].rect[0] = 0.5f, g_set.s[0].rect[1] = 0.25f, g_set.s[0].rect[2] = (float)tsz / asz;
        g_set.s[0].rect[3] = pass == 0 ? 0.0f : 1.0f;
        g_set.h.atl[0] = asz, g_set.h.atl[1] = 1.0f / asz, g_set.h.eye[3] = 1;
        memcpy([sb contents], &g_set, sizeof g_set);
        memset(&p, 0, sizeof p);
        p.n = 1, p.t[0].x = asz / 2, p.t[0].y = asz / 4, p.t[0].size = tsz, p.t[0].render = 1;
        {
            float (*P)[4] = (float (*)[4])[pb contents];
            id<MTLCommandBuffer> cb = [g_q commandBuffer];
            id<MTLComputeCommandEncoder> e;
            for (j = 0; j < 24; j++)
                for (i = 0; i < 32; i++) {
                    float *q = P[j * 32 + i];
                    q[0] = D + sh, q[1] = -0.8f + 1.6f * (i + 0.5f) / 32, q[2] = 0.2f + 1.15f * (j + 0.5f) / 24, q[3] = 1;
                }
            draw_tiles(cb, atl, &p, vb, 36, 1);
            e = [cb computeCommandEncoder];
            [e setComputePipelineState:kernel_named(@"kbox")];
            [e setBuffer:sb offset:0 atIndex:0];
            [e setTexture:atl atIndex:0];
            [e setBuffer:pb offset:0 atIndex:1];
            [e setBuffer:ob offset:0 atIndex:2];
            [e dispatchThreads:MTLSizeMake((NSUInteger)np, 1, 1) threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];
            [e endEncoding];
            [cb commit];
            [cb waitUntilCompleted];
        }
        {
            const float (*o)[2] = (const float (*)[2])[ob contents];
            double se = 0, sv = 0;
            for (i = 0; i < np; i++)
                se += o[i][0], sv += o[i][0] * o[i][1];
            if (pass == 0)
                *E = se / np;
            else if (pass == 1)
                *vf = sv / (se > 0 ? se : 1);
            else
                *vs = sv / (se > 0 ? se : 1);
        }
    }
}

static void test_carbox(void)
{
    static const float dist[] = { 10.0f, 15.0f, 20.0f };
    int i;
    for (i = 0; i < 3; i++) {
        double E, vf, vs;
        box_case(dist[i], LAMP_TEST_HEIGHT, 0.5f, &E, &vf, &vs);
        printf("car box %2.0f m ahead, lamp %.2f m up: rear face %.1f lux unshadowed; lit fraction %.4f fresh tile, %.4f stale tile "
               "(lamp and car 0.5 m on)\n", dist[i], LAMP_TEST_HEIGHT, E, vf, vs);
        CHECK(vf > 0.98, "car box %.0f m: its rear shadows itself in a fresh tile (%.4f lit)", dist[i], vf);
        /* the stale tile's figure is the mechanism the plan now refuses (light_shadow_plan keeps
           a tile stale only while the moving casters near it are unchanged: cluster_test.c) */
    }
}

/* ---- the 20-car night grid ------------------------------------------------------- */
static double gpu_ms(id<MTLCommandBuffer> cb) { return (cb.GPUEndTime - cb.GPUStartTime) * 1e3; }
static int dcmp(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y; }

static void grid(void)
{
    const int W = 1920, H = 1080, reps = 41;
    Cam c;
    int ids[LC_LIGHTS], n, i, m, car;
    LightClusterStats st;
    LightShadowPlan p;
    id<MTLTexture> atl = atlas_tex(4096);
    id<MTLBuffer> sb = [g_dev newBufferWithLength:sizeof(LightSet) options:MTLResourceStorageModeShared];
    id<MTLBuffer> ub = [g_dev newBufferWithLength:sizeof(LightSet) options:MTLResourceStorageModeShared];
    id<MTLBuffer> ob = [g_dev newBufferWithLength:(NSUInteger)W * H * 16 options:MTLResourceStorageModeShared];
    id<MTLComputePipelineState> k = kernel_named(@"klit");
    float cas[20 * 12 + 2][9];
    id<MTLBuffer> vb;
    struct { float eye[4], fwd[4], right[4], up[4], tan[4]; } cu;
    double ms[4][41], tshadow[41], med[4], medsh;
    n = grid_scene(g_set.l, ids, &c, (float)W / H);
    light_cluster_build(&g_set, n, c.vp, c.eye, &st);
    light_shadow_reset();
    light_shadow_plan(&g_set, ids, &st, c.eye, NULL, 0, 4096, 1L << 40, &p);
    memcpy([sb contents], &g_set, sizeof g_set);
    for (i = 0; i < n; i++)
        g_set.s[i].rect[3] = 0;                 /* the same lights, no tiles */
    memcpy([ub contents], &g_set, sizeof g_set);
    /* the casters: each car a 4.2 x 1.8 x 1.4 m box (its lamps 10 cm inside its
       faces, as the renderer places them), and the ground */
    for (car = 0; car < 20; car++) {
        float cx = (float)((19 - car) / 2) * 8.0f, cy = car & 1 ? 2.0f : -2.0f, lo[3] = { cx - 2.1f, cy - 0.9f, 0.1f }, hi[3] = { cx + 2.1f, cy + 0.9f, 1.5f };
        static const int f[6][4] = { { 0, 1, 3, 2 }, { 4, 6, 7, 5 }, { 0, 4, 5, 1 }, { 2, 3, 7, 6 }, { 0, 2, 6, 4 }, { 1, 5, 7, 3 } };
        int q;
        for (q = 0; q < 6; q++) {
            int t2, j;
            for (t2 = 0; t2 < 2; t2++)
                for (j = 0; j < 3; j++) {
                    int cv = f[q][j == 0 ? 0 : t2 + j], a;
                    for (a = 0; a < 3; a++)
                        cas[car * 12 + q * 2 + t2][3 * j + a] = cv >> a & 1 ? hi[a] : lo[a];
                }
        }
    }
    {
        static const float g2[2][9] = { { -200, -200, 0, 400, -200, 0, 400, 200, 0 }, { -200, -200, 0, 400, 200, 0, -200, 200, 0 } };
        memcpy(cas[240], g2, sizeof g2);
    }
    vb = [g_dev newBufferWithBytes:cas length:sizeof cas options:MTLResourceStorageModeShared];
    memcpy(cu.eye, c.eye, 12);
    memcpy(cu.fwd, c.fwd, 12);
    memcpy(cu.right, c.right, 12);
    memcpy(cu.up, c.up, 12);
    cu.tan[0] = c.tx;
    cu.tan[1] = c.ty;
    cu.tan[2] = W;
    cu.tan[3] = H;
    for (i = -60; i < reps; i++) {         /* the first 60 bring the GPU up to its clock, untimed */
        id<MTLCommandBuffer> cb = [g_q commandBuffer];
        draw_tiles(cb, atl, &p, vb, 242 * 3, 1);
        [cb commit];
        [cb waitUntilCompleted];
        if (i >= 0)
            tshadow[i] = gpu_ms(cb);
        for (m = 0; m < 4; m++) {          /* clustered, every light; shadowed, not */
            uint32_t mode = m & 1;
            id<MTLComputeCommandEncoder> e;
            cb = [g_q commandBuffer];
            e = [cb computeCommandEncoder];
            [e setComputePipelineState:k];
            [e setBuffer:m < 2 ? sb : ub offset:0 atIndex:0];
            [e setTexture:atl atIndex:0];
            [e setTexture:g_be atIndex:1];
            [e setBytes:&cu length:sizeof cu atIndex:1];
            [e setBytes:&mode length:4 atIndex:2];
            [e setBuffer:ob offset:0 atIndex:3];
            [e dispatchThreads:MTLSizeMake(W, H, 1) threadsPerThreadgroup:MTLSizeMake(16, 8, 1)];
            [e endEncoding];
            [cb commit];
            [cb waitUntilCompleted];
            if (i >= 0)
                ms[m][i] = gpu_ms(cb);
        }
    }
    {   /* clustered and brute force light the same (shadowed: modes 0 and 1) */
        static float a[1920 * 1080 * 4];
        id<MTLCommandBuffer> cb;
        double maxrel = 0;
        long cnt = 0;
        for (m = 0; m < 2; m++) {
            uint32_t mode = m;
            id<MTLComputeCommandEncoder> e;
            cb = [g_q commandBuffer];
            e = [cb computeCommandEncoder];
            [e setComputePipelineState:k];
            [e setBuffer:sb offset:0 atIndex:0];
            [e setTexture:atl atIndex:0];
            [e setTexture:g_be atIndex:1];
            [e setBytes:&cu length:sizeof cu atIndex:1];
            [e setBytes:&mode length:4 atIndex:2];
            [e setBuffer:ob offset:0 atIndex:3];
            [e dispatchThreads:MTLSizeMake(W, H, 1) threadsPerThreadgroup:MTLSizeMake(16, 8, 1)];
            [e endEncoding];
            [cb commit];
            [cb waitUntilCompleted];
            if (m == 0)
                memcpy(a, [ob contents], sizeof a);
        }
        {
            const float *b = (const float *)[ob contents];
            for (i = 0; i < W * H; i++) {
                double y0 = a[4 * i] + a[4 * i + 1] + a[4 * i + 2], y1 = b[4 * i] + b[4 * i + 1] + b[4 * i + 2], rel;
                if (y1 <= 0 && y0 <= 0)
                    continue;
                rel = fabs(y0 - y1) / fmax(y1, 1e-6);
                maxrel = fmax(maxrel, rel);
                cnt++;
            }
        }
        printf("gpu grid: clustered lamps() against every light, shadowed, 1920 x 1080: %ld lit pixels, largest relative "
               "difference %.2e\n", cnt, maxrel);
        CHECK(maxrel < 1e-5, "clustered and brute-force light differ by %g", maxrel);
    }
    for (m = 0; m < 4; m++) {
        qsort(ms[m], reps, sizeof(double), dcmp);
        med[m] = ms[m][reps / 2];
    }
    qsort(tshadow, reps, sizeof(double), dcmp);
    medsh = tshadow[reps / 2];
    {
        long tex = 0;
        for (i = 0; i < p.n; i++)
            tex += (long)p.t[i].size * p.t[i].size;
        printf("gpu grid timing (%s, median of %d): lamps at 1920 x 1080: clustered + shadowed %.3f ms, every light + "
               "shadowed %.3f ms, clustered unshadowed %.3f ms, every light unshadowed %.3f ms; atlas %d tiles (%.1f Mtexels, "
               "%d casters each) %.3f ms\n",
               [[g_dev name] UTF8String], reps, med[0], med[1], med[2], med[3], p.n, tex / 1e6, 242, medsh);
    }
}

int main(void)
{
    @autoreleasepool {
        NSError *err = nil;
        MTLDepthStencilDescriptor *dd;
        g_dev = MTLCreateSystemDefaultDevice();
        if (!g_dev) {
            printf("no Metal device\n");
            return 1;
        }
        g_q = [g_dev newCommandQueue];
        g_be = [g_dev newTextureWithDescriptor:[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRG16Float
                                                                                              width:1 height:1 mipmapped:NO]];
        g_lib = [g_dev newLibraryWithSource:k_test_src options:nil error:&err];
        if (!g_lib) {
            printf("shader: %s\n", [[err description] UTF8String]);
            return 1;
        }
        g_pclr = depth_pipe(@"lcclr");
        g_pcas = depth_pipe(@"lcvs");
        dd = [MTLDepthStencilDescriptor new];
        dd.depthCompareFunction = MTLCompareFunctionAlways;
        dd.depthWriteEnabled = YES;
        g_dsall = [g_dev newDepthStencilStateWithDescriptor:dd];
        dd = [MTLDepthStencilDescriptor new];
        dd.depthCompareFunction = MTLCompareFunctionLessEqual;
        dd.depthWriteEnabled = YES;
        g_dsless = [g_dev newDepthStencilStateWithDescriptor:dd];
        {   /* the light set's layout: the shader's against the C structs */
            id<MTLBuffer> o = [g_dev newBufferWithLength:16 options:MTLResourceStorageModeShared];
            id<MTLCommandBuffer> cb = [g_q commandBuffer];
            id<MTLComputeCommandEncoder> e = [cb computeCommandEncoder];
            const uint32_t *z = (const uint32_t *)[o contents];
            [e setComputePipelineState:kernel_named(@"ksz")];
            [e setBuffer:o offset:0 atIndex:0];
            [e dispatchThreads:MTLSizeMake(1, 1, 1) threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
            [e endEncoding];
            [cb commit];
            [cb waitUntilCompleted];
            printf("gpu layout: LSet %u bytes (C %zu), LS %u (%zu), LCH %u (%zu), LSH %u (%zu)\n", z[0], sizeof(LightSet), z[1],
                   sizeof(LightSpot), z[2], sizeof(LightClusterHdr), z[3], sizeof(LightShadowRec));
            CHECK(z[0] == sizeof(LightSet) && z[1] == sizeof(LightSpot) && z[2] == sizeof(LightClusterHdr) && z[3] == sizeof(LightShadowRec),
                  "the light set's layout differs between the shader and C");
        }
        test_clusters();
        test_shadow();
        test_shadow_penumbra();
        test_carbox();
        grid();
    }
    printf(g_fail ? "metal_test: %d FAILED\n" : "metal_test: all passed\n", g_fail);
    return g_fail != 0;
}
