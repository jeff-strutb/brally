/* brr_metal_sky.h: the Remastered sky and atmosphere (BR_FLAG_LIGHTING),
 * after Hillaire 2020, "A Scalable and Production Ready Sky and Atmosphere
 * Rendering Technique" (EGSR), with Bruneton's transmittance parameterisation.
 * Earth's atmosphere in kilometres: ground 6360, top 6460; Rayleigh
 * scattering (5.802, 13.558, 33.1) x 1e-3 /km, scale height 8 km; Mie
 * scattering 3.996e-3 and absorption 4.4e-3 /km, scale height 1.2 km,
 * Cornette-Shanks phase g 0.8; ozone absorption (0.650, 1.881, 0.085) x 1e-3
 * /km in a 30 km tent at 25 km; ground albedo 0.3.
 *
 *   transmittance    256 x 64, from (height, view zenith)
 *   multiscattering  32 x 32: the isotropic multiple-scattering term
 *                    (Hillaire section 5.5), from (height, sun zenith)
 *   sky view         192 x 108 around the camera: in-scattered light per unit
 *                    of sun illuminance at the top of the atmosphere, latitude
 *                    compressed toward the horizon
 *   irradiance       1 x 1: the sky's light on an up-facing surface and the
 *                    sun's transmittance at the camera, for the lighting
 *
 * The weather scales the Mie density (haze, fog) and the top-of-atmosphere
 * light the clouds let through (LU.atm). */
#ifndef BRR_METAL_SKY_H
#define BRR_METAL_SKY_H

#include "brr_metal_clouds.h"

#define BRR_SKY_SRC \
"constant float RG = 6360.0, RT = 6460.0;\n" \
"constant float3 RAYS = float3(5.802, 13.558, 33.1) * 1e-3;\n" \
"constant float MIES = 3.996e-3, MIEA = 4.4e-3;\n" \
"constant float3 OZA = float3(0.650, 1.881, 0.085) * 1e-3;\n" \
"struct AU { float4 sun; float4 cam; float4 atm; };   /* sun: direction (z up); cam: height km; atm: x mie scale */\n" \
"struct Med { float3 sr; float sm; float3 ext; };\n" \
"static Med medium(float h, float mie) {\n" \
"  float r = exp(-h / 8.0), m = exp(-h / 1.2) * mie, oz = max(0.0, 1.0 - abs(h - 25.0) / 15.0);\n" \
"  Med o; o.sr = RAYS * r; o.sm = MIES * m; o.ext = RAYS * r + (MIES + MIEA) * m + OZA * oz; return o; }\n" \
"static float sphere(float r, float mu, float R) {   /* distance along mu from radius r to the sphere R, -1 if none */\n" \
"  float d = r * r * (mu * mu - 1.0) + R * R; if (d < 0.0) return -1.0; return -r * mu + sqrt(d); }\n" \
"static float ground(float r, float mu) { float d = r * r * (mu * mu - 1.0) + RG * RG; if (d < 0.0 || mu > 0.0) return -1.0; return -r * mu - sqrt(d); }\n" \
"static float2 tuv(float r, float mu) {\n" \
"  float H = sqrt(RT * RT - RG * RG), rho = sqrt(max(r * r - RG * RG, 0.0));\n" \
"  float d = max(sphere(r, mu, RT), 0.0), dmin = RT - r, dmax = rho + H;\n" \
"  return float2((d - dmin) / max(dmax - dmin, 1e-6), rho / H); }\n" \
/* the table's unit range on its texel centres, edge to edge (Bruneton 2017, \
   GetTextureCoordFromUnitRange): the clamped half texel at each edge put \
   the zenith seen from the ground at 11.5 degrees */ \
"static float3 trans(texture2d<float> T, float r, float mu) {\n" \
"  constexpr sampler s(filter::linear, address::clamp_to_edge); float2 n = float2(T.get_width(), T.get_height());\n" \
"  return T.sample(s, (0.5 + tuv(r, mu) * (n - 1.0)) / n).rgb; }\n" \
"static float rayleigh(float c) { return 3.0 / (16.0 * 3.14159) * (1.0 + c * c); }\n" \
"static float miep(float c) { float g = 0.8, k = 3.0 / (8.0 * 3.14159) * (1.0 - g * g) / (2.0 + g * g);\n" \
"  return k * (1.0 + c * c) / pow(1.0 + g * g - 2.0 * g * c, 1.5); }\n" \
/* the transmittance LUT; the steps grow with the square of the distance, \
   fine where the ray starts (even 2.5 km steps up from the ground missed 16% \
   of the haze's 1.2 km layer) */ \
"fragment float4 atm_t(LO in [[stage_in]], constant AU &a [[buffer(0)]]) {\n" \
"  float2 n = round(in.pos.xy / in.uv), x = (in.pos.xy - 0.5) / (n - 1.0);   /* texel i: i / (n - 1) */\n" \
"  float H = sqrt(RT * RT - RG * RG), rho = H * x.y, r = sqrt(rho * rho + RG * RG);\n" \
"  float dmin = RT - r, dmax = rho + H, d = dmin + x.x * (dmax - dmin);\n" \
"  float mu = d == 0.0 ? 1.0 : clamp((H * H - rho * rho - d * d) / (2.0 * r * d), -1.0, 1.0);\n" \
"  float len = max(sphere(r, mu, RT), 0.0); float3 tau = 0.0; int N = 40;\n" \
"  for (int i = 0; i < N; i++) { float x0 = float(i) / N, x1 = float(i + 1) / N, t0 = len * x0 * x0, dt = len * x1 * x1 - t0, t = t0 + 0.5 * dt;\n" \
"    float h = sqrt(r * r + t * t + 2.0 * r * mu * t) - RG;\n" \
"    tau += medium(h, a.atm.x).ext * dt; }\n" \
"  return float4(exp(-tau), 1); }\n" \
/* the multiple-scattering LUT (Hillaire 5.5): from 256 directions, the \
   second-order light and the fraction transferred, Psi = L2 / (1 - f_ms); \
   the directions split at the horizon's cosine, each half weighted by its \
   share of the sphere (the light jumps there: the sunlit ground below, the \
   sky above), 32 cosines a side by 4 azimuths (the long paths just above \
   the horizon carry much of f_ms).  64 even directions were up to 6% of the \
   table's peak and 17% of a value off a converged reference; these are \
   within half a percent.  Made only when the weather's haze changes. */ \
"fragment float4 atm_ms(LO in [[stage_in]], constant AU &a [[buffer(0)]], texture2d<float> T [[texture(0)]]) {\n" \
"  float cs = in.uv.x * 2.0 - 1.0, h = mix(0.0, RT - RG, in.uv.y), r = RG + max(h, 0.01);\n" \
"  float3 sun = float3(0.0, sqrt(max(1.0 - cs * cs, 0.0)), cs), L2 = 0.0, fms = 0.0;\n" \
"  float mh = -sqrt(max(1.0 - (RG / r) * (RG / r), 0.0));\n" \
"  for (int k = 0; k < 256; k++) {\n" \
"    bool up = k < 128; float u1 = (k % 4 + 0.5) / 4.0, u2 = ((k / 4) % 32 + 0.5) / 32.0, th = 2.0 * 3.14159 * u1;\n" \
"    float cz = up ? mix(mh, 1.0, u2) : mix(-1.0, mh, u2), sz = sqrt(max(1.0 - cz * cz, 0.0)), wk = (up ? 1.0 - mh : 1.0 + mh) / 256.0;\n" \
"    float3 d = float3(cos(th) * sz, sin(th) * sz, cz);\n" \
"    float mu = d.z, tg = ground(r, mu), len = tg > 0.0 ? tg : max(sphere(r, mu, RT), 0.0); int N = 20; float dt = len / N;\n" \
"    float3 tr = 1.0, Ls = 0.0, f = 0.0;\n" \
"    for (int i = 0; i < N; i++) { float t = (i + 0.5) * dt; float3 p = float3(0, 0, r) + d * t; float rr = length(p), hh = rr - RG;\n" \
"      Med m = medium(hh, a.atm.x); float3 st = exp(-m.ext * dt), sc = m.sr + m.sm;\n" \
"      float3 ts = trans(T, rr, dot(p / rr, sun)); float sh = ground(rr, dot(p / rr, sun)) > 0.0 ? 0.0 : 1.0;\n" \
"      float3 S = ts * sh * sc / (4.0 * 3.14159), Sf = sc;\n" \
"      Ls += tr * (S - S * st) / max(m.ext, 1e-9); f += tr * (Sf - Sf * st) / max(m.ext, 1e-9); tr *= st; }\n" \
"    if (tg > 0.0) { float3 pg = float3(0, 0, r) + d * tg; float3 ng = normalize(pg);\n" \
"      Ls += tr * trans(T, RG, dot(ng, sun)) * max(dot(ng, sun), 0.0) * 0.3 / 3.14159; }\n" \
"    L2 += Ls * wk; fms += f * wk; }   /* isotropic phase 1/(4 pi) over the sphere: the average */\n" \
"  return float4(L2 / max(1.0 - fms, 1e-3), 1); }\n" \
"static float3 msamp(texture2d<float> M, float r, float cs) {\n" \
"  constexpr sampler s(filter::linear, address::clamp_to_edge); return M.sample(s, float2(cs * 0.5 + 0.5, (r - RG) / (RT - RG))).rgb; }\n" \
/* the sky view LUT: latitude compressed toward the horizon, longitude in the world; \
   the steps along each ray grow with the square of the distance (as Hillaire's \
   own implementation spaces them), fine in the dense air next to the camera: \
   even steps of 3.3 km straight up undercounted the haze's 1.2 km layer and \
   the zenith under a high sun by 15% */ \
"static float3 svdir(float2 uv) { float v = uv.y * 2.0 - 1.0, lat = sign(v) * v * v * 1.5708, lon = (uv.x * 2.0 - 1.0) * 3.14159;\n" \
"  return float3(cos(lat) * cos(lon), cos(lat) * sin(lon), sin(lat)); }\n" \
"static float2 svuv(float3 d) { float lat = asin(clamp(d.z, -1.0, 1.0)), lon = atan2(d.y, d.x);\n" \
"  return float2(lon / 3.14159 * 0.5 + 0.5, 0.5 + 0.5 * sign(lat) * sqrt(abs(lat) / 1.5708)); }\n" \
"fragment float4 atm_sv(LO in [[stage_in]], constant AU &a [[buffer(0)]], texture2d<float> T [[texture(0)]], texture2d<float> M [[texture(1)]]) {\n" \
"  float3 d = svdir(in.uv), sun = a.sun.xyz; float r = RG + a.cam.x, mu = d.z;\n" \
"  float tg = ground(r, mu), len = tg > 0.0 ? tg : max(sphere(r, mu, RT), 0.0); int N = 30;\n" \
"  float c = dot(d, sun), pr = rayleigh(c), pm = miep(c); float3 tr = 1.0, L = 0.0;\n" \
"  for (int i = 0; i < N; i++) { float x0 = float(i) / N, x1 = float(i + 1) / N, t0 = len * x0 * x0, dt = len * x1 * x1 - t0, t = t0 + 0.5 * dt;\n" \
"    float3 p = float3(0, 0, r) + d * t; float rr = length(p);\n" \
"    Med m = medium(rr - RG, a.atm.x); float3 st = exp(-m.ext * dt); float csun = dot(p / rr, sun);\n" \
"    float sh = ground(rr, csun) > 0.0 ? 0.0 : 1.0;\n" \
"    float3 S = trans(T, rr, csun) * sh * (m.sr * pr + m.sm * pm) + msamp(M, rr, csun) * (m.sr + m.sm);\n" \
"    L += tr * (S - S * st) / max(m.ext, 1e-9); tr *= st; }\n" \
"  return float4(L, 1); }\n" \
/* the sky's light on an up-facing surface (cosine-weighted over the upper \
   half of the sky view), and the sun's transmittance at the camera */ \
"fragment float4 atm_irr(LO in [[stage_in]], constant AU &a [[buffer(0)]], texture2d<float> S [[texture(0)]], texture2d<float> T [[texture(1)]]) {\n" \
"  constexpr sampler s(filter::linear, address::clamp_to_edge); float3 E = 0.0;\n" \
"  if (in.pos.x >= 2.0) {   /* the air's light at the horizon, averaged round it: the fog's colour */\n" \
"    for (int i = 0; i < 32; i++) { float th = (i + 0.5) / 32.0 * 2.0 * 3.14159; E += S.sample(s, svuv(float3(cos(th), sin(th), 0.03))).rgb; }\n" \
"    return float4(E / 32.0, 1); }\n" \
"  if (in.pos.x >= 1.0) return float4(trans(T, RG + a.cam.x, a.sun.z), 1);\n" \
"  for (int j = 0; j < 16; j++) for (int i = 0; i < 32; i++) {\n" \
"    float ph = acos(sqrt(1.0 - (j + 0.5) / 16.0)), th = (i + 0.5) / 32.0 * 2.0 * 3.14159;   /* cosine-distributed */\n" \
"    float3 d = float3(cos(th) * sin(ph), sin(th) * sin(ph), cos(ph)); E += S.sample(s, svuv(d)).rgb; }\n" \
"  return float4(E / 512.0 * 3.14159, 1); }\n" \
/* the sky, per pixel: the camera's ray for the pixel (through the main 3D \
   view's screen map, the game's viewport and its view x projection), the \
   sky view, and the sun's disc (0.2675 degrees, its illuminance over the disc: \
   the soft rim, a smoothstep over the outer tenth of its solid angle, keeps \
   0.95 of it, divided back); kept within half float's range (the disc's \
   centre is about 1.6e9 cd/m2) */ \
"struct KU { float4 xft; float4 vpt; float4 m0; float4 m1; float4 m2; float4 off; float4 eye; float4 tgt; float4 sun; float4 ktop; float4 cld; float4 moon; };\n" \
"static float3 camray(constant KU &k, float2 px) {\n" \
"  float2 ndc = float2((px.x + 0.5) / k.tgt.x * 2.0 - 1.0, 1.0 - (px.y + 0.5) / k.tgt.y * 2.0);\n" \
"  float2 g = float2((ndc.x - k.xft.y) / k.xft.x, (ndc.y - k.xft.w) / k.xft.z);\n" \
"  if (k.tgt.w > 0.0) g.y = k.tgt.w - g.y;   /* the game's rows run up (Glide's lower-left origin) */\n" \
"  float3 c = float3((g.x - k.vpt.y) / k.vpt.x - k.off.x, (g.y - k.vpt.w) / k.vpt.z - k.off.y, 1.0 - k.off.z);\n" \
"  float3 p = float3(dot(k.m0.xyz, c), dot(k.m1.xyz, c), dot(k.m2.xyz, c));\n" \
"  return normalize(p - k.eye.xyz); }\n" \
/* the night sky (light_night.h): the moon's disc at the light's direction \
   (0.26 degrees, linear limb darkening u 0.3, centre luminance moon.y a lux \
   times moon.x, the illuminance reaching the ground facing it, about 3,300 \
   cd/m2 at Coastline's 42 degrees), and the stars: on each face of the \
   direction's cube a 512 x 512 grid, a star in 1.2% of its cells at a \
   hashed spot, magnitude 6.5 + 2.5 log10 h (h uniform, at most -1), its \
   illuminance 10^(-0.4 (m + 13.99)) lux over a disc of 0.035 degrees, dimmed \
   by the air (0.172 magnitudes an air mass, Kasten and Young) and gone under \
   the horizon; the clouds (skyfs) cover both */ \
"static uint sh3(uint3 v) { uint h = v.x * 0x8da6b343u ^ v.y * 0xd8163841u ^ v.z * 0xcb1ab31fu; h ^= h >> 15; h *= 0x2c1b3c6du; h ^= h >> 12; h *= 0x297a2d39u; h ^= h >> 15; return h; }\n" \
"static float3 moonstars(constant KU &k, float3 d) {\n" \
"  float3 L = 0.0; const float R = 0.26 * 3.14159265 / 180.0;\n" \
"  float r = acos(clamp(dot(d, k.sun.xyz), -1.0, 1.0)) / R;\n" \
"  if (r < 1.0) L += k.moon.x * k.moon.y * (1.0 - 0.3 * (1.0 - sqrt(1.0 - r * r))) * float3(1.0, 0.96, 0.90);\n" \
"  if (k.moon.z > 0.0 && d.z > 0.0) {\n" \
"    float3 a = abs(d); int ax = a.x >= a.y && a.x >= a.z ? 0 : (a.y >= a.z ? 1 : 2); float mj = a[ax];\n" \
"    float2 uv = ax == 0 ? d.yz : (ax == 1 ? d.xz : d.xy); uv = uv / mj * 0.5 + 0.5;\n" \
"    uint face = uint(ax * 2) + (d[ax] < 0.0 ? 1u : 0u); float2 g = uv * 512.0; uint2 c = uint2(min(g, 511.0));\n" \
"    uint h = sh3(uint3(c, face));\n" \
"    if (float(h & 0xffffu) / 65536.0 < 0.012) {\n" \
"      uint h2 = sh3(uint3(c.yx, face + 17u));\n" \
"      float2 sp = float2(c) + 0.2 + 0.6 * float2(float(h2 & 0xffu), float((h2 >> 8) & 0xffu)) / 255.0;\n" \
"      float2 suv = sp / 512.0 * 2.0 - 1.0; float3 sd = ax == 0 ? float3(sign(d.x), suv.x, suv.y) : (ax == 1 ? float3(suv.x, sign(d.y), suv.y) : float3(suv.x, suv.y, sign(d.z)));\n" \
"      sd = normalize(sd); const float rs = 0.035 * 3.14159265 / 180.0;\n" \
"      if (dot(d, sd) > cos(rs)) {\n" \
"        float hm = max(float(h >> 16) / 65536.0 + 0.5 / 65536.0, 1e-3), m = 6.5 + 2.5 * log10(hm);\n" \
"        float el = asin(sd.z) * 57.29578, X = 1.0 / (max(sd.z, 0.0) + 0.50572 * pow(max(el, 0.0) + 6.07995, -1.6364));\n" \
"        L += pow(10.0, -0.4 * (m + 13.99 + 0.172 * X)) / (3.14159265 * rs * rs); } } }\n" \
"  return L; }\n" \
"static float3 skyrad(constant KU &k, texture2d<float> S, texture2d<float> T, float3 d) {\n" \
"  constexpr sampler s(filter::linear, address::clamp_to_edge); float3 L = S.sample(s, svuv(d)).rgb * k.ktop.x;\n" \
"  float cd = dot(d, k.sun.xyz), cr = cos(0.2675 * 3.14159 / 180.0);\n" \
"  if (k.moon.x > 0.0) { L += moonstars(k, d); return L; }\n" \
"  if (cd > cr && d.z > -0.01) L += trans(T, RG + k.eye.w, d.z) * k.ktop.x * k.ktop.y / (3.14159 * pow(0.2675 * 3.14159 / 180.0, 2.0)) * smoothstep(cr, mix(cr, 1.0, 0.1), cd) / 0.95;\n" \
"  return L; }\n" \
BRR_CLOUD_SRC \
/* the cloud's ambient (brr_metal_clouds.h): isotropic scattering, albedo \
   0.98, of the sky above (its zenith radiance over the upper hemisphere) and \
   the ground below (albedo 0.3, Lambertian, lit by the sun Esun and the sky \
   pi Lz): 0.98 / 2 (Lz + 0.3 (Esun / pi + Lz)) */ \
"static float3 camb(float3 Lz, float3 Esun) { return 0.49 * (Lz + 0.3 * (Esun / 3.14159265 + Lz)); }\n" \
/* the sky with the cloud layer (brr_metal_clouds.h) in front of it */ \
"fragment float4 skyfs(LO in [[stage_in]], constant KU &k [[buffer(0)]], texture2d<float> S [[texture(0)]], texture2d<float> T [[texture(1)]]) {\n" \
"  float3 d = camray(k, in.pos.xy);\n" \
"  if (k.ktop.w == 9.0) return float4(d * 0.5 + 0.5, 1);   /* debug: the ray */\n" \
"  if (k.ktop.w == 10.0) { constexpr sampler s(filter::linear, address::clamp_to_edge); return float4(S.sample(s, svuv(d)).rgb * 20.0, 1); }\n" \
"  float3 L = skyrad(k, S, T, d);\n" \
"  { constexpr sampler s(filter::linear, address::clamp_to_edge);\n" \
"    float4 c = clouds(d, k.sun.xyz, k.eye.w, k.cld, trans(T, RG + 2.0, k.sun.z) * k.ktop.y, camb(S.sample(s, svuv(float3(0, 0, 1))).rgb, trans(T, RG + 0.0, k.sun.z) * k.ktop.y * max(k.sun.z, 0.0)));\n" \
"    L = L * c.a + c.rgb * k.ktop.x; }\n" \
"  return float4(min(L * k.ktop.z * k.tgt.z, float3(60000.0)), 1); }   /* pre-exposed (k.tgt.z the exposure) */\n" \
/* the game's painted backdrop over the sky: its hills, coast and sea stay, \
   its painted sky gives way.  The sky is the texture's structural mask \
   (brr_skymask.h, texture 2: 1 above the skyline), sampled bilinearly at two \
   rows half a texel apart: a soft edge of about two texels. */ \
"fragment float4 fsbk2(O in [[stage_in]], constant U &u [[buffer(0)]], texture2d<float> t [[texture(0)]], sampler sm [[sampler(0)]],\n" \
"                      constant LU &lu [[buffer(1)]], constant KU &k [[buffer(2)]], texture2d<float> skym [[texture(2)]]) {\n" \
"  SH h = shade(in, u, t, sm); if (!h.keep) discard_fragment();\n" \
"  constexpr sampler ms(filter::linear, s_address::repeat, t_address::clamp_to_edge);\n" \
"  float3 c = h.o.rgb; float ty = 0.25 / float(skym.get_height());\n" \
"  float skyish = 0.5 * (skym.sample(ms, in.st - float2(0, ty)).r + skym.sample(ms, in.st + float2(0, ty)).r);\n" \
"  if (int(lu.misc.w) == 8) skyish = 1.0;   /* BR_LIGHT_VIEW=8: the sky alone */\n" \
"  return float4(lin(c) * lu.bg.x, 1.0 - skyish); }\n"

#endif
