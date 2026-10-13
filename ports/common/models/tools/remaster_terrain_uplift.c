/* remaster_terrain_uplift.c -- mountains grown by uplift against river
 * erosion, for remaster_terrain.py (the stream power law, solved implicitly
 * along the drainage tree; after Braun & Willett 2013 and Cordonnier et al.
 * 2016).
 *
 *   uplift W H CELL STEPS in.f32 uplift.f32 fixed.u8 out.f32 area.f32
 *
 * Each step: the drainage network by a priority flood from the outlets (the
 * grid's border and the fixed cells, which hold their heights: the track),
 * so every cell drains to one neighbour and pits fill into lakes that spill;
 * the upstream area of every cell; then each cell, outlets first, is raised
 * by its uplift and cut toward its receiver by K A^m S; then slopes steeper
 * than the angle of repose crumble (thermal erosion).  What comes out has
 * the branching valleys, sharp ridges and smooth concave river profiles of
 * real ranges.  area is the final upstream area (m2): the rivers.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int W, H;
static float *h;
static unsigned char *fixd;

/* a binary min-heap of (height, index) */
static float *hk; static int *hi_; static int hn;
static void push(float k, int i)
{
    int c = hn++;
    while (c) { int p = (c - 1) / 2; if (hk[p] <= k) break; hk[c] = hk[p]; hi_[c] = hi_[p]; c = p; }
    hk[c] = k; hi_[c] = i;
}
static int pop(float *k)
{
    int r = hi_[0]; float lk = hk[--hn]; int li = hi_[hn], c = 0;
    *k = hk[0];
    for (;;) {
        int a = 2 * c + 1, b = a + 1, m = c; float mk = lk;
        if (a < hn && hk[a] < mk) { m = a; mk = hk[a]; }
        if (b < hn && hk[b] < mk) { m = b; mk = hk[b]; }
        if (m == c) break;
        hk[c] = hk[m]; hi_[c] = hi_[m]; c = m;
    }
    if (hn) { hk[c] = lk; hi_[c] = li; }
    return r;
}

int main(int argc, char **argv)
{
    if (argc != 10) { fprintf(stderr, "usage: uplift W H CELL STEPS in uplift fixed out area\n"); return 2; }
    W = atoi(argv[1]); H = atoi(argv[2]);
    float cell = (float)atof(argv[3]);
    int steps = atoi(argv[4]);
    long N = (long)W * H;
    h = malloc(sizeof(float) * N);
    float *U = malloc(sizeof(float) * N), *A = malloc(sizeof(float) * N);
    fixd = malloc(N);
    int *rec = malloc(sizeof(int) * N), *order = malloc(sizeof(int) * N);
    unsigned char *seen = malloc(N);
    hk = malloc(sizeof(float) * N); hi_ = malloc(sizeof(int) * N);
    FILE *f;
    f = fopen(argv[5], "rb"); if (!f || fread(h, 4, N, f) != (size_t)N) return 1; fclose(f);
    f = fopen(argv[6], "rb"); if (!f || fread(U, 4, N, f) != (size_t)N) return 1; fclose(f);
    f = fopen(argv[7], "rb"); if (!f || fread(fixd, 1, N, f) != (size_t)N) return 1; fclose(f);

    const int DX[8] = { 1, -1, 0, 0, 1, 1, -1, -1 }, DY[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };
    const float DD[8] = { 1, 1, 1, 1, 1.41421356f, 1.41421356f, 1.41421356f, 1.41421356f };
    /* stream power: dh/dt = U - K A^m S (n = 1); K so a 10 km2 river grades
     * a 1 mm/yr uplift to a slope of about 1 % ; dt in years */
    const float m = 0.45f, K = 2.5e-5f, dt = 25000.0f;
    const float talus = tanf(33.0f * 3.14159265f / 180.0f) * cell;
    unsigned rs = 12345;
    for (int st = 0; st < steps; st++) {
        /* the drainage tree: flood from the outlets */
        memset(seen, 0, N); hn = 0;
        for (long i = 0; i < N; i++) {
            int x = (int)(i % W), y = (int)(i / W);
            if (fixd[i] || x == 0 || y == 0 || x == W - 1 || y == H - 1) { push(h[i], (int)i); seen[i] = 1; rec[i] = (int)i; }
        }
        long no = 0;
        while (hn) {
            float k; int i = pop(&k);
            order[no++] = i;
            int x = i % W, y = i / W;
            /* visit the neighbours in a shuffled order: no grid-aligned bias */
            rs = rs * 1664525u + 1013904223u;
            int s0 = (int)(rs >> 29);
            for (int q0 = 0; q0 < 8; q0++) {
                int q = (q0 + s0) & 7;
                int nx = x + DX[q], ny = y + DY[q];
                if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
                int j = ny * W + nx;
                if (seen[j]) continue;
                seen[j] = 1;
                rec[j] = i;
                /* a pit fills to just above its spill point: a lake */
                if (h[j] <= h[i]) h[j] = h[i] + 1e-3f;
                push(h[j], j);
            }
        }
        /* upstream area: highest first */
        for (long i = 0; i < N; i++) A[i] = cell * cell;
        for (long k = no - 1; k >= 0; k--) { int i = order[k]; if (rec[i] != i) A[rec[i]] += A[i]; }
        /* uplift and incision, outlets first (implicit along the tree) */
        for (long k = 0; k < no; k++) {
            int i = order[k];
            if (rec[i] == i || fixd[i]) continue;
            int r = rec[i];
            int dxr = (i % W) - (r % W), dyr = (i / W) - (r / W);
            float d = (dxr && dyr ? 1.41421356f : 1.0f) * cell;
            float fk = K * dt * powf(A[i], m) / d;
            float hn2 = (h[i] + dt * U[i] + fk * h[r]) / (1.0f + fk);
            if (hn2 < h[r] + 1e-3f) hn2 = h[r] + 1e-3f;
            h[i] = hn2;
        }
        /* thermal: slopes past the angle of repose slump */
        for (int pass = 0; pass < 2; pass++)
            for (long i = 0; i < N; i++) {
                if (fixd[i]) continue;
                int x = (int)(i % W), y = (int)(i / W);
                if (x == 0 || y == 0 || x == W - 1 || y == H - 1) continue;
                for (int q = 0; q < 8; q++) {
                    int j = (y + DY[q]) * W + x + DX[q];
                    float lim = talus * DD[q];
                    float dh = h[i] - h[j];
                    if (dh > lim) {
                        float mv = (dh - lim) * 0.25f;
                        h[i] -= mv;
                        if (!fixd[j]) h[j] += mv;
                    }
                }
            }
        if (st % 20 == 0 || st == steps - 1) {
            float mx = -1e30f; for (long i = 0; i < N; i++) if (h[i] > mx) mx = h[i];
            fprintf(stderr, "uplift: step %d highest %.0f m\n", st, mx);
        }
    }
    f = fopen(argv[8], "wb"); fwrite(h, 4, N, f); fclose(f);
    f = fopen(argv[9], "wb"); fwrite(A, 4, N, f); fclose(f);
    return 0;
}
