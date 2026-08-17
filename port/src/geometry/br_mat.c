/* br_mat.c -- matrix math decompiled from BRD3D.dll. See br_mat.h.
 *
 * Both routines accumulate directly into the output (the original zeroes
 * out[i] then does `fadd [eax]; fstp [eax]` each step), so out must not alias
 * v. The original has the same constraint.
 */
#include "br_mat.h"

#include <math.h>
#include <stddef.h>

void BrMat4MulVec3(BrVec3 *pOut, const BrMat4 *pM, const BrVec3 *pV)
{
    const float *v = &pV->x;
    float *o = &pOut->x;
    int i, k;

    for (i = 0; i < 3; i++) {
        o[i] = 0.0f;
        for (k = 0; k < 3; k++)
            o[i] += pM->m[i][k] * v[k];
    }
}

void BrMat4MulVec3Transposed(BrVec3 *pOut, const BrMat4 *pM, const BrVec3 *pV)
{
    const float *v = &pV->x;
    float *o = &pOut->x;
    int i, k;

    for (i = 0; i < 3; i++) {
        o[i] = 0.0f;
        for (k = 0; k < 3; k++)
            o[i] += pM->m[k][i] * v[k];
    }
}

/* @implements 0x100349C0 glide BrVec3Project */
/* WHAT IT DOES: projects the point pV through the 4x4 matrix pM using the
 * row-vector convention (v' = v * M, no translation row -- see below) and then
 * divides all three components by the resulting w.  This is the vertex ->
 * clip/screen projection the render frontier uses (caller 0x10017110 runs a
 * vertex array through g_6E78F0 with it).  GLIDE-ONLY: no D3D twin exists.
 *
 * THREE FAITHFULNESS POINTS, all read straight off the x87 sequence:
 *  1. Only the linear 3x3 and the projection column (m[*][3]) participate.  The
 *     translation row m[3][0..2] is NOT added to the x/y/z numerators; only
 *     m[3][3] is added to w.  That is what the original computes -- it assumes a
 *     pure projection matrix -- so it is reproduced verbatim, not "fixed".
 *  2. The summation GROUPING differs between components because MSVC scheduled
 *     the first column around the divide: x groups (m10*vy + m20*vz) + m00*vx,
 *     while y and z group (m0k*vx + m1k*vy) + m2k*vz, and w groups
 *     (m13*vy + m03*vx) + m23*vz + m33.  Float add is not associative, so these
 *     orders are preserved exactly.
 *  3. The divisor g_0775F0 is 1.0f (same constant br_dl.c divides by), and the
 *     original forms it as `fdivr` (1.0 / w), so the reciprocal is taken once
 *     and then multiplied in -- reproduced as `1.0 / w`.
 * Intermediates are held in double, which models the x87 registers at their
 * 53-bit precision (PC=2, CONVENTIONS.md); only the stores to pOut round to
 * float, exactly as the original's `fstp dword`. */
void BrVec3Project(BrVec3 *pOut, const BrVec3 *pV, const BrMat4 *pM)
{
    double vx = pV->x, vy = pV->y, vz = pV->z;
    const float (*m)[4] = pM->m;
    double w = (((double)m[1][3] * vy + (double)m[0][3] * vx)
                 + (double)m[2][3] * vz) + (double)m[3][3];
    double r = 1.0 / w;                 /* g_0775F0 == 1.0f, taken via fdivr */

    pOut->x = (float)((((double)m[1][0] * vy + (double)m[2][0] * vz)
                        + (double)m[0][0] * vx) * r);
    pOut->y = (float)((((double)m[0][1] * vx + (double)m[1][1] * vy)
                        + (double)m[2][1] * vz) * r);
    pOut->z = (float)((((double)m[0][2] * vx + (double)m[1][2] * vy)
                        + (double)m[2][2] * vz) * r);
}

/* Source first -- see the warning in br_mat.h. The original copies 4 rows of
 * 4 dwords using a displacement trick (ecx = src - dst, then [ecx+eax]);
 * that is just how MSVC strength-reduced two pointers into one. */
void BrMat4Copy(const BrMat4 *pSrc, BrMat4 *pDst)
{
    int i, k;
    for (i = 0; i < 4; i++)
        for (k = 0; k < 4; k++)
            pDst->m[i][k] = pSrc->m[i][k];
}

/* 0x100307D0 -- stores 0x3F800000 (1.0f) on the diagonal and zero elsewhere,
 * fully unrolled in the original. */
void BrMat4Identity(BrMat4 *pM)
{
    int i, k;
    for (i = 0; i < 4; i++)
        for (k = 0; k < 4; k++)
            pM->m[i][k] = (i == k) ? 1.0f : 0.0f;
}

/* 0x10030810 -- see br_mat.h. The original computes the three differences
 * once each and divides by them repeatedly (fdiv against stack scratch),
 * which is what the expressions below reproduce. */
/* WHAT IT DOES: builds the camera's projection matrix from the six edges of
 * the viewing box -- what the player can see and how far into the distance.
 * A degenerate box, where two opposite edges are equal, leaves the matrix
 * untouched and reports failure rather than producing nonsense. */
/* @implements 0x10030810 d3d BrMat4Frustum */
int BrMat4Frustum(BrMat4 *pM, float l, float r, float b, float t,
                  float n, float f)
{
    float dx, dy, dz;

    /* The guards are exact equality compares, in this order. A degenerate
     * frustum leaves the matrix untouched and reports failure; the original
     * prints a diagnostic and returns without writing. */
    if (l == r || t == b || n == f)
        return 1;

    dx = r - l;
    dy = t - b;
    dz = f - n;

    pM->m[0][0] = (n + n) / dx;
    pM->m[0][1] = 0.0f;
    pM->m[0][2] = 0.0f;
    pM->m[0][3] = 0.0f;

    pM->m[1][0] = 0.0f;
    pM->m[1][1] = (n + n) / dy;
    pM->m[1][2] = 0.0f;
    pM->m[1][3] = 0.0f;

    pM->m[2][0] = (r + l) / dx;
    pM->m[2][1] = (t + b) / dy;
    pM->m[2][2] = -(f + n) / dz;
    pM->m[2][3] = -1.0f;

    pM->m[3][0] = 0.0f;
    pM->m[3][1] = 0.0f;
    pM->m[3][2] = -((f + f) * n) / dz;
    pM->m[3][3] = 0.0f;
    return 0;
}

/* 0x10030930 -- see br_mat.h. 0x1008F4A8 holds pi/360. */
#define BR_PI_OVER_360 0.0087266462599716477

int BrMat4Perspective(BrMat4 *pM, unsigned short *pPerspNorm,
                      float fovyDegrees, float aspect, float n, float f)
{
    float ty = (float)tan((double)fovyDegrees * BR_PI_OVER_360);
    float h  = n * ty;
    float w  = h * aspect;
    int rc;

    rc = BrMat4Frustum(pM, -w, w, -h, h, n, f);
    if (pPerspNorm != NULL)
        *pPerspNorm = 1;          /* hardcoded in the original */
    return rc;
}

/* 0x100310F0 -- the original moves sy and sz as integers (plain `mov`, since
 * copying a float bit pattern needs no FPU) and only sx goes through
 * fld/fstp. Semantically identical. */
/* WHAT IT DOES: builds a matrix that scales by a different amount along each
 * axis, clearing everything else first. */
/* @implements 0x100310F0 d3d BrMat4Scale */
void BrMat4Scale(BrMat4 *pM, float sx, float sy, float sz)
{
    int i, k;
    for (i = 0; i < 4; i++)
        for (k = 0; k < 4; k++)
            pM->m[i][k] = 0.0f;
    pM->m[0][0] = sx;
    pM->m[1][1] = sy;
    pM->m[2][2] = sz;
    pM->m[3][3] = 1.0f;
}
