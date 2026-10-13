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
 * the atmosphere to 2 km, its self-shadow exp(-tau_sun / 4) (the first octave
 * of the multiple-scattering approximation, Wrenninge 2015), a two-lobe
 * Henyey-Greenstein phase (forward g 0.6, back -0.3, weights 0.7 and 0.3; each
 * lobe and so the mix integrates to 1 over the sphere) and the powder term
 * 1 - exp(-2 tau) (Schneider 2015); the sky's light is the sky view at the
 * zenith.  The slab's own light is (1 - T) times that; the sky behind it is
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
"static float4 clouds(float3 d, float3 sun, float h, float4 cld, float3 tsun, float3 amb) {\n" \
"  if (cld.w < 0.5 || d.z < 0.005 || h > 1.8) return float4(0, 0, 0, 1);\n" \
"  float t0 = (1.8 - h) / d.z, L = min(0.6 / d.z, 30.0); float2 wind = float2(0.012, 0.005) * cld.z;\n" \
"  float2 p = d.xy * (t0 + 0.5 * L) + wind; float rho = cdens(p, cld.x);\n" \
"  float tau = cld.y * 1000.0 * rho * L, T = exp(-tau);\n" \
"  float ts = cld.y * 1000.0 * cdens(p + sun.xy / max(sun.z, 0.15) * 0.3, cld.x) * 0.3 / max(sun.z, 0.15);\n" \
"  float3 Ls = tsun * exp(-0.25 * ts) * cphase(dot(d, sun)) * (1.0 - exp(-2.0 * tau)) + amb;\n" \
"  float f = smoothstep(0.005, 0.087, d.z);\n" \
"  return float4(Ls * (1.0 - T) * f, mix(1.0, T, f)); }\n"

#endif
