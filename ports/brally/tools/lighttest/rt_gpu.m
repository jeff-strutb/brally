/* rt_gpu.m: the GPU's ray tracing (brr_metal_rt.h, brr_metal_rt.m) against
 * the CPU's (light_bvh.c), which bvh_test.c holds to brute force.  Not the
 * game: the same scenes and rays as bvh_test (lt_scene.h), the world built
 * by the renderer's own brr_rt_update, every ray traced by
 *
 *   rt_closest, rt_any        the compute BVH in shader code: expected to
 *                             agree with the CPU bit for bit (distance,
 *                             weights, triangle)
 *   rt_hw_closest, rt_hw_any  Metal's intersector over the acceleration
 *                             structure, where supported: its own
 *                             arithmetic, so the same triangle with the hit
 *                             points compared in units in the last place of
 *                             their largest coordinate, or a tie (bvh_test.c's:
 *                             another triangle hit at the same point to the
 *                             coordinates' precision)
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
"}\n"
"#if BRR_RT_HW\n"
"kernel void k_hw_closest(device const LtRay *ray [[buffer(0)]], device RtHit *out [[buffer(1)]],\n"
"                         constant uint &n [[buffer(2)]],\n"
"                         raytracing::primitive_acceleration_structure as [[buffer(8)]],\n"
"                         uint i [[thread_position_in_grid]]) {\n"
"  if (i >= n) return;\n"
"  LtRay r = ray[i];\n"
"  out[i] = rt_hw_closest(as, float3(r.o), float3(r.d), r.tmin, r.tmax);\n"
"}\n"
"kernel void k_hw_any(device const LtRay *ray [[buffer(0)]], device uint *out [[buffer(1)]],\n"
"                     constant uint &n [[buffer(2)]], device const float *tmax [[buffer(3)]],\n"
"                     raytracing::primitive_acceleration_structure as [[buffer(8)]],\n"
"                     uint i [[thread_position_in_grid]]) {\n"
"  if (i >= n) return;\n"
"  LtRay r = ray[i];\n"
"  out[i] = uint(rt_hw_trace(as, float3(r.o), float3(r.d), r.tmin, tmax[i], true).tri + 1);   /* rt_hw_any, with its triangle */\n"
"}\n"
"#endif\n";

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
        if (w->as)
            [e setAccelerationStructure:w->as atBufferIndex:8];
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

/* bvh_test.c's tie: both triangles hit by the CPU's test, the points within
 * a unit in the last place of the hit's largest coordinate */
static int tie(const LtScene *s, const LtRay *r, int ta, float t_a, int tb, float t_b)
{
    float t, u, v, p, m = 0.0f, lo = t_a < t_b ? t_a : t_b;
    double dist;
    int k;
    if (!light_tri_hit(r->o, r->d, s->tri + 9 * ta, r->tmin, INFINITY, &t, &u, &v))
        return 0;
    if (!light_tri_hit(r->o, r->d, s->tri + 9 * tb, r->tmin, INFINITY, &t, &u, &v))
        return 0;
    for (k = 0; k < 3; k++) {
        p = fabsf(r->o[k] + lo * r->d[k]);
        if (p > m)
            m = p;
    }
    dist = fabs((double)t_a - t_b) * sqrt((double)r->d[0] * r->d[0] + (double)r->d[1] * r->d[1] + (double)r->d[2] * r->d[2]);
    return dist <= (double)(nextafterf(m, INFINITY) - m);
}

static double coord_ulps(const LtRay *r, float ta, float tb)
{
    float p, m = 0.0f;
    int k;
    for (k = 0; k < 3; k++) {
        p = fabsf(r->o[k] + tb * r->d[k]);
        if (p > m)
            m = p;
    }
    return fabs((double)ta - tb) * sqrt((double)r->d[0] * r->d[0] + (double)r->d[1] * r->d[1] + (double)r->d[2] * r->d[2]) /
           (double)(nextafterf(m, INFINITY) - m);
}

/* where the hardware and the CPU part, the exact answer (long double) for a
 * triangle: its distance, its smallest barycentric weight, and how thin it
 * is (its height over its longest side) */
static void exact(const LtScene *s, const LtRay *r, int tri, long double *t, long double *bmin, long double *thin)
{
    const float *v = s->tri + 9 * tri;
    long double e1[3], e2[3], e3[3], n[3], p[3], dn, nn, d00, d01, d11, d20, d21, den, b1, b2, l, lmax = 0.0L;
    int k;
    for (k = 0; k < 3; k++) {
        e1[k] = (long double)v[3 + k] - v[k];
        e2[k] = (long double)v[6 + k] - v[k];
        e3[k] = (long double)v[6 + k] - v[3 + k];
    }
    n[0] = e1[1] * e2[2] - e1[2] * e2[1];
    n[1] = e1[2] * e2[0] - e1[0] * e2[2];
    n[2] = e1[0] * e2[1] - e1[1] * e2[0];
    nn = sqrtl(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    dn = n[0] * r->d[0] + n[1] * r->d[1] + n[2] * r->d[2];
    *t = dn != 0.0L ? (n[0] * (v[0] - (long double)r->o[0]) + n[1] * (v[1] - (long double)r->o[1]) +
                       n[2] * (v[2] - (long double)r->o[2])) / dn : INFINITY;
    for (k = 0; k < 3; k++)
        p[k] = r->o[k] + *t * r->d[k] - v[k];
    d00 = e1[0] * e1[0] + e1[1] * e1[1] + e1[2] * e1[2];
    d01 = e1[0] * e2[0] + e1[1] * e2[1] + e1[2] * e2[2];
    d11 = e2[0] * e2[0] + e2[1] * e2[1] + e2[2] * e2[2];
    d20 = p[0] * e1[0] + p[1] * e1[1] + p[2] * e1[2];
    d21 = p[0] * e2[0] + p[1] * e2[1] + p[2] * e2[2];
    den = d00 * d11 - d01 * d01;
    b1 = den != 0.0L ? (d11 * d20 - d01 * d21) / den : 0.0L;
    b2 = den != 0.0L ? (d00 * d21 - d01 * d20) / den : 0.0L;
    *bmin = fminl(1.0L - b1 - b2, fminl(b1, b2));
    l = d00; if (l > lmax) lmax = l;
    l = d11; if (l > lmax) lmax = l;
    l = e3[0] * e3[0] + e3[1] * e3[1] + e3[2] * e3[2]; if (l > lmax) lmax = l;
    *thin = lmax > 0.0L ? nn / lmax : 0.0L;      /* 2 area / longest^2 = height / longest */
}

/* why the hardware and the CPU disagree on a ray, from the exact answers of
 * the triangles involved (either side's); NULL when nothing explains it.
 *   segment end  the hit is at the segment's end, which the intersector
 *                includes and the CPU's test excludes
 *   origin       the origin lies on a surface (tmin 0, a hit within a unit of
 *                the coordinates): t = 0 is either side's choice
 *   graze        the ray passes within 1e-6 (barycentric) of an edge the
 *                triangle shares with no other: inside or out is rounding
 *   sliver       a triangle 10,000 times longer than it is high, where the
 *                intersector's distance is off by more than the hit's margin
 *                (the exact hit is outside tmin..tmax) */
static const char *why(const LtScene *s, const LtRay *r, const int *tri, int ntri, float tmax, float tcpu)
{
    float p, m = 0.0f;
    double unit;
    int k, j;
    for (k = 0; k < 3; k++) {
        p = fabsf(r->o[k]);
        if (p > m)
            m = p;
    }
    unit = (double)(nextafterf(m, INFINITY) - m) /
           sqrt((double)r->d[0] * r->d[0] + (double)r->d[1] * r->d[1] + (double)r->d[2] * r->d[2]);
    if (tcpu < INFINITY && tcpu == tmax)
        return "segment end";
    for (j = 0; j < ntri; j++) {
        long double t, bmin, thin;
        if (tri[j] < 0)
            continue;
        exact(s, r, tri[j], &t, &bmin, &thin);
        if (r->tmin == 0.0f && fabsl(t) <= unit)
            return "origin";
        if (fabsl(bmin) <= 1e-6L)
            return "graze";
        if (thin < 1e-4L && (t <= r->tmin || t >= tmax))
            return "sliver";
    }
    return NULL;
}

static void tally(const char **names, long *counts, const char *w)
{
    int k;
    for (k = 0; names[k]; k++)
        if (!strcmp(names[k], w)) {
            counts[k]++;
            return;
        }
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
    printf("\n%s: %d triangles; BVH %.2f ms, %.0f KB; acceleration structure %s%.2f ms, %.0f KB (compacted)\n",
           s->name, s->ntri, w.bvh_ms, w.bvh_bytes / 1024.0, w.as ? "" : "none ", w.as_ms, w.as_bytes / 1024.0);
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
        long same = 0, ties = 0, bad = 0, abad = 0, hits = 0, occl = 0;
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
        if (w.as) {
            double maxrel = 0.0;
            long lostsame = 0, unexpl = 0, cnt[8] = { 0 };
            static const char *names[] = { "segment end", "origin", "graze", "sliver", NULL };
            tc = run(@"k_hw_closest", &w, rb, hb, nil, (uint32_t)nray, 5);
            ta = run(@"k_hw_any", &w, rb, ab, tb, (uint32_t)nray, 5);
            same = ties = bad = abad = 0;
            for (i = 0; i < nray; i++) {
                if (gh[i].tri == ch[i].tri) {
                    if (ch[i].tri >= 0) {
                        /* the two hit points apart, in units in the last place of the
                         * point's largest coordinate */
                        double u = coord_ulps(&r[i], gh[i].t, ch[i].t);
                        if (u > maxrel)
                            maxrel = u;
                        if (u > 1.0)
                            lostsame++;
                    }
                    same++;
                } else if (gh[i].tri >= 0 && ch[i].tri >= 0 && tie(s, &r[i], gh[i].tri, gh[i].t, ch[i].tri, ch[i].t)) {
                    ties++;
                } else {
                    int tt[2] = { gh[i].tri, ch[i].tri };
                    const char *y = why(s, &r[i], tt, 2, r[i].tmax, INFINITY);
                    if (y)
                        tally(names, cnt, y);
                    else {
                        printf("  HW DIFF ray %d: hw tri %d t %.9g, cpu tri %d t %.9g\n", i, gh[i].tri, gh[i].t, ch[i].tri, ch[i].t);
                        unexpl++;
                    }
                    bad++;
                }
                if ((ga[i] != 0) != (ca[i] != 0)) {
                    int tt[2] = { (int)ga[i] - 1, ca[i] ? ch[i].tri : -1 };
                    const char *y = why(s, &r[i], tt, 2, tm[i], ch[i].tri >= 0 ? ch[i].t : INFINITY);
                    if (y)
                        tally(names, cnt, y);
                    else {
                        printf("  HW ANY DIFF ray %d: hw tri %d, cpu %u (closest tri %d t %.9g, segment %.9g)\n", i,
                               (int)ga[i] - 1, ca[i], ch[i].tri, ch[i].t, tm[i]);
                        unexpl++;
                    }
                    abad++;
                }
            }
            printf("    hardware: closest %ld same triangle (%ld more than a coordinate unit apart, %.2g units at most), %ld ties, %ld differ; any-hit %ld differ\n",
                   same, lostsame, maxrel, ties, bad, abad);
            if (bad || abad)
                printf("    hardware differences: %ld at a segment's end, %ld origins on a surface, %ld grazing an edge, %ld on slivers, %ld unexplained\n",
                       cnt[0], cnt[1], cnt[2], cnt[3], unexpl);
            if (unexpl)
                exit(1);
            printf("    hardware: %.1f Mrays/s closest, %.1f Mrays/s any (GPU time %.2f ms, %.2f ms)\n",
                   nray / tc * 1e-6, nray / ta * 1e-6, tc * 1e3, ta * 1e3);
        }
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
