/* brr_metal_smooth.h: smooth shading normals on the GPU, each frame, for the
 * lit draws (the same normals as light/light_smooth.c, which stays as their
 * reference: BR_LIGHT_SMOOTHCHECK compares the two on the live frame).
 *
 * The frame's lit corners are the vertices of its opaque and blended 3D
 * draws, read straight from the frame's vertex buffer: op k's n vertices at
 * byte voff, corner indices from base[k] (a prefix over the ops, searched by
 * each thread).  Four dispatches:
 *
 *   sm_face     a corner's triangle's face normal (by its first corner), its
 *               angle at the corner, its weld cell (LIGHT_SMOOTH_WELD metres)
 *               hashed into SM_CELLS buckets, counted
 *   sm_scan     the buckets' counts into starts (one threadgroup)
 *   sm_scatter  each corner into its bucket's list
 *   sm_normal   a corner's normal: over the 27 cells about its own, every
 *               corner within the weld distance whose face is within the
 *               crease of its own (turned over when it faces the other
 *               way), weighted by its angle; written as float4 at the
 *               corner's place in the normal buffer (the order of the ops,
 *               as op_draw binds it) */
#ifndef BRR_METAL_SMOOTH_H
#define BRR_METAL_SMOOTH_H

#include "light_smooth.h"

#define SM_CELLS 65536
#define SM_OPS_MAX 4096

#define BRR_SM_STR_(x) #x
#define BRR_SM_STR(x) BRR_SM_STR_(x)
#define BRR_SMOOTH_SRC \
"#define SM_CELLS " BRR_SM_STR(SM_CELLS) "u\n" \
"#define SM_WELD " BRR_SM_STR(LIGHT_SMOOTH_WELD) "\n" \
"struct SMU { uint nops; uint ncorner; float coscrease; uint pad; };\n" \
"static uint sm_op(const device uint *base, uint nops, uint c) {\n" \
"  uint lo = 0u, hi = nops - 1u;\n" \
"  while (lo < hi) { uint m = (lo + hi + 1u) >> 1; if (base[m] <= c) lo = m; else hi = m - 1u; }\n" \
"  return lo; }\n" \
/* a corner's world position: brr_vertex's wx, wy, wz (floats 10..12) */ \
"static float3 sm_pos(const device float *vb, const device uint *voff, const device uint *base, uint nops, uint c) {\n" \
"  uint k = sm_op(base, nops, c), f = voff[k] / 4u + (c - base[k]) * 14u;\n" \
"  return float3(vb[f + 10u], vb[f + 11u], vb[f + 12u]); }\n" \
"static int3 sm_cell(float3 p) { return int3(floor(p / SM_WELD)); }\n" \
"static uint sm_hash(int3 q) { uint h = uint(q.x) * 0x9E3779B1u ^ uint(q.y) * 0x85EBCA77u ^ uint(q.z) * 0xC2B2AE3Du;\n" \
"  h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; return h & (SM_CELLS - 1u); }\n" \
"kernel void sm_face(uint c [[thread_position_in_grid]], constant SMU &u [[buffer(0)]], const device float *vb [[buffer(1)]],\n" \
"                    const device uint *voff [[buffer(2)]], const device uint *base [[buffer(3)]], device float4 *pc [[buffer(4)]],\n" \
"                    device float4 *fn [[buffer(5)]], device atomic_uint *cnt [[buffer(6)]], device uint *cell [[buffer(7)]]) {\n" \
"  if (c >= u.ncorner) return;\n" \
"  uint t = c / 3u, i = c - t * 3u;\n" \
"  float3 a = sm_pos(vb, voff, base, u.nops, t * 3u), b = sm_pos(vb, voff, base, u.nops, t * 3u + 1u), d = sm_pos(vb, voff, base, u.nops, t * 3u + 2u);\n" \
"  float3 x = cross(b - a, d - a); float l = length(x);\n" \
"  if (i == 0u) fn[t] = float4(l > 1e-12 ? x / l : float3(0), 0);\n" \
"  float3 o = i == 0u ? a : i == 1u ? b : d, p = i == 0u ? b : i == 1u ? d : a, q = i == 0u ? d : i == 1u ? a : b;\n" \
"  float3 du = p - o, dv = q - o; float lu = length(du), lv = length(dv);\n" \
"  float ang = l > 1e-12 && lu > 0.0 && lv > 0.0 ? acos(clamp(dot(du, dv) / (lu * lv), -1.0, 1.0)) : 0.0;\n" \
"  pc[c] = float4(o, ang);\n" \
"  uint h = sm_hash(sm_cell(o)); cell[c] = h; atomic_fetch_add_explicit(&cnt[h], 1u, memory_order_relaxed); }\n" \
/* the counts into starts: 1024 threads, 64 buckets each, then their sums */ \
"kernel void sm_scan(uint t [[thread_index_in_threadgroup]], device uint *cnt [[buffer(0)]], device uint *start [[buffer(1)]]) {\n" \
"  threadgroup uint part[1024]; uint s = 0u, b0 = t * (SM_CELLS / 1024u);\n" \
"  for (uint i = 0u; i < SM_CELLS / 1024u; i++) s += cnt[b0 + i];\n" \
"  part[t] = s; threadgroup_barrier(mem_flags::mem_threadgroup);\n" \
"  if (t == 0u) { uint r = 0u; for (uint i = 0u; i < 1024u; i++) { uint v = part[i]; part[i] = r; r += v; } }\n" \
"  threadgroup_barrier(mem_flags::mem_threadgroup);\n" \
"  uint r = part[t]; for (uint i = 0u; i < SM_CELLS / 1024u; i++) { start[b0 + i] = r; r += cnt[b0 + i]; } }\n" \
"kernel void sm_scatter(uint c [[thread_position_in_grid]], constant SMU &u [[buffer(0)]], const device uint *cell [[buffer(1)]],\n" \
"                       const device uint *start [[buffer(2)]], device atomic_uint *fill [[buffer(3)]], device uint *list [[buffer(4)]]) {\n" \
"  if (c >= u.ncorner) return;\n" \
"  uint h = cell[c]; list[start[h] + atomic_fetch_add_explicit(&fill[h], 1u, memory_order_relaxed)] = c; }\n" \
"kernel void sm_normal(uint c [[thread_position_in_grid]], constant SMU &u [[buffer(0)]], const device float4 *pc [[buffer(1)]],\n" \
"                      const device float4 *fn [[buffer(2)]], const device uint *start [[buffer(3)]], const device uint *cnt [[buffer(4)]],\n" \
"                      const device uint *list [[buffer(5)]], device float4 *nb [[buffer(6)]]) {\n" \
"  if (c >= u.ncorner) return;\n" \
"  float3 own = fn[c / 3u].xyz, p = pc[c].xyz, sum = 0.0; int3 q = sm_cell(p);\n" \
"  if (all(own == 0.0)) { nb[c] = 0.0; return; }\n" \
/* the 27 cells' buckets, each once (two cells can share one) */ \
"  uint seen[27]; int ns = 0;\n" \
"  for (int dz = -1; dz <= 1; dz++) for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {\n" \
"    uint h = sm_hash(q + int3(dx, dy, dz)); bool dup = false;\n" \
"    for (int m = 0; m < ns; m++) dup = dup || seen[m] == h;\n" \
"    if (dup) continue;\n" \
"    seen[ns++] = h; uint s0 = start[h], n = cnt[h];\n" \
"    for (uint k = 0u; k < n; k++) {\n" \
"      uint j = list[s0 + k]; float4 pj = pc[j]; float3 o = fn[j / 3u].xyz; float dd = dot(o, own);\n" \
"      if (distance_squared(pj.xyz, p) > SM_WELD * SM_WELD || abs(dd) < u.coscrease) continue;\n" \
"      sum += (dd < 0.0 ? -1.0 : 1.0) * pj.w * o; }\n" \
"  }\n" \
"  float l = length(sum); nb[c] = float4(l > 1e-12 ? sum / l : own, 0); }\n"

#endif
