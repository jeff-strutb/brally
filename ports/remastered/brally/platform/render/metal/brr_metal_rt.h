/* brr_metal_rt.h: rays against the whole track for the Remastered lighting
 * (DESIGN.md 8.1), for the irradiance probes' bake when a track loads
 * (brr_metal_gi.h) and the diagnostics; nothing traces while the race runs.
 * light_bvh.c's tree, built once a track generation (brr_metal_rt.m) and
 * traversed in shader code by rt_closest and rt_any (BRR_RT_SRC), on every
 * GPU.  A hit reports the triangle by its number in the track (the surface
 * records' index) and the barycentric weights of its second and third
 * corners.  The compute traversal is light_bvh.c's operation for operation,
 * with the multiply-adds written as fma on both sides, so that a library
 * compiled without fast math (brr_rt_library) agrees with the CPU bit for
 * bit.  Fast math is not allowed: the slabs need IEEE infinities for rays
 * parallel to an axis.  The hardware intersector and its acceleration
 * structure are gone (2026-10-14: no ray tracing while racing, every device
 * alike). */
#ifndef BRR_METAL_RT_H
#define BRR_METAL_RT_H

/* buffers, as brr_rt_bind binds them from its base index:
 *   base + 0  RtNode node[]     32 bytes, depth first (light_bvh.h)
 *   base + 1  float tri[]       9 a triangle, leaf order
 *   base + 2  uint id[]         leaf order -> track order
 *   base + 3  RtPrim prim[]     surface records, track order */
#define BRR_RT_SRC \
"struct RtNode { packed_float3 lo; int a; packed_float3 hi; int n; };\n" \
"struct RtPrim { int obj; uint mat; uint albedo; uint emit; };\n" \
"struct RtHit { float t; float u; float v; int tri; };\n" \
"struct RtRay { float3 o; float3 d; float3 inv; int kx; int ky; int kz; float sx; float sy; float sz; };\n" \
/* the ray, prepared once: its reciprocal for the slabs, its longest axis and \
   the shear onto it for the triangles (Woop, Benthin and Wald 2013) */ \
"static RtRay rt_ray(float3 o, float3 d) {\n" \
"  RtRay r; r.o = o; r.d = d; r.inv = 1.0 / d;\n" \
"  float ax = abs(d.x), ay = abs(d.y), az = abs(d.z);\n" \
"  r.kz = ax > ay ? (ax > az ? 0 : 2) : (ay > az ? 1 : 2);\n" \
"  r.kx = r.kz == 2 ? 0 : r.kz + 1; r.ky = r.kx == 2 ? 0 : r.kx + 1;\n" \
"  if (d[r.kz] < 0.0) { int k = r.kx; r.kx = r.ky; r.ky = k; }\n" \
"  r.sz = 1.0 / d[r.kz]; r.sx = d[r.kx] * r.sz; r.sy = d[r.ky] * r.sz;\n" \
"  return r;\n" \
"}\n" \
/* a * b - c * d to two units in the last place (Kahan): the exact sign */ \
"static float rt_dp(float a, float b, float c, float d) {\n" \
"  float w = c * d; float e = fma(-c, d, w); float f = fma(a, b, -w); return f + e;\n" \
"}\n" \
/* watertight: hit at tmin < t < tmax, the weights of corners 2 and 3 */ \
"static bool rt_tri(thread const RtRay &r, float3 p0, float3 p1, float3 p2, float tmin, float tmax,\n" \
"                   thread float &t, thread float &u, thread float &v) {\n" \
"  float3 A = p0 - r.o, B = p1 - r.o, C = p2 - r.o;\n" \
"  float Ax = fma(-r.sx, A[r.kz], A[r.kx]), Ay = fma(-r.sy, A[r.kz], A[r.ky]);\n" \
"  float Bx = fma(-r.sx, B[r.kz], B[r.kx]), By = fma(-r.sy, B[r.kz], B[r.ky]);\n" \
"  float Cx = fma(-r.sx, C[r.kz], C[r.kx]), Cy = fma(-r.sy, C[r.kz], C[r.ky]);\n" \
"  float U = rt_dp(Cx, By, Cy, Bx), V = rt_dp(Ax, Cy, Ay, Cx), W = rt_dp(Bx, Ay, By, Ax);\n" \
"  if ((U < 0.0 || V < 0.0 || W < 0.0) && (U > 0.0 || V > 0.0 || W > 0.0)) return false;\n" \
"  float det = (U + V) + W;\n" \
"  if (det == 0.0) return false;\n" \
"  float Az = r.sz * A[r.kz], Bz = r.sz * B[r.kz], Cz = r.sz * C[r.kz];\n" \
"  float T = fma(U, Az, fma(V, Bz, W * Cz));\n" \
"  float rcp = 1.0 / det, tt = T * rcp;\n" \
"  if (!(tt > tmin && tt < tmax)) return false;\n" \
"  t = tt; u = V * rcp; v = W * rcp;\n" \
"  return true;\n" \
"}\n" \
/* the slabs, the exit rounded outward (1 + 2 gamma(3), Ize 2013); NaN (an \
   origin on the slab of a ray parallel to it) leaves the interval as it was */ \
"static bool rt_box(thread const RtRay &r, device const RtNode &nd, float tmin, float tmax, thread float &tn) {\n" \
"  float3 a = (float3(nd.lo) - r.o) * r.inv, b = (float3(nd.hi) - r.o) * r.inv;\n" \
"  float t0 = tmin, t1 = tmax;\n" \
"  for (int k = 0; k < 3; k++) {\n" \
"    float n = a[k] < b[k] ? a[k] : b[k], f = a[k] < b[k] ? b[k] : a[k];\n" \
"    f = f * 1.0000004;\n" \
"    t0 = n > t0 ? n : t0; t1 = f < t1 ? f : t1;\n" \
"  }\n" \
"  tn = t0; return t0 <= t1;\n" \
"}\n" \
"#define RT_STACK 32\n" \
/* the closest (any == false) or any hit with tmin < t < tmax: depth first, \
   the nearer child first, the farther stacked with where it is entered */ \
"static RtHit rt_trace(device const RtNode *node, device const float *tri, device const uint *id,\n" \
"                      float3 o, float3 d, float tmin, float tmax, bool any) {\n" \
"  RtRay r = rt_ray(o, d);\n" \
"  RtHit h; h.t = tmax; h.u = 0.0; h.v = 0.0; h.tri = -1;\n" \
"  int stk[RT_STACK]; float stt[RT_STACK]; int sp = 0, i = 0, hit = -1;\n" \
"  float tn;\n" \
"  device const packed_float3 *tv = (device const packed_float3 *)tri;\n" \
"  if (!rt_box(r, node[0], tmin, h.t, tn)) return h;\n" \
"  for (;;) {\n" \
"    RtNode nd = node[i];\n" \
"    if (nd.n) {\n" \
"      for (int j = nd.a; j < nd.a + nd.n; j++) {\n" \
"        float t, u, v;\n" \
"        if (rt_tri(r, float3(tv[3 * j]), float3(tv[3 * j + 1]), float3(tv[3 * j + 2]), tmin, h.t, t, u, v)) {\n" \
"          h.t = t; h.u = u; h.v = v; hit = j;\n" \
"          if (any) { h.tri = int(id[hit]); return h; }\n" \
"        }\n" \
"      }\n" \
"    } else {\n" \
"      float tl, tr; int l = i + 1, rr = nd.a;\n" \
"      bool hl = rt_box(r, node[l], tmin, h.t, tl), hr = rt_box(r, node[rr], tmin, h.t, tr);\n" \
"      if (hl && hr) {\n" \
"        bool ln = tl <= tr;\n" \
"        stk[sp] = ln ? rr : l; stt[sp++] = ln ? tr : tl; i = ln ? l : rr; continue;\n" \
"      }\n" \
"      if (hl) { i = l; continue; }\n" \
"      if (hr) { i = rr; continue; }\n" \
"    }\n" \
"    for (;;) {\n" \
"      if (!sp) { if (hit >= 0) h.tri = int(id[hit]); return h; }\n" \
"      sp--; if (stt[sp] <= h.t) break;\n" \
"    }\n" \
"    i = stk[sp];\n" \
"  }\n" \
"}\n" \
"static RtHit rt_closest(device const RtNode *node, device const float *tri, device const uint *id,\n" \
"                        float3 o, float3 d, float tmin, float tmax) {\n" \
"  return rt_trace(node, tri, id, o, d, tmin, tmax, false);\n" \
"}\n" \
"static bool rt_any(device const RtNode *node, device const float *tri, device const uint *id,\n" \
"                   float3 o, float3 d, float tmin, float tmax) {\n" \
"  return rt_trace(node, tri, id, o, d, tmin, tmax, true).tri >= 0;\n" \
"}\n"

/* the trace the kernels call: the compute BVH, its buffers named node, tri
 * and id in the kernel (brr_rt_bind's base .. base + 2) */
#define BRR_RT_SEL_SRC \
"#define RT_AS(n)\n" \
"#define RT_CLOSEST(o, d, t0, t1) rt_closest(node, tri, id, o, d, t0, t1)\n" \
"#define RT_ANY(o, d, t0, t1) rt_any(node, tri, id, o, d, t0, t1)\n"

#ifdef __OBJC__
#import <Metal/Metal.h>
#include "light.h"
#include "light_bvh.h"

/* the track's ray-tracing world on the GPU (brr_metal_rt.m) */
typedef struct BrrRtWorld {
    LightBvh bvh;                        /* the CPU's copy (its traversals: light_bvh.h) */
    id<MTLBuffer> node, tri, id, prim;   /* the compute BVH: BRR_RT_SRC's buffers */
    id<MTLBuffer> leaf;                  /* the track's triangle -> its leaf slot (tri's corners for a hit) */
    id<MTLBuffer> acc;                   /* the track's albedo learned from the frames: its sums, 4 uints a triangle (brr_metal.m) */
    int ntri;
    unsigned gen;            /* the track generation it was built from */
    int built;
    double bvh_ms;           /* the CPU's build */
    size_t bvh_bytes;
} BrrRtWorld;

/* (re)builds w when the track's generation differs from the one it holds;
 * 1 when w holds a world afterwards */
int  brr_rt_update(BrrRtWorld *w, id<MTLDevice> dev, id<MTLCommandQueue> q, const LightTrack *t);
void brr_rt_release(BrrRtWorld *w);
/* binds the compute BVH's four buffers at base .. base + 3 */
void brr_rt_bind(const BrrRtWorld *w, id<MTLComputeCommandEncoder> e, int base);
/* a library of BRR_RT_SRC and BRR_RT_SEL_SRC followed by src, compiled
 * without fast math */
id<MTLLibrary> brr_rt_library(id<MTLDevice> dev, NSString *src, NSError **err);
/* the renderer's world for the current track (brr_metal.m), built on first
 * use; NULL without a track */
const BrrRtWorld *brr_metal_rt_world(void);
#endif

#endif
