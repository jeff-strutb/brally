/* test_slice2_21.c -- behaviour/invariant tests for slice2_21.
 *
 * Assertions are properties of the ORIGINAL (identities, round trips, the
 * clamps and sentinels the disassembly actually contains), not restatements of
 * the C. Where a value is hand-computed it is computed from the disassembly's
 * constants, and the inputs are chosen so most of the arithmetic cancels.
 */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "slice2_21.h"

/* ==========================================================================
 * Stand-ins for the cross-slice leaves. TEST ONLY -- the real bodies live at
 * the addresses named in slice2_21.h and are wired up by integration.
 * ========================================================================== */
float BrSqrtF(float x) { return sqrtf(x); }
float BrSinF(float x)  { return sinf(x); }

int32_t BrFtolTrunc(float f)
{
    /* __ftol: `fistp qword` with the rounding mode forced to truncate, then
     * the low dword. Out of range / NaN gives the integer indefinite
     * 0x8000000000000000, whose low dword is 0. */
    double d = (double)f;
    if (!(d > -9223372036854775808.0 && d < 9223372036854775808.0))
        return 0;
    return (int32_t)(long long)d;
}
int32_t BrFtolArg(float f) { return BrFtolTrunc(f); }

/* The 27-bit multiplicative generator of slice2_22 (0x1003BD50). */
uint32_t BrDPlayRandStep(uint32_t *pSeed)
{
    *pSeed = (*pSeed * 16807u) & 0x07FFFFFFu;
    return *pSeed;
}

static int g_nSub9020;
void BrCarSub9020(struct BrCar *pCar) { (void)pCar; g_nSub9020++; }

/* br_vec.h -- stand-ins matching the documented semantics. */
float BrVec3Dot(const BrVec3 *a, const BrVec3 *b)
{ return a->x*b->x + a->y*b->y + a->z*b->z; }
float BrVec3DistSq(const BrVec3 *a, const BrVec3 *b)
{ float dx=a->x-b->x, dy=a->y-b->y, dz=a->z-b->z; return dx*dx+dy*dy+dz*dz; }
void BrVec3Zero(BrVec3 *v) { v->x = v->y = v->z = 0.0f; }
void BrVec3Sub(BrVec3 *o, const BrVec3 *a, const BrVec3 *b)
{ o->x=a->x-b->x; o->y=a->y-b->y; o->z=a->z-b->z; }
void BrVec3SubFrom(BrVec3 *a, const BrVec3 *b)
{ a->x-=b->x; a->y-=b->y; a->z-=b->z; }
void BrVec3Scale(BrVec3 *o, const BrVec3 *v, float s)
{ o->x=v->x*s; o->y=v->y*s; o->z=v->z*s; }
void BrVec3ScaleBy(BrVec3 *v, float s) { v->x*=s; v->y*=s; v->z*=s; }
void BrVec3MulAdd(BrVec3 *o, const BrVec3 *a, const BrVec3 *b, float s)
{ o->x=a->x+b->x*s; o->y=a->y+b->y*s; o->z=a->z+b->z*s; }
void BrVec3MulAddTo(BrVec3 *a, const BrVec3 *b, float s)
{ a->x+=b->x*s; a->y+=b->y*s; a->z+=b->z*s; }
void BrVec3Lerp(BrVec3 *o, const BrVec3 *a, const BrVec3 *b, float t)
{ o->x=(a->x-b->x)*t+b->x; o->y=(a->y-b->y)*t+b->y; o->z=(a->z-b->z)*t+b->z; }

/* br_span.h -- stand-ins transcribed from br_span.c. */
static int span_clamp63(int v)
{ return v < 0 ? 0 : (v >= BR_SPAN_ROWS ? BR_SPAN_ROWS - 1 : v); }
void BrSpanAdd(BrSpanGrid *g, int col, int row)
{
    col = span_clamp63(col);
    row = span_clamp63(row);
    if (col < g->aMin[row]) g->aMin[row] = col;
    if (col > g->aMax[row]) g->aMax[row] = col;
}
int BrSpanTest(const BrSpanGrid *g, int col, int row)
{
    if (row < g->rowLo || row > g->rowHi) return 0;
    if (col < g->aMin[row] || col > g->aMax[row]) return 0;
    return 1;
}

/* ========================================================================== */

static int g_nFail;
#define CHECK(c) do { if (!(c)) { \
    printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); g_nFail++; } } while (0)
#define NEAR(a, b, e) CHECK(fabs((double)(a) - (double)(b)) <= (e))

/* --------------------------------------------------------------------------
 * 1. Geometry leaves
 * -------------------------------------------------------------------------- */
static void test_vec(void)
{
    BrVec3 v, o, a, b;

    /* Normalising leaves unit length. */
    v.x = 3.0f; v.y = -4.0f; v.z = 12.0f;
    BrVec3NormaliseGuard(&v);
    NEAR(sqrt((double)(v.x*v.x + v.y*v.y + v.z*v.z)), 1.0, 1e-5);
    /* ... and does not flip any component. */
    CHECK(v.x > 0.0f && v.y < 0.0f && v.z > 0.0f);

    /* The zero sentinel: (0,0,1), NOT the input and NOT NaN. */
    v.x = v.y = v.z = 0.0f;
    BrVec3NormaliseGuard(&v);
    CHECK(v.x == 0.0f && v.y == 0.0f && v.z == 1.0f);

    /* BrVec3Direction points FROM arg2 TO arg3. */
    a.x = 1.0f; a.y = 1.0f; a.z = 1.0f;
    b.x = 4.0f; b.y = 1.0f; b.z = 1.0f;
    BrVec3Direction(&o, &a, &b);
    NEAR(o.x, 1.0, 1e-6); NEAR(o.y, 0.0, 1e-6); NEAR(o.z, 0.0, 1e-6);
    BrVec3Direction(&o, &b, &a);
    NEAR(o.x, -1.0, 1e-6);
    /* Same zero sentinel. */
    BrVec3Direction(&o, &a, &a);
    CHECK(o.x == 0.0f && o.y == 0.0f && o.z == 1.0f);

    /* The XY length genuinely ignores z (0x1003B170's 3D form lives in
     * br_vec.c as BrVec3Length and is not part of this module). */
    v.x = 3.0f; v.y = 4.0f; v.z = 12.0f;
    NEAR(BrVec3LenXY(&v), 5.0, 1e-4);
    v.z = -1000.0f;
    NEAR(BrVec3LenXY(&v), 5.0, 1e-4);

    a.x = 10.0f; a.y = 10.0f; a.z = 999.0f;
    b.x = 13.0f; b.y = 14.0f; b.z = -999.0f;
    NEAR(BrVec3DistXY(&a, &b), 5.0, 1e-4);
    /* Symmetric. */
    NEAR(BrVec3DistXY(&a, &b), BrVec3DistXY(&b, &a), 1e-6);
}

/* --------------------------------------------------------------------------
 * 2. atan2
 * -------------------------------------------------------------------------- */
static void test_atan2(void)
{
    int i;

    /* The degenerate case the original guards with `r == 0`. */
    CHECK(BrAtan2(0.0f, 0.0f) == 0.0f);

    /* Argument order is (x, y) and the range is [0, 2*pi).
     * The bisection stops at |sin err| < 0.005, so ~0.02 rad is the honest
     * tolerance -- this is not a machine-precision atan2. */
    for (i = 0; i < 64; i++) {
        double th = (double)i * (2.0 * M_PI / 64.0) + 0.013;
        float  x  = (float)(7.5 * cos(th));
        float  y  = (float)(7.5 * sin(th));
        double got = (double)BrAtan2(x, y);
        double want = atan2((double)y, (double)x);
        if (want < 0.0)
            want += 2.0 * M_PI;
        CHECK(got >= -0.05 && got <= 2.0 * M_PI + 0.05);
        NEAR(got, want, 0.05);
    }

    /* Scale invariance: only the direction matters. */
    NEAR(BrAtan2(1.0f, 2.0f), BrAtan2(1000.0f, 2000.0f), 1e-5);
}

/* --------------------------------------------------------------------------
 * 3. Matrices
 * -------------------------------------------------------------------------- */
static void mat_ident(BrMat4 *m)
{
    int r, c;
    for (r = 0; r < 4; r++)
        for (c = 0; c < 4; c++)
            m->m[r][c] = (r == c) ? 1.0f : 0.0f;
}

static void test_mat(void)
{
    BrMat4 id, m, n, p, inv;
    BrVec3 v;
    float  out4[4], dir[4];
    BrVec3 d3;
    int r, c;

    mat_ident(&id);

    /* A non-orthonormal affine transform: rot(z, 0.3) * scale(2,3,1) + T. */
    mat_ident(&m);
    m.m[0][0] =  2.0f * (float)cos(0.3); m.m[0][1] = 3.0f * (float)sin(0.3);
    m.m[1][0] = -2.0f * (float)sin(0.3); m.m[1][1] = 3.0f * (float)cos(0.3);
    m.m[2][2] =  1.5f;
    m.m[3][0] = 11.0f; m.m[3][1] = -4.0f; m.m[3][2] = 7.0f;

    /* Identity is neutral on both sides. */
    BrMtxMul(&p, &m, &id);
    for (r = 0; r < 4; r++) for (c = 0; c < 4; c++) NEAR(p.m[r][c], m.m[r][c], 1e-5);
    BrMtxMul(&p, &id, &m);
    for (r = 0; r < 4; r++) for (c = 0; c < 4; c++) NEAR(p.m[r][c], m.m[r][c], 1e-5);

    /* The original multiplies into a stack temp, so out may alias an input. */
    n = m;
    BrMtxMul(&n, &n, &n);
    BrMtxMul(&p, &m, &m);
    for (r = 0; r < 4; r++) for (c = 0; c < 4; c++) NEAR(n.m[r][c], p.m[r][c], 1e-4);

    /* Point transform uses row 3; direction transform does not. The two must
     * differ by exactly the translation row. */
    v.x = 1.5f; v.y = -2.5f; v.z = 0.25f;
    BrMat4TransformPoint4(out4, &v, &m.m[0][0]);
    BrMtxXfmDir3(&d3, &v, &m);
    dir[0] = d3.x; dir[1] = d3.y; dir[2] = d3.z;
    for (c = 0; c < 3; c++)
        NEAR(out4[c] - dir[c], m.m[3][c], 1e-4);
    /* The fourth component exists and picks up column 3. */
    NEAR(out4[3], m.m[3][3], 1e-4);

    /* Inverse round trip through the point transform. */
    CHECK(BrMtxInvert(&inv, &m) == 1);
    {
        BrVec3 back;
        float mid[4], fin[4];
        BrMat4TransformPoint4(mid, &v, &m.m[0][0]);
        back.x = mid[0]; back.y = mid[1]; back.z = mid[2];
        BrMat4TransformPoint4(fin, &back, &inv.m[0][0]);
        NEAR(fin[0], v.x, 1e-3);
        NEAR(fin[1], v.y, 1e-3);
        NEAR(fin[2], v.z, 1e-3);
    }
    /* Column 3 is forced, not copied. */
    NEAR(inv.m[0][3], 0.0, 1e-9);
    NEAR(inv.m[1][3], 0.0, 1e-9);
    NEAR(inv.m[2][3], 0.0, 1e-9);
    NEAR(inv.m[3][3], 1.0, 1e-9);

    /* Singular input: identity is written and the return value is STILL 1
     * (both exits of the original load eax = 1). */
    mat_ident(&n);
    n.m[1][1] = 0.0f; n.m[1][0] = 0.0f; n.m[1][2] = 0.0f;
    CHECK(BrMtxInvert(&inv, &n) == 1);
    for (r = 0; r < 4; r++)
        for (c = 0; c < 4; c++)
            NEAR(inv.m[r][c], (r == c) ? 1.0 : 0.0, 1e-9);
}

/* --------------------------------------------------------------------------
 * 4. Segment predicates
 * -------------------------------------------------------------------------- */
static void test_seg(void)
{
    BrVec2 a, b, c, d;

    /* A proper X crossing. */
    a.x = -1.0f; a.y =  0.0f;  b.x = 1.0f; b.y = 0.0f;
    c.x =  0.0f; c.y = -1.0f;  d.x = 0.0f; d.y = 1.0f;
    CHECK(BrSeg2Intersect(&a, &b, &c, &d) != 0);
    CHECK(BrSeg2Intersect(&c, &d, &a, &b) != 0);
    /* c and d straddle a-b, and their cross products differ -> 1. */
    CHECK(BrSeg2SideTest(&a, &b, &c, &d) == 1);

    /* Disjoint bounding boxes on x: rejected before any cross product. */
    c.x = 100.0f; c.y = -1.0f; d.x = 100.0f; d.y = 1.0f;
    CHECK(BrSeg2Intersect(&a, &b, &c, &d) == 0);
    /* Rejection is symmetric in the two segments. */
    CHECK(BrSeg2Intersect(&c, &d, &a, &b) == 0);

    /* Boxes overlap but c and d are on the same side of a-b -> 0. */
    c.x = -0.5f; c.y = 0.5f;  d.x = 0.5f; d.y = 0.25f;
    CHECK(BrSeg2SideTest(&a, &b, &c, &d) == 0);
    CHECK(BrSeg2Intersect(&a, &b, &c, &d) == 0);

    /* Equal NON-ZERO cross products can never reach the 1-vs-2 compare: two
     * equal non-zero crosses are by definition on the same side, so the
     * same-side rejection fires first and the answer is 0. The reserved 2 is
     * therefore reachable only when both crosses are exactly zero. */
    c.x = 0.0f; c.y = 1.0f; d.x = 0.0f; d.y = 1.0f;
    CHECK(BrSeg2SideTest(&a, &b, &c, &d) == 0);
    /* Both collinear with a-b: both crosses are 0 -> 2. */
    c.x = -0.5f; c.y = 0.0f; d.x = 0.5f; d.y = 0.0f;
    CHECK(BrSeg2SideTest(&a, &b, &c, &d) == 2);
    CHECK(BrSeg2Intersect(&a, &b, &c, &d) == 2);
}

/* --------------------------------------------------------------------------
 * 5. Coverage grid
 * -------------------------------------------------------------------------- */
static void test_span(void)
{
    static BrSpanVolume vol;
    BrVec3 pt[6];
    int col, nCovered = 0;

    /* Cell coordinates scaled into world units by BR_SPAN_CELL. */
    pt[0].x = 10*32.0f; pt[0].y = 10*32.0f; pt[0].z = 0.0f;  /* apex   */
    pt[1].x = 20*32.0f; pt[1].y = 15*32.0f; pt[1].z = 0.0f;  /* ring   */
    pt[2].x = 20*32.0f; pt[2].y = 25*32.0f; pt[2].z = 0.0f;
    pt[3].x = 30*32.0f; pt[3].y = 25*32.0f; pt[3].z = 0.0f;
    pt[4].x = 30*32.0f; pt[4].y = 15*32.0f; pt[4].z = 0.0f;
    pt[5].x = 40*32.0f; pt[5].y = 20*32.0f; pt[5].z = 0.0f;  /* apex   */

    BrSpanBuildHull(&vol, pt);

    /* The clamps at the end of 0x1003A990. */
    CHECK(vol.colLo >= 0 && vol.colHi <= BR_SPAN_ROWS - 1);
    CHECK(vol.grid.rowLo >= 0 && vol.grid.rowHi <= BR_SPAN_ROWS - 1);
    CHECK(vol.colLo <= vol.colHi);
    CHECK(vol.grid.rowLo <= vol.grid.rowHi);
    /* The hull spans columns 10..40 and rows 10..25. */
    CHECK(vol.colLo <= 10 && vol.colHi >= 40);
    CHECK(vol.grid.rowLo <= 10 && vol.grid.rowHi >= 25);

    /* Every vertex cell was inserted, so it must test inside. */
    CHECK(BrSpanTestPoint(&vol, pt[0].x, pt[0].y) == 1);
    CHECK(BrSpanTestPoint(&vol, pt[5].x, pt[5].y) == 1);
    CHECK(BrSpanTestPoint(&vol, pt[2].x, pt[2].y) == 1);

    /* Well outside the row range -> rejected by rowLo/rowHi. */
    CHECK(BrSpanTestPoint(&vol, 50*32.0f, 60*32.0f) == 0);

    /* Every row that was touched has a sane span; untouched rows keep the
     * 64/0 emptiness markers the original writes (NOT INT_MAX/INT_MIN). */
    for (col = 0; col < BR_SPAN_ROWS; col++) {
        if (vol.grid.aMin[col] == BR_SPAN_ROWS) {
            CHECK(vol.grid.aMax[col] == 0);
        } else {
            CHECK(vol.grid.aMin[col] <= vol.grid.aMax[col]);
        }
    }

    /* The per-column row range: for a column an edge actually crosses, the
     * derived [aRowLo, aRowHi] must be a non-empty in-range interval. */
    for (col = vol.colLo; col <= vol.colHi; col++) {
        if (vol.aRowLo[col] < BR_SPAN_ROWS && vol.aRowHi[col] >= 0 &&
            vol.aRowLo[col] <= vol.aRowHi[col]) {
            nCovered++;
            CHECK(vol.grid.aMin[vol.aRowLo[col]] <= col);
            CHECK(vol.grid.aMax[vol.aRowLo[col]] >= col);
            CHECK(vol.grid.aMin[vol.aRowHi[col]] <= col);
            CHECK(vol.grid.aMax[vol.aRowHi[col]] >= col);
        }
    }
    CHECK(nCovered > 0);

    /* A degenerate (zero dy) segment returns after the two endpoint inserts
     * and the column min/max, without touching rowLo/rowHi. */
    {
        static BrSpanVolume v2;
        BrVec3 flat[6];
        int i;
        for (i = 0; i < 6; i++) { flat[i].x = 5*32.0f; flat[i].y = 5*32.0f;
                                  flat[i].z = 0.0f; }
        BrSpanBuildHull(&v2, flat);
        CHECK(v2.colLo == 5 && v2.colHi == 5);
        /* Every edge had dy == 0, so rowLo/rowHi kept their initial 63/0 and
         * were then clamped -- rowLo stays above rowHi. */
        CHECK(v2.grid.rowLo == BR_SPAN_ROWS - 1 && v2.grid.rowHi == 0);
        CHECK(BrSpanTestPoint(&v2, 5*32.0f, 5*32.0f) == 0);
    }
}

/* --------------------------------------------------------------------------
 * 6. Particle pool
 * -------------------------------------------------------------------------- */
static void test_pool(void)
{
    static BrPfxPool pool;
    static BrPfxSnapshot snap;
    BrPfxEnv env;
    int i, n;
    unsigned idx;

    memset(&pool, 0xAB, sizeof(pool));
    BrPfxReset(&pool);

    /* Index 0 is the null sentinel; the free list is 1..255 and terminates. */
    CHECK(pool.iFree == 1);
    CHECK(pool.iListAC == 0 && pool.iListB0 == 0 && pool.iListB4 == 0);
    n = 0;
    idx = pool.iFree;
    while (idx != 0) {
        CHECK(idx >= 1 && idx < BR_PFX_RECS);
        idx = pool.aRec[idx].iNext;
        if (++n > BR_PFX_RECS) break;
    }
    CHECK(n == BR_PFX_RECS - 1);
    CHECK(pool.aRec[BR_PFX_RECS - 1].iNext == 0);

    /* Snapshot copies the four heads in the order free, B0, AC, B4. */
    pool.iListB0 = 7; pool.iListAC = 8; pool.iListB4 = 9;
    pool.aRec[3].age = 1.25f;
    BrPfxSaveState(&pool, &snap);
    CHECK(snap.iFree == pool.iFree);
    CHECK(snap.iListB0 == 7 && snap.iListAC == 8 && snap.iListB4 == 9);
    CHECK(snap.aRec[3].age == 1.25f);
    CHECK(memcmp(snap.aRec, pool.aRec, sizeof(snap.aRec)) == 0);

    /* --- integrator B0 --- */
    BrPfxReset(&pool);
    env.dt = 0.5f;
    env.drift.x = 1.0f; env.drift.y = 2.0f; env.drift.z = 3.0f;

    /* One live particle (scale = 200*255/65280 well above 0.03125). */
    idx = pool.iFree;
    pool.iFree = pool.aRec[idx].iNext;
    pool.aRec[idx].iNext = 0;
    pool.iListB0 = (uint16_t)idx;
    pool.aRec[idx].pos.x = 0.0f; pool.aRec[idx].pos.y = 0.0f;
    pool.aRec[idx].pos.z = 0.0f;
    pool.aRec[idx].vel.x = 4.0f; pool.aRec[idx].vel.y = 0.0f;
    pool.aRec[idx].vel.z = 0.0f;
    pool.aRec[idx].age = 1.0f;
    pool.aRec[idx].f1E = 200; pool.aRec[idx].f1F = 255;

    BrPfxUpdateB0(&pool, &env);
    CHECK(pool.iListB0 == (uint16_t)idx);          /* survived */
    NEAR(pool.aRec[idx].age, 1.0 + 0.5 * 0.3, 1e-6);
    {
        double scale = 200.0 * 255.0 / 65280.0;    /* the 1/65280 constant */
        NEAR(pool.aRec[idx].pos.x, 4.0 * scale * 0.5 + 1.0, 1e-4);
        NEAR(pool.aRec[idx].pos.y, 2.0, 1e-4);     /* drift only */
        /* z gets the constant +0.8 added to the velocity first. */
        NEAR(pool.aRec[idx].pos.z, 0.8 * 0.5 + 3.0, 1e-4);
    }
    /* f1E is reloaded from 5.7375 / age^2. */
    {
        double a = (double)pool.aRec[idx].age;
        CHECK(pool.aRec[idx].f1E == (uint8_t)(int)(5.7375006675720215 / (a*a)));
    }

    /* Zero f1E -> scale 0 -> unlinked and returned to the free list head. */
    pool.aRec[idx].f1E = 0;
    BrPfxUpdateB0(&pool, &env);
    CHECK(pool.iListB0 == 0);
    CHECK(pool.iFree == (uint16_t)idx);

    /* --- integrator B4/AC: B4 is walked FIRST --- */
    BrPfxReset(&pool);
    {
        unsigned i4 = pool.iFree;
        unsigned iAC;
        pool.iFree = pool.aRec[i4].iNext;
        iAC = pool.iFree;
        pool.iFree = pool.aRec[iAC].iNext;

        pool.aRec[i4].iNext = 0; pool.iListB4 = (uint16_t)i4;
        pool.aRec[iAC].iNext = 0; pool.iListAC = (uint16_t)iAC;
        for (i = 0; i < 2; i++) {
            BrPfxRec *p = &pool.aRec[i ? iAC : i4];
            p->pos.x = p->pos.y = p->pos.z = 0.0f;
            p->vel.x = p->vel.y = p->vel.z = 0.0f;
            p->age = 1.0f;
            p->f1E = 0;   /* dies immediately */
            p->f1F = 255;
        }
        env.dt = 0.25f;
        BrPfxUpdateB4AC(&pool, &env);
        CHECK(pool.iListB4 == 0 && pool.iListAC == 0);
        /* B4's entry was pushed onto the free list before AC's, so AC's ends
         * up at the head. That ordering is the observable half of "B4 then
         * AC" and is what 0x1003A340's pass 0 / pass 1 split means. */
        CHECK(pool.iFree == (uint16_t)iAC);
        CHECK(pool.aRec[iAC].iNext == (uint16_t)i4);
    }

    /* Gravity and the -30 death gate. */
    BrPfxReset(&pool);
    idx = pool.iFree;
    pool.iFree = pool.aRec[idx].iNext;
    pool.aRec[idx].iNext = 0;
    pool.iListB4 = (uint16_t)idx;
    pool.aRec[idx].pos.x = pool.aRec[idx].pos.y = pool.aRec[idx].pos.z = 0.0f;
    pool.aRec[idx].vel.x = pool.aRec[idx].vel.y = 0.0f;
    pool.aRec[idx].vel.z = 0.0f;
    pool.aRec[idx].age = 1.0f;
    pool.aRec[idx].f1E = 255; pool.aRec[idx].f1F = 255;
    env.dt = 0.1f;
    env.drift.x = env.drift.y = env.drift.z = 0.0f;
    BrPfxUpdateB4AC(&pool, &env);
    CHECK(pool.iListB4 == (uint16_t)idx);
    NEAR(pool.aRec[idx].vel.z, -0.1 * 19.6200008392334, 1e-5);
    NEAR(pool.aRec[idx].age, 1.0 + 0.1 * 0.699999988079071, 1e-6);

    /* Drop it far enough that vel.z passes -30 and it dies. */
    pool.aRec[idx].vel.z = -29.9f;
    pool.aRec[idx].f1E = 255;
    BrPfxUpdateB4AC(&pool, &env);
    CHECK(pool.iListB4 == 0);
    CHECK(pool.iFree == (uint16_t)idx);
}

/* --------------------------------------------------------------------------
 * 7. Car-driven effects
 * -------------------------------------------------------------------------- */
static union { double align; unsigned char b[0x2B68]; } g_car;

#define TC_B(o)   (g_car.b + (o))
#define TC_F(o)   (*(float *)   TC_B(o))
#define TC_I(o)   (*(int32_t *) TC_B(o))
#define TC_U16(o) (*(uint16_t *)TC_B(o))
#define TC_S16(o) (*(int16_t *) TC_B(o))
#define TC_V(o)   (*(BrVec3 *)  TC_B(o))

static const unsigned kWheel[4] = { 0x994u, 0x57Cu, 0x370u, 0x788u };

static void car_clear(void) { memset(&g_car, 0, sizeof(g_car)); }

static void test_spawn(void)
{
    static BrPfxPool pool;
    BrPfxEnv env;
    uint32_t seed = 12345u;
    unsigned iFirst;
    int surf;

    env.dt = 0.1f;
    env.drift.x = env.drift.y = env.drift.z = 0.0f;

    /* Gate 1: the +0x36D byte. */
    car_clear();
    BrPfxReset(&pool);
    iFirst = pool.iFree;
    TC_F(0x1030) = 100.0f;
    BrCarPfxSpawn((struct BrCar *)&g_car, &pool, &env, &seed);
    CHECK(pool.iFree == (uint16_t)iFirst);

    /* Gate 2: speed must be strictly greater than 40. */
    car_clear();
    BrPfxReset(&pool);
    g_car.b[0x36D] = 4;
    TC_F(0x1030) = 40.0f;
    TC_F(0x106C) = 1.0f;
    BrCarPfxSpawn((struct BrCar *)&g_car, &pool, &env, &seed);
    CHECK(pool.iFree == (uint16_t)iFirst);

    /* A real spawn on wheel 0, which is the record at +0x994. */
    for (surf = 1; surf <= 3; surf++) {
        car_clear();
        BrPfxReset(&pool);
        iFirst = pool.iFree;
        g_car.b[0x36D] = 4;
        TC_F(0x1030) = 100.0f;
        TC_F(0x106C) = 1.0f;                 /* accumulator already past 0.75 */
        *(int32_t *)(g_car.b + kWheel[0] + 0x1B4) = 1;
        *(signed char *)(g_car.b + kWheel[0] + 0x1A0) = (signed char)surf;
        TC_V(0x70).x = 5.0f; TC_V(0x70).y = 6.0f; TC_V(0x70).z = 7.0f;
        TC_V(0x107C).x = 1000.0f;            /* far away: no lerp */
        TC_V(0x107C).y = 1000.0f;
        TC_V(0x107C).z = 1000.0f;

        BrCarPfxSpawn((struct BrCar *)&g_car, &pool, &env, &seed);

        /* One record left the free list. */
        CHECK(pool.iFree != (uint16_t)iFirst);
        /* Surface 3 goes on B4, 1 and 2 on AC. */
        if (surf == 3) {
            CHECK(pool.iListB4 == (uint16_t)iFirst && pool.iListAC == 0);
        } else {
            CHECK(pool.iListAC == (uint16_t)iFirst && pool.iListB4 == 0);
        }
        /* The accumulator is cleared on spawn. */
        CHECK(TC_F(0x106C) == 0.0f);
        /* Position is the wheel point minus the car origin, plus half of the
         * +0x2994 float (zero here), and the previous-position slot now holds
         * that same value. */
        NEAR(pool.aRec[iFirst].pos.x, 5.0, 1e-5);
        NEAR(pool.aRec[iFirst].pos.y, 6.0, 1e-5);
        NEAR(pool.aRec[iFirst].pos.z, 7.0, 1e-5);
        NEAR(TC_V(0x107C).x, 5.0, 1e-5);
        /* Fixed spawn constants. */
        NEAR(pool.aRec[iFirst].age, 0.4000000059604645, 1e-7);
        CHECK(pool.aRec[iFirst].f1E == 0x19);
        {
            double t = 1.0 - 50.0 / (100.0 + 50.0);
            CHECK(pool.aRec[iFirst].f1F ==
                  (uint8_t)(int)(16.0 + 167.3000030517578 * t));
        }
        /* Only the wheels whose record is live spawn: wheels 1..3 had
         * +0x1B4 == 0, so exactly one record was taken. */
        CHECK(pool.aRec[pool.iFree].iNext != 0);
    }

    /* +0x2994 contributes half of itself to z. */
    car_clear();
    BrPfxReset(&pool);
    iFirst = pool.iFree;
    g_car.b[0x36D] = 4;
    TC_F(0x1030) = 100.0f;
    TC_F(0x106C) = 1.0f;
    TC_F(0x2994) = 8.0f;
    *(int32_t *)(g_car.b + kWheel[0] + 0x1B4) = 1;
    *(signed char *)(g_car.b + kWheel[0] + 0x1A0) = 1;
    TC_V(0x107C).x = 1000.0f;
    BrCarPfxSpawn((struct BrCar *)&g_car, &pool, &env, &seed);
    NEAR(pool.aRec[iFirst].pos.z, 4.0, 1e-5);
}

static void test_wheelfx(void)
{
    BrCarFxEnv env;
    uint32_t seed = 1u;

    memset(&env, 0, sizeof(env));
    env.dt = 0.1f;

    /* mode6620 set and sel0B380C neither 2 nor 8 -> the whole routine is a
     * no-op, including the timer at +0x10AC+0x20. */
    car_clear();
    env.mode6620 = 1;
    env.sel0B380C = 5;
    TC_F(0x10AC + 0x20) = 0.5f;
    TC_S16(0x2326) = 0x1234;
    BrCarWheelFx((struct BrCar *)&g_car, &env, &seed);
    CHECK(TC_F(0x10AC + 0x20) == 0.5f);
    CHECK(TC_S16(0x2326) == 0x1234);

    /* sel0B380C == 8 lets it through. */
    env.sel0B380C = 8;
    BrCarWheelFx((struct BrCar *)&g_car, &env, &seed);
    CHECK(TC_F(0x10AC + 0x20) != 0.5f);

    /* Idle branch: the timer stays at or below 0.75, so the s16 stream is
     * left alone but the tag and the float stream are still written. */
    car_clear();
    memset(&env, 0, sizeof(env));
    env.dt = 0.1f;
    TC_F(0x1030) = 100.0f;
    *(int32_t *)(g_car.b + kWheel[0] + 0x1B4) = 1;
    *(signed char *)(g_car.b + kWheel[0] + 0x1A0) = 2;
    TC_V(0x70).x = 1.0f; TC_V(0x70).y = 2.0f; TC_V(0x70).z = 3.0f;
    TC_S16(0x2326) = 0x5A5A;
    BrCarWheelFx((struct BrCar *)&g_car, &env, &seed);
    CHECK(TC_S16(0x2326) == 0x5A5A);          /* untouched while idle */
    CHECK(TC_I(0x10AC + 0x30) == 0);          /* the idle flag */
    CHECK(TC_U16(0x2680) == 2);               /* tag = surface id */
    CHECK(TC_F(0x10AC) == 3.0f);              /* contact resets +0x00 to 3 */
    /* Axes are all zero, so the encoded point is the wheel point * 127. */
    NEAR(TC_F(0x1140), 127.0, 1e-5);
    NEAR(TC_F(0x1140 + 4), 254.0, 1e-5);
    NEAR(TC_F(0x1140 + 8), 381.0, 1e-5);
    /* ... and it is mirrored 0x20 bytes earlier. */
    NEAR(TC_F(0x1140 - 0x20), 127.0, 1e-5);
    NEAR(TC_F(0x1140 - 0x1C), 254.0, 1e-5);
    NEAR(TC_F(0x1140 - 0x18), 381.0, 1e-5);

    /* Active branch, surface 4: the direction vector is axis B * 2.5 scaled
     * by (1 - 50/(speed+50)) * 3 = 2.0 at speed 100, then encoded * 127. The
     * halved copy at [-3]/[-2] uses an arithmetic shift. */
    car_clear();
    TC_F(0x1030) = 100.0f;
    TC_F(0x10AC + 0x20) = 1.0f;               /* timer -> 1.4, past 0.75 */
    *(int32_t *)(g_car.b + kWheel[0] + 0x1B4) = 1;
    *(signed char *)(g_car.b + kWheel[0] + 0x1A0) = 4;
    TC_V(0x20).x = 1.0f;                      /* axis B */
    TC_V(0x70).x = 1.0f; TC_V(0x70).y = 2.0f; TC_V(0x70).z = 3.0f;
    TC_V(0x10EC).x = 1000.0f;                 /* far: no lerp */
    BrCarWheelFx((struct BrCar *)&g_car, &env, &seed);
    CHECK(TC_I(0x10AC + 0x30) == 1);
    CHECK(TC_F(0x10AC + 0x20) == 0.0f);       /* 1.4 < 1.7 -> zeroed */
    /* 2.5 * k1 * 127 where k1 = (1 - 50/150) * 3. In float that is
     * 4.99999952 * 127 = 634.99994, and __ftol TRUNCATES -- 634, not 635.
     * This is exactly why the conversion has to be __ftol and not a round. */
    CHECK(TC_S16(0x2326) == 634);
    CHECK(TC_S16(0x2326 + 2) == 0);
    CHECK(TC_S16(0x2326 + 4) == 0);
    CHECK(TC_S16(0x2326 - 6) == 317);         /* 634 >> 1 */
    CHECK(TC_S16(0x2326 - 4) == 0);
    CHECK(TC_S16(0x2326 - 2) == 0);           /* always cleared */
    CHECK(TC_U16(0x2680) == 4);
    /* vecB = wheel point + axisB * -0.25, then encoded. */
    NEAR(TC_F(0x1140), (double)(int16_t)(int)((1.0 - 0.25) * 127.0), 1e-5);
    /* The pre-lerp value is what lands in the previous-position slot. */
    NEAR(TC_V(0x10EC).x, 0.75, 1e-5);
    NEAR(TC_V(0x10EC).y, 2.0, 1e-5);

    /* A timer past 1.7 wraps by subtracting 1 rather than zeroing. */
    car_clear();
    TC_F(0x1030) = 100.0f;
    TC_F(0x10AC + 0x20) = 1.8f;
    BrCarWheelFx((struct BrCar *)&g_car, &env, &seed);
    NEAR(TC_F(0x10AC + 0x20), 1.8 + 0.4 - 1.0, 1e-5);
}

static void test_tick(void)
{
    static BrPfxPool pool;
    BrPfxEnv env;
    BrCarFxEnv fx;
    BrPfxTickEnv tick;
    struct BrCar *apCar[1];
    int32_t bInit = 0;
    uint32_t seed = 1u;

    memset(&env, 0, sizeof(env));
    memset(&fx, 0, sizeof(fx));
    memset(&tick, 0, sizeof(tick));
    car_clear();
    memset(&pool, 0, sizeof(pool));

    apCar[0] = (struct BrCar *)&g_car;
    tick.apCar = apCar;
    tick.nCar = 1;
    tick.pbInit = &bInit;
    env.dt = 0.1f;
    fx.dt = 0.1f;

    /* First call initialises the pool exactly once. */
    tick.mode6620 = 1;
    g_nSub9020 = 0;
    BrPfxTick(&pool, &env, &fx, &tick, &seed);
    CHECK(bInit == 1);
    CHECK(pool.iFree == 1);
    CHECK(g_nSub9020 == 1);          /* mode6620 path calls 0x10039020 */

    /* The other two modes never call it. */
    tick.mode6620 = 0;
    tick.mode661C = 0;
    tick.flag6624 = 0;
    g_nSub9020 = 0;
    BrPfxTick(&pool, &env, &fx, &tick, &seed);
    CHECK(g_nSub9020 == 0);

    tick.flag6624 = 1;
    BrPfxTick(&pool, &env, &fx, &tick, &seed);
    CHECK(g_nSub9020 == 0);

    /* Null slots are skipped, not dereferenced. */
    apCar[0] = NULL;
    tick.mode6620 = 1;
    g_nSub9020 = 0;
    BrPfxTick(&pool, &env, &fx, &tick, &seed);
    CHECK(g_nSub9020 == 0);
}

int main(void)
{
    test_vec();
    test_atan2();
    test_mat();
    test_seg();
    test_span();
    test_pool();
    test_spawn();
    test_wheelfx();
    test_tick();

    if (g_nFail != 0) {
        printf("slice2_21: %d FAILURES\n", g_nFail);
        return 1;
    }
    printf("slice2_21: all tests passed\n");
    return 0;
}
