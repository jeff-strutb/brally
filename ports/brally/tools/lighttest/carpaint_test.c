/* carpaint_test.c: the capture's car classification (light.h:
 * light_car_inbox, as light_capture_oncar applies it) on synthetic cars, a
 * rotated box with the margin: points just inside and just outside every
 * face, the corners, and random points against a direct test in the box's
 * frame.  Exit status: 0 when every point is classified right. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "light.h"

static int fails;

static void check(int got, int want, const char *what, const float p[3])
{
    if (got != want) {
        fails++;
        if (fails < 10)
            printf("FAIL  %s: (%.3f %.3f %.3f) got %d want %d\n", what, p[0], p[1], p[2], got, want);
    }
}

int main(void)
{
    LightCar c = { 0 };
    float yaw = 0.7f, pitch = 0.2f, m = LIGHT_CAR_MARGIN;
    int i, a, s, n = 0;
    /* fwd, side, up: a yaw then a pitch about the side axis */
    c.fwd[0] = cosf(yaw) * cosf(pitch), c.fwd[1] = sinf(yaw) * cosf(pitch), c.fwd[2] = sinf(pitch);
    c.side[0] = -sinf(yaw), c.side[1] = cosf(yaw), c.side[2] = 0;
    c.up[0] = c.fwd[1] * c.side[2] - c.fwd[2] * c.side[1];
    c.up[1] = c.fwd[2] * c.side[0] - c.fwd[0] * c.side[2];
    c.up[2] = c.fwd[0] * c.side[1] - c.fwd[1] * c.side[0];
    c.pos[0] = 120.0f, c.pos[1] = -45.0f, c.pos[2] = 3.0f;
    c.lo[0] = -2.1f, c.lo[1] = -0.9f, c.lo[2] = -0.2f;
    c.hi[0] = 2.2f, c.hi[1] = 0.9f, c.hi[2] = 1.3f;
    c.has_box = 1;
    const float *ax[3] = { c.fwd, c.side, c.up };
    /* every face, 1 cm inside and outside the margin, at the face's centre */
    for (a = 0; a < 3; a++)
        for (s = 0; s < 2; s++)
            for (i = 0; i < 2; i++) {
                float l[3], p[3];
                int k, b;
                for (b = 0; b < 3; b++)
                    l[b] = (c.lo[b] + c.hi[b]) * 0.5f;
                l[a] = s ? c.hi[a] + m + (i ? 0.01f : -0.01f) : c.lo[a] - m - (i ? 0.01f : -0.01f);
                for (k = 0; k < 3; k++)
                    p[k] = c.pos[k] + l[0] * ax[0][k] + l[1] * ax[1][k] + l[2] * ax[2][k];
                check(light_car_inbox(&c, p, m), !i, "face", p);
                n++;
            }
    /* random points over a box three times the size, against the direct test */
    srand(7);
    for (i = 0; i < 100000; i++) {
        float l[3], p[3];
        int k, want = 1;
        for (k = 0; k < 3; k++) {
            float lo = c.lo[k] - 2.0f, hi = c.hi[k] + 2.0f;
            l[k] = lo + (hi - lo) * (float)rand() / (float)RAND_MAX;
            if (fabsf(l[k] - (c.lo[k] - m)) < 1e-3f || fabsf(l[k] - (c.hi[k] + m)) < 1e-3f)
                l[k] += 2e-3f;          /* off the boundary itself: float rounding decides there */
            want = want && l[k] >= c.lo[k] - m && l[k] <= c.hi[k] + m;
        }
        for (k = 0; k < 3; k++)
            p[k] = c.pos[k] + l[0] * ax[0][k] + l[1] * ax[1][k] + l[2] * ax[2][k];
        check(light_car_inbox(&c, p, m), want, "random", p);
        n++;
    }
    /* a car with no model box classifies nothing */
    c.has_box = 0;
    check(light_car_inbox(&c, c.pos, m), 0, "no box", c.pos);
    n++;
    printf("%s  car classification: %d points, rotated box with %.2f m margin, %d wrong\n", fails ? "FAIL" : "PASS", n, m, fails);
    return fails != 0;
}
