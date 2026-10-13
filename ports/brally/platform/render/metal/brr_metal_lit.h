/* brr_metal_lit.h: the Remastered lighting's shaders (BR_FLAG_LIGHTING;
 * docs/brally/remaster/lighting/DESIGN.md).  Appended to brr_metal.m's
 * source (they share its combiner functions and vertex stage) and compiled
 * with it at start-up.
 *
 * A lit frame is the game's draws replayed in passes (brr_metal.m):
 *
 *   G-buffer     the camera's opaque 3D: albedo (the game's colour with its
 *                baked light taken out; a: 3D and how much it glows), world
 *                position (w: fog colour), normal (w: fog amount), depth
 *   lighting     linear HDR, physical units: the painted backdrop where there
 *                is no geometry (until the sky is decided), then every
 *                G-buffer pixel lit by the sun (shadowed), the sky, the
 *                ground's light, every lamp, and its own emission
 *   transparent  the game's blended 3D (decals, windows, foliage, smoke) in
 *                its own order, depth-tested, forward-lit by the same lights
 *   bloom        a 13-tap downsample and tent upsample chain (Jimenez 2014)
 *   post         bloom mixed in, the metered exposure, Khronos PBR Neutral,
 *                sRGB, and the luminance histogram for the next frame's meter
 *
 * then the game's 2D, mirror and LFB writes over the result as it drew them. */
#ifndef BRR_METAL_LIT_H
#define BRR_METAL_LIT_H

#include "brr_metal_refl.h"

#include "brr_metal_gi_src.h"

/* the lighting's constants (LU in brr_metal.m: the same layout).  The sun's
 * cascades: their matrices, where each ends in view depth, metres per texel,
 * one texel in depth units, metres per unit of depth; csoft: tan of the
 * sun's angular radius, the blocker search's and the penumbra filter's
 * widest radius in texels, the contact shadow's march in metres (0: off);
 * scr: world to the target's pixels (brr_shadow.h, brr_screen_matrix); gi.x:
 * the irradiance probes on (brr_metal_gi.h), which then replace the sky's
 * ambient term */
#define BRR_LU_SRC \
"struct LU { float4x4 sunvp[4]; float4 csplit; float4 cfwd; float4 ctexel; float4 cbias; float4 eye; float4 sun; float4 sunc; float4 skyc; float4 gndc; float4 misc; float4 wb; float4 bg; float4 fogc;\n" \
"            float4 cdepth; float4 csoft; float4x4 scr; float4 vol; float4 gi; float4 wet; };\n"

/* the sun's shadows (DESIGN.md 7.4), needing only LU (and so compiled alone
 * by ports/brally/tools/lighttest against synthetic maps) */
#define BRR_SHADOW_SRC \
/* interleaved gradient noise (Jimenez 2014): a pixel's own rotation of the \
   sample discs, so neighbouring pixels see different samples */ \
"static float ign(float2 px) { return fract(52.9829189 * fract(dot(px, float2(0.06711056, 0.00583715)))); }\n" \
/* point i of n of a Vogel (golden angle) disc of radius 1, turned by the \
   angle whose cosine and sine are rot: even cover of the disc for any n. \
   The point's own angle is a constant once the loops unroll; only the \
   pixel's turn is computed, once */ \
"static float2 vogel(int i, int n, float2 rot) {\n" \
"  float r = sqrt((float(i) + 0.5) / float(n)), t = float(i) * 2.39996323; float2 v = r * float2(cos(t), sin(t));\n" \
"  return float2(v.x * rot.x - v.y * rot.y, v.x * rot.y + v.y * rot.x); }\n" \
/* the sun's visibility from cascade k, a disc of the sun 0.535 degrees \
 * across (percentage-closer soft shadows, Fernando 2005, for a directional \
 * light): the receiver pushed along its normal by one to three texels and \
 * compared one texel nearer, both growing as the surface turns from the sun \
 * (slope-scaled); a blocker search over a disc as wide as the sun's \
 * penumbra from the farthest possible caster (the window reaches back to \
 * it: depth 0), the blockers' mean depth, the penumbra's radius from that \
 * distance, D x tan(sun's radius), and the filtered comparisons over a disc \
 * that wide: for a straight edge that is the share of the sun's disc in \
 * view, the physical penumbra.  Widths clamped to sane texel counts; the \
 * bilinear comparison adds its one texel. */ \
"static float casvis(constant LU &u, depth2d_array<float> sm, float3 p, float3 n, int k, float2 rot) {\n" \
"  constexpr sampler cs(filter::linear, address::clamp_to_edge, compare_func::less_equal);\n" \
"  constexpr sampler ps(filter::nearest, address::clamp_to_edge);\n" \
"  float nl = clamp(dot(n, u.sun.xyz), 0.05, 1.0), tn = min(sqrt(1.0 - nl * nl) / nl, 8.0);\n" \
"  float4 c = u.sunvp[k] * float4(p + n * u.ctexel[k] * (1.0 + 2.0 * (1.0 - nl)), 1.0); float2 st = float2(c.x * 0.5 + 0.5, 0.5 - c.y * 0.5);\n" \
"  if (any(st <= 0.0) || any(st >= 1.0) || c.z >= 1.0) return 1.0;\n" \
"  float tx = 1.0 / float(sm.get_width()), zr = c.z - u.cbias[k] * (1.0 + tn);\n" \
"  float tpd = u.cdepth[k] * u.csoft.x / u.ctexel[k];   /* penumbra radius in texels per unit of depth from blocker to receiver */\n" \
"  float rs = clamp(c.z * tpd, 1.0, u.csoft.y), zb = 0.0, nb = 0.0;\n" \
"  for (int i = 0; i < 16; i++) {\n" \
"    float4 g = sm.gather(ps, st + vogel(i, 16, rot) * (rs * tx), uint(k)), b = select(float4(0.0), float4(1.0), g < zr);\n" \
"    zb += dot(g, b); nb += dot(b, float4(1.0));\n" \
"  }\n" \
"  if (nb == 0.0) return 1.0;\n" \
"  float wr = min((c.z - zb / nb) * tpd, u.csoft.z), vis = 0.0;\n" \
"  for (int i = 0; i < 24; i++) vis += sm.sample_compare(cs, st + vogel(i, 24, rot) * (wr * tx), uint(k), zr);\n" \
"  return vis / 24.0;\n" \
"}\n" \
/* the cascade by view depth, the next one blended in over the last tenth of \
   each, faded out at the end of the last */ \
"static float sunvis(constant LU &u, depth2d_array<float> sm, float3 p, float3 n, float phi) {\n" \
"  if (u.misc.y < 0.5) return 1.0;\n" \
"  float d = dot(p - u.eye.xyz, u.cfwd.xyz); int k = 0;\n" \
"  while (k < 3 && d > u.csplit[k]) k++;\n" \
"  if (d > u.csplit[3]) return 1.0;\n" \
"  float d0 = k ? u.csplit[k - 1] : 0.0, w = saturate((d - mix(d0, u.csplit[k], 0.9)) / max(0.1 * (u.csplit[k] - d0), 1e-3));\n" \
"  float2 rot = float2(cos(phi), sin(phi)); float vis = casvis(u, sm, p, n, k, rot);\n" \
"  if (w > 0.0) vis = mix(vis, k < 3 ? casvis(u, sm, p, n, k + 1, rot) : 1.0, w);\n" \
"  return vis;\n" \
"}\n" \
/* screen-space contact shadow: steps toward the sun over CONTACT_LEN \
 * (jittered by j), one for each pixel the march crosses on screen, at most \
 * 16, none when it stays within its own pixel (the far ground, where the \
 * cascades hold all there is); each step's point taken to its pixel and \
 * tested against \
 * the G-buffer's surface there: the surface seen is nearer the camera than \
 * the point by less than a thickness (two steps, and about four pixels of \
 * depth at 1080p) and stands off the receiver's own plane (so a flat \
 * receiver never shadows itself, whatever the pixel rounding).  A single \
 * depth layer cannot tell a point inside a thing from one just behind its \
 * silhouette: the thickness keeps that fringe to a pixel or two (lighttest \
 * measures it).  For what the cascades' texels are too coarse to hold (a \
 * wheel on the road, a kerb, a post's foot); hits fade out over the march's \
 * last quarter, where the cascades take over.  An empty pixel (no 3D: \
 * position 0) is passed over. */ \
"static float contact(constant LU &u, texture2d<float> pos, float3 p, float3 n, float j) {\n" \
"  if (u.csoft.w <= 0.0 || dot(n, u.sun.xyz) <= 0.0) return 1.0;\n" \
"  float2 wh = float2(pos.get_width(), pos.get_height());\n" \
"  float4 h0 = u.scr * float4(p, 1.0), h1 = u.scr * float4(p + u.sun.xyz * u.csoft.w, 1.0);\n" \
"  float lpx = h0.w > 1e-6 && h1.w > 1e-6 ? length(h1.xy / h1.w - h0.xy / h0.w) : 16.0;\n" \
"  if (lpx < 1.0) return 1.0;\n" \
"  int ns = int(min(ceil(lpx), 16.0));\n" \
"  for (int i = 0; i < ns; i++) {\n" \
"    float t = (float(i) + j) / float(ns) * u.csoft.w; float3 q = p + u.sun.xyz * t;\n" \
"    float4 h = u.scr * float4(q, 1.0); if (h.w <= 1e-6) return 1.0;\n" \
"    float2 s = h.xy / h.w; if (any(s < 0.0) || any(s >= wh)) return 1.0;\n" \
"    float3 P = pos.read(uint2(s)).xyz; if (all(P == 0.0)) continue;\n" \
"    float dq = dot(q - u.eye.xyz, u.cfwd.xyz), ds = dot(P - u.eye.xyz, u.cfwd.xyz);\n" \
"    if (ds < dq && dq - ds < 0.0625 + 0.004 * dq && dot(P - p, n) > 0.02 + 0.002 * dq) return saturate((t / u.csoft.w - 0.75) * 4.0);\n" \
"  }\n" \
"  return 1.0;\n" \
"}\n" \
/* the sun's shadow map: world triangles through a cascade's matrix */ \
"struct SO { float4 pos [[position]]; };\n" \
"vertex SO svs(uint i [[vertex_id]], const device packed_float3 *v [[buffer(0)]], constant float4x4 &m [[buffer(1)]]) {\n" \
"  SO o; o.pos = m * float4(float3(v[i]), 1.0); return o; }\n" \
/* a cascade's static casters copied from the cache into the moving casters' \
   tile (brr_shadow.h): depth for depth, k.x the cascade, k.yz the tile's \
   corner in the cache */ \
"struct SCD { float d [[depth(any)]]; };\n" \
"fragment SCD scopy(float4 pos [[position]], depth2d_array<float> c [[texture(0)]], constant uint4 &k [[buffer(0)]]) {\n" \
"  SCD o; o.d = c.read(uint2(pos.xy) + k.yz, k.x); return o; }\n"

#include "brr_metal_night.h"
#include "brr_metal_vol.h"

#define BRR_LIT_SRC \
BRR_LU_SRC \
BRR_WET_SRC \
BRR_GI_DEFS \
BRR_GI_SAMPLE_SRC \
BRR_VOL_AT_SRC \
"static float3 lin(float3 c) { return select(pow((c + 0.055) / 1.055, 2.4), c / 12.92, c <= 0.04045); }\n" \
"static float srgb1(float x) { return x <= 0.0031308 ? 12.92 * x : 1.055 * pow(x, 1.0 / 2.4) - 0.055; }\n" \
/* the albedo: the game's texture times its vertex colour's hue (the colour's \
   brightness is the game's baked light, which the lighting replaces), 0.85 \
   of it; the hue halved into the sRGB curve's range and linearised, then \
   scaled back by 1 / lin(0.5) = 4.672 so a grey vertex colour is a hue of 1 \
   (4.0, a gamma-2 reading of the curve, left grey at 0.856) */ \
"static float3 delit(float3 tex, float3 vc) {\n" \
"  float lv = dot(vc, float3(0.2126, 0.7152, 0.0722));\n" \
"  float3 hue = lv > 0.02 ? clamp(vc / lv, 0.0, 2.0) : float3(1);\n" \
"  return clamp(lin(tex) * lin(saturate(hue * 0.5)) * 4.672 * 0.85, 0.0, 0.9);\n" \
"}\n" \
/* the specular lobe: GGX with Smith visibility (height-correlated \
   approximation) and Schlick Fresnel (F0 0.04), without the n.l; the GGX \
   denominator's 1 - (n.h)^2 as |n x h|^2, exact in float for a smooth lobe */ \
"static float p5(float x) { float x2 = x * x; return x2 * x2 * x; }\n" \
"static float ggxspec(float3 n, float3 v, float3 l, float rough) {\n" \
"  float nl = clamp(dot(n, l), 0.0, 1.0), nv = clamp(dot(n, v), 1e-3, 1.0);\n" \
"  float3 h = normalize(l + v); float nh = max(dot(n, h), 0.0);\n" \
"  float a2 = max(rough * rough, 2e-3); a2 *= a2;\n" \
"  float3 nxh = cross(n, h); float d = nh * nh * a2 + dot(nxh, nxh), D = a2 / (3.14159 * d * d);\n" \
"  float Vs = 0.5 / max(nl * sqrt(nv * nv * (1.0 - a2) + a2) + nv * sqrt(nl * nl * (1.0 - a2) + a2), 1e-4);\n" \
"  return D * Vs * (0.04 + 0.96 * p5(1.0 - saturate(dot(h, v))));\n" \
"}\n" \
/* the lobe's directional albedo E(mu), sampling the normals visible from v \
   (Heitz 2018), ns x ns stratified: pdf(l) = G1(v) D(h) / (4 n.v), so a \
   sample weighs F G2 / G1, at most 1, even at grazing */ \
"static float ggxalb(float mu, float rough, int ns) {\n" \
"  float3 n = float3(0, 0, 1), v = float3(sqrt(max(1.0 - mu * mu, 0.0)), 0.0, mu);\n" \
"  float a = max(rough * rough, 2e-3), a2 = a * a, e = 0.0;\n" \
"  float g1 = 2.0 * mu / (mu + sqrt(a2 + (1.0 - a2) * mu * mu));\n" \
"  float3 vh = normalize(float3(a * v.x, a * v.y, v.z)), t1 = float3(0, 1, 0), t2 = cross(vh, t1); float s = 0.5 * (1.0 + vh.z);\n" \
"  for (int j = 0; j < ns; j++) for (int i = 0; i < ns; i++) {\n" \
"    float r = sqrt((j + 0.5) / ns), ph = 6.28318 * (i + 0.5) / ns, p1 = r * cos(ph), p2 = r * sin(ph);\n" \
"    p2 = (1.0 - s) * sqrt(max(1.0 - p1 * p1, 0.0)) + s * p2;\n" \
"    float3 nh = p1 * t1 + p2 * t2 + sqrt(max(1.0 - p1 * p1 - p2 * p2, 0.0)) * vh;\n" \
"    float3 h = normalize(float3(a * nh.x, a * nh.y, max(nh.z, 0.0))); float vhd = dot(v, h); float3 l = 2.0 * vhd * h - v;\n" \
"    float dd = h.z * h.z * a2 + (h.x * h.x + h.y * h.y), D = a2 / (3.14159 * dd * dd);\n" \
"    if (l.z > 0.0 && vhd > 0.0) e += ggxspec(n, v, l, rough) * l.z * 4.0 * mu / (g1 * D); }\n" \
"  return e / (ns * ns); }\n" \
/* GGX (above) and a Lambert diffuse coupled to it as Kelemen and \
   Szirmay-Kalos 2001 and Kulla and Conty 2017 couple them: the diffuse gets \
   the energy the lobe leaves on the way in and on the way out, (1 - E(n.l)) \
   (1 - E(n.v)) / (pi (1 - E_avg)), so white reflects at most all that \
   arrives at every angle and roughness (the weighting by 1 - F(h.v) it \
   replaces reflected up to 1.79 of it at grazing views); nothing with the \
   light or the eye below the surface (there l = -v made h = normalize(0); \
   radiance turns n to the eye, so the eye never is) */ \
"static float3 brdf0(float3 n, float3 v, float3 l, float3 alb, float rough, texture2d<float> bE) {\n" \
"  float nl = min(dot(n, l), 1.0); if (nl <= 0.0 || dot(n, v) < 0.0) return 0.0;\n" \
"  constexpr sampler s(filter::linear, address::clamp_to_edge); float2 sz = float2(bE.get_width(), bE.get_height());\n" \
"  float nv = clamp(dot(n, v), 1e-3, 1.0), ry = saturate((rough - 0.0447) / 0.9553);   /* the table from the roughness floor (a^2 = 2e-3) */\n" \
"  float2 ev = bE.sample(s, (0.5 + float2(sqrt(nv), ry) * (sz - 1.0)) / sz).rg; float el = bE.sample(s, (0.5 + float2(sqrt(nl), ry) * (sz - 1.0)) / sz).r;\n" \
"  float fd = (1.0 - ev.x) * (1.0 - el) / (3.14159 * max(1.0 - ev.y, 1e-3));\n" \
"  return (alb * fd + ggxspec(n, v, l, rough)) * nl;\n" \
"}\n" \
/* car paint, a clear coat over the paint (MEASUREMENTS.md, car paint): the \
   coat a GGX lobe of roughness BRR_COAT_ROUGH with F0 0.04 (ggxspec's), the \
   paint under it the coupled diffuse and lobe above at BRR_CAR_ROUGH, seen \
   through what the coat leaves: f = f_base (1 - Ec(n.v)) + f_coat, Ec the \
   coat's directional albedo (the table at its roughness), the albedo-scaled \
   form of Filament's f_base (1 - Fc) + f_coat, so white reflects at most all \
   that arrives (coat + transmitted base <= 1); a negative roughness asks for \
   it (radiance, lamps), -rough the paint's */ \
"static float coatE(float nv, texture2d<float> bE) {\n" \
"  constexpr sampler s(filter::linear, address::clamp_to_edge); float2 sz = float2(bE.get_width(), bE.get_height());\n" \
"  return bE.sample(s, (0.5 + float2(sqrt(clamp(nv, 1e-3, 1.0)), saturate((BRR_COAT_ROUGH - 0.0447) / 0.9553)) * (sz - 1.0)) / sz).r; }\n" \
"static float3 brdf(float3 n, float3 v, float3 l, float3 alb, float rough, texture2d<float> bE) {\n" \
"  if (rough >= 0.0) return brdf0(n, v, l, alb, rough, bE);\n" \
"  float nl = min(dot(n, l), 1.0); if (nl <= 0.0 || dot(n, v) < 0.0) return 0.0;\n" \
"  return brdf0(n, v, l, alb, -rough, bE) * (1.0 - coatE(dot(n, v), bE)) + ggxspec(n, v, l, BRR_COAT_ROUGH) * nl;\n" \
"}\n" \
BRR_SHADOW_SRC \
/* a surface's normal turned toward the eye (the game's faces are seen from \
   either side) */ \
"static float3 toeye(constant LU &u, float3 p, float3 n) { return dot(n, u.eye.xyz - p) < 0.0 ? -n : n; }\n" \
/* the light leaving a surface toward the eye, cd/m2, before exposure; n \
   toward the eye (toeye), sv how much of the sun reaches it; the lamps from \
   the point's cluster, shadowed from their atlas tiles (brr_metal_lights.h); \
   gi the probes' indirect irradiance (gi_irr, lux) where u.gi.x is on: the \
   diffuse's albedo / pi of it (nothing metallic) in place of the sky's term; \
   a negative rough is car paint (brdf), the coat's own mirror of the \
   surroundings the reflection pass's (brr_metal_refl.h) */ \
"static float3 radiance(constant LU &u, const device LSet &ls, depth2d<float> atl, float3 p, float3 n, float3 alb, float rough, float glow, float sv,\n" \
"                       texture2d<float> bE, float3 gi) {\n" \
"  float3 v = normalize(u.eye.xyz - p); bool coat = rough < 0.0; rough = abs(rough); wet(u, p, n, alb, rough);\n" \
"  float rb = coat ? -rough : rough, tb = coat ? 1.0 - coatE(dot(n, v), bE) : 1.0;   /* car paint: the coat's layering (brdf) */\n" \
"  float3 Lo = u.sunc.rgb * brdf(n, v, u.sun.xyz, alb, rb, bE) * sv;\n" \
"  Lo += alb / 3.14159 * tb * (u.gi.x > 0.5 ? gi : mix(u.gndc.rgb, u.skyc.rgb, n.z * 0.5 + 0.5));\n" \
"  Lo += lamps(ls, atl, p, n, v, alb, rb, bE);\n" \
"  Lo += alb / max(dot(alb, float3(0.2126, 0.7152, 0.0722)), 0.02) * glow * 100.0;   /* about 100 cd/m2: a lit window */\n" \
"  return Lo * u.wb.rgb;\n" \
"}\n" \
/* the game's fog amount, toward the air's own light at the horizon (the sky \
   model's) where there is a sky, else the game's fog colour */ \
"static float3 fogged(constant LU &u, float3 c, float fog, float3 fogc) { return mix(c, u.fogc.w > 0.5 ? u.fogc.rgb * u.misc.x : lin(fogc) * u.bg.x, fog); }\n" \
/* pre-exposure (as Frostbite and Unreal store HDR): light in cd/m2 times the \
   camera's exposure, so the half-float targets hold about 0..100 instead of \
   sunlit glints and skies past their 65504 (which overflowed to infinity and \
   NaN); held finite below it for the sun's own disc */ \
"static float3 preexp(constant LU &u, float3 c) { return min(c * u.misc.x, float3(60000.0)); }\n" \
/* a face's normal from the world position's screen derivatives; straight up \
   for a sliver too thin to have one (normalize(0) would be NaN) */ \
"static float3 facen(float3 wp) { float3 c = cross(dfdx(wp), dfdy(wp)); float l = length(c); return l > 1e-12 ? c / l : float3(0, 0, 1); }\n" \
/* the G-buffer */ \
"struct GB { float4 alb [[color(0)]]; float4 pos [[color(1)]]; float4 nrm [[color(2)]]; float d [[depth(any)]]; };\n" \
"fragment GB fsgb(O in [[stage_in]], constant U &u [[buffer(0)]], texture2d<float> t [[texture(0)]], sampler sm [[sampler(0)]],\n" \
"                 texture2d<float> em [[texture(1)]]) {\n" \
"  SH h = shade(in, u, t, sm); if (!h.keep) discard_fragment();\n" \
"  GB r; r.d = h.d;\n" \
"  float g = u.glow * (u.has_tex ? em.sample(sm, in.st).r : 0.0);\n" \
"  r.alb = float4(delit(h.tex.rgb, h.it.rgb), in.mat > 2.5 ? BRR_MAT_CAR / 255.0 : (128.0 + floor(saturate(g) * 127.0 + 0.5)) / 255.0);\n" \
"  uint fc = (uint(u.fog_color.r * 255.0) << 16) | (uint(u.fog_color.g * 255.0) << 8) | uint(u.fog_color.b * 255.0);\n" \
"  r.pos = float4(in.wp, float(fc)); r.nrm = float4(facen(in.wp), h.fog);\n" \
"  return r;\n" \
"}\n" \
/* the painted backdrop, in HDR at the exposure that shows it as the game drew it */ \
"fragment float4 fsbk(O in [[stage_in]], constant U &u [[buffer(0)]], texture2d<float> t [[texture(0)]], sampler sm [[sampler(0)]],\n" \
"                     constant LU &lu [[buffer(1)]]) {\n" \
"  SH h = shade(in, u, t, sm); if (!h.keep) discard_fragment();\n" \
"  return float4(lin(h.o.rgb) * lu.bg.x, 1);\n" \
"}\n" \
"struct LO { float4 pos [[position]]; float2 uv; };\n" \
"vertex LO lvs(uint i [[vertex_id]]) { float2 p = float2((i << 1) & 2, i & 2); LO o; o.pos = float4(p * 2.0 - 1.0, 0, 1); o.uv = float2(p.x, 1.0 - p.y); return o; }\n" \
/* the table brdf couples the diffuse through, 64 x 64 over (mu, roughness), \
   made once at start-up in two passes: r the lobe's albedo E(mu) (brdf_lut), \
   g its cosine-weighted mean over the hemisphere E_avg, integrated from the \
   table itself through the lookup brdf makes (brdf_avg), so the diffuse's \
   normalisation is that of what it reads; texel (i, j) holds mu = (i / 63)^2 \
   (fine at grazing, where E changes fastest) and roughness 0.0447 + 0.9553 \
   j / 63 (from where the lobe stops being held at its smoothest) */ \
"fragment float4 brdf_lut(LO in [[stage_in]]) {\n" \
"  float2 n = round(in.pos.xy / in.uv), x = (in.pos.xy - 0.5) / (n - 1.0);\n" \
"  return float4(ggxalb(max(x.x * x.x, 1e-3), 0.0447 + 0.9553 * x.y, 64), 0, 0, 1); }\n" \
"fragment float4 brdf_avg(LO in [[stage_in]], texture2d<float> A [[texture(0)]]) {\n" \
"  constexpr sampler s(filter::linear, address::clamp_to_edge); float2 sz = float2(A.get_width(), A.get_height());\n" \
"  uint2 px = uint2(in.pos.xy); float yv = (px.y + 0.5) / sz.y, avg = 0.0;\n" \
"  for (int k = 0; k < 1024; k++) { float m = (k + 0.5) / 1024.0; avg += A.sample(s, float2((0.5 + sqrt(m) * (sz.x - 1.0)) / sz.x, yv)).r * 2.0 * m / 1024.0; }\n" \
"  return float4(A.read(px).r, avg, 0, 1); }\n" \
/* the deferred lighting */ \
"fragment float4 lfs(LO in [[stage_in]], constant LU &u [[buffer(0)]], const device LSet &ls [[buffer(1)]],\n" \
"                    texture2d<float> alb [[texture(0)]], texture2d<float> pos [[texture(1)]], texture2d<float> nrm [[texture(2)]],\n" \
"                    depth2d_array<float> sm [[texture(3)]], depth2d<float> atl [[texture(4)]],\n" \
"                    texture2d<float> bE [[texture(5)]], constant GiU &gu [[buffer(2)]],\n" \
"                    texture2d<float> girr [[texture(6)]], texture2d<float> gdst [[texture(7)]]) {\n" \
"  uint2 px = uint2(in.pos.xy); float4 a = alb.read(px);\n" \
"  if (a.a < 0.25) discard_fragment();\n" \
"  float4 P = pos.read(px), N = nrm.read(px);\n" \
"  float glow = saturate((a.a * 255.0 - 128.0) / 127.0);\n" \
"  int dbg = int(u.misc.w);\n" \
"  float j = ign(in.pos.xy);\n" \
"  float3 n = toeye(u, P.xyz, N.xyz);\n" \
/* the sun's visibility: none to look up on a face turned from it (the BRDF \
   is zero there), and no contact march where the cascades already shadow \
   it fully */ \
"  float sv = dot(n, u.sun.xyz) > 0.0 ? sunvis(u, sm, P.xyz, n, j * 6.2831853) : 0.0;\n" \
"  if (sv > 0.0) sv = min(sv, contact(u, pos, P.xyz, n, j));\n" \
"  if (dbg == 1) return float4(a.rgb, 1);\n" \
"  if (dbg == 2) return float4((N.xyz * 0.5 + 0.5), 1);\n" \
"  if (dbg == 3) return float4(float3(sv), 1);\n" \
"  if (dbg == 5) return float4(float3(glow), 1);\n" \
"  if (dbg == 6) return float4(float3(contact(u, pos, P.xyz, n, j)), 1);\n" \
"  if (dbg == 7) return float4(lc_heat(ls, P.xyz), 1);\n" \
"  float3 gi = u.gi.x > 0.5 ? gi_irr(gu, girr, gdst, P.xyz, n, normalize(u.eye.xyz - P.xyz)) : float3(0);\n" \
"  if (dbg == 8) return float4(preexp(u, a.rgb / 3.14159 * gi), 1);\n" \
"  uint fc = uint(P.w); float3 fogc = float3((fc >> 16) & 255, (fc >> 8) & 255, fc & 255) / 255.0;\n" \
"  return float4(fogged(u, preexp(u, radiance(u, ls, atl, P.xyz, n, a.rgb, carpaint(a.a) ? -BRR_CAR_ROUGH : u.misc.z, glow, sv, bE, gi)), u.vol.x > 0.5 ? 0.0 : N.w, fogc), 1);\n" \
"}\n" \
/* the game's blended 3D, forward-lit; an additive draw is light it adds; vu, iv: the volumetric fog's (brr_vol_bind) */ \
"fragment float4 fsf(O in [[stage_in]], constant U &u [[buffer(0)]], texture2d<float> t [[texture(0)]], sampler sm [[sampler(0)]],\n" \
"                    texture2d<float> em [[texture(1)]], constant LU &lu [[buffer(1)]], const device LSet &ls [[buffer(2)]],\n" \
"                    depth2d_array<float> shm [[texture(3)]], depth2d<float> atl [[texture(4)]],\n" \
"                    texture2d<float> bE [[texture(5)]], constant GiU &gu [[buffer(3)]],\n" \
"                    texture2d<float> girr [[texture(6)]], texture2d<float> gdst [[texture(7)]],\n" \
"                    constant VU &vu [[buffer(4)]], texture3d<float> iv [[texture(8)]]) {\n" \
"  SH h = shade(in, u, t, sm); if (!h.keep) discard_fragment();\n" \
"  float g = u.glow * (u.has_tex ? em.sample(sm, in.st).r : 0.0);\n" \
"  float3 n = toeye(lu, in.wp, facen(in.wp)), c;\n" \
/* a lighthouse beam: its own colour as light, the game's gradient along it, \
   no face normal; under the volumetric fog the spot's in-scatter carries it */ \
"  if (u.beam > 0.0) c = lin(h.o.rgb) * u.beam * lu.wb.rgb * (lu.vol.x > 0.5 ? u.beamw : 1.0);\n" \
"  else if (u.addk > 0.0) c = lin(h.o.rgb) * u.addk * lu.wb.rgb;\n" \
"  else {\n" \
"    float sv = dot(n, lu.sun.xyz) > 0.0 ? sunvis(lu, shm, in.wp, n, ign(in.pos.xy) * 6.2831853) : 0.0;\n" \
"    float3 gi = lu.gi.x > 0.5 ? gi_irr(gu, girr, gdst, in.wp, n, normalize(lu.eye.xyz - in.wp)) : float3(0);\n" \
"    c = radiance(lu, ls, atl, in.wp, n, delit(h.tex.rgb, h.it.rgb), lu.misc.z, g, sv, bE, gi);\n" \
"  }\n" \
/* the volumetric fog on, the same as the opaque surfaces behind it take (brr_metal_vol.h), never the game's */ \
"  if (lu.vol.x > 0.5) return float4(vol_fwd(vu, iv, in.pos.xy, in.wp, lu.eye.xyz, preexp(lu, c), u.addk > 0.0 || u.beam > 0.0), h.o.a);\n" \
"  return float4(fogged(lu, preexp(lu, c), u.addk > 0.0 || u.beam > 0.0 ? 0.0 : h.fog, u.fog_color.rgb), h.o.a);\n" \
"}\n" \
/* bloom: a 13-tap downsample and a 3x3 tent upsample (Jimenez 2014) */ \
"fragment float4 bdn(LO in [[stage_in]], texture2d<float> t [[texture(0)]]) {\n" \
"  constexpr sampler s(filter::linear, address::clamp_to_edge); float2 d = 1.0 / float2(t.get_width(), t.get_height()), uv = in.uv;\n" \
"  float3 a = t.sample(s, uv + d * float2(-2, -2)).rgb, b = t.sample(s, uv + d * float2(0, -2)).rgb, c = t.sample(s, uv + d * float2(2, -2)).rgb;\n" \
"  float3 e = t.sample(s, uv + d * float2(-2, 0)).rgb, f = t.sample(s, uv).rgb, g = t.sample(s, uv + d * float2(2, 0)).rgb;\n" \
"  float3 hh = t.sample(s, uv + d * float2(-2, 2)).rgb, i = t.sample(s, uv + d * float2(0, 2)).rgb, j = t.sample(s, uv + d * float2(2, 2)).rgb;\n" \
"  float3 k = t.sample(s, uv + d * float2(-1, -1)).rgb, l = t.sample(s, uv + d * float2(1, -1)).rgb;\n" \
"  float3 m = t.sample(s, uv + d * float2(-1, 1)).rgb, n = t.sample(s, uv + d * float2(1, 1)).rgb;\n" \
"  float3 r = f * 0.125 + (a + c + hh + j) * 0.03125 + (b + e + g + i) * 0.0625 + (k + l + m + n) * 0.125;\n" \
"  return float4(select(r, float3(0), isnan(r) || isinf(r)), 1);\n" \
"}\n" \
"fragment float4 bup(LO in [[stage_in]], texture2d<float> t [[texture(0)]]) {\n" \
"  constexpr sampler s(filter::linear, address::clamp_to_edge); float2 d = 1.0 / float2(t.get_width(), t.get_height()), uv = in.uv;\n" \
"  float3 r = t.sample(s, uv).rgb * 4.0;\n" \
"  r += (t.sample(s, uv + d * float2(-1, 0)).rgb + t.sample(s, uv + d * float2(1, 0)).rgb + t.sample(s, uv + d * float2(0, -1)).rgb + t.sample(s, uv + d * float2(0, 1)).rgb) * 2.0;\n" \
"  r += t.sample(s, uv + d * float2(-1, -1)).rgb + t.sample(s, uv + d * float2(1, -1)).rgb + t.sample(s, uv + d * float2(-1, 1)).rgb + t.sample(s, uv + d * float2(1, 1)).rgb;\n" \
"  return float4(r / 16.0, 1);\n" \
"}\n" \
/* post: bloom, exposure, the meter's histogram, the Purkinje shift (brr_metal_night.h), Khronos PBR Neutral, sRGB */ \
BRR_NIGHT_SRC \
/* the meter's centre weighting (light_night.h light_meter_weight): 1 at the \
   frame's edge, 4 at its centre, 1 + 3 (1 - r^2) within the ellipse r < 1 */ \
"static uint mweight(float2 uv) { float2 q = (uv - 0.5) * 2.0; return 1u + uint(3.0 * saturate(1.0 - dot(q, q)) + 0.5); }\n" \
/* the grade for a high-key day (the weather's bg.z saturation, bg.w contrast \
   about the mid-grey 0.18, both 1 for none), on the tonemapped colour */ \
"static float3 grade(float3 c, float sat, float con) {\n" \
"  float y = dot(c, float3(0.2126, 0.7152, 0.0722)); c = max(mix(float3(y), c, sat), 0.0);\n" \
"  return 0.18 * pow(max(c, 1e-6) / 0.18, con); }\n" \
"static float3 neutral(float3 c) {\n" \
"  const float start = 0.8 - 0.04, desat = 0.15;\n" \
"  float x = min(c.r, min(c.g, c.b)); float off = x < 0.08 ? x - 6.25 * x * x : 0.04; c -= off;\n" \
"  float p = max(c.r, max(c.g, c.b)); if (p < start) return c;\n" \
"  float d = 1.0 - start, np = 1.0 - d * d / (p + d - start); c *= np / p;\n" \
"  float g = 1.0 - 1.0 / (desat * (p - np) + 1.0); return mix(c, float3(np), g); }\n" \
"fragment float4 pfs(LO in [[stage_in]], constant LU &u [[buffer(0)]], texture2d<float> hdr [[texture(0)]], texture2d<float> bl [[texture(1)]],\n" \
"                    texture2d<float> alb [[texture(2)]],\n" \
"                    device atomic_uint *hist [[buffer(1)]]) {\n" \
"  constexpr sampler s(filter::linear, address::clamp_to_edge);\n" \
"  uint2 px = uint2(in.pos.xy); float3 c = min(hdr.read(px).rgb, 65504.0);   /* past half float's range (the sun's disc): its largest */\n" \
"  if (int(u.misc.w) == 0) c = mix(c, bl.sample(s, in.uv).rgb * u.bg.y, 0.04);   /* the levels summed: their mean */\n" \
"  float Y = dot(c, float3(0.2126, 0.7152, 0.0722)) / max(u.misc.x, 1e-12);   /* the meter reads cd/m2: the exposure divided back out */\n" \
"  uint2 apx = uint2(float2(px) * float2(alb.get_width(), alb.get_height()) / float2(hdr.get_width(), hdr.get_height()));   /* the G-buffer at the lit 3D's size */\n" \
"  if (((px.x | px.y) & 3) == 0 && Y > 0.0 && alb.read(apx).a >= 0.25)   /* the meter reads lit surfaces only; 16 bins a stop */\n" \
"    atomic_fetch_add_explicit(&hist[uint(clamp((log2(max(Y, 1e-6)) + 10.0) * 16.0, 0.0, 511.0))],\n" \
"                              mweight(float2(px) / float2(hdr.get_width(), hdr.get_height())), memory_order_relaxed);   /* centre-weighted */\n" \
"  c = purkinje(c, u.misc.x);   /* the eye at night: rods, before the tonemap */\n" \
"  c = neutral(c);   /* pre-exposed already */\n" \
"  if (u.bg.z > 0.0) c = saturate(grade(c, u.bg.z, u.bg.w));\n" \
"  return float4(srgb1(c.r), srgb1(c.g), srgb1(c.b), 1);\n" \
"}\n"

#endif
