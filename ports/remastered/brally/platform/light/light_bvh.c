/* light_bvh.c: the track's bounding volume hierarchy (light_bvh.h; DESIGN.md
 * 8.1).  Built with binned SAH (Wald 2007): 16 bins on each axis of the
 * centroids' extent, traversal and triangle costs 1, leaves of at most 4
 * triangles.  Depth first, so a node's first child follows it and the
 * traversal stacks only the far child.
 *
 * The traversals here are the reference for the GPU's (brr_metal_rt.h): the
 * same operations in the same order, every multiply-add an explicit fma and
 * the rest built without contraction (-ffp-contract=off), so the GPU's,
 * compiled without fast math, agrees bit for bit. */
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "light_bvh.h"

#define BINS 16
#define LEAF_MAX 4
#define DEPTH_MAX 32            /* the traversals stack at most this many (RT_STACK) */
#define FAR_GROW 1.0000004f      /* 1 + 2 gamma(3): a slab's exit, rounded outward (Ize 2013) */

typedef struct { float lo[3], hi[3]; } Box;

static void box_empty(Box *b)
{
    int k;
    for (k = 0; k < 3; k++) {
        b->lo[k] = FLT_MAX;
        b->hi[k] = -FLT_MAX;
    }
}

static void box_grow(Box *b, const Box *c)
{
    int k;
    for (k = 0; k < 3; k++) {
        if (c->lo[k] < b->lo[k]) b->lo[k] = c->lo[k];
        if (c->hi[k] > b->hi[k]) b->hi[k] = c->hi[k];
    }
}

static float box_area(const Box *b)
{
    float x = b->hi[0] - b->lo[0], y = b->hi[1] - b->lo[1], z = b->hi[2] - b->lo[2];
    if (x < 0.0f)
        return 0.0f;
    return 2.0f * (x * y + y * z + z * x);
}

typedef struct {
    LightBvh *b;
    Box *tb;                 /* each triangle's bounds */
    float *cen;              /* and centroid */
    uint32_t *idx;           /* the triangles, partitioned as the tree is built */
    double sah;
    float root_area;
} Build;

static int node_new(Build *c)
{
    return c->b->nnode++;
}

static void make_leaf(Build *c, int ni, const Box *nb, int first, int n, int depth)
{
    LightBvhNode *nd = &c->b->node[ni];
    nd->a = first;
    nd->n = n;
    c->b->nleaf++;
    if (depth > c->b->depth)
        c->b->depth = depth;
    c->sah += c->root_area > 0.0f ? (double)box_area(nb) / c->root_area * n : n;
}

static void build(Build *c, int ni, int first, int n, int depth)
{
    LightBvhNode *nd = &c->b->node[ni];
    Box nb, cb, bin[3][BINS], lb[BINS];
    int cnt[3][BINS], ln[BINS];
    int k, i, j, axis = -1, split = 0, mid, left;
    float best = FLT_MAX, scale[3];

    box_empty(&nb);
    box_empty(&cb);
    for (i = first; i < first + n; i++) {
        uint32_t t = c->idx[i];
        Box p;
        box_grow(&nb, &c->tb[t]);
        for (k = 0; k < 3; k++)
            p.lo[k] = p.hi[k] = c->cen[3 * t + k];
        box_grow(&cb, &p);
    }
    memcpy(nd->lo, nb.lo, sizeof nd->lo);
    memcpy(nd->hi, nb.hi, sizeof nd->hi);
    if (n == 1 || depth >= DEPTH_MAX) {
        make_leaf(c, ni, &nb, first, n, depth);
        return;
    }

    /* binned: each axis's centroid extent in 16, each bin's bounds and count,
     * then the 15 planes between them priced by SAH */
    for (k = 0; k < 3; k++) {
        float ext = cb.hi[k] - cb.lo[k];
        scale[k] = ext > 0.0f ? (float)BINS * (1.0f - 1e-6f) / ext : 0.0f;
        for (j = 0; j < BINS; j++) {
            box_empty(&bin[k][j]);
            cnt[k][j] = 0;
        }
    }
    for (i = first; i < first + n; i++) {
        uint32_t t = c->idx[i];
        for (k = 0; k < 3; k++) {
            if (scale[k] == 0.0f)
                continue;
            j = (int)((c->cen[3 * t + k] - cb.lo[k]) * scale[k]);
            if (j > BINS - 1) j = BINS - 1;
            if (j < 0) j = 0;
            box_grow(&bin[k][j], &c->tb[t]);
            cnt[k][j]++;
        }
    }
    for (k = 0; k < 3; k++) {
        Box acc;
        int an = 0;
        if (scale[k] == 0.0f)
            continue;
        box_empty(&acc);
        for (j = 0; j < BINS - 1; j++) {
            box_grow(&acc, &bin[k][j]);
            an += cnt[k][j];
            lb[j] = acc;
            ln[j] = an;
        }
        box_empty(&acc);
        an = 0;
        for (j = BINS - 1; j > 0; j--) {
            float cost;
            box_grow(&acc, &bin[k][j]);
            an += cnt[k][j];
            if (!ln[j - 1] || !an)
                continue;
            cost = box_area(&lb[j - 1]) * ln[j - 1] + box_area(&acc) * an;
            if (cost < best) {
                best = cost;
                axis = k;
                split = j;
            }
        }
    }

    if (axis >= 0) {
        /* leaf or split, by SAH: a leaf costs n, a split 1 + its children */
        float area = box_area(&nb);
        float split_cost = 1.0f + (area > 0.0f ? best / area : 0.0f);
        if (n <= LEAF_MAX && (float)n <= split_cost) {
            make_leaf(c, ni, &nb, first, n, depth);
            return;
        }
        i = first;
        j = first + n - 1;
        while (i <= j) {
            uint32_t t = c->idx[i];
            int b = (int)((c->cen[3 * t + axis] - cb.lo[axis]) * scale[axis]);
            if (b > BINS - 1) b = BINS - 1;
            if (b < 0) b = 0;
            if (b < split) {
                i++;
            } else {
                c->idx[i] = c->idx[j];
                c->idx[j--] = t;
            }
        }
        mid = i;
    } else {
        /* every centroid in one point (duplicated or degenerate triangles):
         * a leaf if it is small, else halves by order */
        if (n <= LEAF_MAX) {
            make_leaf(c, ni, &nb, first, n, depth);
            return;
        }
        mid = first + n / 2;
    }
    if (mid == first || mid == first + n)
        mid = first + n / 2;     /* cannot happen with nonempty bins on both sides; kept as a guard */

    nd->n = 0;
    c->sah += c->root_area > 0.0f ? (double)box_area(&nb) / c->root_area : 1.0;
    left = node_new(c);
    build(c, left, first, mid - first, depth + 1);
    k = node_new(c);
    c->b->node[ni].a = k;
    build(c, k, mid, first + n - mid, depth + 1);
}

int light_bvh_build(LightBvh *b, const float *tri, const int *obj, int ntri)
{
    Build c;
    int i, k;
    memset(b, 0, sizeof *b);
    if (ntri <= 0)
        return 0;
    memset(&c, 0, sizeof c);
    c.b = b;
    b->node = malloc((size_t)(2 * ntri) * sizeof *b->node);
    b->tri = malloc((size_t)ntri * 9 * sizeof *b->tri);
    b->id = malloc((size_t)ntri * sizeof *b->id);
    b->prim = calloc((size_t)ntri, sizeof *b->prim);
    c.tb = malloc((size_t)ntri * sizeof *c.tb);
    c.cen = malloc((size_t)ntri * 3 * sizeof *c.cen);
    c.idx = b->id;
    if (!b->node || !b->tri || !b->id || !b->prim || !c.tb || !c.cen) {
        free(c.tb);
        free(c.cen);
        light_bvh_free(b);
        return 0;
    }
    for (i = 0; i < ntri; i++) {
        const float *v = tri + 9 * i;
        for (k = 0; k < 3; k++) {
            float a = v[k], bb = v[3 + k], cc = v[6 + k];
            c.tb[i].lo[k] = fminf(a, fminf(bb, cc));
            c.tb[i].hi[k] = fmaxf(a, fmaxf(bb, cc));
            c.cen[3 * i + k] = 0.5f * (c.tb[i].lo[k] + c.tb[i].hi[k]);
        }
        c.idx[i] = (uint32_t)i;
        b->prim[i].obj = obj ? obj[i] : 0;
    }
    {
        Box rb;
        box_empty(&rb);
        for (i = 0; i < ntri; i++)
            box_grow(&rb, &c.tb[i]);
        c.root_area = box_area(&rb);
    }
    b->ntri = ntri;
    build(&c, node_new(&c), 0, ntri, 0);
    b->sah = (float)c.sah;
    for (i = 0; i < ntri; i++)
        memcpy(b->tri + 9 * i, tri + 9 * b->id[i], 9 * sizeof *tri);
    free(c.tb);
    free(c.cen);
    return 1;
}

void light_bvh_free(LightBvh *b)
{
    free(b->node);
    free(b->tri);
    free(b->id);
    free(b->prim);
    memset(b, 0, sizeof *b);
}

/* ---- traversal --------------------------------------------------------------
 * The ray, prepared once: its reciprocal for the slabs, and for the triangle
 * test the axis it is longest in (kz) and the shear that makes it that axis
 * (Woop et al. 2013). */
typedef struct {
    float o[3], d[3], inv[3];
    int kx, ky, kz;
    float sx, sy, sz;
} Ray;

static void ray_init(Ray *r, const float o[3], const float d[3])
{
    int k;
    float ax = fabsf(d[0]), ay = fabsf(d[1]), az = fabsf(d[2]);
    for (k = 0; k < 3; k++) {
        r->o[k] = o[k];
        r->d[k] = d[k];
        r->inv[k] = 1.0f / d[k];
    }
    r->kz = ax > ay ? (ax > az ? 0 : 2) : (ay > az ? 1 : 2);
    r->kx = r->kz == 2 ? 0 : r->kz + 1;
    r->ky = r->kx == 2 ? 0 : r->kx + 1;
    if (d[r->kz] < 0.0f) {
        k = r->kx;
        r->kx = r->ky;
        r->ky = k;
    }
    r->sz = 1.0f / d[r->kz];
    r->sx = d[r->kx] * r->sz;
    r->sy = d[r->ky] * r->sz;
}

/* a * b - c * d with a relative error of at most two units in the last
 * place (Kahan; Jeannerod, Louvet and Muller 2013): its sign is the exact
 * sign, and it is zero only when the exact value is.  Plain float loses the
 * edge functions of a small, thin triangle far from the ray to cancellation:
 * two of them rounded to zero and a ray 100 m away "hit" a millimetre
 * sliver (the soup scene, bvh_test.c).  This replaces the paper's double
 * precision fallback, which the GPU has not got. */
static float diff_prod(float a, float b, float c, float d)
{
    float w = c * d;
    float e = fmaf(-c, d, w);
    float f = fmaf(a, b, -w);
    return f + e;
}

/* watertight: a ray through an edge or corner shared by two triangles hits
 * at least one of them, since both see the exact sign of the edge function */
static int tri_test(const Ray *r, const float *v, float tmin, float tmax, float *t, float *u, float *w)
{
    float A[3], B[3], C[3];
    float Ax, Ay, Bx, By, Cx, Cy, Az, Bz, Cz, U, V, W, det, T, rcp, tt;
    int k;
    for (k = 0; k < 3; k++) {
        A[k] = v[k] - r->o[k];
        B[k] = v[3 + k] - r->o[k];
        C[k] = v[6 + k] - r->o[k];
    }
    Ax = fmaf(-r->sx, A[r->kz], A[r->kx]);
    Ay = fmaf(-r->sy, A[r->kz], A[r->ky]);
    Bx = fmaf(-r->sx, B[r->kz], B[r->kx]);
    By = fmaf(-r->sy, B[r->kz], B[r->ky]);
    Cx = fmaf(-r->sx, C[r->kz], C[r->kx]);
    Cy = fmaf(-r->sy, C[r->kz], C[r->ky]);
    U = diff_prod(Cx, By, Cy, Bx);
    V = diff_prod(Ax, Cy, Ay, Cx);
    W = diff_prod(Bx, Ay, By, Ax);
    if ((U < 0.0f || V < 0.0f || W < 0.0f) && (U > 0.0f || V > 0.0f || W > 0.0f))
        return 0;
    det = (U + V) + W;
    if (det == 0.0f)
        return 0;
    Az = r->sz * A[r->kz];
    Bz = r->sz * B[r->kz];
    Cz = r->sz * C[r->kz];
    T = fmaf(U, Az, fmaf(V, Bz, W * Cz));
    rcp = 1.0f / det;
    tt = T * rcp;
    if (!(tt > tmin && tt < tmax))
        return 0;
    *t = tt;
    *u = V * rcp;
    *w = W * rcp;
    return 1;
}

int light_tri_hit(const float o[3], const float d[3], const float *v, float tmin, float tmax,
                  float *t, float *u, float *w)
{
    Ray r;
    ray_init(&r, o, d);
    return tri_test(&r, v, tmin, tmax, t, u, w);
}

/* the slabs; where the box is entered (or tmin) in *tn.  NaN (an origin on a
 * slab of a ray parallel to it) leaves the interval as it was. */
static int box_test(const Ray *r, const LightBvhNode *nd, float tmin, float tmax, float *tn)
{
    float t0 = tmin, t1 = tmax;
    int k;
    for (k = 0; k < 3; k++) {
        float a = (nd->lo[k] - r->o[k]) * r->inv[k];
        float b = (nd->hi[k] - r->o[k]) * r->inv[k];
        float n = a < b ? a : b, f = a < b ? b : a;
        f = f * FAR_GROW;
        t0 = n > t0 ? n : t0;
        t1 = f < t1 ? f : t1;
    }
    *tn = t0;
    return t0 <= t1;
}

#define STACK 32

static int trace(const LightBvh *b, const float o[3], const float d[3], float tmin, float tmax,
                 LightHit *h, LightBvhStats *st, int any)
{
    Ray r;
    int stk[STACK];
    float stt[STACK];
    int sp = 0, i = 0, hit = -1;
    float best = tmax, bu = 0.0f, bv = 0.0f, tn;
    uint64_t nn = 0, nt = 0;

    if (!b->nnode)
        return 0;
    ray_init(&r, o, d);
    if (!box_test(&r, &b->node[0], tmin, best, &tn))
        goto done;
    for (;;) {
        const LightBvhNode *nd = &b->node[i];
        nn++;
        if (nd->n) {
            int j;
            for (j = nd->a; j < nd->a + nd->n; j++) {
                float t, u, v;
                nt++;
                if (tri_test(&r, b->tri + 9 * j, tmin, best, &t, &u, &v)) {
                    best = t;
                    bu = u;
                    bv = v;
                    hit = j;
                    if (any)
                        goto done;
                }
            }
        } else {
            float tl, tr;
            int l = i + 1, rr = nd->a;
            int hl = box_test(&r, &b->node[l], tmin, best, &tl);
            int hr = box_test(&r, &b->node[rr], tmin, best, &tr);
            if (hl && hr) {
                int nearer = tl <= tr ? l : rr;
                stk[sp] = nearer == l ? rr : l;
                stt[sp++] = nearer == l ? tr : tl;
                i = nearer;
                continue;
            }
            if (hl) { i = l; continue; }
            if (hr) { i = rr; continue; }
        }
        /* the next stacked node that a closer hit has not ruled out */
        for (;;) {
            if (!sp)
                goto done;
            sp--;
            if (stt[sp] <= best)
                break;
        }
        i = stk[sp];
    }
done:
    if (st) {
        st->nodes += nn;
        st->tris += nt;
    }
    if (hit < 0)
        return 0;
    if (h) {
        h->t = best;
        h->u = bu;
        h->v = bv;
        h->tri = (int)b->id[hit];
    }
    return 1;
}

int light_bvh_closest(const LightBvh *b, const float o[3], const float d[3], float tmin, float tmax,
                      LightHit *h, LightBvhStats *st)
{
    if (h) {
        h->t = tmax;
        h->u = h->v = 0.0f;
        h->tri = -1;
    }
    return trace(b, o, d, tmin, tmax, h, st, 0);
}

int light_bvh_any(const LightBvh *b, const float o[3], const float d[3], float tmin, float tmax,
                  LightBvhStats *st)
{
    return trace(b, o, d, tmin, tmax, NULL, st, 1);
}
