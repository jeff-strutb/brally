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

#include "brr_metal_env.h"
#include "brr_metal_ao.h"
#include "brr_metal_surf.h"
#include "brr_vsm.h"

/* the lighting's constants (LU in brr_metal.m: the same layout).  The sun's
 * cascades: their matrices, where each ends in view depth, metres per texel,
 * one texel in depth units, metres per unit of depth; csoft: tan of the
 * sun's angular radius, the blocker search's and the penumbra filter's
 * widest radius in texels, the contact shadow's march in metres (0: off);
 * scr: world to the target's pixels (brr_shadow.h, brr_screen_matrix); gi.x:
 * the irradiance probes on (brr_metal_gi.h), which then replace the sky's
 * ambient term; env: x the specular cube's last mip (brr_metal_env.h), y its
 * bearing's turn, z 1 with a skymap; sh: the skymap's irradiance harmonics
 * (light_sky.c), lux; sbias: the sun's shadow bias in texels (casvis): x the
 * receiver's offset along its normal, y more of it times the sine of its
 * angle from the sun, z the comparison's depth bias */
#define BRR_LU_SRC \
"struct LU { float4x4 sunvp[4]; float4 csplit; float4 cfwd; float4 ctexel; float4 cbias; float4 eye; float4 sun; float4 sunc; float4 skyc; float4 gndc; float4 misc; float4 wb; float4 bg; float4 fogc;\n" \
"            float4 cdepth; float4 csoft; float4x4 scr; float4 vol; float4 gi; float4 wet; float4 env; float4 sh[9]; float4 sbias; };\n"

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
 * bilinear comparison adds its one texel.  The bias, in texels (sbias): \
 * the receiver moved along its normal by x + y sin(angle from the sun) \
 * (normal offset, Holbert 2011: a move of a texels gives a / cos of depth \
 * margin, and a bilinear comparison's neighbours lie within tan of a \
 * texel, so a >= sin suffices), each sample compared against the \
 * receiver's own plane at that sample (receiver plane depth bias, Isidoro \
 * 2006: no depth margin grows with the kernel), and z more texels of \
 * depth for the map's rounding.  Nothing on the casters' side: a bias \
 * there, or one scaled by the angle's tangent, hides every caster less \
 * than several texels above its receiver. */ \
"static float casvis(constant LU &u, depth2d_array<float> sm, float3 p, float3 n, int k, float2 rot) {\n" \
"  constexpr sampler cs(filter::linear, address::clamp_to_edge, compare_func::less_equal);\n" \
"  constexpr sampler ps(filter::nearest, address::clamp_to_edge);\n" \
"  float4x4 m = u.sunvp[k]; float nl = clamp(dot(n, u.sun.xyz), 0.0, 1.0), sl = sqrt(1.0 - nl * nl);\n" \
"  float4 c = m * float4(p + n * u.ctexel[k] * (u.sbias.x + u.sbias.y * sl), 1.0); float2 st = float2(c.x * 0.5 + 0.5, 0.5 - c.y * 0.5);\n" \
"  if (any(st <= 0.0) || any(st >= 1.0) || c.z >= 1.0) return 1.0;\n" \
/* the receiver's plane in the map: depth per unit of st, from its normal \
   in the light's axes (rows of m: right, up and toward the light's far \
   side, scaled to clip units), its slope clamped to a tangent of 8 */ \
"  float3 rx = float3(m[0][0], m[1][0], m[2][0]), ry = float3(m[0][1], m[1][1], m[2][1]), rz = float3(m[0][2], m[1][2], m[2][2]);\n" \
"  float sx = length(rx), sz = length(rz), nz = max(-dot(n, rz) / sz, 0.125);\n" \
"  float2 dz = float2(dot(n, rx) / sx, -dot(n, ry) / sx) / nz * (2.0 / sx) * sz;\n" \
"  float tx = 1.0 / float(sm.get_width()), zr = c.z - u.cbias[k] * u.sbias.z;\n" \
"  float tpd = u.cdepth[k] * u.csoft.x / u.ctexel[k];   /* penumbra radius in texels per unit of depth from blocker to receiver */\n" \
"  float rs = clamp(c.z * tpd, 1.0, u.csoft.y), zb = 0.0, nb = 0.0;\n" \
"  for (int i = 0; i < 16; i++) {\n" \
"    float2 o = vogel(i, 16, rot) * (rs * tx); float4 g = sm.gather(ps, st + o, uint(k)), b = select(float4(0.0), float4(1.0), g < zr + dot(dz, o));\n" \
"    zb += dot(g, b); nb += dot(b, float4(1.0));\n" \
"  }\n" \
"  if (nb == 0.0) return 1.0;\n" \
"  float wr = min((c.z - zb / nb) * tpd, u.csoft.z), vis = 0.0;\n" \
"  for (int i = 0; i < 24; i++) { float2 o = vogel(i, 24, rot) * (wr * tx); vis += sm.sample_compare(cs, st + o, uint(k), zr + dot(dz, o)); }\n" \
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

#include "brr_metal_vol.h"

#define BRR_LIT_SRC \
BRR_LU_SRC \
BRR_WET_SRC \
BRR_GI_DEFS \
BRR_GI_SAMPLE_SRC \
BRR_VOL_AT_SRC \
BRR_ENV_SRC \
BRR_AO_SRC \
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
/* the draw's albedo: a natural surface's (u.tr > 0, lit_tint) its texture's \
   one chroma times the texel's luminance, 0.85 of it, as delit's grey; \
   else delit; then the material's scale */ \
"static float3 albedo(constant U &u, float3 tex, float3 vc) {\n" \
"  float3 a = u.tr > 0.0 ? float3(u.tr, u.tg, u.tb) * dot(lin(tex), float3(0.2126, 0.7152, 0.0722)) * 0.85 : delit(tex, vc);\n" \
"  return min(a * u.albk, 0.9); }\n" \
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
BRR_VSM_SRC \
/* a surface's normal turned toward the eye (the game's faces are seen from \
   either side) */ \
/* a face's normal from the world position's screen derivatives; straight up \
   for a sliver too thin to have one (normalize(0) would be NaN) */ \
"static float3 facen(float3 wp) { float3 c = cross(dfdx(wp), dfdy(wp)); float l = length(c); return l > 1e-12 ? c / l : float3(0, 0, 1); }\n" \
"static float3 toeye(constant LU &u, float3 p, float3 n) { return dot(n, u.eye.xyz - p) < 0.0 ? -n : n; }\n" \
/* the shading normal: the vertices' smooth one (vsn, light_smooth.h) on \
   the side of the face that is toward the eye, else the face's own */ \
"static float3 shadenrm(constant LU &u, float3 wp, float3 sn) {\n" \
"  float3 f = toeye(u, wp, facen(wp)); float l = length(sn);\n" \
"  if (!(l > 0.5)) return f;\n" \
"  sn /= l; return dot(sn, f) < 0.0 ? -sn : sn; }\n" \
/* the lobe's directional albedo E(n.v, roughness) (brdf_lut): the split \
   sum's second sum for F0 0.04 */ \
"static float envE(float nv, float rough, texture2d<float> bE) {\n" \
"  constexpr sampler s(filter::linear, address::clamp_to_edge); float2 sz = float2(bE.get_width(), bE.get_height());\n" \
"  return bE.sample(s, (0.5 + float2(sqrt(clamp(nv, 1e-3, 1.0)), saturate((rough - 0.0447) / 0.9553)) * (sz - 1.0)) / sz).r; }\n" \
/* the light leaving a surface toward the eye, cd/m2, before exposure; n \
   toward the eye (toeye), sv how much of the sun reaches it, ao how much of \
   the sky (1: open).  The sun through the BRDF; the sky's diffuse, its \
   irradiance harmonics (or the probes' gi where u.gi.x is on), through the \
   coupled diffuse, which under even light keeps 1 - E(n.v) of it (brdf0's \
   (1 - E(l)) / (1 - E_avg) averages to 1 over the hemisphere); the sky's \
   specular, the prefiltered cube along the mirror direction times E (the \
   split sum, Karis 2013), for car paint the coat's over the paint's; the \
   lamps from the point's cluster, shadowed from their atlas tiles \
   (brr_metal_lights.h); its own emission.  A negative rough is car paint \
   (brdf) */ \
/* a rough surface's specular from the irradiance probes, not the sky: past \
   BRR_GISPEC_R0 (where the reflections stop tracing, REFL_ROUGH_MAX) the \
   prefiltered sky gives way, by BRR_GISPEC_R1, to the probes' irradiance \
   about the mirror direction over pi (gis), the mean radiance of a cosine \
   lobe there, which sees the track's occlusion (Unreal's Lumen reuses its \
   diffuse radiance cache for rough reflections the same way).  The sky's \
   cube alone lit a tunnel's walls with the open sky.  Below, a lobe the \
   reflections never trace (rougher than REFL_TRACE_MAX, or car paint's \
   base under its traced coat) keeps the sky's cube times the probes' \
   occlusion of it: their irradiance about the mirror direction over the \
   open sky's (1 in the open, a tunnel's ceiling near 0) */ \
"#define BRR_GISPEC_R0 0.6\n#define BRR_GISPEC_R1 0.8\n#define BRR_TRACED_MAX " REFL_STR(REFL_TRACE_MAX) "\n" \
"static float3 radiance(constant LU &u, const device LSet &ls, depth2d<float> atl, float3 p, float3 n, float3 alb, float rough, float glow, float sv,\n" \
"                       texture2d<float> bE, float3 gi, float phi, texturecube<float> sp, float ao, float seal, float3 gis) {\n" \
"  float3 v = normalize(u.eye.xyz - p); bool coat = rough < 0.0; rough = abs(rough); wet(u, p, n, alb, rough, coat ? 1.0 : seal);\n" \
"  float rb = coat ? -rough : rough, nv = clamp(dot(n, v), 1e-3, 1.0), tb = coat ? 1.0 - coatE(nv, bE) : 1.0;   /* car paint: the coat's layering (brdf) */\n" \
"  float3 r = reflect(-v, n);\n" \
"  float3 Ls = u.sunc.rgb * brdf(n, v, u.sun.xyz, alb, rb, bE) * sv;\n" \
"  float3 La = alb / 3.14159 * tb * (1.0 - envE(nv, rough, bE)) * (u.gi.x > 0.5 ? gi : env_irr(u, n)) * ao_multi(ao, alb);\n" \
"  float3 es = env_spec(u, sp, r, rough);\n" \
"  if (u.gi.x > 0.5 && (coat || rough > BRR_TRACED_MAX)) {   /* a lobe the reflections never trace: the probes' occlusion of the sky */\n" \
"    float3 k = float3(0.2126, 0.7152, 0.0722); es *= saturate(dot(gis, k) / max(dot(env_irr(u, r), k), 1e-6));\n" \
"    if (rough > BRR_GISPEC_R0) es = mix(es, gis / 3.14159, saturate((rough - BRR_GISPEC_R0) / (BRR_GISPEC_R1 - BRR_GISPEC_R0))); }\n" \
"  float3 Le = u.env.z > 0.5 ? (es * envE(nv, rough, bE) * tb\n" \
"                               + (coat ? env_spec(u, sp, r, BRR_COAT_ROUGH) * coatE(nv, bE) : float3(0))) * ao_spec(ao, nv, rough) : float3(0);\n" \
"  float3 Ll = lamps(ls, atl, p, n, v, alb, rb, bE, phi);\n" \
"  float3 Lg = alb / max(dot(alb, float3(0.2126, 0.7152, 0.0722)), 0.02) * glow * 100.0;   /* about 100 cd/m2: a lit window */\n" \
/* BR_LIGHT_VIEW 9..12, 14 (diagnostic): one term alone, the sun or moon, the sky's diffuse, the lamps, the glow, the sky's specular */ \
"  int dv = int(u.misc.w);\n" \
"  float3 Lo = dv == 9 ? Ls : dv == 10 ? La : dv == 11 ? Ll : dv == 12 ? Lg : dv == 14 ? Le : Ls + La + Le + Ll + Lg;\n" \
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
/* a light source's own surface as the frame shows it: at most BRR_EMIT_MAX \
   (a stop over the display's white, where PBR Neutral still keeps a red \
   red) in its brightest channel, its hue \
   kept.  Playground's rule for Forza's emissives, a few stops over the \
   scene's light: at physical luminance a lamp's face is thousands of times \
   the night's exposure and every colour reaches the display white (PBR \
   Neutral desaturates what passes it).  The light it casts stays physical */ \
"#define BRR_EMIT_MAX 2.0\n" \
/* the light past the cap the bloom is given (brr_metal.m s_emit): four stops more, a lamp's glow about \
   it; at 1000 a signal head's or a lit window's bloom swamped its own colour and drew a white ball */ \
"#define BRR_GLOW_MAX 128.0\n" \
"static float3 emitcap(float3 c) { float m = max(c.r, max(c.g, c.b)); return m > BRR_EMIT_MAX ? c * (BRR_EMIT_MAX / m) : c; }\n" \
/* the light past the cap, for the bloom, at most BRR_GLOW_MAX in its brightest channel with its hue \
   kept (each channel clamped alone, a stop lamp's red reached the cap while its green kept rising: \
   its bloom came out yellow-white) */ \
"static float3 glowcap(float3 c) { float3 e = max(c - emitcap(c), 0.0); float m = max(e.r, max(e.g, e.b)); return m > BRR_GLOW_MAX ? e * (BRR_GLOW_MAX / m) : e; }\n" \
/* the G-buffer */ \
"struct GB { float4 alb [[color(0)]]; float4 pos [[color(1)]]; float4 nrm [[color(2)]]; float d [[depth(any)]]; };\n" \
"fragment GB fsgb(O in [[stage_in]], constant U &u [[buffer(0)]], texture2d<float> t [[texture(0)]], sampler sm [[sampler(0)]],\n" \
"                 texture2d<float> em [[texture(1)]], constant LU &lu [[buffer(2)]], constant SurfU &su [[buffer(3)]],\n" \
"                 texture2d<float> smap [[texture(11)]], texture2d<float> svar [[texture(12)]],\n" \
"                 texture2d_array<float> sseta [[texture(13)]], texture2d_array<float> ssetb [[texture(14)]]) {\n" \
"  SH h = shade(in, u, t, sm);\n" \
"  if (!h.keep && !(su.flags.x > 0.5 && su.misc.x > 0.5)) discard_fragment();   /* a foliage card: its own crisp silhouette below */\n" \
"  GB r; r.d = h.d;\n" \
"  float g = u.glow * (u.has_tex ? em.sample(sm, in.st).r : 0.0);\n" \
"  float mat = in.mat > 2.5 ? (u.lp3 > 0.5 ? BRR_MAT_RUBBER : BRR_MAT_CAR) : u.lamp > 0.5 ? BRR_MAT_LAMP : 128.0 + floor(saturate(g) * 127.0 + 0.5);   /* lp3: a tyre's texture (texmat rubber) */\n" \
"  if (mat == 128.0 && u.lp1 > 0.5) mat = BRR_MAT_SEALED;   /* sealed ground, no glow (lp1: texmat, brr_metal.m) */\n" \
/* a car's lamp lens, where its texture paints it (carlamp.csv): the lens pass lights exactly these texels */ \
"  if (in.mat > 2.5 && u.lensk.x > 0.5 && all(in.st >= u.lens0.xy) && all(in.st < u.lens0.zw)) mat = BRR_MAT_LENS + 4.0 * (u.lensk.x - 1.0);\n" \
"  if (in.mat > 2.5 && u.lensk.y > 0.5 && all(in.st >= u.lens1.xy) && all(in.st < u.lens1.zw)) mat = BRR_MAT_LENS + 4.0 * (u.lensk.y - 1.0);\n" \
"  float3 alb = albedo(u, h.tex.rgb, h.it.rgb), nn = shadenrm(lu, in.wp, in.nrm);\n" \
/* the Remastered textures (brr_metal_surf.h): a draw whose texture has a surface map */ \
"  if (su.flags.w == 2.0 && su.flags.x < 0.5) alb = float3(0);   /* BR_SURF_VIEW=2: no surface, black */\n" \
"  if (su.flags.x > 0.5) {\n" \
"    SurfO so = surfshade(su, smap, svar, sseta, ssetb, in.st, in.wp, nn, alb, h.it.rgb);\n" \
"    if (so.a >= 0.0 && so.a < 0.5) discard_fragment();\n" \
"    alb = so.alb; float3 n2 = normalize(nn + so.dn); nn = dot(n2, nn) > 0.2 ? n2 : nn;\n" \
"  }\n" \
"  r.alb = float4(alb, mat / 255.0);\n" \
"  uint fc = (uint(u.fog_color.r * 255.0) << 16) | (uint(u.fog_color.g * 255.0) << 8) | uint(u.fog_color.b * 255.0);\n" \
"  r.pos = float4(in.wp, float(fc)); r.nrm = float4(nn, h.fog);   /* toward the eye */\n" \
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
"                    texture2d<float> bE [[texture(5)]], constant GiU &gu [[buffer(2)]], device const float4 *goff [[buffer(6)]],\n" \
"                    device const float4 *gsh [[buffer(7)]], texture2d<float> gdst [[texture(7)]], texturecube<float> sp [[texture(9)]],\n" \
"                    texture2d<float> aot [[texture(10)]], constant VSU &vsu [[buffer(3)]], const device uint *vent [[buffer(4)]],\n" \
"                    const device uint *vdyn [[buffer(5)]], texture2d<float> vpool [[texture(11)]], depth2d<float> vdp [[texture(12)]],\n" \
"                    depth2d<float> vtm [[texture(13)]]) {\n" \
"  uint2 px = uint2(in.pos.xy); float4 a = alb.read(px);\n" \
"  if (a.a < 0.25) discard_fragment();\n" \
"  float4 P = pos.read(px), N = nrm.read(px);\n" \
"  float glow = saturate((a.a * 255.0 - 128.0) / 127.0);\n" \
"  int dbg = int(u.misc.w);\n" \
"  float j = ign(in.pos.xy);\n" \
"  float3 n = N.xyz;   /* toward the eye already (fsgb) */\n" \
/* the sun's visibility: none to look up on a face turned from it (the BRDF \
   is zero there), and no contact march where the cascades already shadow \
   it fully */ \
"  float sv = dot(n, u.sun.xyz) <= 0.0 ? 0.0 : vsu.on.x > 0.5 ? vsmvis(vsu, u.sbias, u.csoft, vent, vdyn, vpool, vdp, vtm, P.xyz, n,\n" \
"             fract(j + 0.618034 * float(vsu.cnt.x & 1023u)), float2(cos(j * 6.2831853), sin(j * 6.2831853))) : sunvis(u, sm, P.xyz, n, j * 6.2831853);\n" \
"  if (sv > 0.0) sv = min(sv, contact(u, pos, P.xyz, n, j));\n" \
"  if (dbg == 1) return float4(a.rgb, 1);\n" \
"  if (dbg == 2) return float4((N.xyz * 0.5 + 0.5), 1);\n" \
"  if (dbg == 3) return float4(float3(sv), 1);\n" \
"  if (dbg == 5) return float4(float3(glow), 1);\n" \
"  if (dbg == 6) return float4(float3(contact(u, pos, P.xyz, n, j)), 1);\n" \
"  if (dbg == 7) return float4(lc_heat(ls, P.xyz), 1);\n" \
"  float rin = carpaint(a.a) ? BRR_CAR_ROUGH : rubbermat(a.a) ? BRR_RUBBER_ROUGH : u.misc.z; float3 ve = normalize(u.eye.xyz - P.xyz), gi = 0.0, gis = 0.0;\n" \
"  if (u.gi.x > 0.5) gi = carpaint(a.a) || rin > BRR_TRACED_MAX ? gi_irr2(gu, goff, gsh, gdst, P.xyz, n, ve, reflect(-ve, n), gis)   /* wet() only smooths */\n" \
"                                                               : gi_irr(gu, goff, gsh, gdst, P.xyz, n, ve);\n" \
"  if (dbg == 8) return float4(preexp(u, a.rgb / 3.14159 * gi), 1);\n" \
"  float ao = u.env.w > 0.5 ? ao_at(aot, pos, px, P.xyz, n) : 1.0;   /* env.w: the occlusion ran */\n" \
"  if (dbg == 15) return float4(float3(ao), 1);\n" \
"  uint fc = uint(P.w); float3 fogc = float3((fc >> 16) & 255, (fc >> 8) & 255, fc & 255) / 255.0;\n" \
"  float3 L = radiance(u, ls, atl, P.xyz, n, a.rgb, carpaint(a.a) ? -BRR_CAR_ROUGH : rin, 0.0, sv, bE, gi, j * 6.2831853, sp, ao, sealedmat(a.a) ? 1.0 : 0.0, gis);\n" \
"  float3 E = a.rgb / max(dot(a.rgb, float3(0.2126, 0.7152, 0.0722)), 0.02) * glow * 100.0;   /* a lit window, about 100 cd/m2 */\n" \
"  if (lampmat(a.a)) E += a.rgb / 0.85 * BRR_LAMP_L;   /* a lamp's own light */\n" \
"  return float4(fogged(u, preexp(u, L) + emitcap(preexp(u, E * u.wb.rgb)), u.vol.x > 0.5 ? 0.0 : N.w, fogc), 1);\n" \
"}\n" \
/* the game's blended 3D, forward-lit; an additive draw is light it adds; vu, iv: the volumetric fog's \
   (brr_vol_bind).  Its depth is the game's own (shade's h.d), as the \
   G-buffer's opaque 3D wrote it: the rasteriser's z is another encoding, \
   and tested against the G-buffer's it passed and failed at random */ \
"struct FF { float4 c [[color(0)]]; float d [[depth(any)]]; };\n" \
"fragment FF fsf(O in [[stage_in]], constant U &u [[buffer(0)]], texture2d<float> t [[texture(0)]], sampler sm [[sampler(0)]],\n" \
"                    texture2d<float> em [[texture(1)]], constant LU &lu [[buffer(1)]], const device LSet &ls [[buffer(2)]],\n" \
"                    depth2d_array<float> shm [[texture(3)]], depth2d<float> atl [[texture(4)]],\n" \
"                    texture2d<float> bE [[texture(5)]], constant GiU &gu [[buffer(3)]], device const float4 *goff [[buffer(6)]],\n" \
"                    device const float4 *gsh [[buffer(7)]], texture2d<float> gdst [[texture(7)]],\n" \
"                    constant VU &vu [[buffer(4)]], texture3d<float> iv [[texture(8)]], texturecube<float> sp [[texture(9)]],\n" \
"                    constant VSU &vsu [[buffer(5)]], const device uint *vent [[buffer(8)]], const device uint *vdyn [[buffer(9)]],\n" \
"                    texture2d<float> vpool [[texture(11)]], depth2d<float> vdp [[texture(12)]], depth2d<float> vtm [[texture(13)]],\n" \
"                    constant SurfU &su [[buffer(10)]], texture2d<float> smap [[texture(14)]], texture2d<float> svar [[texture(15)]],\n" \
"                    texture2d_array<float> sseta [[texture(16)]], texture2d_array<float> ssetb [[texture(17)]]) {\n" \
"  SH h = shade(in, u, t, sm);\n" \
"  if (!h.keep && !(su.flags.x > 0.5 && su.misc.x > 0.5)) discard_fragment();   /* a foliage card: its own crisp silhouette below */\n" \
"  float g = u.glow * (u.has_tex ? em.sample(sm, in.st).r : 0.0);\n" \
"  float3 n = shadenrm(lu, in.wp, in.nrm), c, ge = 0.0;\n" \
/* the Remastered textures (brr_metal_surf.h): a blended draw whose texture has a surface map */ \
"  SurfO so; so.alb = float3(0); so.dn = float3(0); so.a = -1.0; bool sf = su.flags.x > 0.5;\n" \
"  if (sf) { so = surfshade(su, smap, svar, sseta, ssetb, in.st, in.wp, n, albedo(u, h.tex.rgb, h.it.rgb), h.it.rgb);\n" \
"    if (so.a >= 0.0 && so.a < 0.5) discard_fragment();\n" \
"    float3 n2 = normalize(n + so.dn); n = dot(n2, n) > 0.2 ? n2 : n; }\n" \
/* a lighthouse beam: its own colour as light, the game's gradient along it, \
   no face normal; under the volumetric fog the spot's in-scatter carries it */ \
"  if (u.beam > 0.0) c = lin(h.o.rgb) * u.beam * lu.wb.rgb * (lu.vol.x > 0.5 ? u.beamw : 1.0);\n" \
"  else if (u.addk > 0.0) c = lin(h.o.rgb) * u.addk * lu.wb.rgb;\n" \
"  else if (u.lamp > 0.5) c = lin(h.o.rgb) * BRR_LAMP_L * lu.wb.rgb;   /* a lamp's flare: light */\n" \
"  else {\n" \
"    float jf = ign(in.pos.xy), sv = dot(n, lu.sun.xyz) <= 0.0 ? 0.0 : vsu.on.x > 0.5 ? vsmvis(vsu, lu.sbias, lu.csoft, vent, vdyn, vpool, vdp, vtm, in.wp, n,\n" \
"               fract(jf + 0.618034 * float(vsu.cnt.x & 1023u)), float2(cos(jf * 6.2831853), sin(jf * 6.2831853))) : sunvis(lu, shm, in.wp, n, jf * 6.2831853);\n" \
"    float3 ve = normalize(lu.eye.xyz - in.wp), gi = 0.0, gis = 0.0;\n" \
"    if (lu.gi.x > 0.5) gi = lu.misc.z > BRR_TRACED_MAX ? gi_irr2(gu, goff, gsh, gdst, in.wp, n, ve, reflect(-ve, n), gis) : gi_irr(gu, goff, gsh, gdst, in.wp, n, ve);\n" \
/* a flat decal (lp2, brr_metal.m: a tire mark): its colour the game's own combine of texture and \
   vertex colour, the paint it laid, where delit would divide the vertex colour out as baked light */ \
"    float3 al = sf ? so.alb : u.lp2 > 0.5 ? min(lin(h.o.rgb) * 0.85, 0.9) : albedo(u, h.tex.rgb, h.it.rgb);\n" \
"    c = radiance(lu, ls, atl, in.wp, n, al, lu.misc.z, 0.0, sv, bE, gi, ign(in.pos.xy) * 6.2831853, sp, 1.0, u.lp1, gis);\n" \
"    ge = al / max(dot(al, float3(0.2126, 0.7152, 0.0722)), 0.02) * g * 100.0 * lu.wb.rgb;   /* a lit window's own light */\n" \
"  }\n" \
"  bool emit = u.addk > 0.0 || u.beam > 0.0 || u.lamp > 0.5;\n" \
"  float3 cp = emit ? emitcap(preexp(lu, c)) : preexp(lu, c) + emitcap(preexp(lu, ge));\n" \
/* the volumetric fog on, the same as the opaque surfaces behind it take (brr_metal_vol.h), never the game's */ \
"  FF r; r.d = h.d;\n" \
/* a flat decal's alpha as the game meant it: coverage in display (gamma) space, where its renderer \
   blended (half a black mark halves the road's display value); in linear light that is \
   1 - (1 - a)^2.2, exact for a black decal (a tire mark: 0.5 -> 0.78) */ \
"  float oa = u.lp2 > 0.5 ? 1.0 - pow(1.0 - saturate(h.o.a), 2.2) : h.o.a;\n" \
"  if (lu.vol.x > 0.5) r.c = float4(vol_fwd(vu, iv, in.pos.xy, in.wp, lu.eye.xyz, cp, emit), oa);\n" \
"  else r.c = float4(fogged(lu, cp, emit ? 0.0 : h.fog, u.fog_color.rgb), oa);\n" \
"  return r;\n" \
"}\n" \
/* the cars' lamps seen: each lamp's lens (an ellipse LENS_HW x LENS_HH \
   about its position, facing along its axis) shows the light it sends \
   toward the eye over its area, L = I(v) / (A cos), I the same pattern and \
   candela that light the scene (lamp, light_scene.c): a brake lamp as \
   bright as the light it throws.  Additive, depth-tested with the game's \
   own encoding (wfloat of 1/w), after the blended 3D.  k.S: world to the \
   lit target's pixels (brr_screen_matrix, jittered as the frame's draws), \
   k.tgt: its size and the exposure */ \
"#define LENS_HW_HEAD 0.10\n#define LENS_HH_HEAD 0.06\n#define LENS_HW_REAR 0.09\n#define LENS_HH_REAR 0.05\n" \
/* a rear lamp's lens is its texture's (carlamp.csv, BRR_MAT_LENS in the G-buffer): the pass covers \
   LENS_SEARCH x the lens about where the lamp is placed and lights the lens texels there */ \
"#define LENS_SEARCH 3.0\n" \
/* a lit lens's face at least LENS_FLOOR x the square root of its candela over a tail lamp's 5, \
   in the exposed frame (1: the display's white): Playground's rule for Forza's emissives, a lamp's \
   face over the scene's light whatever the exposure.  By day a 5 cd tail lamp's own luminance \
   (350 cd/m2 over its lens) sat under an overcast road's and read as unlit; the floor puts it at \
   the display's red, a stop lamp (80 cd) four times over into the bloom.  At night the lamps' own \
   luminance is far over it; the light they cast is unchanged */ \
"#define LENS_FLOOR 0.6\n" \
"struct LNU { float4x4 S; float4 tgt; float4 eye; float4 wb; };\n" \
"struct LNO { float4 pos [[position]]; float2 q; float oow [[center_no_perspective]]; float3 wp; uint li [[flat]]; };\n" \
"vertex LNO lnvs(uint v [[vertex_id]], uint li [[instance_id]], const device LSet &ls [[buffer(0)]], constant LNU &k [[buffer(1)]]) {\n" \
"  LNO o; LS l = ls.l[li]; o.li = li; o.q = 0.0; o.oow = 0.0;\n" \
"  if (l.kind > 1.5) { o.pos = float4(0.0, 0.0, 0.0, 1.0); return o; }   /* not a car's lamp: no area */\n" \
"  float2 c = float2((v == 1 || v == 2 || v == 4) ? 1.0 : -1.0, (v == 2 || v == 4 || v == 5) ? 1.0 : -1.0);\n" \
"  float3 ax = float3(l.axis), sd = float3(l.side), up = cross(sd, ax);\n" \
"  float hw = l.kind < 0.5 ? LENS_HW_HEAD : LENS_HW_REAR, hh = l.kind < 0.5 ? LENS_HH_HEAD : LENS_HH_REAR;\n" \
"  hw *= LENS_SEARCH; hh *= LENS_SEARCH;   /* the area a lamp's painted lens is looked for in */\n" \
"  if ((uint(k.tgt.w) & 32u) != 0u) { hw *= 20.0; hh *= 20.0; }\n" \
"  float3 wp = float3(l.pos) + sd * (c.x * hw) + up * (c.y * hh); float4 h = k.S * float4(wp, 1.0); o.wp = wp;\n" \
"  if (!(h.w > 1e-3)) { o.pos = float4(0.0, 0.0, 0.0, 1.0); return o; }\n" \
"  float2 px = h.xy / h.w, ndc = float2(px.x / k.tgt.x * 2.0 - 1.0, 1.0 - px.y / k.tgt.y * 2.0);\n" \
"  o.pos = float4(ndc * h.w, 0.5 * h.w, h.w); o.q = c; o.oow = 1.0 / h.w; return o; }\n" \
"struct LNF { float4 c [[color(0)]]; float d [[depth(any)]]; };\n" \
/* BR_LIGHT_LENSPROBE (diagnostic): per lamp, its centre's pixel through the lens's screen \
   matrix and what the G-buffer holds there: pixel x, y, the material byte, then the \
   G-buffer's position and the lamp's, and their distances from the eye */ \
"kernel void lnprobe(uint li [[thread_position_in_grid]], const device LSet &ls [[buffer(0)]], constant LNU &k [[buffer(1)]],\n" \
"                    device float4 *out [[buffer(2)]], constant uint &n [[buffer(3)]], texture2d<float> alb [[texture(0)]], texture2d<float> pos [[texture(1)]]) {\n" \
"  if (li >= n) return; LS l = ls.l[li]; float4 h = k.S * float4(float3(l.pos), 1.0);\n" \
"  float2 px = h.w > 1e-3 ? h.xy / h.w : float2(-1.0); out[3 * li] = float4(px, -1.0, h.w);\n" \
"  out[3 * li + 1] = h; out[3 * li + 2] = float4(float3(l.pos), distance(k.eye.xyz, float3(l.pos)));\n" \
"  if (any(px < 0.0) || px.x >= k.tgt.x || px.y >= k.tgt.y) return;\n" \
"  uint2 p = uint2(px); float3 g = pos.read(p).xyz;\n" \
"  out[3 * li] = float4(px, alb.read(p).a * 255.0, h.w);\n" \
"  out[3 * li + 1] = float4(g, distance(k.eye.xyz, g));\n" \
"  out[3 * li + 2] = float4(float3(l.pos), distance(k.eye.xyz, float3(l.pos))); }\n" \
"fragment LNF lnfs(LNO in [[stage_in]], const device LSet &ls [[buffer(0)]], constant LNU &k [[buffer(1)]],\n" \
"                  texture2d<float> alb [[texture(0)]], texture2d<float> pos [[texture(1)]]) {\n" \
/* only on its car's body as drawn: car paint within 35 cm (a car the game \
   does not draw, the player's in the bumper and cockpit views, shows no lenses) */ \
"  uint dbg = uint(k.tgt.w);   /* BR_LIGHT_LENSDBG (diagnostic): 1 no paint gate, 2 no distance gate, 4 no facing gate, 8 solid magenta, 32 quads 20 x */\n" \
"  uint2 px = uint2(in.pos.xy); float3 gp = pos.read(px).xyz;\n" \
"  if ((dbg & 64u) != 0u) { LNF m; float dl = distance(k.eye.xyz, in.wp), dg = distance(k.eye.xyz, gp);\n" \
"    m.c = float4(dg < dl - 0.10 ? 4.0 : 0.0, carpaint(alb.read(px).a) ? 0.0 : 4.0, distance(gp, in.wp) > 0.35 ? 4.0 : 0.0, 1.0); m.d = 0.0; return m; }\n" \
"  LS l0 = ls.l[in.li]; float ma = alb.read(px).a;\n" \
/* a lamp lights its own lens's texels (carlamp.csv: the headlamp its lens, the reversing lamp \
   the white lens, the tail and stop lamp the red) */ \
"  bool lensok = lensmat(ma) == (l0.kind < 0.5 ? 3 : l0.rgb.g > 0.5 ? 2 : 1);\n" \
"  if (((dbg & 1u) == 0u && !lensok) || ((dbg & 2u) == 0u && distance(gp, in.wp) > 0.5)) discard_fragment();\n" \
"  LS l = ls.l[in.li]; float3 dv = normalize(k.eye.xyz - float3(l.pos)); float ct = dot(float3(l.axis), dv);\n" \
"  if ((dbg & 4u) == 0u && ct <= 0.0) discard_fragment();\n" \
"  if ((dbg & 8u) != 0u) { LNF m; m.c = float4(4.0, 0.0, 4.0, 1.0); m.d = float(wfloat(in.oow)) / 65536.0; return m; }\n" \
"  float A = 3.14159265 * (l.kind < 0.5 ? LENS_HW_HEAD * LENS_HH_HEAD : LENS_HW_REAR * LENS_HH_REAR);\n" \
"  float I = lamp(l, dv), Lf = LENS_FLOOR * sqrt(max(I, 0.0) / 5.0);\n" \
"  float3 raw = float3(l.rgb) * max(I / (A * max(ct, 0.1)) * k.tgt.z, Lf) * k.wb.rgb;\n" \
/* tgt.w bit 128: the emission target's draw, the light over the display's cap (the bloom's, brr_metal.m) */ \
"  LNF o; o.c = float4((dbg & 128u) != 0u ? glowcap(raw) : emitcap(raw), 1.0);\n" \
"  o.d = float(wfloat(in.oow)) / 65536.0; return o; }\n" \
/* the exposure's meter, on the GPU: the frame's scene luminance (the \
   pre-exposed HDR divided by its exposure, p.x) over a 64 x 36 grid, \
   centre-weighted (1 at the edges, 4 in the middle), its mean log2 into \
   out[0] (a reflected-light meter's reading, as Unreal's average meter \
   takes it), and the log2 under which 90% of the grid lies into out[1] \
   (a histogram of 64 bins, half a stop each, from 2^-12); one threadgroup of 256 */ \
"kernel void meter(uint t [[thread_index_in_threadgroup]], texture2d<float> hdr [[texture(0)]], constant float4 &p [[buffer(0)]],\n" \
"                  device float *out [[buffer(1)]]) {\n" \
"  constexpr sampler s(filter::linear, address::clamp_to_edge);\n" \
"  threadgroup float sa[256], sw[256]; threadgroup atomic_uint hb[64]; float a = 0.0, w = 0.0;\n" \
"  if (t < 64u) atomic_store_explicit(&hb[t], 0u, memory_order_relaxed); threadgroup_barrier(mem_flags::mem_threadgroup);\n" \
"  for (uint i = t; i < 64u * 36u; i += 256u) {\n" \
"    float2 uv = (float2(i % 64u, i / 64u) + 0.5) / float2(64.0, 36.0), q = (uv - 0.5) * 2.0;\n" \
"    float L = dot(hdr.sample(s, uv).rgb, float3(0.2126, 0.7152, 0.0722)) / max(p.x, 1e-12);\n" \
"    float k = 1.0 + 3.0 * saturate(1.0 - dot(q, q)); a += k * log2(max(L, 1e-5)); w += k;\n" \
"    atomic_fetch_add_explicit(&hb[uint(clamp((log2(max(L, 1e-5)) + 12.0) * 2.0, 0.0, 63.0))], 1u, memory_order_relaxed); }\n" \
"  sa[t] = a; sw[t] = w; threadgroup_barrier(mem_flags::mem_threadgroup);\n" \
"  for (uint st = 128u; st > 0u; st >>= 1) { if (t < st) { sa[t] += sa[t + st]; sw[t] += sw[t + st]; } threadgroup_barrier(mem_flags::mem_threadgroup); }\n" \
"  if (t == 0u) { out[0] = sa[0] / max(sw[0], 1e-6);\n" \
"    uint c = 0u, n90 = uint(0.9 * 64.0 * 36.0), b = 0u;\n" \
"    for (; b < 64u; b++) { c += atomic_load_explicit(&hb[b], memory_order_relaxed); if (c >= n90) break; }\n" \
"    out[1] = (float(b) + 1.0) * 0.5 - 12.0; } }\n" \
/* bloom: a 13-tap downsample and a 3x3 tent upsample (Jimenez 2014) */ \
/* the emission target: an emitter's light over the display's cap (emitcap), which the screen \
   does not show (its colour kept) and the bloom does (a lamp's glow); the G-buffer's emitters \
   here, as lfs makes them; the lenses' added by their pass */ \
"fragment float4 lfe(LO in [[stage_in]], constant LU &u [[buffer(0)]], texture2d<float> alb [[texture(0)]]) {\n" \
"  float4 a = alb.read(uint2(in.pos.xy)); if (a.a < 0.25) return float4(0);\n" \
"  float glow = saturate((a.a * 255.0 - 128.0) / 127.0);\n" \
"  float3 E = a.rgb / max(dot(a.rgb, float3(0.2126, 0.7152, 0.0722)), 0.02) * glow * 100.0;\n" \
"  if (lampmat(a.a)) E += a.rgb / 0.85 * BRR_LAMP_L;\n" \
"  float3 e = preexp(u, E * u.wb.rgb); return float4(glowcap(e), 1); }\n" \
"fragment float4 bdn(LO in [[stage_in]], texture2d<float> t [[texture(0)]], texture2d<float> em [[texture(1)]], constant float &emk [[buffer(0)]]) {\n" \
"  constexpr sampler s(filter::linear, address::clamp_to_edge); float2 d = 1.0 / float2(t.get_width(), t.get_height()), uv = in.uv;\n" \
"  float3 a = t.sample(s, uv + d * float2(-2, -2)).rgb, b = t.sample(s, uv + d * float2(0, -2)).rgb, c = t.sample(s, uv + d * float2(2, -2)).rgb;\n" \
"  float3 e = t.sample(s, uv + d * float2(-2, 0)).rgb, f = t.sample(s, uv).rgb, g = t.sample(s, uv + d * float2(2, 0)).rgb;\n" \
"  float3 hh = t.sample(s, uv + d * float2(-2, 2)).rgb, i = t.sample(s, uv + d * float2(0, 2)).rgb, j = t.sample(s, uv + d * float2(2, 2)).rgb;\n" \
"  float3 k = t.sample(s, uv + d * float2(-1, -1)).rgb, l = t.sample(s, uv + d * float2(1, -1)).rgb;\n" \
"  float3 m = t.sample(s, uv + d * float2(-1, 1)).rgb, n = t.sample(s, uv + d * float2(1, 1)).rgb;\n" \
"  float3 r = f * 0.125 + (a + c + hh + j) * 0.03125 + (b + e + g + i) * 0.0625 + (k + l + m + n) * 0.125;\n" \
"  if (emk > 0.5) r += em.sample(s, uv).rgb;   /* the first level: the emitters' light past the screen's cap */\n" \
"  r = select(r, float3(0), isnan(r) || isinf(r));\n" \
"  float y = dot(r, float3(0.2126, 0.7152, 0.0722)); if (y > 1000.0) r *= 1000.0 / y;   /* the sun's and moon's discs (to 60000) are the glare pass's: in the bloom too they were a second veil, a white blob about the moon */\n" \
"  return float4(r, 1);\n" \
"}\n" \
"fragment float4 bup(LO in [[stage_in]], texture2d<float> t [[texture(0)]]) {\n" \
"  constexpr sampler s(filter::linear, address::clamp_to_edge); float2 d = 1.0 / float2(t.get_width(), t.get_height()), uv = in.uv;\n" \
"  float3 r = t.sample(s, uv).rgb * 4.0;\n" \
"  r += (t.sample(s, uv + d * float2(-1, 0)).rgb + t.sample(s, uv + d * float2(1, 0)).rgb + t.sample(s, uv + d * float2(0, -1)).rgb + t.sample(s, uv + d * float2(0, 1)).rgb) * 2.0;\n" \
"  r += t.sample(s, uv + d * float2(-1, -1)).rgb + t.sample(s, uv + d * float2(1, -1)).rgb + t.sample(s, uv + d * float2(-1, 1)).rgb + t.sample(s, uv + d * float2(1, 1)).rgb;\n" \
"  return float4(r / 16.0, 1);\n" \
"}\n" \
/* post: bloom, Khronos PBR Neutral, sRGB (the exposure is the lit targets', pre-exposed) */ \
BRR_ENV_PASS_SRC \
/* Khronos PBR Neutral (2024): linear under 0.76, the highlights compressed \
   to 1 and desaturated toward white as they go, hue kept */ \
"static float3 neutral(float3 c) {\n" \
"  const float start = 0.8 - 0.04, desat = 0.15;\n" \
"  float x = min(c.r, min(c.g, c.b)); float off = x < 0.08 ? x - 6.25 * x * x : 0.04; c -= off;\n" \
"  float p = max(c.r, max(c.g, c.b)); if (p < start) return c;\n" \
"  float d = 1.0 - start, np = 1.0 - d * d / (p + d - start); c *= np / p;\n" \
"  float g = 1.0 - 1.0 / (desat * (p - np) + 1.0); return mix(c, float3(np), g); }\n" \
/* the weather's saturation (k_weather, brr_metal.m): Oklab chroma (Ottosson 2020) times s, its \
   lightness and hue kept, after the tonemap on display-linear colour, clipped to the display's gamut */ \
"static float3 oksat(float3 c, float s) {\n" \
"  if (abs(s - 1.0) < 1e-3) return c;\n" \
"  float3 lms = float3(0.4122214708 * c.r + 0.5363325363 * c.g + 0.0514459929 * c.b, 0.2119034982 * c.r + 0.6806995451 * c.g + 0.1073969566 * c.b,\n" \
"                      0.0883024619 * c.r + 0.2817188376 * c.g + 0.6299787005 * c.b);\n" \
"  lms = sign(lms) * pow(abs(lms), 1.0 / 3.0);\n" \
"  float L = 0.2104542553 * lms.x + 0.7936177850 * lms.y - 0.0040720468 * lms.z;\n" \
"  float a = (1.9779984951 * lms.x - 2.4285922050 * lms.y + 0.4505937099 * lms.z) * s;\n" \
"  float b = (0.0259040371 * lms.x + 0.7827717662 * lms.y - 0.8086757660 * lms.z) * s;\n" \
"  float3 q = float3(L + 0.3963377774 * a + 0.2158037573 * b, L - 0.1055613458 * a - 0.0638541728 * b, L - 0.0894841775 * a - 1.2914855480 * b);\n" \
"  q = q * q * q;\n" \
"  return saturate(float3(4.0767416621 * q.x - 3.3077115913 * q.y + 0.2309699292 * q.z, -1.2684380046 * q.x + 2.6097574011 * q.y - 0.3413193965 * q.z,\n" \
"                         -0.0041960863 * q.x - 0.7034186147 * q.y + 1.7076147010 * q.z)); }\n" \
"fragment float4 pfs(LO in [[stage_in]], constant LU &u [[buffer(0)]], texture2d<float> hdr [[texture(0)]], texture2d<float> bl [[texture(1)]],\n" \
"                    texture2d<float> galb [[texture(2)]]) {\n" \
"  constexpr sampler s(filter::linear, address::clamp_to_edge);\n" \
"  uint2 px = uint2(in.pos.xy); float3 c = min(hdr.read(px).rgb, 65504.0);   /* past half float's range (the sun's disc): its largest */\n" \
"  if (int(u.misc.w) == 0) c = mix(c, bl.sample(s, in.uv).rgb * u.bg.y, 0.04);   /* the levels summed: their mean */\n" \
"  c = neutral(c);   /* pre-exposed already */\n" \
/* the weather's saturation, the cars' their own (bg.w: their clear coat mirrors the pale \
   sky and lost more of the art's colour than the land; k_weather, brr_metal.m) */ \
"  if (int(u.misc.w) == 0) { constexpr sampler sn(filter::nearest, address::clamp_to_edge); float ma = galb.sample(sn, in.uv).a;\n" \
"    c = oksat(c, carpaint(ma) || lensmat(ma) > 0 ? u.bg.w : u.bg.z); }\n" \
"  return float4(srgb1(c.r), srgb1(c.g), srgb1(c.b), 1);\n" \
"}\n" \
/* the painted backdrop's scenery in front of the skymap: its colour as \
   albedo (0.85 of it, linear, as delit), facing the eye across the \
   horizontal, lit by the sky's harmonics and the sun, unshadowed, faded \
   toward the horizon's sky by k.misc.y (the air between: aerial \
   perspective at the backdrop's few kilometres).  Its painted sky (the \
   mask, brr_skymask.h: 255 sky, 0 scenery, 128 a flat panel, sky above the \
   horizon and scenery under it) leaves the skymap behind it */ \
"fragment float4 fsbk2(O in [[stage_in]], constant U &u [[buffer(0)]], texture2d<float> t [[texture(0)]], sampler sm [[sampler(0)]],\n" \
"                      constant LU &lu [[buffer(1)]], constant EU &k [[buffer(2)]], texture2d<float> skym [[texture(2)]]) {\n" \
"  SH h = shade(in, u, t, sm); if (!h.keep) discard_fragment();\n" \
"  constexpr sampler ms(filter::linear, s_address::repeat, t_address::clamp_to_edge);\n" \
"  float ty = 0.25 / float(skym.get_height());\n" \
"  float m = 0.5 * (skym.sample(ms, in.st - float2(0, ty)).r + skym.sample(ms, in.st + float2(0, ty)).r);\n" \
"  float3 d = env_camray(k, in.pos.xy);\n" \
"  float skyish = abs(m - 0.502) < 0.1 ? smoothstep(-0.004, 0.004, d.z) : smoothstep(0.25, 0.75, m);\n" \
"  float3 f = -normalize(float3(d.xy, 0.0) + float3(0, 0, 1e-6));\n" \
"  float3 alb = lin(h.o.rgb) * 0.85, E = env_irr(lu, f) + lu.sunc.rgb * max(dot(f, lu.sun.xyz), 0.0);\n" \
"  float3 L = mix(alb / 3.14159 * E, lu.fogc.rgb, k.misc.y);\n" \
"  return float4(preexp(lu, L * lu.wb.rgb), 1.0 - skyish); }\n"

#endif
