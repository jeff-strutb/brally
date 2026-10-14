/* light_sky.c: the sky as a captured HDR map (light_sky.h). */
#include "light_sky.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PI_F 3.14159265358979f
#define DEG  (PI_F / 180.0f)

static float lum(const float *c) { return 0.2126f * c[0] + 0.7152f * c[1] + 0.0722f * c[2]; }

static void seterr(char *err, int nerr, const char *m)
{
    if (err && nerr > 0)
        snprintf(err, (size_t)nerr, "%s", m);
}

/* ---- Radiance RGBE ------------------------------------------------------- */

static void rgbe(const unsigned char *p, float *o)
{
    if (p[3] == 0) {
        o[0] = o[1] = o[2] = 0.0f;
        return;
    }
    float f = ldexpf(1.0f, (int)p[3] - (128 + 8));
    o[0] = ((float)p[0] + 0.5f) * f;
    o[1] = ((float)p[1] + 0.5f) * f;
    o[2] = ((float)p[2] + 0.5f) * f;
}

/* one scanline of w RGBE texels from the file's bytes at *pp (end e) into
 * line (w x 4): new-style run length (2, 2, w high, w low, then each
 * channel's runs) or flat */
static int scanline(const unsigned char **pp, const unsigned char *e, unsigned char *line, int w)
{
    const unsigned char *p = *pp;
    int c, x, n;
    if (e - p < 4)
        return -1;
    if (w < 8 || w > 32767 || p[0] != 2 || p[1] != 2 || (p[2] & 0x80)) {
        if (e - p < (long)w * 4)
            return -1;
        memcpy(line, p, (size_t)w * 4);
        *pp = p + (size_t)w * 4;
        return 0;
    }
    if (((int)p[2] << 8 | p[3]) != w)
        return -1;
    p += 4;
    for (c = 0; c < 4; c++)
        for (x = 0; x < w;) {
            if (p >= e)
                return -1;
            n = *p++;
            if (n > 128) {
                n -= 128;
                if (p >= e || x + n > w)
                    return -1;
                while (n--)
                    line[4 * x++ + c] = *p;
                p++;
            } else {
                if (n == 0 || x + n > w || e - p < n)
                    return -1;
                while (n--)
                    line[4 * x++ + c] = *p++;
            }
        }
    *pp = p;
    return 0;
}

/* the file read whole, then decoded */
int light_hdr_read(const char *path, float **rgb, int *w, int *h)
{
    FILE *f = fopen(path, "rb");
    unsigned char *buf, *line;
    const unsigned char *p, *e;
    long sz;
    int fmt = 0, x, y, W = 0, H = 0;
    float *o;
    *rgb = NULL;
    if (!f)
        return -1;
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = sz > 16 ? malloc((size_t)sz + 1) : NULL;
    if (!buf || fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf);
        fclose(f);
        return -1;
    }
    fclose(f);
    buf[sz] = 0;
    p = buf;
    e = buf + sz;
    if (strncmp((const char *)p, "#?", 2)) {
        free(buf);
        return -1;
    }
    for (;;) {   /* the header's lines, to the blank one */
        const unsigned char *nl = memchr(p, '\n', (size_t)(e - p));
        if (!nl) {
            free(buf);
            return -1;
        }
        if (nl == p)
            break;
        if (!strncmp((const char *)p, "FORMAT=32-bit_rle_rgbe", 22))
            fmt = 1;
        p = nl + 1;
    }
    p++;
    if (!fmt || sscanf((const char *)p, "-Y %d +X %d", &H, &W) != 2 || W <= 0 || H <= 0 || W > 65536 || H > 65536) {
        free(buf);
        return -1;
    }
    p = memchr(p, '\n', (size_t)(e - p));
    if (!p) {
        free(buf);
        return -1;
    }
    p++;
    line = malloc((size_t)W * 4);
    o = malloc((size_t)W * (size_t)H * 3 * sizeof(float));
    if (!line || !o) {
        free(line);
        free(o);
        free(buf);
        return -1;
    }
    for (y = 0; y < H; y++) {
        if (scanline(&p, e, line, W)) {
            free(line);
            free(o);
            free(buf);
            return -1;
        }
        for (x = 0; x < W; x++)
            rgbe(line + 4 * x, o + ((size_t)y * W + x) * 3);
    }
    free(line);
    free(buf);
    *rgb = o;
    *w = W;
    *h = H;
    return 0;
}

/* ---- directions and harmonics -------------------------------------------- */

void light_sky_dir(const LightSky *s, float x, float y, float d[3])
{
    float el = 0.5f * PI_F - PI_F * (y + 0.5f) / (float)s->h, az = 2.0f * PI_F * (x + 0.5f) / (float)s->w + s->rot;
    d[0] = cosf(el) * cosf(az);
    d[1] = cosf(el) * sinf(az);
    d[2] = sinf(el);
}

void light_sky_sh9(const float d[3], float y[9])
{
    float x = d[0], yy = d[1], z = d[2];
    y[0] = 0.282095f;
    y[1] = 0.488603f * yy;
    y[2] = 0.488603f * z;
    y[3] = 0.488603f * x;
    y[4] = 1.092548f * x * yy;
    y[5] = 1.092548f * yy * z;
    y[6] = 0.315392f * (3.0f * z * z - 1.0f);
    y[7] = 1.092548f * x * z;
    y[8] = 0.546274f * (x * x - yy * yy);
}

void light_sky_irr(const LightSky *s, const float n[3], float e[3])
{
    float y[9];
    int i, c;
    light_sky_sh9(n, y);
    for (c = 0; c < 3; c++) {
        e[c] = 0.0f;
        for (i = 0; i < 9; i++)
            e[c] += s->sh[i][c] * y[i];
        if (e[c] < 0.0f)
            e[c] = 0.0f;
    }
}

/* a texel's solid angle (row y of h) */
static float texel_omega(int w, int h, int y)
{
    float el = 0.5f * PI_F - PI_F * ((float)y + 0.5f) / (float)h;
    return (2.0f * PI_F / (float)w) * (PI_F / (float)h) * cosf(el);
}

static int cmpf(const void *a, const void *b)
{
    float x = *(const float *)a, y = *(const float *)b;
    return x < y ? -1 : x > y;
}

/* the texels within ang radians of unit direction c: their rows and, per
 * row, the columns (the bearing window widens toward the poles) */
#define FOR_NEAR(s, c, ang, X, Y, D)                                                                \
    for (int Y = 0; Y < (s)->h; Y++) {                                                              \
        float el_ = 0.5f * PI_F - PI_F * ((float)Y + 0.5f) / (float)(s)->h;                         \
        float cel_ = asinf(fmaxf(fminf((c)[2], 1.0f), -1.0f));                                      \
        if (fabsf(el_ - cel_) > (ang) + PI_F / (float)(s)->h) continue;                             \
        for (int X = 0; X < (s)->w; X++) {                                                          \
            float D[3];                                                                             \
            light_sky_dir((s), (float)X, (float)Y, D);                                              \
            if (D[0] * (c)[0] + D[1] * (c)[1] + D[2] * (c)[2] < cosf(ang)) continue;

#define END_NEAR }}

/* light_sky_make on radiance it takes over (rgb, malloc'd: freed with the sky, or on failure) */
static int sky_make_owned(LightSky *s, float *rgb, int w, int h, const LightSkyOpt *opt, char *err, int nerr)
{
    LightSkyOpt o = opt ? *opt : (LightSkyOpt){ 0 };
    size_t n = (size_t)w * (size_t)h;
    int x, y, c, i, bx = -1, by = -1;
    float best = -1.0f, sc;
    memset(s, 0, sizeof *s);
    if (w < 16 || h < 8 || w != 2 * h) {
        free(rgb);
        seterr(err, nerr, "skymap: not an equirectangular map (width twice the height, at least 16 x 8)");
        return -1;
    }
    if (!o.sun_lux)
        o.sun_lux = 100000.0f;
    if (!o.horiz_lux)
        o.horiz_lux = 15000.0f;
    if (!o.ground_albedo)
        o.ground_albedo = 0.2f;
    s->w = w;
    s->h = h;
    s->rgb = rgb;
    for (i = 0; i < (int)(n * 3); i++)
        if (!(isfinite(rgb[i]) && rgb[i] > 0.0f))
            rgb[i] = 0.0f;

    /* 1. the sun: the brightest texel above the horizon, its disc over the sky around it */
    for (y = 0; y < h / 2; y++)
        for (x = 0; x < w; x++) {
            float l = lum(s->rgb + ((size_t)y * w + x) * 3);
            if (l > best) {
                best = l;
                bx = x;
                by = y;
            }
        }
    if (bx >= 0) {
        float c0[3], ring[3] = { 0, 0, 0 }, *rl = malloc((n < 4000000 ? n : 4000000) * sizeof(float)), lmed, e[3] = { 0, 0, 0 }, dd[3] = { 0, 0, 0 }, om = 0.0f, reach = 0.0f;
        int nr = 0;
        light_sky_dir(s, (float)bx, (float)by, c0);
        if (!rl) {
            light_sky_free(s);
            seterr(err, nerr, "skymap: out of memory");
            return -1;
        }
        FOR_NEAR(s, c0, LIGHT_SKY_SUN_RING1 * DEG, X, Y, D)
            if (D[0] * c0[0] + D[1] * c0[1] + D[2] * c0[2] > cosf(LIGHT_SKY_SUN_RING0 * DEG)) continue;
            {
                const float *p = s->rgb + ((size_t)Y * w + X) * 3;
                if (nr < 4000000)
                    rl[nr++] = lum(p);
                for (c = 0; c < 3; c++)
                    ring[c] += p[c];
            }
        END_NEAR
        if (nr) {
            for (c = 0; c < 3; c++)
                ring[c] /= (float)nr;
            qsort(rl, (size_t)nr, sizeof(float), cmpf);
            lmed = rl[nr / 2];
        } else
            lmed = best;
        free(rl);
        if (best > LIGHT_SKY_SUN_RATIO * lmed) {
            FOR_NEAR(s, c0, LIGHT_SKY_SUN_SEARCH * DEG, X, Y, D)
                {
                    const float *p = s->rgb + ((size_t)Y * w + X) * 3;
                    float wgt = texel_omega(w, h, Y);
                    if (lum(p) <= LIGHT_SKY_SUN_RATIO * lmed)
                        continue;
                    for (c = 0; c < 3; c++) {
                        e[c] += fmaxf(p[c] - ring[c], 0.0f) * wgt;
                        dd[c] += (lum(p) - lum(ring)) * wgt * D[c];
                    }
                    om += wgt;
                    reach = fmaxf(reach, acosf(fminf(D[0] * c0[0] + D[1] * c0[1] + D[2] * c0[2], 1.0f)));
                }
            END_NEAR
            if (lum(e) > 0.0f) {
                float l = sqrtf(dd[0] * dd[0] + dd[1] * dd[1] + dd[2] * dd[2]);
                s->has_sun = 1;
                for (c = 0; c < 3; c++) {
                    s->sun[c] = l > 0.0f ? dd[c] / l : c0[c];
                    s->sun_lux[c] = e[c];
                }
                s->sun_omega = om;
                /* painted over with the sky around it, a texel past its reach */
                reach += 1.5f * PI_F / (float)h;
                FOR_NEAR(s, c0, reach, X, Y, D)
                    memcpy(s->rgb + ((size_t)Y * w + X) * 3, ring, sizeof ring);
                END_NEAR
            }
        }
    }

    /* the copy the light is measured on: box-filtered to LIGHT_SKY_ANALYSIS
       texels across (the full map itself when it is no wider, or not a
       whole multiple of it) */
    {
        LightSky a = *s;
        int f = w > LIGHT_SKY_ANALYSIS && w % LIGHT_SKY_ANALYSIS == 0 && h % (LIGHT_SKY_ANALYSIS / 2) == 0 ? w / LIGHT_SKY_ANALYSIS : 1;
        int aw = w / f, ah = h / f;
        double up[3] = { 0, 0, 0 }, L[9][3];
        float upf[3];
        if (f > 1) {
            a.w = aw;
            a.h = ah;
            a.rgb = calloc((size_t)aw * ah * 3, sizeof(float));
            if (!a.rgb) {
                light_sky_free(s);
                seterr(err, nerr, "skymap: out of memory");
                return -1;
            }
            for (y = 0; y < h; y++)
                for (x = 0; x < w; x++)
                    for (c = 0; c < 3; c++)
                        a.rgb[((size_t)(y / f) * aw + x / f) * 3 + c] += s->rgb[((size_t)y * w + x) * 3 + c];
            for (i = 0; i < aw * ah * 3; i++)
                a.rgb[i] /= (float)(f * f);
        }
        /* the sky's light on level ground, as the file has it */
        for (y = 0; y < ah / 2; y++) {
            double wgt = texel_omega(aw, ah, y);
            for (x = 0; x < aw; x++) {
                float d[3];
                const float *p = a.rgb + ((size_t)y * aw + x) * 3;
                light_sky_dir(&a, (float)x, (float)y, d);
                if (d[2] <= 0.0f)
                    continue;
                for (c = 0; c < 3; c++)
                    up[c] += p[c] * d[2] * wgt;
            }
        }
        for (c = 0; c < 3; c++)
            upf[c] = (float)up[c];
        if (!(lum(upf) > 0.0f)) {
            if (f > 1)
                free(a.rgb);
            light_sky_free(s);
            seterr(err, nerr, "skymap: the upper hemisphere is black");
            return -1;
        }
        s->clip_ratio = s->has_sun ? lum(s->sun_lux) / lum(upf) : 0.0f;
        /* 2. physical units */
        if (o.total_lux > 0.0f)
            sc = o.total_lux / (lum(upf) + (s->has_sun ? lum(s->sun_lux) * fmaxf(s->sun[2], 0.0f) : 0.0f));
        else if (s->has_sun)
            sc = o.sun_lux / lum(s->sun_lux);
        else
            sc = o.horiz_lux / lum(upf);
        s->scale = sc;
        for (i = 0; i < (int)(n * 3); i++)
            s->rgb[i] *= sc;
        if (f > 1)
            for (i = 0; i < aw * ah * 3; i++)
                a.rgb[i] *= sc;
        for (c = 0; c < 3; c++) {
            s->sun_lux[c] *= sc;
            s->sky_up[c] = upf[c] * sc;
        }

        /* 4. the sun's bearing (before the ground and harmonics, which use directions) */
        if (s->has_sun && o.has_sun_az) {
            float az = atan2f(s->sun[1], s->sun[0]), hz = sqrtf(s->sun[0] * s->sun[0] + s->sun[1] * s->sun[1]);
            s->rot = o.sun_az - az;
            s->sun[0] = hz * cosf(o.sun_az);
            s->sun[1] = hz * sinf(o.sun_az);
        }
        a.rot = s->rot;

        /* 3. the ground: lit by the sun and the sky, on the map and its copy */
        for (c = 0; c < 3; c++)
            s->ground[c] = o.ground_albedo / PI_F * (s->sky_up[c] + (s->has_sun ? s->sun_lux[c] * fmaxf(s->sun[2], 0.0f) : 0.0f));
        for (int pass = 0; pass < (f > 1 ? 2 : 1); pass++) {
            LightSky *m = pass ? &a : s;
            for (y = m->h / 2 - 1; y < m->h; y++) {
                float el = 0.5f * PI_F - PI_F * ((float)y + 0.5f) / (float)m->h, t = fminf(fmaxf(-el / DEG, 0.0f), 1.0f);
                if (t <= 0.0f)
                    continue;
                for (x = 0; x < m->w; x++) {
                    float *p = m->rgb + ((size_t)y * m->w + x) * 3;
                    for (c = 0; c < 3; c++)
                        p[c] += (s->ground[c] - p[c]) * t;
                }
            }
        }

        /* 5. the irradiance harmonics */
        memset(L, 0, sizeof L);
        for (y = 0; y < ah; y++) {
            float wgt = texel_omega(aw, ah, y);
            for (x = 0; x < aw; x++) {
                float d[3], b[9];
                const float *p = a.rgb + ((size_t)y * aw + x) * 3;
                light_sky_dir(&a, (float)x, (float)y, d);
                light_sky_sh9(d, b);
                for (i = 0; i < 9; i++)
                    for (c = 0; c < 3; c++)
                        L[i][c] += (double)p[c] * b[i] * wgt;
            }
        }
        for (i = 0; i < 9; i++) {
            float k = i == 0 ? PI_F : i < 4 ? 2.0f * PI_F / 3.0f : PI_F / 4.0f;
            for (c = 0; c < 3; c++)
                s->sh[i][c] = (float)(k * L[i][c]);
        }
        if (f > 1)
            free(a.rgb);
    }
    return 0;
}

int light_sky_make(LightSky *s, const float *src, int w, int h, const LightSkyOpt *opt, char *err, int nerr)
{
    size_t n = (size_t)w * (size_t)(h > 0 ? h : 0) * 3;
    float *rgb = w > 0 && h > 0 ? malloc(n * sizeof(float)) : NULL;
    memset(s, 0, sizeof *s);
    if (!rgb) {
        seterr(err, nerr, "skymap: out of memory");
        return -1;
    }
    memcpy(rgb, src, n * sizeof(float));
    return sky_make_owned(s, rgb, w, h, opt, err, nerr);
}

int light_sky_load(LightSky *s, const char *path, const LightSkyOpt *opt, char *err, int nerr)
{
    float *rgb;
    int w, h, r;
    memset(s, 0, sizeof *s);
    if (light_hdr_read(path, &rgb, &w, &h)) {
        char m[512];
        snprintf(m, sizeof m, "skymap: cannot read %s (a Radiance .hdr, -Y h +X w)", path);
        seterr(err, nerr, m);
        return -1;
    }
    r = sky_make_owned(s, rgb, w, h, opt, err, nerr);
    return r;
}

void light_sky_free(LightSky *s)
{
    free(s->rgb);
    memset(s, 0, sizeof *s);
}
