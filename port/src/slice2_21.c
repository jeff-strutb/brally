/* slice2_21.c -- 0x10039200 .. 0x1003BC90, decompiled from BRD3D.dll.
 *
 * See slice2_21.h for the module map, the field offsets and the gotchas.
 *
 * Every x87 sequence below was traced instruction by instruction through its
 * fxch/fsubr/fdivr chain; the reversed operand forms (fsubr, fdivr, fsubp
 * st(n)) are spelled out in the arithmetic here rather than "tidied", because
 * `fsubr m32` is `st0 = m32 - st0` and getting that backwards is silent.
 */
#include "slice2_21.h"

#include <string.h>

/* --------------------------------------------------------------------------
 * Constants, all read straight out of .rdata rather than guessed.
 * -------------------------------------------------------------------------- */
#define K_0            0.0f                    /* 0x1008F62C, 0x1008F59C */
#define K_1            1.0f                    /* 0x1008F628, 0x1008F588 */
#define K_EPS_REL      1.0000000036274937e-15f /* 0x1008F63C */
#define K_PI           3.1415927410125732f     /* 0x1008F640 */
#define K_PI_2         1.5707963705062866f     /* -0x1008F644 */
#define K_PI_4         0.7853981852531433f     /* -0x1008F648, +0x1008F658 */
#define K_PI_8         0.39269909262657166f    /* 0x1008F64C */
#define K_PI_16        0.19634954631328583f    /* 0x1008F650 */
#define K_SIN_TOL      0.004999999888241291f   /* 0x1008F654 */
#define K_CELL_RECIP   0.03125f                /* 0x1008F61C, 0x1008F608 */
#define K_CELL         32.0f                   /* 0x1008F624 */
#define K_65280_RECIP  1.5318628356908448e-05f /* 0x1008F5FC = 1/65280 */
#define K_65536_RECIP  1.5259021893143654e-05f /* 0x1008F57C = 1/65536 */

/* --------------------------------------------------------------------------
 * 1. Float geometry leaves
 * -------------------------------------------------------------------------- */

/* 0x1003ADA0 */
/* WHAT IT DOES: works out which way one point lies from another as a unit
 * direction. Two points in exactly the same place have no direction, so it
 * answers "straight up" rather than dividing by zero. */
/* @implements 0x1003ADA0 d3d BrVec3Direction */
void BrVec3Direction(BrVec3 *pOut, const BrVec3 *pFrom, const BrVec3 *pTo)
{
    float dx = pTo->x - pFrom->x;
    float dy = pTo->y - pFrom->y;
    float dz = pTo->z - pFrom->z;
    float len = BrSqrtF(dx * dx + dy * dy + dz * dz);

    if (len == K_0) {
        pOut->x = 0.0f;
        pOut->y = 0.0f;
        pOut->z = 1.0f;
        return;
    }
    len = K_1 / len;                /* fdivr: 1.0 / len, computed once */
    pOut->x = len * dx;
    pOut->y = len * dy;
    pOut->z = len * dz;
}

/* 0x1003AE50 */
void BrVec3NormaliseGuard(BrVec3 *pV)
{
    /* The original spills y and z to the stack and squares them from there,
     * so the sum order is y*y + z*z + x*x -- reproduced. */
    float len = BrSqrtF(pV->y * pV->y + pV->z * pV->z + pV->x * pV->x);

    if (len == K_0) {
        pV->x = 0.0f;
        pV->y = 0.0f;
        pV->z = 1.0f;
        return;
    }
    len = K_1 / len;
    pV->x = len * pV->x;
    pV->y = len * pV->y;
    pV->z = len * pV->z;
}

/* 0x1003B1C0 */
/* WHAT IT DOES: measures how long a vector is looking down from above, that
 * is, ignoring height. */
/* @implements 0x1003B1C0 d3d BrVec3LenXY */
float BrVec3LenXY(const BrVec3 *pV)
{
    return BrSqrtF(pV->y * pV->y + pV->x * pV->x);
}

/* 0x1003B0A0 */
/* WHAT IT DOES: measures the distance between two points as seen from above,
 * ignoring any difference in height -- which is what "how far apart are these
 * two cars on the track" means. */
/* @implements 0x1003B0A0 d3d BrVec3DistXY */
float BrVec3DistXY(const BrVec3 *pA, const BrVec3 *pB)
{
    float dx = pA->x - pB->x;
    float dy = pA->y - pB->y;
    return BrSqrtF(dx * dx + dy * dy);
}

/* 0x1003B7B0 */
/* WHAT IT DOES: works out the compass angle of a direction. Rather than a
 * lookup table it folds the direction into one eighth of a circle and then
 * hunts for the answer by halving the interval sixteen times, stopping early
 * once it is close enough -- about a quarter of a degree. Note its two
 * arguments are in the opposite order to the C library's atan2. */
/* @implements 0x1003B7B0 d3d BrAtan2 */
float BrAtan2(float x, float y)
{
    float acc = 0.0f;   /* the reduction offset, [esp+8] */
    float r;
    float ang, step;
    int   i;
    int   bDirect = 1;  /* edi: cleared by the x<y swap only */

    if (y < K_0) {
        x = -x;
        y = -y;
        acc = K_PI;
    }
    if (x < K_0) {
        /* one rotation (x, y) -> (y, -x); the original tests once, not in a
         * loop, and y >= 0 by now so one rotation always suffices */
        float t = y;
        y = -x;
        x = t;
        acc = acc + K_PI_2;
    }
    if (x < y) {
        float t = x;
        x = y;
        y = t;
        acc += K_PI_4;
        bDirect = 0;
    }

    r = BrSqrtF(y * y + x * x);
    if (r == K_0)
        return K_0;

    y = y / r;                      /* sin of the reduced angle */
    ang  = K_PI_8;
    step = K_PI_16;

    for (i = 0; i < 16; i++) {
        float s = BrSinF(ang);
        if (s < y) {
            if (y - s < K_SIN_TOL)
                break;
            ang = step + ang;
        } else if (s > y) {
            if (s - y < K_SIN_TOL)
                break;
            ang = ang - step;
        } else {
            break;                  /* exact hit: the original drops out too */
        }
        step = step * 0.5f;
    }

    if (bDirect)
        return acc + ang;
    /* fsubr against the saved offset: acc - (ang - pi/4). */
    return acc - (ang - K_PI_4);
}

/* --------------------------------------------------------------------------
 * 2. 4x4 matrices
 * -------------------------------------------------------------------------- */

/* 0x1003B2A0 -- signature deliberately matches slice2_18.h's XSLICE
 * declaration (a bare `const float *` matrix) so the two link. */
/* WHAT IT DOES: puts a point through a transform -- moving, rotating and
 * scaling it in one step -- and keeps the fourth component, which is what the
 * perspective divide later needs. */
/* @implements 0x1003B2A0 d3d BrMat4TransformPoint4 */
void BrMat4TransformPoint4(float pOut[4], const BrVec3 *pV, const float *pM)
{
    int j;
    for (j = 0; j < 4; j++) {
        pOut[j] = pM[0 * 4 + j] * pV->x + pM[1 * 4 + j] * pV->y
                + pM[2 * 4 + j] * pV->z + pM[3 * 4 + j];
    }
}

/* 0x1003B3F0 */
/* WHAT IT DOES: puts a direction through a transform. Unlike a point, a
 * direction is only rotated and scaled and never moved, so the transform's
 * position part is deliberately left out. */
/* @implements 0x1003B3F0 d3d BrMtxXfmDir3 */
void BrMtxXfmDir3(BrVec3 *pOut, const BrVec3 *pV, const BrMat4 *pM)
{
    float a[3];
    int j;
    for (j = 0; j < 3; j++)
        a[j] = pM->m[0][j] * pV->x + pM->m[1][j] * pV->y + pM->m[2][j] * pV->z;
    pOut->x = a[0];
    pOut->y = a[1];
    pOut->z = a[2];
}

/* 0x1003B470 */
/* WHAT IT DOES: combines two transforms into one. It builds the answer in a
 * scratch copy first, so it is safe to write the result back over either of the
 * things being multiplied. */
/* @implements 0x1003B470 d3d BrMtxMul */
void BrMtxMul(BrMat4 *pOut, const BrMat4 *pA, const BrMat4 *pB)
{
    BrMat4 t;   /* the original's 64-byte stack temp: aliasing is safe */
    int i, j, k;

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            float acc = 0.0f;
            for (k = 0; k < 4; k++)
                acc = acc + pA->m[i][k] * pB->m[k][j];
            t.m[i][j] = acc;
        }
    }
    *pOut = t;
}

/* 0x1003B4F0 */
/* WHAT IT DOES: works out the transform that undoes a given one -- how to get
 * from world space back into an object's own space, for instance. A transform
 * that cannot be undone, because it squashes everything flat, yields the
 * identity instead. It reports success either way, so the caller cannot tell
 * the two apart. */
/* @implements 0x1003B4F0 d3d BrMtxInvert */
int BrMtxInvert(BrMat4 *pOut, const BrMat4 *pM)
{
    const float (*m)[4] = pM->m;
    float pos = 0.0f, neg = 0.0f;   /* the original's two accumulators */
    float aTerm[6];
    float det, spread, ratio, id;
    int i;

    aTerm[0] =  m[0][0] * (m[1][1] * m[2][2]);
    aTerm[1] =  (m[1][2] * m[2][0]) * m[0][1];
    aTerm[2] =  (m[1][0] * m[0][2]) * m[2][1];
    aTerm[3] = -((m[0][2] * m[2][0]) * m[1][1]);
    aTerm[4] = -((m[1][0] * m[0][1]) * m[2][2]);
    aTerm[5] = -(m[0][0] * (m[2][1] * m[1][2]));

    for (i = 0; i < 6; i++) {
        if (aTerm[i] < K_0)
            neg = neg + aTerm[i];
        else
            pos = pos + aTerm[i];
    }

    det    = neg + pos;
    spread = pos - neg;
    ratio  = det / spread;
    if (ratio < K_0)
        ratio = -ratio;

    if (det == K_0 || ratio < K_EPS_REL) {
        int r, c;
        for (r = 0; r < 4; r++)
            for (c = 0; c < 4; c++)
                pOut->m[r][c] = (r == c) ? 1.0f : 0.0f;
        return 1;   /* see the header: the success path returns 1 as well */
    }

    id = K_1 / det;

    pOut->m[0][0] =  (m[1][1] * m[2][2] - m[2][1] * m[1][2]) * id;
    pOut->m[1][0] = -(m[1][0] * m[2][2] - m[1][2] * m[2][0]) * id;
    pOut->m[2][0] =  (m[1][0] * m[2][1] - m[2][0] * m[1][1]) * id;
    pOut->m[0][1] = -(m[0][1] * m[2][2] - m[0][2] * m[2][1]) * id;
    pOut->m[1][1] =  (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * id;
    pOut->m[2][1] = -(m[0][0] * m[2][1] - m[2][0] * m[0][1]) * id;
    pOut->m[0][2] =  (m[1][2] * m[0][1] - m[0][2] * m[1][1]) * id;
    pOut->m[1][2] = -(m[0][0] * m[1][2] - m[1][0] * m[0][2]) * id;
    pOut->m[2][2] =  (m[0][0] * m[1][1] - m[1][0] * m[0][1]) * id;

    pOut->m[3][0] = -(pOut->m[0][0] * m[3][0] + pOut->m[1][0] * m[3][1]
                    + pOut->m[2][0] * m[3][2]);
    pOut->m[3][1] = -(pOut->m[0][1] * m[3][0] + pOut->m[1][1] * m[3][1]
                    + pOut->m[2][1] * m[3][2]);
    pOut->m[3][2] = -(m[3][0] * pOut->m[0][2] + m[3][1] * pOut->m[1][2]
                    + m[3][2] * pOut->m[2][2]);

    pOut->m[0][3] = 0.0f;
    pOut->m[1][3] = 0.0f;
    pOut->m[2][3] = 0.0f;
    pOut->m[3][3] = 1.0f;
    return 1;
}

/* --------------------------------------------------------------------------
 * 3. 2D segment predicates
 * -------------------------------------------------------------------------- */

/* The straddle core shared by 0x1003BC90 and the two halves of 0x1003BA70.
 * Returns 1 when c and d are strictly on the same side of a->b (a rejection),
 * and hands back the two cross products. */
static int BrSeg2Cross(const BrVec2 *pA, const BrVec2 *pB,
                       const BrVec2 *pC, const BrVec2 *pD,
                       float *pD1, float *pD2)
{
    float dx = pB->x - pA->x;
    float dy = pB->y - pA->y;
    float d1 = (pC->x - pA->x) * dy - (pC->y - pA->y) * dx;
    float d2 = (pD->x - pA->x) * dy - (pD->y - pA->y) * dx;

    *pD1 = d1;
    *pD2 = d2;
    if (d1 != K_0 && d2 != K_0 && d1 * d2 > K_0)
        return 1;
    return 0;
}

/* 0x1003BC90 */
/* WHAT IT DOES: asks which side of a line two points fall on: nothing if they
 * are both on the same side, and otherwise whether they straddle it normally or
 * lie exactly balanced across it. */
/* @implements 0x1003BC90 d3d BrSeg2SideTest */
int BrSeg2SideTest(const BrVec2 *pA, const BrVec2 *pB,
                   const BrVec2 *pC, const BrVec2 *pD)
{
    float d1, d2;
    if (BrSeg2Cross(pA, pB, pC, pD, &d1, &d2))
        return 0;
    return (d1 - d2 == K_0) ? 2 : 1;
}

/* 0x1003BA70 */
/* WHAT IT DOES: asks whether two line segments cross. It first rules out the
 * easy cases where their bounding boxes do not even overlap, then checks that
 * each segment really does straddle the other. It distinguishes an ordinary
 * crossing from a balanced one, but only the first of the two straddle tests
 * gets a say in which. */
/* @implements 0x1003BA70 d3d BrSeg2Intersect */
int BrSeg2Intersect(const BrVec2 *pA, const BrVec2 *pB,
                    const BrVec2 *pC, const BrVec2 *pD)
{
    float d1, d2, dummy1, dummy2;

    /* Four separating-axis rejections on the two bounding boxes. The original
     * spells all four out with min/max selected by fcomp; the asymmetry in
     * which comparison is <= and which is < is preserved by using the same
     * strict "max < min" rejection each time. */
    {
        float maxAB = (pA->x > pB->x) ? pA->x : pB->x;
        float minCD = (pC->x < pD->x) ? pC->x : pD->x;
        if (maxAB < minCD)
            return 0;
    }
    {
        float maxCD = (pC->x > pD->x) ? pC->x : pD->x;
        float minAB = (pA->x < pB->x) ? pA->x : pB->x;
        if (maxCD < minAB)
            return 0;
    }
    {
        float maxAB = (pA->y > pB->y) ? pA->y : pB->y;
        float minCD = (pC->y < pD->y) ? pC->y : pD->y;
        if (maxAB < minCD)
            return 0;
    }
    {
        float maxCD = (pC->y > pD->y) ? pC->y : pD->y;
        float minAB = (pA->y < pB->y) ? pA->y : pB->y;
        if (maxCD < minAB)
            return 0;
    }

    if (BrSeg2Cross(pA, pB, pC, pD, &d1, &d2))
        return 0;
    if (BrSeg2Cross(pC, pD, pA, pB, &dummy1, &dummy2))
        return 0;

    /* Only the FIRST test's cross products decide 1 vs 2 -- see the header. */
    return (d1 - d2 == K_0) ? 2 : 1;
}

/* --------------------------------------------------------------------------
 * 4. Coverage grid
 * -------------------------------------------------------------------------- */

/* 0x1003A6B0 */
/* WHAT IT DOES: marks a straight line's footprint onto a coarse grid of
 * 32-unit cells: both its endpoints, and then every cell the line passes
 * through as it climbs from one row to the next, including the cells either
 * side when it runs close to a boundary. This is how a shape's outline becomes
 * a set of covered cells. */
/* @implements 0x1003A6B0 d3d BrSpanAddLine */
void BrSpanAddLine(BrSpanVolume *pVol, float x0, float y0, float x1, float y1)
{
    int nx0, nx1, lo, hi, row, rowEnd;
    float dy;

    BrSpanAdd(&pVol->grid, BrFtolArg(x0 * K_CELL_RECIP),
                           BrFtolArg(y0 * K_CELL_RECIP));
    BrSpanAdd(&pVol->grid, BrFtolArg(x1 * K_CELL_RECIP),
                           BrFtolArg(y1 * K_CELL_RECIP));

    nx0 = BrFtolArg(x0 * K_CELL_RECIP);
    nx1 = BrFtolArg(x1 * K_CELL_RECIP);
    lo = nx0;
    hi = nx1;
    if (lo > hi) {
        int t = lo;
        lo = hi;
        hi = t;
    }
    if (lo < pVol->colLo)
        pVol->colLo = lo;
    if (hi > pVol->colHi)
        pVol->colHi = hi;

    if (y0 > y1) {                  /* orient the scan upward */
        float t;
        t = x0; x0 = x1; x1 = t;
        t = y0; y0 = y1; y1 = t;
    }

    dy = y1 - y0;
    if (dy == K_0)
        return;

    row    = BrFtolArg(y0 * K_CELL_RECIP);
    rowEnd = BrFtolArg(y1 * K_CELL_RECIP);
    if (row < pVol->grid.rowLo)
        pVol->grid.rowLo = row;
    if (rowEnd > pVol->grid.rowHi)
        pVol->grid.rowHi = rowEnd;
    if (row > rowEnd)
        return;

    for (; row <= rowEnd; row++) {
        float y = (float)row * K_CELL;
        float x;
        int col;

        if (y < y0)
            continue;
        if (y > y1)
            continue;

        x = ((x1 - x0) * (y - y0)) / dy + x0;
        col = BrFtolArg(x * K_CELL_RECIP);

        BrSpanAdd(&pVol->grid, col, row - 1);
        BrSpanAdd(&pVol->grid, col, row);

        if (x <= (float)col * K_CELL) {
            BrSpanAdd(&pVol->grid, col - 1, row - 1);
            BrSpanAdd(&pVol->grid, col - 1, row);
        }
        if (x >= (float)(col + 1) * K_CELL) {
            BrSpanAdd(&pVol->grid, col + 1, row - 1);
            BrSpanAdd(&pVol->grid, col + 1, row);
        }
    }
}

/* 0x1003A950 */
/* WHAT IT DOES: asks whether a point falls inside the covered area, by
 * dropping it into the coarse grid and checking that cell. */
/* @implements 0x1003A950 d3d BrSpanTestPoint */
int BrSpanTestPoint(const BrSpanVolume *pVol, float x, float y)
{
    return BrSpanTest(&pVol->grid, BrFtolArg(x * K_CELL_RECIP),
                                   BrFtolArg(y * K_CELL_RECIP));
}

/* 0x1003A990 */
/* WHAT IT DOES: works out the coarse footprint of an eight-sided shape -- a
 * top point, a bottom point and a four-corner ring between them -- by wiping the
 * grid, drawing all twelve of its edges onto it, and then reducing each column
 * to the first and last row that the shape reaches. The result is a cheap
 * stand-in for the shape that later tests can be run against. */
/* @implements 0x1003A990 d3d BrSpanBuildHull */
void BrSpanBuildHull(BrSpanVolume *pVol, const BrVec3 aPt[6])
{
    static const unsigned char aEdge[12][2] = {
        {0,1},{0,2},{0,3},{0,4},{5,1},{5,2},{5,3},{5,4},{1,2},{2,3},{3,4},{4,1}
    };
    int i, col;

    for (i = 0; i < BR_SPAN_ROWS; i++) {
        pVol->aRowHi[i]     = 0;
        pVol->aRowLo[i]     = BR_SPAN_ROWS;
        pVol->grid.aMax[i]  = 0;
        pVol->grid.aMin[i]  = BR_SPAN_ROWS;
    }
    pVol->colHi        = 0;
    pVol->grid.rowHi   = 0;
    pVol->colLo        = BR_SPAN_ROWS - 1;
    pVol->grid.rowLo   = BR_SPAN_ROWS - 1;

    for (i = 0; i < 12; i++) {
        const BrVec3 *a = &aPt[aEdge[i][0]];
        const BrVec3 *b = &aPt[aEdge[i][1]];
        BrSpanAddLine(pVol, a->x, a->y, b->x, b->y);
    }

    if (pVol->colLo < 0)
        pVol->colLo = 0;
    if (pVol->grid.rowLo < 0)
        pVol->grid.rowLo = 0;
    if (pVol->colHi >= BR_SPAN_ROWS)
        pVol->colHi = BR_SPAN_ROWS - 1;
    if (pVol->grid.rowHi >= BR_SPAN_ROWS)
        pVol->grid.rowHi = BR_SPAN_ROWS - 1;

    for (col = pVol->colLo; col <= pVol->colHi; col++) {
        int lo = pVol->grid.rowLo;
        int hi = pVol->grid.rowHi;

        /* DEVIATION: the original scans with no upper/lower bound at all
         * (`inc edx; jmp` / `dec ecx; jmp`), so a column that no row covers
         * walks off both ends of the 64-entry arrays. Bounded here. When a
         * covering row exists -- the case the original was written for -- the
         * results are identical. */
        while (lo < BR_SPAN_ROWS &&
               (col < pVol->grid.aMin[lo] || col > pVol->grid.aMax[lo]))
            lo++;
        while (hi >= 0 &&
               (col < pVol->grid.aMin[hi] || col > pVol->grid.aMax[hi]))
            hi--;

        pVol->aRowLo[col] = lo;
        pVol->aRowHi[col] = hi;
    }
}

/* --------------------------------------------------------------------------
 * 5. Particle pool
 * -------------------------------------------------------------------------- */

/* 0x1003A4D0, free-list half. */
/* WHAT IT DOES: empties the particle pool -- puts every record back on the
 * free list and clears the three lists of particles in flight, so all the dust
 * and spray currently in the air vanishes. */
/* @implements 0x1003A4D0 d3d BrPfxReset */
void BrPfxReset(BrPfxPool *pPool)
{
    int i;
    for (i = 1; i <= BR_PFX_RECS - 1; i++)
        pPool->aRec[i].iNext = (uint16_t)(i + 1);
    pPool->aRec[BR_PFX_RECS - 1].iNext = 0;   /* written after the loop */
    pPool->iFree    = 1;
    pPool->iListB0  = 0;
    pPool->iListAC  = 0;
    pPool->iListB4  = 0;
}

/* 0x1003A610 */
/* WHAT IT DOES: takes a complete copy of the particle pool -- every record and
 * all four list heads -- so it can be examined or restored later. */
/* @implements 0x1003A610 d3d BrPfxSaveState */
void BrPfxSaveState(const BrPfxPool *pPool, BrPfxSnapshot *pOut)
{
    pOut->iFree   = pPool->iFree;
    pOut->iListB0 = pPool->iListB0;
    pOut->iListAC = pPool->iListAC;
    pOut->iListB4 = pPool->iListB4;
    memcpy(pOut->aRec, pPool->aRec, sizeof(pOut->aRec));
}

/* Unlink *piLink's record and push it onto the free list. */
static void BrPfxFree(BrPfxPool *pPool, uint16_t *piLink, uint16_t iRec)
{
    *piLink = pPool->aRec[iRec].iNext;
    pPool->aRec[iRec].iNext = pPool->iFree;
    pPool->iFree = iRec;
}

/* 0x1003A200 */
/* WHAT IT DOES: moves one family of particles on by a frame: each drifts along
 * its own velocity, is carried by the ambient drift, rises slightly, and fades
 * as it ages. Once a particle has faded past a threshold it is returned to the
 * pool and disappears. These ones do not fall -- they have no gravity. */
/* @implements 0x1003A200 d3d BrPfxUpdateB0 */
void BrPfxUpdateB0(BrPfxPool *pPool, const BrPfxEnv *pEnv)
{
    float k = pEnv->dt * 0.3f;      /* 0x1008F5C4 */
    uint16_t *piLink = &pPool->iListB0;
    unsigned iRec = pPool->iListB0;

    while (iRec != 0) {
        BrPfxRec *p = &pPool->aRec[iRec];
        unsigned iNext = p->iNext;
        float scale;

        p->age = k + p->age;
        scale = (float)((int)p->f1F * (int)p->f1E) * K_65280_RECIP;

        p->pos.x = (p->vel.x * scale) * pEnv->dt + pEnv->drift.x + p->pos.x;
        p->pos.y = (p->vel.y * scale) * pEnv->dt + pEnv->drift.y + p->pos.y;
        p->pos.z = (p->vel.z * scale - -0.8f) * pEnv->dt + pEnv->drift.z
                 + p->pos.z;

        p->f1E = (uint8_t)BrFtolTrunc(5.7375006675720215f / (p->age * p->age));

        if (scale < K_CELL_RECIP)   /* 0.03125 */
            BrPfxFree(pPool, piLink, (uint16_t)iRec);
        else
            piLink = &p->iNext;

        iRec = iNext;
    }
}

/* 0x1003A340 */
/* WHAT IT DOES: moves the other two families of particles on by a frame. Like
 * their sibling above they drift and fade, but these also fall -- twice normal
 * gravity is taken off their vertical speed each frame -- and a particle is
 * dropped either when it fades out or when it is falling fast enough to have
 * clearly gone. */
/* @implements 0x1003A340 d3d BrPfxUpdateB4AC */
void BrPfxUpdateB4AC(BrPfxPool *pPool, const BrPfxEnv *pEnv)
{
    float k = pEnv->dt * 0.699999988079071f;   /* 0x1008F60C */
    int pass;

    for (pass = 0; pass < 2; pass++) {
        /* pass 0 walks 0x10A99BB4, pass 1 walks 0x10A99BAC. */
        uint16_t *piLink = (pass == 0) ? &pPool->iListB4 : &pPool->iListAC;
        unsigned iRec = *piLink;

        while (iRec != 0) {
            BrPfxRec *p = &pPool->aRec[iRec];
            unsigned iNext = p->iNext;
            float scale;

            p->age = k + p->age;
            scale = (float)((int)p->f1F * (int)p->f1E) * K_65280_RECIP;

            p->pos.x = (p->vel.x * scale) * pEnv->dt + pEnv->drift.x + p->pos.x;
            p->pos.y = (p->vel.y * scale) * pEnv->dt + pEnv->drift.y + p->pos.y;
            p->pos.z = (scale * p->vel.z - -0.8f) * pEnv->dt + pEnv->drift.z
                     + p->pos.z;

            /* fsubr: vel.z - dt*19.62, i.e. 2g of drag on the vertical. */
            p->vel.z = p->vel.z - pEnv->dt * 19.6200008392334f;

            p->f1E = (uint8_t)BrFtolTrunc(102.0f / p->age);

            if (scale < K_CELL_RECIP || p->vel.z < -30.0f)
                BrPfxFree(pPool, piLink, (uint16_t)iRec);
            else
                piLink = &p->iNext;

            iRec = iNext;
        }
    }
}

/* --------------------------------------------------------------------------
 * 6. Car-driven effects
 * -------------------------------------------------------------------------- */

#define CAR_B(p, o)   ((unsigned char *)(p) + (o))
#define CAR_F(p, o)   (*(float *)   CAR_B(p, o))
#define CAR_I(p, o)   (*(int32_t *) CAR_B(p, o))
#define CAR_U16(p, o) (*(uint16_t *)CAR_B(p, o))
#define CAR_S16(p, o) ((int16_t *)  CAR_B(p, o))
#define CAR_V(p, o)   ((BrVec3 *)   CAR_B(p, o))

/* The four wheel records, in the order both routines index them. */
static const unsigned aWheelOff[4] = { 0x994u, 0x57Cu, 0x370u, 0x788u };

#define WHEEL_SURF(w) (*(signed char *)((w) + 0x1A0))
#define WHEEL_LIVE(w) (*(int32_t *)    ((w) + 0x1B4))

/* 0x10039F20 */
/* WHAT IT DOES: throws dust and spray up from a car's wheels. It only does
 * anything above about forty units of speed, and then only for wheels actually
 * touching a loose surface; the faster the car goes the more often each wheel
 * emits, and each new particle is flung backwards and outwards from the wheel,
 * with the two front wheels also thrown sideways. New particles are nudged part
 * of the way toward where that wheel emitted last time, so a spray follows the
 * wheel's path instead of appearing in a line of separate puffs. */
/* @implements 0x10039F20 d3d BrCarPfxSpawn */
void BrCarPfxSpawn(struct BrCar *pCar, BrPfxPool *pPool, const BrPfxEnv *pEnv,
                   uint32_t *pSeed)
{
    float v, rate, t;
    int i;

    if (CAR_B(pCar, 0x36D)[0] == 0)
        return;

    v = CAR_F(pCar, 0x1030);
    if (v <= 40.0f)             /* 0x1008F5E8 */
        return;

    /* fsubp: (dt * 0.5) - (v * -0.00066006603) */
    rate = pEnv->dt * 0.5f - v * -0.0006600660271942616f;

    for (i = 0; i < 4; i++) {
        unsigned char *pW;
        BrPfxRec *p;
        BrVec3 saved;
        BrVec3 *pPrev;
        unsigned iRec;
        int surf;
        float acc;

        acc = (float)(int)CAR_B(pCar, 0x36D)[0] * 0.029999999329447746f
            * rate + CAR_F(pCar, 0x106C + 4u * (unsigned)i);
        CAR_F(pCar, 0x106C + 4u * (unsigned)i) = acc;
        if (acc <= 0.75f)
            continue;

        pW = CAR_B(pCar, aWheelOff[i]);
        CAR_F(pCar, 0x106C + 4u * (unsigned)i) = 0.0f;

        if (WHEEL_LIVE(pW) == 0)
            continue;
        surf = WHEEL_SURF(pW);
        if (surf <= 0 || surf > 3)
            continue;

        iRec = pPool->iFree;
        if (iRec == 0)
            continue;
        p = &pPool->aRec[iRec];
        pPool->iFree = p->iNext;

        if (surf != 3) {
            p->iNext = pPool->iListAC;
            pPool->iListAC = (uint16_t)iRec;
        } else {
            p->iNext = pPool->iListB4;
            pPool->iListB4 = (uint16_t)iRec;
        }

        BrVec3Scale(&p->vel, CAR_V(pCar, 0x1024), 0.20000000298023224f);
        BrVec3MulAddTo(&p->vel, CAR_V(pCar, 0x20), (i < 2) ? 0.25f : 2.0f);
        if (i < 2)
            BrVec3MulAddTo(&p->vel, CAR_V(pCar, 0x10), (i == 0) ? 0.5f : -0.5f);
        BrVec3SubFrom(&p->vel, CAR_V(pCar, 0x00));

        /* fsubr/fdivr chain: 1 - 50/(v + 50) */
        t = K_1 - 50.0f / (v - -50.0f);
        BrVec3ScaleBy(&p->vel, t);

        BrVec3Sub(&p->pos, CAR_V(pCar, 0x70 + 0x40u * (unsigned)i),
                           CAR_V(pCar, 0x00));
        /* fsubr: pos.z - (h * -0.5) */
        p->pos.z = p->pos.z - CAR_F(pCar, 0x2994) * -0.5f;

        saved = p->pos;
        pPrev = CAR_V(pCar, 0x107C + 12u * (unsigned)i);

        if (BrVec3DistSq(&p->pos, pPrev) < 256.0f) {
            float u = (float)(int)(BrDPlayRandStep(pSeed) & 0xFFFFu)
                    * K_65536_RECIP;
            BrVec3Lerp(&p->pos, pPrev, &p->pos, u * u);
        }
        *pPrev = saved;

        p->age = 0.4000000059604645f;
        p->f1E = 0x19;
        /* fsubr: 16 - (t * -167.3) */
        p->f1F = (uint8_t)BrFtolTrunc(16.0f - t * -167.3000030517578f);
    }
}

/* 0x10039200 */
/* WHAT IT DOES: works out, once a frame and for each of a car's four wheels,
 * what kind of effect that wheel should be showing and which way it should be
 * throwing it -- the direction and the point it comes from, both scaled by how
 * fast the car is going and how much it is sliding sideways. Water is treated
 * differently from dust, a wheel keeps emitting for three frames after it
 * leaves the ground, and the results are written out in the packed form the
 * effect drawing later reads. */
/* @implements 0x10039200 d3d BrCarWheelFx */
void BrCarWheelFx(struct BrCar *pCar, const BrCarFxEnv *pEnv, uint32_t *pSeed)
{
    int bMasked = 0;
    float k1, k2, dot;
    int i;

    if (CAR_I(pCar, 0x294C) != 0 && pEnv->pRecs != NULL) {
        unsigned idx = CAR_U16(pCar, 0x290C);
        if (pEnv->pRecs[idx * 84u + 0x4Cu] & 0x10u)
            bMasked = 1;
    }

    if (pEnv->mode6620 != 0) {
        if (pEnv->sel0B380C != 2 && pEnv->sel0B380C != 8)
            return;
    }

    /* (1 - 50/(speed + 50)) * 3 */
    k1 = (K_1 - 50.0f / (CAR_F(pCar, 0x1030) - -50.0f)) * 3.0f;

    dot = BrVec3Dot(CAR_V(pCar, 0x00), CAR_V(pCar, 0x1024));
    if (dot < K_0)
        dot = -dot;
    /* (1 - 25/(25 - (-2.24 * |dot|))) * 3 */
    k2 = (K_1 - 25.0f / (25.0f - dot * -2.240000009536743f)) * 3.0f;

    for (i = 0; i < 4; i++) {
        unsigned char *pW    = CAR_B(pCar, aWheelOff[i]);
        unsigned char *pSoa  = CAR_B(pCar, 0x10AC + 4u * (unsigned)i);
        BrVec3        *pWpos = CAR_V(pCar, 0x70   + 0x40u * (unsigned)i);
        BrVec3        *pPrev = CAR_V(pCar, 0x10EC + 12u * (unsigned)i);
        int16_t       *pS16  = CAR_S16(pCar, 0x2326 + 0xD8u * (unsigned)i);
        uint16_t      *pTag  = &CAR_U16(pCar, 0x2680 + 0x12u * (unsigned)i);
        float         *pF    = &CAR_F(pCar, 0x1140 + 0x480u * (unsigned)i);

        float *pSoa00 = (float *)  (pSoa + 0x00);
        int32_t *pSoa10 = (int32_t *)(pSoa + 0x10);
        float *pSoa20 = (float *)  (pSoa + 0x20);
        int32_t *pSoa30 = (int32_t *)(pSoa + 0x30);

        BrVec3 vecA, vecB, vecC;
        float t, s, dv;
        int bIdle, kind;

        vecA.x = vecA.y = vecA.z = 0.0f;
        vecB = vecA;
        vecC = vecA;
        s = 0.0f;

        /* fsubr: timer - (dt * -4) */
        t = *pSoa20 - pEnv->dt * -4.0f;
        *pSoa20 = t;
        if (t <= 0.75f) {
            bIdle = 1;
            *pSoa30 = 0;
        } else {
            bIdle = 0;
            if (t < 1.7000000476837158f)
                *pSoa20 = 0.0f;         /* stored as an integer 0 */
            else
                *pSoa20 = t - K_1;
            *pSoa30 = 1;
        }

        if (WHEEL_LIVE(pW) != 0) {
            *pSoa00 = 3.0f;
            *pSoa10 = WHEEL_SURF(pW);   /* movsx: signed */
        } else if (*pSoa00 != K_0) {
            *pSoa00 = *pSoa00 - K_1;
        }

        kind = *pSoa10;
        if (*pSoa00 == K_0)
            goto zero_path;

        if (bIdle) {
            s = 1.5f;
        } else if (kind == 4) {
            s = 2.5f;
        } else if (kind == 3) {
            if (pEnv->flag6624 == 0)
                goto zero_path;
            if (bMasked)
                goto zero_path;
            s = 1.5f;
            *pSoa20 = 7.75f;
        } else {
            goto zero_path;
        }

        if (!bIdle) {
            BrVec3Scale(&vecA, CAR_V(pCar, 0x20), s);
            if (kind != 3) {
                BrVec3MulAddTo(&vecA, CAR_V(pCar, 0x10),
                               (i != 0 && i < 3) ? -s : s);
                BrVec3ScaleBy(&vecA, k1);
                BrVec3MulAddTo(&vecA, CAR_V(pCar, 0x1024),
                               0.30000001192092896f);
                dv = BrVec3Dot(CAR_V(pCar, 0x10), CAR_V(pCar, 0x1024)) * 0.5f;
                BrVec3MulAddTo(&vecA, CAR_V(pCar, 0x10), dv);
            } else {
                BrVec3ScaleBy(&vecA, k2);
                if (k2 != K_0) {
                    if (i >= 2)
                        vecA.z = vecA.z + vecA.z;
                    dv = BrVec3Dot(CAR_V(pCar, 0x10), CAR_V(pCar, 0x1024))
                       * 0.30000001192092896f;
                    BrVec3MulAddTo(&vecA, CAR_V(pCar, 0x10), dv);
                }
            }
        }

        BrVec3MulAdd(&vecB, pWpos, CAR_V(pCar, 0x20), -0.25f);

        if (!(kind == 3 && i >= 2)) {
            float s3 = (i == 0) ? 0.15000000596046448f
                     : (i < 3)  ? -0.15000000596046448f
                                : 0.15000000596046448f;
            BrVec3MulAddTo(&vecB, CAR_V(pCar, 0x10), s3);
        }

        vecC = vecB;
        if (!bIdle) {
            if (BrVec3DistSq(&vecB, pPrev) < 256.0f) {
                float u = (float)(int)(BrDPlayRandStep(pSeed) & 0xFFFFu)
                        * K_65536_RECIP;
                BrVec3Lerp(&vecB, pPrev, &vecB, u * u);
            }
        }
        /* The PRE-lerp value is what gets remembered, on both branches. */
        *pPrev = vecC;
        goto have_vectors;

    zero_path:
        BrVec3Zero(&vecA);
        vecB = *pWpos;

    have_vectors:
        if (!bIdle) {
            pS16[0] = (int16_t)BrFtolTrunc(vecA.x * 127.0f);
            pS16[1] = (int16_t)BrFtolTrunc(vecA.y * 127.0f);
            pS16[2] = (int16_t)BrFtolTrunc(vecA.z * 127.0f);
            if (kind == 3 && pEnv->flag6624 != 0) {
                pS16[-3] = pS16[0];
                pS16[-2] = pS16[1];
            } else {
                pS16[-3] = (int16_t)(pS16[0] >> 1);   /* sar: arithmetic */
                pS16[-2] = (int16_t)(pS16[1] >> 1);
            }
            pS16[-1] = 0;
        }

        *pTag = (uint16_t)kind;

        pF[0] = (float)(int16_t)BrFtolTrunc((vecB.x - CAR_F(pCar, 0x26C8)) * 127.0f);
        pF[1] = (float)(int16_t)BrFtolTrunc((vecB.y - CAR_F(pCar, 0x26CC)) * 127.0f);
        pF[2] = (float)(int16_t)BrFtolTrunc((vecB.z - CAR_F(pCar, 0x26D0)) * 127.0f);
        pF[-8] = pF[0];
        pF[-7] = pF[1];
        pF[-6] = pF[2];
    }
}

/* 0x1003A530 */
/* WHAT IT DOES: the once-a-frame driver for all the dust, spray and wheel
 * effects. It sets the pool up the first time it runs, then -- depending on
 * which mode the game is in -- ages one or another family of particles, lets
 * each car throw up new ones, and updates every car's wheel effects. In one
 * mode no new particles are spawned at all and only the wheels are
 * updated. */
/* @implements 0x1003A530 d3d BrPfxTick */
void BrPfxTick(BrPfxPool *pPool, const BrPfxEnv *pEnv,
               const BrCarFxEnv *pFxEnv, const BrPfxTickEnv *pTick,
               uint32_t *pSeed)
{
    int i;

    if (*pTick->pbInit == 0) {
        BrPfxReset(pPool);
        *pTick->pbInit = 1;
    }

    if (pTick->mode6620 != 0) {
        BrPfxUpdateB0(pPool, pEnv);
        for (i = 0; i < pTick->nCar; i++) {
            struct BrCar *pCar = pTick->apCar[i];
            if (pCar == NULL)
                continue;
            BrCarSub9020(pCar);
            BrCarWheelFx(pCar, pFxEnv, pSeed);
        }
        return;
    }

    if (pTick->mode661C == 0 && pTick->flag6624 == 0) {
        BrPfxUpdateB4AC(pPool, pEnv);
        for (i = 0; i < pTick->nCar; i++) {
            struct BrCar *pCar = pTick->apCar[i];
            if (pCar == NULL)
                continue;
            BrCarPfxSpawn(pCar, pPool, pEnv, pSeed);
            BrCarWheelFx(pCar, pFxEnv, pSeed);
        }
        return;
    }

    for (i = 0; i < pTick->nCar; i++) {
        struct BrCar *pCar = pTick->apCar[i];
        if (pCar == NULL)
            continue;
        BrCarWheelFx(pCar, pFxEnv, pSeed);
    }
}
