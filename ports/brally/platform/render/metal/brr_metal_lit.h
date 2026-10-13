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

#define BRR_LIT_SRC \
"struct LU { float4x4 sunvp[4]; float4 csplit; float4 cfwd; float4 ctexel; float4 cbias; float4 eye; float4 sun; float4 sunc; float4 skyc; float4 gndc; float4 misc; float4 wb; float4 bg; float4 fogc; };\n" \
"struct LS { packed_float3 pos; float kind; packed_float3 axis; float cd; packed_float3 side; float range; float4 rgb; };\n" \
/* a lamp's intensity toward d (unit, from the lamp), candela: a headlamp's \
   low beam (UN ECE R112's asymmetric pattern for right-hand traffic, aimed \
   as its paragraph 6.2.2.1 aims it: the cut-off's horizontal part 0.57 \
   degrees below the horizon on the near side, rising 15 degrees from the \
   elbow on the kerb side; the hot spot below the cut-off and right of \
   centre, the near side dimmer, a wide spread below it; the cut-off a \
   blend in log intensity, so its line, R112's steepest log gradient, sits \
   where it is placed; checked against R112's test points by the \
   verification suite), a rear lamp's broad lobe */ \
"static float lamp(LS l, float3 d) {\n" \
"  float3 ax = float3(l.axis), sd = float3(l.side), up = cross(sd, ax);\n" \
"  float al = dot(d, ax); if (al <= 0.0) return 0.0;\n" \
"  if (l.kind > 0.5) return l.cd * pow(al, 1.5);\n" \
"  float h = atan2(dot(d, sd), al) * 57.2958, v = atan2(dot(d, up), al) * 57.2958 + 0.57;\n" \
"  float hw = h < 1.5 ? 5.0 : 10.0;\n" \
"  float spread = 0.25 * exp(-(h / 32.0) * (h / 32.0)) + 0.75 * exp(-((h - 1.5) / hw) * ((h - 1.5) / hw));\n" \
"  float vert = v < -1.5 ? exp(-((v + 1.5) / 4.5) * ((v + 1.5) / 4.5)) : 1.0;\n" \
"  float cut = h > 0.0 ? min(h * 0.268, 2.0) : 0.0;\n" \
"  return l.cd * spread * vert * pow(0.012, smoothstep(cut - 0.35, cut + 0.35, v));\n" \
"}\n" \
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
"static float3 brdf(float3 n, float3 v, float3 l, float3 alb, float rough, texture2d<float> bE) {\n" \
"  float nl = min(dot(n, l), 1.0); if (nl <= 0.0 || dot(n, v) < 0.0) return 0.0;\n" \
"  constexpr sampler s(filter::linear, address::clamp_to_edge); float2 sz = float2(bE.get_width(), bE.get_height());\n" \
"  float nv = clamp(dot(n, v), 1e-3, 1.0), ry = saturate((rough - 0.0447) / 0.9553);   /* the table from the roughness floor (a^2 = 2e-3) */\n" \
"  float2 ev = bE.sample(s, (0.5 + float2(sqrt(nv), ry) * (sz - 1.0)) / sz).rg; float el = bE.sample(s, (0.5 + float2(sqrt(nl), ry) * (sz - 1.0)) / sz).r;\n" \
"  float fd = (1.0 - ev.x) * (1.0 - el) / (3.14159 * max(1.0 - ev.y, 1e-3));\n" \
"  return (alb * fd + ggxspec(n, v, l, rough)) * nl;\n" \
"}\n" \
/* the sun's visibility from its cascades: the cascade by view depth, the
 * next one blended in over the last tenth of each, faded out at the end of
 * the last; the receiver pushed along its normal by one to three texels of
 * its cascade and compared one texel nearer, both growing as the surface
 * turns from the sun (slope-scaled: the depth a texel spans on it); 3x3
 * filtered comparisons */ \
"static float casvis(constant LU &u, depth2d_array<float> sm, float3 p, float3 n, int k) {\n" \
"  constexpr sampler cs(filter::linear, address::clamp_to_edge, compare_func::less_equal);\n" \
"  float nl = clamp(dot(n, u.sun.xyz), 0.05, 1.0), tn = min(sqrt(1.0 - nl * nl) / nl, 8.0);\n" \
"  float4 c = u.sunvp[k] * float4(p + n * u.ctexel[k] * (1.0 + 2.0 * (1.0 - nl)), 1.0); float2 st = float2(c.x * 0.5 + 0.5, 0.5 - c.y * 0.5);\n" \
"  if (any(st <= 0.0) || any(st >= 1.0) || c.z >= 1.0) return 1.0;\n" \
"  float2 tx = 1.0 / float2(sm.get_width(), sm.get_height()); float vis = 0.0;\n" \
"  for (int y = -1; y <= 1; y++) for (int x = -1; x <= 1; x++) vis += sm.sample_compare(cs, st + float2(x, y) * tx, uint(k), c.z - u.cbias[k] * (1.0 + tn));\n" \
"  return vis / 9.0;\n" \
"}\n" \
"static float sunvis(constant LU &u, depth2d_array<float> sm, float3 p, float3 n) {\n" \
"  if (u.misc.y < 0.5) return 1.0;\n" \
"  float d = dot(p - u.eye.xyz, u.cfwd.xyz); int k = 0;\n" \
"  while (k < 3 && d > u.csplit[k]) k++;\n" \
"  if (d > u.csplit[3]) return 1.0;\n" \
"  float d0 = k ? u.csplit[k - 1] : 0.0, w = saturate((d - mix(d0, u.csplit[k], 0.9)) / max(0.1 * (u.csplit[k] - d0), 1e-3));\n" \
"  float vis = casvis(u, sm, p, n, k);\n" \
"  if (w > 0.0) vis = mix(vis, k < 3 ? casvis(u, sm, p, n, k + 1) : 1.0, w);\n" \
"  return vis;\n" \
"}\n" \
/* the light leaving a surface toward the eye, cd/m2, before exposure */ \
"static float3 radiance(constant LU &u, const device LS *ls, depth2d_array<float> sm, float3 p, float3 n, float3 alb, float rough, float glow,\n" \
"                       texture2d<float> bE) {\n" \
"  float3 v = normalize(u.eye.xyz - p); if (dot(n, v) < 0) n = -n;\n" \
"  float3 Lo = u.sunc.rgb * brdf(n, v, u.sun.xyz, alb, rough, bE) * sunvis(u, sm, p, n);\n" \
"  Lo += alb / 3.14159 * mix(u.gndc.rgb, u.skyc.rgb, n.z * 0.5 + 0.5);\n" \
"  for (int i = 0; i < int(u.eye.w); i++) {\n" \
"    LS l = ls[i]; float3 L = float3(l.pos) - p; float d2 = max(dot(L, L), 0.01), dl = sqrt(d2); float3 ld = L / dl;\n" \
"    if (dot(n, ld) <= 0.0) continue;\n" \
"    float E = lamp(l, -ld) / d2 * (1.0 - smoothstep(0.7 * l.range, l.range, dl));\n" \
"    Lo += l.rgb.rgb * E * brdf(n, v, ld, alb, rough, bE);\n" \
"  }\n" \
"  Lo += alb / max(dot(alb, float3(0.2126, 0.7152, 0.0722)), 0.02) * glow * 100.0;   /* about 100 cd/m2: a lit window */\n" \
"  return Lo * u.wb.rgb;\n" \
"}\n" \
/* the game's fog amount, toward the air's own light at the horizon (the sky \
   model's) where there is a sky, else the game's fog colour */ \
"static float3 fogged(constant LU &u, float3 c, float fog, float3 fogc) { return mix(c, u.fogc.w > 0.5 ? u.fogc.rgb : lin(fogc) * u.bg.x, fog); }\n" \
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
"  r.alb = float4(delit(h.tex.rgb, h.it.rgb), (128.0 + floor(saturate(g) * 127.0 + 0.5)) / 255.0);\n" \
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
"fragment float4 lfs(LO in [[stage_in]], constant LU &u [[buffer(0)]], const device LS *ls [[buffer(1)]],\n" \
"                    texture2d<float> alb [[texture(0)]], texture2d<float> pos [[texture(1)]], texture2d<float> nrm [[texture(2)]],\n" \
"                    depth2d_array<float> sm [[texture(3)]], texture2d<float> bE [[texture(4)]]) {\n" \
"  uint2 px = uint2(in.pos.xy); float4 a = alb.read(px);\n" \
"  if (a.a < 0.25) discard_fragment();\n" \
"  float4 P = pos.read(px), N = nrm.read(px);\n" \
"  float glow = saturate((a.a * 255.0 - 128.0) / 127.0);\n" \
"  int dbg = int(u.misc.w);\n" \
"  if (dbg == 1) return float4(a.rgb / max(u.misc.x, 1e-9), 1);\n" \
"  if (dbg == 2) return float4((N.xyz * 0.5 + 0.5) / max(u.misc.x, 1e-9), 1);\n" \
"  if (dbg == 3) return float4(float3(sunvis(u, sm, P.xyz, N.xyz)) / max(u.misc.x, 1e-9), 1);\n" \
"  if (dbg == 5) return float4(float3(glow) / max(u.misc.x, 1e-9), 1);\n" \
"  uint fc = uint(P.w); float3 fogc = float3((fc >> 16) & 255, (fc >> 8) & 255, fc & 255) / 255.0;\n" \
"  return float4(fogged(u, radiance(u, ls, sm, P.xyz, N.xyz, a.rgb, u.misc.z, glow, bE), N.w, fogc), 1);\n" \
"}\n" \
/* the game's blended 3D, forward-lit; an additive draw is light it adds */ \
"fragment float4 fsf(O in [[stage_in]], constant U &u [[buffer(0)]], texture2d<float> t [[texture(0)]], sampler sm [[sampler(0)]],\n" \
"                    texture2d<float> em [[texture(1)]], constant LU &lu [[buffer(1)]], const device LS *ls [[buffer(2)]],\n" \
"                    depth2d_array<float> shm [[texture(3)]], texture2d<float> bE [[texture(4)]]) {\n" \
"  SH h = shade(in, u, t, sm); if (!h.keep) discard_fragment();\n" \
"  float g = u.glow * (u.has_tex ? em.sample(sm, in.st).r : 0.0);\n" \
"  float3 n = facen(in.wp);\n" \
"  float3 c = u.addk > 0.0 ? lin(h.o.rgb) * u.addk * lu.wb.rgb\n" \
"                          : radiance(lu, ls, shm, in.wp, n, delit(h.tex.rgb, h.it.rgb), lu.misc.z, g, bE);\n" \
"  return float4(fogged(lu, c, u.addk > 0.0 ? 0.0 : h.fog, u.fog_color.rgb), h.o.a);\n" \
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
/* post: bloom, exposure, the meter's histogram, Khronos PBR Neutral, sRGB */ \
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
"  float Y = dot(c, float3(0.2126, 0.7152, 0.0722));\n" \
"  if (((px.x | px.y) & 3) == 0 && Y > 0.0 && alb.read(px).a >= 0.25)   /* the meter reads lit surfaces only; 16 bins a stop */\n" \
"    atomic_fetch_add_explicit(&hist[uint(clamp((log2(max(Y, 1e-6)) + 10.0) * 16.0, 0.0, 511.0))], 1u, memory_order_relaxed);\n" \
"  c = neutral(c * u.misc.x);\n" \
"  return float4(srgb1(c.r), srgb1(c.g), srgb1(c.b), 1);\n" \
"}\n" \
/* the sun's shadow map */ \
"struct SO { float4 pos [[position]]; };\n" \
"vertex SO svs(uint i [[vertex_id]], const device packed_float3 *v [[buffer(0)]], constant float4x4 &m [[buffer(1)]]) {\n" \
"  SO o; o.pos = m * float4(float3(v[i]), 1.0); return o; }\n"

#endif
