/* geom.c -- the simulation's geometry: geometry/mat3.c, driving/rbquat.c's
 * orientation helpers, libultra's guRotateF, guMtxCatF and guNormalize,
 * geometry/atan2.c BrAtan2; under FX_FLOAT also libultra's sinf and cosf as
 * the cartridge has them (the fixed build's are in fxmath.c). */
#include "sim.h"

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

void BrMat3Transpose(fx t[3][3], fx c[3][3], fx m[4][4])
{
    int i, j;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            c[i][j] = t[j][i] = m[i][j];
}

void BrMat3FromMat4T(fx t[3][3], fx m[4][4])
{
    int i, j;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            t[j][i] = m[i][j];
}

void BrMat3FromMat4(fx out[3][3], fx m[4][4])
{
    int i, j;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            out[i][j] = m[i][j];
}

void BrMat3Skew(fx out[3][3], const fx v[3])
{
    out[2][2] = FX(0.0);
    out[1][1] = FX(0.0);
    out[0][0] = FX(0.0);
    out[0][1] = -v[2];
    out[0][2] = v[1];
    out[1][0] = v[2];
    out[1][2] = -v[0];
    out[2][0] = -v[1];
    out[2][1] = v[0];
}

void BrMat3Mul(fx out[3][3], fx a[3][3], fx b[3][3])
{
    int i, j;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            out[i][j] = FMUL(a[i][0], b[0][j]) + FMUL(a[i][1], b[1][j]) + FMUL(a[i][2], b[2][j]);
}

void BrMat3Sub(fx out[3][3], fx a[3][3], fx b[3][3])
{
    int i, j;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            out[i][j] = a[i][j] - b[i][j];
}

/* m's rotation transposed and scaled per axis, and the translation that undoes m's */
void BrMat4InvertScaled(fx m[4][4], fx out[4][4], const fx s[3])
{
    int i, j;
    fx t[3];
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            out[i][j] = m[j][i];
            out[i][j] = FMUL(out[i][j], s[j]);
        }
        out[i][3] = FX(0.0);
    }
    out[3][3] = FX(1.0);
    t[0] = -m[3][0];
    t[1] = -m[3][1];
    t[2] = -m[3][2];
    BrMat4RotateVecT(out[3], out, t);
}

void BrMat3Solve(fx out[3], fx mm[3][3], const fx v[3])
{
    const fx *m = &mm[0][0];
    fx p0, p1, p2, p3, p4, p5, d;

    p0 = FMUL(m[4], m[8]);
    p1 = FMUL(m[5], m[7]);
    p2 = FMUL(m[1], m[8]);
    p3 = FMUL(m[2], m[7]);
    p4 = FMUL(m[1], m[5]);
    p5 = FMUL(m[2], m[4]);
    d = FMUL(-m[0], p0) + FMUL(m[0], p1) + FMUL(m[3], p2) - FMUL(m[3], p3) - FMUL(m[6], p4) + FMUL(m[6], p5);
    d = FDIV(FX(1.0), d);
    out[0] = FMUL(FMUL(-v[0], p0) + FMUL(v[0], p1) + FMUL(v[1], p2) - FMUL(v[1], p3) - FMUL(v[2], p4) + FMUL(v[2], p5), d);
    p0 = FMUL(m[3], m[8]);
    p1 = FMUL(m[5], m[6]);
    p2 = FMUL(m[0], m[8]);
    p3 = FMUL(m[2], m[6]);
    p4 = FMUL(m[0], m[5]);
    p5 = FMUL(m[2], m[3]);
    out[1] = FMUL(-(FMUL(-v[0], p0) + FMUL(v[0], p1) + FMUL(v[1], p2) - FMUL(v[1], p3) - FMUL(v[2], p4) + FMUL(v[2], p5)), d);
    p0 = FMUL(m[3], m[7]);
    p1 = FMUL(m[4], m[6]);
    p2 = FMUL(m[0], m[7]);
    p3 = FMUL(m[1], m[6]);
    p4 = FMUL(m[0], m[4]);
    p5 = FMUL(m[1], m[3]);
    out[2] = FMUL(FMUL(-v[0], p0) + FMUL(v[0], p1) + FMUL(v[1], p2) - FMUL(v[1], p3) - FMUL(v[2], p4) + FMUL(v[2], p5), d);
}

/* ---- driving/rbquat.c ---- */

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

void BrVec4Normalise(fx v[4])
{
    fx k;
    k = FSQRT(FMUL(v[0], v[0]) + FMUL(v[1], v[1]) + FMUL(v[2], v[2]) + FMUL(v[3], v[3]));
    k = FDIV(FX(1.0), k);
    v[0] = FMUL(v[0], k);
    v[1] = FMUL(v[1], k);
    v[2] = FMUL(v[2], k);
    v[3] = FMUL(v[3], k);
}

void BrVec3NormaliseF(fx v[3])
{
    fx k;
    k = FSQRT(FMUL(v[0], v[0]) + FMUL(v[1], v[1]) + FMUL(v[2], v[2]));
    k = FDIV(FX(1.0), k);
    v[0] = FMUL(v[0], k);
    v[1] = FMUL(v[1], k);
    v[2] = FMUL(v[2], k);
}

/* the orientation rate: half the spin times the orientation */
void BrRbQuatDerivative(RbState *b)
{
    fx h[3];
    h[0] = FMUL(b->omega[0], FX(0.5));
    h[1] = FMUL(b->omega[1], FX(0.5));
    h[2] = FMUL(b->omega[2], FX(0.5));
    b->qdot[0] = FMUL(-h[0], b->q[1]) - FMUL(h[1], b->q[2]) - FMUL(h[2], b->q[3]);
    b->qdot[1] = FMUL(b->q[0], h[0]) + FMUL(h[1], b->q[3]) - FMUL(h[2], b->q[2]);
    b->qdot[2] = FMUL(b->q[0], h[1]) + FMUL(h[2], b->q[1]) - FMUL(h[0], b->q[3]);
    b->qdot[3] = FMUL(b->q[0], h[2]) + FMUL(h[0], b->q[2]) - FMUL(h[1], b->q[1]);
}

/* one Euler step of a state: position by velocity, orientation by its rate (renormalised) */
void BrRbStateStep(RbState *out, const RbState *in, fx dt)
{
    fx dp[3], dq[4];
    dp[0] = FMUL(in->vel[0], dt);
    dp[1] = FMUL(in->vel[1], dt);
    dp[2] = FMUL(in->vel[2], dt);
    out->pos[0] = in->pos[0] + dp[0];
    out->pos[1] = in->pos[1] + dp[1];
    out->pos[2] = in->pos[2] + dp[2];
    out->vel[0] = in->vel[0];
    out->vel[1] = in->vel[1];
    out->vel[2] = in->vel[2];
    dq[0] = FMUL(in->qdot[0], dt);
    dq[1] = FMUL(in->qdot[1], dt);
    dq[2] = FMUL(in->qdot[2], dt);
    dq[3] = FMUL(in->qdot[3], dt);
    out->q[0] = in->q[0] + dq[0];
    out->q[1] = in->q[1] + dq[1];
    out->q[2] = in->q[2] + dq[2];
    out->q[3] = in->q[3] + dq[3];
    BrVec4Normalise(out->q);
    out->omega[0] = in->omega[0];
    out->omega[1] = in->omega[1];
    out->omega[2] = in->omega[2];
    out->qdot[0] = in->qdot[0];
    out->qdot[1] = in->qdot[1];
    out->qdot[2] = in->qdot[2];
    out->qdot[3] = in->qdot[3];
}

/* ---- libultra gu ---- */

static void guNormalize(fx *x, fx *y, fx *z)
{
    fx m = FDIV(FX(1.0), FSQRT(FMUL(*x, *x) + FMUL(*y, *y) + FMUL(*z, *z)));
    *x = FMUL(*x, m);
    *y = FMUL(*y, m);
    *z = FMUL(*z, m);
}

/* a rotation of a degrees about (x, y, z) (row vectors) */
void guRotateF(fx mf[4][4], fx a, fx x, fx y, fx z)
{
    static const fx dtor = FX(3.1415926 / 180.0);
    fx sine, cosine, ab, bc, ca, t;
    int i, j;

    guNormalize(&x, &y, &z);
    a = FMUL(a, dtor);
    sine = FSIN(a);
    cosine = FCOS(a);
    t = FX(1.0) - cosine;
    ab = FMUL(FMUL(x, y), t);
    bc = FMUL(FMUL(y, z), t);
    ca = FMUL(FMUL(z, x), t);
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            mf[i][j] = i == j ? FX(1.0) : FX(0.0);
    t = FMUL(x, x);
    mf[0][0] = t + FMUL(cosine, FX(1.0) - t);
    mf[2][1] = bc - FMUL(x, sine);
    mf[1][2] = bc + FMUL(x, sine);
    t = FMUL(y, y);
    mf[1][1] = t + FMUL(cosine, FX(1.0) - t);
    mf[2][0] = ca + FMUL(y, sine);
    mf[0][2] = ca - FMUL(y, sine);
    t = FMUL(z, z);
    mf[2][2] = t + FMUL(cosine, FX(1.0) - t);
    mf[1][0] = ab - FMUL(z, sine);
    mf[0][1] = ab + FMUL(z, sine);
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

/* ---- geometry/atan2.c: the angle of (x, y), 0 to 2 pi, by bisecting sin ---- */
fx BrAtan2(fx x, fx y)
{
    fx ang, t, r, a, step, s;
    int noSwap, i;

    ang = FX(0.0);
    noSwap = 1;
    if (y < FX(0.0)) {
        x = -x;
        y = -y;
        ang = ang + FX(3.1415927);
    }
    if (x < FX(0.0)) {
        t = x;
        x = y;
        y = -t;
        ang = ang + FX(1.5707964);
    }
    if (x < y) {
        t = x;
        x = y;
        y = t;
        noSwap = 0;
        ang = ang + FX(0.7853982);
    }
    r = FSQRT(FMUL(y, y) + FMUL(x, x));
    if (r == FX(0.0))
        return FX(0.0);
    y = FDIV(y, r);
    a = FX(0.3926991);
    step = FMUL(a, FX(0.5));
    for (i = 0; i < 16; i++) {
        s = FSIN(a);
        if (s < y) {
            if (y - s < FX(0.005))
                break;
            a = a + step;
        } else {
            if (!(y < s))
                break;
            if (s - y < FX(0.005))
                break;
            a = a - step;
        }
        step = FMUL(step, FX(0.5));
    }
    if (noSwap)
        ang = ang + a;
    else
        ang += FX(0.7853982) - a;
    return ang;
}

#ifdef FX_FLOAT
/* ---- libultra's sinf and cosf, as the cartridge computes them ---- */
typedef union { unsigned int i[2]; double d; } du;
#define ROUND(d) (int)(((d) >= 0.0) ? ((d) + 0.5) : ((d) - 0.5))
static double D(uint32_t hi, uint32_t lo)
{
    union { uint64_t u; double d; } x;
    x.u = (uint64_t)hi << 32 | lo;
    return x.d;
}
#define P1 D(0xbfc55554, 0xbc83656d)
#define P2 D(0x3f8110ed, 0x3804c2a0)
#define P3 D(0xbf29f6ff, 0xeea56814)
#define P4 D(0x3ec5dbdf, 0x0e314bfe)
#define RPI D(0x3fd45f30, 0x6dc9c883)
#define PIHI D(0x400921fb, 0x50000000)
#define PILO D(0x3e6110b4, 0x611a6263)

float fx_sinf(float x)
{
    double dx, xsq, poly, dn, result;
    int n, ix, xpt;
    union { float f; int i; } u;
    u.f = x;
    ix = u.i;
    xpt = (ix >> 22) & 0x1ff;
    if (xpt < 0xff) {
        dx = x;
        if (xpt >= 0xe6) {
            xsq = dx * dx;
            poly = ((P4 * xsq + P3) * xsq + P2) * xsq + P1;
            result = dx + (dx * xsq) * poly;
            return (float)result;
        }
        return x;
    }
    if (xpt < 0x136) {
        dx = x;
        dn = dx * RPI;
        n = ROUND(dn);
        dn = n;
        dx = dx - dn * PIHI;
        dx = dx - dn * PILO;
        xsq = dx * dx;
        poly = ((P4 * xsq + P3) * xsq + P2) * xsq + P1;
        result = dx + (dx * xsq) * poly;
        if ((n & 1) == 0)
            return (float)result;
        return -(float)result;
    }
    return 0.0f;
}

float fx_cosf(float x)
{
    float absx;
    double dx, xsq, poly, dn, result;
    int n, ix, xpt;
    union { float f; int i; } u;
    u.f = x;
    ix = u.i;
    xpt = (ix >> 22) & 0x1ff;
    if (xpt < 0x136) {
        absx = x > 0 ? x : -x;
        dx = absx;
        dn = dx * RPI + 0.5;
        n = ROUND(dn);
        dn = n;
        dn -= 0.5;
        dx = dx - dn * PIHI;
        dx = dx - dn * PILO;
        xsq = dx * dx;
        poly = ((P4 * xsq + P3) * xsq + P2) * xsq + P1;
        result = dx + (dx * xsq) * poly;
        if ((n & 1) == 0)
            return (float)result;
        return -(float)result;
    }
    return 0.0f;
}
#endif
