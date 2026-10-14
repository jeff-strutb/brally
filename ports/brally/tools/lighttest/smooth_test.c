/* smooth_test.c: smooth shading normals (light_smooth.c) on shapes whose
 * normals are known: a 16-sided prism (its sides radial, its caps flat
 * across the 90 degree crease), the same with half its triangles wound the
 * other way, a UV sphere against its radii, and a triangle with no area.
 * smooth.sh builds and runs it. */
#include "light_smooth.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PI_F 3.14159265358979f
static int s_fail;
#define CHECK(c, ...) do { if (!(c)) { s_fail++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static int s_n;
static float s_p[200000];
static void tri(const float *a, const float *b, const float *c, int flip)
{
    memcpy(s_p + 3 * s_n, a, 12);
    memcpy(s_p + 3 * s_n + 3, flip ? c : b, 12);
    memcpy(s_p + 3 * s_n + 6, flip ? b : c, 12);
    s_n += 3;
}

static void prism(int flips)
{
    int i, k, N = 16;
    float cosc = cosf(LIGHT_SMOOTH_CREASE * PI_F / 180.0f), *n, worst_side = 0.0f, worst_cap = 0.0f;
    s_n = 0;
    for (i = 0; i < N; i++) {
        float a0 = 2.0f * PI_F * i / N, a1 = 2.0f * PI_F * (i + 1) / N;
        float b0[3] = { cosf(a0), sinf(a0), 0 }, b1[3] = { cosf(a1), sinf(a1), 0 }, t0[3] = { cosf(a0), sinf(a0), 1 }, t1[3] = { cosf(a1), sinf(a1), 1 };
        float c0[3] = { 0, 0, 0 }, c1[3] = { 0, 0, 1 };
        tri(b0, b1, t1, flips && (i & 1));
        tri(b0, t1, t0, flips && !(i & 1));
        tri(c0, b1, b0, flips && (i & 2));
        tri(c1, t0, t1, flips && !(i & 2));
    }
    n = malloc((size_t)s_n * 3 * sizeof(float));
    CHECK(!light_smooth_normals(s_p, s_n, cosc, n), "prism: smooth");
    for (i = 0; i < s_n; i++) {
        const float *p = s_p + 3 * i, *m = n + 3 * i;
        int cap = (i / 3) % 4 >= 2;
        float own[3], e1[3], e2[3], d;
        for (k = 0; k < 3; k++) {
            e1[k] = s_p[9 * (i / 3) + 3 + k] - s_p[9 * (i / 3) + k];
            e2[k] = s_p[9 * (i / 3) + 6 + k] - s_p[9 * (i / 3) + k];
        }
        own[0] = e1[1] * e2[2] - e1[2] * e2[1];
        own[1] = e1[2] * e2[0] - e1[0] * e2[2];
        own[2] = e1[0] * e2[1] - e1[1] * e2[0];
        d = own[0] * m[0] + own[1] * m[1] + own[2] * m[2];
        CHECK(d > 0.0f, "prism%s: corner %d's normal faces away from its own face", flips ? " (flipped)" : "", i);
        if (cap) {
            float er = 1.0f - fabsf(m[2]);
            worst_cap = fmaxf(worst_cap, er);
        } else if (hypotf(p[0], p[1]) > 0.5f) {
            float r = hypotf(p[0], p[1]), er = 1.0f - fabsf((m[0] * p[0] + m[1] * p[1]) / r) + fabsf(m[2]);
            worst_side = fmaxf(worst_side, er);
        }
    }
    printf("prism%s: sides off radial by %.2e, caps off flat by %.2e\n", flips ? " (half flipped)" : "", worst_side, worst_cap);
    CHECK(worst_side < 1e-4f, "prism: a side's normal off radial by %.2e", worst_side);
    CHECK(worst_cap < 1e-5f, "prism: a cap's normal not flat (%.2e): the crease leaked", worst_cap);
    free(n);
}

static void sphere(void)
{
    int i, j, NU = 32, NV = 16;
    float *n, worst = 0.0f;
    s_n = 0;
    for (j = 0; j < NV; j++)
        for (i = 0; i < NU; i++) {
            float u0 = 2.0f * PI_F * i / NU, u1 = 2.0f * PI_F * (i + 1) / NU, v0 = PI_F * j / NV - PI_F / 2, v1 = PI_F * (j + 1) / NV - PI_F / 2;
            float a[3] = { cosf(v0) * cosf(u0), cosf(v0) * sinf(u0), sinf(v0) }, b[3] = { cosf(v0) * cosf(u1), cosf(v0) * sinf(u1), sinf(v0) };
            float c[3] = { cosf(v1) * cosf(u1), cosf(v1) * sinf(u1), sinf(v1) }, d[3] = { cosf(v1) * cosf(u0), cosf(v1) * sinf(u0), sinf(v1) };
            if (j > 0)
                tri(a, b, c, 0);
            if (j < NV - 1)
                tri(a, c, d, 0);
        }
    n = malloc((size_t)s_n * 3 * sizeof(float));
    CHECK(!light_smooth_normals(s_p, s_n, cosf(LIGHT_SMOOTH_CREASE * PI_F / 180.0f), n), "sphere: smooth");
    for (i = 0; i < s_n; i++) {
        const float *p = s_p + 3 * i, *m = n + 3 * i;
        float l = sqrtf(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]), d = (m[0] * p[0] + m[1] * p[1] + m[2] * p[2]) / l;
        worst = fmaxf(worst, acosf(fminf(d, 1.0f)) * 180.0f / PI_F);
    }
    printf("sphere (%d triangles): worst normal %.3f degrees off its radius (faces %.1f degrees apart)\n", s_n / 3, worst, 360.0f / NU);
    CHECK(worst < 1.0f, "sphere: a normal %.3f degrees off its radius", worst);
    free(n);
}

int main(void)
{
    float z[9] = { 1, 2, 3, 1, 2, 3, 2, 4, 6 }, n[9];
    prism(0);
    prism(1);
    sphere();
    CHECK(!light_smooth_normals(z, 3, 0.5f, n) && n[0] == 0 && n[4] == 0 && n[8] == 0, "a triangle with no area got a normal");
    printf(s_fail ? "smooth_test: %d FAILED\n" : "smooth_test: all passed\n", s_fail);
    return s_fail != 0;
}
