/* lt_scene.h: the scenes and rays the lighting's ray tracing tests share
 * (bvh_test.c on the CPU, rt_gpu.m on the GPU): a track dumped as Wavefront
 * OBJ (BR_LIGHT_TRACKOBJ's form: '#' object records, 'g oN' groups, 'v', 'f'),
 * synthetic scenes, and ray sets made from a fixed seed, so both programs
 * trace the same rays. */
#ifndef LT_SCENE_H
#define LT_SCENE_H

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *name;
    float *tri;              /* ntri x 9 */
    int *obj;
    int ntri;
    float lo[3], hi[3];
} LtScene;

typedef struct {
    float o[3], tmin, d[3], tmax;
} LtRay;

static uint64_t lt_rng = 0x9E3779B97F4A7C15ull;
static float lt_rand(void)       /* [0, 1) */
{
    lt_rng ^= lt_rng << 13;
    lt_rng ^= lt_rng >> 7;
    lt_rng ^= lt_rng << 17;
    return (float)(lt_rng >> 40) * (1.0f / 16777216.0f);
}

static void lt_push(LtScene *s, int *cap, const float v[9], int obj)
{
    if (s->ntri == *cap) {
        *cap = *cap ? *cap * 2 : 1024;
        s->tri = realloc(s->tri, (size_t)*cap * 9 * sizeof *s->tri);
        s->obj = realloc(s->obj, (size_t)*cap * sizeof *s->obj);
    }
    memcpy(s->tri + 9 * s->ntri, v, 9 * sizeof *v);
    s->obj[s->ntri++] = obj;
}

static void lt_bounds(LtScene *s)
{
    int i, k;
    for (k = 0; k < 3; k++) {
        s->lo[k] = 1e30f;
        s->hi[k] = -1e30f;
    }
    for (i = 0; i < s->ntri * 3; i++)
        for (k = 0; k < 3; k++) {
            float x = s->tri[3 * i + k];
            if (x < s->lo[k]) s->lo[k] = x;
            if (x > s->hi[k]) s->hi[k] = x;
        }
}

static int lt_load_obj(LtScene *s, const char *path)
{
    FILE *f = fopen(path, "r");
    char line[512];
    float *v = NULL;
    int nv = 0, capv = 0, cap = 0, obj = 0;
    memset(s, 0, sizeof *s);
    s->name = "track";
    if (!f)
        return 0;
    while (fgets(line, sizeof line, f)) {
        if (line[0] == 'v' && line[1] == ' ') {
            if (nv == capv) {
                capv = capv ? capv * 2 : 4096;
                v = realloc(v, (size_t)capv * 3 * sizeof *v);
            }
            sscanf(line + 2, "%f %f %f", &v[3 * nv], &v[3 * nv + 1], &v[3 * nv + 2]);
            nv++;
        } else if (line[0] == 'g' && line[1] == ' ') {
            sscanf(line + 2, "o%d", &obj);
        } else if (line[0] == 'f' && line[1] == ' ') {
            int a, b, c;
            float t[9];
            if (sscanf(line + 2, "%d %d %d", &a, &b, &c) != 3 || a < 1 || b < 1 || c < 1 || a > nv || b > nv || c > nv)
                continue;
            memcpy(t, v + 3 * (a - 1), 3 * sizeof *t);
            memcpy(t + 3, v + 3 * (b - 1), 3 * sizeof *t);
            memcpy(t + 6, v + 3 * (c - 1), 3 * sizeof *t);
            lt_push(s, &cap, t, obj);
        }
    }
    fclose(f);
    free(v);
    lt_bounds(s);
    return s->ntri > 0;
}

/* 20,000 triangles of every size and orientation, overlapping, in a 100 m
 * cube a kilometre from the origin (the track's magnitudes) */
static void lt_soup(LtScene *s)
{
    int i, k, cap = 0;
    memset(s, 0, sizeof *s);
    s->name = "soup";
    lt_rng = 0x1234567ull;
    for (i = 0; i < 20000; i++) {
        float c[3], t[9], size = 0.01f * powf(1000.0f, lt_rand());
        for (k = 0; k < 3; k++)
            c[k] = 1000.0f + 100.0f * lt_rand();
        for (k = 0; k < 9; k++)
            t[k] = c[k % 3] + size * (lt_rand() - 0.5f);
        lt_push(s, &cap, t, i);
    }
    lt_bounds(s);
}

/* a 100 x 100 heightfield, 20,000 triangles sharing every edge and corner (the
 * watertight test), at 2 km from the origin, with flat terraces (flat
 * boxes) and a wall standing on it */
static void lt_terrain(LtScene *s)
{
    int x, y, cap = 0;
    float h[101][101];
    memset(s, 0, sizeof *s);
    s->name = "terrain";
    for (y = 0; y <= 100; y++)
        for (x = 0; x <= 100; x++) {
            float z = 3.0f * sinf(x * 0.21f) * cosf(y * 0.17f) + 0.4f * sinf(x * 1.3f + y * 0.7f);
            if (x > 30 && x < 50)
                z = 2.0f;            /* a terrace: flat, axis aligned */
            h[y][x] = 20.0f + z;
        }
    for (y = 0; y < 100; y++)
        for (x = 0; x < 100; x++) {
            float x0 = 2000.0f + 2.0f * x, y0 = 1500.0f + 2.0f * y, x1 = x0 + 2.0f, y1 = y0 + 2.0f;
            float a[9] = { x0, y0, h[y][x], x1, y0, h[y][x + 1], x1, y1, h[y + 1][x + 1] };
            float b[9] = { x0, y0, h[y][x], x1, y1, h[y + 1][x + 1], x0, y1, h[y + 1][x] };
            lt_push(s, &cap, a, 0);
            lt_push(s, &cap, b, 0);
        }
    for (y = 0; y < 50; y++) {   /* a wall across it: vertical, 50 quads */
        float y0 = 1500.0f + 4.0f * y, y1 = y0 + 4.0f;
        float a[9] = { 2100.0f, y0, 18.0f, 2100.0f, y1, 18.0f, 2100.0f, y1, 26.0f };
        float b[9] = { 2100.0f, y0, 18.0f, 2100.0f, y1, 26.0f, 2100.0f, y0, 26.0f };
        lt_push(s, &cap, a, 1);
        lt_push(s, &cap, b, 1);
    }
    lt_bounds(s);
}

/* the awkward cases: axis-aligned boxes stacked and touching (coplanar,
 * shared faces), every box's triangles duplicated once, zero-area slivers
 * and needles, and long thin triangles crossing everything */
static void lt_rooms(LtScene *s)
{
    static const int q[6][4] = { {0,1,3,2}, {4,6,7,5}, {0,4,5,1}, {2,3,7,6}, {0,2,6,4}, {1,5,7,3} };
    int i, f, k, cap = 0;
    memset(s, 0, sizeof *s);
    s->name = "rooms";
    lt_rng = 0x55AA55AAull;
    for (i = 0; i < 700; i++) {
        float lo[3], c[8][3];
        int gx = i % 10, gy = (i / 10) % 10, gz = i / 100;
        for (k = 0; k < 3; k++)
            lo[k] = 500.0f + 4.0f * (k == 0 ? gx : k == 1 ? gy : gz);
        for (k = 0; k < 8; k++) {
            c[k][0] = lo[0] + ((k & 1) ? 4.0f : 0.0f);
            c[k][1] = lo[1] + ((k & 2) ? 4.0f : 0.0f);
            c[k][2] = lo[2] + ((k & 4) ? 4.0f : 0.0f);
        }
        for (f = 0; f < 6; f++) {
            float a[9], b[9];
            memcpy(a, c[q[f][0]], 12); memcpy(a + 3, c[q[f][1]], 12); memcpy(a + 6, c[q[f][2]], 12);
            memcpy(b, c[q[f][0]], 12); memcpy(b + 3, c[q[f][2]], 12); memcpy(b + 6, c[q[f][3]], 12);
            lt_push(s, &cap, a, i);
            lt_push(s, &cap, b, i);
            if (i % 2 == 0) {        /* duplicates: two triangles at every t */
                lt_push(s, &cap, a, i);
                lt_push(s, &cap, b, i);
            }
        }
    }
    for (i = 0; i < 500; i++) {
        float p[3], t[9];
        for (k = 0; k < 3; k++)
            p[k] = 500.0f + 40.0f * lt_rand();
        for (k = 0; k < 3; k++) {        /* a needle: three collinear corners */
            float dk = lt_rand() - 0.5f;
            t[k] = p[k];
            t[3 + k] = p[k] + dk;
            t[6 + k] = p[k] + 2.0f * dk;
        }
        lt_push(s, &cap, t, 1000);
        for (k = 0; k < 9; k++)          /* a point */
            t[k] = p[k % 3];
        lt_push(s, &cap, t, 1001);
        for (k = 0; k < 3; k++) {        /* a sliver 40 m long and a millimetre wide */
            t[k] = 500.0f + 40.0f * lt_rand();
            t[3 + k] = 500.0f + 40.0f * lt_rand();
            t[6 + k] = t[k] + 0.001f * lt_rand();
        }
        lt_push(s, &cap, t, 1002);
    }
    lt_bounds(s);
}

static void lt_sphere(float d[3])
{
    float z = 2.0f * lt_rand() - 1.0f, a = 6.2831853f * lt_rand(), r = sqrtf(fmaxf(0.0f, 1.0f - z * z));
    d[0] = r * cosf(a);
    d[1] = r * sinf(a);
    d[2] = z;
}

/* set 0: origins anywhere in the scene's box grown by a tenth, directions
 * uniform on the sphere, unbounded.
 * set 1: from a point on the surface (area weighted), cosine weighted about
 * the triangle's normal on a random side: how GI's rays leave a hit; tmin a
 * millimetre (the origin's rounding at 2 km is a tenth of that).
 * Both then get, for the any-hit comparison, a second tmax (see bvh_test.c). */
static LtRay *lt_rays(const LtScene *s, int set, int n, uint64_t seed)
{
    LtRay *r = malloc((size_t)n * sizeof *r);
    float *cdf = NULL;
    int i, k;
    lt_rng = seed;
    if (set == 1) {
        double acc = 0.0;
        cdf = malloc((size_t)s->ntri * sizeof *cdf);
        for (i = 0; i < s->ntri; i++) {
            const float *v = s->tri + 9 * i;
            float e1[3], e2[3], c[3];
            for (k = 0; k < 3; k++) {
                e1[k] = v[3 + k] - v[k];
                e2[k] = v[6 + k] - v[k];
            }
            c[0] = e1[1] * e2[2] - e1[2] * e2[1];
            c[1] = e1[2] * e2[0] - e1[0] * e2[2];
            c[2] = e1[0] * e2[1] - e1[1] * e2[0];
            acc += sqrt((double)c[0] * c[0] + (double)c[1] * c[1] + (double)c[2] * c[2]);
            cdf[i] = (float)acc;
        }
        for (i = 0; i < s->ntri; i++)
            cdf[i] /= (float)acc;
    }
    for (i = 0; i < n; i++) {
        if (set == 0) {
            for (k = 0; k < 3; k++) {
                float ext = s->hi[k] - s->lo[k];
                r[i].o[k] = s->lo[k] - 0.05f * ext + 1.1f * ext * lt_rand();
            }
            lt_sphere(r[i].d);
            r[i].tmin = 0.0f;
        } else {
            float x = lt_rand(), a = lt_rand(), b = lt_rand(), nrm[3], e1[3], e2[3], len, t1[3], t2[3];
            float ph, ct, st;
            int lo = 0, hi = s->ntri - 1;
            const float *v;
            while (lo < hi) {
                int m = (lo + hi) / 2;
                if (cdf[m] < x) lo = m + 1; else hi = m;
            }
            v = s->tri + 9 * lo;
            if (a + b > 1.0f) {
                a = 1.0f - a;
                b = 1.0f - b;
            }
            for (k = 0; k < 3; k++) {
                e1[k] = v[3 + k] - v[k];
                e2[k] = v[6 + k] - v[k];
                r[i].o[k] = v[k] + a * e1[k] + b * e2[k];
            }
            nrm[0] = e1[1] * e2[2] - e1[2] * e2[1];
            nrm[1] = e1[2] * e2[0] - e1[0] * e2[2];
            nrm[2] = e1[0] * e2[1] - e1[1] * e2[0];
            len = sqrtf(nrm[0] * nrm[0] + nrm[1] * nrm[1] + nrm[2] * nrm[2]);
            if (len == 0.0f) {
                lt_sphere(nrm);
                len = 1.0f;
            }
            if (lt_rand() < 0.5f)
                len = -len;
            for (k = 0; k < 3; k++)
                nrm[k] /= len;
            /* a frame about the normal (Duff et al. 2017) */
            {
                float sg = nrm[2] >= 0.0f ? 1.0f : -1.0f, aa = -1.0f / (sg + nrm[2]), bb = nrm[0] * nrm[1] * aa;
                t1[0] = 1.0f + sg * nrm[0] * nrm[0] * aa; t1[1] = sg * bb; t1[2] = -sg * nrm[0];
                t2[0] = bb; t2[1] = sg + nrm[1] * nrm[1] * aa; t2[2] = -nrm[1];
            }
            ph = 6.2831853f * lt_rand();
            st = sqrtf(lt_rand());
            ct = sqrtf(fmaxf(0.0f, 1.0f - st * st));
            for (k = 0; k < 3; k++)
                r[i].d[k] = st * cosf(ph) * t1[k] + st * sinf(ph) * t2[k] + ct * nrm[k];
            r[i].tmin = 1e-3f;
        }
        r[i].tmax = INFINITY;
    }
    free(cdf);
    return r;
}

/* the any-hit comparison's bound for ray i, from its closest hit (t, or
 * INFINITY for none): half to one and a half times the hit's distance, so
 * about half the segments reach it; a missing ray gets that of the scene's
 * diagonal */
static float lt_shadow_tmax(const LtScene *s, int i, float t)
{
    uint32_t h = (uint32_t)i * 2654435761u;
    float f;
    h ^= h >> 16;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    f = 0.5f + (float)(h >> 8) * (1.0f / 16777216.0f);
    if (!(t < INFINITY)) {
        float dx = s->hi[0] - s->lo[0], dy = s->hi[1] - s->lo[1], dz = s->hi[2] - s->lo[2];
        t = sqrtf(dx * dx + dy * dy + dz * dz);
    }
    return t * f;
}

#endif
