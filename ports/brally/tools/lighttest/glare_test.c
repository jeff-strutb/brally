/* glare_test.c: the sun's glare and flare constants (brr_metal_glare.h).
 * The PSF integrates to 1 (so the veiling glare carries GLARE_SCATTER of the
 * disc's energy); the glare is zero behind the camera and when every tap is
 * covered; visibility falls monotonically as the disc is covered; the light
 * projects back onto the pixel its camera ray goes through; the C cloud
 * density gives a transmittance in (0, 1] that falls with the extinction. */
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "brr_metal_glare.h"

static int fails;
static void check(int ok, const char *what, double a, double b)
{
    printf("%s  %s (%g, %g)\n", ok ? "PASS" : "FAIL", what, a, b);
    fails += !ok;
}

/* a KU: a 640 x 480 frame, 90 degree horizontal field, looking along +y, z up */
static void mk_ku(float k[12][4])
{
    memset(k, 0, sizeof(float) * 48);
    k[0][0] = 1, k[0][2] = 1;                    /* xft: the identity */
    k[1][0] = 1, k[1][2] = 1;                    /* vpt */
    /* c = (gx, gy, 1) to the world: x right, y forward, z up; gy scaled by the aspect */
    k[2][0] = 1;                                 /* m0: world x = cx */
    k[3][2] = 1;                                 /* m1: world y = cz */
    k[4][1] = 0.75f;                             /* m2: world z = 0.75 cy */
    k[6][3] = 0;                                 /* eye at the origin, height 0 km */
    k[7][0] = 640, k[7][1] = 480;
    k[10][0] = 0.5f, k[10][1] = 0.02f, k[10][2] = 0, k[10][3] = 1;
}

int main(void)
{
    float k[12][4], s[3], il[3] = { 1e5f, 1e5f, 1e5f };
    double px[2], om, d[3], sum, h, x, y;
    BrrGlare g;
    int i, j, n;

    /* 1. the PSF's energy: integrated over the plane (out to 1 frame height) */
    h = 0.0002, sum = 0;
    for (n = 0; n < 5000; n++) {
        double r = (n + 0.5) * h;
        sum += brr_glare_psf(r) * 2 * 3.14159265358979 * r * h;
    }
    check(fabs(sum - 1) < 0.01, "the PSF integrates to 1 within 1% (radial)", sum, 1);
    /* on a pixel grid as the post samples it: 480 rows, 640 columns, the light at the centre */
    sum = 0;
    for (j = 0; j < 480; j++)
        for (i = 0; i < 640; i++) {
            x = (i + 0.5 - 320) / 480.0, y = (j + 0.5 - 240) / 480.0;
            sum += brr_glare_psf(sqrt(x * x + y * y)) / (480.0 * 480.0);
        }
    check(fabs(sum - 1) < 0.01, "the PSF summed over a 640 x 480 frame's pixels: 1 within 1%", sum, 1);
    printf("      veiling glare energy: %.4f of the disc's (GLARE_SCATTER %.2f)\n", sum * GLARE_SCATTER, GLARE_SCATTER);

    /* 2. projection: the light straight ahead at the frame's centre; a ray's own pixel round-trips */
    mk_ku(k);
    s[0] = 0, s[1] = 1, s[2] = 0;
    check(brr_glare_project((const float (*)[4])k, s, px, &om) && fabs(px[0] - 319.5) < 1e-3 && fabs(px[1] - 239.5) < 1e-3,
          "the light ahead projects to the centre", px[0], px[1]);
    check(fabs(om - (2.0 / 640) * (2.0 / 640)) < 1e-9, "a pixel's solid angle at the centre", om, (2.0 / 640) * (2.0 / 640));
    brr_glare_ray((const float (*)[4])k, 100.25, 400.5, d);
    s[0] = (float)d[0], s[1] = (float)d[1], s[2] = (float)d[2];
    check(brr_glare_project((const float (*)[4])k, s, px, &om) && fabs(px[0] - 100.25) < 1e-2 && fabs(px[1] - 400.5) < 1e-2,
          "a pixel's camera ray projects back to it", px[0], px[1]);

    /* 3. behind the camera: no glare */
    s[0] = 0, s[1] = -1, s[2] = 0;
    g = brr_glare_setup((const float (*)[4])k, s, il, 1, 1e-3f);
    check(g.sp[3] == 0 && g.e[0] == 0, "the light behind the camera: no glare", g.sp[3], g.e[0]);
    /* far off the frame (beyond the glare's radius): none; just off it: some */
    s[0] = 0.99f, s[1] = 0.14f, s[2] = 0;
    g = brr_glare_setup((const float (*)[4])k, s, il, 1, 1e-3f);
    check(g.sp[3] == 0, "the light far off the frame: no glare", g.sp[0], g.sp[3]);
    s[0] = 0.75f, s[1] = 0.66f, s[2] = 0.05f;
    g = brr_glare_setup((const float (*)[4])k, s, il, 1, 1e-3f);
    check(g.sp[3] == 1 && g.sp[0] > 1, "the light just off the frame: its glare reaches in", g.sp[0], g.sp[3]);

    /* 4. visibility: all taps covered -> 0, so the glare is 0; monotonic as the disc is covered */
    {
        unsigned char cv[16];
        float v, last = 2;
        int mono = 1;
        memset(cv, 1, 16);
        check(brr_glare_vis(cv) == 0, "every tap covered: visibility 0 (glare energy 0)", brr_glare_vis(cv), 0);
        memset(cv, 0, 16);
        for (i = 0; i <= 16; i++) {
            if (i)
                cv[i - 1] = 1;
            v = brr_glare_vis(cv);
            if (v > last)
                mono = 0;
            last = v;
        }
        check(mono && last == 0, "visibility falls monotonically as the taps are covered", last, mono);
    }

    /* 5. clouds: transmittance in (0, 1], falling with the extinction; none with the tier off */
    {
        float dir[3] = { 0.3f, 0.4f, 0.866f }, c1[4] = { 0.9f, 0.002f, 0, 1 }, c2[4] = { 0.9f, 0.02f, 0, 1 }, c0[4] = { 0.9f, 0.02f, 0, 0 };
        float t1 = brr_glare_cloud_T(dir, 0, c1), t2 = brr_glare_cloud_T(dir, 0, c2), t0 = brr_glare_cloud_T(dir, 0, c0);
        check(t1 > 0 && t1 <= 1 && t2 <= t1 && t0 == 1, "the clouds' transmittance toward the light", t1, t2);
    }
    printf("%s\n", fails ? "glare: FAIL" : "glare: all pass");
    return fails != 0;
}
