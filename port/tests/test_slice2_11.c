/* test_slice2_11.c -- behavioural tests for work packet 11.
 *
 * Every assertion below is either an invariant of the original (a clamp it
 * really performs, a sentinel it really reserves, an aliasing rule it really
 * relies on) or an algebraic identity that must hold whatever the constants
 * turn out to mean.  Nothing here encodes "what I think the numbers are".
 *
 * ---------------------------------------------------------------------
 * TEST STAND-INS
 * ---------------------------------------------------------------------
 * Everything between the two STAND-IN banners exists only so this test
 * links on its own.  None of it is part of the port: the vector routines
 * are br_vec.c (0x1003AC30-0x1003B060), BrTriContainsPoint is slice1_06
 * (0x1003B940), and the rest are cross-slice callees declared XSLICE in
 * slice2_11.h.  The integration links the real ones.
 */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "slice2_11.h"
#include "slice1_06.h"

/* ==================== STAND-INS BEGIN ============================== */

void BrVec3Cross(BrVec3 *pOut, const BrVec3 *pA, const BrVec3 *pB)
{
    BrVec3 r;
    r.x = pA->y * pB->z - pA->z * pB->y;
    r.y = pA->z * pB->x - pA->x * pB->z;
    r.z = pA->x * pB->y - pA->y * pB->x;
    *pOut = r;
}
float BrVec3Dot(const BrVec3 *pA, const BrVec3 *pB)
{ return pA->x * pB->x + pA->y * pB->y + pA->z * pB->z; }
void BrVec3Sub(BrVec3 *pOut, const BrVec3 *pA, const BrVec3 *pB)
{ pOut->x = pA->x - pB->x; pOut->y = pA->y - pB->y; pOut->z = pA->z - pB->z; }
void BrVec3Add(BrVec3 *pOut, const BrVec3 *pA, const BrVec3 *pB)
{ pOut->x = pA->x + pB->x; pOut->y = pA->y + pB->y; pOut->z = pA->z + pB->z; }
void BrVec3AddTo(BrVec3 *pA, const BrVec3 *pB)
{ pA->x += pB->x; pA->y += pB->y; pA->z += pB->z; }
void BrVec3SubFrom(BrVec3 *pA, const BrVec3 *pB)
{ pA->x -= pB->x; pA->y -= pB->y; pA->z -= pB->z; }
void BrVec3Scale(BrVec3 *pOut, const BrVec3 *pV, float s)
{ pOut->x = pV->x * s; pOut->y = pV->y * s; pOut->z = pV->z * s; }
void BrVec3ScaleBy(BrVec3 *pV, float s)
{ pV->x *= s; pV->y *= s; pV->z *= s; }
void BrVec3Div(BrVec3 *pOut, const BrVec3 *pV, float s)
{ float r = 1.0f / s; pOut->x = pV->x * r; pOut->y = pV->y * r;
  pOut->z = pV->z * r; }
void BrVec3MulAdd(BrVec3 *pOut, const BrVec3 *pA, const BrVec3 *pB, float s)
{ BrVec3 r; r.x = pA->x + pB->x * s; r.y = pA->y + pB->y * s;
  r.z = pA->z + pB->z * s; *pOut = r; }
void BrVec3MulAddTo(BrVec3 *pA, const BrVec3 *pB, float s)
{ pA->x += pB->x * s; pA->y += pB->y * s; pA->z += pB->z * s; }
void BrVec3Lerp(BrVec3 *pOut, const BrVec3 *pA, const BrVec3 *pB, float t)
{ BrVec3 r; r.x = (pA->x - pB->x) * t + pB->x;
  r.y = (pA->y - pB->y) * t + pB->y;
  r.z = (pA->z - pB->z) * t + pB->z; *pOut = r; }
float BrVec3Dist(const BrVec3 *pA, const BrVec3 *pB)
{ BrVec3 d; BrVec3Sub(&d, pA, pB);
  return (float)sqrt((double)(d.x * d.x + d.y * d.y + d.z * d.z)); }
float BrVec3Length(const BrVec3 *pV)
{ float s = pV->x * pV->x + pV->y * pV->y + pV->z * pV->z;
  return (float)sqrt((double)s); }

int BrTriContainsPoint(const BrVec3 *pPt, const BrVec3 *pA, const BrVec3 *pB,
                       const BrVec3 *pC, const BrVec3 *pRef)
{
    const BrVec3 *v[3];
    int i;
    v[0] = pA; v[1] = pB; v[2] = pC;
    for (i = 0; i < 3; ++i) {
        BrVec3 e, w, n;
        const BrVec3 *p0 = v[i], *p1 = v[(i + 1) % 3];
        BrVec3Sub(&e, p1, p0);
        BrVec3Sub(&w, pPt, p0);
        BrVec3Cross(&n, &e, &w);
        if (BrVec3Dot(&n, pRef) < 0.0f)
            return 0;
    }
    return 1;
}

/* --- collision grid ------------------------------------------------ */
static BrCollPlane   s_grid[4 * BR_COLL_CELL_PLANES];
static uint16_t      s_gridCount[4];
BrCollPlane         *g_pBrCollGrid      = s_grid;
const uint16_t      *g_pBrCollGridCount = s_gridCount;
static short         s_forcedCell;
short BrCollGridCellAcquire(float x, float y) { (void)x; (void)y;
                                                return s_forcedCell; }

/* --- mode flags ---------------------------------------------------- */
int   g_brCamCollided;
int   g_brMode0AA010;
int   g_brFlag6909E0;
int   g_brMode0AA8B4;

/* --- lookup tables ------------------------------------------------- */
static uint16_t  s_grid64[64 * 64 + 2];
const uint16_t  *g_pBrGrid64 = s_grid64;
static uint16_t  s_qtab[0x10000];
const uint16_t  *g_pBrU16QueueTable = s_qtab;

/* --- CD audio ------------------------------------------------------ */
int g_brCdEnabled, g_brCdPlaying, g_brCdTrackLast, g_brCdTrackFirst;
int g_brCdTrackCur;
static int s_cdGet, s_cdPlayed, s_cdPlayCount;
int  BrCdTrackGet(void)        { return s_cdGet; }
void BrCdTrackPlay(int track)  { s_cdPlayed = track; ++s_cdPlayCount; }

/* --- network ------------------------------------------------------- */
float      g_brNet220D68;
BrCarState g_brNetLastFull;
int        g_brNetTickCount;
int        g_brNetSendCount;
float      g_abrNetPeak[7];
static int s_fullCount, s_deltaCount, s_flushCount, s_keepCount;
int  BrNetSendFull(BrCarState *p)  { (void)p; ++s_fullCount;  return 10; }
int  BrNetSendDelta(BrCarState *p, BrCarState *r)
                                   { (void)p; (void)r; ++s_deltaCount;
                                     return 20; }
void BrNetSendFlush(void)          { ++s_flushCount; }
void BrNetKeepAliveTick(void)      { ++s_keepCount; }

/* ==================== STAND-INS END ================================ */

#define CHECK(c) do { if (!(c)) { \
    printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); ++g_fails; } } while (0)

static int g_fails;

static int nearf(float a, float b, float eps)
{ float d = a - b; if (d < 0.0f) d = -d; return d <= eps; }

/* A car record is the 0x2B68-byte entity of slice1_09.h. */
#define CAR_BYTES 0x2B68
static unsigned char s_car[CAR_BYTES];

static BrCamFrame *carFrame(size_t off)
{ return (BrCamFrame *)(void *)(s_car + off); }
static BrVec3 *carVec(size_t off)
{ return (BrVec3 *)(void *)(s_car + off); }
static float *carFloat(size_t off)
{ return (float *)(void *)(s_car + off); }
static int *carInt(size_t off)
{ return (int *)(void *)(s_car + off); }

static void carReset(void)
{
    BrCamFrame *pF;
    memset(s_car, 0, sizeof(s_car));
    pF = carFrame(BR_CAR_OFF_FRAME);
    pF->f00.x = 1.0f;              /* forward */
    pF->f10.y = 1.0f;              /* right   */
    pF->f20.z = 1.0f;              /* up      */
    pF->f30.x = 0.0f; pF->f30.y = 0.0f; pF->f30.z = 0.0f;
}

/* ================================================================== */
/* 0x10002E90                                                         */
/* ================================================================== */
static void test_grid64(void)
{
    int i;
    uint32_t r;

    for (i = 0; i < 64 * 64 + 2; ++i)
        s_grid64[i] = (uint16_t)(i * 7 + 3);

    /* Reserved out-of-range answer is 0 on every boundary. */
    CHECK(BrGrid64Fetch(-1, 0) == 0);
    CHECK(BrGrid64Fetch(64, 0) == 0);
    CHECK(BrGrid64Fetch(0, -1) == 0);
    CHECK(BrGrid64Fetch(0, 64) == 0);
    CHECK(BrGrid64Fetch(63, 63) != 0);   /* last in-range cell is valid */

    /* Packing identity: low half is the sample, and low + high (mod 2^16)
     * recovers the NEXT sample.  That is the whole point of the packing. */
    for (i = 0; i < 64 * 64; ++i) {
        int x = i % 64, y = i / 64;
        uint32_t lo, hi;
        r  = BrGrid64Fetch(x, y);
        lo = r & 0xFFFFu;
        hi = r >> 16;
        CHECK(lo == s_grid64[i]);
        CHECK(((lo + hi) & 0xFFFFu) == s_grid64[i + 1]);
    }

    /* Documented over-read: i == 63 samples the first cell of row j+1,
     * it does NOT wrap back to column 0 of the same row. */
    r = BrGrid64Fetch(63, 0);
    CHECK((((r & 0xFFFFu) + (r >> 16)) & 0xFFFFu) == s_grid64[64]);
}

/* ================================================================== */
/* 0x10002F40                                                         */
/* ================================================================== */
static void test_u16queue(void)
{
    uint16_t q[2];
    int i;

    for (i = 0; i < 0x10000; ++i)
        s_qtab[i] = (uint16_t)(0xF00D ^ i);

    /* Empty queue: reserved 0, and no state change at all. */
    q[0] = 1234; q[1] = 0;
    CHECK(BrU16QueuePop(q) == 0);
    CHECK(q[0] == 1234 && q[1] == 0);

    /* Draining returns consecutive table entries then reports empty. */
    q[0] = 100; q[1] = 5;
    for (i = 0; i < 5; ++i) {
        uint16_t v = BrU16QueuePop(q);
        CHECK(v == s_qtab[100 + i]);
        CHECK(q[0] == (uint16_t)(101 + i));
        CHECK(q[1] == (uint16_t)(4 - i));
    }
    CHECK(BrU16QueuePop(q) == 0);
    CHECK(q[0] == 105 && q[1] == 0);

    /* The head/count carry bug: head 0xFFFF wraps to 0 and the carry is
     * OR-ed into the new count.  count 4 -> (4-1)|1 == 3 happens to be
     * unchanged by the OR, so use count 5: (5-1)|1 == 5, i.e. the pop
     * does NOT decrement.  Preserved deliberately. */
    q[0] = 0xFFFF; q[1] = 5;
    CHECK(BrU16QueuePop(q) == s_qtab[0xFFFF]);
    CHECK(q[0] == 0);
    CHECK(q[1] == ((5u - 1u) | 1u));
}

/* ================================================================== */
/* 0x100020D0                                                         */
/* ================================================================== */
static void test_timeformat(void)
{
    char buf[64];

    BrTimeFormat(buf, sizeof(buf), 0.0f);
    CHECK(strcmp(buf, "0:00.00") == 0);

    BrTimeFormat(buf, sizeof(buf), 65.43f);
    CHECK(strcmp(buf, "1:05.43") == 0);

    /* Truncation toward zero (__ftol), not rounding: 1.999 -> 1.99. */
    BrTimeFormat(buf, sizeof(buf), 1.999f);
    CHECK(strcmp(buf, "0:01.99") == 0);

    /* Minutes are not zero-padded, seconds and hundredths are, to two. */
    BrTimeFormat(buf, sizeof(buf), 605.0f);
    CHECK(strcmp(buf, "10:05.00") == 0);
}

/* ================================================================== */
/* 0x10002930 / 0x10002970 / 0x100029B0                               */
/* ================================================================== */
static void test_cdtracks(void)
{
    g_brCdEnabled = 1; g_brCdPlaying = 1;
    g_brCdTrackFirst = 2; g_brCdTrackLast = 9;

    /* All three always report success. */
    s_cdGet = 5; s_cdPlayCount = 0;
    CHECK(BrCdTrackNext() == 1);
    CHECK(g_brCdTrackCur == 6 && s_cdPlayed == 6);
    CHECK(BrCdTrackPrev() == 1);
    CHECK(g_brCdTrackCur == 4 && s_cdPlayed == 4);

    /* Bottom edge: Prev clamps to first. */
    s_cdGet = g_brCdTrackFirst;
    BrCdTrackPrev();
    CHECK(g_brCdTrackCur == g_brCdTrackFirst);
    CHECK(s_cdPlayed == g_brCdTrackFirst);

    /* Top edge: Next CLAMPS, NextWrap WRAPS.  This is the one difference
     * between two otherwise identical routines. */
    s_cdGet = g_brCdTrackLast;
    BrCdTrackNext();
    CHECK(g_brCdTrackCur == g_brCdTrackLast);
    s_cdGet = g_brCdTrackLast;
    BrCdTrackNextWrap();
    CHECK(g_brCdTrackCur == g_brCdTrackFirst);

    /* Guards: either flag clear means nothing is played, still returns 1. */
    s_cdPlayCount = 0;
    g_brCdEnabled = 0; g_brCdPlaying = 1;
    CHECK(BrCdTrackNext() == 1 && BrCdTrackPrev() == 1
          && BrCdTrackNextWrap() == 1);
    g_brCdEnabled = 1; g_brCdPlaying = 0;
    CHECK(BrCdTrackNext() == 1 && BrCdTrackPrev() == 1
          && BrCdTrackNextWrap() == 1);
    CHECK(s_cdPlayCount == 0);
}

/* ================================================================== */
/* 0x10005130                                                         */
/* ================================================================== */
static void test_netsend(void)
{
    BrCarState st;
    float *pAll = (float *)(void *)&st;
    int i, sends;

    memset(&st, 0, sizeof(st));
    memset(&g_brNetLastFull, 0, sizeof(g_brNetLastFull));
    memset(g_abrNetPeak, 0, sizeof(g_abrNetPeak));
    g_brNetTickCount = 0;
    g_brNetSendCount = 0;
    g_brNet220D68 = 0.0f;
    s_fullCount = s_deltaCount = s_flushCount = s_keepCount = 0;

    /* Sentinel path: f78 at/above the reserved value while the reference
     * is below it forces an immediate full packet and refreshes the
     * delta reference. */
    pAll[30] = 4188888.0f;
    pAll[0]  = 42.0f;
    CHECK(BrNetCarStateSend(&st) == 10);
    CHECK(s_fullCount == 1 && s_deltaCount == 0);
    CHECK(((float *)(void *)&g_brNetLastFull)[0] == 42.0f);
    CHECK(g_brNetTickCount == 0);   /* untouched by the fast path */

    /* Once the reference has also crossed, the fast path stops firing. */
    g_brNet220D68 = 4188888.0f;
    pAll[30] = 0.0f;

    /* Two ticks accumulate and send nothing; the third sends. */
    memset(g_abrNetPeak, 0, sizeof(g_abrNetPeak));
    g_brNetTickCount = 0;
    g_brNetSendCount = 0;
    s_fullCount = s_deltaCount = 0;

    for (i = 0; i < 7; ++i) pAll[32 + i] = 1.0f;
    CHECK(BrNetCarStateSend(&st) == 1);
    for (i = 0; i < 7; ++i) pAll[32 + i] = 3.0f;
    CHECK(BrNetCarStateSend(&st) == 1);
    CHECK(s_fullCount == 0 && s_deltaCount == 0);
    /* Peak so far is the running maximum. */
    for (i = 0; i < 7; ++i) CHECK(g_abrNetPeak[i] == 3.0f);

    /* Third tick: the peak is pushed back into the state and the
     * accumulator is cleared. */
    for (i = 0; i < 7; ++i) pAll[32 + i] = 2.0f;
    (void)BrNetCarStateSend(&st);
    for (i = 0; i < 7; ++i) CHECK(pAll[32 + i] == 3.0f);
    for (i = 0; i < 7; ++i) CHECK(g_abrNetPeak[i] == 0.0f);
    CHECK(g_brNetTickCount == 0);

    /* Send cadence: one full packet in every four sends, the other three
     * deltas, each delta followed by exactly one flush and one keepalive. */
    g_brNetSendCount = 0;
    g_brNetTickCount = 0;
    s_fullCount = s_deltaCount = s_flushCount = s_keepCount = 0;
    sends = 0;
    for (i = 0; i < 3 * 8; ++i)
        if (BrNetCarStateSend(&st) != 1)
            ++sends;
    CHECK(sends == 8);
    CHECK(s_fullCount == 2 && s_deltaCount == 6);
    CHECK(s_flushCount == 6 && s_keepCount == 6);
}

/* ================================================================== */
/* 0x10001890 -- orientation invariants                               */
/* ================================================================== */
static void test_camorient(void)
{
    BrCamFrame cam;
    BrVec3 *pAnchor;
    float d;

    carReset();
    pAnchor = carVec(BR_CAR_OFF_ANCHOR);
    pAnchor->x = 3.0f; pAnchor->y = 4.0f; pAnchor->z = 12.0f;

    memset(&cam, 0, sizeof(cam));
    *carInt(BR_CAR_OFF_CAMFLAG) = 0;
    BrCamOrient(s_car, &cam);

    /* f00 is the unit vector camera -> anchor. */
    CHECK(nearf(BrVec3Length(&cam.f00), 1.0f, 1e-5f));
    CHECK(nearf(cam.f00.x * 13.0f, 3.0f, 1e-4f));

    /* The two cross products make an orthogonal set. */
    CHECK(nearf(BrVec3Dot(&cam.f00, &cam.f10), 0.0f, 1e-5f));
    CHECK(nearf(BrVec3Dot(&cam.f00, &cam.f20), 0.0f, 1e-5f));
    CHECK(nearf(BrVec3Dot(&cam.f10, &cam.f20), 0.0f, 1e-5f));

    /* Degenerate case: camera exactly on the anchor keeps the previous
     * forward vector instead of producing NaNs. */
    cam.f30 = *pAnchor;
    cam.f00.x = 0.0f; cam.f00.y = 0.0f; cam.f00.z = 1.0f;
    BrCamOrient(s_car, &cam);
    CHECK(cam.f00.x == 0.0f && cam.f00.y == 0.0f && cam.f00.z == 1.0f);

    /* Doubly degenerate: a zero forward vector is replaced by the car's. */
    cam.f00.x = 0.0f; cam.f00.y = 0.0f; cam.f00.z = 0.0f;
    BrCamOrient(s_car, &cam);
    d = BrVec3Dist(&cam.f00, &carFrame(BR_CAR_OFF_FRAME)->f00);
    CHECK(d == 0.0f);
}

/* ================================================================== */
/* 0x10001760 -- the +-0.1 slew                                       */
/* ================================================================== */
static void test_anchorslew(void)
{
    float *pSlew = carFloat(BR_CAR_OFF_SLEW);
    float  prev;
    int    i;

    carReset();
    *carInt(BR_CAR_OFF_CAMFLAG) = 0;
    g_brMode0AA010 = 0;
    /* speed == 0 -> the target is the low-speed constant. */
    *pSlew = 0.0f;

    prev = *pSlew;
    for (i = 0; i < 100; ++i) {
        BrCamAnchorUpdate(s_car);
        /* Never moves more than the 0.1f step in one call... */
        CHECK(*pSlew - prev <= 0.1f + 1e-5f);
        /* ...and never turns back on itself while climbing. */
        CHECK(*pSlew >= prev);
        prev = *pSlew;
    }
    /* It settles exactly on the target and stops -- no overshoot, no
     * oscillation.  The value it settles on is whatever the low-speed
     * constant is; the invariant is that it is a fixed point. */
    {
        float settled = *pSlew;
        BrCamAnchorUpdate(s_car);
        CHECK(*pSlew == settled);
        CHECK(settled > 0.0f);

        /* Approach from above reaches the same fixed point. */
        *pSlew = settled + 5.0f;
        for (i = 0; i < 100; ++i)
            BrCamAnchorUpdate(s_car);
        CHECK(*pSlew == settled);
    }

    /* The anchor sits above the car by a fixed height plus f00 * slew. */
    {
        BrCamFrame *pF      = carFrame(BR_CAR_OFF_FRAME);
        BrVec3     *pAnchor = carVec(BR_CAR_OFF_ANCHOR);
        float       slew    = *pSlew;
        BrCamAnchorUpdate(s_car);
        CHECK(nearf(pAnchor->x, pF->f30.x + pF->f00.x * slew, 1e-5f));
        CHECK(nearf(pAnchor->y, pF->f30.y + pF->f00.y * slew, 1e-5f));
        CHECK(pAnchor->z > pF->f30.z);   /* the fixed height offset */
    }

    /* g_brMode0AA010 == 5 returns before the slew, so the scalar freezes
     * but the anchor height is still applied. */
    {
        float before;
        g_brMode0AA010 = 5;
        *pSlew = 0.0f;
        BrCamAnchorUpdate(s_car);
        before = *pSlew;
        CHECK(before == 0.0f);
        g_brMode0AA010 = 0;
    }

    /* CAMFLAG != 0 takes the simple path and ignores the slew entirely. */
    {
        BrCamFrame *pF      = carFrame(BR_CAR_OFF_FRAME);
        BrVec3     *pAnchor = carVec(BR_CAR_OFF_ANCHOR);
        *carInt(BR_CAR_OFF_CAMFLAG) = 1;
        *pSlew = 12345.0f;
        BrCamAnchorUpdate(s_car);
        CHECK(*pSlew == 12345.0f);
        CHECK(nearf(pAnchor->x, pF->f30.x + pF->f20.x * 1.1f, 1e-5f));
        CHECK(nearf(pAnchor->z, pF->f30.z + pF->f20.z * 1.1f, 1e-5f));
        *carInt(BR_CAR_OFF_CAMFLAG) = 0;
    }
}

/* ================================================================== */
/* 0x10001970 / 0x10001FF0                                            */
/* ================================================================== */
static void test_frameinit(void)
{
    BrCamFrame *pF, *pB, *pD, *pA;

    carReset();
    pF = carFrame(BR_CAR_OFF_FRAME);
    pA = carFrame(BR_CAR_OFF_CAM_A);
    pB = carFrame(BR_CAR_OFF_CAM_B);
    pD = carFrame(BR_CAR_OFF_CAM_D);

    BrCamFrameInitD(s_car);
    CHECK(BrCarActiveCam(s_car) == pD);
    CHECK(*carInt(BR_CAR_OFF_MODE) == 2);
    CHECK(nearf(pD->f30.x, pF->f30.x + pF->f00.x * 6.0f
                           + pF->f10.x * 2.0f + pF->f20.x, 1e-5f));
    CHECK(nearf(pD->f30.y, pF->f30.y + pF->f00.y * 6.0f
                           + pF->f10.y * 2.0f + pF->f20.y, 1e-5f));
    CHECK(nearf(pD->f30.z, pF->f30.z + pF->f00.z * 6.0f
                           + pF->f10.z * 2.0f + pF->f20.z, 1e-5f));

    /* Frame selection: mode 5 picks B, anything else picks A -- but the
     * frame that is actually SEATED is always B. */
    carReset();
    g_brMode0AA010 = 0;
    g_brFlag6909E0 = 0;
    BrCamFrameInitB(s_car);
    CHECK(BrCarActiveCam(s_car) == pA && BrCarActiveCam2(s_car) == pA);
    CHECK(nearf(pB->f30.x, pF->f30.x + pF->f20.x * 4.0f - pF->f00.x, 1e-5f));
    CHECK(nearf(pB->f30.z, pF->f30.z + pF->f20.z * 4.0f - pF->f00.z, 1e-5f));
    /* Three other slots are seeded from that same position. */
    CHECK(pD->f30.x == pB->f30.x && pD->f30.z == pB->f30.z);
    CHECK(carVec(BR_CAR_OFF_PREVPOS)->x == pB->f30.x);
    CHECK(carVec(BR_CAR_OFF_V2900)->z == pB->f30.z);
    CHECK(*carFloat(BR_CAR_OFF_SHAKE) == 0.0f);
    CHECK(*carFloat(BR_CAR_OFF_SLEW) == 2.0f);

    carReset();
    g_brMode0AA010 = 5;
    BrCamFrameInitB(s_car);
    CHECK(BrCarActiveCam(s_car) == pB && BrCarActiveCam2(s_car) == pB);
    g_brMode0AA010 = 0;
}

/* ================================================================== */
/* 0x100015D0                                                         */
/* ================================================================== */
static void test_placechase(void)
{
    BrCamFrame *pF;
    BrCamFrame  cam;
    float       reach;

    carReset();
    pF = carFrame(BR_CAR_OFF_FRAME);
    g_brFlag6909E0 = 0;
    g_brMode0AA010 = 0;

    /* Simple rig: exact offset from the car, `bias` ignored. */
    *carInt(BR_CAR_OFF_CAMFLAG) = 1;
    g_brMode0AA8B4 = 1;
    memset(&cam, 0, sizeof(cam));
    BrCamPlaceChase(s_car, &cam, 0.0f);
    {
        BrCamFrame cam2;
        memset(&cam2, 0, sizeof(cam2));
        BrCamPlaceChase(s_car, &cam2, 0.75f);
        CHECK(cam2.f30.x == cam.f30.x && cam2.f30.y == cam.f30.y
              && cam2.f30.z == cam.f30.z);
    }
    /* The 0AA8B4 flag really does change the standoff distance. */
    {
        BrCamFrame cam3;
        memset(&cam3, 0, sizeof(cam3));
        g_brMode0AA8B4 = 0;
        BrCamPlaceChase(s_car, &cam3, 0.0f);
        CHECK(cam3.f30.x != cam.f30.x);
        g_brMode0AA8B4 = 1;
    }

    /* Full rig: with bias == 1 the camera lands on the ideal direction,
     * a fixed distance from the car once the deliberate height bump is
     * taken back out.  That distance is the invariant. */
    *carInt(BR_CAR_OFF_CAMFLAG) = 0;
    memset(&cam, 0, sizeof(cam));
    cam.f30.x = -1.0f;                       /* somewhere behind */
    BrCamPlaceChase(s_car, &cam, 1.0f);
    {
        BrVec3 d;
        BrVec3Sub(&d, &cam.f30, &pF->f30);
        d.z -= 2.4000000953674316f;          /* the height bump */
        reach = BrVec3Length(&d);
        CHECK(nearf(reach, 11.0f, 1e-3f));
    }

    /* bias == 0 keeps the CURRENT direction, renormalised to that same
     * standoff -- so the reach is unchanged but the direction is not. */
    memset(&cam, 0, sizeof(cam));
    cam.f30.y = -3.0f;
    BrCamPlaceChase(s_car, &cam, 0.0f);
    {
        BrVec3 d;
        BrVec3Sub(&d, &cam.f30, &pF->f30);
        d.z -= 2.4000000953674316f;
        CHECK(nearf(BrVec3Length(&d), reach, 1e-3f));
        CHECK(d.y < 0.0f);                   /* still behind in y */
    }
}

/* ================================================================== */
/* 0x100011F0                                                         */
/* ================================================================== */
static void collSetTriangle(float z)
{
    static BrVec3 v0, v1, v2;
    BrCollPlane *pl = &s_grid[0];

    v0.x = -100.0f; v0.y = -100.0f; v0.z = z;
    v1.x =  100.0f; v1.y = -100.0f; v1.z = z;
    v2.x =    0.0f; v2.y =  100.0f; v2.z = z;

    memset(pl, 0, sizeof(*pl));
    pl->nx = 0.0f; pl->ny = 0.0f; pl->nz = 1.0f;
    pl->d  = -z;
    pl->pV0 = &v0; pl->pV1 = &v1; pl->pV2 = &v2;
    s_gridCount[0] = 1;
    s_forcedCell   = 0;
}

static void test_collidesweep(void)
{
    BrCamFrame cam;
    BrVec3     anchor, prev;

    carReset();
    collSetTriangle(0.0f);

    /* Clean miss: the segment never reaches the plane.  Nothing moves and
     * the flag reports "no collision". */
    memset(&cam, 0, sizeof(cam));
    *carVec(BR_CAR_OFF_ANCHOR) = (BrVec3){0.0f, 0.0f, 5.0f};
    cam.f30.z = 1.0f;
    prev = cam.f30;
    g_brCamCollided = 1;
    BrCamCollideSweep(s_car, &cam, &prev);
    CHECK(g_brCamCollided == 0);
    CHECK(cam.f30.z == 1.0f);

    /* Back-facing plane: the same geometry approached from below is
     * ignored, because dot(dir, n) < 0 is required. */
    memset(&cam, 0, sizeof(cam));
    *carVec(BR_CAR_OFF_ANCHOR) = (BrVec3){0.0f, 0.0f, -5.0f};
    cam.f30.z = 5.0f;
    prev = cam.f30;
    BrCamCollideSweep(s_car, &cam, &prev);
    CHECK(g_brCamCollided == 0);
    CHECK(cam.f30.z == 5.0f);

    /* Real hit: camera ends up in front of the plane, offset along the
     * normal, and the flag is raised.  BOTH passes must hit -- pass 2
     * starts at the previous camera position, so that has to be on the
     * front side too. */
    memset(&cam, 0, sizeof(cam));
    anchor = (BrVec3){0.0f, 0.0f, 5.0f};
    *carVec(BR_CAR_OFF_ANCHOR) = anchor;
    cam.f30.z = -5.0f;
    prev = anchor;
    BrCamCollideSweep(s_car, &cam, &prev);
    CHECK(g_brCamCollided == 1);
    CHECK(cam.f30.z > 0.0f);            /* pushed out to the front side */
    CHECK(nearf(cam.f30.z, 0.3f, 1e-5f));
    CHECK(cam.f30.x == 0.0f && cam.f30.y == 0.0f);

    /* Distance clamp: when the push-out would leave the camera further
     * from the anchor than it started, it is pulled back to exactly the
     * original distance.  That equality is the invariant. */
    memset(&cam, 0, sizeof(cam));
    anchor = (BrVec3){0.0f, 0.0f, 0.05f};
    *carVec(BR_CAR_OFF_ANCHOR) = anchor;
    cam.f30.z = -0.05f;
    prev = anchor;
    {
        float before = BrVec3Dist(&cam.f30, &anchor);
        BrCamCollideSweep(s_car, &cam, &prev);
        CHECK(g_brCamCollided == 1);
        CHECK(nearf(BrVec3Dist(&cam.f30, &anchor), before, 1e-5f));
    }

    /* An empty cell is a no-op even with the geometry still loaded. */
    s_gridCount[0] = 0;
    memset(&cam, 0, sizeof(cam));
    *carVec(BR_CAR_OFF_ANCHOR) = (BrVec3){0.0f, 0.0f, 5.0f};
    cam.f30.z = -5.0f;
    prev = cam.f30;
    BrCamCollideSweep(s_car, &cam, &prev);
    CHECK(g_brCamCollided == 0);
    CHECK(cam.f30.z == -5.0f);
}

int main(void)
{
    test_grid64();
    test_u16queue();
    test_timeformat();
    test_cdtracks();
    test_netsend();
    test_camorient();
    test_anchorslew();
    test_frameinit();
    test_placechase();
    test_collidesweep();

    if (g_fails == 0) {
        printf("slice2_11: all tests passed\n");
        return 0;
    }
    printf("slice2_11: %d failure(s)\n", g_fails);
    return 1;
}
