/* brr_metal_clouds.h: the Remastered cloud layer over the physical sky (port
 * code; in BRR_SKY_SRC, drawn by skyfs in place of the backdrop's painted
 * clouds, which brr_skymask.h now lets through as sky).
 *
 * Volumetric cumulus from 1.5 to 3.5 km over a spherical earth, ray-marched
 * per pixel (Schneider 2015 and 2017, Nubis; Hillaire 2016): a tileable 3D
 * Perlin-Worley shape noise and a Worley detail noise made once on the GPU
 * (cloud_noise, 16 bits a channel), the weather's coverage (KU.cld.x, the share of the layer in
 * cloud, through a measured quantile table) varied by a 2D fBm so clouds
 * gather in fields, a cumulus height profile, and the weather's extinction
 * (KU.cld.y, per metre, at full density).  The sun's (the moon's) light into
 * each step through a six-sample cone toward it, then multiple scattering by
 * Wrenninge's octaves (Wrenninge 2013, as Hillaire 2016 and Schneider 2015 use
 * them: four octaves, the n-th weighted a^n, its self-shadow optical depth
 * scaled b^n, its phase's eccentricities c^n, a = b = c = 1/2, normalised to
 * a single-scattering albedo of 0.98) over a two-lobe Henyey-Greenstein phase
 * (forward g 0.6, back -0.3, weights 0.7 and 0.3), or where deeper the
 * two-stream diffusion's transmission; the ambient of the sky above and the
 * ground below.  Each step integrated as Hillaire's S (1 - exp(-sigma dt))
 * under the transmittance so far; the far field fades into the air's light.
 * KU.cld.z is the wind's clock (seconds), KU.cld.w the tier flag. */
#ifndef BRR_METAL_CLOUDS_H
#define BRR_METAL_CLOUDS_H

#define BRR_CLOUD_SRC \
"static float hgp(float c, float g) { float q = 1.0 + g * g - 2.0 * g * c; return (1.0 - g * g) / (4.0 * 3.14159265 * q * sqrt(q)); }\n" \
"static float vhash(float2 p) { p = fract(p * float2(0.1031, 0.1030)); p += dot(p, p.yx + 33.33); return fract((p.x + p.y) * p.x); }\n" \
"static float vnoise(float2 p) { float2 i = floor(p), f = fract(p), u = f * f * (3.0 - 2.0 * f);\n" \
"  return mix(mix(vhash(i), vhash(i + float2(1, 0)), u.x), mix(vhash(i + float2(0, 1)), vhash(i + float2(1, 1)), u.x), u.y); }\n" \
"static float cfbm(float2 p) { float a = 0.5, s = 0.0; for (int i = 0; i < 5; i++) { s += a * vnoise(p); p = p * 2.0 + float2(17.1, 5.3); a *= 0.5; } return s / 0.96875; }\n" \
"#define CMS_N 4\n" \
"#define CMS_A 0.5\n" \
"#define CMS_B 0.5\n" \
"#define CMS_C 0.5\n" \
"#define CMS_W 0.98\n" \
/* the sun's light scattered toward the eye per unit of the light into the \
   slab: Wrenninge's octaves (Hillaire 2016), octave n weighted a^n, its \
   self-shadow optical depth scaled b^n and its phase's eccentricities c^n; \
   the weights normalised to the single-scattering albedo CMS_W, so with no \
   self-shadow the slab scatters CMS_W of what reaches it (energy kept) */ \
"static float cms(float ts, float c) {\n" \
"  float s = 0.0, a = 1.0, b = 1.0, g = 1.0, an = 0.0;\n" \
"  for (int n = 0; n < CMS_N; n++) { an += a; a *= CMS_A; } a = 1.0;\n" \
"  for (int n = 0; n < CMS_N; n++) {\n" \
"    s += a * exp(-b * ts) * (0.7 * hgp(c, 0.6 * g) + 0.3 * hgp(c, -0.3 * g));\n" \
"    a *= CMS_A; b *= CMS_B; g *= CMS_C; }\n" \
"  return s * CMS_W / an; }\n" \
/* ---- the volumetric layer ---------------------------------------------------- \
   A cumulus layer from CLOUD_BASE to CLOUD_TOP km over a spherical earth, \
   ray-marched (Schneider 2015 and 2017, Nubis; Hillaire 2016): its density \
   from a tileable 3D Perlin-Worley shape noise (cn: r the Perlin-Worley, g b a \
   Worley fBms at rising frequencies, 24 km a tile) and a Worley detail noise \
   (cdt, 3 km a tile) eroding its edges, cut by the weather's coverage (a 2D \
   fBm about KU.cld.x, so clouds gather in fields) and shaped by a cumulus \
   height profile.  Each step's light: the sun's (the moon's) through a cone of \
   six samples toward it (its self-shadow optical depth) into Wrenninge's \
   octaves (cms, above), plus the ambient of the sky above and the ground \
   below; integrated per step as Hillaire's energy-conserving \
   S (1 - exp(-sigma dt)), under the transmittance so far. */ \
"#define CLOUD_BASE 1.5\n" \
"#define CLOUD_TOP 3.5\n" \
"static float cremap(float x, float a, float b, float c, float d) { return c + (x - a) / max(b - a, 1e-5) * (d - c); }\n" \
/* the stretched shape's threshold for a coverage c (0, 0.1 .. 1): the 1 - c quantile, measured over the noise \
   (60,000 points through a CPU copy of cloud_noise), so a coverage is the share of the layer's middle in cloud */ \
"constant float CLOUD_Q[11] = { 1.093, 0.735, 0.672, 0.624, 0.579, 0.536, 0.490, 0.439, 0.380, 0.294, 0.000 };\n" \
"static float cthr(float c) { float x = saturate(c) * 10.0; int i = min(int(x), 9); return mix(CLOUD_Q[i], CLOUD_Q[i + 1], x - float(i)); }\n" \
"static float cloud_rho(float3 p, float alt, float4 cld, texture3d<float> cn, texture3d<float> cdt, bool detail) {\n" \
"  float h = (alt - CLOUD_BASE) / (CLOUD_TOP - CLOUD_BASE); if (h <= 0.0 || h >= 1.0) return 0.0;\n" \
"  float2 wind = float2(0.012, 0.005) * cld.z; float3 q = float3(p.xy + wind, alt);\n" \
"  float cov = saturate(cld.x + (cfbm(q.xy * 0.05) - 0.5) * 0.7 * 4.0 * cld.x * (1.0 - cld.x));   /* fields of cloud, none at 0 and 1: an overcast is overcast */\n" \
"  if (cov <= 0.0) return 0.0;\n" \
"  constexpr sampler rs(filter::linear, address::repeat);\n" \
"  float4 n = cn.sample(rs, q / 24.0);\n" \
"  float fbm = n.g * 0.625 + n.b * 0.25 + n.a * 0.125;\n" \
"  float shape = saturate(cremap(saturate(cremap(n.r, fbm - 1.0, 1.0, 0.0, 1.0)), 0.70, 0.95, 0.0, 1.0));   /* stretched: it spans 0.70 to 0.95 */\n" \
"  float hg = saturate(cremap(h, 0.0, 0.12, 0.0, 1.0)) * saturate(cremap(h, 0.45, 1.0, 1.0, 0.0));\n" \
"  float th = cthr(cov), rho = saturate(cremap(shape * hg, th, 1.0, 0.0, 1.0));   /* not times the coverage: at a fair-weather 0.3 it left the erosion nothing */\n" \
"  if (!detail || rho <= 0.0) return rho;\n" \
"  float3 dn = cdt.sample(rs, q / 3.0 + float3(0.0, 0.0, 0.1) * cld.z * 0.01).rgb;\n" \
"  float dfbm = dn.r * 0.625 + dn.g * 0.25 + dn.b * 0.125, m = mix(dfbm, 1.0 - dfbm, saturate(h * 6.0));\n" \
"  return saturate(cremap(rho, m * 0.3, 1.0, 0.0, 1.0)); }\n" \
/* the outer intersection of a ray from radius r0 (inside) with the sphere of radius rs */ \
"static float cshell(float r0, float mu, float rs) { float b = r0 * mu, c = r0 * r0 - rs * rs, D = b * b - c; return D < 0.0 ? -1.0 : -b + sqrt(D); }\n" \
"static float4 clouds(float3 d, float3 sun, float h, float4 cld, float3 tsun, float3 amb, texture3d<float> cn, texture3d<float> cdt, float jit) {\n" \
"  if (cld.w < 0.5 || d.z < -0.02 || h > CLOUD_BASE) return float4(0, 0, 0, 1);\n" \
"  float r0 = 6360.0 + h, t0 = cshell(r0, d.z, 6360.0 + CLOUD_BASE), t1 = cshell(r0, d.z, 6360.0 + CLOUD_TOP);\n" \
"  if (t0 < 0.0 || t1 <= t0) return float4(0, 0, 0, 1);\n" \
"  t1 = min(t1, t0 + 12.0);\n" \
"  float fade = exp(-t0 / 60.0);   /* the far field into the air's own light */\n" \
"  if (fade < 0.02) return float4(0, 0, 0, 1);\n" \
"  int N = int(mix(40.0, 72.0, 1.0 - saturate(d.z * 3.0))); float dt = (t1 - t0) / float(N), sig = cld.y * 1000.0;\n" \
"  float mu = dot(d, sun); float3 L = 0.0; float T = 1.0; float3 o = float3(0, 0, r0);\n" \
"  for (int i = 0; i < N && T > 0.01; i++) {\n" \
"    float t = t0 + (float(i) + jit) * dt; float3 p = o + d * t; float alt = length(p) - 6360.0;\n" \
"    float rho = cloud_rho(p, alt, cld, cn, cdt, true); if (rho <= 0.002) continue;\n" \
"    float st = rho * sig, od = 0.0, ls = 0.06;\n" \
"    for (int k = 0; k < 6; k++) { float3 q = p + sun * ls * (float(k) + 0.5); od += cloud_rho(q, length(q) - 6360.0, cld, cn, cdt, false) * sig * ls; ls *= 1.6; }\n" \
"    float hh = saturate((alt - CLOUD_BASE) / (CLOUD_TOP - CLOUD_BASE));\n" \
/* the light deep in a thick cloud: the octaves still fall off as exp(-tau / 8), \
   black under a cumulus' tau of 50; the two-stream (diffusion) transmission \
   1 / (1 + 0.75 (1 - g) tau), g 0.85 for cumulus, leaves 0.15 of it there \
   (a cloud base is grey, not black), emerging as a Lambertian source E mu0 T / pi, \
   weighted in as the light goes deep (1 - exp(-tau / 2)), so a thin cloud's \
   light is the octaves'; the larger of the two (both are the same light) */ \
"    float mu0 = max(sun.z, 0.05), tdif = 0.98 * mu0 / (1.0 + 0.1125 * od * mu0) / 3.14159265 * (1.0 - exp(-0.5 * od));   /* a slab: the flux E mu0 per area, the vertical depth od mu0 */\n" \
"    float3 S = tsun * max(cms(od, mu), tdif) + amb * mix(0.55, 1.0, hh);   /* the larger, not the sum: the octaves already carry the shallow light */\n" \
"    float a = exp(-st * dt); L += T * S * (1.0 - a); T *= a; }\n" \
"  return float4(L * fade, mix(1.0, T, fade)); }\n" \
/* the layer's transmittance from the eye toward the light (the glare's, brr_metal_glare.h): one thread */ \
"kernel void cloud_light_t(constant KU &k [[buffer(0)]], device float *out [[buffer(1)]], texture3d<float> cn [[texture(0)]], texture3d<float> cdt [[texture(1)]]) {\n" \
"  float3 d = k.sun.xyz; float h = k.eye.w, r0 = 6360.0 + h; out[0] = 1.0; out[1] = -1.0;\n" \
"  if (k.cld.w < 0.5 || d.z <= 0.0 || h > CLOUD_BASE) return;\n" \
"  float t0 = cshell(r0, d.z, 6360.0 + CLOUD_BASE), t1 = min(cshell(r0, d.z, 6360.0 + CLOUD_TOP), t0 + 12.0), od = 0.0; float3 o = float3(0, 0, r0);\n" \
"  for (int i = 0; i < 128; i++) { float3 p = o + d * mix(t0, t1, (float(i) + 0.5) / 128.0); od += cloud_rho(p, length(p) - 6360.0, k.cld, cn, cdt, true) * k.cld.y * 1000.0 * (t1 - t0) / 128.0; }\n" \
"  out[0] = exp(-od); out[1] = od; out[2] = t0; out[3] = t1; }   /* 1..3: the optical depth and the segment (the tests' view) */\n" \
/* the noises, once: tileable Worley (F1 over the 27 cells about a point, \
   the cells' feature points hashed, wrapped at the period) and Perlin \
   gradient noise of a period, fBms of them; cn 128^3, cdt 32^3 */ \
"static float3 nhash3(int3 c) { uint3 v = uint3(c) * uint3(1597334673u, 3812015801u, 2798796415u); uint n = (v.x ^ v.y ^ v.z) * 1597334673u;\n" \
"  return float3(n & 1023u, (n >> 10) & 1023u, (n >> 20) & 1023u) / 1023.0; }\n" \
"static float worley(float3 x, int P) { float3 g = x * float(P); int3 c = int3(floor(g)); float3 f = g - floor(g); float m = 9.0;\n" \
"  for (int k = -1; k <= 1; k++) for (int j = -1; j <= 1; j++) for (int i = -1; i <= 1; i++) {\n" \
"    int3 o = int3(i, j, k), cc = ((c + o) % P + P) % P; float3 r = float3(o) + nhash3(cc) - f; m = min(m, dot(r, r)); }\n" \
"  return 1.0 - saturate(sqrt(m)); }\n" \
"static float wfbm(float3 x, int P) { return worley(x, P) * 0.625 + worley(x, P * 2) * 0.25 + worley(x, P * 4) * 0.125; }\n" \
"static float pgrad(int3 c, float3 f, int P) { int3 cc = (c % P + P) % P; float3 g = nhash3(cc + int3(17, 59, 113)) * 2.0 - 1.0; return dot(normalize(g + 1e-4), f); }\n" \
"static float perlin(float3 x, int P) { float3 g = x * float(P); int3 c = int3(floor(g)); float3 f = g - floor(g), u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);\n" \
"  float n000 = pgrad(c, f, P), n100 = pgrad(c + int3(1,0,0), f - float3(1,0,0), P), n010 = pgrad(c + int3(0,1,0), f - float3(0,1,0), P), n110 = pgrad(c + int3(1,1,0), f - float3(1,1,0), P);\n" \
"  float n001 = pgrad(c + int3(0,0,1), f - float3(0,0,1), P), n101 = pgrad(c + int3(1,0,1), f - float3(1,0,1), P), n011 = pgrad(c + int3(0,1,1), f - float3(0,1,1), P), n111 = pgrad(c + int3(1,1,1), f - float3(1,1,1), P);\n" \
"  return mix(mix(mix(n000, n100, u.x), mix(n010, n110, u.x), u.y), mix(mix(n001, n101, u.x), mix(n011, n111, u.x), u.y), u.z); }\n" \
"kernel void cloud_noise(uint3 id [[thread_position_in_grid]], texture3d<float, access::write> o [[texture(0)]], constant int &detail [[buffer(0)]]) {\n" \
"  uint n = o.get_width(); if (id.x >= n || id.y >= n || id.z >= n) return; float3 x = (float3(id) + 0.5) / float(n);\n" \
"  if (detail) { o.write(float4(wfbm(x, 2), wfbm(x, 4), wfbm(x, 8), 1.0), id); return; }\n" \
"  float p = saturate(0.5 + 0.5 * (perlin(x, 4) * 0.625 + perlin(x, 8) * 0.25 + perlin(x, 16) * 0.125) * 1.4);\n" \
"  float w = wfbm(x, 4), pw = w + p * (1.0 - w);   /* Perlin dilated by Worley (Schneider 2015) */\n" \
"  o.write(float4(pw, w, wfbm(x, 8), wfbm(x, 16)), id); }\n"

#endif
