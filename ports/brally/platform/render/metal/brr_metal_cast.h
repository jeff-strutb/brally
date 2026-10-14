/* brr_metal_cast.h: the sun's and the lamps' moving casters on the GPU, each
 * frame (the CPU only places the cars' boxes).
 *
 *   cast_pick  one thread a triangle of the frame's opaque 3D (op k's n
 *              vertices at byte voff[k], triangles from base[k], a prefix
 *              searched per thread, as brr_metal_smooth.h): a triangle that
 *              lies on the track (every corner within CAST_ON of one track
 *              triangle of the grid cell holding its centroid, the track's
 *              hash of light_capture_trackhash) is the static cache's
 *              already; the rest, the moving casters, are appended to dyn
 *              (args[0] their vertex count, the indirect draw's), and widen
 *              each cascade's rectangle of texels (rect[4 c], min x y, max
 *              x y, a texel out each side), as brr_shadow.h brr_shc_rect
 *              does on the CPU
 *   rvs        the restore's quad: cascade k.x's rectangle from last frame's
 *              rect (clamped to the map), so the live map is restored from
 *              the cache only where last frame's movers were, the vertex
 *              stage reading where on the GPU; drawn with scopy
 *
 *   cast_tiles one threadgroup a lamp's shadow tile drawn this frame (tile[k],
 *              its light, ~0u for none): the track grid's cells under the
 *              lamp's square that hold triangles and meet its frustum
 *              (light_cluster.c light_shadow_track_casters, its reference),
 *              counted, a run of the index buffer taken (used, an atomic),
 *              then their triangles' vertex numbers written there; the
 *              tile's indexed indirect draw in args[5 k] (count, 1, first)
 *
 * The CPU reference is light_capture_ontrack, brr_shc_rect and
 * light_shadow_track_casters (BR_LIGHT_CASTCHECK compares them on the live
 * frame). */
#ifndef BRR_METAL_CAST_H
#define BRR_METAL_CAST_H

#include "light.h"
#include "light_cluster.h"

#define CAST_OPS_MAX 4096
#define CAST_TILE_THREADS 256

/* cast_tiles' constants */
typedef struct { float lo[2]; int nx, ny; uint32_t cap, pad[3]; } CastGrid;

#define BRR_CAST_STR_(x) #x
#define BRR_CAST_STR(x) BRR_CAST_STR_(x)
#define BRR_CAST_SRC \
"#define CAST_CELL " BRR_CAST_STR(LIGHT_TC_CELL) "\n" \
"#define CAST_HASH " BRR_CAST_STR(LIGHT_TC_HASH) "u\n" \
"#define CAST_ON 0.05\n" \
"struct CU { float4x4 cm[4]; uint nops; uint ntri; uint cap; float size; };\n" \
"static uint cast_op(const device uint *base, uint nops, uint t) {\n" \
"  uint lo = 0u, hi = nops - 1u;\n" \
"  while (lo < hi) { uint m = (lo + hi + 1u) >> 1; if (base[m] <= t) lo = m; else hi = m - 1u; }\n" \
"  return lo; }\n" \
"static uint cast_key(int3 q) { return (uint(q.x) * 73856093u ^ uint(q.y) * 19349663u ^ uint(q.z) * 83492791u) & (CAST_HASH - 1u); }\n" \
/* the distance from p to triangle abc (Ericson 5.1.5; light_capture.c tc_dist) */ \
"static float cast_dist(float3 p, float3 a, float3 b, float3 c) {\n" \
"  float3 ab = b - a, ac = c - a, ap = p - a; float d1 = dot(ab, ap), d2 = dot(ac, ap);\n" \
"  if (d1 <= 0.0 && d2 <= 0.0) return distance(p, a);\n" \
"  float3 bp = p - b; float d3 = dot(ab, bp), d4 = dot(ac, bp);\n" \
"  if (d3 >= 0.0 && d4 <= d3) return distance(p, b);\n" \
"  float vc = d1 * d4 - d3 * d2;\n" \
"  if (vc <= 0.0 && d1 >= 0.0 && d3 <= 0.0) return distance(p, a + ab * (d1 / (d1 - d3)));\n" \
"  float3 cp = p - c; float d5 = dot(ab, cp), d6 = dot(ac, cp);\n" \
"  if (d6 >= 0.0 && d5 <= d6) return distance(p, c);\n" \
"  float vb = d5 * d2 - d1 * d6;\n" \
"  if (vb <= 0.0 && d2 >= 0.0 && d6 <= 0.0) return distance(p, a + ac * (d2 / (d2 - d6)));\n" \
"  float va = d3 * d6 - d5 * d4;\n" \
"  if (va <= 0.0 && (d4 - d3) >= 0.0 && (d5 - d6) >= 0.0) return distance(p, b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6))));\n" \
"  float den = 1.0 / (va + vb + vc); return distance(p, a + ab * (vb * den) + ac * (vc * den)); }\n" \
"kernel void cast_pick(uint t [[thread_position_in_grid]], constant CU &u [[buffer(0)]], const device float *vb [[buffer(1)]],\n" \
"                      const device uint *voff [[buffer(2)]], const device uint *base [[buffer(3)]],\n" \
"                      const device int *head [[buffer(4)]], const device int *next [[buffer(5)]], const device int *tidx [[buffer(6)]],\n" \
"                      const device packed_float3 *trk [[buffer(7)]], device packed_float3 *dyn [[buffer(8)]],\n" \
"                      device atomic_uint *args [[buffer(9)]], device atomic_int *rect [[buffer(10)]]) {\n" \
"  if (t >= u.ntri) return;\n" \
"  uint k = cast_op(base, u.nops, t), f0 = voff[k] / 4u + (t - base[k]) * 3u * 14u;\n" \
"  float3 p[3]; for (uint i = 0u; i < 3u; i++) p[i] = float3(vb[f0 + i * 14u + 10u], vb[f0 + i * 14u + 11u], vb[f0 + i * 14u + 12u]);\n" \
"  float3 m = (p[0] + p[1] + p[2]) / 3.0;\n" \
"  for (int e = head[cast_key(int3(floor(m / CAST_CELL)))]; e >= 0; e = next[e]) {\n" \
"    int j = tidx[e]; float3 a = float3(trk[3 * j]), b = float3(trk[3 * j + 1]), c = float3(trk[3 * j + 2]);\n" \
"    if (cast_dist(p[0], a, b, c) < CAST_ON && cast_dist(p[1], a, b, c) < CAST_ON && cast_dist(p[2], a, b, c) < CAST_ON) return; }\n" \
"  uint o = atomic_fetch_add_explicit(&args[0], 3u, memory_order_relaxed);\n" \
"  if (o + 3u > u.cap) return;\n" \
"  for (uint i = 0u; i < 3u; i++) dyn[o + i] = packed_float3(p[i]);\n" \
"  for (uint c = 0u; c < 4u; c++) {\n" \
"    float2 lo = float2(1e30), hi = float2(-1e30);\n" \
"    for (uint i = 0u; i < 3u; i++) { float4 h = u.cm[c] * float4(p[i], 1.0); float2 st = float2((h.x * 0.5 + 0.5) * u.size, (0.5 - h.y * 0.5) * u.size);\n" \
"      lo = min(lo, st); hi = max(hi, st); }\n" \
"    atomic_fetch_min_explicit(&rect[4u * c], int(floor(lo.x)) - 1, memory_order_relaxed);\n" \
"    atomic_fetch_min_explicit(&rect[4u * c + 1u], int(floor(lo.y)) - 1, memory_order_relaxed);\n" \
"    atomic_fetch_max_explicit(&rect[4u * c + 2u], int(ceil(hi.x)) + 1, memory_order_relaxed);\n" \
"    atomic_fetch_max_explicit(&rect[4u * c + 3u], int(ceil(hi.y)) + 1, memory_order_relaxed); } }\n" \
/* the restore's quad over last frame's movers' rectangle in cascade k.x (pixels, clamped) */ \
"vertex float4 rvs(uint v [[vertex_id]], const device int4 *rect [[buffer(0)]], constant uint4 &k [[buffer(1)]]) {\n" \
"  int4 r = rect[k.x]; float s = float(k.y);\n" \
"  float4 q = clamp(float4(r), 0.0, s); if (q.z <= q.x || q.w <= q.y) return float4(0, 0, 0, 1);\n" \
"  float2 c = float2((v == 1u || v == 2u || v == 4u) ? q.z : q.x, (v == 2u || v == 4u || v == 5u) ? q.w : q.y);\n" \
"  return float4(c.x / s * 2.0 - 1.0, 1.0 - c.y / s * 2.0, 0.0, 1.0); }\n" \
"#define CAST_TCELL " BRR_CAST_STR(LS_TRACK_CELL) "\n" \
"struct CTG { float2 lo; int nx, ny; uint cap, pad0, pad1, pad2; };\n" \
/* the cell's box outside one of the tile's frustum planes (light_cluster.c box_outside) */ \
"static bool cast_out(thread const float4 *pl, const device float *b) {\n" \
"  for (int j = 0; j < 6; j++) { float3 q = float3(pl[j].x >= 0.0 ? b[3] : b[0], pl[j].y >= 0.0 ? b[4] : b[1], pl[j].z >= 0.0 ? b[5] : b[2]);\n" \
"    if (dot(pl[j].xyz, q) + pl[j].w < -0.01) return true; }\n" \
"  return false; }\n" \
"kernel void cast_tiles(uint k [[threadgroup_position_in_grid]], uint i [[thread_index_in_threadgroup]], uint nt [[threads_per_threadgroup]],\n" \
"                       const device LSet &ls [[buffer(0)]], constant CTG &g [[buffer(1)]], const device uint *tile [[buffer(2)]],\n" \
"                       const device int *start [[buffer(3)]], const device int *list [[buffer(4)]], const device float *box [[buffer(5)]],\n" \
"                       device atomic_uint *used [[buffer(6)]], device uint *args [[buffer(7)]], device uint *idx [[buffer(8)]]) {\n" \
"  threadgroup atomic_uint cnt; threadgroup uint base, ok;\n" \
"  uint li = tile[k]; float4 pl[6]; int x0 = 0, y0 = 0, w = 0, nc = 0;\n" \
"  if (i == 0u) atomic_store_explicit(&cnt, 0u, memory_order_relaxed);\n" \
"  if (li != ~0u) {\n" \
"    float4x4 m = ls.s[li].m; float4 rx = float4(m[0].x, m[1].x, m[2].x, m[3].x), ry = float4(m[0].y, m[1].y, m[2].y, m[3].y);\n" \
"    float4 rz = float4(m[0].z, m[1].z, m[2].z, m[3].z), rw = float4(m[0].w, m[1].w, m[2].w, m[3].w);\n" \
"    pl[0] = rw - rx; pl[1] = rw + rx; pl[2] = rw - ry; pl[3] = rw + ry; pl[4] = rz; pl[5] = rw - rz;\n" \
"    for (int j = 0; j < 6; j++) { float l = length(pl[j].xyz); if (l > 0.0) pl[j] /= l; }\n" \
"    float3 p = float3(ls.l[li].pos); float r = ls.l[li].range;\n" \
"    x0 = max(int(floor((p.x - r - g.lo.x) / CAST_TCELL)) - 1, 0); int x1 = min(int(floor((p.x + r - g.lo.x) / CAST_TCELL)) + 1, g.nx - 1);\n" \
"    y0 = max(int(floor((p.y - r - g.lo.y) / CAST_TCELL)) - 1, 0); int y1 = min(int(floor((p.y + r - g.lo.y) / CAST_TCELL)) + 1, g.ny - 1);\n" \
"    w = x1 - x0 + 1; nc = x1 >= x0 && y1 >= y0 ? w * (y1 - y0 + 1) : 0; }\n" \
"  threadgroup_barrier(mem_flags::mem_threadgroup);\n" \
"  for (int q = int(i); q < nc; q += int(nt)) { int c = (y0 + q / w) * g.nx + x0 + q % w;\n" \
"    if (start[c] != start[c + 1] && !cast_out(pl, box + 6 * c)) atomic_fetch_add_explicit(&cnt, uint(start[c + 1] - start[c]), memory_order_relaxed); }\n" \
"  threadgroup_barrier(mem_flags::mem_threadgroup);\n" \
"  if (i == 0u) { uint n3 = 3u * atomic_load_explicit(&cnt, memory_order_relaxed), b = n3 ? atomic_fetch_add_explicit(used, n3, memory_order_relaxed) : 0u;\n" \
"    if (b + n3 > g.cap) n3 = 0u;\n" \
"    args[5u * k] = n3; args[5u * k + 1u] = 1u; args[5u * k + 2u] = b; args[5u * k + 3u] = 0u; args[5u * k + 4u] = 0u;\n" \
"    base = b; ok = n3 != 0u; atomic_store_explicit(&cnt, 0u, memory_order_relaxed); }\n" \
"  threadgroup_barrier(mem_flags::mem_threadgroup);\n" \
"  if (ok == 0u) return;\n" \
"  for (int q = int(i); q < nc; q += int(nt)) { int c = (y0 + q / w) * g.nx + x0 + q % w;\n" \
"    if (start[c] == start[c + 1] || cast_out(pl, box + 6 * c)) continue;\n" \
"    uint o = base + 3u * atomic_fetch_add_explicit(&cnt, uint(start[c + 1] - start[c]), memory_order_relaxed);\n" \
"    for (int e = start[c]; e < start[c + 1]; e++, o += 3u) { uint t = 3u * uint(list[e]); idx[o] = t; idx[o + 1u] = t + 1u; idx[o + 2u] = t + 2u; } } }\n"

#endif
