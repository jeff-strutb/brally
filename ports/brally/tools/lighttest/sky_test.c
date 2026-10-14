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

/* the lens's glare taken out (light_sky.h 1b): a sky of 1 with stars of 40
   on one texel in 997, a halo of 10 / theta (degrees) from 0.3 to 45 degrees
   off the sun, the sun's disc; after the load every ring's median sky is the
   background's again, a star keeps its contrast over its ring, the disc's
   light is as without the halo, and the sky past 45 degrees is untouched */
static int cmpf(const void *a, const void *b) { float x = *(const float *)a, y = *(const float *)b; return x < y ? -1 : x > y; }
static float ringmed(const LightSky *s, const float c0[3], float a0, float a1, float *starmed)
{
    float *v = malloc((size_t)s->w * s->h * sizeof(float)), *st = malloc((size_t)s->w * s->h * sizeof(float)), m;
    size_t n = 0, ns = 0;
    for (int y = 0; y < s->h / 2; y++)
        for (int x = 0; x < s->w; x++) {
            float d[3];
            light_sky_dir(s, (float)x, (float)y, d);
            float a = acosf(fminf(d[0] * c0[0] + d[1] * c0[1] + d[2] * c0[2], 1.0f)) * 180.0f / PI_F;
            if (a < a0 || a >= a1 || d[2] <= 0.05f)
                continue;
            float l = lum(s->rgb + ((size_t)y * s->w + x) * 3);
            if ((((size_t)y * 4096u + (size_t)x * 7u) % 997u) == 0)   /* the star texels (the map was 4096 wide) */
                st[ns++] = l;
            else
                v[n++] = l;
        }
    qsort(v, n, sizeof(float), cmpf);
    m = n ? v[n / 2] : 0.0f;
    if (starmed) {
        qsort(st, ns, sizeof(float), cmpf);
        *starmed = ns ? st[ns / 2] : 0.0f;
    }
    free(v); free(st);
    return m;
}
static void glare(void)
{
    int w = 2048, h = 1024, x, y, c, pass;
    float sd[3], el = 30.0f * PI_F / 180.0f, az = 20.0f * PI_F / 180.0f, Lsun = 2e5f, lux[2] = { 0, 0 };
    LightSky s;
    char err[256];
    sd[0] = cosf(el) * cosf(az); sd[1] = cosf(el) * sinf(az); sd[2] = sinf(el);
    for (pass = 0; pass < 2; pass++) {   /* 0: no halo (the reference), 1: the halo and deglare */
        float *m = malloc((size_t)w * h * 3 * sizeof(float));
        LightSkyOpt o = { 0 };
        for (y = 0; y < h; y++) {
            float e2 = 0.5f * PI_F - PI_F * ((float)y + 0.5f) / (float)h;
            for (x = 0; x < w; x++) {
                float a = 2.0f * PI_F * ((float)x + 0.5f) / (float)w, d[3] = { cosf(e2) * cosf(a), cosf(e2) * sinf(a), sinf(e2) };
                float th = acosf(fminf(d[0] * sd[0] + d[1] * sd[1] + d[2] * sd[2], 1.0f)) * 180.0f / PI_F;
                float v = e2 > 0.0f ? 1.0f : 0.1f;
                if (e2 > 0.0f && (((size_t)y * 4096u + (size_t)x * 7u) % 997u) == 0)
                    v = 40.0f;
                if (pass && e2 > 0.0f && th > 0.3f && th < 45.0f)
                    v += 10.0f / th - 10.0f / 45.0f;
                if (th < 0.6f)
                    v = Lsun;
                for (c = 0; c < 3; c++)
                    m[((size_t)y * w + x) * 3 + c] = v;
            }
        }
        o.sun_lux = 1.0f;   /* scaled to 1 lux: the sky is compared as the scale leaves it */
        o.deglare = pass;
        CHECK(!light_sky_make(&s, m, w, h, &o, err, sizeof err), "glare make: %s", err);
        free(m);
        lux[pass] = lum(s.sun_lux) / s.scale;   /* the disc's light in the file's units */
        if (pass) {
            float worst = 0.0f, sworst = 0.0f, bg = ringmed(&s, s.sun, 50.0f, 60.0f, NULL);
            static const float r[][2] = { { 0.8f, 1.2f }, { 1.5f, 2.0f }, { 3.0f, 4.0f }, { 6.0f, 8.0f }, { 10.0f, 14.0f }, { 20.0f, 25.0f }, { 35.0f, 40.0f } };
            for (int i = 0; i < 7; i++) {
                float sm, md = ringmed(&s, s.sun, r[i][0], r[i][1], &sm);
                worst = fmaxf(worst, fabsf(md / bg - 1.0f));
                if (sm > 0.0f)
                    sworst = fmaxf(sworst, fabsf((sm / md) / 40.0f - 1.0f));
                printf("glare: ring %4.1f-%4.1f degrees: median %.4f of the background's (the halo was %.1f x), star / ring %.2f\n",
                       r[i][0], r[i][1], md / bg, 1.0f + 10.0f / (0.5f * (r[i][0] + r[i][1])), md > 0.0f ? sm / md : 0.0f);
            }
            CHECK(worst < 0.03f, "glare: a ring's median sky %.1f%% off the background's after the deglare", worst * 100.0f);
            CHECK(sworst < 0.08f, "glare: a star's contrast %.1f%% off", sworst * 100.0f);
            CHECK(fabsf(lux[1] / lux[0] - 1.0f) < 0.05f, "glare: the disc's light %.4g with the halo, %.4g without", lux[1], lux[0]);
            printf("glare: worst ring %.2f%%, star contrast %.2f%%, the disc's light with the halo / without %.4f\n", worst * 100.0f, sworst * 100.0f, lux[1] / lux[0]);
        }
        light_sky_free(&s);
    }
}

int main(int argc, char **argv)
{
    synthetic();
    glare();
    if (argc > 1)
        real(argv[1]);
    printf(s_fail ? "sky_test: %d FAILED\n" : "sky_test: all passed\n", s_fail);
    return s_fail != 0;
}
