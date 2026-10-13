/* brr_metal_vol.h: volumetric fog (DESIGN.md 9), the froxel method of
 * Wronski 2014 and Hillaire 2015.  A grid of VOL_NX x VOL_NY x VOL_NZ cells
 * over the camera's view, its slices exponential in distance from the eye
 * out to VOL_FAR; per cell the medium's extinction (the weather's density,
 * thinning with height, a gentle 3D noise) and the light it scatters toward
 * the eye: the sun (Henyey-Greenstein phase, its visibility one tap of the
 * cascades), the sky (isotropic) and the lamps of the cell's light cluster.
 * Each frame's cells are blended with last frame's at the same place in the
 * world (temporal reprojection; the sample's depth in its slice jittered
 * each frame, so the blend removes the slices' banding).  The cells are then
 * integrated front to back along each column with Hillaire's
 * energy-conserving step into (in-scattered light, transmittance) up to each
 * slice's far side, and a full-screen pass after the lighting, before the
 * blended 3D, applies that to every pixel at its distance (the sky and the
 * backdrop at VOL_FAR): colour * T + S.
 *
 * The plain-C half is the reference the tests (ports/brally/tools/lighttest,
 * vol.sh) hold the shaders to; BRR_VOL_SRC is the shaders, appended to the
 * renderer's source after the lighting's (it uses LU, LSet, lamp, lwin,
 * lc_index). */
#ifndef BRR_METAL_VOL_H
#define BRR_METAL_VOL_H

#include <math.h>

#define VOL_NX 160
#define VOL_NY 90
#define VOL_NZ 64
#define VOL_NEAR 0.5f           /* metres: the first slice's far side is VOL_NEAR * (VOL_FAR / VOL_NEAR)^(1 / VOL_NZ) */
#define VOL_FAR 300.0f          /* metres: the grid's reach (the cascades' too) */
#define VOL_HISTORY 0.9f        /* the share of last frame's cell kept */

/* the medium per weather (the order of brr_metal.m's k_sky and k_weather):
 * extinction at the ground, 1/m (Koschmieder: visibility = 3.912 / sigma);
 * single-scattering albedo; the height over which it thins by e, metres;
 * the noise's amplitude */
static const struct { float sigma, albedo, height, noise; } k_vol[5] = {
    /* sunny: a light haze, 20 km  */ { 0.0002f, 0.90f, 300.0f, 0.15f },
    /* fog:   150 m                */ { 0.0260f, 0.98f,  40.0f, 0.25f },
    /* rain:  1 km                 */ { 0.0040f, 0.95f, 120.0f, 0.20f },
    /* snow:  700 m                */ { 0.0055f, 0.97f, 150.0f, 0.20f },
    /* night: a mist, 4 km         */ { 0.0010f, 0.95f,  60.0f, 0.20f },
};
#define VOL_G 0.7f              /* Henyey-Greenstein asymmetry: a forward-scattering haze */

/* the volume's uniforms (the shaders' VU, the same layout): screen pixel to
 * world (the inverse of the screen map's x, y and w rows, and their
 * translations), last frame's screen map and eye (w: 1 when last frame's
 * cells are valid), the grid, the target and this frame's jitter, the
 * medium */
typedef struct BrrVU {
    float ir0[4], ir1[4], ir2[4], tr[4];
    float pscr[16];
    float peye[4];
    float grid[4];      /* nx, ny, nz, far */
    float tgt[4];       /* target width, height, jitter 0..1, near */
    float med[4];       /* sigma_t at the ground, albedo, height, ground z */
    float nse[4];       /* noise amplitude, noise scale 1/m, time s, g */
    float hist[4];      /* history weight */
} BrrVU;

/* ---- the reference ---- */

/* Henyey-Greenstein, per steradian: c the cosine between the light's travel
 * and the scattered direction */
static inline float vol_hg(float g, float c)
{
    float d = 1.0f + g * g - 2.0f * g * c;
    return (1.0f - g * g) / (4.0f * 3.14159265f * d * sqrtf(d));
}

/* a slice coordinate (0..VOL_NZ) to its distance, and back */
static inline float vol_dist(float s) { return VOL_NEAR * powf(VOL_FAR / VOL_NEAR, s / (float)VOL_NZ); }
static inline float vol_slice(float t) { return (float)VOL_NZ * logf(fmaxf(t, 1e-4f) / VOL_NEAR) / logf(VOL_FAR / VOL_NEAR); }
/* slice k's near and far sides: slice 0 starts at the eye */
static inline float vol_t0(int k) { return k ? vol_dist((float)k) : 0.0f; }
static inline float vol_t1(int k) { return vol_dist((float)(k + 1)); }

/* one step of the integration, a channel: a cell of extinction st and
 * in-scattering sc (light scattered per metre) over length ds, the
 * light and transmittance so far (S, T) carried across it.  Hillaire 2015:
 * the in-scattering integrated analytically over the cell, as the light
 * fades through it, int_0^ds sc exp(-st x) dx = (sc - sc exp(-st ds)) / st */
static inline void vol_step(float st, float sc, float ds, float *S, float *T)
{
    float tr = expf(-st * ds), si = st > 1e-7f ? (sc - sc * tr) / st : sc * ds;
    *S += *T * si;
    *T *= tr;
}
/* the naive Riemann step (the cell's light at its near side, times its length) */
static inline void vol_step_naive(float st, float sc, float ds, float *S, float *T)
{
    *S += *T * sc * ds;
    *T *= expf(-st * ds);
}

/* the medium's density factor at height z (above the ground zg): 1 at the
 * ground and below, falling by e every h metres */
static inline float vol_height(float z, float zg, float h) { return expf(-fmaxf(z - zg, 0.0f) / h); }

/* the screen map's x, y and w rows inverted over x, y, z: the world point on
 * a pixel's ray where w = 1 */
static inline void vol_inverse(const float s[16], BrrVU *v)
{
    double a[3][3], inv[3][3], det;
    int r, c;
    for (r = 0; r < 3; r++)
        for (c = 0; c < 3; c++)
            a[r][c] = s[c * 4 + (r == 2 ? 3 : r)];
    det = a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1]) - a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0])
        + a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0]);
    if (fabs(det) < 1e-30)
        det = 1e-30;
    inv[0][0] = (a[1][1] * a[2][2] - a[1][2] * a[2][1]) / det;
    inv[0][1] = (a[0][2] * a[2][1] - a[0][1] * a[2][2]) / det;
    inv[0][2] = (a[0][1] * a[1][2] - a[0][2] * a[1][1]) / det;
    inv[1][0] = (a[1][2] * a[2][0] - a[1][0] * a[2][2]) / det;
    inv[1][1] = (a[0][0] * a[2][2] - a[0][2] * a[2][0]) / det;
    inv[1][2] = (a[0][2] * a[1][0] - a[0][0] * a[1][2]) / det;
    inv[2][0] = (a[1][0] * a[2][1] - a[1][1] * a[2][0]) / det;
    inv[2][1] = (a[0][1] * a[2][0] - a[0][0] * a[2][1]) / det;
    inv[2][2] = (a[0][0] * a[1][1] - a[0][1] * a[1][0]) / det;
    for (c = 0; c < 3; c++) {
        v->ir0[c] = (float)inv[0][c];
        v->ir1[c] = (float)inv[1][c];
        v->ir2[c] = (float)inv[2][c];
    }
    v->tr[0] = s[12]; v->tr[1] = s[13]; v->tr[2] = s[15];
}

/* the volume's sampling, shared by its full-screen pass and the forward-lit
 * blended 3D (BRR_LIT_SRC's fsf, which comes before BRR_VOL_SRC): every 3D
 * surface of the lit frame, opaque or blended, takes the same (in-scattered
 * light, transmittance) at its pixel and distance */
#define BRR_VOL_AT_SRC \
"struct VU { float4 ir0; float4 ir1; float4 ir2; float4 tr; float4x4 pscr; float4 peye; float4 grid; float4 tgt; float4 med; float4 nse; float4 hist; };\n" \
"static float vol_dist(constant VU &v, float s) { return v.tgt.w * pow(v.grid.w / v.tgt.w, s / v.grid.z); }\n" \
"static float vol_slice(constant VU &v, float t) { return v.grid.z * log(max(t, 1e-4) / v.tgt.w) / log(v.grid.w / v.tgt.w); }\n" \
/* the volume at distance t along a pixel's ray: (light, transmittance) */ \
"static float4 vol_at(constant VU &v, texture3d<float> iv, float2 px, float t) {\n" \
"  constexpr sampler s3(filter::linear, address::clamp_to_edge);\n" \
"  float i = vol_slice(v, min(t, v.grid.w)) - 1.0;\n" \
"  float4 r = iv.sample(s3, float3(px / v.tgt.xy, (max(i, 0.0) + 0.5) / v.grid.z));\n" \
"  if (i < 0.0) { float k = saturate(t / vol_dist(v, 1.0)); r = mix(float4(0, 0, 0, 1), r, k); }\n" \
"  return r; }\n" \
/* a surface at world point P on target pixel px, seen from eye: (light, transmittance); \
   P zero is an empty pixel (the sky, the backdrop), at the grid's far side */ \
"static float4 vol_pix(constant VU &v, texture3d<float> iv, float2 px, float3 P, float3 eye) {\n" \
"  return vol_at(v, iv, px, all(P == 0.0) ? v.grid.w : length(P - eye)); }\n" \
/* a blended surface's colour c through the volume: c * T + S; an additive one (light it adds) only c * T, \
   the in-scattered light being the surface's behind it already */ \
"static float3 vol_fwd(constant VU &v, texture3d<float> iv, float2 px, float3 wp, float3 eye, float3 c, bool add) {\n" \
"  float4 r = vol_pix(v, iv, px, wp, eye); return c * r.a + (add ? float3(0.0) : min(r.rgb, float3(60000.0))); }\n"

#define BRR_VOL_SRC \
"static float vol_hg(float g, float c) { float d = 1.0 + g * g - 2.0 * g * c; return (1.0 - g * g) / (4.0 * 3.14159265 * d * sqrt(d)); }\n" \
/* the world ray through a target pixel: the screen map solved for w = 1 */ \
"static float3 vol_ray(constant VU &v, float3 eye, float2 px) {\n" \
"  float3 c = float3(px, 1.0) - v.tr.xyz; float3 p = float3(dot(v.ir0.xyz, c), dot(v.ir1.xyz, c), dot(v.ir2.xyz, c));\n" \
"  return normalize(p - eye); }\n" \
/* value noise, trilinear over a hashed lattice */ \
"static float vol_hash(float3 p) { p = fract(p * 0.3183099 + 0.1); p *= 17.0; return fract(p.x * p.y * p.z * (p.x + p.y + p.z)); }\n" \
"static float vol_noise(float3 x) {\n" \
"  float3 i = floor(x), f = fract(x); f = f * f * (3.0 - 2.0 * f);\n" \
"  return mix(mix(mix(vol_hash(i), vol_hash(i + float3(1, 0, 0)), f.x), mix(vol_hash(i + float3(0, 1, 0)), vol_hash(i + float3(1, 1, 0)), f.x), f.y),\n" \
"             mix(mix(vol_hash(i + float3(0, 0, 1)), vol_hash(i + float3(1, 0, 1)), f.x), mix(vol_hash(i + float3(0, 1, 1)), vol_hash(i + float3(1, 1, 1)), f.x), f.y), f.z); }\n" \
"static float vol_sigma(constant VU &v, float3 p) {\n" \
"  float h = exp(-max(p.z - v.med.w, 0.0) / v.med.z);\n" \
"  float n = v.nse.x > 0.0 ? 1.0 + v.nse.x * (2.0 * vol_noise(p * v.nse.y + float3(0.3, 0.1, 0.0) * v.nse.z) - 1.0) : 1.0;\n" \
"  return v.med.x * h * n; }\n" \
/* the sun's visibility at a point in the air: one bilinear comparison in its cascade */ \
"static float vol_sun(constant LU &u, depth2d_array<float> sm, float3 p) {\n" \
"  if (u.misc.y < 0.5) return 1.0;\n" \
"  constexpr sampler cs(filter::linear, address::clamp_to_edge, compare_func::less_equal);\n" \
"  float d = dot(p - u.eye.xyz, u.cfwd.xyz); int k = 0;\n" \
"  while (k < 3 && d > u.csplit[k]) k++;\n" \
"  if (d > u.csplit[3]) return 1.0;\n" \
"  float4 c = u.sunvp[k] * float4(p, 1.0); float2 st = float2(c.x * 0.5 + 0.5, 0.5 - c.y * 0.5);\n" \
"  if (any(st <= 0.0) || any(st >= 1.0) || c.z >= 1.0) return 1.0;\n" \
"  return sm.sample_compare(cs, st, uint(k), c.z - u.cbias[k]); }\n" \
/* the light a point scatters toward the eye per unit of extinction, before the albedo: \
   the sun (vis its visibility), the sky's light (skyc: the illuminance on a face toward it, \
   as radiance over the sky's half: skyc / pi, scattered isotropically), the lamps of the point's cluster */ \
"static float3 vol_inlight(constant LU &u, constant VU &v, const device LSet &ls, float3 p, float3 d, float vis) {\n" \
"  float3 L = u.sunc.rgb * vis * vol_hg(v.nse.w, dot(u.sun.xyz, d)) + u.skyc.rgb * (1.0 / 3.14159265);\n" \
"  uint e = ls.cl[lc_index(ls.h, p)], o = e >> 8, cnt = min(e & 255u, 16u);\n" \
"  for (uint k = 0; k < cnt; k++) {\n" \
"    LS l = ls.l[ls.idx[o + k]];\n" \
"    float3 q = float3(l.pos) - p; float d2 = max(dot(q, q), 0.01), dl = sqrt(d2); float3 ld = q / dl;\n" \
"    float E = lamp(l, -ld) * lwin(l, -ld, dl) / d2;\n" \
"    if (E > 0.0) L += float3(l.rgb) * E * vol_hg(v.nse.w, dot(ld, d));\n" \
"  }\n" \
"  return L; }\n" \
/* a cell: (scattered light per metre, pre-exposed; extinction), blended with last frame's */ \
"kernel void vol_inject(uint3 id [[thread_position_in_grid]], constant LU &u [[buffer(0)]], constant VU &v [[buffer(1)]],\n" \
"                       const device LSet &ls [[buffer(2)]], depth2d_array<float> sm [[texture(0)]],\n" \
"                       texture3d<float> prev [[texture(1)]], texture3d<float, access::write> out [[texture(2)]]) {\n" \
"  if (id.x >= uint(v.grid.x) || id.y >= uint(v.grid.y) || id.z >= uint(v.grid.z)) return;\n" \
"  float2 px = (float2(id.xy) + 0.5) / v.grid.xy * v.tgt.xy;\n" \
"  float3 d = vol_ray(v, u.eye.xyz, px);\n" \
"  float t0 = id.z ? vol_dist(v, float(id.z)) : 0.0, t1 = vol_dist(v, float(id.z) + 1.0);\n" \
"  float t = mix(t0, t1, v.tgt.z); float3 p = u.eye.xyz + d * t;\n" \
"  float st = vol_sigma(v, p);\n" \
"  float3 sc = st * v.med.y * vol_inlight(u, v, ls, p, d, vol_sun(u, sm, p)) * u.misc.x;\n" \
"  float4 c = float4(min(sc, float3(60000.0)), st);\n" \
"  if (v.peye.w > 0.5) {\n" \
"    float4 h = v.pscr * float4(p, 1.0);\n" \
"    if (h.w > 1e-4) {\n" \
"      float3 uvw = float3(h.xy / h.w / v.tgt.xy, vol_slice(v, length(p - v.peye.xyz)) / v.grid.z);\n" \
"      constexpr sampler ls3(filter::linear, address::clamp_to_edge);\n" \
"      if (all(uvw >= 0.0) && all(uvw <= 1.0)) c = mix(c, prev.sample(ls3, uvw), v.hist.x);\n" \
"    }\n" \
"  }\n" \
"  out.write(c, id); }\n" \
/* the column's front-to-back integration: each slice's (light, transmittance) up to its far side */ \
"kernel void vol_integrate(uint2 id [[thread_position_in_grid]], constant VU &v [[buffer(1)]],\n" \
"                          texture3d<float> cell [[texture(0)]], texture3d<float, access::write> out [[texture(1)]]) {\n" \
"  if (id.x >= uint(v.grid.x) || id.y >= uint(v.grid.y)) return;\n" \
"  float3 S = float3(0.0); float T = 1.0; float t0 = 0.0;\n" \
"  for (uint z = 0; z < uint(v.grid.z); z++) {\n" \
"    float4 c = cell.read(uint3(id, z)); float t1 = vol_dist(v, float(z) + 1.0), ds = t1 - t0;\n" \
"    float st = max(c.a, 1e-7), tr = exp(-st * ds);\n" \
"    S += T * (c.rgb - c.rgb * tr) / st; T *= tr; t0 = t1;\n" \
"    out.write(float4(S, T), uint3(id, z));\n" \
"  }\n" \
"}\n" \
/* the full-screen pass: blended (one, source alpha), colour * T + S; an empty pixel (the sky, the backdrop) at the grid's far side */ \
"fragment float4 vol_apply(LO in [[stage_in]], constant LU &u [[buffer(0)]], constant VU &v [[buffer(1)]],\n" \
"                          texture2d<float> pos [[texture(0)]], texture3d<float> iv [[texture(1)]]) {\n" \
"  float4 r = vol_pix(v, iv, in.pos.xy, pos.read(uint2(in.pos.xy)).xyz, u.eye.xyz);\n" \
"  return float4(min(r.rgb, float3(60000.0)), r.a); }\n"

#ifdef __OBJC__
#import <Metal/Metal.h>
/* the volume's frame (brr_metal_vol.m): on (returns 1) when the tier has it
 * and BR_LIGHT_VOL is not 0; the cells injected and integrated, then the
 * full-screen pass over hdr.  u: the lighting's uniforms (LU; vol.x set by
 * the caller from brr_vol_on), scr: world to the target's pixels, w the
 * weather */
int brr_vol_on(id<MTLDevice> dev);
void brr_vol_frame(id<MTLDevice> dev, id<MTLLibrary> lib, id<MTLCommandBuffer> cmd, const void *lu, size_t lusz,
                   id<MTLBuffer> lset, id<MTLTexture> shadow, id<MTLTexture> gpos, id<MTLTexture> hdr,
                   const float eye[3], const float scr[16], int w, double time);
/* this frame's volume (its uniforms at buf, the integrated volume at tex)
 * for the forward pass's fsf */
void brr_vol_bind(id<MTLRenderCommandEncoder> e, NSUInteger buf, NSUInteger tex);
#endif

#endif
