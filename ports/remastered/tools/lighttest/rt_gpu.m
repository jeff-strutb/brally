/* rt_gpu.m: the GPU's ray tracing (brr_metal_rt.h, brr_metal_rt.m) against
 * the CPU's (light_bvh.c), which bvh_test.c holds to brute force.  Not the
 * game: the same scenes and rays as bvh_test (lt_scene.h), the world built
 * by the renderer's own brr_rt_update, every ray traced by
 *
 *   rt_closest, rt_any        the compute BVH in shader code: expected to
 *                             agree with the CPU bit for bit (distance,
 *                             weights, triangle)
 *
 * and timed on the GPU (the command buffer's GPU start and end, best of 5).
 * Usage: rt_gpu [track.obj] [rays per set]   (run.sh) */
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <dispatch/dispatch.h>
#include "brr_metal_rt.h"
#include "lt_scene.h"

static const char *k_kern =
"struct LtRay { packed_float3 o; float tmin; packed_float3 d; float tmax; };\n"
"kernel void k_closest(device const LtRay *ray [[buffer(0)]], device RtHit *out [[buffer(1)]],\n"
"                      constant uint &n [[buffer(2)]], device const RtNode *node [[buffer(4)]],\n"
"                      device const float *tri [[buffer(5)]], device const uint *id [[buffer(6)]],\n"
"                      uint i [[thread_position_in_grid]]) {\n"
"  if (i >= n) return;\n"
"  LtRay r = ray[i];\n"
"  out[i] = rt_closest(node, tri, id, float3(r.o), float3(r.d), r.tmin, r.tmax);\n"
"}\n"
"kernel void k_any(device const LtRay *ray [[buffer(0)]], device uint *out [[buffer(1)]],\n"
"                  constant uint &n [[buffer(2)]], device const float *tmax [[buffer(3)]],\n"
"                  device const RtNode *node [[buffer(4)]], device const float *tri [[buffer(5)]],\n"
"                  device const uint *id [[buffer(6)]], uint i [[thread_position_in_grid]]) {\n"
"  if (i >= n) return;\n"
"  LtRay r = ray[i];\n"
"  out[i] = rt_any(node, tri, id, float3(r.o), float3(r.d), r.tmin, tmax[i]) ? 1 : 0;\n"
"}\n";

static id<MTLDevice> s_dev;
static id<MTLCommandQueue> s_q;
static id<MTLLibrary> s_lib;

/* runs kernel name over n threads; the best GPU time of reps, seconds */
static double run(NSString *name, const BrrRtWorld *w, id<MTLBuffer> rays, id<MTLBuffer> out, id<MTLBuffer> tmax,
                  uint32_t n, int reps)
{
    NSError *err = nil;
    id<MTLFunction> f = [s_lib newFunctionWithName:name];
    id<MTLComputePipelineState> p = [s_dev newComputePipelineStateWithFunction:f error:&err];
    double best = 1e9;
    int k;
    if (!p) {
        fprintf(stderr, "rt_gpu: %s: %s\n", [name UTF8String], [[err description] UTF8String]);
        exit(1);
    }
    for (k = 0; k < reps; k++) {
        id<MTLCommandBuffer> cb = [s_q commandBuffer];
        id<MTLComputeCommandEncoder> e = [cb computeCommandEncoder];
        [e setComputePipelineState:p];
        [e setBuffer:rays offset:0 atIndex:0];
        [e setBuffer:out offset:0 atIndex:1];
        [e setBytes:&n length:sizeof n atIndex:2];
        if (tmax)
            [e setBuffer:tmax offset:0 atIndex:3];
        brr_rt_bind(w, e, 4);
        [e dispatchThreads:MTLSizeMake(n, 1, 1) threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];
        [e endEncoding];
        [cb commit];
        [cb waitUntilCompleted];
        if (cb.status != MTLCommandBufferStatusCompleted) {
            fprintf(stderr, "rt_gpu: %s failed: %s\n", [name UTF8String], [[cb.error description] UTF8String]);
            exit(1);
        }
        if (cb.GPUEndTime - cb.GPUStartTime < best)
            best = cb.GPUEndTime - cb.GPUStartTime;
    }
    return best;
}

static void run_scene(const LtScene *s, int nray)
{
    static BrrRtWorld w;     /* released at the end of each scene */
    LightTrack t;
    int set;
    memset(&t, 0, sizeof t);
    t.tri = s->tri;
    t.obj = s->obj;
    t.ntri = s->ntri;
    t.gen = 1;
    if (!brr_rt_update(&w, s_dev, s_q, &t)) {
        printf("%s: brr_rt_update failed\n", s->name);
        exit(1);
    }
    printf("\n%s: %d triangles; BVH %.2f ms, %.0f KB\n", s->name, s->ntri, w.bvh_ms, w.bvh_bytes / 1024.0);
    for (set = 0; set < 2; set++) {
        LtRay *r = lt_rays(s, set, nray, 0xC0FFEEull + (uint64_t)set * 7919u);
        LightHit *ch = malloc((size_t)nray * sizeof *ch);
        uint8_t *ca = malloc((size_t)nray);
        float *tm = malloc((size_t)nray * sizeof *tm);
        id<MTLBuffer> rb = [s_dev newBufferWithBytes:r length:(size_t)nray * sizeof *r options:MTLResourceStorageModeShared];
        id<MTLBuffer> hb = [s_dev newBufferWithLength:(size_t)nray * sizeof(LightHit) options:MTLResourceStorageModeShared];
        id<MTLBuffer> ab = [s_dev newBufferWithLength:(size_t)nray * 4 options:MTLResourceStorageModeShared];
        id<MTLBuffer> tb;
        const LightHit *gh;
        const uint32_t *ga;
        long same = 0, bad = 0, abad = 0, hits = 0, occl = 0;
        double tc, ta;
        int i;

        dispatch_apply((size_t)(nray + 4095) / 4096, dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^(size_t c) {
            int j;
            for (j = (int)c * 4096; j < (int)c * 4096 + 4096 && j < nray; j++) {
                light_bvh_closest(&w.bvh, r[j].o, r[j].d, r[j].tmin, r[j].tmax, &ch[j], NULL);
                tm[j] = lt_shadow_tmax(s, j, ch[j].tri >= 0 ? ch[j].t : INFINITY);
                ca[j] = (uint8_t)light_bvh_any(&w.bvh, r[j].o, r[j].d, r[j].tmin, tm[j], NULL);
            }
        });
        for (i = 0; i < nray; i++) {
            hits += ch[i].tri >= 0;
            occl += ca[i];
        }
        tb = [s_dev newBufferWithBytes:tm length:(size_t)nray * sizeof *tm options:MTLResourceStorageModeShared];

        tc = run(@"k_closest", &w, rb, hb, nil, (uint32_t)nray, 5);
        ta = run(@"k_any", &w, rb, ab, tb, (uint32_t)nray, 5);
        gh = [hb contents];
        ga = [ab contents];
        for (i = 0; i < nray; i++) {
            if (!memcmp(&gh[i], &ch[i], sizeof ch[i]))
                same++;
            else {
                if (bad < 5)
                    printf("  DIFF ray %d: gpu tri %d t %.9g u %.9g v %.9g, cpu tri %d t %.9g u %.9g v %.9g\n", i,
                           gh[i].tri, gh[i].t, gh[i].u, gh[i].v, ch[i].tri, ch[i].t, ch[i].u, ch[i].v);
                bad++;
            }
            if ((ga[i] != 0) != (ca[i] != 0))
                abad++;
        }
        printf("  %s rays: %d (%ld hit; any-hit segments %ld occluded)\n", set ? "surface (GI)" : "random", nray, hits, occl);
        printf("    compute BVH: closest %ld of %d identical to the CPU (distance, weights, triangle), %ld differ; any-hit %ld differ\n",
               same, nray, bad, abad);
        printf("    compute BVH: %.1f Mrays/s closest, %.1f Mrays/s any (GPU time %.2f ms, %.2f ms)\n",
               nray / tc * 1e-6, nray / ta * 1e-6, tc * 1e3, ta * 1e3);
        if (bad || abad)
            exit(1);
        free(r); free(ch); free(ca); free(tm);
    }
    brr_rt_release(&w);
}

int main(int argc, char **argv)
{
    @autoreleasepool {
        int nray = argc > 2 ? atoi(argv[2]) : 1000000;
        NSError *err = nil;
        LtScene s;
        s_dev = MTLCreateSystemDefaultDevice();
        s_q = [s_dev newCommandQueue];
        s_lib = brr_rt_library(s_dev, [NSString stringWithUTF8String:k_kern], &err);
        if (!s_lib) {
            fprintf(stderr, "rt_gpu: %s\n", [[err description] UTF8String]);
            return 1;
        }
        printf("%s: supportsRaytracing %d, Apple 9 %d\n", [[s_dev name] UTF8String], s_dev.supportsRaytracing,
               [s_dev supportsFamily:MTLGPUFamilyApple9]);
        if (argc > 1 && argv[1][0]) {
            if (!lt_load_obj(&s, argv[1])) {
                fprintf(stderr, "rt_gpu: cannot read %s\n", argv[1]);
                return 1;
            }
            run_scene(&s, nray);
        }
        lt_soup(&s);
        run_scene(&s, nray);
        lt_terrain(&s);
        run_scene(&s, nray);
        lt_rooms(&s);
        run_scene(&s, nray);
        printf("\nall passed\n");
    }
    return 0;
}
