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
/* point i of n of a Vogel (golden angle) disc of radius 1 turned by rot (cos, \
   sin): the sun's disc (brr_metal_lit.h), under its own name as this source \
   comes first */ \
"static float2 lvog(int i, int n, float2 rot) {\n" \
"  float r = sqrt((float(i) + 0.5) / float(n)), t = float(i) * 2.39996323; float2 v = r * float2(cos(t), sin(t));\n" \
"  return float2(v.x * rot.x - v.y * rot.y, v.x * rot.y + v.y * rot.x); }\n" \
/* a lamp's shadow from its atlas tile, as soft as a lamp 0.15 m across \
   makes it (percentage-closer soft shadows, Fernando 2005, for a \
   perspective tile): the receiver pushed along its normal by one to three \
   texels of the tile at its depth and compared nearer by one texel x (1 + \
   tan theta), theta the light's angle on the surface (slope-scaled), each \
   sample's reference following the receiver's plane across the tile (its \
   z/w is affine in the tile, so its gradient from two points on the plane \
   a texel off: the wide discs then never find the receiver itself); depth \
   along the axis from the tile's z/w as w = B / (z - A).  A blocker search \
   over a disc as wide as the lamp's penumbra from a caster at the near \
   plane, the blockers' mean distance db, the penumbra's width at the \
   receiver (depth dr) w = 0.15 (dr - db) / db, and the filtered comparisons \
   over a disc that wide: for a straight edge the share of the lamp's disc \
   in view.  A metre at depth d is T / (2 d tan) of the tile's texels (T its \
   size), so the disc's radius in texels is 0.075 T / (2 tan) (1 / db - 1 / \
   dr).  The search at most 12 texels across its radius, the disc 0.5 to \
   16, every sample kept half a texel inside the tile; rot the pixel's turn \
   of the discs (interleaved gradient noise).  1 outside the tile's frustum \
   (the cone lies inside it), for an unshadowed lamp and where the search \
   finds no blocker. */ \
"static float lampvis(const device LSet &s, depth2d<float> atl, uint i, float3 p, float3 n, float3 ld, float2 rot) {\n" \
"  const device LSH &h = s.s[i];\n" \
"  if (h.rect.w < 0.5) return 1.0;\n" \
"  float4 c0 = h.m * float4(p, 1.0); if (c0.w <= h.prm.w) return 1.0;\n" \
"  float T = h.rect.z * s.h.atl.x, tx = 2.0 * c0.w * h.prm.z / T;\n" \
"  float nl = clamp(dot(n, ld), 0.05, 1.0), tn = min(sqrt(1.0 - nl * nl) / nl, 8.0);\n" \
"  float4 c = h.m * float4(p + n * tx * (1.0 + 2.0 * (1.0 - nl)), 1.0); float2 nd = c.xy / c.w;\n" \
"  if (c.w <= h.prm.w || any(abs(nd) >= 1.0)) return 1.0;\n" \
"  float ts = s.h.atl.y; float2 lo = h.rect.xy + 0.5 * ts, hi = h.rect.xy + h.rect.z - 0.5 * ts;\n" \
"  float2 st = h.rect.xy + float2(nd.x * 0.5 + 0.5, 0.5 - nd.y * 0.5) * h.rect.z;\n" \
"  float dr = max(c.w - tx * (1.0 + tn), h.prm.w), zr = h.prm.x + h.prm.y / dr;\n" \
"  float3 t1 = normalize(cross(n, abs(n.z) < 0.9 ? float3(0, 0, 1) : float3(1, 0, 0))), t2 = cross(n, t1), q = p + n * tx * (1.0 + 2.0 * (1.0 - nl));\n" \
"  float4 a1 = h.m * float4(q + t1 * tx, 1.0), a2 = h.m * float4(q + t2 * tx, 1.0);\n" \
"  float2 e1 = (a1.xy / a1.w - nd) * float2(0.5, -0.5) * h.rect.z, e2 = (a2.xy / a2.w - nd) * float2(0.5, -0.5) * h.rect.z;\n" \
"  float z1 = a1.z / a1.w - c.z / c.w, z2 = a2.z / a2.w - c.z / c.w, det = e1.x * e2.y - e1.y * e2.x;\n" \
"  float2 gz = abs(det) > 1e-12 ? float2(z1 * e2.y - z2 * e1.y, z2 * e1.x - z1 * e2.x) / det : float2(0.0);   /* dz / dst */\n" \
"  zr -= (abs(gz.x) + abs(gz.y)) * 0.5 * ts;   /* half a texel of the plane's slope: the texels a sample's footprint reaches */\n" \
"  float kt = 0.075 * T / (2.0 * h.prm.z);   /* the lamp's radius in texels per unit of 1 / depth */\n" \
"  float rs = clamp(kt * (1.0 / h.prm.w - 1.0 / dr), 1.0, 12.0), zb = 0.0, nb = 0.0;\n" \
"  constexpr sampler ps(filter::nearest, address::clamp_to_edge);\n" \
"  for (int k = 0; k < 16; k++) {\n" \
"    float2 o = clamp(st + lvog(k, 16, rot) * (rs * ts), lo, hi); float4 g = atl.gather(ps, o); bool4 b = g < zr + dot(gz, o - st);\n" \
"    zb += dot(select(float4(0.0), h.prm.y / (g - h.prm.x), b), float4(1.0)); nb += dot(select(float4(0.0), float4(1.0), b), float4(1.0));\n" \
"  }\n" \
"  if (nb == 0.0) return 1.0;\n" \
"  float db = max(zb / nb, h.prm.w), wr = clamp(kt * (1.0 / db - 1.0 / dr), 0.5, 16.0), vis = 0.0;\n" \
"  constexpr sampler cs(filter::linear, address::clamp_to_edge, compare_func::less_equal);\n" \
"  for (int k = 0; k < 24; k++) { float2 o = clamp(st + lvog(k, 24, rot) * (wr * ts), lo, hi); vis += atl.sample_compare(cs, o, zr + dot(gz, o - st)); }\n" \
"  return vis / 24.0;\n" \
"}\n" \
/* the lamps' light leaving a surface toward v (cd/m2 before exposure): only \
   the lights in the point's cluster, each shadowed from its tile, the \
   shadow's discs turned by phi (radians: the pixel's noise); brdf is the \
   lighting's (brr_metal_lit.h), bE its table */ \
"static float3 brdf(float3 n, float3 v, float3 l, float3 alb, float rough, texture2d<float> bE);\n" \
"static float3 lamps(const device LSet &s, depth2d<float> atl, float3 p, float3 n, float3 v, float3 alb, float rough, texture2d<float> bE,\n" \
"                    float phi) {\n" \
"  uint e = s.cl[lc_index(s.h, p)], o = e >> 8, cnt = e & 255u; float3 Lo = float3(0); float2 rot = float2(cos(phi), sin(phi));\n" \
"  for (uint k = 0; k < cnt; k++) {\n" \
"    uint i = s.idx[o + k]; LS l = s.l[i];\n" \
"    float3 L = float3(l.pos) - p; float d2 = max(dot(L, L), 0.01), dl = sqrt(d2); float3 ld = L / dl;\n" \
"    if (dot(n, ld) <= 0.0) continue;\n" \
"    float E = lamp(l, -ld) * lwin(l, -ld, dl) / d2;\n" \
"    if (E <= 0.0) continue;\n" \
"    Lo += float3(l.rgb) * E * lampvis(s, atl, i, p, n, ld, rot) * brdf(n, v, ld, alb, rough, bE);\n" \
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
