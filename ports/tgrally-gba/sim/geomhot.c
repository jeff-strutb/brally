/* geomhot.c -- the geometry kernels the race calls most (geometry/mat3.c's rotations and
 * products, BrQuatToMat, libultra's guMtxCatF): on the GBA they are ARM code in IWRAM with
 * the multiply in line (gba/place.txt puts them in IWRAM). */
#include "sim.h"
#if defined(__arm__) && !defined(FX_FLOAT)
#undef FMUL
#define FMUL(a, b) fx_muli((a), (b))
#endif

/* BrMat4RotateVec (0x802586C0): out = m v (rows: world to body) */
void BrMat4RotateVec(fx out[3], fx m[4][4], const fx v[3])
{
    int i, k;
    for (i = 0; i < 3; i++) {
        out[i] = FX(0.0);
        for (k = 0; k < 3; k++)
            out[i] += FMUL(m[i][k], v[k]);
    }
}

/* BrMat4RotateVecT (0x80258758): out = m^T v (body to world) */
void BrMat4RotateVecT(fx out[3], fx m[4][4], const fx v[3])
{
    int i, k;
    for (i = 0; i < 3; i++) {
        out[i] = FX(0.0);
        for (k = 0; k < 3; k++)
            out[i] += FMUL(m[k][i], v[k]);
    }
}

/* BrMat3MulVecRows: m^T v plus m's translation (a body point into the world) */
void BrMat3MulVecRows(fx out[3], fx m[4][4], const fx v[3])
{
    int i, k;
    for (i = 0; i < 3; i++) {
        out[i] = FX(0.0);
        for (k = 0; k < 3; k++)
            out[i] += FMUL(m[k][i], v[k]);
    }
    out[0] += m[3][0];
    out[1] += m[3][1];
    out[2] += m[3][2];
}

void BrMat3MulVec(fx out[3], fx m[3][3], const fx v[3])
{
    int i, k;
    for (i = 0; i < 3; i++) {
        out[i] = FX(0.0);
        for (k = 0; k < 3; k++)
            out[i] += FMUL(m[i][k], v[k]);
    }
}

void BrMat3Mul(fx out[3][3], fx a[3][3], fx b[3][3])
{
    int i, j;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            out[i][j] = FMUL(a[i][0], b[0][j]) + FMUL(a[i][1], b[1][j]) + FMUL(a[i][2], b[2][j]);
}

/* BrQuatToMat: the rotation of a unit quaternion (w, x, y, z) and the state's position */
void BrQuatToMat(fx m[4][4], const RbState *p)
{
    fx ww, xx, yy, zz, xy2, wz2, xz2, wy2, yz2, wx2;
    const fx *q = p->q;

    ww = FMUL(q[0], q[0]);
    xx = FMUL(q[1], q[1]);
    yy = FMUL(q[2], q[2]);
    zz = FMUL(q[3], q[3]);
    xy2 = FMUL(q[2], FMUL(q[1], FX(2.0)));
    wz2 = FMUL(q[3], FMUL(q[0], FX(2.0)));
    xz2 = FMUL(q[3], FMUL(q[1], FX(2.0)));
    wy2 = FMUL(q[2], FMUL(q[0], FX(2.0)));
    yz2 = FMUL(q[3], FMUL(q[2], FX(2.0)));
    wx2 = FMUL(q[1], FMUL(q[0], FX(2.0)));
    m[0][0] = ww + xx - yy - zz;
    m[0][1] = xy2 + wz2;
    m[0][2] = xz2 - wy2;
    m[0][3] = FX(0.0);
    m[1][0] = xy2 - wz2;
    m[1][1] = ww - xx + yy - zz;
    m[1][2] = yz2 + wx2;
    m[1][3] = FX(0.0);
    m[2][0] = xz2 + wy2;
    m[2][1] = yz2 - wx2;
    m[2][2] = ww - xx - yy + zz;
    m[2][3] = FX(0.0);
    m[3][0] = p->pos[0];
    m[3][1] = p->pos[1];
    m[3][2] = p->pos[2];
    m[3][3] = FX(1.0);
}

void guMtxCatF(fx mf[4][4], fx nf[4][4], fx res[4][4])
{
    int i, j, k;
    fx temp[4][4];
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++) {
            temp[i][j] = FX(0.0);
            for (k = 0; k < 4; k++)
                temp[i][j] += FMUL(mf[i][k], nf[k][j]);
        }
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            res[i][j] = temp[i][j];
}

/* sim_cell_pick: the entries of a cell (their indices, in order) whose triangle's box meets
   the box q (sim_pick_box): the triangles a test about q can find, the rest passed by */
int sim_cell_pick(const SimCell *c, const int32_t q[6], uint16_t *out)
{
    const uint16_t *t = c->tris;
    const Plane *pl = g_track->planes;
    int32_t x0 = q[0], x1 = q[1], y0 = q[2], y1 = q[3], z0 = q[4], z1 = q[5];
    int i, n = c->n, k = 0;
    for (i = 0; i < n; i++) {
        const int16_t *b = pl[t[i]].bx;
        if (b[0] > x1 || b[1] < x0 || b[2] > y1 || b[3] < y0 || b[4] > z1 || b[5] < z0)
            continue;
        out[k++] = (uint16_t)i;
    }
    return k;
}

#ifndef FX_FLOAT
/* ---- sim_tri_box: tri_in_box and BrTriCubeTest (driving/obb.c, Voorhies' triangle against the
   unit cube) in 32 bits.  The box's frame is small: the first corner goes through m in 32.32,
   the two edges through m's rotation in 16.16 and 2.30, and the tests run in 8.24 with their
   products in 64 bits.  -1: a triangle too big for 8.24 (the caller takes the 32.32 way). ---- */
#define H24 (1 << 23)                           /* 0.5 */
#define O24 (1 << 24)                           /* 1.0 */
#define T24 (3 << 23)                           /* 1.5 */
#define LIM24 (40 << 24)                        /* the corners within 40 */
#define PAIR24(cum, out, expr, bp, bn, lim) \
    if ((cum) & ((bp) | (bn))) { \
        int32_t s_ = (expr); \
        if (((cum) & (bp)) && s_ > (lim)) (out) |= (bp); \
        else if (((cum) & (bn)) && s_ < -(lim)) (out) |= (bn); \
    }

static int trivial24(int32_t v[3][3])
{
    int cum, i, bits;
    cum = ~0;
    for (i = 0; i < 3; i++) {
        int fb = 0;
        const int32_t *p = v[i];
        PAIR24(~0, fb, p[0], 0x01, 0x02, H24)
        PAIR24(~0, fb, p[1], 0x04, 0x08, H24)
        PAIR24(~0, fb, p[2], 0x10, 0x20, H24)
        if (fb == 0)
            return 1;
        cum &= fb;
    }
    if (cum != 0)
        return 0;
    cum = ~0;
    for (i = 0; i < 3; i++) {
        bits = 0;
        PAIR24(cum, bits, v[i][0] + v[i][1], 0x001, 0x002, O24)
        PAIR24(cum, bits, v[i][0] - v[i][1], 0x004, 0x008, O24)
        PAIR24(cum, bits, v[i][0] + v[i][2], 0x010, 0x020, O24)
        PAIR24(cum, bits, v[i][0] - v[i][2], 0x040, 0x080, O24)
        PAIR24(cum, bits, v[i][1] + v[i][2], 0x100, 0x200, O24)
        PAIR24(cum, bits, v[i][1] - v[i][2], 0x400, 0x800, O24)
        cum = bits;
        if (cum == 0)
            break;
    }
    if (cum != 0)
        return 0;
    cum = ~0;
    for (i = 0; i < 3; i++) {
        bits = 0;
        PAIR24(cum, bits, v[i][0] + v[i][1] + v[i][2], 0x01, 0x02, T24)
        PAIR24(cum, bits, v[i][0] + v[i][1] - v[i][2], 0x04, 0x08, T24)
        PAIR24(cum, bits, v[i][0] - v[i][1] + v[i][2], 0x10, 0x20, T24)
        PAIR24(cum, bits, v[i][0] - v[i][1] - v[i][2], 0x40, 0x80, T24)
        cum = bits;
        if (cum == 0)
            break;
    }
    if (cum != 0)
        return 0;
    return -1;
}

static const signed char s_m3[5] = { 0, 1, 2, 0, 1 };

static int seg24(const int32_t *v0, const int32_t *v1)
{
    int32_t e[3], sg[3];
    int i;
    for (i = 0; i < 3; i++) {
        e[i] = v1[i] - v0[i];
        sg[i] = e[i] < 0 ? -1 : 1;
    }
    for (i = 0; i < 3; i++) {
        if (v0[i] * sg[i] > H24)
            return 0;
        if (v1[i] * sg[i] < -H24)
            return 0;
    }
    for (i = 0; i < 3; i++) {
        int i1 = s_m3[i + 1], i2 = s_m3[i + 2];
        int64_t a = (int64_t)e[i2] * v0[i1] - (int64_t)e[i1] * v0[i2];          /* 16.48 */
        int64_t b = ((int64_t)e[i2] * sg[i1] + (int64_t)e[i1] * sg[i2]) << 23;    /* x 0.5, 16.48 */
        if (a < 0) a = -a;
        if (b < 0) b = -b;
        if (a > b)
            return 0;
    }
    return 1;
}

static int contains24(int32_t v[3][3], const int32_t *n, const int32_t *p)
{
    int32_t an[3];
    int z, x, y, i, count = 0, xd;
    for (i = 0; i < 3; i++)
        an[i] = n[i] < 0 ? -n[i] : n[i];
    z = an[0] > an[2] ? (an[0] > an[1] ? 0 : 1) : (an[1] > an[2] ? 1 : 2);
    if (n[z] < 0) {
        x = s_m3[z + 2];
        y = s_m3[z + 1];
    } else {
        x = s_m3[z + 1];
        y = s_m3[z + 2];
    }
    for (i = 0; i < 3; i++) {
        const int32_t *a = v[i], *b = v[s_m3[i + 1]];
        xd = (b[x] > p[x]) - (a[x] > p[x]);
        if (xd) {
            if ((b[y] > p[y]) - (a[y] > p[y])) {
                int64_t l = (int64_t)(p[x] - a[x]) * (b[y] - a[y]), r = (int64_t)(p[y] - a[y]) * (b[x] - a[x]);
                if (xd > 0 ? l <= r : l >= r)
                    count += xd;
            } else if (a[y] <= p[y]) {
                count += xd;
            }
        }
    }
    return count;
}

int sim_tri_box(fx v[3][3], fx nrm[3], fx m[4][4], const Plane *pP)
{
    int32_t vb[3][3], e1w[3], e2w[3], n[3];
    int64_t cr[3], big;
    fx t0[3];
    int i, k, r, sh;

    BrMat3MulVecRows(t0, m, pP->v0);
    for (k = 0; k < 3; k++) {
        if (t0[k] >= (fx)LIM24 << 8 || t0[k] <= -((fx)LIM24 << 8))
            return -1;
        vb[0][k] = (int32_t)(t0[k] >> 8);
        e1w[k] = (int32_t)((pP->v1[k] - pP->v0[k]) >> 16);
        e2w[k] = (int32_t)((pP->v2[k] - pP->v0[k]) >> 16);
    }
    for (i = 0; i < 3; i++) {                    /* the edges through the rotation: 2.30 x 16.16 */
        int64_t a = 0, b = 0;
        for (k = 0; k < 3; k++) {
            int32_t rk = (int32_t)(m[k][i] >> 2);
            a += (int64_t)rk * e1w[k];
            b += (int64_t)rk * e2w[k];
        }
        a >>= 22;
        b >>= 22;
        a += vb[0][i];
        b += vb[0][i];
        if (a >= LIM24 || a <= -LIM24 || b >= LIM24 || b <= -LIM24)
            return -1;
        vb[1][i] = (int32_t)a;
        vb[2][i] = (int32_t)b;
    }
    {
        int32_t a1[3], a2[3];
        for (k = 0; k < 3; k++) {
            a1[k] = vb[1][k] - vb[0][k];
            a2[k] = vb[2][k] - vb[0][k];
        }
        cr[0] = (int64_t)a1[1] * a2[2] - (int64_t)a1[2] * a2[1];   /* 16.48 */
        cr[1] = (int64_t)a1[2] * a2[0] - (int64_t)a1[0] * a2[2];
        cr[2] = (int64_t)a1[0] * a2[1] - (int64_t)a1[1] * a2[0];
    }
    r = trivial24(vb);
    if (r == -1) {
        r = 0;
        for (i = 0; i < 3 && !r; i++)
            r = seg24(vb[i], vb[s_m3[i + 1]]);
        if (!r) {                                /* the cube's diagonal nearest the normal */
            int64_t num = 0, den = 0, t;
            int32_t p[3];
            big = 0;
            for (k = 0; k < 3; k++)
                big |= cr[k] < 0 ? -cr[k] : cr[k];
            for (sh = 0; (big >> sh) >= (1 << 28); sh++)
                ;
            for (k = 0; k < 3; k++) {
                n[k] = (int32_t)(cr[k] >> sh);
                num += (int64_t)n[k] * vb[0][k];
                den += n[k] < 0 ? -n[k] : n[k];  /* the normal on its diagonal (each sign 1 or -1) */
            }
            if (den != 0 && 2 * (num < 0 ? -num : num) <= den << 24) {
                t = FDIV(num, den << 32);        /* 8.24 */
                for (k = 0; k < 3; k++)
                    p[k] = n[k] < 0 ? -(int32_t)t : (int32_t)t;
                r = contains24(vb, n, p) != 0;
            }
        }
    }
    if (r && v) {
        for (i = 0; i < 3; i++)
            for (k = 0; k < 3; k++)
                v[i][k] = (fx)vb[i][k] << 8;
        for (k = 0; k < 3; k++)
            nrm[k] = cr[k] >> 16;
    }
    return r;
}

/* BrCrTriContainsPoint in integers: the point and the corners relative to v0 in 16.16 on the
   two axes the normal is least along, u and v by cross products against their determinant
   (no divide, exact) */
int sim_tri_contains(const Plane *pT, const fx *pP)
{
    fx a0, a1;
    int c, i1, i2;
    int32_t d1, d2, b1, b2, c1, c2;
    int64_t U, V, D;

    a0 = pT->n[0] < 0 ? -pT->n[0] : pT->n[0];
    a1 = pT->n[1] < 0 ? -pT->n[1] : pT->n[1];
    if (a1 < a0) {
        a1 = pT->n[2] < 0 ? -pT->n[2] : pT->n[2];
        c = a1 < a0 ? 0 : 2;
    } else {
        a0 = a1;
        a1 = pT->n[2] < 0 ? -pT->n[2] : pT->n[2];
        c = a1 < a0 ? 1 : 2;
    }
    i1 = s_m3[c + 1];
    i2 = s_m3[c + 2];
    d1 = (int32_t)((pP[i1] - pT->v0[i1]) >> 16);
    d2 = (int32_t)((pP[i2] - pT->v0[i2]) >> 16);
    b1 = (int32_t)((pT->v1[i1] - pT->v0[i1]) >> 16);
    b2 = (int32_t)((pT->v1[i2] - pT->v0[i2]) >> 16);
    c1 = (int32_t)((pT->v2[i1] - pT->v0[i1]) >> 16);
    c2 = (int32_t)((pT->v2[i2] - pT->v0[i2]) >> 16);
    D = (int64_t)c2 * b1 - (int64_t)c1 * b2;
    U = (int64_t)d2 * b1 - (int64_t)d1 * b2;
    V = (int64_t)d1 * c2 - (int64_t)c1 * d2;
    if (D < 0) {
        D = -D;
        U = -U;
        V = -V;
    }
    return D != 0 && U >= 0 && V >= 0 && U + V <= D;
}

/* BrWheelGroundProbe's walk of the picked triangles in integers: each plane's distance from
   the point relative to its first corner (normals 2.30 by 16.16), the drop along dir by one
   divide where the plane is near enough and faces up */
static inline __attribute__((always_inline)) fx probe_one(Body *w, const Plane *p, const fx *world, const fx *dir,
                                                            const int32_t *d30, fx best);

/* the plane the wheel was on last tick, alone: still under it, it is the ground (the walk below
   would find the same, short of triangles lying over each other) */
fx sim_probe_last(Body *w, const Plane *p, const fx *world, const fx *dir)
{
    int32_t d30[3];
    int j;
    for (j = 0; j < 3; j++)
        d30[j] = (int32_t)(dir[j] >> 2);
    return probe_one(w, p, world, dir, d30, FX(100.0f));
}

fx sim_probe_walk(Body *w, const SimCell *cell, const uint16_t *pick, int npick, const fx *world, const fx *dir)
{
    const Plane *pl = g_track->planes;
    fx best = FX(100.0f);
    int32_t d30[3];
    int k, j;
    for (j = 0; j < 3; j++)
        d30[j] = (int32_t)(dir[j] >> 2);
    for (k = 0; k < npick; k++)
        best = probe_one(w, &pl[cell->tris[pick[k]]], world, dir, d30, best);
    return best;
}

static inline __attribute__((always_inline)) fx probe_one(Body *w, const Plane *p, const fx *world, const fx *dir,
                                                            const int32_t *d30, fx best)
{
    int j;
    {
        int32_t n30[3];
        int64_t d64 = 0, t64 = 0, sq = 0, rr;
        fx d, t, h, mount[3];
        for (j = 0; j < 3; j++) {                /* (a hit within 2 of the point lies on the triangle) */
            int32_t e = (int32_t)((p->c[j] - world[j]) >> 16);
            sq += (int64_t)e * e;
        }
        rr = (p->r + FX(2.01f)) >> 16;
        if (sq > rr * rr)
            return best;
        if (p->n[2] <= FXD(0.2))
            return best;
        for (j = 0; j < 3; j++) {
            n30[j] = (int32_t)(p->n[j] >> 2);
            d64 += (int64_t)n30[j] * (int32_t)((world[j] - p->v0[j]) >> 16);
            t64 += (int64_t)n30[j] * d30[j];
        }
        d = d64 >> 14;
        if (!(d > FXD(-2.0) && d < FXD(2.0)))
            return best;
        t = t64 >> 28;
        if ((t < 0 ? -t : t) <= FXD(0.001))
            return best;
        h = FDIV(-d, t);
        if (!(h > FXD(-2.0) && h < FXD(2.0) && h < best))
            return best;
        for (j = 0; j < 3; j++)
            mount[j] = fx_muli(dir[j], h) + world[j];
        if (sim_tri_contains(p, mount)) {
            w->hit = p;
            w->surface = p->surface;
            best = h;
            w->hitN[0] = p->n[0];
            w->hitN[1] = p->n[1];
            w->hitN[2] = p->n[2];
            w->hitN[3] = p->d;
        }
    }
    return best;
}
#endif
