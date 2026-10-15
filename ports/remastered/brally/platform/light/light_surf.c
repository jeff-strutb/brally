/* light_surf.c: a game texture's surface map (light_surf.h).
 *
 *   1. each texel's surface, from its colour (HSV) in its material's context:
 *      natural ground (asphalt, dirt, rock, grass, sand, snow, ice, concrete)
 *      is one base surface for the whole texture, chosen by its mean colour,
 *      and over it only what is plainly something else (a road's paint, grass
 *      on rock or a beach's crest, water and its foam, snow's rock).  A
 *      layout (a building's wall, a sign, wood, metal) is its colours'
 *      fewest clusters (k-means, at most 8, mean error under 0.035), each a
 *      surface by its colour: its art kept, region by region
 *   2. the 15/16-bit dither's lone texels given to their neighbours (a 3x3
 *      vote, wrapping); a texel-wide line (a road's paint, foam, any layout
 *      region) kept where it runs on
 *   3. each region's field at four times the size (Catmull-Rom, wrapping, a
 *      slight blur): the two strongest at each texel and where it sits
 *      between them, so a border stays crisp under bilinear filtering at any
 *      magnification and runs smooth instead of in texel steps
 *   4. each region's own light and dark, by normalised convolution (never a
 *      neighbour's colour across a border), its mean colour, linear
 *
 * Doubles throughout, the order of ports/remastered/tools/texregion.py's
 * numpy, so the two make the same map (surftest.c). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "light_surf.h"
#include "light_texmat.h"

/* the photographed sets (Poly Haven, CC0; ports/remastered/tools/surf_fetch.sh),
 * chosen against Forza Horizon 6's own surfaces (docs/brally/remaster/textures/
 * DESIGN.md section 3): their sides from Poly Haven's dimensions */
const light_surf_set k_light_surf_set[] = {
    { "asphalt_pit_lane", 2.00f },      /* 0 */
    { "dry_ground_rocks", 4.00f },      /* 1 */
    { "red_dirt_mud_01", 1.50f },       /* 2 */
    { "rocky_trail", 2.00f },           /* 3 */
    { "sand_01", 1.50f },               /* 4 */
    { "leafy_grass", 2.00f },           /* 5 */
    { "withered_grass", 2.00f },        /* 6 */
    { "rock_face_04", 2.00f },          /* 7 */
    { "rock_face_03", 2.70f },          /* 8 */
    { "cliff_side", 1.83f },            /* 9 */
    { "snow_02", 2.00f },               /* 10 */
    { "brushed_concrete", 2.50f },      /* 11 */
    { "painted_plaster_wall", 2.00f },  /* 12 */
    { "weathered_planks", 2.00f },      /* 13 */
    { "asphalt_track", 2.00f },         /* 14 */
    { "worn_asphalt", 2.00f },          /* 15 */
    { "rocky_trail_02", 2.00f },        /* 16 */
    { "brown_mud_dry", 1.30f },         /* 17 */
    { "red_laterite_soil_stones", 2.00f }, /* 18 */
    { "gravel_stones", 2.00f },         /* 19 */
    { "dense_sand", 1.80f },            /* 20 */
    { "grass_ground", 2.51f },          /* 21 */
    { "grass_path_2", 1.00f },          /* 22 */
    { "grass_path_3", 1.00f },          /* 23 */
    { "seaside_rock", 2.00f },          /* 24 */
    { "tiger_rock", 2.55f },            /* 25 */
};
const int k_light_surf_nset = (int)(sizeof k_light_surf_set / sizeof k_light_surf_set[0]);

/* name, variants' sets and brightness, the variants' patch size (m), detail, normal, var, rough, lum, chroma
 * under the Remastered lighting and under the game's own (DESIGN.md section 4).
 * The variants: a road's newer, darker asphalt (asphalt_track) and its worn, paler stretches
 * (worn_asphalt) about the usual; dirt fine, rocky or cracked; grass lush, sparse or worn to
 * gravel; rock of two kinds; sand coarser and finer */
#define N3 { -1, -1, -1 }
#define B1 { 1.0f, 1.0f, 1.0f }
const light_surf_info k_light_surf[LIGHT_SURF_COUNT] = {
    [LIGHT_SURF_NONE] = { "none", N3, B1, 0, 0.0f, 0.0f, 1.0f, 0.8f, 1.0f, 1.0f, 1.0f, 1.0f },
    [LIGHT_SURF_ASPHALT] = { "asphalt", { 0, 14, 15 }, { 1.0f, 0.82f, 1.15f }, 70, 1.0f, 1.0f, 0.15f, 0.85f, 1.00f, 1.00f, 1.23f, 1.00f },   /* FH6: even, no baked streaks */
    [LIGHT_SURF_PAINT_WHITE] = { "paint_white", { 0, -1, -1 }, B1, 0, 0.25f, 0.6f, 0.2f, 0.6f, 1.0f, 1.0f, 1.0f, 1.0f },   /* road paint over the asphalt's grain */
    [LIGHT_SURF_PAINT_YELLOW] = { "paint_yellow", { 0, -1, -1 }, B1, 0, 0.25f, 0.6f, 0.2f, 0.6f, 1.0f, 1.0f, 1.0f, 1.0f },
    [LIGHT_SURF_GRAVEL] = { "gravel", { 3, 19, -1 }, { 1.0f, 0.9f, 1.0f }, 30, 1.0f, 1.0f, 0.5f, 0.9f, 1.10f, 0.45f, 1.16f, 0.62f },
    [LIGHT_SURF_DIRT] = { "dirt", { 1, 16, 17 }, { 1.0f, 1.05f, 0.92f }, 35, 1.0f, 1.0f, 0.5f, 0.9f, 1.10f, 0.45f, 1.16f, 0.62f },
    [LIGHT_SURF_DIRT_RED] = { "dirt_red", { 2, 18, -1 }, { 1.0f, 0.92f, 1.0f }, 35, 1.0f, 1.0f, 0.5f, 0.9f, 1.10f, 0.45f, 1.16f, 0.62f },
    [LIGHT_SURF_SAND] = { "sand", { 4, 20, -1 }, { 1.0f, 1.04f, 1.0f }, 40, 1.0f, 0.8f, 0.6f, 0.9f, 1.0f, 1.0f, 1.0f, 1.0f },
    [LIGHT_SURF_SAND_WET] = { "sand_wet", { 4, -1, -1 }, B1, 0, 0.8f, 0.5f, 0.6f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f },
    [LIGHT_SURF_FOAM] = { "foam", { 4, -1, -1 }, B1, 0, 0.3f, 0.3f, 0.6f, 0.7f, 1.0f, 1.0f, 1.0f, 1.0f },
    [LIGHT_SURF_WATER] = { "water", N3, B1, 0, 0.0f, 0.0f, 1.0f, 0.1f, 1.0f, 1.0f, 1.0f, 1.0f },
    [LIGHT_SURF_GRASS] = { "grass", { 5, 21, 22 }, { 1.0f, 0.93f, 1.08f }, 25, 1.0f, 1.0f, 0.6f, 0.95f, 0.60f, 1.50f, 0.90f, 1.22f },
    [LIGHT_SURF_GRASS_DRY] = { "grass_dry", { 6, 23, -1 }, { 1.0f, 1.05f, 1.0f }, 25, 1.0f, 1.0f, 0.6f, 0.95f, 0.60f, 1.50f, 0.90f, 1.22f },
    [LIGHT_SURF_MOSS] = { "moss", { 21, -1, -1 }, B1, 0, 1.0f, 0.8f, 0.6f, 0.95f, 0.60f, 1.50f, 0.90f, 1.22f },
    [LIGHT_SURF_ROCK_GREY] = { "rock_grey", { 7, 24, -1 }, { 1.0f, 1.05f, 1.0f }, 30, 0.75f, 1.0f, 0.7f, 0.85f, 1.08f, 0.55f, 1.16f, 0.65f },   /* rock_face_04 coarser than FH6's rock: 0.75 */
    [LIGHT_SURF_ROCK_BROWN] = { "rock_brown", { 8, 25, -1 }, { 1.0f, 0.95f, 1.0f }, 30, 1.0f, 1.0f, 0.7f, 0.85f, 1.08f, 0.55f, 1.16f, 0.65f },
    [LIGHT_SURF_ROCK_RED] = { "rock_red", { 9, -1, -1 }, B1, 0, 1.0f, 1.0f, 0.7f, 0.85f, 0.83f, 0.45f, 1.05f, 0.45f },
    [LIGHT_SURF_CONCRETE] = { "concrete", { 11, -1, -1 }, B1, 0, 1.0f, 0.8f, 0.6f, 0.85f, 1.0f, 1.0f, 1.0f, 1.0f },
    [LIGHT_SURF_PLASTER] = { "plaster", { 12, -1, -1 }, B1, 0, 1.0f, 0.7f, 1.0f, 0.9f, 1.0f, 1.0f, 1.0f, 1.0f },
    [LIGHT_SURF_BRICK] = { "brick", { 11, -1, -1 }, B1, 0, 0.6f, 0.6f, 1.0f, 0.9f, 1.0f, 1.0f, 1.0f, 1.0f },   /* the art draws the bricks: fine grain only */
    [LIGHT_SURF_STONE] = { "stone", { 11, -1, -1 }, B1, 0, 0.8f, 0.8f, 1.0f, 0.9f, 1.0f, 1.0f, 1.0f, 1.0f },
    [LIGHT_SURF_WOOD] = { "wood", { 13, -1, -1 }, B1, 0, 0.5f, 0.6f, 1.0f, 0.8f, 1.0f, 1.0f, 1.0f, 1.0f },
    [LIGHT_SURF_METAL] = { "metal", { 11, -1, -1 }, B1, 0, 0.3f, 0.3f, 1.0f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f },
    [LIGHT_SURF_RUST] = { "rust", { 11, -1, -1 }, B1, 0, 0.7f, 0.6f, 1.0f, 0.8f, 1.0f, 1.0f, 1.0f, 1.0f },
    [LIGHT_SURF_SNOW] = { "snow", { 10, -1, -1 }, B1, 0, 0.85f, 0.8f, 0.5f, 0.75f, 1.00f, 0.40f, 1.00f, 0.40f },   /* FH6's snow: smoother than the scan */
    [LIGHT_SURF_ICE] = { "ice", { 10, -1, -1 }, B1, 0, 0.3f, 0.4f, 0.5f, 0.15f, 1.0f, 1.0f, 1.0f, 1.0f },
    [LIGHT_SURF_ROOF] = { "roof", { 11, -1, -1 }, B1, 0, 0.6f, 0.6f, 1.0f, 0.85f, 1.0f, 1.0f, 1.0f, 1.0f },
    [LIGHT_SURF_LEAVES] = { "leaves", N3, B1, 0, 0.0f, 0.0f, 1.0f, 0.9f, 1.0f, 1.0f, 1.0f, 1.0f },
    [LIGHT_SURF_PAINT_SIGN] = { "paint_sign", N3, B1, 0, 0.0f, 0.0f, 1.0f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f },   /* flat printed colour */
};

/* ---- colour ------------------------------------------------------------------------- */
static void hsv(const double c[3], double *h, double *s, double *v)
{
    double r = c[0], g = c[1], b = c[2];
    double mx = fmax(r, fmax(g, b)), mn = fmin(r, fmin(g, b)), d = mx - mn;
    *h = 0.0;
    if (d > 1e-6) {
        if (mx == r)
            *h = fmod(60.0 * ((g - b) / d), 360.0);
        else if (mx == g)
            *h = 60.0 * ((b - r) / d) + 120.0;
        else
            *h = 60.0 * ((r - g) / d) + 240.0;
        if (*h < 0)
            *h += 360.0;
    }
    *s = mx > 1e-6 ? d / fmax(mx, 1e-6) : 0.0;
    *v = mx;
}
static double lin1(double c) { return c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4); }

static int cmpd(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return x < y ? -1 : x > y;
}
static double median(double *v, int n)   /* numpy's: the mean of the middle two for even n */
{
    qsort(v, (size_t)n, sizeof *v, cmpd);
    return n & 1 ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

/* ---- labels ------------------------------------------------------------------------- */
/* a texel's label: its surface, and for a layout its cluster (cluster * 64 + surface) */
#define LAB(cl, sf) ((cl) * 64 + (sf))
#define LSURF(l) ((l) % 64)

/* the texture's one base surface (natural ground) */
static int base_surface(int mat, const double *rgb, const uint8_t *a, int n)
{
    double m[3] = { 0, 0, 0 }, h, s, v;
    int i, k, cnt = 0, any = 0;
    for (i = 0; i < n; i++)
        any |= a[i];
    for (i = 0; i < n; i++)
        if (!any || a[i]) {
            for (k = 0; k < 3; k++)
                m[k] += rgb[i * 3 + k];
            cnt++;
        }
    for (k = 0; k < 3; k++)
        m[k] /= cnt;
    hsv(m, &h, &s, &v);
    switch (mat) {
    case LIGHT_MAT_ROCK: return h < 40 && s > 0.38 ? LIGHT_SURF_ROCK_RED : s > 0.2 ? LIGHT_SURF_ROCK_BROWN : LIGHT_SURF_ROCK_GREY;
    case LIGHT_MAT_DIRT: return h < 26 && s > 0.42 ? LIGHT_SURF_DIRT_RED : s < 0.13 ? LIGHT_SURF_GRAVEL : LIGHT_SURF_DIRT;
    case LIGHT_MAT_ASPHALT: return LIGHT_SURF_ASPHALT;
    case LIGHT_MAT_GRASS: return LIGHT_SURF_GRASS;
    case LIGHT_MAT_SAND: return LIGHT_SURF_SAND;
    case LIGHT_MAT_SNOW: return LIGHT_SURF_SNOW;
    case LIGHT_MAT_ICE: return LIGHT_SURF_ICE;
    default: return LIGHT_SURF_CONCRETE;   /* concrete, tunnel */
    }
}

static void classify(int mat, const double *rgb, const uint8_t *a, int n, int *lab)
{
    double *H = malloc(sizeof(double) * n * 4), *S = H + n, *V = S + n, *tmp = V + n;
    double vmed, smed;
    int i, base = base_surface(mat, rgb, a, n);
    for (i = 0; i < n; i++)
        hsv(rgb + i * 3, &H[i], &S[i], &V[i]);
    memcpy(tmp, V, sizeof(double) * n);
    vmed = median(tmp, n);
    memcpy(tmp, S, sizeof(double) * n);
    smed = median(tmp, n);
    for (i = 0; i < n; i++) {
        double h = H[i], s = S[i], v = V[i];
        int green = h > 65 && h <= 165 && s > 0.10, blue = h > 165 && h <= 260 && s > 0.18, grey = s < 0.13;
        int warm = !green && !blue && !grey, o = base;
        switch (mat) {
        case LIGHT_MAT_ASPHALT:
            if (warm && s > fmax(0.3, smed + 0.15)) o = LIGHT_SURF_DIRT;
            if (green && s > smed + 0.1) o = LIGHT_SURF_GRASS;
            if (v > fmax(0.55, vmed + 0.22) && s < 0.2) o = LIGHT_SURF_PAINT_WHITE;
            if (h >= 35 && h <= 65 && s > 0.45 && v > 0.45) o = LIGHT_SURF_PAINT_YELLOW;
            break;
        case LIGHT_MAT_DIRT: case LIGHT_MAT_ROCK: case LIGHT_MAT_SAND: case LIGHT_MAT_GRASS:
            if (mat != LIGHT_MAT_GRASS && green) o = LIGHT_SURF_GRASS;
            if (blue) o = LIGHT_SURF_WATER;
            if ((blue || green) && v > 0.72 && s < 0.3 && h > 150) o = LIGHT_SURF_FOAM;
            if (mat == LIGHT_MAT_GRASS && warm && s > smed + 0.12 && h < 40) o = LIGHT_SURF_DIRT;
            if (mat != LIGHT_MAT_SAND && v > 0.8 && s < 0.12) o = LIGHT_SURF_SNOW;   /* a snow track's ground */
            break;
        case LIGHT_MAT_SNOW:
            if (blue && s > 0.25) o = LIGHT_SURF_ICE;
            if (v < 0.45 && !blue) o = LIGHT_SURF_GRAVEL;                       /* a road's ruts, bare ground */
            if (v < 0.45 && warm && s > 0.2) o = LIGHT_SURF_ROCK_BROWN;         /* rock through the snow */
            break;
        case LIGHT_MAT_ICE:
            if (v > 0.85 && s < 0.1) o = LIGHT_SURF_SNOW;
            break;
        default:   /* concrete, tunnel: their lamp strips the game's own */
            if (warm && s > 0.5 && v > 0.5) o = LIGHT_SURF_NONE;
            break;
        }
        if ((h < 12 || h > 330) && s > 0.6 && v > 0.4)   /* kerbs, paint: strong reds keep the game's own texels */
            o = LIGHT_SURF_NONE;
        lab[i] = LAB(0, o);
    }
    free(H);
}

/* a layout's colour cluster: its surface */
static int cluster_surface(int mat, const double c[3])
{
    double h, s, v;
    hsv(c, &h, &s, &v);
    if (mat == LIGHT_MAT_SIGN)
        return LIGHT_SURF_PAINT_SIGN;
    if (mat == LIGHT_MAT_WOOD)
        return v < 0.12 ? LIGHT_SURF_NONE : LIGHT_SURF_WOOD;
    if (mat == LIGHT_MAT_METAL)
        return h < 45 && s > 0.3 ? LIGHT_SURF_RUST : v < 0.12 ? LIGHT_SURF_NONE : LIGHT_SURF_METAL;
    if ((h > 165 && h <= 260 && s > 0.2) || v < 0.12)
        return LIGHT_SURF_NONE;         /* glass, openings: the game's own */
    if (h < 25 && s > 0.35 && v < 0.75)
        return v > 0.35 ? LIGHT_SURF_BRICK : LIGHT_SURF_WOOD;
    if (h >= 25 && h < 55 && s > 0.35 && v < 0.5)
        return LIGHT_SURF_WOOD;
    if (s < 0.12 && v < 0.55)
        return LIGHT_SURF_STONE;
    return LIGHT_SURF_PLASTER;
}

/* distances this close are a tie, taken by the lower index (texregion.py TIE): numpy's pairwise
   sums and these sequential ones differ in the last bits */
#define TIE 1e-9

static double d2(const double *a, const double *b)
{
    double x = a[0] - b[0], y = a[1] - b[1], z = a[2] - b[2];
    return x * x + y * y + z * z;
}

/* k-means over x (m colours) from farthest-point seeds, 12 rounds: centres c, labels l */
static void kmeans(const double *x, int m, int k, double *c, int *l)
{
    double mean[3] = { 0, 0, 0 }, best;
    int i, j, q, it, bi = 0;
    for (i = 0; i < m; i++)
        for (q = 0; q < 3; q++)
            mean[q] += x[i * 3 + q];
    for (q = 0; q < 3; q++)
        mean[q] /= m;
    best = INFINITY;
    for (i = 0; i < m; i++)
        if (d2(x + i * 3, mean) < best - TIE) {
            best = d2(x + i * 3, mean);
            bi = i;
        }
    memcpy(c, x + bi * 3, sizeof(double) * 3);
    for (j = 1; j < k; j++) {
        double far = -1.0;
        for (i = 0; i < m; i++) {
            double d = INFINITY;
            for (q = 0; q < j; q++)
                d = fmin(d, d2(x + i * 3, c + q * 3));
            if (d > far + TIE) {
                far = d;
                bi = i;
            }
        }
        memcpy(c + j * 3, x + bi * 3, sizeof(double) * 3);
    }
    for (it = 0; it < 12; it++) {
        double sum[LIGHT_SURF_REGIONS][3];
        int cnt[LIGHT_SURF_REGIONS];
        memset(sum, 0, sizeof sum);
        memset(cnt, 0, sizeof cnt);
        for (i = 0; i < m; i++) {
            double bd = INFINITY;
            int b = 0;
            for (q = 0; q < k; q++)
                if (d2(x + i * 3, c + q * 3) < bd - TIE) {
                    bd = d2(x + i * 3, c + q * 3);
                    b = q;
                }
            l[i] = b;
            cnt[b]++;
            for (j = 0; j < 3; j++)
                sum[b][j] += x[i * 3 + j];
        }
        for (q = 0; q < k; q++)
            if (cnt[q])
                for (j = 0; j < 3; j++)
                    c[q * 3 + j] = sum[q][j] / cnt[q];
    }
}

static int cmp3(const void *a, const void *b)
{
    const double *x = a, *y = b;
    int i;
    for (i = 0; i < 3; i++)
        if (x[i] != y[i])
            return x[i] < y[i] ? -1 : 1;
    return 0;
}

static void cluster_labels(int mat, const double *rgb, const uint8_t *a, int n, int *lab)
{
    double *x = malloc(sizeof(double) * 3 * n), *u = malloc(sizeof(double) * 3 * n), c[LIGHT_SURF_REGIONS * 3];
    int *l = malloc(sizeof(int) * n), m = 0, i, k, q, nu = 0, kk = 1;
    for (i = 0; i < n; i++)
        if (a[i])
            memcpy(x + 3 * m++, rgb + 3 * i, sizeof(double) * 3);
    memcpy(u, x, sizeof(double) * 3 * m);
    qsort(u, (size_t)m, sizeof(double) * 3, cmp3);
    for (i = 0; i < m; i++)
        nu += !i || cmp3(u + 3 * i, u + 3 * (i - 1));
    for (k = 1; k <= LIGHT_SURF_REGIONS; k++) {
        double err = 0;
        kk = k < nu ? k : nu;
        kmeans(x, m, kk, c, l);
        for (i = 0; i < m; i++)
            err += sqrt(d2(x + 3 * i, c + 3 * l[i]));
        if (err / m < 0.035)
            break;
    }
    for (i = 0; i < n; i++) {
        double bd = INFINITY;
        int b = 0;
        for (q = 0; q < kk; q++)
            if (d2(rgb + 3 * i, c + 3 * q) < bd - TIE) {
                bd = d2(rgb + 3 * i, c + 3 * q);
                b = q;
            }
        lab[i] = LAB(b, cluster_surface(mat, c + 3 * b));
    }
    free(x);
    free(u);
    free(l);
}

/* the 3x3 vote, wrapping (texregion.py vote) */
static int is_thin(int l) { int s = LSURF(l); return s == LIGHT_SURF_PAINT_WHITE || s == LIGHT_SURF_PAINT_YELLOW || s == LIGHT_SURF_FOAM; }

static void vote(int *lab, int w, int h, int thin_all)
{
    int ids[64], nid = 0, i, j, x, y, dx, dy;
    int *out = malloc(sizeof(int) * w * h);
    double *cnt;
    for (i = 0; i < w * h; i++) {
        for (j = 0; j < nid && ids[j] != lab[i]; j++)
            ;
        if (j == nid && nid < 64)
            ids[nid++] = lab[i];
    }
    for (i = 1; i < nid; i++)   /* sorted, as numpy's sorted(set()) of the names: by surface name, */
        for (j = i; j > 0; j--) {   /* and a layout's by cluster then surface ("c<i>:<name>") */
            int a = ids[j - 1], b = ids[j], sw;
            sw = a / 64 != b / 64 ? a / 64 > b / 64 : strcmp(k_light_surf[LSURF(a)].name, k_light_surf[LSURF(b)].name) > 0;
            if (!sw)
                break;
            ids[j - 1] = b;
            ids[j] = a;
        }
    cnt = calloc((size_t)nid * w * h, sizeof(double));
    for (j = 0; j < nid; j++)
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++) {
                double c = 0;
                for (dy = -1; dy <= 1; dy++)
                    for (dx = -1; dx <= 1; dx++)
                        c += lab[((y - dy + h) % h) * w + (x - dx + w) % w] == ids[j];
                c += 2.0 * (lab[y * w + x] == ids[j]);
                cnt[(size_t)j * w * h + y * w + x] = c;
            }
    for (i = 0; i < w * h; i++) {
        int b = 0;
        for (j = 1; j < nid; j++)
            if (cnt[(size_t)j * w * h + i] > cnt[(size_t)b * w * h + i])
                b = j;
        out[i] = ids[b];
    }
    for (j = 0; j < nid; j++)
        if (thin_all || is_thin(ids[j]))
            for (i = 0; i < w * h; i++)
                if (lab[i] == ids[j] && cnt[(size_t)j * w * h + i] >= 4)
                    out[i] = ids[j];
    memcpy(lab, out, sizeof(int) * w * h);
    free(out);
    free(cnt);
}

/* ---- fields ------------------------------------------------------------------------- */
/* f (h x w) upsampled s times, wrapping, Catmull-Rom (texregion.py cubic_up) */
static void cubic_up(const double *f, int w, int h, int s, double *out)
{
    int W = w * s, H = h * s, x, y, a;
    double *rows = malloc(sizeof(double) * H * w);
    for (y = 0; y < H; y++) {
        double p = (y + 0.5) / s - 0.5, t;
        int i = (int)floor(p);
        double wt[4];
        t = p - i;
        wt[0] = (-t * t * t + 2 * t * t - t) / 2; wt[1] = (3 * t * t * t - 5 * t * t + 2) / 2;
        wt[2] = (-3 * t * t * t + 4 * t * t + t) / 2; wt[3] = (t * t * t - t * t) / 2;
        for (x = 0; x < w; x++) {
            double v = 0;
            for (a = 0; a < 4; a++)
                v += wt[a] * f[(((i + a - 1) % h + h) % h) * w + x];
            rows[y * w + x] = v;
        }
    }
    for (x = 0; x < W; x++) {
        double p = (x + 0.5) / s - 0.5, t;
        int i = (int)floor(p);
        double wt[4];
        t = p - i;
        wt[0] = (-t * t * t + 2 * t * t - t) / 2; wt[1] = (3 * t * t * t - 5 * t * t + 2) / 2;
        wt[2] = (-3 * t * t * t + 4 * t * t + t) / 2; wt[3] = (t * t * t - t * t) / 2;
        for (y = 0; y < H; y++) {
            double v = 0;
            for (a = 0; a < 4; a++)
                v += wt[a] * rows[y * w + ((i + a - 1) % w + w) % w];
            out[y * W + x] = v;
        }
    }
    free(rows);
}

/* a Gaussian blur, wrapping, radius ceil(3 sigma) (texregion.py blur) */
static void blur(double *f, int w, int h, double sig)
{
    int r = (int)ceil(sig * 3), i, x, y, n = 2 * r + 1;
    double *k = malloc(sizeof(double) * n), sum = 0, *g = calloc((size_t)w * h, sizeof(double));
    for (i = 0; i < n; i++) {
        double d = i - r;
        k[i] = exp(-d * d / (2 * sig * sig));
        sum += k[i];
    }
    for (i = 0; i < n; i++)
        k[i] /= sum;
    for (i = 0; i < n; i++)   /* np.roll(f, d, 0): g[y] += k * f[y - d] */
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++)
                g[y * w + x] += k[i] * f[((y - (i - r)) % h + h) % h * w + x];
    memset(f, 0, sizeof(double) * w * h);
    for (i = 0; i < n; i++)
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++)
                f[y * w + x] += k[i] * g[y * w + ((x - (i - r)) % w + w) % w];
    free(k);
    free(g);
}

static void clip(double *f, int n, double lo, double hi)
{
    int i;
    for (i = 0; i < n; i++)
        f[i] = f[i] < lo ? lo : f[i] > hi ? hi : f[i];
}

static int is_layout(int mat) { return mat == LIGHT_MAT_BUILDING || mat == LIGHT_MAT_SIGN || mat == LIGHT_MAT_WOOD || mat == LIGHT_MAT_METAL; }
static int is_natural(int mat)
{
    return mat == LIGHT_MAT_ASPHALT || mat == LIGHT_MAT_DIRT || mat == LIGHT_MAT_ROCK || mat == LIGHT_MAT_GRASS || mat == LIGHT_MAT_SAND
        || mat == LIGHT_MAT_SNOW || mat == LIGHT_MAT_ICE || mat == LIGHT_MAT_CONCRETE || mat == LIGHT_MAT_TUNNEL;
}

/* a label's name as texregion.py spells it, for its tie order */
static void lname(int l, int layout, char *out, size_t n)
{
    if (layout)
        snprintf(out, n, "c%d:%s", l / 64, k_light_surf[LSURF(l)].name);
    else
        snprintf(out, n, "%s", k_light_surf[LSURF(l)].name);
}

int light_surf_build(int mat, const uint8_t *rgba, int w, int h, light_surf_map *out)
{
    int n = w * h, up = LIGHT_SURF_UP, W = w * up, H = h * up, N = W * H, layout = is_layout(mat);
    int i, j, q, nkeep = 0, keep[LIGHT_SURF_REGIONS], cnts[64], labs[64], nl = 0;
    double *rgb, *F, *V, *m, *tmp, *num, *den, *v, *cov;
    uint8_t *a;
    int *lab;
    memset(out, 0, sizeof *out);
    if (!layout && !is_natural(mat))
        return 0;
    rgb = malloc(sizeof(double) * 3 * n);
    a = malloc((size_t)n);
    lab = malloc(sizeof(int) * n);
    for (i = 0; i < n; i++) {
        for (q = 0; q < 3; q++)
            rgb[i * 3 + q] = rgba[i * 4 + q] / 255.0;
        a[i] = rgba[i * 4 + 3] >= 128;
    }
    if (layout) {
        int any = 0;
        for (i = 0; i < n; i++)
            any |= a[i];
        if (!any) {
            free(rgb); free(a); free(lab);
            return 0;
        }
        cluster_labels(mat, rgb, a, n, lab);
        vote(lab, w, h, 1);
    } else {
        classify(mat, rgb, a, n, lab);
        vote(lab, w, h, 0);
    }
    /* the regions (their labels among the opaque texels), largest first; ties by name, descending */
    for (i = 0; i < n; i++)
        if (a[i]) {
            for (j = 0; j < nl && labs[j] != lab[i]; j++)
                ;
            if (j == nl && nl < 64)
                labs[nl++] = lab[i];
        }
    {
        int allnone = 1;
        for (j = 0; j < nl; j++)
            allnone &= LSURF(labs[j]) == LIGHT_SURF_NONE;
        if (!nl || allnone) {
            free(rgb); free(a); free(lab);
            return 0;
        }
    }
    for (j = 0; j < nl; j++) {
        cnts[j] = 0;
        for (i = 0; i < n; i++)
            cnts[j] += lab[i] == labs[j];
    }
    for (i = 1; i < nl; i++)
        for (j = i; j > 0; j--) {
            char x[64], y[64];
            int sw;
            lname(labs[j - 1], layout, x, sizeof x);
            lname(labs[j], layout, y, sizeof y);
            sw = cnts[j] > cnts[j - 1] || (cnts[j] == cnts[j - 1] && strcmp(y, x) > 0);
            if (!sw)
                break;
            q = cnts[j]; cnts[j] = cnts[j - 1]; cnts[j - 1] = q;
            q = labs[j]; labs[j] = labs[j - 1]; labs[j - 1] = q;
        }
    nkeep = nl < LIGHT_SURF_REGIONS ? nl : LIGHT_SURF_REGIONS;
    for (j = 0; j < nkeep; j++)
        keep[j] = labs[j];

    F = malloc(sizeof(double) * N * nkeep);
    V = malloc(sizeof(double) * N * nkeep);
    m = malloc(sizeof(double) * n);
    tmp = malloc(sizeof(double) * n);
    num = malloc(sizeof(double) * n);
    den = malloc(sizeof(double) * n);
    v = malloc(sizeof(double) * n);
    cov = malloc(sizeof(double) * N);
    for (j = 0; j < nkeep; j++) {
        double mean[3] = { 0, 0, 0 }, msum = 0, ym;
        for (i = 0; i < n; i++) {
            m[i] = lab[i] == keep[j] && a[i];
            msum += m[i];
        }
        cubic_up(m, w, h, up, F + (size_t)j * N);
        clip(F + (size_t)j * N, N, 0, 1);
        blur(F + (size_t)j * N, W, H, (layout ? 0.2 : 0.35) * up);
        for (i = 0; i < n; i++)
            for (q = 0; q < 3; q++)
                mean[q] += lin1(rgb[i * 3 + q]) * m[i];
        for (q = 0; q < 3; q++)
            mean[q] /= msum > 1 ? msum : 1;
        ym = fmax(0.2126 * mean[0] + 0.7152 * mean[1] + 0.0722 * mean[2], 1e-4);
        for (i = 0; i < n; i++) {
            double y = 0.2126 * lin1(rgb[i * 3]) + 0.7152 * lin1(rgb[i * 3 + 1]) + 0.0722 * lin1(rgb[i * 3 + 2]);
            num[i] = y * m[i];
            den[i] = m[i];
        }
        blur(num, w, h, 1.0);
        blur(den, w, h, 1.0);
        for (i = 0; i < n; i++)
            v[i] = (den[i] > 1e-3 ? num[i] / fmax(den[i], 1e-3) : ym) / ym;
        blur(v, w, h, 0.5);
        cubic_up(v, w, h, up, V + (size_t)j * N);
        clip(V + (size_t)j * N, N, 0.5, 1.5);
        out->surf[j] = (uint8_t)LSURF(keep[j]);
        for (q = 0; q < 3; q++)
            out->lin[j][q] = (float)mean[q];
    }
    for (i = 0; i < n; i++)
        m[i] = a[i];
    cubic_up(m, w, h, up, cov);
    clip(cov, N, 0, 1);
    out->w = W;
    out->h = H;
    out->n = nkeep;
    out->map = malloc((size_t)N * 4);
    out->var = malloc((size_t)N * 2);
    for (i = 0; i < N; i++) {
        int i0 = 0, i1 = 0, lo, hi;
        double f0, f1, flo, fhi, t, vlo, vhi;
        for (j = 1; j < nkeep; j++)   /* the strongest, then the second (np.argsort(-F): stable for ties) */
            if (F[(size_t)j * N + i] > F[(size_t)i0 * N + i])
                i0 = j;
        if (nkeep > 1) {
            i1 = i0 == 0 ? 1 : 0;
            for (j = 0; j < nkeep; j++)
                if (j != i0 && F[(size_t)j * N + i] > F[(size_t)i1 * N + i])
                    i1 = j;
        }
        f0 = F[(size_t)i0 * N + i];
        f1 = F[(size_t)i1 * N + i];
        lo = i0 < i1 ? i0 : i1;
        hi = i0 < i1 ? i1 : i0;
        flo = i0 == lo ? f0 : f1;
        fhi = i0 == lo ? f1 : f0;
        t = lo == hi ? 0.0 : fhi / fmax(flo + fhi, 1e-6);
        vlo = V[(size_t)lo * N + i];
        vhi = V[(size_t)hi * N + i];
        out->map[i * 4] = (uint8_t)lo;
        out->map[i * 4 + 1] = (uint8_t)hi;
        out->map[i * 4 + 2] = (uint8_t)lrint(t * 255);
        out->map[i * 4 + 3] = (uint8_t)lrint(cov[i] * 255);
        out->var[i * 2] = (uint8_t)lrint(fmin(fmax((vlo - 0.5) * 255, 0), 255));
        out->var[i * 2 + 1] = (uint8_t)lrint(fmin(fmax((vhi - 0.5) * 255, 0), 255));
    }
    free(F); free(V); free(m); free(tmp); free(num); free(den); free(v); free(cov);
    free(rgb); free(a); free(lab);
    return 1;
}

void light_surf_free(light_surf_map *m)
{
    free(m->map);
    free(m->var);
    memset(m, 0, sizeof *m);
}
