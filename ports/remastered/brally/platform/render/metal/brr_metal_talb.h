/* brr_metal_talb.h: each track triangle's albedo learned from the frames by
 * rasterisation (brr_metal.m lit_talb_learn), for the irradiance probes'
 * bake (brr_metal_gi.h: a ray's hit is shaded with it).
 *
 *   talb_vs, talb_fs   the track's triangles (the shadows' static casters,
 *                      track order) drawn from the frame's camera into an ID
 *                      target with depth: each pixel the nearest triangle's
 *                      number
 *   talb_learn         one pixel in 16 a frame (a 4 x 4 block's, rotating):
 *                      where the G-buffer holds a surface of the scene
 *                      (material 124 and up: no lamps, lenses or cars) lying
 *                      on that triangle's plane (within 2 cm + 0.2% of its
 *                      distance: the pixel is that triangle, not a car or a
 *                      billboard in front of it), its albedo (linear, the
 *                      game's texture with its baked light divided out)
 *                      summed onto the triangle
 *   talb_resolve       one thread a triangle: the frame's mean blended into
 *                      its record (a quarter a frame; the first sample taken
 *                      whole), alpha 255 marking it learned, the sums cleared
 *
 * It replaced a ray traced from the eye per sample (2026-10-14: no ray
 * tracing while racing). */
#ifndef BRR_METAL_TALB_H
#define BRR_METAL_TALB_H

#include <stdint.h>

#define BRR_TALB_NEAR 0.05f
#define BRR_TALB_FAR 20000.0f

/* TA: S the world-to-pixel matrix (brr_screen_matrix, column-major), tg the
 * target's size, near and far; eye; k the frame and the triangles */
typedef struct BrrTalbU { float S[16]; float tg[4]; float eye[4]; uint32_t k[4]; } BrrTalbU;

#define BRR_TALB_SRC \
"#include <metal_stdlib>\n" \
"using namespace metal;\n" \
"struct TA { float4x4 S; float4 tg; float4 eye; uint4 k; };\n" \
"struct Prim { int obj; uint mat; uint albedo; uint emit; };\n" \
"struct TBO { float4 pos [[position]]; uint t [[flat]]; };\n" \
"vertex TBO talb_vs(uint v [[vertex_id]], const device packed_float3 *p [[buffer(0)]], constant TA &a [[buffer(1)]]) {\n" \
"  TBO o; o.t = v / 3u; float4 h = a.S * float4(float3(p[v]), 1.0); float w = h.w, n = a.tg.z, f = a.tg.w;\n" \
"  o.pos = float4(h.x / a.tg.x * 2.0 - w, w - h.y / a.tg.y * 2.0, (w - n) * f / (f - n), w); return o; }\n" \
"fragment uint talb_fs(TBO in [[stage_in]]) { return in.t; }\n" \
"kernel void talb_learn(uint2 q [[thread_position_in_grid]], const device packed_float3 *p [[buffer(0)]], constant TA &a [[buffer(1)]],\n" \
"                       device atomic_uint *acc [[buffer(2)]], texture2d<float> alb [[texture(0)]], texture2d<float> pos [[texture(1)]],\n" \
"                       texture2d<uint> ids [[texture(2)]]) {\n" \
"  uint k = (a.k.x * 7u) & 15u; uint2 px = q * 4u + uint2(k & 3u, k >> 2);\n" \
"  if (px.x >= alb.get_width() || px.y >= alb.get_height()) return;\n" \
"  float4 c = alb.read(px); if (c.a * 255.0 < 123.5) return;\n" \
"  uint t = ids.read(px).r; if (t >= a.k.y) return;\n" \
"  float3 P = pos.read(px).xyz; if (all(P == 0.0)) return;\n" \
"  float3 p0 = float3(p[3u * t]), p1 = float3(p[3u * t + 1u]), p2 = float3(p[3u * t + 2u]), nn = cross(p1 - p0, p2 - p0);\n" \
"  float l = length(nn); if (!(l > 0.0)) return;\n" \
"  if (abs(dot(nn / l, P - p0)) > 0.02 + 0.002 * distance(P, a.eye.xyz)) return;\n" \
"  uint3 b = uint3(saturate(c.rgb) * 1023.0 + 0.5); uint o = t * 4u;\n" \
"  atomic_fetch_add_explicit(&acc[o], b.x, memory_order_relaxed); atomic_fetch_add_explicit(&acc[o + 1u], b.y, memory_order_relaxed);\n" \
"  atomic_fetch_add_explicit(&acc[o + 2u], b.z, memory_order_relaxed); atomic_fetch_add_explicit(&acc[o + 3u], 1u, memory_order_relaxed); }\n" \
"static float3 tb_rgb(uint c) { return float3(c & 255u, (c >> 8) & 255u, (c >> 16) & 255u) / 255.0; }\n" \
"kernel void talb_resolve(uint t [[thread_position_in_grid]], constant TA &a [[buffer(1)]], device uint4 *acc [[buffer(2)]],\n" \
"                         device Prim *prim [[buffer(3)]]) {\n" \
"  if (t >= a.k.y) return; uint4 s = acc[t]; if (s.w == 0u) return;\n" \
"  float3 m = float3(s.xyz) / (1023.0 * float(s.w)); uint old = prim[t].albedo;\n" \
"  if (old) m = mix(tb_rgb(old), m, 0.25);\n" \
"  uint3 b = uint3(saturate(m) * 255.0 + 0.5); prim[t].albedo = b.x | b.y << 8 | b.z << 16 | 0xFF000000u; acc[t] = uint4(0); }\n"

#endif
