/* shadowtest.c: the sun's shadows checked by arithmetic, no game run
 * (ports/brally/tools/lighttest/run.sh builds and runs it).
 *
 *   penumbra   the shader's soft shadow (shadowcpu.h, its CPU twin) against
 *              the physics: a straight-edged blocker D above a plane under a
 *              sun 0.535 degrees across casts a penumbra 2 D tan(0.2675 deg)
 *              wide whose profile is the share of the sun's disc in view
 *   cache      the cascades' static cache (brr_shadow.h) against brute force:
 *              a simulated drive, every cascade's live map compared bit for
 *              bit with the scene drawn from scratch, every frame
 *   screen     brr_screen_matrix against the sky's camray, its inverse
 *   contact    the contact shadow against ray-traced truth
 *
 * Exits non-zero when a check fails. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "scenes.h"

static int s_fail;
#define CHECK(c, ...) do { if (!(c)) { s_fail++; printf("  FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* ---- penumbra ------------------------------------------------------------ */
#define PEN_ROT 64
#define PEN_X0 (-0.6f)
#define PEN_DX 0.0005f
#define PEN_NX 2401

/* the share of a disc of radius 1 left of h (the physical penumbra's profile) */
static double disc_cdf(double h)
{
    if (h <= -1) return 0;
    if (h >= 1) return 1;
    return 0.5 + (h * sqrt(1 - h * h) + asin(h)) / 3.14159265358979;
}
/* the physical profile seen through the hardware's bilinear comparison (a
 * one-texel box): the reference the filter should match */
static double pen_expect(double x, double r)
{
    double s = 0;
    int i;
    for (i = 0; i < 400; i++) {
        double o = ((i + 0.5) / 400.0 - 0.5) * PEN_TEXEL;
        s += r > 1e-9 ? disc_cdf((x - o) / r) : (x - o > 0 ? 1.0 : 0.0);
    }
    return s / 400.0;
}

static void test_penumbra(void)
{
    static const float Ds[] = { 0.05f, 2.0f, 5.0f, 10.0f, 20.0f, 40.0f, 60.0f };
    float *map = malloc((size_t)PEN_SIZE * PEN_SIZE * sizeof(float));
    float *vis = malloc((size_t)PEN_NX * PEN_ROT * sizeof(float));
    double tanh_ = tan(SUN_HALF_DEG * 3.14159265358979 / 180.0);
    DepthMap dm;
    LU u;
    int di;
    printf("penumbra: a blocker D above a plane, sun overhead, 2 cm texels, %d disc turns, x step %.1f mm\n", PEN_ROT, PEN_DX * 1000);
    printf("  %6s %9s %9s %9s %8s %9s %9s %7s %6s\n", "D m", "2Dtan mm", "supp mm", "width mm", "err tx", "rms", "max err", "noise", "drops");
    dm.d = map;
    dm.size = PEN_SIZE;
    for (di = 0; di < (int)(sizeof Ds / sizeof Ds[0]); di++) {
        float D = Ds[di], n[3] = { 0, 0, 1 };
        double w = 2.0 * D * tanh_, xa = 1e9, xb = 1e9, rms = 0, mx = 0, noise = 0, maxdrop = 0;
        int i, m, drops = 0, exact_bad = 0;
        pen_setup(D, &u, map);
        for (i = 0; i < PEN_NX; i++)
            for (m = 0; m < PEN_ROT; m++) {
                float p[3] = { PEN_X0 + PEN_DX * (float)i, 0.0037f, 0.0f };
                vis[i * PEN_ROT + m] = cpu_casvis(&u, &dm, p, n, 0, 6.2831853f * ((float)m + 0.5f) / PEN_ROT);
            }
        for (m = 0; m < PEN_ROT; m++) {          /* each turn of the disc alone: never darker further from the blocker */
            for (i = 1; i < PEN_NX; i++) {
                double d = vis[(i - 1) * PEN_ROT + m] - vis[i * PEN_ROT + m];
                if (d > 1e-6) {
                    drops++;
                    if (d > maxdrop) maxdrop = d;
                }
            }
        }
        for (i = 0; i < PEN_NX; i++) {
            double x = PEN_X0 + PEN_DX * i, e = 0, e2 = 0, ex;
            for (m = 0; m < PEN_ROT; m++) {
                e += vis[i * PEN_ROT + m];
                e2 += (double)vis[i * PEN_ROT + m] * vis[i * PEN_ROT + m];
            }
            e /= PEN_ROT;
            e2 = e2 / PEN_ROT - e * e;
            if (e2 > 0 && sqrt(e2) > noise) noise = sqrt(e2);
            if (e > 0 && xa > 1e8) xa = x;
            if (e >= 1.0 && xb > 1e8) xb = x;
            ex = pen_expect(x, D * tanh_);
            rms += (e - ex) * (e - ex);
            if (fabs(e - ex) > mx) mx = fabs(e - ex);
            if ((x <= -0.5 && e != 0.0) || (x >= 0.5 && e != 1.0))
                exact_bad++;
        }
        rms = sqrt(rms / PEN_NX);
        {
            double supp = xb - xa, width = supp - PEN_TEXEL, err = (width - w) / PEN_TEXEL;
            printf("  %6.2f %9.2f %9.1f %9.1f %8.2f %9.4f %9.4f %7.3f %6d\n", D, w * 1000, supp * 1000, width * 1000, err, rms, mx, noise, drops);
            CHECK(exact_bad == 0, "D %.2f: %d receivers well inside the shadow or the light not exactly 0 or 1", D, exact_bad);
            CHECK(drops == 0, "D %.2f: %d places the shadow darkens moving away from the blocker (largest %.4f)", D, drops, maxdrop);
            CHECK(fabs(err) <= 1.0, "D %.2f: penumbra %.1f mm against 2 D tan = %.1f mm: off by %.2f texels", D, width * 1000, w * 1000, err);
            if (D < 0.1f)
                CHECK(supp <= 1.1 * PEN_TEXEL, "contact: the edge spreads over %.2f texels", supp / PEN_TEXEL);
        }
    }
    free(map);
    free(vis);
}

/* ---- cache --------------------------------------------------------------- */
#define SIM_S 256
static unsigned s_rng = 12345u;
static float rnd(void) { s_rng = s_rng * 1664525u + 1013904223u; return (float)(s_rng >> 8) / 16777216.0f; }

typedef struct { float *v; size_t n, cap; } Tris;   /* n corners */
static void tri_add(Tris *t, const float a[3], const float b[3], const float c[3])
{
    if (t->n + 3 > t->cap) {
        t->cap = t->cap ? t->cap * 2 : 4096;
        t->v = realloc(t->v, t->cap * 3 * sizeof(float));
    }
    memcpy(&t->v[3 * t->n], a, 12);
    memcpy(&t->v[3 * t->n + 3], b, 12);
    memcpy(&t->v[3 * t->n + 6], c, 12);
    t->n += 3;
}
static void box_add(Tris *t, const float lo[3], const float hi[3])
{
    static const int f[6][4] = { { 0, 2, 6, 4 }, { 1, 5, 7, 3 }, { 0, 4, 5, 1 }, { 2, 3, 7, 6 }, { 0, 1, 3, 2 }, { 4, 6, 7, 5 } };
    float c[8][3];
    int i;
    for (i = 0; i < 8; i++) {
        c[i][0] = i & 1 ? hi[0] : lo[0];
        c[i][1] = i & 2 ? hi[1] : lo[1];
        c[i][2] = i & 4 ? hi[2] : lo[2];
    }
    for (i = 0; i < 6; i++) {
        tri_add(t, c[f[i][0]], c[f[i][1]], c[f[i][2]]);
        tri_add(t, c[f[i][0]], c[f[i][2]], c[f[i][3]]);
    }
}

/* depth-test rasterisation (less-equal, keep the nearer), texel centres
 * inside the triangle, depth 0..1 only (outside it is clipped), into the
 * window of the map's texels from (wx, wy), ww x wh, stored at stride
 * (a tile: the same texel grid, offset by whole texels).  The cached, the
 * tiled and the reference maps all go through this function: what is checked
 * is the bookkeeping, not a rasteriser's rules (gputest checks the GPU's tile
 * against its direct drawing) */
static void sim_raster_win(float *map, int stride, int wx, int wy, int ww, int wh, const float m[16], const Tris *t)
{
    size_t k;
    for (k = 0; k + 3 <= t->n; k += 3) {
        float X[3], Y[3], Z[3], area;
        int i, x0, x1, y0, y1, x, y;
        for (i = 0; i < 3; i++) {
            const float *p = &t->v[3 * (k + i)];
            X[i] = ((m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12]) * 0.5f + 0.5f) * SIM_S;
            Y[i] = (0.5f - (m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13]) * 0.5f) * SIM_S;
            Z[i] = m[2] * p[0] + m[6] * p[1] + m[10] * p[2] + m[14];
        }
        area = (X[1] - X[0]) * (Y[2] - Y[0]) - (X[2] - X[0]) * (Y[1] - Y[0]);
        if (area == 0)
            continue;
        x0 = (int)floorf(fminf(X[0], fminf(X[1], X[2])));
        x1 = (int)ceilf(fmaxf(X[0], fmaxf(X[1], X[2])));
        y0 = (int)floorf(fminf(Y[0], fminf(Y[1], Y[2])));
        y1 = (int)ceilf(fmaxf(Y[0], fmaxf(Y[1], Y[2])));
        if (x0 < wx) x0 = wx;
        if (y0 < wy) y0 = wy;
        if (x1 > wx + ww) x1 = wx + ww;
        if (y1 > wy + wh) y1 = wy + wh;
        for (y = y0; y < y1; y++)
            for (x = x0; x < x1; x++) {
                float px = x + 0.5f, py = y + 0.5f, z, *o = &map[(y - wy) * stride + (x - wx)];
                float w0 = ((X[2] - X[1]) * (py - Y[1]) - (Y[2] - Y[1]) * (px - X[1])) / area;
                float w1 = ((X[0] - X[2]) * (py - Y[2]) - (Y[0] - Y[2]) * (px - X[2])) / area;
                float w2 = 1.0f - w0 - w1;
                if (w0 < 0 || w1 < 0 || w2 < 0)
                    continue;
                z = w0 * Z[0] + w1 * Z[1] + w2 * Z[2];
                if (z >= 0 && z <= 1 && z <= *o)
                    *o = z;
            }
    }
}
static void sim_raster(float *map, const float m[16], const Tris *t) { sim_raster_win(map, SIM_S, 0, 0, SIM_S, SIM_S, m, t); }
static void fill1(float *m) { int i; for (i = 0; i < SIM_S * SIM_S; i++) m[i] = 1.0f; }

typedef struct {
    float eye[3], yaw, pitch;
} SimCam;

/* the view's corner rays and forward axis, as brr_metal.m derives them */
static void sim_rays(const SimCam *c, float ray[4][3], float fwd[3])
{
    float f[3] = { cosf(c->pitch) * cosf(c->yaw), cosf(c->pitch) * sinf(c->yaw), sinf(c->pitch) }, up[3] = { 0, 0, 1 }, r[3], v[3];
    float ty = tanf(0.5f * 1.0472f), tx = ty * 4.0f / 3.0f;
    int k, i;
    v3cross(f, up, r);
    v3norm(r);
    v3cross(r, f, v);
    for (i = 0; i < 4; i++) {
        float sx = i & 1 ? 1.0f : -1.0f, sy = i & 2 ? 1.0f : -1.0f;
        for (k = 0; k < 3; k++)
            ray[i][k] = f[k] + r[k] * tx * sx + v[k] * ty * sy;
        v3norm(ray[i]);
    }
    memcpy(fwd, f, 12);
}

static void test_cache(void)
{
    Tris st = { 0 }, dy = { 0 };
    float sun[3] = { -1.0f, -1.5f, 2.5f }, f[3], trklo[3] = { 1e30f, 1e30f, 1e30f }, trkhi[3] = { -1e30f, -1e30f, -1e30f };
    float *cache[CASCADES], *live[CASCADES], *ref = malloc(SIM_S * SIM_S * sizeof(float)), prevm[CASCADES][16];
    BrrShCache shc[CASCADES];
    SimCam cam = { { -100.0f, -6.0f, 2.0f }, 0.5f, -0.12f };
    unsigned gen = 1;
    int c, i, x, y, frame, mismatch = 0, frames = 0, outside = 0, notmoved_redraw = 0, moved_noredraw = 0;
    long redraws[4][CASCADES], dynseen = 0, restored = 0, tiled = 0;
    double croperr = 0;
    static const char *const phase[4] = { "camera still, cars moving", "driving 0.3 m a frame", "the track rebuilt", "turning 1 degree a frame" };
    memset(redraws, 0, sizeof redraws);
    memset(shc, 0, sizeof shc);
    for (c = 0; c < CASCADES; c++) {
        cache[c] = malloc(SIM_S * SIM_S * sizeof(float));
        live[c] = malloc(SIM_S * SIM_S * sizeof(float));
        fill1(cache[c]);
        fill1(live[c]);
    }
    v3norm(sun);
    for (i = 0; i < 3; i++) f[i] = -sun[i];
    /* the static scene: ground in 10 m squares over 400 m, and 60 posts */
    for (y = -20; y < 20; y++)
        for (x = -20; x < 20; x++) {
            float a[3] = { x * 10.0f, y * 10.0f, 0 }, b[3] = { x * 10.0f + 10, y * 10.0f, 0 }, cc[3] = { x * 10.0f + 10, y * 10.0f + 10, 0 }, d[3] = { x * 10.0f, y * 10.0f + 10, 0 };
            tri_add(&st, a, b, cc);
            tri_add(&st, a, cc, d);
        }
    for (i = 0; i < 60; i++) {
        float px = rnd() * 360 - 180, py = rnd() * 40 - 20, h = 2 + rnd() * 6;
        float lo[3] = { px, py, 0 }, hi[3] = { px + 0.3f, py + 0.3f, h };
        box_add(&st, lo, hi);
    }
    for (i = 0; i < (int)st.n; i++)
        for (x = 0; x < 3; x++) {
            trklo[x] = fminf(trklo[x], st.v[3 * i + x]);
            trkhi[x] = fmaxf(trkhi[x], st.v[3 * i + x]);
        }
    /* fit's unit behaviour: the same matrix twice draws once; a step moved draws again */
    {
        BrrShCache t;
        float m[16] = { 0.1f, 0, 0, 0, 0, 0.1f, 0, 0, 0, 0, 0.01f, 0, 0.25f, -0.5f, 0.3f, 1 };
        int d0[4] = { 0, 0, 0, 0 }, rs[4];
        memset(&t, 0, sizeof t);
        CHECK(brr_shc_plan(&t, m, 1, d0, SIM_S, rs) == 1, "a first frame does not draw the cache");
        CHECK(brr_shc_plan(&t, m, 1, d0, SIM_S, rs) == 0, "the same matrix draws the cache again");
        m[12] += 2.0f / SHADOW_SNAP;
        CHECK(brr_shc_plan(&t, m, 1, d0, SIM_S, rs) == 1, "a window moved a step does not draw the cache again");
        CHECK(brr_shc_plan(&t, m, 2, d0, SIM_S, rs) == 1, "a rebuilt track does not draw the cache again");
    }
    for (frame = 0; frame < 20 + 300 + 1 + 60; frame++) {
        int ph = frame < 20 ? 0 : frame < 320 ? 1 : frame < 321 ? 2 : 3;
        BrrCascade cs[CASCADES];
        float ray[4][3], fwd[3], lo = 1e30f;
        if (ph == 1) cam.eye[0] += 0.3f;
        if (ph == 2) gen++;
        if (ph == 3) cam.yaw += 0.0174533f;
        /* three cars ahead, moving at different speeds */
        dy.n = 0;
        for (i = 0; i < 3; i++) {
            float cx = cam.eye[0] + 8.0f + 6.0f * i + 0.37f * (float)frame * (float)(i + 1) * 0.1f, cy = -4.0f + 4.0f * i;
            float blo[3] = { cx, cy, 0.2f }, bhi[3] = { cx + 4.0f, cy + 1.8f, 1.5f };
            box_add(&dy, blo, bhi);
        }
        sim_rays(&cam, ray, fwd);
        for (i = 0; i < 8; i++) {
            float p[3] = { i & 1 ? trkhi[0] : trklo[0], i & 2 ? trkhi[1] : trklo[1], i & 4 ? trkhi[2] : trklo[2] };
            lo = fminf(lo, p[0] * f[0] + p[1] * f[1] + p[2] * f[2]);
        }
        for (i = 0; i < (int)dy.n; i++)
            lo = fminf(lo, dy.v[3 * i] * f[0] + dy.v[3 * i + 1] * f[1] + dy.v[3 * i + 2] * f[2]);
        brr_cascade_fit(sun, cam.eye, (const float (*)[3])ray, fwd, lo, SIM_S, cs);
        for (c = 0; c < CASCADES; c++) {
            int dyn[4], rs[4], redraw, j, moved = frame > 0 && memcmp(prevm[c], cs[c].m, 64) != 0;
            brr_shc_rect(cs[c].m, dy.v, dy.n, SIM_S, dyn);
            redraw = brr_shc_plan(&shc[c], cs[c].m, gen, dyn, SIM_S, rs);
            redraws[ph][c] += redraw;
            if (frame > 0 && ph != 2) {
                if (!moved && redraw) notmoved_redraw++;
                if (moved && !redraw) moved_noredraw++;
            }
            memcpy(prevm[c], cs[c].m, 64);
            if (redraw) {
                fill1(cache[c]);
                sim_raster(cache[c], cs[c].m, &st);
            }
            for (y = rs[1]; y < rs[3]; y++)
                for (x = rs[0]; x < rs[2]; x++)
                    live[c][y * SIM_S + x] = cache[c][y * SIM_S + x];
            if (rs[0] < rs[2])
                restored += (long)(rs[2] - rs[0]) * (rs[3] - rs[1]);
            if (dyn[0] < dyn[2]) {          /* the tile: the cache's depth, the moving casters over it, blitted in */
                int w = dyn[2] - dyn[0], h = dyn[3] - dyn[1], tb = brr_shc_tile(w, h, SIM_S), tx, ty;
                float *tile = malloc((size_t)tb * tb * sizeof(float));
                CHECK(tb >= w && tb >= h && tb <= SIM_S, "tile %d for a %dx%d rectangle", tb, w, h);
                for (ty = 0; ty < h; ty++)
                    for (tx = 0; tx < w; tx++)
                        tile[ty * tb + tx] = cache[c][(dyn[1] + ty) * SIM_S + dyn[0] + tx];
                sim_raster_win(tile, tb, dyn[0], dyn[1], w, h, cs[c].m, &dy);
                for (ty = 0; ty < h; ty++)
                    for (tx = 0; tx < w; tx++)
                        live[c][(dyn[1] + ty) * SIM_S + dyn[0] + tx] = tile[ty * tb + tx];
                tiled += (long)w * h;
                {   /* the cropped matrix puts each point at its map texel less the tile's corner */
                    float mc[16];
                    int q;
                    brr_shc_crop(cs[c].m, SIM_S, dyn[0], dyn[1], tb, mc);
                    for (q = 0; q < (int)dy.n; q++) {
                        const float *p = &dy.v[3 * q], *m = cs[c].m;
                        double X = ((m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12]) * 0.5 + 0.5) * SIM_S - dyn[0];
                        double Y = (0.5 - (m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13]) * 0.5) * SIM_S - dyn[1];
                        double Xc = ((mc[0] * p[0] + mc[4] * p[1] + mc[8] * p[2] + mc[12]) * 0.5 + 0.5) * tb;
                        double Yc = (0.5 - (mc[1] * p[0] + mc[5] * p[1] + mc[9] * p[2] + mc[13]) * 0.5) * tb;
                        if (fabs(X - Xc) > croperr) croperr = fabs(X - Xc);
                        if (fabs(Y - Yc) > croperr) croperr = fabs(Y - Yc);
                    }
                }
                free(tile);
            }
            /* brute force: the scene drawn from scratch with this frame's matrix */
            fill1(ref);
            sim_raster(ref, cs[c].m, &st);
            sim_raster(ref, cs[c].m, &dy);
            if (memcmp(ref, live[c], SIM_S * SIM_S * sizeof(float)))
                mismatch++;
            for (j = 0; j < SIM_S * SIM_S; j++)
                dynseen += live[c][j] < cache[c][j];
            /* the slice inside its window: its eight corners within the map and its depth */
            for (j = 0; j < 8; j++) {
                float d = j < 4 ? (c ? cs[c - 1].split : 0.5f) : cs[c].split, *rr = ray[j & 3], tt = d / (rr[0] * fwd[0] + rr[1] * fwd[1] + rr[2] * fwd[2]);
                float p[3] = { cam.eye[0] + rr[0] * tt, cam.eye[1] + rr[1] * tt, cam.eye[2] + rr[2] * tt };
                const float *m = cs[c].m;
                float X = m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12], Y = m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13];
                float Z = m[2] * p[0] + m[6] * p[1] + m[10] * p[2] + m[14];
                if (fabsf(X) > 1.0f + 1e-5f || fabsf(Y) > 1.0f + 1e-5f || Z <= 0.0f || Z >= 1.0f)
                    outside++;
            }
        }
        frames++;
    }
    printf("cache: %d frames x %d cascades of %d^2, %zu static and %zu moving triangles\n", frames, CASCADES, SIM_S, st.n / 3, dy.n / 3);
    for (i = 0; i < 4; i++)
        printf("  %-28s static redraws per cascade %ld %ld %ld %ld\n", phase[i], redraws[i][0], redraws[i][1], redraws[i][2], redraws[i][3]);
    printf("  live maps differing from brute force: %d of %d\n", mismatch, frames * CASCADES);
    printf("  redraws without a matrix change: %d; matrix changes without a redraw: %d\n", notmoved_redraw, moved_noredraw);
    printf("  texels where a moving caster is nearer than the cached track: %ld (%.1f a frame); texels a frame restored %.0f and"
           " composited %.0f, of %d\n", dynseen, (double)dynseen / frames, (double)restored / frames, (double)tiled / frames, SIM_S * SIM_S * CASCADES);
    printf("  the cropped matrix against the map's texel less the tile's corner: %.2e texels at most\n", croperr);
    printf("  slice corners outside their window: %d\n", outside);
    CHECK(mismatch == 0, "%d live maps differ from the scene drawn from scratch", mismatch);
    CHECK(redraws[0][0] == 1 && redraws[0][1] == 1 && redraws[0][2] == 1 && redraws[0][3] == 1, "a still camera redraws its static casters");
    CHECK(redraws[2][0] + redraws[2][1] + redraws[2][2] + redraws[2][3] == 4, "a rebuilt track is not redrawn in every cascade");
    CHECK(notmoved_redraw == 0 && moved_noredraw == 0, "redraws do not follow matrix changes");
    CHECK(redraws[1][0] > 0 && redraws[1][0] < 300, "driving: cascade 0 redrawn %ld times in 300 frames", redraws[1][0]);
    CHECK(dynseen > 0, "the moving casters never show in the live maps");
    CHECK(outside == 0, "%d slice corners fall outside their cascade's window", outside);
    CHECK(croperr < 1e-3, "the tile's matrix is off by %.2e texels", croperr);
    for (c = 0; c < CASCADES; c++) {
        free(cache[c]);
        free(live[c]);
    }
    free(ref);
    free(st.v);
    free(dy.v);
}

/* ---- screen -------------------------------------------------------------- */
static void test_screen(void)
{
    double worst = 0;
    int i;
    for (i = 0; i < 2000; i++) {
        Cam c;
        LU u;
        float eye[3] = { rnd() * 200 - 100, rnd() * 200 - 100, rnd() * 20 + 0.5f }, tgt[3], P[3], d[3], h[4], px, py, e[3];
        double dot;
        int k;
        memset(&c, 0, sizeof c);
        tgt[0] = eye[0] + rnd() * 20 - 10; tgt[1] = eye[1] + rnd() * 20 - 10; tgt[2] = eye[2] - rnd() * 3;
        cam_make(&c, eye, tgt, 0.6f + rnd() * 0.8f, 4.0f / 3.0f);
        c.sx = 320 * (0.5f + rnd()); c.ox = 320 + rnd() * 20; c.sy = (i & 1 ? 1.0f : -1.0f) * 240 * (0.5f + rnd()); c.oy = 240 + rnd() * 20;
        c.flip = i & 2 ? 480.0f : 0.0f;
        c.xft[0] = 2.0f / 640 * (0.8f + rnd() * 0.4f); c.xft[1] = -1 + rnd() * 0.1f; c.xft[2] = -2.0f / 480 * (0.8f + rnd() * 0.4f); c.xft[3] = 1 - rnd() * 0.1f;
        c.tw = 1280 + (float)(int)(rnd() * 1000); c.th = 960 + (float)(int)(rnd() * 600);
        brr_screen_matrix(c.vp, c.sx, c.ox, c.sy, c.oy, c.flip, c.xft, c.tw, c.th, u.scr);
        for (k = 0; k < 3; k++)                 /* a point in front of the camera */
            P[k] = eye[k] + c.fwd[k] * (2 + rnd() * 50) + (rnd() - 0.5f) * 3;
        for (k = 0; k < 4; k++)
            h[k] = u.scr[k] * P[0] + u.scr[4 + k] * P[1] + u.scr[8 + k] * P[2] + u.scr[12 + k];
        if (h[3] <= 0)
            continue;
        px = h[0] / h[3];
        py = h[1] / h[3];
        cam_ray(&c, px - 0.5f, py - 0.5f, d);
        for (k = 0; k < 3; k++)
            e[k] = P[k] - eye[k];
        v3norm(e);
        {   /* the angle from the cross product: acos of a dot product near 1 would
               show the vectors' own float rounding as 3e-4 rad */
            double cx = (double)d[1] * e[2] - (double)d[2] * e[1], cy = (double)d[2] * e[0] - (double)d[0] * e[2];
            double cz = (double)d[0] * e[1] - (double)d[1] * e[0], a;
            dot = (double)d[0] * e[0] + (double)d[1] * e[1] + (double)d[2] * e[2];
            a = atan2(sqrt(cx * cx + cy * cy + cz * cz), dot);
            if (a > worst)
                worst = a;
        }
    }
    printf("screen: 2000 cameras (both row orders, random viewports and maps): the sky's ray through the computed pixel"
           " misses the point by %.2e rad at most\n", worst);
    CHECK(worst < 2e-5, "world-to-pixel map disagrees with camray by %.2e rad", worst);
}

/* ---- contact ------------------------------------------------------------- */
static void test_contact(void)
{
    LU u;
    Cam c;
    float *pos = malloc(CON_W * CON_H * 16), *nrm = malloc(CON_W * CON_H * 16);
    int x, y, nocc = 0, hit = 0, nlit = 0, falsep = 0, nmid = 0;
    con_setup(&u, &c, pos, nrm);
    for (y = 0; y < CON_H; y++)
        for (x = 0; x < CON_W; x++) {
            const float *P = &pos[4 * (y * CON_W + x)], *N = &nrm[4 * (y * CON_W + x)];
            float o[3], th, v;
            if (P[3] != 1.0f)
                continue;
            o[0] = P[0]; o[1] = P[1]; o[2] = P[2] + 1e-5f;
            th = box_hit(o, u.sun, k_box_lo, k_box_hi, NULL);
            v = cpu_contact(&u, pos, CON_W, CON_H, P, N, cpu_ign(x + 0.5f, y + 0.5f));
            if (th >= 0 && th < 0.30f) {
                nocc++;
                hit += v == 0.0f;
            } else if (th < 0 || th > 0.55f) {
                nlit++;
                falsep += v < 1.0f;
            } else
                nmid++;
        }
    printf("contact: a 12 cm block in the sun, %dx%d G-buffer: %d ground pixels the block shadows within 0.30 m along the sun:"
           " %d (%.1f%%) found; %d ground pixels with nothing within 0.55 m: %d (%.2f%%) wrongly shadowed; %d in between not graded\n",
           CON_W, CON_H, nocc, hit, 100.0 * hit / (nocc ? nocc : 1), nlit, falsep, 100.0 * falsep / (nlit ? nlit : 1), nmid);
    CHECK(nocc > 100, "the scene has too few shadowed pixels to grade (%d)", nocc);
    CHECK(hit >= nocc * 0.9, "contact shadows found %.1f%% of the shadowed pixels", 100.0 * hit / (nocc ? nocc : 1));
    /* a single depth layer cannot tell a point just behind the block's
       silhouette from one inside it: a fringe of a pixel or two along the
       silhouette is the method's, anything more is a fault */
    CHECK(falsep <= 4, "%d pixels with nothing toward the sun were shadowed", falsep);
    free(pos);
    free(nrm);
}

int main(void)
{
    printf("LU %zu bytes\n", sizeof(LU));
    CHECK(sizeof(LU) == 560, "LU is %zu bytes, the shader's is 560", sizeof(LU));
    test_penumbra();
    test_cache();
    test_screen();
    test_contact();
    printf(s_fail ? "shadowtest: %d FAILED\n" : "shadowtest: all passed\n", s_fail);
    return s_fail != 0;
}
