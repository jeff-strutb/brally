/* light_smooth.c: smooth shading normals (light_smooth.h). */
#include "light_smooth.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static uint32_t cellhash(int64_t x, int64_t y, int64_t z)
{
    uint64_t h = (uint64_t)x * 0x9E3779B97F4A7C15ull ^ (uint64_t)y * 0xC2B2AE3D27D4EB4Full ^ (uint64_t)z * 0x165667B19E3779F9ull;
    h ^= h >> 29;
    h *= 0xBF58476D1CE4E5B9ull;
    h ^= h >> 32;
    return (uint32_t)h;
}

int light_smooth_normals(const float *p, int ncorner, float cos_crease, float *n)
{
    int ntri = ncorner / 3, i, k, c;
    uint32_t cap = 16, mask;
    float *fn = malloc((size_t)(ntri ? ntri : 1) * 3 * sizeof(float)), *ang = malloc((size_t)(ncorner ? ncorner : 1) * sizeof(float));
    int *head, *next = malloc((size_t)(ncorner ? ncorner : 1) * sizeof(int));
    int64_t *key;
    memset(n, 0, (size_t)ncorner * 3 * sizeof(float));
    while (cap < (uint32_t)ncorner * 2u)
        cap <<= 1;
    mask = cap - 1;
    head = malloc(cap * sizeof(int));
    key = malloc((size_t)cap * 3 * sizeof(int64_t));
    if (!fn || !ang || !next || !head || !key) {
        free(fn); free(ang); free(next); free(head); free(key);
        return -1;
    }
    for (i = 0; i < (int)cap; i++)
        head[i] = -1;
    /* the faces' normals and each corner's angle */
    for (i = 0; i < ntri; i++) {
        const float *a = p + 9 * i, *b = a + 3, *d = a + 6;
        float e1[3] = { b[0] - a[0], b[1] - a[1], b[2] - a[2] }, e2[3] = { d[0] - a[0], d[1] - a[1], d[2] - a[2] }, x[3], l;
        x[0] = e1[1] * e2[2] - e1[2] * e2[1];
        x[1] = e1[2] * e2[0] - e1[0] * e2[2];
        x[2] = e1[0] * e2[1] - e1[1] * e2[0];
        l = sqrtf(x[0] * x[0] + x[1] * x[1] + x[2] * x[2]);
        for (k = 0; k < 3; k++)
            fn[3 * i + k] = l > 1e-12f ? x[k] / l : 0.0f;
        for (c = 0; c < 3; c++) {
            const float *o = p + 9 * i + 3 * c, *u = p + 9 * i + 3 * ((c + 1) % 3), *v = p + 9 * i + 3 * ((c + 2) % 3);
            float du[3] = { u[0] - o[0], u[1] - o[1], u[2] - o[2] }, dv[3] = { v[0] - o[0], v[1] - o[1], v[2] - o[2] };
            float lu = sqrtf(du[0] * du[0] + du[1] * du[1] + du[2] * du[2]), lv = sqrtf(dv[0] * dv[0] + dv[1] * dv[1] + dv[2] * dv[2]);
            float cs = lu > 0.0f && lv > 0.0f ? (du[0] * dv[0] + du[1] * dv[1] + du[2] * dv[2]) / (lu * lv) : 1.0f;
            ang[3 * i + c] = l > 1e-12f ? acosf(fminf(fmaxf(cs, -1.0f), 1.0f)) : 0.0f;
        }
    }
    /* corners by their welded point: an open-addressed table of cells, each
       cell's corners on a list (a point near a cell's edge is looked up in
       the neighbouring cells too) */
    for (i = 0; i < ncorner; i++) {
        int64_t q[3];
        uint32_t s;
        for (k = 0; k < 3; k++)
            q[k] = (int64_t)floorf(p[3 * i + k] / LIGHT_SMOOTH_WELD);
        s = cellhash(q[0], q[1], q[2]) & mask;
        while (head[s] >= 0 && (key[3 * s] != q[0] || key[3 * s + 1] != q[1] || key[3 * s + 2] != q[2]))
            s = (s + 1) & mask;
        if (head[s] < 0) {
            key[3 * s] = q[0];
            key[3 * s + 1] = q[1];
            key[3 * s + 2] = q[2];
        }
        next[i] = head[s];
        head[s] = i;
    }
    for (i = 0; i < ncorner; i++) {
        const float *own = fn + 3 * (i / 3);
        float sum[3] = { 0, 0, 0 }, l;
        int64_t q[3];
        int dx, dy, dz;
        if (own[0] == 0.0f && own[1] == 0.0f && own[2] == 0.0f)
            continue;
        for (k = 0; k < 3; k++)
            q[k] = (int64_t)floorf(p[3 * i + k] / LIGHT_SMOOTH_WELD);
        for (dz = -1; dz <= 1; dz++)
            for (dy = -1; dy <= 1; dy++)
                for (dx = -1; dx <= 1; dx++) {
                    uint32_t s = cellhash(q[0] + dx, q[1] + dy, q[2] + dz) & mask;
                    int j;
                    while (head[s] >= 0 && (key[3 * s] != q[0] + dx || key[3 * s + 1] != q[1] + dy || key[3 * s + 2] != q[2] + dz))
                        s = (s + 1) & mask;
                    for (j = head[s]; j >= 0; j = next[j]) {
                        const float *o = fn + 3 * (j / 3), *pj = p + 3 * j, *pi = p + 3 * i;
                        float d = o[0] * own[0] + o[1] * own[1] + o[2] * own[2], sg = d < 0.0f ? -1.0f : 1.0f;
                        float w2 = (pj[0] - pi[0]) * (pj[0] - pi[0]) + (pj[1] - pi[1]) * (pj[1] - pi[1]) + (pj[2] - pi[2]) * (pj[2] - pi[2]);
                        if (w2 > LIGHT_SMOOTH_WELD * LIGHT_SMOOTH_WELD || fabsf(d) < cos_crease)
                            continue;
                        for (k = 0; k < 3; k++)
                            sum[k] += sg * ang[j] * o[k];
                    }
                }
        l = sqrtf(sum[0] * sum[0] + sum[1] * sum[1] + sum[2] * sum[2]);
        for (k = 0; k < 3; k++)
            n[3 * i + k] = l > 1e-12f ? sum[k] / l : own[k];
    }
    free(fn); free(ang); free(next); free(head); free(key);
    return 0;
}
