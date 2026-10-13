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
"struct LU { float4x4 sunvp; float4 eye; float4 sun; float4 sunc; float4 skyc; float4 gndc; float4 misc; float4 wb; float4 bg; };\n" \
"struct LS { packed_float3 pos; float kind; packed_float3 axis; float cd; packed_float3 side; float range; float4 rgb; };\n" \
/* a lamp's intensity toward d (unit, from the lamp), candela: a headlamp's \
   low beam (UN ECE R112's asymmetric pattern: a sharp cut-off at the horizon \
   on the near side, stepping up 15 degrees on the kerb side, the hot spot a \
   degree and a half below the horizon and right of centre, a wide spread \
   below it), a rear lamp's broad lobe */ \
"static float lamp(LS l, float3 d) {\n" \
"  float3 ax = float3(l.axis), sd = float3(l.side), up = cross(sd, ax);\n" \
"  float al = dot(d, ax); if (al <= 0.0) return 0.0;\n" \
"  if (l.kind > 0.5) return l.cd * pow(al, 1.5);\n" \
"  float h = atan2(dot(d, sd), al) * 57.2958, v = atan2(dot(d, up), al) * 57.2958;\n" \
"  float spread = 0.25 * exp(-(h / 32.0) * (h / 32.0)) + 0.75 * exp(-((h - 1.5) / 10.0) * ((h - 1.5) / 10.0));\n" \
"  float vert = v < -1.5 ? exp(-((v + 1.5) / 4.5) * ((v + 1.5) / 4.5)) : 1.0;\n" \
"  float cut = h > 0.0 ? min(h * 0.268, 2.0) : 0.0;\n" \
"  return l.cd * spread * vert * mix(1.0, 0.012, smoothstep(cut - 0.35, cut + 0.35, v));\n" \
"}\n" \
"static float3 lin(float3 c) { return select(pow((c + 0.055) / 1.055, 2.4), c / 12.92, c <= 0.04045); }\n" \
"static float srgb1(float x) { return x <= 0.0031308 ? 12.92 * x : 1.055 * pow(x, 1.0 / 2.4) - 0.055; }\n" \
/* the albedo: the game's texture times its vertex colour's hue (the colour's \
   brightness is the game's baked light, which the lighting replaces) */ \
"static float3 delit(float3 tex, float3 vc) {\n" \
"  float lv = dot(vc, float3(0.2126, 0.7152, 0.0722));\n" \
"  float3 hue = lv > 0.02 ? clamp(vc / lv, 0.0, 2.0) : float3(1);\n" \
"  return clamp(lin(tex) * lin(saturate(hue * 0.5)) * 4.0 * 0.85, 0.0, 0.9);\n" \
"}\n" \
/* GGX with Smith visibility (height-correlated approximation) and Schlick \
   Fresnel; Lambert diffuse; energy split by Fresnel */ \
"static float3 brdf(float3 n, float3 v, float3 l, float3 alb, float rough) {\n" \
"  float3 h = normalize(l + v); float nl = max(dot(n, l), 0.0), nv = max(dot(n, v), 1e-3), nh = max(dot(n, h), 0.0);\n" \
"  float a2 = max(rough * rough, 2e-3); a2 *= a2;\n" \
"  float d = nh * nh * (a2 - 1.0) + 1.0, D = a2 / (3.14159 * d * d);\n" \
"  float Vs = 0.5 / max(nl * sqrt(nv * nv * (1.0 - a2) + a2) + nv * sqrt(nl * nl * (1.0 - a2) + a2), 1e-4);\n" \
"  float F = 0.04 + 0.96 * pow(1.0 - max(dot(h, v), 0.0), 5.0);\n" \
"  return (alb / 3.14159 * (1.0 - F) + D * Vs * F) * nl;\n" \
"}\n" \
"static float sunvis(constant LU &u, depth2d<float> sm, float3 p, float3 n) {\n" \
"  if (u.misc.y < 0.5) return 1.0;\n" \
"  constexpr sampler cs(filter::linear, address::clamp_to_edge, compare_func::less_equal);\n" \
"  float4 c = u.sunvp * float4(p + n * 0.05, 1.0); float2 st = float2(c.x * 0.5 + 0.5, 0.5 - c.y * 0.5);\n" \
"  if (any(st <= 0.0) || any(st >= 1.0) || c.z >= 1.0) return 1.0;\n" \
"  float2 tx = 1.0 / float2(sm.get_width(), sm.get_height()); float vis = 0.0;\n" \
"  for (int y = -1; y <= 1; y++) for (int x = -1; x <= 1; x++) vis += sm.sample_compare(cs, st + float2(x, y) * tx, c.z - 0.0008);\n" \
"  return vis / 9.0;\n" \
"}\n" \
/* the light leaving a surface toward the eye, cd/m2, before exposure */ \
"static float3 radiance(constant LU &u, const device LS *ls, depth2d<float> sm, float3 p, float3 n, float3 alb, float rough, float glow) {\n" \
"  float3 v = normalize(u.eye.xyz - p); if (dot(n, v) < 0) n = -n;\n" \
"  float3 Lo = u.sunc.rgb * brdf(n, v, u.sun.xyz, alb, rough) * sunvis(u, sm, p, n);\n" \
"  Lo += alb / 3.14159 * mix(u.gndc.rgb, u.skyc.rgb, n.z * 0.5 + 0.5);\n" \
"  for (int i = 0; i < int(u.eye.w); i++) {\n" \
"    LS l = ls[i]; float3 L = float3(l.pos) - p; float d2 = max(dot(L, L), 0.01), dl = sqrt(d2); float3 ld = L / dl;\n" \
"    if (dot(n, ld) <= 0.0) continue;\n" \
"    float E = lamp(l, -ld) / d2 * (1.0 - smoothstep(0.7 * l.range, l.range, dl));\n" \
"    Lo += l.rgb.rgb * E * brdf(n, v, ld, alb, rough);\n" \
"  }\n" \
"  Lo += alb / max(dot(alb, float3(0.2126, 0.7152, 0.0722)), 0.02) * glow * 100.0;   /* about 100 cd/m2: a lit window */\n" \
"  return Lo * u.wb.rgb;\n" \
"}\n" \
"static float3 fogged(constant LU &u, float3 c, float fog, float3 fogc) { return mix(c, lin(fogc) * u.bg.x, fog); }\n" \
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
/* the deferred lighting */ \
"fragment float4 lfs(LO in [[stage_in]], constant LU &u [[buffer(0)]], const device LS *ls [[buffer(1)]],\n" \
"                    texture2d<float> alb [[texture(0)]], texture2d<float> pos [[texture(1)]], texture2d<float> nrm [[texture(2)]],\n" \
"                    depth2d<float> sm [[texture(3)]]) {\n" \
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
"  return float4(fogged(u, radiance(u, ls, sm, P.xyz, N.xyz, a.rgb, u.misc.z, glow), N.w, fogc), 1);\n" \
"}\n" \
/* the game's blended 3D, forward-lit; an additive draw is light it adds */ \
"fragment float4 fsf(O in [[stage_in]], constant U &u [[buffer(0)]], texture2d<float> t [[texture(0)]], sampler sm [[sampler(0)]],\n" \
"                    texture2d<float> em [[texture(1)]], constant LU &lu [[buffer(1)]], const device LS *ls [[buffer(2)]],\n" \
"                    depth2d<float> shm [[texture(3)]]) {\n" \
"  SH h = shade(in, u, t, sm); if (!h.keep) discard_fragment();\n" \
"  float g = u.glow * (u.has_tex ? em.sample(sm, in.st).r : 0.0);\n" \
"  float3 n = facen(in.wp);\n" \
"  float3 c = u.addk > 0.0 ? lin(h.o.rgb) * u.addk * lu.wb.rgb\n" \
"                          : radiance(lu, ls, shm, in.wp, n, delit(h.tex.rgb, h.it.rgb), lu.misc.z, g);\n" \
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
"  uint2 px = uint2(in.pos.xy); float3 c = hdr.read(px).rgb;\n" \
"  if (int(u.misc.w) == 0) c = mix(c, bl.sample(s, in.uv).rgb * u.bg.y, 0.04);   /* the levels summed: their mean */\n" \
"  float Y = dot(c, float3(0.2126, 0.7152, 0.0722));\n" \
"  if (((px.x | px.y) & 3) == 0 && Y > 0.0 && alb.read(px).a >= 0.25)   /* the meter reads lit surfaces only */\n" \
"    atomic_fetch_add_explicit(&hist[uint(clamp((log2(max(Y, 1e-6)) + 10.0) * 2.0, 0.0, 63.0))], 1u, memory_order_relaxed);\n" \
"  c = neutral(c * u.misc.x);\n" \
"  return float4(srgb1(c.r), srgb1(c.g), srgb1(c.b), 1);\n" \
"}\n" \
/* the sun's shadow map */ \
"struct SO { float4 pos [[position]]; };\n" \
"vertex SO svs(uint i [[vertex_id]], const device packed_float3 *v [[buffer(0)]], constant float4x4 &m [[buffer(1)]]) {\n" \
"  SO o; o.pos = m * float4(float3(v[i]), 1.0); return o; }\n"

#endif
