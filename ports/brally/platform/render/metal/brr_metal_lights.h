/* brr_metal_lights.h: the lights' shader code (DESIGN.md 7.4): the lamps'
 * photometry, the clusters' lists, the lamps' shadow atlas and the lamps'
 * light.  Needs metal_stdlib and a brdf (declared here): brr_metal.m puts it
 * before brr_metal_lit.h's source, which defines it, and the standalone tests
 * (ports/brally/tools/lighttest) compile it with a copy.
 *
 * The structs mirror light.h's LightSpot and light_cluster.h's LightSet
 * byte for byte (light_cluster.c checks the sizes; the tests check them on
 * the GPU). */
#ifndef BRR_METAL_LIGHTS_H
#define BRR_METAL_LIGHTS_H

#define BRR_LIGHTS_SRC \
"struct LS { packed_float3 pos; float kind; packed_float3 axis; float cd; packed_float3 side; float range; packed_float3 rgb; float cosout; };\n" \
"struct LCH { float4x4 vp; float4 eye; float4 zk; float4 zb[7]; float4 atl; };\n" \
"struct LSH { float4x4 m; float4 rect; float4 prm; };\n" \
"struct LSet { LCH h; LS l[128]; LSH s[128]; uint cl[3456]; uchar idx[442368]; };\n" \
/* a lamp's intensity toward d (unit, from the lamp), candela: a headlamp's \
   low beam (UN ECE R112's asymmetric pattern for right-hand traffic, aimed \
   as its paragraph 6.2.2.1 aims it: the cut-off's horizontal part 0.57 \
   degrees below the horizon on the near side, rising 15 degrees from the \
   elbow on the kerb side; the hot spot below the cut-off and right of \
   centre, the near side dimmer, a wide spread below it; the cut-off a \
   blend in log intensity, so its line, R112's steepest log gradient, sits \
   where it is placed, its profile the lens's blur: a Gaussian of 0.28 \
   degrees in log intensity (tanh's form of the normal CDF), R112's \
   sharpness G (log drop per 0.1 degree) at most 0.27, inside its 0.13..0.3, \
   where a smoothstep over 0.7 degrees made 0.41 and a kink at its ends; the \
   sideways spread taken to zero smoothly at the outer cone (zero slope \
   there), so no edge where it ends; the elbow and the step's top \
   rounded (smooth max and min, 0.15 degree), so the line has no corner; \
   checked against R112's test points by \
   ports/brally/tools/lighttest), a rear lamp's broad lobe; a point's or a \
   spot's candela (lwin shapes a spot) */ \
"static float lamp(LS l, float3 d) {\n" \
"  if (l.kind > 1.5) return l.cd;\n" \
"  float3 ax = float3(l.axis), sd = float3(l.side), up = cross(sd, ax);\n" \
"  float al = dot(d, ax); if (al <= 0.0) return 0.0;\n" \
"  if (l.kind > 0.5) return l.cd * pow(al, 1.5);\n" \
"  float h = atan2(dot(d, sd), al) * 57.2958, v = atan2(dot(d, up), al) * 57.2958 + 0.57;\n" \
"  float hw = h < 1.5 ? 5.0 : 10.0;\n" \
"  float spread = 0.25 * exp(-(h / 32.0) * (h / 32.0)) + 0.75 * exp(-((h - 1.5) / hw) * ((h - 1.5) / hw));\n" \
"  float vert = v < -1.5 ? exp(-((v + 1.5) / 4.5) * ((v + 1.5) / 4.5)) : 1.0;\n" \
"  float a = h * 0.268, c0 = 0.5 * (a + sqrt(a * a + 0.0225)), cut = 2.0 - 0.5 * ((2.0 - c0) + sqrt((2.0 - c0) * (2.0 - c0) + 0.0225));\n" \
"  float x = clamp((v - cut) / 0.28, -4.0, 4.0), ph = 0.5 + 0.5 * tanh(0.7978846 * (x + 0.044715 * x * x * x));\n" \
"  float t = saturate((al - l.cosout) / (1.0 - l.cosout)), side = t * t * (3.0 - 2.0 * t);\n" \
"  return l.cd * spread * vert * side * pow(0.012, ph);\n" \
"}\n" \
/* where a light reaches: none from its range on (a smooth window from 0.7 of \
   it, Karis 2013), none outside a cone's outer angle (a spot's from its inner \
   cone, a lamp's over its last 0.1 of cosine; a hemisphere's edge is the \
   lamp's own); d unit, from the lamp, dl the distance.  Zero exactly where \
   the culling (light_cluster.c) may leave the light out. */ \
"static float lwin(LS l, float3 d, float dl) {\n" \
"  if (!(dl < l.range)) return 0.0;\n" \
"  float w = 1.0 - smoothstep(0.7 * l.range, l.range, dl);\n" \
"  if ((l.kind > 1.5 && l.kind < 2.5) || l.kind > 3.5) return w;\n" \
"  float al = dot(d, float3(l.axis));\n" \
"  if (al <= l.cosout) return 0.0;\n" \
"  if (l.cosout <= 0.0) return w;\n" \
"  float ci = l.kind > 2.5 ? max(l.side[0], l.cosout + 1e-4) : min(l.cosout + 0.1, 1.0);\n" \
"  return w * smoothstep(l.cosout, ci, al);\n" \
"}\n" \
/* the cluster a world point is in: its screen tile by clip x/w and y/w, its \
   slice by view depth (the slices' table decides at a boundary, as the \
   culling's planes do) */ \
"static float lczb(const device LCH &h, int k) { return h.zb[k >> 2][k & 3]; }\n" \
"static uint lc_index(const device LCH &h, float3 p) {\n" \
"  float4 c = h.vp * float4(p, 1.0); float2 nd = c.xy / c.w;\n" \
"  int tx = clamp(int(floor((nd.x * 0.5 + 0.5) * 16.0)), 0, 15), ty = clamp(int(floor((nd.y * 0.5 + 0.5) * 9.0)), 0, 8);\n" \
"  float z = c.w * h.zk.x;\n" \
"  int tz = z >= lczb(h, 1) ? clamp(1 + int(floor((log2(z) - h.zk.z) * h.zk.y)), 1, 23) : 0;\n" \
"  if (tz > 0 && z < lczb(h, tz)) tz--;\n" \
"  if (tz < 23 && z >= lczb(h, tz + 1)) tz++;\n" \
"  return uint((tz * 9 + ty) * 16 + tx);\n" \
"}\n" \
/* a lamp's shadow from its atlas tile: the receiver pushed along its normal \
   by one to three texels of the tile at its depth (three at grazing light: \
   the 3x3 kernel's nearest row then still lies on the receiver's far side, \
   where a low lamp's road is deeper by metres a texel), and compared nearer \
   by one texel x (1 + tan theta), theta the light's angle on the surface \
   (slope-scaled); 3x3 bilinear comparisons kept inside the tile.  1 outside \
   the tile's frustum (the cone lies inside it) and for an unshadowed lamp. */ \
"static float lampvis(const device LSet &s, depth2d<float> atl, uint i, float3 p, float3 n, float3 ld) {\n" \
"  const device LSH &h = s.s[i];\n" \
"  if (h.rect.w < 0.5) return 1.0;\n" \
"  float4 c0 = h.m * float4(p, 1.0); if (c0.w <= h.prm.w) return 1.0;\n" \
"  float tx = 2.0 * c0.w * h.prm.z / (h.rect.z * s.h.atl.x);\n" \
"  float nl = clamp(dot(n, ld), 0.05, 1.0), tn = min(sqrt(1.0 - nl * nl) / nl, 8.0);\n" \
"  float4 c = h.m * float4(p + n * tx * (1.0 + 2.0 * (1.0 - nl)), 1.0); float2 nd = c.xy / c.w;\n" \
"  if (c.w <= h.prm.w || any(abs(nd) >= 1.0)) return 1.0;\n" \
"  float2 st = h.rect.xy + float2(nd.x * 0.5 + 0.5, 0.5 - nd.y * 0.5) * h.rect.z, e = float2(1.5 * s.h.atl.y);\n" \
"  st = clamp(st, h.rect.xy + e, h.rect.xy + h.rect.z - e);\n" \
"  float zr = h.prm.x + h.prm.y / max(c.w - tx * (1.0 + tn), h.prm.w);\n" \
"  constexpr sampler cs(filter::linear, address::clamp_to_edge, compare_func::less_equal);\n" \
"  float vis = 0.0;\n" \
"  for (int y = -1; y <= 1; y++) for (int x = -1; x <= 1; x++) vis += atl.sample_compare(cs, st + float2(x, y) * s.h.atl.y, zr);\n" \
"  return vis / 9.0;\n" \
"}\n" \
/* the lamps' light leaving a surface toward v (cd/m2 before exposure): only \
   the lights in the point's cluster, each shadowed from its tile; brdf is \
   the lighting's (brr_metal_lit.h), bE its table */ \
"static float3 brdf(float3 n, float3 v, float3 l, float3 alb, float rough, texture2d<float> bE);\n" \
"static float3 lamps(const device LSet &s, depth2d<float> atl, float3 p, float3 n, float3 v, float3 alb, float rough, texture2d<float> bE) {\n" \
"  uint e = s.cl[lc_index(s.h, p)], o = e >> 8, cnt = e & 255u; float3 Lo = float3(0);\n" \
"  for (uint k = 0; k < cnt; k++) {\n" \
"    uint i = s.idx[o + k]; LS l = s.l[i];\n" \
"    float3 L = float3(l.pos) - p; float d2 = max(dot(L, L), 0.01), dl = sqrt(d2); float3 ld = L / dl;\n" \
"    if (dot(n, ld) <= 0.0) continue;\n" \
"    float E = lamp(l, -ld) * lwin(l, -ld, dl) / d2;\n" \
"    if (E <= 0.0) continue;\n" \
"    Lo += float3(l.rgb) * E * lampvis(s, atl, i, p, n, ld) * brdf(n, v, ld, alb, rough, bE);\n" \
"  }\n" \
"  return Lo;\n" \
"}\n" \
/* BR_LIGHT_VIEW=6: the lights a pixel's cluster lists, 0 blue, 16 green, 32 \
   and more red */ \
"static float3 lc_heat(const device LSet &s, float3 p) {\n" \
"  float t = saturate(float(s.cl[lc_index(s.h, p)] & 255u) / 32.0);\n" \
"  return mix(mix(float3(0, 0, 1), float3(0, 1, 0), saturate(t * 2.0)), float3(1, 0, 0), saturate(t * 2.0 - 1.0));\n" \
"}\n" \
/* the atlas: a tile cleared to the far plane, then its casters */ \
"struct LCO { float4 pos [[position]]; };\n" \
"vertex LCO lcclr(uint i [[vertex_id]]) { float2 p = float2((i << 1) & 2, i & 2); LCO o; o.pos = float4(p * 2.0 - 1.0, 1.0, 1.0); return o; }\n" \
"vertex LCO lcvs(uint i [[vertex_id]], const device packed_float3 *v [[buffer(0)]], constant float4x4 &m [[buffer(1)]]) {\n" \
"  LCO o; o.pos = m * float4(float3(v[i]), 1.0); return o; }\n"

#endif
