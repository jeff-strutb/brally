/* cluster_test.c: the many-lights code (platform/light/light_cluster.c)
 * against brute force, on synthetic scenes: no game, no GPU.
 *
 *   clusters   random cameras, lights and points: every light that reaches a
 *              point (light_influence > 0, the shaders' formula) is in the
 *              list of the point's cluster (none missing, no tolerance); the
 *              share of listed lights that do not reach the point (false
 *              positives), per point and per cluster
 *   shadows    a point in a lamp's cone lands inside its atlas tile; depth
 *              grows with distance; an occluder between a lamp and the
 *              ground shadows exactly the ground behind it (the shader's
 *              test, in C, on a depth tile rasterised here); the importance
 *              allocator places 80 (and 128) lights in the atlas without
 *              overlap; the cache redraws only what changed; the track's
 *              casters for a tile miss no triangle the tile sees
 *   grid       the 20-car night grid at 1920 x 1080: lights a pixel visits
 *
 * Build and run: ports/brally/tools/lighttest/run.sh */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "scene.h"

static int g_fail;
#define CHECK(c, ...)                                                                    \
    do {                                                                                 \
        if (!(c)) {                                                                      \
            g_fail++;                                                                    \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                                  \
            printf(__VA_ARGS__);                                                         \
            printf("\n");                                                                \
        }                                                                                \
    } while (0)

static double now_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1e3 + t.tv_nsec / 1e6;
}

static int listed(const LightSet *s, int c, int li)
{
    uint32_t e = s->cl[c], o = e >> 8, n = e & 255u, k;
    for (k = 0; k < n; k++)
        if (s->idx[o + k] == li)
            return 1;
    return 0;
}

static LightSet g_set;

/* ---- clusters ------------------------------------------------------------------- */
static void test_clusters(void)
{
    const int trials = 300, npts = 20000;
    long checked = 0, reach = 0, fn = 0, listed_n = 0, fp = 0;
    double tbuild = 0;
    long entries = 0, cand = 0, refined = 0;
    int t, maxlist = 0;
    for (t = 0; t < trials; t++) {
        Cam c;
        float eye[3] = { rndf(-1000, 1000), rndf(-1000, 1000), rndf(0.3f, 30.0f) }, fwd[3] = { rndf(-1, 1), rndf(-1, 1), rndf(-0.5f, 0.3f) };
        int n = t % 3 == 0 ? LC_LIGHTS : 1 + (int)(rnd() * LC_LIGHTS), i, k;
        LightClusterStats st;
        double t0;
        cam_make(&c, eye, fwd, rndf(35, 95), t & 1 ? 16.0f / 9.0f : rndf(1.0f, 2.4f), rndf(0.05f, 1.0f), 3000.0f);
        for (i = 0; i < n; i++)
            light_random(&g_set.l[i], &c);
        t0 = now_ms();
        light_cluster_build(&g_set, n, c.vp, c.eye, &st);
        tbuild += now_ms() - t0;
        entries += st.entries;
        cand += st.cand;
        refined += st.refined;
        if (st.maxlist > maxlist)
            maxlist = st.maxlist;
        for (k = 0; k < npts; k++) {
            float p[3], w;
            int cl;
            if (k & 1)
                cam_point(&c, rndf(-1.0f, 1.0f), rndf(-1.0f, 1.0f), 0.05f * powf(2000.0f / 0.05f, (float)rnd()), p);
            else
                point_near(&g_set.l[(int)(rnd() * n)], p);
            w = c.vp[3] * p[0] + c.vp[7] * p[1] + c.vp[11] * p[2] + c.vp[15];
            if (!(w > 0.0f))
                continue;               /* behind the eye: no pixel there */
            cl = light_cluster_of(&g_set, p);
            checked++;
            for (i = 0; i < n; i++) {
                int in = light_influence(&g_set.l[i], p) > 0.0f, li = listed(&g_set, cl, i);
                reach += in;
                listed_n += li;
                if (in && !li) {
                    if (fn < 10)
                        printf("  missing: trial %d point %d light %d (kind %g) cluster %d\n", t, k, i, g_set.l[i].kind, cl);
                    fn++;
                }
                if (li && !in)
                    fp++;
            }
        }
    }
    printf("clusters: %d cameras, %ld points, %ld (point, light) pairs reaching, %ld missing from the point's cluster\n",
           trials, checked, reach, fn);
    printf("  listed per point %.2f, of them reaching %.2f: false positives %.1f%% of listed (point level)\n",
           (double)listed_n / checked, (double)reach / checked, 100.0 * fp / (listed_n ? listed_n : 1));
    printf("  build %.3f ms a frame (mean, up to 128 lights), %.0f entries a frame, longest list %d; "
           "sphere test dropped %.1f%% of the plane candidates\n",
           tbuild / trials, (double)entries / trials, maxlist, 100.0 * refined / (cand ? cand : 1));
    CHECK(fn == 0, "%ld lights reach a point but are not in its cluster", fn);
}

/* per cluster: sample many points inside some clusters; a listed light that
 * reaches none of them is a cluster-level false positive (an upper bound:
 * the samples can miss a corner it reaches) */
static void test_cluster_fp(void)
{
    const int trials = 40, per = 400, samples = 512;
    long listed_n = 0, fp = 0;
    int t;
    for (t = 0; t < trials; t++) {
        Cam c;
        float eye[3] = { 0, 0, 1.5f }, fwd[3] = { 1, rndf(-0.3f, 0.3f), -0.05f };
        int n = 96, i, q;
        cam_make(&c, eye, fwd, 60, 16.0f / 9.0f, 0.1f, 3000.0f);
        for (i = 0; i < n; i++)
            light_random(&g_set.l[i], &c);
        light_cluster_build(&g_set, n, c.vp, c.eye, NULL);
        for (q = 0; q < per; q++) {
            int x = (int)(rnd() * LC_TX), y = (int)(rnd() * LC_TY), z = 1 + (int)(rnd() * (LC_TZ - 2));
            int cl = (z * LC_TY + y) * LC_TX + x, k, s;
            uint32_t e = g_set.cl[cl];
            float z0 = g_set.h.zb[z], z1 = g_set.h.zb[z + 1];
            uint8_t hit[LC_LIGHTS];
            memset(hit, 0, sizeof hit);
            for (s = 0; s < samples; s++) {
                float p[3];
                cam_point(&c, -1.0f + 2.0f * (x + (float)rnd()) / LC_TX, -1.0f + 2.0f * (y + (float)rnd()) / LC_TY,
                          z0 + (z1 - z0) * (float)rnd(), p);
                for (k = 0; k < (int)(e & 255u); k++) {
                    int li = g_set.idx[(e >> 8) + (uint32_t)k];
                    if (!hit[li] && light_influence(&g_set.l[li], p) > 0.0f)
                        hit[li] = 1;
                }
            }
            for (k = 0; k < (int)(e & 255u); k++) {
                listed_n++;
                fp += !hit[g_set.idx[(e >> 8) + (uint32_t)k]];
            }
        }
    }
    printf("  cluster level: %ld listings in %d closed clusters, %.1f%% reach none of %d samples in the cluster\n",
           listed_n, trials * per, 100.0 * fp / (listed_n ? listed_n : 1), samples);
}

/* ---- shadows -------------------------------------------------------------------- */
static void clipof(const LightShadowRec *r, const float p[3], float c[4])
{
    int k;
    for (k = 0; k < 4; k++)
        c[k] = r->m[k] * p[0] + r->m[4 + k] * p[1] + r->m[8 + k] * p[2] + r->m[12 + k];
}

static void test_shadow_projection(void)
{
    long pts = 0, bad_tile = 0, bad_mono = 0, rays = 0;
    int t;
    for (t = 0; t < 2000; t++) {
        LightSpot l;
        LightShadowRec r;
        Cam c;
        float eye[3] = { 0, 0, 0 }, fwd[3] = { 1, 0, 0 }, u0 = (float)(int)(rnd() * 8) / 8, v0 = (float)(int)(rnd() * 8) / 8, us = 1.0f / 8;
        int k;
        cam_make(&c, eye, fwd, 60, 1.5f, 0.1f, 1000);
        do
            light_random(&l, &c);
        while (!light_shadow_matrix(&l, &r));
        for (k = 0; k < 200; k++) {
            float p[3], cc[4], nx, ny, st[2];
            point_near(&l, p);
            {
                float d[3] = { p[0] - l.pos[0], p[1] - l.pos[1], p[2] - l.pos[2] }, dl = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
                float al = (d[0] * l.axis[0] + d[1] * l.axis[1] + d[2] * l.axis[2]) / dl;
                if (!(al > l.cosout) || dl >= l.range || al * dl <= LS_NEARP)
                    continue;           /* outside the cone, the range, or nearer than the near plane */
            }
            clipof(&r, p, cc);
            nx = cc[0] / cc[3];
            ny = cc[1] / cc[3];
            st[0] = u0 + (nx * 0.5f + 0.5f) * us;
            st[1] = v0 + (0.5f - ny * 0.5f) * us;
            pts++;
            if (!(fabsf(nx) <= 1.0f && fabsf(ny) <= 1.0f && cc[2] / cc[3] >= 0.0f && cc[2] / cc[3] <= 1.0f + 1e-6f
                  && st[0] >= u0 && st[0] <= u0 + us && st[1] >= v0 && st[1] <= v0 + us))
                bad_tile++;
        }
        for (k = 0; k < 20; k++) {      /* depth along a ray from the lamp */
            float p0[3], prev = -1e30f, prevd = 0;
            int j;
            point_near(&l, p0);
            for (j = 1; j <= 64; j++) {
                float f = (float)j / 64.0f, p[3], cc[4], d;
                int a;
                for (a = 0; a < 3; a++)
                    p[a] = l.pos[a] + (p0[a] - l.pos[a]) * f * 1.5f;
                clipof(&r, p, cc);
                if (cc[3] <= LS_NEARP)
                    continue;
                d = cc[2] / cc[3];
                if (prev > -1e29f && !(d > prev) && cc[3] > prevd * 1.0001f)
                    bad_mono++;
                prev = d;
                prevd = cc[3];
            }
            rays++;
        }
    }
    printf("shadows: %ld points in lamps' cones: %ld outside their atlas tile; %ld rays: %ld depth steps not increasing\n",
           pts, bad_tile, rays, bad_mono);
    CHECK(bad_tile == 0, "%ld cone points outside the tile", bad_tile);
    CHECK(bad_mono == 0, "%ld non-monotonic depth steps", bad_mono);
}

/* the shader's lampvis, in C, over a depth tile (atlas texels, row 0 at the top) */
static float sample_cmp(const float *dep, int asz, float s, float t, float ref)
{
    float x = s * asz - 0.5f, y = t * asz - 0.5f, fx, fy, v = 0;
    int x0 = (int)floorf(x), y0 = (int)floorf(y), i, j;
    fx = x - x0;
    fy = y - y0;
    for (j = 0; j < 2; j++)
        for (i = 0; i < 2; i++) {
            int xx = x0 + i < 0 ? 0 : x0 + i >= asz ? asz - 1 : x0 + i, yy = y0 + j < 0 ? 0 : y0 + j >= asz ? asz - 1 : y0 + j;
            float w = (i ? fx : 1 - fx) * (j ? fy : 1 - fy);
            v += w * (ref <= dep[yy * asz + xx] ? 1.0f : 0.0f);
        }
    return v;
}
static float lampvis_c(const LightShadowRec *h, const float *dep, int asz, const float p[3], const float n[3], const float ld[3])
{
    float c0[4], c[4], q[3], tx, nl, tn, nd[2], st[2], e, zr, vis = 0;
    int k, x, y;
    if (h->rect[3] < 0.5f)
        return 1;
    clipof(h, p, c0);
    if (c0[3] <= h->prm[3])
        return 1;
    tx = 2.0f * c0[3] * h->prm[2] / (h->rect[2] * asz);
    nl = fminf(fmaxf(n[0] * ld[0] + n[1] * ld[1] + n[2] * ld[2], 0.05f), 1.0f);
    tn = fminf(sqrtf(1 - nl * nl) / nl, 8.0f);
    for (k = 0; k < 3; k++)
        q[k] = p[k] + n[k] * tx * (1.0f + 2.0f * (1.0f - nl));
    clipof(h, q, c);
    nd[0] = c[0] / c[3];
    nd[1] = c[1] / c[3];
    if (c[3] <= h->prm[3] || fabsf(nd[0]) >= 1 || fabsf(nd[1]) >= 1)
        return 1;
    e = 1.5f / asz;
    st[0] = fminf(fmaxf(h->rect[0] + (nd[0] * 0.5f + 0.5f) * h->rect[2], h->rect[0] + e), h->rect[0] + h->rect[2] - e);
    st[1] = fminf(fmaxf(h->rect[1] + (0.5f - nd[1] * 0.5f) * h->rect[2], h->rect[1] + e), h->rect[1] + h->rect[2] - e);
    zr = h->prm[0] + h->prm[1] / fmaxf(c[3] - tx * (1 + tn), h->prm[3]);
    for (y = -1; y <= 1; y++)
        for (x = -1; x <= 1; x++)
            vis += sample_cmp(dep, asz, st[0] + x / (float)asz, st[1] + y / (float)asz, zr);
    return vis / 9;
}

/* the occluder scene, shared with the Metal test: a spot 1 m up looking down
 * the x axis, a 2 x 2 m board standing at x = 10, the ground z = 0 */
static const float k_occ[2][9] = { { 10, -1, 0, 10, 1, 0, 10, 1, 2 }, { 10, -1, 0, 10, 1, 2, 10, -1, 2 } };
static const float k_gnd[2][9] = { { -5, -60, 0, 80, -60, 0, 80, 60, 0 }, { -5, -60, 0, 80, 60, 0, -5, 60, 0 } };

/* the depth a ray from the lamp through texel centre (px, py) of the tile meets first (z/w), 1 for none */
static float ray_depth(const LightSpot *l, const LightShadowRec *r, float nx, float ny)
{
    float f[3], rt[3], u[3], dir[3], best = 1e30f;
    int k, t;
    for (k = 0; k < 3; k++) {          /* the tile's basis back from its matrix rows */
        rt[k] = r->m[k * 4 + 0] * r->prm[2];
        u[k] = r->m[k * 4 + 1] * r->prm[2];
        f[k] = r->m[k * 4 + 3];
    }
    for (k = 0; k < 3; k++)
        dir[k] = f[k] + nx * r->prm[2] * rt[k] + ny * r->prm[2] * u[k];
    for (t = 0; t < 4; t++) {          /* ray and triangle, Moller and Trumbore; depth along the axis */
        const float *v = t < 2 ? k_occ[t] : k_gnd[t - 2];
        float e1[3], e2[3], pv[3], tv[3], qv[3], det, a, b, s;
        for (k = 0; k < 3; k++) {
            e1[k] = v[3 + k] - v[k];
            e2[k] = v[6 + k] - v[k];
            tv[k] = l->pos[k] - v[k];
        }
        cross3(dir, e2, pv);
        det = e1[0] * pv[0] + e1[1] * pv[1] + e1[2] * pv[2];
        if (fabsf(det) < 1e-12f)
            continue;
        a = (tv[0] * pv[0] + tv[1] * pv[1] + tv[2] * pv[2]) / det;
        cross3(tv, e1, qv);
        b = (dir[0] * qv[0] + dir[1] * qv[1] + dir[2] * qv[2]) / det;
        s = (e2[0] * qv[0] + e2[1] * qv[1] + e2[2] * qv[2]) / det;   /* dir has unit axis component: s is the depth */
        if (a < 0 || b < 0 || a + b > 1 || s <= r->prm[3] || s >= l->range)
            continue;
        best = fminf(best, s);
    }
    return best > 1e29f ? 1.0f : r->prm[0] + r->prm[1] / best;
}

static void test_shadow_occluder(void)
{
    const int asz = 1024, tsz = 512, tx0 = 512, ty0 = 256;
    float *dep = malloc((size_t)asz * asz * sizeof(float));
    LightSpot l;
    LightShadowRec r;
    int x, y, n = 0, nshad = 0, nlit = 0, bad = 0;
    memset(&l, 0, sizeof l);
    l.pos[2] = 1.0f;
    l.axis[0] = 1;
    l.side[1] = -1;
    l.kind = LIGHT_SPOT;
    l.candela = 1000;
    l.range = 60;
    l.cosout = cosf(50.0f * 3.14159265f / 180.0f);
    l.side[0] = cosf(40.0f * 3.14159265f / 180.0f);
    light_shadow_matrix(&l, &r);
    r.rect[0] = (float)tx0 / asz;
    r.rect[1] = (float)ty0 / asz;
    r.rect[2] = (float)tsz / asz;
    r.rect[3] = 1;
    for (y = 0; y < asz * asz; y++)
        dep[y] = 1.0f;
    for (y = 0; y < tsz; y++)
        for (x = 0; x < tsz; x++)
            dep[(ty0 + y) * asz + tx0 + x] = ray_depth(&l, &r, (x + 0.5f) / tsz * 2 - 1, 1 - (y + 0.5f) / tsz * 2);
    /* the ground from x 1 to 55, y -12..12: behind the board's shadow is where
       the line to the lamp crosses x = 10 within the board: |y| 10 / x <= 1 */
    for (y = 0; y < 240; y++)
        for (x = 0; x < 540; x++) {
            float p[3] = { 1.0f + x * 0.1f, -12.0f + y * 0.1f, 0.0f }, nn[3] = { 0, 0, 1 }, ld[3], dl, v, yc, texel, d;
            int k, sh;
            for (k = 0; k < 3; k++)
                ld[k] = l.pos[k] - p[k];
            dl = sqrtf(ld[0] * ld[0] + ld[1] * ld[1] + ld[2] * ld[2]);
            for (k = 0; k < 3; k++)
                ld[k] /= dl;
            if (light_influence(&l, p) <= 0.0f)
                continue;
            d = p[0];
            yc = fabsf(p[1]) * 10.0f / d;
            sh = p[0] > 10.0f && yc <= 1.0f;
            /* the penumbra: within three of the tile's texels of the board's edge at the board */
            texel = 2.0f * 10.0f * r.prm[2] / tsz;
            if (p[0] > 10.0f && fabsf(yc - 1.0f) < 3.0f * texel)
                continue;
            if (fabsf(p[0] - 10.0f) < 0.6f)
                continue;               /* the board's foot: the bias's contact gap */
            v = lampvis_c(&r, dep, asz, p, nn, ld);
            n++;
            if (sh) {
                nshad++;
                bad += v > 0.05f;
            } else {
                nlit++;
                bad += v < 0.95f;
            }
        }
    printf("  occluder: %d ground points the spot reaches (%d behind the board, %d in the open): %d misclassified "
           "(behind: vis > 0.05, open: vis < 0.95), the 3-texel penumbra and the board's foot left out\n", n, nshad, nlit, bad);
    CHECK(bad == 0, "%d ground points shadowed wrongly", bad);
    CHECK(nshad > 1000 && nlit > 1000, "too few samples");
    free(dep);
}

static void check_plan(const LightShadowPlan *p, int atlas, const char *what)
{
    int i, j, bad = 0;
    for (i = 0; i < p->n; i++) {
        const LightShadowTile *a = &p->t[i];
        if (a->size < LS_TILE_MIN || a->size > LS_TILE_MAX || (a->size & (a->size - 1)) || a->x < 0 || a->y < 0
            || a->x + a->size > atlas || a->y + a->size > atlas || a->x % a->size || a->y % a->size)
            bad++;
        for (j = 0; j < i; j++) {
            const LightShadowTile *b = &p->t[j];
            if (a->x < b->x + b->size && b->x < a->x + a->size && a->y < b->y + b->size && b->y < a->y + a->size)
                bad++;
        }
    }
    CHECK(bad == 0, "%s: %d tiles overlap or leave the atlas", what, bad);
}

static void test_alloc(void)
{
    LightClusterStats st;
    LightShadowPlan p;
    int ids[LC_LIGHTS], i, n, hist[4] = { 0, 0, 0, 0 };
    float eye[3] = { 0, 0, 1.5f };
    long budget;
    for (n = 0; n < 80; n++) {
        LightSpot *l = &g_set.l[n];
        memset(l, 0, sizeof *l);
        l->kind = n % 2 ? LIGHT_SPOT : LIGHT_HEAD;
        l->pos[0] = rndf(-100, 100);
        l->pos[1] = rndf(-100, 100);
        l->pos[2] = 0.6f;
        l->axis[0] = 1;
        l->side[1] = -1;
        l->candela = 20000;
        l->range = 150;
        l->cosout = 0.3420201f;
        ids[n] = n;
    }
    memset(&st, 0, sizeof st);
    for (i = 0; i < 80; i++)
        st.cov[i] = (float)pow(rnd(), 3.0);   /* most lights small on screen, a few large */
    g_set.h.eye[3] = 80;
    light_shadow_reset();
    light_shadow_plan(&g_set, ids, &st, eye, NULL, 0, 4096, 1L << 40, &p);
    check_plan(&p, 4096, "80 lights, 4096");
    for (i = 0; i < p.n; i++)
        hist[p.t[i].size == 1024 ? 0 : p.t[i].size == 512 ? 1 : p.t[i].size == 256 ? 2 : 3]++;
    printf("  allocator: 80 lamps in a 4096^2 atlas: %d tiles (1024: %d, 512: %d, 256: %d, 128: %d), %d dropped, "
           "%ld texels drawn\n", p.n, hist[0], hist[1], hist[2], hist[3], p.dropped, p.texels);
    CHECK(p.n == 80 && p.dropped == 0, "80 lamps: %d placed, %d dropped", p.n, p.dropped);
    /* the same frame again: nothing to draw; one lamp moves: only it */
    light_shadow_plan(&g_set, ids, &st, eye, NULL, 0, 4096, 1L << 40, &p);
    CHECK(p.rendered == 0 && p.cached == 80, "still frame: %d drawn, %d cached", p.rendered, p.cached);
    g_set.l[7].pos[0] += 0.01f;
    light_shadow_plan(&g_set, ids, &st, eye, NULL, 0, 4096, 1L << 40, &p);
    CHECK(p.rendered == 1 && p.cached == 79, "one lamp moved: %d drawn, %d cached", p.rendered, p.cached);
    printf("  cache: a still frame draws %d tiles; one lamp moved, %d\n", 0, p.rendered);
    /* a moving caster near one lamp redraws it alone */
    {
        float tri[9] = { g_set.l[3].pos[0] + 5, g_set.l[3].pos[1], 0, g_set.l[3].pos[0] + 5, g_set.l[3].pos[1] + 1, 0,
                         g_set.l[3].pos[0] + 5, g_set.l[3].pos[1], 1 };
        int near = 0;
        for (i = 0; i < 80; i++) {      /* the caster's box meets the lamp's (its position +- its range) */
            const LightSpot *l = &g_set.l[i];
            if (!(tri[0] > l->pos[0] + l->range || tri[0] < l->pos[0] - l->range || tri[1] > l->pos[1] + l->range
                  || tri[4] < l->pos[1] - l->range))
                near++;
        }
        light_shadow_plan(&g_set, ids, &st, eye, tri, 3, 4096, 1L << 40, &p);
        CHECK(p.rendered == near, "a caster within %d lamps' reach: %d drawn", near, p.rendered);
    }
    /* a budget: the most important drawn, the rest kept (stale; the moving casters as they
       were, none, after a frame that draws all with the caster gone) */
    light_shadow_plan(&g_set, ids, &st, eye, NULL, 0, 4096, 1L << 40, &p);
    for (i = 0; i < 80; i++)
        g_set.l[i].pos[2] += 0.01f;
    budget = 4L * 1024 * 1024;
    light_shadow_plan(&g_set, ids, &st, eye, NULL, 0, 4096, budget, &p);
    CHECK(p.texels <= budget && p.rendered > 0 && p.stale == 80 - p.rendered, "budget: %ld texels, %d drawn, %d stale",
          p.texels, p.rendered, p.stale);
    printf("  budget %ld texels: %d tiles drawn (%ld texels), %d kept from last frame\n", budget, p.rendered, p.texels, p.stale);
    /* over budget with a moving caster near every lamp: no stale tile (it would shadow the
       moved caster by where it was), the rest unshadowed this frame */
    {
        static const float big[9] = { -1e4f, -1e4f, 0, 1e4f, -1e4f, 0, 0, 1e4f, 1 };
        for (i = 0; i < 80; i++)
            g_set.l[i].pos[2] += 0.01f;
        light_shadow_plan(&g_set, ids, &st, eye, big, 3, 4096, budget, &p);
        CHECK(p.stale == 0 && p.dropped == 80 - p.rendered, "moved casters over budget: %d drawn, %d stale, %d dropped",
              p.rendered, p.stale, p.dropped);
        printf("  over budget, casters moved: %d drawn, %d stale, %d unshadowed\n", p.rendered, p.stale, p.dropped);
    }
    /* 80 lamps in a 2048^2 atlas (the iPhone's) */
    light_shadow_reset();
    light_shadow_plan(&g_set, ids, &st, eye, NULL, 0, 2048, 1L << 40, &p);
    check_plan(&p, 2048, "80 lights, 2048");
    CHECK(p.n == 80 && p.dropped == 0, "80 lamps at 2048: %d placed", p.n);
    printf("  80 lamps in a 2048^2 atlas: %d tiles, %d dropped\n", p.n, p.dropped);
    /* 128 lamps, each over the whole screen (each wants 1024) */
    for (n = 80; n < 128; n++) {
        g_set.l[n] = g_set.l[n - 80];
        g_set.l[n].pos[1] += 3;
        ids[n] = n;
    }
    for (i = 0; i < 128; i++)
        st.cov[i] = 1.0f;
    g_set.h.eye[3] = 128;
    light_shadow_reset();
    light_shadow_plan(&g_set, ids, &st, eye, NULL, 0, 4096, 1L << 40, &p);
    check_plan(&p, 4096, "128 lights, 4096");
    memset(hist, 0, sizeof hist);
    for (i = 0; i < p.n; i++)
        hist[p.t[i].size == 1024 ? 0 : p.t[i].size == 512 ? 1 : p.t[i].size == 256 ? 2 : 3]++;
    printf("  128 lamps all wanting 1024: %d tiles (1024: %d, 512: %d, 256: %d, 128: %d), %d dropped\n", p.n, hist[0], hist[1],
           hist[2], hist[3], p.dropped);
    CHECK(p.n == 128, "128 lamps: %d placed", p.n);
    /* repeated frames with lights coming and going: always packed, never overlapping */
    {
        int f, bad = 0;
        for (f = 0; f < 500; f++) {
            int m = 20 + (int)(rnd() * 108);
            for (i = 0; i < m; i++) {
                ids[i] = (int)(rnd() * 200);
                st.cov[i] = (float)pow(rnd(), 2.0);
            }
            g_set.h.eye[3] = (float)m;
            light_shadow_plan(&g_set, ids, &st, eye, NULL, 0, f & 1 ? 4096 : 2048, 1L << 40, &p);
            check_plan(&p, f & 1 ? 4096 : 2048, "churn");
            bad += p.n != m;
        }
        CHECK(bad == 0, "churn: %d frames left lamps out", bad);
        printf("  500 frames of 20..127 lamps coming and going: every lamp placed, no overlap\n");
    }
}

/* the track's casters: a lamp's tile must get every triangle it sees */
static void test_track_casters(void)
{
    const int ntri = 12000;
    float *tri = malloc((size_t)ntri * 9 * sizeof(float));
    uint32_t *out = malloc((size_t)ntri * 3 * sizeof(uint32_t));
    uint8_t *got = malloc((size_t)ntri);
    int i, t, missed = 0, seen = 0, total = 0;
    for (i = 0; i < ntri; i++) {
        float c[3] = { rndf(0, 2000), rndf(0, 2000), rndf(-3, 40) };
        int k, v;
        for (v = 0; v < 3; v++)
            for (k = 0; k < 3; k++)
                tri[9 * i + 3 * v + k] = c[k] + rndf(-12, 12);
    }
    light_shadow_track(tri, ntri, 1);
    for (t = 0; t < 200; t++) {
        LightSpot l;
        LightShadowRec r;
        int n, k;
        memset(&l, 0, sizeof l);
        l.pos[0] = rndf(100, 1900);
        l.pos[1] = rndf(100, 1900);
        l.pos[2] = rndf(0, 20);
        l.axis[0] = rndf(-1, 1);
        l.axis[1] = rndf(-1, 1);
        l.axis[2] = rndf(-0.4f, 0.2f);
        nrm3(l.axis);
        l.kind = LIGHT_HEAD;
        l.range = 150;
        l.cosout = 0.3420201f;
        light_shadow_matrix(&l, &r);
        n = light_shadow_track_casters(&r, &l, out, ntri * 3);
        memset(got, 0, (size_t)ntri);
        for (k = 0; k < n; k += 3)
            got[out[k] / 3] = 1;
        total += n / 3;
        for (i = 0; i < ntri; i++) {
            int a, b, in = 0;
            for (a = 0; a <= 8 && !in; a++)
                for (b = 0; a + b <= 8 && !in; b++) {
                    float p[3], c[4];
                    int q;
                    for (q = 0; q < 3; q++)
                        p[q] = tri[9 * i + q] + (tri[9 * i + 3 + q] - tri[9 * i + q]) * a / 8.0f + (tri[9 * i + 6 + q] - tri[9 * i + q]) * b / 8.0f;
                    /* where a caster can matter: in the lamp's reach (range and cone,
                       the region between the lamp and anything it lights) past the near plane */
                    clipof(&r, p, c);
                    {
                        float d[3] = { p[0] - l.pos[0], p[1] - l.pos[1], p[2] - l.pos[2] }, dl = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
                        in = c[3] > LS_NEARP && dl < l.range && (d[0] * l.axis[0] + d[1] * l.axis[1] + d[2] * l.axis[2]) > l.cosout * dl;
                    }
                }
            seen += in;
            if (in && !got[i])
                missed++;
        }
    }
    printf("  track casters: 200 lamps over %d triangles: %d triangles in their reach, %d missed; %.0f drawn a lamp\n",
           ntri, seen, missed, total / 200.0);
    CHECK(missed == 0, "%d triangles a tile sees were not drawn into it", missed);
    free(tri);
    free(out);
    free(got);
}

/* ---- the 20-car night grid ------------------------------------------------------- */
static void grid(void)
{
    const int W = 1920, H = 1080;
    Cam c;
    int ids[LC_LIGHTS], n, x, y, i, ground = 0;
    long visits = 0, reach = 0, rhead = 0, shad = 0, hist[9] = { 0 };
    int maxv = 0;
    LightClusterStats st;
    LightShadowPlan p;
    double t0;
    n = grid_scene(g_set.l, ids, &c, (float)W / H);
    t0 = now_ms();
    light_cluster_build(&g_set, n, c.vp, c.eye, &st);
    printf("grid: 20 cars, %d lights (40 low beams, 40 tail lamps): build %.3f ms; %d lights seen, %ld list entries, "
           "longest list %d\n", n, now_ms() - t0, st.visible, st.entries, st.maxlist);
    light_shadow_reset();
    t0 = now_ms();
    light_shadow_plan(&g_set, ids, &st, c.eye, NULL, 0, 4096, 1L << 40, &p);
    printf("  shadow plan %.3f ms: %d tiles (the low beams), %ld texels\n", now_ms() - t0, p.n, p.texels);
    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++) {
            float d[3], pt[3], nn[3] = { 0, 0, 1 }, t;
            int k, cl, v;
            uint32_t e;
            for (k = 0; k < 3; k++)
                d[k] = c.fwd[k] + ((x + 0.5f) / W * 2 - 1) * c.tx * c.right[k] + (1 - (y + 0.5f) / H * 2) * c.ty * c.up[k];
            if (d[2] >= 0)
                continue;               /* sky: the lighting pass has no surface there */
            t = -c.eye[2] / d[2];
            if (t > 2000)
                continue;
            for (k = 0; k < 3; k++)
                pt[k] = c.eye[k] + d[k] * t;
            ground++;
            cl = light_cluster_of(&g_set, pt);
            e = g_set.cl[cl];
            v = (int)(e & 255u);
            visits += v;
            if (v > maxv)
                maxv = v;
            hist[v >= 80 ? 8 : v / 10]++;
            for (k = 0; k < v; k++) {
                int li = g_set.idx[(e >> 8) + (uint32_t)k];
                float L[3] = { g_set.l[li].pos[0] - pt[0], g_set.l[li].pos[1] - pt[1], g_set.l[li].pos[2] - pt[2] };
                if (L[0] * nn[0] + L[1] * nn[1] + L[2] * nn[2] > 0 && light_influence(&g_set.l[li], pt) > 0) {
                    reach++;
                    rhead += g_set.l[li].kind == LIGHT_HEAD;
                    shad += g_set.s[li].rect[3] > 0.5f;
                }
            }
            for (i = 0; i < n; i++)                 /* the brute-force check, here too */
                if (light_influence(&g_set.l[i], pt) > 0 && !listed(&g_set, cl, i)) {
                    g_fail++;
                    printf("FAIL grid: light %d reaches pixel %d,%d but is not listed\n", i, x, y);
                    i = n;
                }
        }
    printf("  1920 x 1080, %d ground pixels: lights visited a pixel %.2f (max %d) instead of %d; reaching it %.2f "
           "(low beams %.2f, tail lamps %.2f), of them shadowed %.2f\n", ground, (double)visits / ground, maxv, n, (double)reach / ground,
           (double)rhead / ground, (double)(reach - rhead) / ground, (double)shad / ground);
    printf("  pixels by lights visited: ");
    for (i = 0; i < 9; i++)
        printf("%s%d%s %.1f%%", i ? ", " : "", i * 10, i < 8 ? "+" : "+", 100.0 * hist[i] / ground);
    printf("\n");
}

int main(void)
{
    test_clusters();
    test_cluster_fp();
    test_shadow_projection();
    test_shadow_occluder();
    test_alloc();
    test_track_casters();
    grid();
    printf(g_fail ? "lighttest: %d FAILED\n" : "lighttest: all passed\n", g_fail);
    return g_fail != 0;
}
