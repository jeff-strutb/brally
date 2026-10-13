/* brr_metal_clouds.h: the Remastered cloud layer over the physical sky (port
 * code; in BRR_SKY_SRC, drawn by skyfs in place of the backdrop's painted
 * clouds, which brr_skymask.h now lets through as sky).
 *
 * One slab of cloud from 1.8 to 2.4 km above the ground, its density a 2D fBm
 * (5 octaves of value noise, cells of 2.5 km down to 160 m) drifting with the
 * wind, cut by the weather's coverage (KU.cld.x: sparse cumulus when sunny,
 * overcast in fog and rain) and scaled by its extinction (KU.cld.y, per
 * metre).  Along the view ray through the slab, optical depth tau and Beer's
 * law T = exp(-tau); the sun's light into the slab is its transmittance down
 * the atmosphere to 2 km, then multiple scattering by Wrenninge's octaves
 * (Wrenninge 2013, as Hillaire 2016 and Schneider 2015 use them: four
 * octaves, the n-th weighted a^n, its self-shadow optical depth scaled b^n,
 * its phase's eccentricities c^n, a = b = c = 1/2; the weights normalised so
 * the slab's albedo is 0.98 at most) over a two-lobe Henyey-Greenstein phase
 * (forward g 0.6, back -0.3, weights 0.7 and 0.3; each lobe and so the mix
 * integrates to 1 over the sphere).  The light the deep octaves carry through
 * the self-shadow is what makes a thick cumulus white, not grey.  The ambient
 * (amb, from skyfs) is isotropic scattering of the sky above and the sunlit
 * ground below: albedo / 2 (L_zenith + L_ground).  The slab's own light is (1 - T) times that; the sky behind it is
 * dimmed by T.  The layer fades out over the last 5 degrees above the
 * horizon.  KU.cld.z is the wind's clock (seconds), KU.cld.w the tier flag. */
#ifndef BRR_METAL_CLOUDS_H
#define BRR_METAL_CLOUDS_H

#define BRR_CLOUD_SRC \
"static float hgp(float c, float g) { float q = 1.0 + g * g - 2.0 * g * c; return (1.0 - g * g) / (4.0 * 3.14159265 * q * sqrt(q)); }\n" \
"static float cphase(float c) { return 0.7 * hgp(c, 0.6) + 0.3 * hgp(c, -0.3); }\n" \
"static float vhash(float2 p) { p = fract(p * float2(0.1031, 0.1030)); p += dot(p, p.yx + 33.33); return fract((p.x + p.y) * p.x); }\n" \
"static float vnoise(float2 p) { float2 i = floor(p), f = fract(p), u = f * f * (3.0 - 2.0 * f);\n" \
"  return mix(mix(vhash(i), vhash(i + float2(1, 0)), u.x), mix(vhash(i + float2(0, 1)), vhash(i + float2(1, 1)), u.x), u.y); }\n" \
"static float cfbm(float2 p) { float a = 0.5, s = 0.0; for (int i = 0; i < 5; i++) { s += a * vnoise(p); p = p * 2.0 + float2(17.1, 5.3); a *= 0.5; } return s / 0.96875; }\n" \
"static float cdens(float2 km, float cover) {   /* 0..1: the slab's density at a point (km), the weather's coverage */\n" \
"  float n = cfbm(km * 0.4); return saturate((n - (1.0 - cover)) / max(cover, 0.05) * 2.0); }\n" \
/* the slab seen along d from height h km: rgb, its own light per unit of the \
   sun's top-of-atmosphere illuminance; a, its transmittance */ \
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
"static float4 clouds(float3 d, float3 sun, float h, float4 cld, float3 tsun, float3 amb) {\n" \
"  if (cld.w < 0.5 || d.z < 0.005 || h > 1.8) return float4(0, 0, 0, 1);\n" \
"  float t0 = (1.8 - h) / d.z, L = min(0.6 / d.z, 30.0); float2 wind = float2(0.012, 0.005) * cld.z;\n" \
"  float2 p = d.xy * (t0 + 0.5 * L) + wind; float rho = cdens(p, cld.x);\n" \
"  float tau = cld.y * 1000.0 * rho * L, T = exp(-tau);\n" \
"  float ts = cld.y * 1000.0 * cdens(p + sun.xy / max(sun.z, 0.15) * 0.3, cld.x) * 0.3 / max(sun.z, 0.15);\n" \
"  float3 Ls = tsun * cms(ts, dot(d, sun)) + amb;\n" \
"  float f = smoothstep(0.005, 0.087, d.z);\n" \
"  return float4(Ls * (1.0 - T) * f, mix(1.0, T, f)); }\n"

#endif
