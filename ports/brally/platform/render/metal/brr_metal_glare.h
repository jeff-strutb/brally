/* brr_metal_glare.h: the sun's (and at night the moon's) glare and lens flare
 * in the post pass (pfs, brr_metal_lit.h), from the light's known screen
 * position and energy rather than from the clipped HDR (the sky writes the
 * disc pre-exposed and at most 60000, over a few pixels).
 *
 * The disc's energy E, in pre-exposed pixel units (the sum of its pixels had
 * nothing been clipped): its illuminance at the camera (the sun's after the
 * air and the weather, LU.sunc; the moon's lux, KU.moon.x) times the sky's
 * scale and the exposure, over one pixel's solid angle at its position.
 *
 *   veiling glare  E times the lens's scattering fraction GLARE_SCATTER,
 *                  spread by a point-spread function that is a sum of four
 *                  normalised Gaussians (widths 2.5, 8, 24 and 64 thousandths
 *                  of the frame's height, weights after the shape of
 *                  Spencer et al. 1995's glare PSF: a bright core, a long
 *                  dim tail); it integrates to 1 over the plane
 *   flare          a halo (a ring about the light), five ghosts along the
 *                  line through the frame's centre (discs of decreasing size,
 *                  each channel's radius a little different: the lens's
 *                  colour fringes) and a starburst of 6 spikes at the light
 *                  (the aperture's hexagon), each a stated fraction of E
 *
 * all times the light's visibility: the fraction of 16 taps on a small disc
 * about it that the G-buffer leaves as sky (its alpha under 0.25), times the
 * cloud layer's transmittance toward it (brr_glare_cloud_T: a C copy of
 * brr_metal_clouds.h's density), times (in E) the air's.  Off the frame by
 * more than the glare's radius, or behind the camera, nothing.
 *
 * GLU (MSL) and BrrGlare (C) have the same layout: three float4s.
 *   sp  the light's position in the frame (0..1, rows down), the taps'
 *       radius as a fraction of the frame's height, 1 when on
 *   e   E (rgb) times the clouds' transmittance, per pixel of the lit 3D's
 *       internal size; w: the internal size's pixel count
 *   g   the frame's aspect (width / height), the starburst's angle, 1 for the
 *       moon (a hundredth of the veil, no halo, ghosts or starburst: the night's
 *       exposure made the full set a second sun), 0 */
#ifndef BRR_METAL_GLARE_H
#define BRR_METAL_GLARE_H

#define GLARE_SCATTER 0.04f     /* the lens's veiling glare: 4% of the light scattered */
#define GLARE_HALO 2e-4f
#define GLARE_GHOST 1.5e-4f
#define GLARE_STAR 6e-4f
#define GLARE_RADIUS 0.6f       /* the glare's reach, frame heights: past it off frame, none */

/* the PSF's Gaussians: widths (frame heights) and weights (sum 1) */
#define GLARE_PSF_SRC \
"constant float GPS_S[4] = { 0.0025, 0.008, 0.024, 0.064 };\n" \
"constant float GPS_W[4] = { 0.55, 0.25, 0.13, 0.07 };\n"

#define BRR_GLARE_SRC \
"struct GLU { float4 sp; float4 e; float4 g; };\n" \
GLARE_PSF_SRC \
/* the PSF per unit area (frame heights squared) at distance r frame heights */ \
"static float gpsf(float r) { float s = 0.0;\n" \
"  for (int i = 0; i < 4; i++) s += GPS_W[i] / (2.0 * 3.14159265 * GPS_S[i] * GPS_S[i]) * exp(-0.5 * r * r / (GPS_S[i] * GPS_S[i]));\n" \
"  return s; }\n" \
/* the light's visibility: the share of 16 taps (a centre, rings of 5 and 10) the G-buffer leaves as sky */ \
"static float gvis(constant GLU &gl, texture2d<float> alb) {\n" \
"  float n = 0.0, ar = gl.g.x; float2 sz = float2(alb.get_width(), alb.get_height());\n" \
"  for (int i = 0; i < 16; i++) {\n" \
"    float rr = i == 0 ? 0.0 : (i < 6 ? 0.5 : 1.0), a = (i < 6 ? float(i) / 5.0 : float(i - 6) / 10.0 + 0.05) * 6.2831853;\n" \
"    float2 q = gl.sp.xy + float2(cos(a) / ar, sin(a)) * rr * gl.sp.z;\n" \
"    if (any(q < 0.0) || any(q >= 1.0)) { n += 1.0; continue; }   /* off the frame: nothing in front of it */\n" \
"    n += alb.read(uint2(q * sz)).a < 0.25 ? 1.0 : 0.0; }\n" \
"  return n / 16.0; }\n" \
/* the glare and flare at uv, pre-exposed radiance: the energy over one \
   internal pixel's area (frame heights squared: aspect / count) times each \
   term's density per unit area, whatever hdr's size */ \
"static float3 glare(constant GLU &gl, float2 uv, texture2d<float> alb) {\n" \
"  if (gl.sp.w < 0.5) return 0.0;\n" \
"  float v = gvis(gl, alb); if (v <= 0.0) return 0.0;\n" \
"  float ar = gl.g.x, ph = ar / max(gl.e.w, 1.0);   /* one internal pixel's area, frame heights squared */\n" \
"  float3 E = gl.e.rgb * v;\n" \
"  float2 d = (uv - gl.sp.xy) * float2(ar, 1.0); float r = length(d);\n" \
"  float3 L = E * 0.04 * gpsf(r) * (gl.g.z > 0.5 ? 0.01 : 1.0);   /* GLARE_SCATTER; the moon a glow only */\n" \
"  if (gl.g.z < 0.5) { float rh = 0.22, w = 0.012; L += E * 2e-4 * float3(exp(-0.5 * pow((r - rh * 1.02) / w, 2.0)), exp(-0.5 * pow((r - rh) / w, 2.0)), exp(-0.5 * pow((r - rh * 0.98) / w, 2.0))) / (2.0 * 3.14159265 * rh * w * 2.5066283); }\n" \
"  if (gl.g.z < 0.5) { float2 c = float2(0.5, 0.5), ax = (c - gl.sp.xy);\n" \
"    const float gt[5] = { 0.5, 0.8, 1.25, 1.6, 2.1 }, gr[5] = { 0.05, 0.02, 0.08, 0.035, 0.12 };\n" \
"    const float3 gc[5] = { float3(1.0, 0.7, 0.4), float3(0.5, 0.8, 1.0), float3(0.6, 1.0, 0.6), float3(1.0, 0.5, 0.8), float3(0.4, 0.6, 1.0) };\n" \
"    for (int i = 0; i < 5; i++) { float2 gp = (gl.sp.xy + ax * gt[i]); float rg = length((uv - gp) * float2(ar, 1.0)), R = gr[i];\n" \
"      float3 m = float3(smoothstep(R * 1.04, R * 0.9, rg), smoothstep(R, R * 0.86, rg), smoothstep(R * 0.96, R * 0.82, rg));\n" \
"      L += E * 1.5e-4 * gc[i] * m / (3.14159265 * R * R); } }\n" \
"  if (gl.g.z < 0.5) { float a = atan2(d.y, d.x) - gl.g.y, s = pow(abs(cos(3.0 * a)), 400.0), r0 = 0.004;\n" \
"    L += E * 6e-4 * s / (2.0 * 3.14159265 * r0 * r0 * 0.0886) * r0 * r0 / (r * r + r0 * r0) * exp(-r / 0.3); }\n" \
"  return L * ph; }\n"

#ifndef __METAL_VERSION__
#include <math.h>

typedef struct BrrGlare { float sp[4], e[4], g[4]; } BrrGlare;

static const float brr_gps_s[4] = { 0.0025f, 0.008f, 0.024f, 0.064f };
static const float brr_gps_w[4] = { 0.55f, 0.25f, 0.13f, 0.07f };

/* the PSF per unit area (frame heights squared), as gpsf */
static inline double brr_glare_psf(double r)
{
    double s = 0;
    int i;
    for (i = 0; i < 4; i++)
        s += brr_gps_w[i] / (2.0 * 3.14159265358979 * brr_gps_s[i] * brr_gps_s[i]) * exp(-0.5 * r * r / ((double)brr_gps_s[i] * brr_gps_s[i]));
    return s;
}

/* the visibility from the 16 taps' coverage (1: scenery in front), as gvis */
static inline float brr_glare_vis(const unsigned char cover[16])
{
    int i, n = 0;
    for (i = 0; i < 16; i++)
        n += !cover[i];
    return n / 16.0f;
}

/* the C copy of brr_metal_clouds.h's density (vhash, vnoise, cfbm, cdens) */
static inline float brr_fract(float x) { return x - floorf(x); }
static inline float brr_vhash(float px, float py)
{
    float d;
    px = brr_fract(px * 0.1031f), py = brr_fract(py * 0.1030f);
    d = px * (py + 33.33f) + py * (px + 33.33f);
    px += d, py += d;
    return brr_fract((px + py) * px);
}
static inline float brr_vnoise(float px, float py)
{
    float ix = floorf(px), iy = floorf(py), fx = px - ix, fy = py - iy, ux = fx * fx * (3 - 2 * fx), uy = fy * fy * (3 - 2 * fy);
    float a = brr_vhash(ix, iy), b = brr_vhash(ix + 1, iy), c = brr_vhash(ix, iy + 1), d = brr_vhash(ix + 1, iy + 1);
    float ab = a + (b - a) * ux, cd = c + (d - c) * ux;
    return ab + (cd - ab) * uy;
}
static inline float brr_cdens(float x, float y, float cover)
{
    float a = 0.5f, s = 0, n, t, px = x * 0.4f, py = y * 0.4f;
    int i;
    for (i = 0; i < 5; i++) {
        s += a * brr_vnoise(px, py);
        px = px * 2 + 17.1f, py = py * 2 + 5.3f, a *= 0.5f;
    }
    n = s / 0.96875f;
    t = (n - (1 - cover)) / fmaxf(cover, 0.05f) * 2;
    return t < 0 ? 0 : t > 1 ? 1 : t;
}
/* the cloud layer's transmittance toward the light d (z up), seen from h km;
   cld as KU.cld (cover, extinction per metre, clock, tier flag), as clouds()'s alpha */
static inline float brr_glare_cloud_T(const float d[3], float h, const float cld[4])
{
    float t0, L, px, py, tau, T, f, x;
    if (cld[3] < 0.5f || d[2] < 0.005f || h > 1.8f)
        return 1;
    t0 = (1.8f - h) / d[2], L = fminf(0.6f / d[2], 30.0f);
    px = d[0] * (t0 + 0.5f * L) + 0.012f * cld[2], py = d[1] * (t0 + 0.5f * L) + 0.005f * cld[2];
    tau = cld[1] * 1000 * brr_cdens(px, py, cld[0]) * L, T = expf(-tau);
    x = (d[2] - 0.005f) / (0.087f - 0.005f);
    x = x < 0 ? 0 : x > 1 ? 1 : x;
    f = x * x * (3 - 2 * x);
    return 1 + (T - 1) * f;
}

/* the sky's camera ray through pixel (x, y) of the lit 3D, as skyfs's camray;
   k: KU's twelve float4s in order (xft vpt m0 m1 m2 off eye tgt ...) */
static inline void brr_glare_ray(const float k[][4], double x, double y, double d[3])
{
    const float *xft = k[0], *vpt = k[1], *off = k[5], *eye = k[6], *tgt = k[7];
    double nx = (x + 0.5) / tgt[0] * 2 - 1, ny = 1 - (y + 0.5) / tgt[1] * 2, gx, gy, c[3], l;
    int i;
    gx = (nx - xft[1]) / xft[0], gy = (ny - xft[3]) / xft[2];
    if (tgt[3] > 0)
        gy = tgt[3] - gy;
    c[0] = (gx - vpt[1]) / vpt[0] - off[0], c[1] = (gy - vpt[3]) / vpt[2] - off[1], c[2] = 1 - off[2];
    for (i = 0; i < 3; i++)
        d[i] = k[2 + i][0] * c[0] + k[2 + i][1] * c[1] + k[2 + i][2] * c[2] - eye[i];
    l = sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    for (i = 0; i < 3; i++)
        d[i] /= l;
}

/* the light s (unit, toward it) into the lit 3D's pixels: 0 when behind the
   camera; *omega the solid angle of one pixel there */
static inline int brr_glare_project(const float k[][4], const float s[3], double px[2], double *omega)
{
    const float *xft = k[0], *vpt = k[1], *off = k[5], *eye = k[6], *tgt = k[7];
    double Mm[3][3], Mi[3][3], det, a[3], b[3], t, c0, c1, gx, gy, nx, ny, d0[3], dx[3], dy[3];
    int r, c;
    for (r = 0; r < 3; r++)
        for (c = 0; c < 3; c++)
            Mm[r][c] = k[2 + r][c];
    det = Mm[0][0] * (Mm[1][1] * Mm[2][2] - Mm[1][2] * Mm[2][1]) - Mm[0][1] * (Mm[1][0] * Mm[2][2] - Mm[1][2] * Mm[2][0])
        + Mm[0][2] * (Mm[1][0] * Mm[2][1] - Mm[1][1] * Mm[2][0]);
    if (fabs(det) < 1e-30)
        return 0;
    Mi[0][0] = (Mm[1][1] * Mm[2][2] - Mm[1][2] * Mm[2][1]) / det;
    Mi[0][1] = (Mm[0][2] * Mm[2][1] - Mm[0][1] * Mm[2][2]) / det;
    Mi[0][2] = (Mm[0][1] * Mm[1][2] - Mm[0][2] * Mm[1][1]) / det;
    Mi[1][0] = (Mm[1][2] * Mm[2][0] - Mm[1][0] * Mm[2][2]) / det;
    Mi[1][1] = (Mm[0][0] * Mm[2][2] - Mm[0][2] * Mm[2][0]) / det;
    Mi[1][2] = (Mm[0][2] * Mm[1][0] - Mm[0][0] * Mm[1][2]) / det;
    Mi[2][0] = (Mm[1][0] * Mm[2][1] - Mm[1][1] * Mm[2][0]) / det;
    Mi[2][1] = (Mm[0][1] * Mm[2][0] - Mm[0][0] * Mm[2][1]) / det;
    Mi[2][2] = (Mm[0][0] * Mm[1][1] - Mm[0][1] * Mm[1][0]) / det;
    /* c = Mi (eye + t s): its z fixed at 1 - off.z */
    for (r = 0; r < 3; r++) {
        a[r] = Mi[r][0] * eye[0] + Mi[r][1] * eye[1] + Mi[r][2] * eye[2];
        b[r] = Mi[r][0] * s[0] + Mi[r][1] * s[1] + Mi[r][2] * s[2];
    }
    if (fabs(b[2]) < 1e-30)
        return 0;
    t = (1 - off[2] - a[2]) / b[2];
    if (t <= 0)
        return 0;
    c0 = a[0] + t * b[0], c1 = a[1] + t * b[1];
    gx = (c0 + off[0]) * vpt[0] + vpt[1], gy = (c1 + off[1]) * vpt[2] + vpt[3];
    if (tgt[3] > 0)
        gy = tgt[3] - gy;
    nx = gx * xft[0] + xft[1], ny = gy * xft[2] + xft[3];
    px[0] = (nx + 1) * 0.5 * tgt[0] - 0.5, px[1] = (1 - ny) * 0.5 * tgt[1] - 0.5;
    brr_glare_ray(k, px[0], px[1], d0);
    brr_glare_ray(k, px[0] + 1, px[1], dx);
    brr_glare_ray(k, px[0], px[1] + 1, dy);
    {
        double ax = acos(fmin(1, d0[0] * dx[0] + d0[1] * dx[1] + d0[2] * dx[2]));
        double ay = acos(fmin(1, d0[0] * dy[0] + d0[1] * dy[1] + d0[2] * dy[2]));
        *omega = ax * ay;
    }
    return 1;
}

/* the frame's glare constants: k the sky's KU (as floats), illum the disc's
   illuminance at the camera (rgb, the units the sky draws it in, before
   ktop.z and the exposure), skyk and expo the sky's scale and exposure */
static inline BrrGlare brr_glare_setup(const float k[][4], const float s[3], const float illum[3], float skyk, float expo)
{
    BrrGlare g;
    double px[2], om, W = k[7][0], H = k[7][1], T;
    int i;
    for (i = 0; i < 4; i++)
        g.sp[i] = g.e[i] = g.g[i] = 0;
    if (W < 1 || H < 1 || !brr_glare_project(k, s, px, &om) || om <= 0)
        return g;
    g.sp[0] = (float)((px[0] + 0.5) / W), g.sp[1] = (float)((px[1] + 0.5) / H);
    if (g.sp[0] < -GLARE_RADIUS * H / W || g.sp[0] > 1 + GLARE_RADIUS * H / W || g.sp[1] < -GLARE_RADIUS || g.sp[1] > 1 + GLARE_RADIUS)
        return g;
    g.sp[2] = (float)fmax(3.0 / H, 0.01);   /* the taps: a hundredth of the frame's height, or three pixels */
    g.sp[3] = 1;
    T = brr_glare_cloud_T(s, k[6][3], k[10]);
    for (i = 0; i < 3; i++)
        g.e[i] = (float)(illum[i] * skyk * expo / om * T);
    g.e[3] = (float)(W * H);
    g.g[0] = (float)(W / H);
    g.g[1] = 0.2617994f;   /* the starburst's angle: 15 degrees */
    return g;
}
#endif

#endif
