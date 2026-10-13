/* brr_metal_refl.h: specular reflections and wet roads (DESIGN.md 8.4).
 *
 *   BRR_WET_SRC   wet surfaces (needs LU): weather 2 (rain) and 3 (snow
 *                 melt) wet what faces up (n.z > 0.9), the road; wetness
 *                 darkens a porous surface's albedo (Lagarde 2013, "Water
 *                 drop 3b", the porosity model) and smooths it, and a
 *                 world-space noise mask lays puddles, smoother still.
 *                 radiance() calls wet() first, so every light sees it.
 *   BRR_REFL_SRC  the reflection pass (needs LU, BRR_WET_SRC and BRR_RT_SRC):
 *                 a compute kernel over the lit target, after the deferred
 *                 lighting.  One GGX sample a pixel (the normals visible
 *                 from the eye, Heitz 2018); the ray marched in screen space
 *                 against the G-buffer's positions, one step a pixel (at most
 *                 64) then six halvings; its confidence fades at the screen's
 *                 edges and to nothing on a miss.  What the march does not
 *                 find comes from the track's BVH (rt_closest) below
 *                 roughness wet.w, a hit shaded as grey ground (albedo 0.3
 *                 under the sun and sky, unshadowed), else from the sky-view
 *                 table.  The split-sum (Karis 2013): the reflected light
 *                 times the lobe's directional albedo E(n.v, roughness), the
 *                 same table the diffuse coupling reads (brdf_lut; F0 is
 *                 0.04 everywhere, so E is the DFG term's F0 A + B).  The
 *                 deferred pass lights with the sun, the sky's diffuse and
 *                 the lamps: it has no sky specular, so this is the
 *                 environment's specular, added once.
 *
 * LU.wet: x the weather's wetness, y its puddles, z the tier gate (0 off,
 * 1 screen space and sky, 2 also the BVH), w the BVH's roughness limit. */
#ifndef BRR_METAL_REFL_H
#define BRR_METAL_REFL_H

#define REFL_ROUGH_MAX 0.6f      /* above this only the sky's diffuse (probes later) */
#define REFL_RT_ROUGH 0.4f       /* below this a miss traces the BVH */

#define BRR_WET_SRC \
"static float wet_h(float2 c) { return fract(sin(dot(c, float2(127.1, 311.7))) * 43758.5453); }\n" \
/* value noise, smooth between the lattice's corners: 0..1 */ \
"static float wet_n(float2 x) { float2 i = floor(x), f = x - i, w = f * f * (3.0 - 2.0 * f);\n" \
"  return mix(mix(wet_h(i), wet_h(i + float2(1, 0)), w.x), mix(wet_h(i + float2(0, 1)), wet_h(i + float2(1, 1)), w.x), w.y); }\n" \
/* puddles: two octaves of noise over metres, the top part of the mask, \
   scaled by the weather's puddles */ \
"static float puddle(constant LU &u, float3 p) {\n" \
"  float m = 0.65 * wet_n(p.xy * 0.25) + 0.35 * wet_n(p.xy * 0.9 + 17.0);\n" \
"  return smoothstep(0.58, 0.68, m) * u.wet.y; }\n" \
/* a surface's wetness 0..1 and its puddle 0..1 (up-facing only) */ \
"static float2 wetness(constant LU &u, float3 p, float3 n) {\n" \
"  if (u.wet.x <= 0.0 || n.z <= 0.9) return 0.0;\n" \
"  float k = saturate((n.z - 0.9) * 20.0), pd = puddle(u, p) * k;\n" \
"  return float2(max(u.wet.x * k, pd), pd); }\n" \
/* Lagarde's porosity (rough surfaces are porous and darken; smooth ones \
   barely), the albedo times mix(1, mix(1, 0.2, porosity), wetness); the \
   roughness toward 0.1 by the wetness and 0.03 in a puddle, never rougher */ \
"static void wet(constant LU &u, float3 p, float3 n, thread float3 &alb, thread float &rough) {\n" \
"  float2 w = wetness(u, p, n); if (w.x <= 0.0) return;\n" \
"  float por = saturate((rough - 0.5) / 0.4), f = mix(1.0, 0.2, por);\n" \
"  alb *= mix(1.0, f, w.x);\n" \
"  rough = min(rough, mix(mix(rough, 0.1, w.x), 0.03, w.y)); }\n"

#define BRR_REFL_SRC \
"static float rf_vd(constant LU &u, float3 q) { return dot(q - u.eye.xyz, u.cfwd.xyz); }\n" \
/* the screen-space march: p (on the surface) along unit r, at most maxd \
   metres (cut at 0.1 m in front of the eye); jit 0..1 the first step's \
   share.  Returns the hit's pixel (xy), its confidence (z: the screen \
   edge's fade; 0 a miss) and the ray's length to it (w) */ \
"static float4 ssr(constant LU &u, texture2d<float> pos, float3 p, float3 r, float maxd, float jit) {\n" \
"  float2 wh = float2(pos.get_width(), pos.get_height());\n" \
"  float dp = rf_vd(u, p), dr = dot(r, u.cfwd.xyz), len = maxd;\n" \
"  if (dp + dr * len < 0.1) len = (0.1 - dp) / dr;\n" \
"  if (len <= 0.0) return 0.0;\n" \
"  float3 e = p + r * len; float4 h0 = u.scr * float4(p, 1.0), h1 = u.scr * float4(e, 1.0);\n" \
"  if (h0.w <= 1e-6 || h1.w <= 1e-6) return 0.0;\n" \
"  float k0 = 1.0 / h0.w, k1 = 1.0 / h1.w; float2 s0 = h0.xy * k0, s1 = h1.xy * k1;\n" \
"  int n = int(clamp(ceil(length(s1 - s0)), 1.0, 64.0));\n" \
"  float lp = 0.0, dqp = dp;\n" \
"  for (int i = 0; i < n; i++) {\n" \
"    float l = (float(i) + jit) / float(n); float2 s = mix(s0, s1, l);\n" \
"    if (any(s < 0.0) || any(s >= wh)) return 0.0;\n" \
"    float3 q = mix(p * k0, e * k1, l) / mix(k0, k1, l); float dq = rf_vd(u, q);\n" \
"    float3 P = pos.read(uint2(s)).xyz;\n" \
"    if (!all(P == 0.0)) {\n" \
"      float ds = rf_vd(u, P), th = 0.05 + 0.01 * dq + 2.0 * abs(dq - dqp);\n" \
"      if (dq > ds && dq - ds < th && length(P - p) > 0.05) {\n" \
"        float lo = lp, hi = l;\n" \
"        for (int b = 0; b < 6; b++) { float m = 0.5 * (lo + hi); float2 sm = mix(s0, s1, m);\n" \
"          float3 qm = mix(p * k0, e * k1, m) / mix(k0, k1, m); float3 Pm = pos.read(uint2(sm)).xyz;\n" \
"          if (!all(Pm == 0.0) && rf_vd(u, qm) > rf_vd(u, Pm)) hi = m; else lo = m; }\n" \
"        float2 sh = mix(s0, s1, hi), ed = min(sh, wh - sh) / (0.08 * wh.y);\n" \
"        float3 qh = mix(p * k0, e * k1, hi) / mix(k0, k1, hi);\n" \
"        return float4(sh, saturate(min(ed.x, ed.y)), length(qh - p));\n" \
"      }\n" \
"    }\n" \
"    lp = l; dqp = dq;\n" \
"  }\n" \
"  return 0.0;\n" \
"}\n" \
/* one GGX normal visible from v (Heitz 2018) for the point (x1, x2) of the \
   unit square, around n */ \
"static float3 rf_vndf(float3 n, float3 v, float a, float2 x) {\n" \
"  float3 t = abs(n.z) < 0.999 ? normalize(cross(float3(0, 0, 1), n)) : float3(1, 0, 0), b = cross(n, t);\n" \
"  float3 vl = float3(dot(v, t), dot(v, b), dot(v, n)), vh = normalize(float3(a * vl.x, a * vl.y, vl.z));\n" \
"  float lsq = vh.x * vh.x + vh.y * vh.y; float3 T1 = lsq > 0.0 ? float3(-vh.y, vh.x, 0) * rsqrt(lsq) : float3(1, 0, 0), T2 = cross(vh, T1);\n" \
"  float r = sqrt(x.x), ph = 6.2831853 * x.y, p1 = r * cos(ph), p2 = r * sin(ph), s = 0.5 * (1.0 + vh.z);\n" \
"  p2 = (1.0 - s) * sqrt(max(1.0 - p1 * p1, 0.0)) + s * p2;\n" \
"  float3 nh = p1 * T1 + p2 * T2 + sqrt(max(1.0 - p1 * p1 - p2 * p2, 0.0)) * vh;\n" \
"  float3 hl = normalize(float3(a * nh.x, a * nh.y, max(nh.z, 0.0)));\n" \
"  return hl.x * t + hl.y * b + hl.z * n; }\n" \
"static float2 rf_svuv(float3 d) { float lat = asin(clamp(d.z, -1.0, 1.0)), lon = atan2(d.y, d.x);\n" \
"  return float2(lon / 3.14159 * 0.5 + 0.5, 0.5 + 0.5 * sign(lat) * sqrt(abs(lat) / 1.5708)); }\n" \
/* the reflection pass: src the lit target as the deferred pass left it \
   (pre-exposed), dst the same plus the reflection; sky the sky-view table \
   (cd/m2), bE the lobe's albedo table; the BVH's buffers 0..2 (rt_closest) */ \
"kernel void reflk(uint2 px [[thread_position_in_grid]], constant LU &u [[buffer(3)]],\n" \
"                  device const RtNode *node [[buffer(0)]], device const float *tri [[buffer(1)]], device const uint *id [[buffer(2)]],\n" \
"                  texture2d<float> alb [[texture(0)]], texture2d<float> pos [[texture(1)]], texture2d<float> nrm [[texture(2)]],\n" \
"                  texture2d<float> src [[texture(3)]], texture2d<float, access::write> dst [[texture(4)]],\n" \
"                  texture2d<float> sky [[texture(5)]], texture2d<float> bE [[texture(6)]]) {\n" \
"  if (px.x >= dst.get_width() || px.y >= dst.get_height()) return;\n" \
"  float4 c = src.read(px), a = alb.read(px);\n" \
"  if (a.a < 0.25 || u.wet.z < 0.5) { dst.write(c, px); return; }\n" \
"  float3 p = pos.read(px).xyz, n = nrm.read(px).xyz; float fog = nrm.read(px).w;\n" \
"  if (dot(n, u.eye.xyz - p) < 0.0) n = -n;\n" \
"  float3 al = a.rgb; float rough = u.misc.z; wet(u, p, n, al, rough);\n" \
"  if (rough > 0.6) { dst.write(c, px); return; }\n" \
"  float3 v = normalize(u.eye.xyz - p); float nv = clamp(dot(n, v), 1e-3, 1.0);\n" \
"  float j = fract(52.9829189 * fract(dot(float2(px) + 0.5, float2(0.06711056, 0.00583715))));\n" \
"  float ar = max(rough * rough, 0.0447);\n" \
"  float3 h = rf_vndf(n, v, ar, float2(fract(j + 0.5), fract(j * 7.31 + 0.25))), r = reflect(-v, h);\n" \
"  if (dot(r, n) <= 0.0) r = reflect(-v, n);\n" \
"  float4 s = ssr(u, pos, p, r, 80.0, j);\n" \
"  float3 Lss = s.z > 0.0 ? src.read(uint2(s.xy)).rgb : float3(0), Lfb;\n" \
"  constexpr sampler ls(filter::linear, address::clamp_to_edge);\n" \
"  RtHit hh; hh.tri = -1;\n" \
"  if (s.z < 1.0 && u.wet.z > 1.5 && rough < u.wet.w) hh = rt_closest(node, tri, id, p + n * 0.02, r, 0.0, 300.0);\n" \
"  if (hh.tri >= 0) Lfb = 0.3 / 3.14159 * (u.sunc.rgb * max(u.sun.z, 0.0) + u.skyc.rgb);   /* grey ground under the sun and sky */\n" \
"  else Lfb = sky.sample(ls, rf_svuv(r.z < 0.0 ? float3(r.xy, -r.z) : r)).rgb;   /* below the horizon: the sky it mirrors */\n" \
"  Lfb = min(Lfb * u.wb.rgb * u.misc.x, float3(60000.0));\n" \
"  float2 sz = float2(bE.get_width(), bE.get_height()), tc = (0.5 + float2(sqrt(nv), saturate((rough - 0.0447) / 0.9553)) * (sz - 1.0)) / sz;\n" \
"  float E = bE.sample(ls, tc).r;\n" \
"  float3 R = mix(Lfb, Lss, s.z) * E * (1.0 - fog);\n" \
"  dst.write(float4(min(c.rgb + R, float3(60000.0)), c.a), px);\n" \
"}\n"

#endif
