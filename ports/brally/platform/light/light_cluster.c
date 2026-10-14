/* light_cluster.c: clustered light culling and the lamps' shadow atlas
 * (light_cluster.h; DESIGN.md 7.4).  Plain C: the renderer uploads what it
 * makes, and the tests (ports/brally/tools/lighttest) run it against brute
 * force.
 *
 * Culling.  A cluster is the intersection of half-spaces: its tile's two
 * column planes and two row planes (through the eye, from the clip
 * transform's rows: clip x - X w = 0) and its slice's two depth planes; the
 * outermost tiles and slices drop their outer plane.  A light's influence is
 * bounded by a sphere (a point, the area kinds) or a sphere-capped cone of
 * its outer angle (every other kind), and the exact extent of such a body
 * along a plane's normal is known in closed form (the cap's direction nearest
 * the normal: cos(max(psi - theta, 0))), so a light is kept in a cluster
 * unless its body lies wholly outside one of the cluster's planes.  The
 * planes separate by axis (columns, rows, slices), so each light is tested
 * against 16 + 9 + 24 tiles' planes, not 3,456 clusters, and the clusters are
 * the products.  A closed cluster then also drops a light whose cone misses
 * the cluster's bounding sphere (the cone and sphere test, conservative: its
 * distance to the cone's side never exceeds the true distance).  Every test
 * keeps a margin of 5 cm plus 1e-5 of the distances, against the rounding
 * between this code and the shader's.
 *
 * Shadows.  A shadowed lamp (a cone no wider than 80 degrees) looks along its
 * axis with a square frustum around its cone, near plane LS_NEARP, far
 * plane its range; tiles in an atlas of 128-texel cells, placed as aligned
 * power-of-two blocks (largest first, in Morton order: a set whose area fits
 * the atlas always packs). */
#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "light_cluster.h"

_Static_assert(sizeof(LightSpot) == 64, "LightSpot is the shaders' LS");
_Static_assert(sizeof(LightClusterHdr) == 224, "the shaders' LCH");
_Static_assert(sizeof(LightShadowRec) == 96, "the shaders' LSH");
_Static_assert(LC_N == 3456 && LC_LIGHTS == 128, "brr_metal_lights.h's LSet sizes");
_Static_assert(sizeof(LightSet) == 224 + 128 * 64 + 128 * 96 + 3456 * 4 + 3456 * 128, "the shaders' LSet");

static float dot3(const float a[3], const float b[3]) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

static float smooth01(float e0, float e1, float x)
{
    float t = (x - e0) / (e1 - e0);
    t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
    return t * t * (3.0f - 2.0f * t);
}

/* ---- the shaders' light, in C (brr_metal_lights.h: lamp, lwin) ---------------------- */
static float lamp_c(const LightSpot *l, const float d[3])
{
    float up[3], al, h, v, spread, vert, cut;
    if (l->kind > 1.5f)
        return l->candela;
    al = dot3(d, l->axis);
    if (al <= 0.0f)
        return 0.0f;
    up[0] = l->side[1] * l->axis[2] - l->side[2] * l->axis[1];      /* cross(side, axis) */
    up[1] = l->side[2] * l->axis[0] - l->side[0] * l->axis[2];
    up[2] = l->side[0] * l->axis[1] - l->side[1] * l->axis[0];
    if (l->kind > 0.5f) {   /* a rear lamp (R7 Annex 4): a Gaussian of 18 degrees across by 6 up and down */
        h = atan2f(dot3(d, l->side), al) * 57.2958f;
        v = atan2f(dot3(d, up), al) * 57.2958f;
        return l->candela * expf(-(h * h) / (2.0f * 18.0f * 18.0f) - (v * v) / (2.0f * 6.0f * 6.0f));
    }
    h = atan2f(dot3(d, l->side), al) * 57.2958f;
    v = atan2f(dot3(d, up), al) * 57.2958f;
    spread = 0.25f * expf(-(h / 32.0f) * (h / 32.0f)) + 0.75f * expf(-((h - 1.5f) / 10.0f) * ((h - 1.5f) / 10.0f));
    vert = v < -1.5f ? expf(-((v + 1.5f) / 4.5f) * ((v + 1.5f) / 4.5f)) : 1.0f;
    cut = h > 0.0f ? fminf(h * 0.268f, 2.0f) : 0.0f;
    return l->candela * spread * vert * (1.0f + (0.012f - 1.0f) * smooth01(cut - 0.35f, cut + 0.35f, v));
}

static float lwin_c(const LightSpot *l, const float d[3], float dl)
{
    float w, al, ci;
    if (!(dl < l->range))
        return 0.0f;
    w = 1.0f - smooth01(0.7f * l->range, l->range, dl);
    if ((l->kind > 1.5f && l->kind < 2.5f) || l->kind > 3.5f)
        return w;
    al = dot3(d, l->axis);
    if (al <= l->cosout)
        return 0.0f;
    if (l->cosout <= 0.0f)
        return w;
    ci = l->kind > 2.5f ? fmaxf(l->side[0], l->cosout + 1e-4f) : fminf(l->cosout + 0.1f, 1.0f);
    return w * smooth01(l->cosout, ci, al);
}

float light_influence(const LightSpot *l, const float p[3])
{
    float L[3] = { l->pos[0] - p[0], l->pos[1] - p[1], l->pos[2] - p[2] }, d2 = fmaxf(dot3(L, L), 0.01f), dl = sqrtf(d2);
    float d[3] = { -L[0] / dl, -L[1] / dl, -L[2] / dl };
    return lamp_c(l, d) * lwin_c(l, d, dl);
}

/* ---- a light's body: a sphere or a sphere-capped cone ------------------------------ */
typedef struct Body {
    float p[3], a[3], r, c, s;   /* apex, unit axis, radius; cos and sin of the half angle */
    int cone;
    float m;                     /* the margin */
} Body;

static void body_of(const LightSpot *l, const float eye[3], Body *b)
{
    float al = sqrtf(dot3(l->axis, l->axis)), de[3] = { l->pos[0] - eye[0], l->pos[1] - eye[1], l->pos[2] - eye[2] };
    int k;
    memcpy(b->p, l->pos, sizeof b->p);
    b->r = l->range;
    /* the area kinds' placeholder and a point: the sphere (lc_bounds: a rect or
       a tube is to bound its emitter's every point with its range) */
    b->cone = !((l->kind > 1.5f && l->kind < 2.5f) || l->kind > 3.5f) && al > 0.0f;
    for (k = 0; k < 3; k++)
        b->a[k] = al > 0.0f ? l->axis[k] / al : 0.0f;
    b->c = fminf(fmaxf(l->cosout, 0.0f), 1.0f);
    b->s = sqrtf(fmaxf(0.0f, 1.0f - b->c * b->c));
    b->m = 0.05f + 1e-5f * (sqrtf(dot3(de, de)) + l->range + fabsf(l->pos[0]) + fabsf(l->pos[1]) + fabsf(l->pos[2]));
}

/* the least and greatest of n.x + d over the body (n unit) */
static void body_span(const Body *b, const float n[3], float d, float *mn, float *mx)
{
    float c = dot3(n, b->p) + d, na, sp, hi, lo;
    if (!b->cone) {
        *mn = c - b->r;
        *mx = c + b->r;
        return;
    }
    na = dot3(n, b->a);
    sp = sqrtf(fmaxf(0.0f, 1.0f - na * na));
    hi = na >= b->c ? 1.0f : na * b->c + sp * b->s;          /* the cap's direction nearest n */
    lo = -na >= b->c ? -1.0f : na * b->c - sp * b->s;        /* and nearest -n */
    *mx = c + b->r * fmaxf(hi, 0.0f);
    *mn = c + b->r * fminf(lo, 0.0f);
}

/* 0 when the body misses the sphere (centre q, radius rq) */
static int body_meets_sphere(const Body *b, const float q[3], float rq)
{
    float v[3] = { q[0] - b->p[0], q[1] - b->p[1], q[2] - b->p[2] }, l2 = dot3(v, v), v1, h;
    rq += b->m;
    if (l2 > (b->r + rq) * (b->r + rq))
        return 0;
    if (!b->cone || b->c <= 0.0f)
        return b->cone ? dot3(v, b->a) > -rq : 1;
    v1 = dot3(v, b->a);
    h = sqrtf(fmaxf(l2 - v1 * v1, 0.0f));
    return b->c * h - v1 * b->s <= rq;
}

/* ---- the clusters ----------------------------------------------------------------- */
typedef struct Plane { float n[3], d; } Plane;

/* the plane (a - X b) . [p 1] = 0 of rows a, b of the clip transform, unit normal */
static Plane clip_plane(const float a[4], const float b[4], double X)
{
    Plane p;
    double n[4], l;
    int k;
    for (k = 0; k < 4; k++)
        n[k] = (double)a[k] - X * (double)b[k];
    l = sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    if (!(l > 0))
        l = 1;
    for (k = 0; k < 3; k++)
        p.n[k] = (float)(n[k] / l);
    p.d = (float)(n[3] / l);
    return p;
}

/* solve the 3 x 3 system m x = r (rows), Cramer's rule in double */
static void solve3(const double m[3][3], const double r[3], double x[3])
{
    double det = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
               + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
    int k;
    if (fabs(det) < 1e-300)
        det = 1e-300;
    for (k = 0; k < 3; k++) {
        double a[3][3];
        int i, j;
        for (i = 0; i < 3; i++)
            for (j = 0; j < 3; j++)
                a[i][j] = j == k ? r[i] : m[i][j];
        x[k] = (a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1]) - a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0])
              + a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0])) / det;
    }
}

static float zbound(int k)          /* slice k's near boundary, k = 1..LC_TZ - 1 */
{
    return LC_NEAR * powf(LC_FAR / LC_NEAR, (float)(k - 1) / (float)(LC_TZ - 2));
}

/* the clusters' geometry, the camera's alone (no light): fills s->h (but
 * atl and the light count) and g: the column, row and slice planes and the
 * rays through the tiles' corners (brr_metal_cluster.h's uniforms) */
void light_cluster_geom(LightSet *s, int n, const float vp[16], const float eye[3], LightClusterGeom *g)
{
    float row[4][4];             /* the clip transform's rows: clip_r = row[r] . [p 1] */
    float gl;
    int i, k, x, y;
    for (i = 0; i < 4; i++)
        for (k = 0; k < 4; k++)
            row[i][k] = vp[k * 4 + i];
    /* the header: vp as Metal's column-major float4x4 is vp's own memory */
    memcpy(s->h.vp, vp, sizeof s->h.vp);
    memcpy(s->h.eye, eye, 12);
    s->h.eye[3] = (float)n;
    gl = sqrtf(row[3][0] * row[3][0] + row[3][1] * row[3][1] + row[3][2] * row[3][2]);
    if (!(gl > 0))
        gl = 1;
    s->h.zk[0] = 1.0f / gl;
    s->h.zk[1] = (float)(LC_TZ - 2) / log2f(LC_FAR / LC_NEAR);
    s->h.zk[2] = log2f(LC_NEAR);
    s->h.zk[3] = 0;
    memset(s->h.zb, 0, sizeof s->h.zb);
    memset(g, 0, sizeof *g);
    for (k = 1; k < LC_TZ; k++)
        s->h.zb[k] = g->zb[k] = zbound(k);
    s->h.zb[LC_TZ] = 1e30f;
    /* the planes */
    for (k = 0; k <= LC_TX; k++) {
        Plane p = clip_plane(row[0], row[3], -1.0 + 2.0 * k / LC_TX);
        memcpy(g->px[k], p.n, 12); g->px[k][3] = p.d;
    }
    for (k = 0; k <= LC_TY; k++) {
        Plane p = clip_plane(row[1], row[3], -1.0 + 2.0 * k / LC_TY);
        memcpy(g->py[k], p.n, 12); g->py[k][3] = p.d;
    }
    for (k = 1; k < LC_TZ; k++) {            /* view depth w / |grad w| minus the boundary */
        int a;
        for (a = 0; a < 3; a++)
            g->pz[k][a] = row[3][a] / gl;
        g->pz[k][3] = row[3][3] / gl - g->zb[k];
    }
    /* the corners of the closed clusters: the eye plus view depth times the ray
       through each tile corner (scaled to unit view depth) */
    {
        double m[3][3], r[3], p0[3], p1[3];
        for (y = 0; y <= LC_TY; y++)
            for (x = 0; x <= LC_TX; x++) {
                double X = -1.0 + 2.0 * x / LC_TX, Y = -1.0 + 2.0 * y / LC_TY;
                int a;
                for (a = 0; a < 3; a++) {
                    m[0][a] = (double)row[0][a] - X * row[3][a];
                    m[1][a] = (double)row[1][a] - Y * row[3][a];
                    m[2][a] = row[3][a];
                }
                r[0] = -((double)row[0][3] - X * row[3][3]);
                r[1] = -((double)row[1][3] - Y * row[3][3]);
                r[2] = -(double)row[3][3];
                solve3(m, r, p0);                   /* w = 0: the eye */
                r[2] = gl - (double)row[3][3];      /* view depth 1 */
                solve3(m, r, p1);
                for (a = 0; a < 3; a++)
                    g->ray[y][x][a] = (float)(p1[a] - p0[a]);
                if (!x && !y)
                    for (a = 0; a < 3; a++)
                        g->org[a] = (float)p0[a];
            }
    }
}

void light_cluster_build(LightSet *s, int n, const float vp[16], const float eye[3], LightClusterStats *st)
{
    static LightClusterGeom g;
    float zb[LC_TZ + 1];
    Plane px[LC_TX + 1], py[LC_TY + 1], pz[LC_TZ + 1];
    uint64_t bx[LC_TX][2], by[LC_TY][2], bz[LC_TZ][2];
    float org[3];
    int i, k, x, y, z;
    uint32_t off = 0;
    LightClusterStats dummy;
    if (!st)
        st = &dummy;
    memset(st, 0, sizeof *st);
    st->lights = n = n < 0 ? 0 : n > LC_LIGHTS ? LC_LIGHTS : n;
    light_cluster_geom(s, n, vp, eye, &g);
    for (k = 0; k <= LC_TZ; k++)
        zb[k] = g.zb[k];
    for (k = 0; k <= LC_TX; k++) { memcpy(px[k].n, g.px[k], 12); px[k].d = g.px[k][3]; }
    for (k = 0; k <= LC_TY; k++) { memcpy(py[k].n, g.py[k], 12); py[k].d = g.py[k][3]; }
    for (k = 1; k < LC_TZ; k++) { memcpy(pz[k].n, g.pz[k], 12); pz[k].d = g.pz[k][3]; }
    memcpy(org, g.org, sizeof org);
#define ray g.ray
    /* each light against the columns, rows and slices */
    memset(bx, 0, sizeof bx);
    memset(by, 0, sizeof by);
    memset(bz, 0, sizeof bz);
    for (i = 0; i < n; i++) {
        Body b;
        float mn[LC_TZ + 1], mx[LC_TZ + 1];
        int cx = 0, cy = 0, cz = 0;
        uint64_t bit = 1ull << (i & 63);
        body_of(&s->l[i], eye, &b);
        for (k = 0; k <= LC_TX; k++)
            body_span(&b, px[k].n, px[k].d, &mn[k], &mx[k]);
        for (k = 0; k < LC_TX; k++)
            if ((k == 0 || mx[k] >= -b.m) && (k == LC_TX - 1 || mn[k + 1] <= b.m))
                bx[k][i >> 6] |= bit, cx++;
        for (k = 0; k <= LC_TY; k++)
            body_span(&b, py[k].n, py[k].d, &mn[k], &mx[k]);
        for (k = 0; k < LC_TY; k++)
            if ((k == 0 || mx[k] >= -b.m) && (k == LC_TY - 1 || mn[k + 1] <= b.m))
                by[k][i >> 6] |= bit, cy++;
        for (k = 1; k < LC_TZ; k++)
            body_span(&b, pz[k].n, pz[k].d, &mn[k], &mx[k]);
        for (k = 0; k < LC_TZ; k++)
            if ((k == 0 || mx[k] >= -b.m) && (k == LC_TZ - 1 || mn[k + 1] <= b.m))
                bz[k][i >> 6] |= bit, cz++;
        st->cov[i] = cz ? (float)(cx * cy) / (float)(LC_TX * LC_TY) : 0.0f;
    }
    /* the lists */
    for (z = 0; z < LC_TZ; z++)
        for (y = 0; y < LC_TY; y++)
            for (x = 0; x < LC_TX; x++) {
                int c = (z * LC_TY + y) * LC_TX + x, cnt = 0, w;
                int closed = x > 0 && x < LC_TX - 1 && y > 0 && y < LC_TY - 1 && z > 0 && z < LC_TZ - 1;
                float q[3] = { 0, 0, 0 }, rq = 0;
                if (closed) {
                    float pt[8][3];
                    int j, a;
                    for (j = 0; j < 8; j++) {
                        const float *rr = ray[y + (j >> 1 & 1)][x + (j & 1)];
                        float d = zb[z + (j >> 2)];
                        for (a = 0; a < 3; a++) {
                            pt[j][a] = org[a] + rr[a] * d;
                            q[a] += pt[j][a] / 8.0f;
                        }
                    }
                    for (j = 0; j < 8; j++) {
                        float dd[3] = { pt[j][0] - q[0], pt[j][1] - q[1], pt[j][2] - q[2] };
                        rq = fmaxf(rq, sqrtf(dot3(dd, dd)));
                    }
                    rq *= 1.0001f;
                }
                for (w = 0; w < 2; w++) {
                    uint64_t m = bx[x][w] & by[y][w] & bz[z][w];
                    while (m) {
                        int b = __builtin_ctzll(m), li = w * 64 + b;
                        m &= m - 1;
                        st->cand++;
                        if (closed) {
                            Body bd;
                            body_of(&s->l[li], eye, &bd);
                            if (!body_meets_sphere(&bd, q, rq)) {
                                st->refined++;
                                continue;
                            }
                        }
                        s->idx[off + (uint32_t)cnt++] = (uint8_t)li;
                    }
                }
                s->cl[c] = off << 8 | (uint32_t)cnt;
                off += (uint32_t)cnt;
                if (cnt > st->maxlist)
                    st->maxlist = cnt;
            }
    st->entries = off;
    for (i = 0; i < n; i++)
        st->visible += st->cov[i] > 0.0f;
#undef ray
}

int light_cluster_of(const LightSet *s, const float p[3])
{
    const float *m = s->h.vp;
    float c[4], nx, ny, z;
    int k, tx, ty, tz;
    for (k = 0; k < 4; k++)
        c[k] = m[k] * p[0] + m[4 + k] * p[1] + m[8 + k] * p[2] + m[12 + k];
    nx = c[0] / c[3];
    ny = c[1] / c[3];
    tx = (int)floorf((nx * 0.5f + 0.5f) * LC_TX);
    ty = (int)floorf((ny * 0.5f + 0.5f) * LC_TY);
    tx = tx < 0 ? 0 : tx > LC_TX - 1 ? LC_TX - 1 : tx;
    ty = ty < 0 ? 0 : ty > LC_TY - 1 ? LC_TY - 1 : ty;
    if (nx != nx)
        tx = 0;
    if (ny != ny)
        ty = 0;
    z = c[3] * s->h.zk[0];
    tz = 0;
    if (z >= s->h.zb[1]) {
        float t = floorf((log2f(z) - s->h.zk[2]) * s->h.zk[1]);
        tz = t < 0.0f ? 1 : t > (float)(LC_TZ - 2) ? LC_TZ - 1 : 1 + (int)t;
    }
    if (tz > 0 && z < s->h.zb[tz])
        tz--;
    if (tz < LC_TZ - 1 && z >= s->h.zb[tz + 1])
        tz++;
    return (tz * LC_TY + ty) * LC_TX + tx;
}

/* ---- shadows ------------------------------------------------------------------------ */
#define TG_CELL LS_TRACK_CELL        /* metres: the track grid's cells */

static struct {
    unsigned gen;
    int have, ntri, nx, ny;
    float lo[2];
    const float *tri;
    int *start;                      /* nx * ny + 1: each cell's triangles in order */
    int *list;
    float *box;                      /* each cell's triangles' extent: lo xyz, hi xyz */
} s_tg;
static unsigned s_tgbuild;           /* the grid's rebuilds (its GPU copy follows it) */

void light_shadow_track(const float *tri, int ntri, unsigned gen)
{
    float lo[2] = { 1e30f, 1e30f }, hi[2] = { -1e30f, -1e30f };
    int i, k, nc, *fill;
    if (s_tg.have && s_tg.gen == gen && s_tg.tri == tri && s_tg.ntri == ntri)
        return;
    free(s_tg.start);
    free(s_tg.list);
    free(s_tg.box);
    memset(&s_tg, 0, sizeof s_tg);
    if (!tri || ntri <= 0)
        return;
    for (i = 0; i < ntri * 3; i++)
        for (k = 0; k < 2; k++) {
            lo[k] = fminf(lo[k], tri[3 * i + k]);
            hi[k] = fmaxf(hi[k], tri[3 * i + k]);
        }
    s_tg.nx = (int)((hi[0] - lo[0]) / TG_CELL) + 1;
    s_tg.ny = (int)((hi[1] - lo[1]) / TG_CELL) + 1;
    if (s_tg.nx > 1024) s_tg.nx = 1024;
    if (s_tg.ny > 1024) s_tg.ny = 1024;
    nc = s_tg.nx * s_tg.ny;
    s_tg.lo[0] = lo[0];
    s_tg.lo[1] = lo[1];
    s_tg.start = calloc((size_t)nc + 1, sizeof(int));
    s_tg.list = malloc((size_t)ntri * sizeof(int));
    s_tg.box = malloc((size_t)nc * 6 * sizeof(float));
    fill = calloc((size_t)nc, sizeof(int));
    if (!s_tg.start || !s_tg.list || !s_tg.box || !fill) {
        free(fill);
        free(s_tg.start);
        free(s_tg.list);
        free(s_tg.box);
        memset(&s_tg, 0, sizeof s_tg);
        return;
    }
    for (i = 0; i < nc; i++) {
        s_tg.box[6 * i + 0] = s_tg.box[6 * i + 1] = s_tg.box[6 * i + 2] = 1e30f;
        s_tg.box[6 * i + 3] = s_tg.box[6 * i + 4] = s_tg.box[6 * i + 5] = -1e30f;
    }
#define TG_CELLOF(t, cx, cy)                                                                        \
    do {                                                                                            \
        const float *v_ = &tri[9 * (t)];                                                            \
        cx = (int)(((v_[0] + v_[3] + v_[6]) / 3.0f - s_tg.lo[0]) / TG_CELL);                          \
        cy = (int)(((v_[1] + v_[4] + v_[7]) / 3.0f - s_tg.lo[1]) / TG_CELL);                          \
        cx = cx < 0 ? 0 : cx >= s_tg.nx ? s_tg.nx - 1 : cx;                                         \
        cy = cy < 0 ? 0 : cy >= s_tg.ny ? s_tg.ny - 1 : cy;                                         \
    } while (0)
    for (i = 0; i < ntri; i++) {
        int cx, cy, c, v;
        TG_CELLOF(i, cx, cy);
        c = cy * s_tg.nx + cx;
        s_tg.start[c + 1]++;
        for (v = 0; v < 3; v++)
            for (k = 0; k < 3; k++) {
                float q = tri[9 * i + 3 * v + k];
                s_tg.box[6 * c + k] = fminf(s_tg.box[6 * c + k], q);
                s_tg.box[6 * c + 3 + k] = fmaxf(s_tg.box[6 * c + 3 + k], q);
            }
    }
    for (i = 0; i < nc; i++)
        s_tg.start[i + 1] += s_tg.start[i];
    for (i = 0; i < ntri; i++) {
        int cx, cy, c;
        TG_CELLOF(i, cx, cy);
        c = cy * s_tg.nx + cx;
        s_tg.list[s_tg.start[c] + fill[c]++] = i;
    }
#undef TG_CELLOF
    free(fill);
    s_tg.tri = tri;
    s_tg.ntri = ntri;
    s_tg.gen = gen;
    s_tg.have = 1;
    s_tgbuild++;
}

int light_shadow_track_grid(LightTrackGrid *g)
{
    if (!s_tg.have)
        return 0;
    g->build = s_tgbuild;
    g->ntri = s_tg.ntri;
    g->nx = s_tg.nx;
    g->ny = s_tg.ny;
    g->lo[0] = s_tg.lo[0];
    g->lo[1] = s_tg.lo[1];
    g->start = s_tg.start;
    g->list = s_tg.list;
    g->box = s_tg.box;
    return 1;
}

int light_shadow_matrix(const LightSpot *l, LightShadowRec *r)
{
    float f[3], up[3] = { 0, 0, 1 }, rt[3], u[3], len, t, n = LS_NEARP, fr = l->range, A, B;
    int k;
    if ((l->kind > 1.5f && l->kind < 2.5f) || l->kind > 3.5f)
        return 0;                    /* a point: no perspective tile (an octahedral one is to come) */
    if (!(l->cosout >= 0.17364818f) || !(fr > n * 2.0f))
        return 0;                    /* wider than 80 degrees (a rear lamp's half space) */
    len = sqrtf(dot3(l->axis, l->axis));
    if (!(len > 0))
        return 0;
    for (k = 0; k < 3; k++)
        f[k] = l->axis[k] / len;
    if (fabsf(f[2]) > 0.99f)
        up[0] = 1, up[2] = 0;
    rt[0] = f[1] * up[2] - f[2] * up[1];
    rt[1] = f[2] * up[0] - f[0] * up[2];
    rt[2] = f[0] * up[1] - f[1] * up[0];
    len = sqrtf(dot3(rt, rt));
    for (k = 0; k < 3; k++)
        rt[k] /= len;
    u[0] = rt[1] * f[2] - rt[2] * f[1];
    u[1] = rt[2] * f[0] - rt[0] * f[2];
    u[2] = rt[0] * f[1] - rt[1] * f[0];
    t = sqrtf(1.0f - l->cosout * l->cosout) / l->cosout;     /* tan of the outer angle: the square holds the cone */
    A = fr / (fr - n);
    B = -n * fr / (fr - n);
    memset(r, 0, sizeof *r);
    for (k = 0; k < 3; k++) {        /* column k of rows x, y, z, w */
        r->m[k * 4 + 0] = rt[k] / t;
        r->m[k * 4 + 1] = u[k] / t;
        r->m[k * 4 + 2] = A * f[k];
        r->m[k * 4 + 3] = f[k];
    }
    r->m[12] = -dot3(rt, l->pos) / t;
    r->m[13] = -dot3(u, l->pos) / t;
    r->m[14] = -A * dot3(f, l->pos) + B;
    r->m[15] = -dot3(f, l->pos);
    r->prm[0] = A;
    r->prm[1] = B;
    r->prm[2] = t;
    r->prm[3] = n;
    return 1;
}

/* the frustum's six planes from its rows (inside: >= 0) */
static void frustum_planes(const LightShadowRec *r, float pl[6][4])
{
    int k, j;
    for (k = 0; k < 4; k++) {
        float x = r->m[k * 4 + 0], y = r->m[k * 4 + 1], z = r->m[k * 4 + 2], w = r->m[k * 4 + 3];
        pl[0][k] = w - x; pl[1][k] = w + x; pl[2][k] = w - y; pl[3][k] = w + y;
        pl[4][k] = z;     pl[5][k] = w - z;
    }
    for (j = 0; j < 6; j++) {
        float l = sqrtf(pl[j][0] * pl[j][0] + pl[j][1] * pl[j][1] + pl[j][2] * pl[j][2]);
        if (l > 0)
            for (k = 0; k < 4; k++)
                pl[j][k] /= l;
    }
}

static int box_outside(float pl[6][4], const float *lo, const float *hi)
{
    int j;
    for (j = 0; j < 6; j++) {
        float x = pl[j][0] >= 0 ? hi[0] : lo[0], y = pl[j][1] >= 0 ? hi[1] : lo[1], z = pl[j][2] >= 0 ? hi[2] : lo[2];
        if (pl[j][0] * x + pl[j][1] * y + pl[j][2] * z + pl[j][3] < -0.01f)
            return 1;
    }
    return 0;
}

int light_shadow_track_casters(const LightShadowRec *r, const LightSpot *l, uint32_t *out, int max)
{
    float pl[6][4];
    int x0, x1, y0, y1, x, y, n = 0;
    if (!s_tg.have)
        return 0;
    frustum_planes(r, pl);
    x0 = (int)floorf((l->pos[0] - l->range - s_tg.lo[0]) / TG_CELL);
    x1 = (int)floorf((l->pos[0] + l->range - s_tg.lo[0]) / TG_CELL);
    y0 = (int)floorf((l->pos[1] - l->range - s_tg.lo[1]) / TG_CELL);
    y1 = (int)floorf((l->pos[1] + l->range - s_tg.lo[1]) / TG_CELL);
    /* a triangle is binned by its centroid, so its cell's extent (which holds
       all of it) may reach past the cells the light's square covers: the
       margin row of cells around it */
    x0 = x0 - 1 < 0 ? 0 : x0 - 1;
    y0 = y0 - 1 < 0 ? 0 : y0 - 1;
    x1 = x1 + 1 >= s_tg.nx ? s_tg.nx - 1 : x1 + 1;
    y1 = y1 + 1 >= s_tg.ny ? s_tg.ny - 1 : y1 + 1;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            int c = y * s_tg.nx + x, i;
            const float *b = &s_tg.box[6 * c];
            if (s_tg.start[c] == s_tg.start[c + 1] || box_outside(pl, b, b + 3))
                continue;
            for (i = s_tg.start[c]; i < s_tg.start[c + 1]; i++) {
                uint32_t t = (uint32_t)s_tg.list[i];
                if (n + 3 > max)
                    return n;
                out[n++] = 3 * t;
                out[n++] = 3 * t + 1;
                out[n++] = 3 * t + 2;
            }
        }
    return n;
}

/* ---- the plan --------------------------------------------------------------------- */
static struct {
    int has;                 /* the tile at x, y, size holds this light's depth as rec says */
    int x, y, size;
    LightSpot key;           /* the light it was drawn for */
    uint64_t dyn;            /* the moving casters near it then */
    LightShadowRec rec;
    unsigned placed;         /* the frame it last held a tile */
} s_sc[LC_IDS];
static unsigned s_planno = 1;

void light_shadow_reset(void)
{
    int i;
    for (i = 0; i < LC_IDS; i++)
        s_sc[i].has = 0;
}

static uint64_t dyn_hash(const LightSpot *l, const float *dyn, int ndyn)
{
    uint64_t h = 1469598103934665603ull;
    int i, k;
    for (i = 0; i + 2 < ndyn; i += 3) {
        const float *v = &dyn[3 * i];
        int out = 0;
        for (k = 0; k < 3 && !out; k++) {
            float lo = fminf(v[k], fminf(v[3 + k], v[6 + k])), hi = fmaxf(v[k], fmaxf(v[3 + k], v[6 + k]));
            out = lo > l->pos[k] + l->range || hi < l->pos[k] - l->range;
        }
        if (out)
            continue;
        {
            const unsigned char *b = (const unsigned char *)v;
            for (k = 0; k < 36; k++)
                h = (h ^ b[k]) * 1099511628211ull;
        }
    }
    return h;
}

/* Morton order: cell (x, y) of a block grid of side g */
static void demorton(unsigned i, int *x, int *y)
{
    unsigned a = 0, b = 0, k;
    for (k = 0; k < 16; k++) {
        a |= ((i >> (2 * k)) & 1u) << k;
        b |= ((i >> (2 * k + 1)) & 1u) << k;
    }
    *x = (int)a;
    *y = (int)b;
}

#define LS_GRID_MAX 64               /* cells a side: an atlas up to 8192 */
static int occ_free(uint8_t occ[LS_GRID_MAX][LS_GRID_MAX], int x, int y, int s)
{
    int i, j;
    for (j = y; j < y + s; j++)
        for (i = x; i < x + s; i++)
            if (occ[j][i])
                return 0;
    return 1;
}

static void occ_set(uint8_t occ[LS_GRID_MAX][LS_GRID_MAX], int x, int y, int s)
{
    int j;
    for (j = y; j < y + s; j++)
        memset(&occ[j][x], 1, (size_t)s);
}

/* the first free aligned block of s cells in Morton order; 0 when none */
static int occ_place(uint8_t occ[LS_GRID_MAX][LS_GRID_MAX], int g, int s, int *x, int *y)
{
    unsigned i, nb = (unsigned)(g / s) * (unsigned)(g / s);
    for (i = 0; i < nb; i++) {
        int bx, by;
        demorton(i, &bx, &by);
        if (occ_free(occ, bx * s, by * s, s)) {
            *x = bx * s;
            *y = by * s;
            occ_set(occ, *x, *y, s);
            return 1;
        }
    }
    return 0;
}

static const int *s_sort_size;
static const float *s_sort_imp;
static int by_imp(const void *a, const void *b)
{
    float x = s_sort_imp[*(const int *)a], y = s_sort_imp[*(const int *)b];
    return x > y ? -1 : x < y ? 1 : *(const int *)a - *(const int *)b;
}
static int by_size_imp(const void *a, const void *b)
{
    int sa = s_sort_size[*(const int *)a], sb = s_sort_size[*(const int *)b];
    return sa != sb ? sb - sa : by_imp(a, b);
}

void light_shadow_plan(LightSet *s, const int *ids, const LightClusterStats *st, const float eye[3],
                       const float *dyn, int ndyn, int atlas, long budget, LightShadowPlan *p)
{
    static uint8_t occ[LS_GRID_MAX][LS_GRID_MAX];
    int cand[LC_LIGHTS], size[LC_LIGHTS], px[LC_LIGHTS], py[LC_LIGHTS], ok[LC_LIGHTS];
    LightShadowRec rec[LC_LIGHTS];
    int n = (int)s->h.eye[3], nc = 0, i, k, g, fail = 0;
    long area = 0, cap;
    unsigned fr = ++s_planno;
    memset(p, 0, sizeof *p);
    if (atlas > LS_GRID_MAX * LS_TILE_MIN)
        atlas = LS_GRID_MAX * LS_TILE_MIN;
    atlas -= atlas % LS_TILE_MIN;
    g = atlas / LS_TILE_MIN;
    cap = (long)g * g;
    for (i = 0; i < n; i++) {
        memset(&s->s[i], 0, sizeof s->s[i]);
        p->imp[i] = 0;
        ok[i] = 0;
        if (!(st->cov[i] > 0.0f) || g <= 0 || !light_shadow_matrix(&s->l[i], &rec[i]))
            continue;
        {
            float d[3] = { s->l[i].pos[0] - eye[0], s->l[i].pos[1] - eye[1], s->l[i].pos[2] - eye[2] };
            int id = ids ? ids[i] & (LC_IDS - 1) : i;
            p->imp[i] = s->l[i].candela * st->cov[i] / fmaxf(dot3(d, d), 1.0f);
            if (s_sc[id].placed + 1 == fr)
                p->imp[i] *= 1.25f;
        }
        cand[nc++] = i;
        /* the tile's size from the light's share of the screen, in cells */
        size[i] = st->cov[i] > 0.25f ? 8 : st->cov[i] > 0.0625f ? 4 : st->cov[i] > 0.015f ? 2 : 1;
        while (size[i] * LS_TILE_MIN > LS_TILE_MAX)
            size[i] /= 2;
        while (size[i] > g)
            size[i] /= 2;
    }
    s_sort_imp = p->imp;
    s_sort_size = size;
    qsort(cand, (size_t)nc, sizeof cand[0], by_imp);
    for (k = 0; k < nc; k++)
        area += (long)size[cand[k]] * size[cand[k]];
    /* too much: halve the least important tile above the smallest, again and
       again; when all are the smallest, drop the least important */
    while (area > cap) {
        for (k = nc - 1; k >= 0 && size[cand[k]] == 1; k--)
            ;
        if (k >= 0) {
            area -= (long)size[cand[k]] * size[cand[k]] * 3 / 4;
            size[cand[k]] /= 2;
        } else {
            area--;
            nc--;
            p->dropped++;
        }
    }
    /* keep last frame's places where they still fit, then the rest largest first */
    memset(occ, 0, sizeof occ);
    for (k = 0; k < nc; k++) {
        int i2 = cand[k], id = ids ? ids[i2] & (LC_IDS - 1) : i2;
        if (s_sc[id].has && s_sc[id].placed + 1 == fr && s_sc[id].size == size[i2] * LS_TILE_MIN
            && s_sc[id].x / LS_TILE_MIN + size[i2] <= g && s_sc[id].y / LS_TILE_MIN + size[i2] <= g
            && occ_free(occ, s_sc[id].x / LS_TILE_MIN, s_sc[id].y / LS_TILE_MIN, size[i2])) {
            px[i2] = s_sc[id].x / LS_TILE_MIN;
            py[i2] = s_sc[id].y / LS_TILE_MIN;
            occ_set(occ, px[i2], py[i2], size[i2]);
            ok[i2] = 1;
        }
    }
    {
        int ord[LC_LIGHTS];
        memcpy(ord, cand, (size_t)nc * sizeof ord[0]);
        qsort(ord, (size_t)nc, sizeof ord[0], by_size_imp);
        for (k = 0; k < nc && !fail; k++)
            if (!ok[ord[k]])
                fail = !(ok[ord[k]] = occ_place(occ, g, size[ord[k]], &px[ord[k]], &py[ord[k]]));
        if (fail) {                  /* fragmented: pack them all afresh (always fits) */
            memset(occ, 0, sizeof occ);
            for (k = 0; k < nc; k++)
                ok[ord[k]] = occ_place(occ, g, size[ord[k]], &px[ord[k]], &py[ord[k]]);
            p->repacked = 1;
        }
    }
    /* what to draw: the most important first, within the budget */
    for (k = 0; k < nc; k++) {
        int i2 = cand[k], id = ids ? ids[i2] & (LC_IDS - 1) : i2, sz = size[i2] * LS_TILE_MIN;
        int x = px[i2] * LS_TILE_MIN, y = py[i2] * LS_TILE_MIN, same, render = 0;
        uint64_t h;
        if (!ok[i2] || getenv("BR_LIGHT_NOLAMPSHADOW"))   /* diagnostic: every lamp unshadowed */
            continue;
        h = dyn_hash(&s->l[i2], dyn, ndyn);
        same = s_sc[id].has && s_sc[id].placed + 1 == fr && s_sc[id].x == x && s_sc[id].y == y && s_sc[id].size == sz;
        if (same && !memcmp(&s_sc[id].key, &s->l[i2], sizeof s->l[i2]) && s_sc[id].dyn == h) {
            p->cached++;
        } else if (p->texels + (long)sz * sz <= budget) {
            render = 1;
            p->texels += (long)sz * sz;
            p->rendered++;
            s_sc[id].has = 1;
            s_sc[id].key = s->l[i2];
            s_sc[id].dyn = h;
            s_sc[id].rec = rec[i2];
        } else if (same && s_sc[id].dyn == h) {
            /* last frame's depth, with last frame's transform: right only while the
               moving casters near it have not moved; else a moving car is shadowed by
               where it was (its whole rear, metal_test.m's car box), so unshadowed */
            p->stale++;
        } else {
            s_sc[id].has = 0;        /* no depth here for it yet: unshadowed this frame */
            s_sc[id].placed = fr;
            s_sc[id].x = x;
            s_sc[id].y = y;
            s_sc[id].size = sz;
            p->dropped++;
            continue;
        }
        s_sc[id].placed = fr;
        s_sc[id].x = x;
        s_sc[id].y = y;
        s_sc[id].size = sz;
        s->s[i2] = s_sc[id].rec;
        s->s[i2].rect[0] = (float)x / (float)atlas;
        s->s[i2].rect[1] = (float)y / (float)atlas;
        s->s[i2].rect[2] = (float)sz / (float)atlas;
        s->s[i2].rect[3] = 1.0f;
        p->t[p->n].light = i2;
        p->t[p->n].x = x;
        p->t[p->n].y = y;
        p->t[p->n].size = sz;
        p->t[p->n].render = render;
        p->n++;
    }
    /* a light without a tile this frame keeps no claim on its old one */
    for (i = 0; i < LC_IDS; i++)
        if (s_sc[i].placed != fr)
            s_sc[i].has = 0;
    s->h.atl[0] = (float)atlas;
    s->h.atl[1] = 1.0f / (float)(atlas > 0 ? atlas : 1);
    s->h.atl[2] = (float)p->n;
    s->h.atl[3] = 0;
}
