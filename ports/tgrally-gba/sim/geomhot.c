/* geomhot.c -- the geometry kernels the race calls most (geometry/mat3.c's rotations and
 * products, BrQuatToMat, libultra's guMtxCatF): on the GBA they are ARM code in IWRAM with
 * the multiply in line, the rest of the simulation calling them from the cartridge. */
#include "sim.h"
#if defined(__arm__) && !defined(FX_FLOAT)
#pragma clang section text=".iwram"
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
