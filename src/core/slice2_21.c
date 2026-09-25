/* slice2_21.c -- 0x10039200 .. 0x1003BC90, decompiled from BRD3D.dll.
 *
 * See slice2_21.h for the module map, the field offsets and the gotchas.
 *
 * Every x87 sequence below was traced instruction by instruction through its
 * fxch/fsubr/fdivr chain; the reversed operand forms (fsubr, fdivr, fsubp
 * st(n)) are spelled out in the arithmetic here rather than "tidied", because
 * `fsubr m32` is `st0 = m32 - st0` and getting that backwards is silent.
 */
#ifdef BR_MATCHING_BUILD
#define BrSpanTestPoint BrSpanTestPoint_port
#define BrPfxReset      BrPfxReset_port
#endif
#include "slice2_21.h"
#ifdef BR_MATCHING_BUILD
#undef BrSpanTestPoint
#undef BrPfxReset
int  BrSpanTestPoint(float x, float y);
void BrPfxReset(void);
int  BrSpanContains(int param_1, int param_2);
#endif

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

/* 0x1003AE50 */
/* @n64 0x80224760 located */
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

/* --------------------------------------------------------------------------
 * 2. 4x4 matrices
 * -------------------------------------------------------------------------- */

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

/* 0x1003BC90 BrSeg2SideTest now lives in src/core/geometry/br_seg2.c. */

/* 0x1003BA70 BrSeg2Intersect now lives in src/core/geometry/br_seg2.c. */


/* --------------------------------------------------------------------------
 * 4. Coverage grid
 * -------------------------------------------------------------------------- */


/* 0x1003A990 */
/* WHAT IT DOES: works out the coarse footprint of an eight-sided shape -- a
 * top point, a bottom point and a four-corner ring between them -- by wiping the
 * grid, drawing all twelve of its edges onto it, and then reducing each column
 * to the first and last row that the shape reaches. The result is a cheap
 * stand-in for the shape that later tests can be run against. */
/* @implements 0x1003A990 d3d BrSpanBuildHull */
#ifdef BR_MATCHING_BUILD
/* NO ARGUMENTS, ABSOLUTE GLOBALS, AND THE TWELVE EDGES UNROLLED. The port
 * takes the volume and the point array as parameters and walks a static edge
 * table; the original reads both as absolute globals and emits twelve
 * separate calls, which is 86 of the 86 missing instructions.
 *
 *   0x106EA3A0  the six points, stride 12 (only x and y are read)
 *   0x10AC2F60  aRowHi[64]   0x10AC2E60  aRowLo[64]
 *   0x10AC2D60  aMax[64]     0x10AC2C60  aMin[64]
 *   0x10AC2C50  colHi   0x10AC2C54  rowHi
 *   0x10AC2C58  colLo   0x10AC2C5C  rowLo
 *
 * The two inner scans are UNBOUNDED in the original (`inc edx; jmp` /
 * `dec ecx; jmp`), so a column no row covers walks off both ends of the
 * 64-entry arrays. Reproduced; the port arm below keeps its bounds.
 *
 * RESIDUE (12+9 regnorm, -7 bytes, 192 instructions against 189, from
 * 59+145 and -327): 0x3F appears FOUR times -- the two `Lo` initialisers and
 * the two `Hi` clamps -- and VC5 pools it into edi, which then costs a
 * `push ebx` for the register it displaced and turns every later
 * `mov esi,0x3f` into `mov esi,edi`. The ORIGINAL materialises the immediate
 * at each of the four sites. This is the documented literal-pooling class
 * (see the BrOptCycleTrack and BrPadTranslate entries in
 * docs/VC5-IDIOMS.md, where && / !-form / split-statement spellings were all
 * probed and all pool); every remaining divergence in this function traces
 * to that one decision. */
extern float   g_aBrSpanPt[6][3];   /* 0x106EA3A0 */
extern int32_t g_aBrSpanRowHi[64];  /* 0x10AC2F60 */
extern int32_t g_aBrSpanRowLo[64];  /* 0x10AC2E60 */
extern int32_t g_aBrSpanMax[64];    /* 0x10AC2D60 */
extern int32_t g_aBrSpanMin[64];    /* 0x10AC2C60 */
extern int32_t g_brSpanColHi;       /* 0x10AC2C50 */
extern int32_t g_brSpanRowHiG;      /* 0x10AC2C54 */
extern int32_t g_brSpanColLo;       /* 0x10AC2C58 */
extern int32_t g_brSpanRowLoG;      /* 0x10AC2C5C */
extern void BrSpanAddLineG(float ax, float ay, float bx, float by);

#define BR_SPAN_EDGE(a, b)                                              \
    BrSpanAddLineG(g_aBrSpanPt[a][0], g_aBrSpanPt[a][1],                \
                   g_aBrSpanPt[b][0], g_aBrSpanPt[b][1])

void BrSpanBuildHull(void)
{
    int32_t i, col, lo, hi;

    for (i = 0; i < 64; i++) g_aBrSpanRowHi[i] = 0;
    for (i = 0; i < 64; i++) g_aBrSpanRowLo[i] = 64;
    for (i = 0; i < 64; i++) g_aBrSpanMax[i]   = 0;
    for (i = 0; i < 64; i++) g_aBrSpanMin[i]   = 64;

    g_brSpanColHi  = 0;
    g_brSpanRowHiG = 0;
    g_brSpanColLo  = 0x3F;
    g_brSpanRowLoG = 0x3F;

    BR_SPAN_EDGE(0, 1);
    BR_SPAN_EDGE(0, 2);
    BR_SPAN_EDGE(0, 3);
    BR_SPAN_EDGE(0, 4);
    BR_SPAN_EDGE(5, 1);
    BR_SPAN_EDGE(5, 2);
    BR_SPAN_EDGE(5, 3);
    BR_SPAN_EDGE(5, 4);
    BR_SPAN_EDGE(1, 2);
    BR_SPAN_EDGE(2, 3);
    BR_SPAN_EDGE(3, 4);
    BR_SPAN_EDGE(4, 1);

    if (g_brSpanColLo  < 0)  g_brSpanColLo  = 0;
    if (g_brSpanRowLoG < 0)  g_brSpanRowLoG = 0;
    if (g_brSpanColHi  >= 64) g_brSpanColHi  = 0x3F;
    if (g_brSpanRowHiG >= 64) g_brSpanRowHiG = 0x3F;

    for (col = g_brSpanColLo; col <= g_brSpanColHi; col++) {
        lo = g_brSpanRowLoG;
        while (col < g_aBrSpanMin[lo] || col > g_aBrSpanMax[lo])
            lo++;
        hi = g_brSpanRowHiG;
        while (col < g_aBrSpanMin[hi] || col > g_aBrSpanMax[hi])
            hi--;
        g_aBrSpanRowLo[col] = lo;
        g_aBrSpanRowHi[col] = hi;
    }
}
#else
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
#endif


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

/* 0x1003A610 */
/* WHAT IT DOES: takes a complete copy of the particle pool -- every record and
 * all four list heads -- so it can be examined or restored later. */
/* port-only body; Glide match is src/core/generated/0x10033C90.c */
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
/* port-only body; Glide match is src/core/generated/0x10033880.c
 * (the original takes no arguments and reaches the pool through globals). */
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
/* port-only body; the Glide transcription is
 * src/core/generated/0x100339C0.c (same globals-parameter class). */
#ifdef BR_MATCHING_BUILD
/* No parameters: dt, drift, the record array (0x10AC0C48, 32-byte
 * records, 1-based) and the three list heads are globals; the free is
 * INLINED against the free-head 0x10AC0C38. */
extern float    g_fPfxDt;        /* 0x106E9D8C */
extern float    g_vPfxDriftX;    /* 0x104ADD40 */
extern float    g_vPfxDriftY;    /* 0x104ADD44 */
extern float    g_vPfxDriftZ;    /* 0x104ADD48 */
extern BrPfxRec g_aPfxRec[];     /* 0x10AC0C48 */
extern int32_t g_iPfxHeadB4;     /* 0x10AC0C44 -- read as a DWORD and
                                  * masked; the head is the low word */
extern int32_t g_iPfxHeadAC;     /* 0x10AC0C3C */
extern uint16_t g_iPfxFree;     /* 0x10AC0C38 */
extern const float kPfx0_7;      /* 0x100775D4  0.7   */
extern const float kPfx19_62;    /* 0x100775D8  19.62 */
extern const float kPfx102;      /* 0x100775DC  102.0 */
extern const float kPfxRecip;    /* 0x100775C4  1/65280 */
extern const float kPfxNeg0_8;   /* 0x100775C8  -0.8  */
extern const float kPfxCell;     /* 0x100775D0  0.03125 */
extern const float kPfxNeg30;    /* 0x100775E0  -30.0 */

/* RESIDUE (13+6 regnorm, T3a-float): the original carries k, scale and
 * the dt-products on the x87 stack across the whole double loop (fmul
 * st(3)/fld st(3), one fstp st(0) at exit); this build spills scale and k
 * to [esp] slots around the __ftol call.  The raw (int32_t) cast (not the
 * BrFtolTrunc helper) and dword head reads were required steps; the
 * remaining carry discipline is the documented float-DAG scheduling
 * class. */
void BrPfxUpdateB4AC(void)
{
    float k = g_fPfxDt * kPfx0_7;
    int pass;

    for (pass = 0; pass < 2; pass++) {
        uint16_t *piLink;
        unsigned iRec;
        int iNext;

        /* Per-arm value+address assignments (each arm loads its own head
         * and stores its address as an immediate); a ternary + *piLink
         * re-reads the head through the stored pointer. */
        if (pass != 0) {
            iRec   = (unsigned)g_iPfxHeadAC & 0xFFFFu;
            piLink = (uint16_t *)&g_iPfxHeadAC;
        } else {
            iRec   = (unsigned)g_iPfxHeadB4 & 0xFFFFu;
            piLink = (uint16_t *)&g_iPfxHeadB4;
        }

        while (iRec != 0) {
            BrPfxRec *p = &g_aPfxRec[iRec];
            float scale;

            iNext = p->iNext;
            p->age = k + p->age;
            scale = (float)((int)p->f1F * (int)p->f1E) * kPfxRecip;

            p->pos.x = (p->vel.x * scale) * g_fPfxDt + g_vPfxDriftX + p->pos.x;
            p->pos.y = (p->vel.y * scale) * g_fPfxDt + g_vPfxDriftY + p->pos.y;
            p->pos.z = (scale * p->vel.z - kPfxNeg0_8) * g_fPfxDt + g_vPfxDriftZ
                     + p->pos.z;

            p->vel.z = p->vel.z - g_fPfxDt * kPfx19_62;

            /* Raw (int32_t) cast -> the __ftol CALL, which VC5 knows
             * preserves the x87 stack; a real helper call would force k
             * and the carried dt-products into memory slots. */
            p->f1E = (uint8_t)(int32_t)(kPfx102 / p->age);

            if (scale < kPfxCell || p->vel.z < kPfxNeg30) {
                *piLink = p->iNext;
                p->iNext = g_iPfxFree;
                g_iPfxFree = (uint16_t)iRec;
            } else {
                piLink = &p->iNext;
            }

            iRec = iNext;
        }
    }
}
#else
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
#endif

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
/* @d3donly 0x10039F20 BrCarPfxSpawn -- glide twin 0x100335A0 is the Glide arm in gamedata/br_pfx.c */
#ifndef BR_MATCHING_BUILD
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
#endif /* !BR_MATCHING_BUILD */

/* 0x10039200 BrCarWheelFx now lives in src/core/drawing/br_carwheelfx.c. */

/* 0x1003A530 */
/* WHAT IT DOES: the once-a-frame driver for all the dust, spray and wheel
 * effects. It sets the pool up the first time it runs, then -- depending on
 * which mode the game is in -- ages one or another family of particles, lets
 * each car throw up new ones, and updates every car's wheel effects. In one
 * mode no new particles are spawned at all and only the wheels are
 * updated. */
/* port-only body; Glide match is src/core/generated/0x10033BB0.c
 * (the original takes NO arguments -- pool, modes, car count and car table
 * are all globals; the aggregate parameters below are a port addition). */
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
#ifndef BR_MATCHING_BUILD
            if (pCar == NULL)
                continue;
#endif
            BrCarSub9020(pCar);
#ifdef BR_MATCHING_BUILD
            BrCarWheelFx(pCar);
#else
            BrCarWheelFx(pCar, pFxEnv, pSeed);
#endif
        }
        return;
    }

    if (pTick->mode661C == 0 && pTick->flag6624 == 0) {
        BrPfxUpdateB4AC(pPool, pEnv);
        for (i = 0; i < pTick->nCar; i++) {
            struct BrCar *pCar = pTick->apCar[i];
#ifndef BR_MATCHING_BUILD
            if (pCar == NULL)
                continue;
#endif
            BrCarPfxSpawn(pCar, pPool, pEnv, pSeed);
#ifdef BR_MATCHING_BUILD
            BrCarWheelFx(pCar);
#else
            BrCarWheelFx(pCar, pFxEnv, pSeed);
#endif
        }
        return;
    }

    for (i = 0; i < pTick->nCar; i++) {
        struct BrCar *pCar = pTick->apCar[i];
#ifndef BR_MATCHING_BUILD
        if (pCar == NULL)
            continue;
#endif
#ifdef BR_MATCHING_BUILD
        BrCarWheelFx(pCar);
#else
        BrCarWheelFx(pCar, pFxEnv, pSeed);
#endif
    }
}

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
extern int DAT_10ac2c54;
extern int DAT_10ac2c5c;
extern int DAT_10ac2c60;
extern int DAT_10ac2d60;

#endif /* BR_MATCHING_BUILD */

