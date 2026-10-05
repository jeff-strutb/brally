/* remaster_terrain_erode.c -- droplet erosion of a heightfield, for
 * remaster_terrain.py.
 *
 *   erode W H CELL DROPLETS SEED in.f32 mask.f32 out.f32 flow.f32 dep.f32
 *
 * Rain falls at random cells; each drop runs downhill with inertia, picks up
 * sediment where it speeds up and the slope carries it, drops it where it
 * slows or fills a pit, and evaporates.  What it takes is taken over a small
 * brush of cells, so channels come out smooth.  mask (0..1 per cell) scales
 * how much a drop may change the ground there: 0 on the track's own surfaces.
 * flow is the water that passed each cell (the gullies and stream beds), dep
 * the sediment left (fans, valley floors, scree at the foot of slopes).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int W, H;
static float *h, *mask, *flow, *dep;

static unsigned rs;
static float rnd(void) { rs = rs * 1664525u + 1013904223u; return (rs >> 8) * (1.0f / 16777216.0f); }

static void grad(float x, float y, float *hh, float *gx, float *gy)
{
    int i = (int)x, j = (int)y;
    float u = x - i, v = y - j;
    float a = h[j * W + i], b = h[j * W + i + 1], c = h[(j + 1) * W + i], d = h[(j + 1) * W + i + 1];
    *gx = (b - a) * (1 - v) + (d - c) * v;
    *gy = (c - a) * (1 - u) + (d - b) * u;
    *hh = a * (1 - u) * (1 - v) + b * u * (1 - v) + c * (1 - u) * v + d * u * v;
}

int main(int argc, char **argv)
{
    long n, k;
    float cell;
    FILE *f;
    if (argc != 11) { fprintf(stderr, "usage: erode W H CELL DROPLETS SEED in mask out flow dep\n"); return 2; }
    W = atoi(argv[1]); H = atoi(argv[2]); cell = (float)atof(argv[3]); n = atol(argv[4]); rs = (unsigned)atoi(argv[5]);
    h = malloc(sizeof(float) * W * H); mask = malloc(sizeof(float) * W * H);
    flow = calloc((size_t)W * H, sizeof(float)); dep = calloc((size_t)W * H, sizeof(float));
    f = fopen(argv[6], "rb"); if (!f || fread(h, sizeof(float), (size_t)W * H, f) != (size_t)W * H) return 1; fclose(f);
    f = fopen(argv[7], "rb"); if (!f || fread(mask, sizeof(float), (size_t)W * H, f) != (size_t)W * H) return 1; fclose(f);
    /* heights in cells, so the slope terms are scale free */
    for (k = 0; k < (long)W * H; k++) h[k] /= cell;

    /* the brush: radius R cells, weights falling off linearly */
    const int R = cell < 2.0f ? 3 : 2;
    int bn = 0, bx[64], by[64]; float bw[64], bs = 0;
    for (int dy = -R; dy <= R; dy++) for (int dx = -R; dx <= R; dx++) {
        float d = sqrtf((float)(dx * dx + dy * dy));
        if (d <= R) { bx[bn] = dx; by[bn] = dy; bw[bn] = R - d + 0.5f; bs += bw[bn]; bn++; }
    }
    for (int i = 0; i < bn; i++) bw[i] /= bs;

    const float inertia = 0.06f, capk = 5.0f, minslope = 0.01f, erosion = 0.35f, deposition = 0.25f;
    const float evap = 0.015f, gravity = 6.0f;
    const int life = cell < 2.0f ? 70 : 90;
    for (long d = 0; d < n; d++) {
        float x = 1 + rnd() * (W - 3), y = 1 + rnd() * (H - 3);
        float dx = 0, dy = 0, sp = 1, water = 1, sed = 0;
        for (int s = 0; s < life; s++) {
            int i = (int)x, j = (int)y;
            float u = x - i, v = y - j, h0, gx, gy;
            grad(x, y, &h0, &gx, &gy);
            dx = dx * inertia - gx * (1 - inertia);
            dy = dy * inertia - gy * (1 - inertia);
            float l = sqrtf(dx * dx + dy * dy);
            if (l < 1e-6f) { float a = rnd() * 6.2831853f; dx = cosf(a); dy = sinf(a); l = 1; }
            dx /= l; dy /= l;
            x += dx; y += dy;
            flow[j * W + i] += water;
            if (x < 1 || y < 1 || x >= W - 2 || y >= H - 2) break;
            float h1, g2x, g2y;
            grad(x, y, &h1, &g2x, &g2y);
            float dh = h1 - h0;
            float m = mask[j * W + i];
            float cap = fmaxf(-dh, minslope) * sp * water * capk;
            if (sed > cap || dh > 0) {
                /* uphill: fill the pit behind (not more than it holds); else leave a share */
                float put = dh > 0 ? fminf(dh, sed) : (sed - cap) * deposition;
                put *= m;
                sed -= put;
                h[j * W + i] += put * (1 - u) * (1 - v); h[j * W + i + 1] += put * u * (1 - v);
                h[(j + 1) * W + i] += put * (1 - u) * v; h[(j + 1) * W + i + 1] += put * u * v;
                dep[j * W + i] += put;
            } else {
                float take = fminf((cap - sed) * erosion, -dh) * m;
                for (int q = 0; q < bn; q++) {
                    int ci = i + bx[q], cj = j + by[q];
                    if (ci < 0 || cj < 0 || ci >= W || cj >= H) continue;
                    float t = take * bw[q] * mask[cj * W + ci];
                    h[cj * W + ci] -= t;
                    sed += t;
                }
            }
            sp = sqrtf(fmaxf(sp * sp + dh * -gravity, 0.0f));
            water *= 1 - evap;
        }
    }
    for (k = 0; k < (long)W * H; k++) h[k] *= cell;
    f = fopen(argv[8], "wb"); fwrite(h, sizeof(float), (size_t)W * H, f); fclose(f);
    f = fopen(argv[9], "wb"); fwrite(flow, sizeof(float), (size_t)W * H, f); fclose(f);
    for (k = 0; k < (long)W * H; k++) dep[k] *= cell;
    f = fopen(argv[10], "wb"); fwrite(dep, sizeof(float), (size_t)W * H, f); fclose(f);
    return 0;
}
