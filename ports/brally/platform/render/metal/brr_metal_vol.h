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
    /* night: a coastal haze, 1.3 km */ { 0.0030f, 0.95f,  60.0f, 0.20f },
};
#define VOL_G 0.7f              /* Henyey-Greenstein asymmetry: a forward-scattering haze */
#define VOL_BEAMS 2             /* the lighthouse beams the full-screen pass scatters (Coastline's two) */
#define VOL_BEAM_STEPS 32       /* the march's steps over the lit stretch of a ray (the shader's loop) */
#define VOL_BEAM_SKY 3000.0f    /* metres: the distance an empty pixel (the sky, the backdrop) gives the beam's march */

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
    float bpos[VOL_BEAMS][4];   /* the lighthouse beams (vol_beam): the lamp, w its candela (0: none) */
    float baxis[VOL_BEAMS][4];  /* the beam's axis, w the cosine of its outer angle */
    float bcol[4];              /* the beams' colour (luminance 1), w the cosine of the inner angle */
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

/* ---- the lighthouse beams: single scattering along the view ray ----
 * A lighthouse's beam is seen only as its light scattered by the air.  Per
 * pixel, along the ray from the eye o (unit direction d) to the pixel's
 * distance tmax, the radiance scattered toward the eye:
 *
 *   L = int sigma_s I f(cos) / r^2  p_HG(g, ld . -d)  exp(-sigma_t r)  exp(-sigma_t t)  dt
 *
 * (r the distance from the lamp, ld the light's travel, f the beam's
 * angular profile: 1 inside the inner angle, smoothstep in the cosine to 0
 * at the outer; sigma the medium's at the ground, homogeneous; r^2 no less
 * than 1 m^2, the lantern's size).  The integrand is zero outside the cone of
 * the outer angle, so the march covers only the ray's interval inside it
 * (vol_beam_span, the ray against the one nappe of the cone: convex, so one
 * interval), with equiangular steps (Kulla and Fajardo 2012: uniform in the
 * angle seen from the lamp, t = tc + D tan(theta), dt = D (1 + tan^2) dtheta,
 * which cancels the 1/r^2), stratified and jittered per pixel and frame (j). */
static inline float vol_dot3(const float *a, const float *b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
static inline float vol_smooth(float a, float b, float x)
{
    float t = (x - a) / (b - a);
    t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
    return t * t * (3.0f - 2.0f * t);
}
/* the ray's interval inside the cone (apex A, unit axis a, cosine c of its
 * half angle) within [0, tmax]: 1 and [*t0, *t1], or 0 */
static inline int vol_beam_span(const float o[3], const float d[3], const float A[3], const float a[3], float c, float tmax,
                                float *t0, float *t1)
{
    float w[3] = { o[0] - A[0], o[1] - A[1], o[2] - A[2] };
    float da = vol_dot3(d, a), wa = vol_dot3(w, a), c2 = c * c;
    float qa = da * da - c2, qb = da * wa - c2 * vol_dot3(d, w), qc = wa * wa - c2 * vol_dot3(w, w);
    float lo[2], hi[2], hl, hh, s0 = 1e30f, s1 = -1e30f;
    int n = 0, k;
    if (fabsf(qa) < 1e-9f) {                    /* the ray parallel to the cone's side */
        if (fabsf(qb) < 1e-12f) return 0;
        if (qb > 0.0f) { lo[0] = -qc / (2.0f * qb); hi[0] = 1e30f; }
        else { lo[0] = -1e30f; hi[0] = -qc / (2.0f * qb); }
        n = 1;
    } else {
        float disc = qb * qb - qa * qc, sq, r0, r1;
        if (disc < 0.0f) {
            if (qa < 0.0f) return 0;
            lo[0] = -1e30f; hi[0] = 1e30f; n = 1;
        } else {
            sq = sqrtf(disc);
            r0 = (-qb - sq) / qa; r1 = (-qb + sq) / qa;
            if (r0 > r1) { float x = r0; r0 = r1; r1 = x; }
            if (qa < 0.0f) { lo[0] = r0; hi[0] = r1; n = 1; }
            else { lo[0] = -1e30f; hi[0] = r0; lo[1] = r1; hi[1] = 1e30f; n = 2; }
        }
    }
    /* the nappe in front of the lamp: wa + t da >= 0 */
    if (fabsf(da) < 1e-12f) { if (wa < 0.0f) return 0; hl = -1e30f; hh = 1e30f; }
    else if (da > 0.0f) { hl = -wa / da; hh = 1e30f; }
    else { hl = -1e30f; hh = -wa / da; }
    for (k = 0; k < n; k++) {
        float l = fmaxf(fmaxf(lo[k], hl), 0.0f), h = fminf(fminf(hi[k], hh), tmax);
        if (h > l) { s0 = fminf(s0, l); s1 = fmaxf(s1, h); }
    }
    if (s1 <= s0) return 0;
    *t0 = s0; *t1 = s1;
    return 1;
}
/* the integrand at t, per candela per unit sigma_s: f / r^2 p T T */
static inline float vol_beam_f(const float o[3], const float d[3], const float A[3], const float a[3], float cout, float cin,
                               float st, float g, float t)
{
    float q[3] = { o[0] + d[0] * t - A[0], o[1] + d[1] * t - A[1], o[2] + d[2] * t - A[2] };
    float r2 = vol_dot3(q, q), r = sqrtf(r2), ca;
    if (r < 1e-6f) return 0.0f;
    ca = vol_dot3(q, a) / r;
    if (ca <= cout) return 0.0f;
    return vol_smooth(cout, cin, ca) * vol_hg(g, -vol_dot3(q, d) / r) * expf(-st * (r + t)) / fmaxf(r2, 1.0f);
}
/* the march: L per candela per unit sigma_s, j the jitter 0..1, n steps */
static inline float vol_beam(const float o[3], const float d[3], const float A[3], const float a[3], float cout, float cin,
                             float st, float g, float tmax, float j, int n)
{
    float t0, t1, tc, f[3], D, th0, th1, dth, s = 0.0f;
    int i;
    if (!vol_beam_span(o, d, A, a, cout, tmax, &t0, &t1))
        return 0.0f;
    tc = (A[0] - o[0]) * d[0] + (A[1] - o[1]) * d[1] + (A[2] - o[2]) * d[2];
    f[0] = o[0] + d[0] * tc - A[0]; f[1] = o[1] + d[1] * tc - A[1]; f[2] = o[2] + d[2] * tc - A[2];
    D = fmaxf(sqrtf(vol_dot3(f, f)), 0.05f);
    th0 = atanf((t0 - tc) / D); th1 = atanf((t1 - tc) / D); dth = (th1 - th0) / (float)n;
    for (i = 0; i < n; i++) {
        float th = th0 + dth * ((float)i + j), u = tanf(th), t = tc + D * u;
        s += vol_beam_f(o, d, A, a, cout, cin, st, g, t) * D * (1.0f + u * u);
    }
    return s * dth;
}

/* the volume's sampling, shared by its full-screen pass and the forward-lit
 * blended 3D (BRR_LIT_SRC's fsf, which comes before BRR_VOL_SRC): every 3D
 * surface of the lit frame, opaque or blended, takes the same (in-scattered
 * light, transmittance) at its pixel and distance */
#define BRR_VOL_AT_SRC \
"struct VU { float4 ir0; float4 ir1; float4 ir2; float4 tr; float4x4 pscr; float4 peye; float4 grid; float4 tgt; float4 med; float4 nse; float4 hist;\n" \
"            float4 bpos[2]; float4 baxis[2]; float4 bcol; };\n" \
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
/* the lighthouse beams (the C reference vol_beam, line for line): the ray's interval in the cone, then \
   equiangular steps over it; per candela per unit sigma_s */ \
"static float2 vol_beam_span(float3 o, float3 d, float3 A, float3 a, float c, float tmax) {\n" \
"  float3 w = o - A; float da = dot(d, a), wa = dot(w, a), c2 = c * c;\n" \
"  float qa = da * da - c2, qb = da * wa - c2 * dot(d, w), qc = wa * wa - c2 * dot(w, w);\n" \
"  float2 iv0 = float2(1e30, -1e30), iv1 = float2(1e30, -1e30);\n" \
"  if (abs(qa) < 1e-9) { if (abs(qb) < 1e-12) return float2(1, 0); iv0 = qb > 0.0 ? float2(-qc / (2.0 * qb), 1e30) : float2(-1e30, -qc / (2.0 * qb)); }\n" \
"  else { float disc = qb * qb - qa * qc;\n" \
"    if (disc < 0.0) { if (qa < 0.0) return float2(1, 0); iv0 = float2(-1e30, 1e30); }\n" \
"    else { float sq = sqrt(disc), r0 = (-qb - sq) / qa, r1 = (-qb + sq) / qa; float lo = min(r0, r1), hi = max(r0, r1);\n" \
"      if (qa < 0.0) iv0 = float2(lo, hi); else { iv0 = float2(-1e30, lo); iv1 = float2(hi, 1e30); } } }\n" \
"  float2 h = abs(da) < 1e-12 ? (wa < 0.0 ? float2(1, 0) : float2(-1e30, 1e30)) : da > 0.0 ? float2(-wa / da, 1e30) : float2(-1e30, -wa / da);\n" \
"  float2 s = float2(1e30, -1e30);\n" \
"  float l = max(max(iv0.x, h.x), 0.0), e = min(min(iv0.y, h.y), tmax); if (e > l) s = float2(min(s.x, l), max(s.y, e));\n" \
"  l = max(max(iv1.x, h.x), 0.0); e = min(min(iv1.y, h.y), tmax); if (e > l) s = float2(min(s.x, l), max(s.y, e));\n" \
"  return s; }\n" \
"static float vol_beam(float3 o, float3 d, float3 A, float3 a, float cout, float cin, float st, float g, float tmax, float j) {\n" \
"  float2 s = vol_beam_span(o, d, A, a, cout, tmax); if (s.y <= s.x) return 0.0;\n" \
"  float tc = dot(A - o, d); float D = max(length(o + d * tc - A), 0.05);\n" \
"  float th0 = atan((s.x - tc) / D), th1 = atan((s.y - tc) / D), dth = (th1 - th0) / 32.0, L = 0.0;\n" \
"  for (int i = 0; i < 32; i++) {\n" \
"    float th = th0 + dth * (float(i) + j), tn = tan(th), t = tc + D * tn;\n" \
"    float3 q = o + d * t - A; float r2 = dot(q, q), r = sqrt(r2);\n" \
"    if (r < 1e-6) continue; float ca = dot(q, a) / r; if (ca <= cout) continue;\n" \
"    L += smoothstep(cout, cin, ca) * vol_hg(g, -dot(q, d) / r) * exp(-st * (r + t)) / max(r2, 1.0) * D * (1.0 + tn * tn);\n" \
"  }\n" \
"  return L * dth; }\n" \
/* the beams' light toward the eye along a pixel's ray to t, pre-exposed */ \
"static float3 vol_beams(constant LU &u, constant VU &v, float3 eye, float3 d, float t, float2 px) {\n" \
"  float L = 0.0, j = fract(ign(px) + v.tgt.z);\n" \
"  for (int k = 0; k < 2; k++)\n" \
"    if (v.bpos[k].w > 0.0) L += v.bpos[k].w * vol_beam(eye, d, v.bpos[k].xyz, v.baxis[k].xyz, v.baxis[k].w, v.bcol.w, v.med.x, v.nse.w, t, j);\n" \
"  return v.bcol.rgb * (L * v.med.x * v.med.y * u.misc.x); }\n" \
/* the full-screen pass: blended (one, source alpha), colour * T + S; an empty pixel (the sky, the backdrop) at the grid's far side; \
   the beams' scattered light added over the whole ray (an empty pixel's to VOL_BEAM_SKY) */ \
"fragment float4 vol_apply(LO in [[stage_in]], constant LU &u [[buffer(0)]], constant VU &v [[buffer(1)]],\n" \
"                          texture2d<float> pos [[texture(0)]], texture3d<float> iv [[texture(1)]]) {\n" \
"  float3 P = pos.read(uint2(in.pos.xy)).xyz;\n" \
"  float4 r = vol_pix(v, iv, in.pos.xy, P, u.eye.xyz);\n" \
"  if (v.bpos[0].w > 0.0 || v.bpos[1].w > 0.0) {\n" \
"    bool sky = all(P == 0.0); float3 d = sky ? vol_ray(v, u.eye.xyz, in.pos.xy) : normalize(P - u.eye.xyz);\n" \
"    r.rgb += vol_beams(u, v, u.eye.xyz, d, sky ? 3000.0 : length(P - u.eye.xyz), in.pos.xy); }\n" \
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
/* this frame's lighthouse beams (light_world.c light_world_beam: their lamp,
 * axis, candela, inner and outer cosines, colour), at most VOL_BEAMS, for
 * the next brr_vol_frame's full-screen pass; n 0 for none */
#include "light.h"
void brr_vol_beams(const LightSpot *s, int n);
/* this frame's volume (its uniforms at buf, the integrated volume at tex)
 * for the forward pass's fsf */
void brr_vol_bind(id<MTLRenderCommandEncoder> e, NSUInteger buf, NSUInteger tex);
#endif

#endif
