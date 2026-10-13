/* ref.h: CPU double-precision references for the verification suite
 * (lighttest.m).  Written from the published sources, not from the shaders:
 *
 *   PBR Neutral    the Khronos reference implementation (KhronosGroup/
 *                  ToneMapping, PBR_Neutral/pbrNeutral.glsl), ported to C
 *   atmosphere     Earth's constants as Hillaire 2020 publishes them
 *                  (EGSR, "A Scalable and Production Ready Sky and Atmosphere
 *                  Rendering Technique", section 4 / table 1, the same as
 *                  Bruneton's 2017 demo): Rayleigh scattering (5.802, 13.558,
 *                  33.1) x 1e-3 /km, scale height 8 km; Mie scattering 3.996e-3
 *                  and absorption 4.4e-3 /km, scale height 1.2 km; ozone
 *                  absorption (0.650, 1.881, 0.085) x 1e-3 /km in a tent 30 km
 *                  wide centred at 25 km; ground 6360 km, top 6460 km, ground
 *                  albedo 0.3; Rayleigh and Cornette-Shanks (g 0.8) phases;
 *                  the multiple-scattering term of Hillaire section 5.5
 *   sRGB           IEC 61966-2-1 */
#ifndef LIGHTTEST_REF_H
#define LIGHTTEST_REF_H

#include <math.h>

#define R_PI 3.14159265358979323846

/* ---- sRGB --------------------------------------------------------------------- */
static double ref_lin(double c) { return c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4); }
static double ref_srgb(double x) { return x <= 0.0031308 ? 12.92 * x : 1.055 * pow(x, 1.0 / 2.4) - 0.055; }

/* ---- Khronos PBR Neutral -------------------------------------------------------- */
static void ref_neutral(const double in[3], double out[3])
{
    const double startCompression = 0.8 - 0.04, desaturation = 0.15;
    double c[3] = { in[0], in[1], in[2] }, x, offset, peak, d, newPeak, g;
    int k;
    x = fmin(c[0], fmin(c[1], c[2]));
    offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
    for (k = 0; k < 3; k++)
        c[k] -= offset;
    peak = fmax(c[0], fmax(c[1], c[2]));
    if (peak < startCompression) {
        for (k = 0; k < 3; k++)
            out[k] = c[k];
        return;
    }
    d = 1.0 - startCompression;
    newPeak = 1.0 - d * d / (peak + d - startCompression);
    for (k = 0; k < 3; k++)
        c[k] *= newPeak / peak;
    g = 1.0 - 1.0 / (desaturation * (peak - newPeak) + 1.0);
    for (k = 0; k < 3; k++)
        out[k] = c[k] * (1.0 - g) + newPeak * g;
}

/* ---- atmosphere ------------------------------------------------------------------ */
#define A_RG 6360.0
#define A_RT 6460.0
static const double A_RAY[3] = { 5.802e-3, 13.558e-3, 33.1e-3 };
static const double A_MIES = 3.996e-3, A_MIEA = 4.4e-3;
static const double A_OZ[3] = { 0.650e-3, 1.881e-3, 0.085e-3 };
#define A_HR 8.0
#define A_HM 1.2
#define A_ALB 0.3

/* extinction, Rayleigh scattering and Mie scattering at height h (km) */
static void a_medium(double h, double mie, double ext[3], double sr[3], double *sm)
{
    double r = exp(-h / A_HR), m = exp(-h / A_HM) * mie, oz = fmax(0.0, 1.0 - fabs(h - 25.0) / 15.0);
    int k;
    for (k = 0; k < 3; k++) {
        sr[k] = A_RAY[k] * r;
        ext[k] = A_RAY[k] * r + (A_MIES + A_MIEA) * m + A_OZ[k] * oz;
    }
    *sm = A_MIES * m;
}

/* distance from radius r along mu to the sphere R (-1: none) */
static double a_sphere(double r, double mu, double R)
{
    double d = r * r * (mu * mu - 1.0) + R * R;
    return d < 0.0 ? -1.0 : -r * mu + sqrt(d);
}
static double a_ground(double r, double mu)
{
    double d = r * r * (mu * mu - 1.0) + A_RG * A_RG;
    return d < 0.0 || mu > 0.0 ? -1.0 : -r * mu - sqrt(d);
}

/* optical depth from (r, mu) to the top of the atmosphere, Simpson's rule */
static void a_tau(double r, double mu, double mie, double tau[3], int n)
{
    double len = fmax(a_sphere(r, mu, A_RT), 0.0), h = len / n, e[3], sr[3], sm;
    int i, k;
    tau[0] = tau[1] = tau[2] = 0.0;
    if (n & 1)
        n++;
    h = len / n;
    for (i = 0; i <= n; i++) {
        double t = i * h, w = i == 0 || i == n ? 1.0 : (i & 1) ? 4.0 : 2.0;
        a_medium(sqrt(r * r + t * t + 2.0 * r * mu * t) - A_RG, mie, e, sr, &sm);
        for (k = 0; k < 3; k++)
            tau[k] += w * e[k];
    }
    for (k = 0; k < 3; k++)
        tau[k] *= h / 3.0;
}

/* the zenith optical depth from the ground, in closed form */
static void a_tau_zenith(double mie, double tau[3])
{
    int k;
    for (k = 0; k < 3; k++)
        tau[k] = A_RAY[k] * A_HR * (1.0 - exp(-(A_RT - A_RG) / A_HR))
               + (A_MIES + A_MIEA) * mie * A_HM * (1.0 - exp(-(A_RT - A_RG) / A_HM))
               + A_OZ[k] * 15.0;       /* the tent's area: height 1, half width 15 km */
}

static double a_rayleigh(double c) { return 3.0 / (16.0 * R_PI) * (1.0 + c * c); }
static double a_mie(double c)
{
    double g = 0.8, k = 3.0 / (8.0 * R_PI) * (1.0 - g * g) / (2.0 + g * g);
    return k * (1.0 + c * c) / pow(1.0 + g * g - 2.0 * g * c, 1.5);
}

#endif
