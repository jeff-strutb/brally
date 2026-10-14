/* sky_test.c: the skymap's sun, calibration and irradiance (light_sky.c)
 * against exact answers on a synthetic sky and brute-force integration on a
 * real map.  sky.sh builds and runs it; the real map is optional (argv[1]). */
#include "light_sky.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PI_F 3.14159265358979f

static int s_fail;
#define CHECK(c, ...) do { if (!(c)) { s_fail++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static float lum(const float *c) { return 0.2126f * c[0] + 0.7152f * c[1] + 0.0722f * c[2]; }

/* the cosine-weighted integral of the map about n, texel by texel */
static void brute_irr(const LightSky *s, const float n[3], double e[3])
{
    int x, y, c;
    e[0] = e[1] = e[2] = 0.0;
    for (y = 0; y < s->h; y++) {
        float el = 0.5f * PI_F - PI_F * ((float)y + 0.5f) / (float)s->h;
        double om = (2.0 * M_PI / s->w) * (M_PI / s->h) * cos(el);
        for (x = 0; x < s->w; x++) {
            float d[3], k;
            light_sky_dir(s, (float)x, (float)y, d);
            k = d[0] * n[0] + d[1] * n[1] + d[2] * n[2];
            if (k <= 0.0f)
                continue;
            for (c = 0; c < 3; c++)
                e[c] += s->rgb[((size_t)y * s->w + x) * 3 + c] * k * om;
        }
    }
}

static void synthetic(void)
{
    /* a sky of radiance 1 (white), a ground of 0.1, a sun of radiance 2e6 over
       a disc 0.6 degrees across at elevation 40, bearing 70 degrees */
    int w = 4096, h = 2048, x, y, c;
    float *m = malloc((size_t)w * h * 3 * sizeof(float)), sd[3], el = 40.0f * PI_F / 180.0f, az = 70.0f * PI_F / 180.0f, Lsun = 2e6f;
    double esun = 0.0;
    LightSky s;
    LightSkyOpt o = { 0 };
    char err[256];
    sd[0] = cosf(el) * cosf(az);
    sd[1] = cosf(el) * sinf(az);
    sd[2] = sinf(el);
    for (y = 0; y < h; y++) {
        float e2 = 0.5f * PI_F - PI_F * ((float)y + 0.5f) / (float)h;
        double om = (2.0 * M_PI / w) * (M_PI / h) * cos(e2);
        for (x = 0; x < w; x++) {
            float a = 2.0f * PI_F * ((float)x + 0.5f) / (float)w, d[3] = { cosf(e2) * cosf(a), cosf(e2) * sinf(a), sinf(e2) };
            float v = e2 > 0.0f ? 1.0f : 0.1f;
            if (d[0] * sd[0] + d[1] * sd[1] + d[2] * sd[2] > cosf(0.3f * PI_F / 180.0f)) {
                v = Lsun;
                esun += (Lsun - 1.0) * om;
            }
            for (c = 0; c < 3; c++)
                m[((size_t)y * w + x) * 3 + c] = v;
        }
    }
    o.sun_lux = (float)esun;      /* scale 1: the map's own units */
    CHECK(!light_sky_make(&s, m, w, h, &o, err, sizeof err), "synthetic make: %s", err);
    free(m);
    printf("synthetic: sun %d, lux %.6g (set %.6g), dir %.4f %.4f %.4f (true %.4f %.4f %.4f), scale %.6f, sky_up %.5f (pi %.5f)\n", s.has_sun,
           s.sun_lux[1], esun, s.sun[0], s.sun[1], s.sun[2], sd[0], sd[1], sd[2], s.scale, s.sky_up[1], PI_F);
    CHECK(s.has_sun, "synthetic: no sun found");
    CHECK(fabsf(s.scale - 1.0f) < 1e-3f, "synthetic: the sun's integral %.6g, set %.6g", s.sun_lux[1] / s.scale, esun);
    CHECK(s.sun[0] * sd[0] + s.sun[1] * sd[1] + s.sun[2] * sd[2] > cosf(0.05f * PI_F / 180.0f), "synthetic: sun direction off by over 0.05 degrees");
    CHECK(fabsf(s.sky_up[1] - PI_F) < 2e-3f * PI_F, "synthetic: the sky on level ground %.5f, pi expected (the disc painted out)", s.sky_up[1]);
    {   /* the ground: 0.2 / pi of the sun's and sky's light on level ground */
        float eg = 0.2f / PI_F * ((float)esun * sd[2] + PI_F);
        CHECK(fabsf(s.ground[1] - eg) < 1e-3f * eg, "synthetic: ground %.5f, %.5f expected", s.ground[1], eg);
    }
    {   /* the harmonics: a uniform sky over a uniform ground, exact up to the order-2 cut (the step at the horizon) */
        float up[3] = { 0, 0, 1 }, dn[3] = { 0, 0, -1 }, sd2[3] = { 1, 0, 0 }, e[3];
        double b[3];
        light_sky_irr(&s, up, e);
        brute_irr(&s, up, b);
        printf("synthetic: irradiance up %.4f (brute %.4f), ", e[1], b[1]);
        CHECK(fabs(e[1] - b[1]) < 0.03 * b[1], "synthetic: up %.4f vs %.4f", e[1], b[1]);
        light_sky_irr(&s, dn, e);
        brute_irr(&s, dn, b);
        printf("down %.4f (brute %.4f), ", e[1], b[1]);
        CHECK(fabs(e[1] - b[1]) < 0.03 * b[0] + 0.03, "synthetic: down %.4f vs %.4f", e[1], b[1]);
        light_sky_irr(&s, sd2, e);
        brute_irr(&s, sd2, b);
        printf("side %.4f (brute %.4f)\n", e[1], b[1]);
        CHECK(fabs(e[1] - b[1]) < 0.03 * b[1], "synthetic: side %.4f vs %.4f", e[1], b[1]);
    }
    light_sky_free(&s);
}

/* the real map: its sun, scale and the harmonics' error over many normals */
static void real(const char *path)
{
    LightSky s;
    LightSkyOpt o = { 0 };
    char err[256];
    int i;
    double worst = 0.0, sum = 0.0;
    o.has_sun_az = 1;
    o.sun_az = atan2f(-1.5f, -1.0f);
    if (light_sky_load(&s, path, &o, err, sizeof err)) {
        CHECK(0, "real: %s", err);
        return;
    }
    printf("real: %dx%d, sun %d at %.4f %.4f %.4f (elevation %.1f deg, bearing %.1f deg), %.0f %.0f %.0f lux facing it over %.3g sr,\n"
           "      sky on level ground %.0f %.0f %.0f lux, sun/sky %.2f, scale %.4g, ground %.0f cd/m2\n",
           s.w, s.h, s.has_sun, s.sun[0], s.sun[1], s.sun[2], asinf(s.sun[2]) * 180.0f / PI_F, atan2f(s.sun[1], s.sun[0]) * 180.0f / PI_F,
           s.sun_lux[0], s.sun_lux[1], s.sun_lux[2], s.sun_omega, s.sky_up[0], s.sky_up[1], s.sky_up[2], s.clip_ratio, s.scale, lum(s.ground));
    CHECK(s.has_sun, "real: no sun found");
    CHECK(fabsf(atan2f(s.sun[1], s.sun[0]) - o.sun_az) < 1e-3f, "real: the sun not turned to its bearing");
    srand(7);
    for (i = 0; i < 64; i++) {
        float n[3], l, e[3];
        double b[3], er;
        do {
            n[0] = 2.0f * rand() / (float)RAND_MAX - 1.0f;
            n[1] = 2.0f * rand() / (float)RAND_MAX - 1.0f;
            n[2] = 2.0f * rand() / (float)RAND_MAX - 1.0f;
            l = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        } while (l < 0.1f || l > 1.0f);
        n[0] /= l; n[1] /= l; n[2] /= l;
        light_sky_irr(&s, n, e);
        brute_irr(&s, n, b);
        er = fabs(lum(e) - (0.2126 * b[0] + 0.7152 * b[1] + 0.0722 * b[2])) / lum(s.sky_up);
        sum += er;
        if (er > worst)
            worst = er;
    }
    printf("real: harmonics against brute force over 64 normals: mean error %.2f%%, worst %.2f%% of the sky's light on level ground\n",
           100.0 * sum / 64.0, 100.0 * worst);
    CHECK(worst < 0.05, "real: the harmonics' worst error %.2f%% (over 5%%)", 100.0 * worst);
    light_sky_free(&s);
}

int main(int argc, char **argv)
{
    synthetic();
    if (argc > 1)
        real(argv[1]);
    printf(s_fail ? "sky_test: %d FAILED\n" : "sky_test: all passed\n", s_fail);
    return s_fail != 0;
}
