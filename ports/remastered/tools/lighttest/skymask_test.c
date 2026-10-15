/* skymask_test.c: the backdrop's structural sky mask (brr_skymask.h) on
 * synthetic panoramas with a known skyline, and the exposure meter
 * (light_night.h light_meter_ev) on synthetic frames.  With file
 * arguments (W H file.rgba ...) it also runs the detector on real textures
 * and writes each mask as a PGM next to an output prefix.
 *
 *   skymask_test [outdir [w h file.rgba]...]
 * Exit status 0 when every check passes. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "brr_skymask.h"
#include "light_night.h"

static int fails;
static void check(int ok, const char *what, double v, double lim)
{
    printf("%s  %-58s %.6g (limit %.6g)\n", ok ? "PASS" : "FAIL", what, v, lim);
    fails += !ok;
}

static double clampd(double v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
static int q5(double v) { int i = (int)(clampd(v) * 31.0 + 0.5); return i * 255 / 31; }   /* the N64's 5 bits a channel */

/* one synthetic panorama, seed s: sky gradient, soft white, grey and yellow
 * clouds, a sun glow, blue distant hills (hard edge) over a near hill and a sea */
static void synth(int s, int W, int H, uint8_t *px, int *truth, uint8_t *cloud, uint8_t *blue)
{
    int x, y, k;
    double cx[6], cy[6], sg[6], am[6], col[6][3];
    double sx = 40 + 37 * s % (W - 80), sy = 6 + s % 8;
    srand(1234 + s);
    for (k = 0; k < 6; k++) {
        cx[k] = rand() % W;
        cy[k] = 4 + rand() % 14;
        sg[k] = 5.0 + rand() % 4;
        am[k] = 0.25 + (rand() % 100) / 400.0;
        if (k % 3 == 0) col[k][0] = 1.0, col[k][1] = 1.0, col[k][2] = 1.0;          /* white */
        else if (k % 3 == 1) col[k][0] = 0.62, col[k][1] = 0.63, col[k][2] = 0.66;  /* grey */
        else col[k][0] = 1.0, col[k][1] = 0.93, col[k][2] = 0.62;                   /* yellow, cream */
    }
    for (x = 0; x < W; x++) {
        double yf = H * 0.48 + 4 * sin(x * 0.05 + s) + 2 * sin(x * 0.13 + 2 * s);
        double yn = H * 0.62 + 7 * sin(x * 0.031 + 1 + s);
        int sf = (int)ceil(yf), sn = (int)ceil(yn);
        truth[x] = sf < sn ? sf : sn;
        for (y = 0; y < H; y++) {
            double t = (double)y / H, c[3], wsum = 0;
            uint8_t *p = &px[(y * W + x) * 4];
            c[0] = 0.32 + 0.45 * t; c[1] = 0.52 + 0.32 * t; c[2] = 0.88 + 0.07 * t;
            {   /* the sun's glow */
                double r = hypot(x - sx, (y - sy) * 1.5), g = 0.55 * exp(-r / 9.0);
                c[0] += g; c[1] += g * 0.9; c[2] += g * 0.5;
            }
            for (k = 0; k < 6; k++) {
                double d2 = ((x - cx[k]) * (x - cx[k]) * 0.25 + (y - cy[k]) * (y - cy[k])) / (sg[k] * sg[k]);
                double a = am[k] * exp(-0.5 * d2);
                int j;
                for (j = 0; j < 3; j++)
                    c[j] = c[j] * (1 - a) + col[k][j] * a;
                wsum += a;
            }
            cloud[y * W + x] = y < truth[x] && wsum > 0.05;
            blue[y * W + x] = 0;
            if (y >= truth[x]) {
                if (y >= sf && y < sn) {          /* the distant hills, hazy blue */
                    c[0] = 0.42 + 0.02 * sin(x * 0.7); c[1] = 0.52; c[2] = 0.72;
                    blue[y * W + x] = 1;
                } else if (y >= sn && x % 64 < 40) {   /* the near hill */
                    c[0] = 0.36 + 0.05 * sin(x * 1.3 + y); c[1] = 0.40; c[2] = 0.22;
                } else {                          /* the sea */
                    c[0] = 0.12; c[1] = 0.32; c[2] = 0.55 + 0.03 * sin(x * 0.9 + y * 0.4);
                    blue[y * W + x] = 1;
                }
            }
            p[0] = (uint8_t)q5(c[0]); p[1] = (uint8_t)q5(c[1]); p[2] = (uint8_t)q5(c[2]); p[3] = 255;
        }
    }
}

static void pgm(const char *path, const uint8_t *m, int w, int h)
{
    FILE *f = fopen(path, "wb");
    if (!f)
        return;
    fprintf(f, "P5\n%d %d\n255\n", w, h);
    fwrite(m, 1, (size_t)w * h, f);
    fclose(f);
}

/* a synthetic frame for the meter: luminance per pixel (cd/m2) into the post pass's histogram, centre-weighted */
static void meter_bin(uint32_t *h, int W, int H, double (*lum)(int, int, int, int))
{
    int x, y;
    memset(h, 0, 512 * 4);
    for (y = 0; y < H; y += 4)
        for (x = 0; x < W; x += 4) {
            double Y = lum(x, y, W, H);
            int b = (int)fmin(fmax((log2(fmax(Y, 1e-6)) + 10.0) * 16.0, 0.0), 511.0);
            h[b] += light_meter_weight((float)x / W, (float)y / H);
        }
}
static double road_only(int x, int y, int W, int H) { (void)x; (void)y; (void)W; (void)H; return 4600.0; }
/* the road below, a sunlit white wall (4 stops up) over the top fifth's corners */
static double road_wall(int x, int y, int W, int H) { return (y < H / 5 && (x < W / 4 || x > 3 * W / 4)) ? 4600.0 * 16 : 4600.0; }

/* The game's own backdrop (Coastline, sunny; the fog race draws the same
 * textures), dumped as drawn: <tex>_64x64.rgba.  Every texture is the
 * picture over its mirror image; the expected skyline is in the full
 * texture's rows at column 20 (the lower, upright half starts at row 32),
 * established from the texels down that column: the sky's 5-bit gradient
 * 5a6a73 at the half's top, the first row of hill paint (brown/green
 * 5a6252, 625a4a, 5a5a52...) or of the sea (525a62 / 4a5a62 after the
 * haze), and the zenith fill colour 4a626a of the backdrop's 4x1 fill
 * texture at the top of the sky tiles.  64: all sky, 32: all scenery. */
static const struct { int tid, sky20; float lo, hi; } s_coast[] = {
    { 6, 64, 1.0f, 1.0f }, { 8, 64, 1.0f, 1.0f }, { 11, 64, 1.0f, 1.0f },   /* the sky: gradient, clouds */
    { 12, 64, 0.98f, 1.0f }, { 13, 64, 1.0f, 1.0f }, { 14, 64, 0.98f, 1.0f },
    { 7, 32, 0.0f, 0.0f },                                                 /* the sea, flat, with glints */
    { 9, 34, 0.05f, 0.13f }, { 10, 40, 0.18f, 0.27f },                    /* the headland's hills to the top */
    { 22, 44, 0.38f, 0.47f }, { 23, 41, 0.23f, 0.31f },                   /* islands under haze */
    { 21, 48, 0.44f, 0.50f }, { 24, 48, 0.44f, 0.50f },                   /* haze over the sea */
};

static void coastline(const char *dir)
{
    char path[1024];
    static uint8_t p[64 * 64 * 4], m[64 * 64];
    int sk[64], i, x, worst = 0;
    for (i = 0; i < (int)(sizeof s_coast / sizeof s_coast[0]); i++) {
        FILE *f;
        float fr;
        int mir = 1, ok;
        snprintf(path, sizeof path, "%s/%d_64x64.rgba", dir, s_coast[i].tid);
        if (!(f = fopen(path, "rb")) || fread(p, 1, sizeof p, f) != sizeof p) {
            printf("FAIL  %s unreadable\n", path);
            fails++;
            if (f) fclose(f);
            continue;
        }
        fclose(f);
        fr = brr_skymask(p, 64, 64, sk, m);
        for (x = 0; x < 64 * 32; x++)
            mir &= m[x] == m[(63 - x / 64) * 64 + x % 64];
        if (abs(sk[20] - s_coast[i].sky20) > worst)
            worst = abs(sk[20] - s_coast[i].sky20);
        ok = fr >= s_coast[i].lo - 1e-6f && fr <= s_coast[i].hi + 1e-6f && abs(sk[20] - s_coast[i].sky20) <= 1 && mir;
        printf("%s  %s tex %2d: sky %.3f (expected %.2f..%.2f), skyline at column 20 row %d (expected %d)%s\n",
               ok ? "PASS" : "FAIL", dir, s_coast[i].tid, fr, s_coast[i].lo, s_coast[i].hi, sk[20], s_coast[i].sky20,
               mir ? "" : ", mask not mirrored");
        fails += !ok;
        snprintf(path, sizeof path, "%s/%d_64x64_mask.pgm", dir, s_coast[i].tid);
        pgm(path, m, 64, 64);
    }
    printf("INFO  %s: worst skyline error at column 20, %d rows\n", dir, worst);
}

int main(int argc, char **argv)
{
    enum { W = 256, H = 64, N = 24 };
    static uint8_t px[W * H * 4], m[W * H], cloud[W * H], blue[W * H];
    int truth[W], sky[W], s, x, i, a;
    long cols = 0, colok = 0, nc = 0, ncs = 0, nb = 0, nbs = 0;
    const char *out = argc > 1 ? argv[1] : NULL;
    char path[1024];

    for (s = 0; s < N; s++) {
        synth(s, W, H, px, truth, cloud, blue);
        brr_skymask(px, W, H, sky, m);
        for (x = 0; x < W; x++, cols++)
            colok += abs(sky[x] - truth[x]) <= 1;
        for (i = 0; i < W * H; i++) {
            if (abs(i / W - truth[i % W]) <= 1) continue;   /* the silhouette's soft edge band, as the shader feathers it */
            if (cloud[i]) { nc++; ncs += m[i] == 255; }
            if (blue[i]) { nb++; nbs += m[i] == 255; }
        }
        if (out && s < 4) {
            snprintf(path, sizeof path, "%s/synth%d_mask.pgm", out, s);
            pgm(path, m, W, H);
            for (i = 0; i < W * H; i++)
                m[i] = (uint8_t)((px[i * 4] + px[i * 4 + 1] + px[i * 4 + 2]) / 3);
            snprintf(path, sizeof path, "%s/synth%d_grey.pgm", out, s);
            pgm(path, m, W, H);
        }
    }
    check((double)colok / cols > 0.99, "skyline within 1 texel, fraction of columns", (double)colok / cols, 0.99);
    check(ncs == nc, "cloud and glow texels classified as sky, fraction", (double)ncs / nc, 1.0);
    check(nbs == 0, "blue hill and sea texels classified as sky, fraction", (double)nbs / nb, 0.0);
    {   /* a pure-sky texture: no scenery */
        for (i = 0; i < W * H; i++) {
            int y = i / W;
            px[i * 4] = (uint8_t)q5(0.3 + 0.4 * y / H); px[i * 4 + 1] = (uint8_t)q5(0.5 + 0.3 * y / H);
            px[i * 4 + 2] = (uint8_t)q5(0.9); px[i * 4 + 3] = 255;
        }
        check(brr_skymask(px, W, H, sky, m) == 1.0f, "pure-sky texture, fraction sky", 1.0, 1.0);
    }

    /* the meter by day: a sunlit road (albedo 0.12 under 100,000 lux sun and
       20,000 sky: 4,600 cd/m2) lands at mid-grey 0.18 plus 0.5 to 1 stop
       pre-exposed (exposure 1 / (1.2 2^EV)) with sunny's band 5..95% and
       compensation 1.5; a bright wall over the top corners keeps it there */
    {
        static uint32_t h[512];
        float ev, ev2;
        double road, road2;
        meter_bin(h, 320, 240, road_only);
        ev = light_meter_ev(h, 512, 0.05f, 0.95f, 12.0f, 16.0f, 1.5f);
        road = 4600.0 / (1.2 * exp2(ev));
        meter_bin(h, 320, 240, road_wall);
        ev2 = light_meter_ev(h, 512, 0.05f, 0.95f, 12.0f, 16.0f, 1.5f);
        road2 = 4600.0 / (1.2 * exp2(ev2));
        printf("INFO  meter: road only EV100 %.2f, road %.3f = mid-grey %+.2f stops; with the wall EV100 %.2f, road %+.2f stops\n",
               ev, road, log2(road / 0.18), ev2, log2(road2 / 0.18));
        check(log2(road / 0.18) >= 0.5 && log2(road / 0.18) <= 1.0, "sunlit road over mid-grey, stops", log2(road / 0.18), 1.0);
        check(log2(road2 / 0.18) >= 0.5 && log2(road2 / 0.18) <= 1.0, "sunlit road with a bright wall in frame, stops", log2(road2 / 0.18), 1.0);
        ev2 = light_meter_ev(h, 512, 0.80f, 0.983f, 12.0f, 16.0f, 0.0f);
        printf("INFO  meter, the old band (80..98.3%%, no compensation), same frame: road %+.2f stops from mid-grey\n", log2(4600.0 / (1.2 * exp2(ev2)) / 0.18));
    }

    /* the game's Coastline backdrop, where dumped (game data, never committed) */
    {
        const char *d = getenv("BACKDROPS");
        if (d && *d) {
            snprintf(path, sizeof path, "%s/20_quickrace_drive", d);
            coastline(path);
            snprintf(path, sizeof path, "%s/33_weather_fog", d);
            coastline(path);
        }
    }

    /* real textures: w h file.rgba triples */
    for (a = 2; a + 2 < argc; a += 3) {
        int w = atoi(argv[a]), h = atoi(argv[a + 1]), j, rough = 0;
        uint8_t *p = malloc((size_t)w * h * 4), *mm = malloc((size_t)w * h);
        int *sk = malloc((size_t)w * sizeof(int));
        FILE *f = fopen(argv[a + 2], "rb");
        float frac;
        if (!f || fread(p, 4, (size_t)w * h, f) != (size_t)w * h) {
            printf("SKIP  %s: unreadable\n", argv[a + 2]);
            if (f) fclose(f);
            continue;
        }
        fclose(f);
        frac = brr_skymask(p, w, h, sk, mm);
        for (j = 1; j < w; j++)
            rough += abs(sk[j] - sk[j - 1]);
        printf("REAL  %s %dx%d: sky %.3f, skyline mean |step| %.3f texels\n", argv[a + 2], w, h, frac, (double)rough / (w - 1));
        if (out) {
            const char *b = strrchr(argv[a + 2], '/');
            snprintf(path, sizeof path, "%s/%s_mask.pgm", out, b ? b + 1 : argv[a + 2]);
            pgm(path, mm, w, h);
        }
        free(p); free(mm); free(sk);
    }
    return fails ? 1 : 0;
}
