/* brr_metal_cluster.h: the lights' clusters built on the GPU each frame, the
 * same lists as light/light_cluster.c light_cluster_build (its reference;
 * BR_LIGHT_CLUSTERCHECK compares them on the live frame).  The CPU gives the
 * camera's geometry alone (light_cluster_geom: the column, row and slice
 * planes, the tile corners' rays); two dispatches:
 *
 *   cl_axes   one thread a light: its body (a sphere, or a cone's apex,
 *             axis, range and half angle) against the planes, each column,
 *             row and slice it may touch set in that axis's light mask
 *             (atomic or, 128 lights in four words); its screen coverage
 *             into cov (the lamps' shadow budget reads it a frame late)
 *   cl_list   one thread a cluster: the three masks anded, the lights in
 *             order; in a closed cluster each also tested against the
 *             cluster's bounding sphere; the list written in the cluster's
 *             own place (cl: offset << 8 | count, the offset the cluster's
 *             number x 128) */
#ifndef BRR_METAL_CLUSTER_H
#define BRR_METAL_CLUSTER_H

#include "light_cluster.h"

#define BRR_CL_STR_(x) #x
#define BRR_CL_STR(x) BRR_CL_STR_(x)
#define BRR_CLUSTER_SRC \
"#define CL_TX " BRR_CL_STR(LC_TX) "\n#define CL_TY " BRR_CL_STR(LC_TY) "\n#define CL_TZ " BRR_CL_STR(LC_TZ) "\n" \
"#define CL_LIGHTS " BRR_CL_STR(LC_LIGHTS) "\n" \
"struct CLG { float4 px[CL_TX + 1]; float4 py[CL_TY + 1]; float4 pz[CL_TZ + 1]; float4 ray[(CL_TY + 1) * (CL_TX + 1)]; float4 org;\n" \
"             float zb[CL_TZ + 4]; float4 eye; };\n" \
"struct CLB { float3 p, a; float r, c, s, m; bool cone; };\n" \
"static CLB cl_body(LS l, float3 eye) {\n" \
"  CLB b; float3 ax = float3(l.axis); float al = length(ax); b.p = float3(l.pos); b.r = l.range;\n" \
"  b.cone = !((l.kind > 1.5 && l.kind < 2.5) || l.kind > 3.5) && al > 0.0;\n" \
"  b.a = al > 0.0 ? ax / al : float3(0); b.c = clamp(l.cosout, 0.0, 1.0); b.s = sqrt(max(0.0, 1.0 - b.c * b.c));\n" \
"  b.m = 0.05 + 1e-5 * (length(b.p - eye) + l.range + abs(b.p.x) + abs(b.p.y) + abs(b.p.z)); return b; }\n" \
"static float2 cl_span(CLB b, float4 pl) {\n" \
"  float c = dot(pl.xyz, b.p) + pl.w; if (!b.cone) return float2(c - b.r, c + b.r);\n" \
"  float na = dot(pl.xyz, b.a), sp = sqrt(max(0.0, 1.0 - na * na));\n" \
"  float hi = na >= b.c ? 1.0 : na * b.c + sp * b.s, lo = -na >= b.c ? -1.0 : na * b.c - sp * b.s;\n" \
"  return float2(c + b.r * min(lo, 0.0), c + b.r * max(hi, 0.0)); }\n" \
"static bool cl_meets(CLB b, float3 q, float rq) {\n" \
"  float3 v = q - b.p; float l2 = dot(v, v); rq += b.m;\n" \
"  if (l2 > (b.r + rq) * (b.r + rq)) return false;\n" \
"  if (!b.cone || b.c <= 0.0) return b.cone ? dot(v, b.a) > -rq : true;\n" \
"  float v1 = dot(v, b.a), h = sqrt(max(l2 - v1 * v1, 0.0)); return b.c * h - v1 * b.s <= rq; }\n" \
"kernel void cl_axes(uint i [[thread_position_in_grid]], const device LSet &ls [[buffer(0)]], constant CLG &g [[buffer(1)]],\n" \
"                    constant uint &n [[buffer(2)]], device atomic_uint *mask [[buffer(3)]], device float *cov [[buffer(4)]]) {\n" \
"  if (i >= n) return;\n" \
"  CLB b = cl_body(ls.l[i], g.eye.xyz); uint w = i >> 5, bit = 1u << (i & 31u); int cx = 0, cy = 0, cz = 0;\n" \
"  float2 s0, s1;\n" \
"  for (int k = 0; k < CL_TX; k++) { s0 = cl_span(b, g.px[k]); s1 = cl_span(b, g.px[k + 1]);\n" \
"    if ((k == 0 || s0.y >= -b.m) && (k == CL_TX - 1 || s1.x <= b.m)) { atomic_fetch_or_explicit(&mask[k * 4 + w], bit, memory_order_relaxed); cx++; } }\n" \
"  for (int k = 0; k < CL_TY; k++) { s0 = cl_span(b, g.py[k]); s1 = cl_span(b, g.py[k + 1]);\n" \
"    if ((k == 0 || s0.y >= -b.m) && (k == CL_TY - 1 || s1.x <= b.m)) { atomic_fetch_or_explicit(&mask[(CL_TX + k) * 4 + w], bit, memory_order_relaxed); cy++; } }\n" \
"  for (int k = 0; k < CL_TZ; k++) { s0 = k > 0 ? cl_span(b, g.pz[k]) : float2(0); s1 = k < CL_TZ - 1 ? cl_span(b, g.pz[k + 1]) : float2(0);\n" \
"    if ((k == 0 || s0.y >= -b.m) && (k == CL_TZ - 1 || s1.x <= b.m)) { atomic_fetch_or_explicit(&mask[(CL_TX + CL_TY + k) * 4 + w], bit, memory_order_relaxed); cz++; } }\n" \
"  cov[i] = cz ? float(cx * cy) / float(CL_TX * CL_TY) : 0.0; }\n" \
"kernel void cl_list(uint c [[thread_position_in_grid]], device LSet &ls [[buffer(0)]], constant CLG &g [[buffer(1)]],\n" \
"                    const device uint *mask [[buffer(3)]]) {\n" \
"  if (c >= uint(CL_TX * CL_TY * CL_TZ)) return;\n" \
"  int x = int(c % uint(CL_TX)), y = int((c / uint(CL_TX)) % uint(CL_TY)), z = int(c / uint(CL_TX * CL_TY));\n" \
"  bool closed = x > 0 && x < CL_TX - 1 && y > 0 && y < CL_TY - 1 && z > 0 && z < CL_TZ - 1;\n" \
"  float3 q = 0.0; float rq = 0.0;\n" \
"  if (closed) { float3 pt[8];\n" \
"    for (int j = 0; j < 8; j++) { float4 rr = g.ray[(y + (j >> 1 & 1)) * (CL_TX + 1) + x + (j & 1)]; pt[j] = g.org.xyz + rr.xyz * g.zb[z + (j >> 2)]; q += pt[j] / 8.0; }\n" \
"    for (int j = 0; j < 8; j++) rq = max(rq, length(pt[j] - q)); rq *= 1.0001; }\n" \
"  uint o = c * uint(CL_LIGHTS), cnt = 0u;\n" \
"  for (uint w = 0u; w < 4u; w++) {\n" \
"    uint m = mask[uint(x) * 4u + w] & mask[uint(CL_TX + y) * 4u + w] & mask[uint(CL_TX + CL_TY + z) * 4u + w];\n" \
"    while (m != 0u) { uint b = ctz(m), li = w * 32u + b; m &= m - 1u;\n" \
"      if (closed && !cl_meets(cl_body(ls.l[li], g.eye.xyz), q, rq)) continue;\n" \
"      ls.idx[o + cnt++] = uchar(li); } }\n" \
"  ls.cl[c] = o << 8 | cnt; }\n"

#endif
