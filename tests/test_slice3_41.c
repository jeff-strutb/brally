/* test_slice3_41.c -- behaviour tests for slice3_41.c.
 *
 * These assert invariants of the ORIGINAL code (clamps, sentinels, the
 * asymmetries called out in slice3_41.h), not just "the function returns
 * what I typed".  Where a numeric value is asserted exactly it is one the
 * disassembly pins down (0.8 at dead centre, 1/(1+v/343) for the Doppler,
 * 1024/dist for the volume).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "slice3_41.h"

static int g_fails;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);         \
            g_fails++;                                                     \
        }                                                                  \
    } while (0)

#define CHECK_NEAR(a, b, eps)                                              \
    do {                                                                   \
        double _a = (double)(a), _b = (double)(b);                         \
        if (!(fabs(_a - _b) <= (eps))) {                                   \
            printf("FAIL %s:%d  %s (%.9g) != %s (%.9g)\n",                 \
                   __FILE__, __LINE__, #a, _a, #b, _b);                    \
            g_fails++;                                                     \
        }                                                                  \
    } while (0)

/* =====================================================================
 * Cross-slice stand-ins.  TEST-ONLY -- the real ones live elsewhere.
 * ===================================================================== */

float   g_f6C2CFC   = 1.0f;     /* 0x106C2CFC, spelled g_BrAnimDt here   */
int32_t g_Br0B380C  = 0;        /* 0x100B380C, owned by slice2_19        */

static int  g_nFatal;
static char g_szFatal[128];

/* XSLICE 0x10035BBA -- stand-in; the real one does not return. */
void BrFatal(const char *pszMsg)
{
    g_nFatal++;
    snprintf(g_szFatal, sizeof g_szFatal, "%s", pszMsg);
}

/* br_vec.h stand-ins (br_vec.c is not linked into this test binary). */
void BrVec3Sub(BrVec3 *pOut, const BrVec3 *pA, const BrVec3 *pB)
{
    pOut->x = pA->x - pB->x;
    pOut->y = pA->y - pB->y;
    pOut->z = pA->z - pB->z;
}

float BrVec3Dot(const BrVec3 *pA, const BrVec3 *pB)
{
    return pA->x * pB->x + pA->y * pB->y + pA->z * pB->z;
}

void BrVec3DivBy(BrVec3 *pV, float s)
{
    float r = 1.0f / s;
    pV->x *= r;
    pV->y *= r;
    pV->z *= r;
}

float BrVec3Length(const BrVec3 *pV)
{
    float ss = pV->x * pV->x + pV->y * pV->y + pV->z * pV->z;
    return (float)sqrt((double)ss);
}

float BrVec3Dist(const BrVec3 *pA, const BrVec3 *pB)
{
    BrVec3 d;
    BrVec3Sub(&d, pA, pB);
    return BrVec3Length(&d);
}

/* =====================================================================
 * 1.  BrRankCmpKey / BrRankAssign
 * ===================================================================== */

static void test_cmp(void)
{
    float a, b;
    float nan = (float)strtod("NAN", NULL);

    a = 2.0f; b = 1.0f;  CHECK(BrRankCmpKey(&a, &b) ==  1);
    a = 1.0f; b = 2.0f;  CHECK(BrRankCmpKey(&a, &b) == -1);
    a = 1.0f; b = 1.0f;  CHECK(BrRankCmpKey(&a, &b) ==  0);

    /* Antisymmetry on ordered pairs. */
    a = -3.5f; b = 7.25f;
    CHECK(BrRankCmpKey(&a, &b) == -BrRankCmpKey(&b, &a));

    /* Negative zero compares equal to zero, as fcomp does. */
    a = 0.0f; b = -0.0f; CHECK(BrRankCmpKey(&a, &b) == 0);

    /* GOTCHA under test: the original reads C0|C3 first and C0 alone
     * second, so an unordered pair reports "less", never "equal" -- and it
     * does so in BOTH argument orders, which is not a valid ordering. */
    CHECK(BrRankCmpKey(&nan, &b) == -1);
    CHECK(BrRankCmpKey(&b, &nan) == -1);
}

static void drivers_init(BrDriver *pS, BrDriverCar *pC, int32_t n)
{
    int32_t i;
    memset(pS, 0, sizeof(BrDriver) * (size_t)n);
    memset(pC, 0, sizeof(BrDriverCar) * (size_t)n);
    for (i = 0; i < n; i++) {
        pS[i].f64  = i;
        pS[i].pCar = &pC[i];
        pC[i].fFF8 = -999;
        pS[i].f54  = -999;
    }
}

static void test_rank(void)
{
    enum { N = 5 };
    BrDriver    s[N];
    BrDriverCar c[N];
    int32_t     i, j, seen;

    /* --- distinct keys: ranks are a permutation of 0..N-1 and are strictly
     *     decreasing in the key (biggest key wins, rank 0). --- */
    drivers_init(s, c, N);
    c[0].fFF4 = 10.0f;
    c[1].fFF4 = 40.0f;
    c[2].fFF4 = 20.0f;
    c[3].fFF4 = 50.0f;
    c[4].fFF4 = 30.0f;
    BrRankAssign(s, N);

    for (i = 0; i < N; i++) {
        seen = 0;
        for (j = 0; j < N; j++)
            if (c[j].fFF8 == i)
                seen++;
        CHECK(seen == 1);           /* a permutation */
    }
    for (i = 0; i < N; i++)
        for (j = 0; j < N; j++)
            if (c[i].fFF4 > c[j].fFF4)
                CHECK(c[i].fFF8 < c[j].fFF8);

    CHECK(c[3].fFF8 == 0);          /* highest key -> rank 0   */
    CHECK(c[0].fFF8 == N - 1);      /* lowest  key -> rank N-1 */

    /* --- idempotent: a second pass over the same state changes nothing. --- */
    {
        int32_t before[N];
        for (i = 0; i < N; i++)
            before[i] = c[i].fFF8;
        BrRankAssign(s, N);
        for (i = 0; i < N; i++)
            CHECK(c[i].fFF8 == before[i]);
    }

    /* --- skipped slots keep their old rank AND shift everyone else: the
     *     count-down starts from n, not from the number of participants. --- */
    drivers_init(s, c, N);
    for (i = 0; i < N; i++)
        c[i].fFF4 = (float)i;
    s[0].f68 |= BR_DRIVER_SKIP;
    s[1].f68 |= BR_DRIVER_SKIP;
    BrRankAssign(s, N);

    CHECK(c[0].fFF8 == -999);       /* untouched */
    CHECK(c[1].fFF8 == -999);
    CHECK(c[4].fFF8 == 2);          /* leader is rank 2, not rank 0 */
    CHECK(c[3].fFF8 == 3);
    CHECK(c[2].fFF8 == 4);

    /* --- a slot with no car writes its own f54 instead. --- */
    drivers_init(s, c, N);
    for (i = 0; i < N; i++)
        c[i].fFF4 = (float)i;
    s[2].pCar = NULL;
    s[2].f50  = 100.0f;             /* highest key of all */
    BrRankAssign(s, N);
    CHECK(s[2].f54  == 0);
    CHECK(c[2].fFF8 == -999);       /* the car record stayed untouched */

    /* --- n == 0 must not touch anything. --- */
    drivers_init(s, c, N);
    BrRankAssign(s, 0);
    for (i = 0; i < N; i++)
        CHECK(c[i].fFF8 == -999);
}

/* =====================================================================
 * 2.  BrVarSave / BrVarLoad
 * ===================================================================== */

static void test_var(void)
{
    uint8_t  a[7], b[4], cbuf[13];
    uint8_t  save[64];
    uint32_t i, total;
    BrVarBlock tbl[4];

    tbl[0].pData = a;    tbl[0].cb = sizeof a;
    tbl[1].pData = b;    tbl[1].cb = sizeof b;
    tbl[2].pData = cbuf; tbl[2].cb = sizeof cbuf;
    tbl[3].pData = NULL; tbl[3].cb = 0;
    total = (uint32_t)(sizeof a + sizeof b + sizeof cbuf);

    for (i = 0; i < sizeof a;    i++) a[i]    = (uint8_t)(0x10 + i);
    for (i = 0; i < sizeof b;    i++) b[i]    = (uint8_t)(0x50 + i);
    for (i = 0; i < sizeof cbuf; i++) cbuf[i] = (uint8_t)(0x90 + i);

    /* --- round trip: save, scribble, load, everything is back. --- */
    memset(save, 0xCC, sizeof save);
    g_nFatal = 0;
    BrVarSave(tbl, save, (int32_t)sizeof save);
    CHECK(g_nFatal == 0);

    /* The blocks are concatenated in table order with no padding. */
    CHECK(memcmp(save, a, sizeof a) == 0);
    CHECK(memcmp(save + sizeof a, b, sizeof b) == 0);
    CHECK(memcmp(save + sizeof a + sizeof b, cbuf, sizeof cbuf) == 0);
    CHECK(save[total] == 0xCC);     /* nothing written past `used` */

    memset(a, 0, sizeof a);
    memset(b, 0, sizeof b);
    memset(cbuf, 0, sizeof cbuf);
    BrVarLoad(tbl, save);
    for (i = 0; i < sizeof a;    i++) CHECK(a[i]    == (uint8_t)(0x10 + i));
    for (i = 0; i < sizeof b;    i++) CHECK(b[i]    == (uint8_t)(0x50 + i));
    for (i = 0; i < sizeof cbuf; i++) CHECK(cbuf[i] == (uint8_t)(0x90 + i));

    /* --- an empty table is a no-op and reports zero bytes used. --- */
    g_nFatal = 0;
    BrVarSave(&tbl[3], save, 0);
    CHECK(g_nFatal == 0);

    /* --- the budget check is `used > avail`, so exactly-full is fine ... */
    g_nFatal = 0;
    BrVarSave(tbl, save, (int32_t)total);
    CHECK(g_nFatal == 0);

    /* ... and one byte short trips it, reporting avail then used. */
    g_nFatal = 0;
    BrVarSave(tbl, save, (int32_t)total - 1);
    CHECK(g_nFatal == 1);
    {
        char szWant[80];
        snprintf(szWant, sizeof szWant,
                 "VAR SAVE OVERFLOW (%d avail, %d used)",
                 (int)total - 1, (int)total);
        CHECK(strcmp(g_szFatal, szWant) == 0);
    }
}

/* =====================================================================
 * 3.  BrSndDoppler
 * ===================================================================== */

static void test_doppler(void)
{
    BrVec3 sp, spv, lp, lpv;
    float  r;

    g_BrAnimDt = 1.0f;

    /* --- nothing moving: exactly 1.0, no drift. --- */
    sp.x = 10.0f; sp.y = 0.0f; sp.z = 0.0f;  spv = sp;
    lp.x =  0.0f; lp.y = 0.0f; lp.z = 0.0f;  lpv = lp;
    CHECK(BrSndDoppler(&sp, &spv, &lp, &lpv) == 1.0f);

    /* --- source receding at exactly c: 1 / (1 + 343/343) = 0.5. --- */
    spv.x = sp.x - 343.0f;
    r = BrSndDoppler(&sp, &spv, &lp, &lpv);
    CHECK_NEAR(r, 0.5, 1e-6);

    /* --- source approaching raises the pitch: 1 / (1 - 34.3/343). --- */
    spv.x = sp.x + 34.3f;
    r = BrSndDoppler(&sp, &spv, &lp, &lpv);
    CHECK(r > 1.0f);
    CHECK_NEAR(r, 1.0 / (1.0 - 0.1), 1e-5);

    /* --- GOTCHA under test: there is no guard on the denominator, so a
     *     source closing faster than c drives it through zero and the
     *     "frequency ratio" comes back NEGATIVE.  Preserved from the
     *     original, which has no clamp anywhere in this function. --- */
    spv.x = sp.x + 400.0f;
    CHECK(BrSndDoppler(&sp, &spv, &lp, &lpv) < 0.0f);

    /* --- listener closing on a still source raises the pitch; the
     *     numerator carries the listener term, so it is 1 + v/c. --- */
    spv = sp;
    lpv.x = lp.x - 34.3f;                    /* listener moved +34.3 toward */
    r = BrSndDoppler(&sp, &spv, &lp, &lpv);
    CHECK_NEAR(r, 1.0 + 34.3 / 343.0, 1e-5);

    /* --- motion perpendicular to the line of sight has no effect. --- */
    lpv = lp; lpv.y = lp.y - 100.0f;
    spv = sp; spv.y = sp.y + 100.0f;
    CHECK_NEAR(BrSndDoppler(&sp, &spv, &lp, &lpv), 1.0, 1e-6);

    /* --- coincident source and listener: |u| == 0, the normalise is
     *     SKIPPED, u stays zero, both dot products vanish -> 1.0 and no
     *     division by zero.  This is the guard the original actually has. --- */
    sp = lp;
    spv.x = 500.0f; spv.y = 0.0f; spv.z = 0.0f;
    lpv = lp;
    r = BrSndDoppler(&sp, &spv, &lp, &lpv);
    CHECK(r == 1.0f);

    /* --- dt scales the velocities, so halving dt doubles the shift. --- */
    sp.x = 10.0f; sp.y = 0.0f; sp.z = 0.0f;
    spv = sp; spv.x = sp.x - 34.3f;
    lp.x = 0.0f; lp.y = 0.0f; lp.z = 0.0f; lpv = lp;
    g_BrAnimDt = 1.0f;
    r = BrSndDoppler(&sp, &spv, &lp, &lpv);
    CHECK_NEAR(r, 1.0 / 1.1, 1e-6);
    g_BrAnimDt = 0.5f;
    r = BrSndDoppler(&sp, &spv, &lp, &lpv);
    CHECK_NEAR(r, 1.0 / 1.2, 1e-6);
    g_BrAnimDt = 1.0f;
}

/* =====================================================================
 * 4.  BrSndPan
 * ===================================================================== */

/* Listener matrix: rows 0..2 the basis, row 3 the position.  Row 1 is the
 * pan axis. */
static void listener_at(BrMat4 *pM, float px, float py, float pz)
{
    memset(pM, 0, sizeof *pM);
    pM->m[0][0] = 1.0f;
    pM->m[1][1] = 1.0f;         /* pan axis = world +Y */
    pM->m[2][2] = 1.0f;
    pM->m[3][0] = px;
    pM->m[3][1] = py;
    pM->m[3][2] = pz;
}

static void pan_at(float projWanted, float dist, int32_t fNarrow,
                   float *pA, float *pB, int32_t *pVol)
{
    BrMat4 lis;
    BrVec3 src;
    listener_at(&lis, 0.0f, 0.0f, 0.0f);
    /* +Y carries the projection, +X carries the rest of the distance. */
    src.x = dist;
    src.y = projWanted;
    src.z = 0.0f;
    BrSndPan(&src, &lis, pA, pB, pVol, fNarrow);
}

static void test_pan(void)
{
    float   a, b, a2, b2;
    int32_t v;
    int     i;

    /* --- dead centre: both channels 0.8, and the two half-laws meet. --- */
    pan_at(0.0f, 100.0f, 0, &a, &b, &v);
    CHECK_NEAR(a, 0.8, 1e-6);
    CHECK_NEAR(b, 0.8, 1e-6);

    /* --- hard over on each side: 1.0 / 0.0. --- */
    pan_at(10.0f, 100.0f, 0, &a, &b, &v);
    CHECK_NEAR(a, 1.0, 1e-6);
    CHECK_NEAR(b, 0.0, 1e-6);
    pan_at(-10.0f, 100.0f, 0, &a, &b, &v);
    CHECK_NEAR(a, 0.0, 1e-6);
    CHECK_NEAR(b, 1.0, 1e-6);

    /* --- the clamp really is a clamp: past +-10 nothing more happens. --- */
    pan_at(10.0f,   100.0f, 0, &a,  &b,  &v);
    pan_at(1.0e6f,  100.0f, 0, &a2, &b2, &v);
    CHECK(a == a2 && b == b2);
    pan_at(-10.0f,  100.0f, 0, &a,  &b,  &v);
    pan_at(-1.0e6f, 100.0f, 0, &a2, &b2, &v);
    CHECK(a == a2 && b == b2);

    /* --- channel A rises and B falls monotonically across the range. --- */
    {
        float prevA = -1.0f, prevB = 2.0f;
        for (i = -10; i <= 10; i++) {
            pan_at((float)i, 100.0f, 0, &a, &b, &v);
            CHECK(a >= prevA);
            CHECK(b <= prevB);
            CHECK(a >= 0.0f && a <= 1.0f);
            CHECK(b >= 0.0f && b <= 1.0f);
            prevA = a;
            prevB = b;
        }
    }

    /* --- the narrow flag scales the projection by 0.4 BEFORE the +10
     *     bias, so a hard-over source only reaches p = 0.7. --- */
    pan_at(10.0f, 100.0f, 1, &a, &b, &v);
    pan_at(4.0f,  100.0f, 0, &a2, &b2, &v);
    CHECK(a == a2 && b == b2);
    CHECK(a < 1.0f);            /* never reaches the rail */

    /* --- the 0.49..0.51 snap: a projection just off centre lands on
     *     exactly the same pair as dead centre. --- */
    pan_at(0.0f,  100.0f, 0, &a,  &b,  &v);
    pan_at(0.15f, 100.0f, 0, &a2, &b2, &v);   /* p = 0.5075, inside */
    CHECK(a == a2 && b == b2);
    pan_at(0.3f,  100.0f, 0, &a2, &b2, &v);   /* p = 0.515, outside */
    CHECK(a2 != a);

    /* --- volume is trunc(1024 / dist) with a hard 32-unit floor. --- */
    pan_at(0.0f, 1024.0f, 0, &a, &b, &v);  CHECK(v == 1);
    pan_at(0.0f,   64.0f, 0, &a, &b, &v);  CHECK(v == 16);
    pan_at(0.0f,   32.0f, 0, &a, &b, &v);  CHECK(v == 32);
    pan_at(0.0f,    1.0f, 0, &a, &b, &v);  CHECK(v == 32);   /* clamped */
    pan_at(0.0f,    0.0f, 0, &a, &b, &v);  CHECK(v == 32);   /* clamped */
    pan_at(0.0f,  100.0f, 0, &a, &b, &v);  CHECK(v == 10);   /* truncates */

    /* --- the projection is measured along row 1 only; moving the source
     *     along row 0 or row 2 must not pan it. --- */
    {
        BrMat4 lis;
        BrVec3 src;
        listener_at(&lis, 3.0f, -4.0f, 5.0f);
        src.x = 3.0f + 50.0f;   /* pure row-0 offset from the listener */
        src.y = -4.0f;
        src.z = 5.0f + 50.0f;   /* pure row-2 offset */
        BrSndPan(&src, &lis, &a, &b, &v, 0);
        CHECK_NEAR(a, 0.8, 1e-6);
        CHECK_NEAR(b, 0.8, 1e-6);
    }
}

/* =====================================================================
 * 5.  Nearest-source tracker
 * ===================================================================== */

static void test_nearest(void)
{
    BrMat4 lis;
    BrVec3 near_, far_;

    listener_at(&lis, 0.0f, 0.0f, 0.0f);
    near_.x = 1.0f;  near_.y = 0.0f; near_.z = 0.0f;
    far_.x  = 50.0f; far_.y  = 0.0f; far_.z  = 0.0f;

    BrSndNearestReset();
    CHECK(g_BrSndAA3470 == -1);
    CHECK(g_BrSndNearest.metric == BR_SND_NEAREST_FAR);
    CHECK(g_BrSndNearest.f84 == -1 && g_BrSndNearest.f88 == -1);
    CHECK(g_BrSndNearest.f8C == -1 && g_BrSndNearest.f90 == -1);
    CHECK(g_BrSndNearest.pObj == NULL && g_BrSndNearest.pObjPrev == NULL);

    /* --- the far one is offered first and still loses. --- */
    BrSndNearestOffer(7, 3, 0x80, 440.0f, &far_,  &lis);
    CHECK(g_BrSndNearest.f8C == 7);
    CHECK_NEAR(g_BrSndNearest.metric, 50.0, 1e-6);

    BrSndNearestOffer(9, 4, 0x180, 880.0f, &near_, &lis);
    CHECK(g_BrSndNearest.f8C == 9);
    CHECK(g_BrSndNearest.f84 == 4);
    CHECK(g_BrSndNearest.f9C == 0x180);
    CHECK(g_BrSndNearest.f98 == 880.0f);
    CHECK(g_BrSndNearest.pos.x == 1.0f);
    CHECK_NEAR(g_BrSndNearest.metric, 1.0, 1e-6);

    /* --- re-offering the far one afterwards is rejected ... --- */
    BrSndNearestOffer(7, 3, 0x80, 440.0f, &far_, &lis);
    CHECK(g_BrSndNearest.f8C == 9);

    /* --- ... and so is an exact tie: the test is strictly nearer. --- */
    BrSndNearestOffer(11, 5, 0x80, 440.0f, &near_, &lis);
    CHECK(g_BrSndNearest.f8C == 9);

    /* --- the "committed" halves are never written by an offer. --- */
    CHECK(g_BrSndNearest.f88 == -1);
    CHECK(g_BrSndNearest.f90 == -1);
    CHECK(g_BrSndNearest.pObjPrev == NULL);

    /* --- GOTCHA under test: Invalidate clears the candidate fields and the
     *     metric but deliberately leaves the committed ones and f98. --- */
    g_BrSndNearest.f88 = 42;
    g_BrSndNearest.f90 = 43;
    BrSndNearestInvalidate();
    CHECK(g_BrSndNearest.metric == BR_SND_NEAREST_FAR);
    CHECK(g_BrSndNearest.f84 == -1 && g_BrSndNearest.f8C == -1);
    CHECK(g_BrSndNearest.f88 == 42);
    CHECK(g_BrSndNearest.f90 == 43);
    CHECK(g_BrSndNearest.f98 == 880.0f);
    CHECK(g_BrSndNearest.pObj == &lis);     /* also left in place */

    /* --- after Invalidate the far source can win again. --- */
    BrSndNearestOffer(7, 3, 0x80, 440.0f, &far_, &lis);
    CHECK(g_BrSndNearest.f8C == 7);

    /* --- GOTCHA under test: the full Reset still does not clear f98. --- */
    g_BrSndNearest.f98 = 1234.0f;
    BrSndNearestReset();
    CHECK(g_BrSndNearest.f98 == 1234.0f);
    CHECK(g_BrSndNearest.f88 == -1 && g_BrSndNearest.f90 == -1);
    CHECK(g_BrSndNearest.f9C == 0 && g_BrSndNearest.fA0 == 0);

    /* --- OfferDefault does nothing unless the mode selector is 4 or 10. --- */
    BrSndNearestReset();
    g_Br0B380C = 0;
    BrSndNearestOfferDefault(5, &near_, &lis);
    CHECK(g_BrSndNearest.f8C == -1);
    CHECK(g_BrSndNearest.metric == BR_SND_NEAREST_FAR);

    g_Br0B380C = 4;
    BrSndNearestOfferDefault(5, &near_, &lis);
    CHECK(g_BrSndNearest.f8C == 5);
    CHECK(g_BrSndNearest.f84 == 0x0F);
    CHECK(g_BrSndNearest.f9C == 0x180);
    CHECK(g_BrSndNearest.f98 == 11000.0f);

    BrSndNearestReset();
    g_Br0B380C = 10;
    BrSndNearestOfferDefault(6, &near_, &lis);
    CHECK(g_BrSndNearest.f8C == 6);
    g_Br0B380C = 0;
}

/* =====================================================================
 * 6.  Per-frame slot banks
 * ===================================================================== */

static void test_banks(void)
{
    static uint8_t buf16[2 * 21 * 16];
    static uint8_t buf32[2 * 21 * 32];
    BrPool         pool64;
    uint8_t        pool64mem[1];
    uint8_t       *p, *pFirst, *pOvf;
    int            i;

    g_BrPool16.pBase = buf16;
    g_BrPool32.pBase = buf32;
    g_BrPool16.frame = 0;
    g_BrPool32.frame = 0;
    BrGfx69580();

    /* --- 20 usable slots, contiguous, cbSlot apart. --- */
    pFirst = (uint8_t *)BrPool16Alloc();
    CHECK(pFirst == buf16);
    for (i = 1; i < 20; i++) {
        p = (uint8_t *)BrPool16Alloc();
        CHECK(p == pFirst + (ptrdiff_t)i * 16);
    }
    CHECK(g_BrPool16.count == 20);

    /* --- past the limit every request aliases the SAME overflow slot,
     *     which is index 20 of the very same bank, and the counter keeps
     *     climbing.  Never NULL. --- */
    pOvf = (uint8_t *)BrPool16Alloc();
    CHECK(pOvf == pFirst + 20 * 16);
    for (i = 0; i < 5; i++)
        CHECK((uint8_t *)BrPool16Alloc() == pOvf);
    CHECK(g_BrPool16.count == 26);

    /* --- frame 1 uses the next bank of 21 slots, not 20. --- */
    g_BrPool16.count = 0;
    g_BrPool16.frame = 1;
    CHECK((uint8_t *)BrPool16Alloc() == buf16 + 21 * 16);

    /* --- the 32-byte pool has the same shape, different stride. --- */
    g_BrPool32.frame = 1;
    g_BrPool32.count = 0;
    CHECK((uint8_t *)BrPool32Alloc() == buf32 + 21 * 32);
    g_BrPool32.frame = 0;
    g_BrPool32.count = 0;
    pFirst = (uint8_t *)BrPool32Alloc();
    CHECK(pFirst == buf32);
    for (i = 1; i < 25; i++)
        (void)BrPool32Alloc();
    CHECK(g_BrPool32.count == 25);
    CHECK((uint8_t *)BrPool32Alloc() == buf32 + 20 * 32);

    /* --- BrGfx69580 zeroes all three counters and leaves the frame
     *     index alone (something else advances 0x106C65EC). --- */
    pool64.pBase = pool64mem;
    pool64.frame = 0;
    pool64.count = 99;
    g_pBrPool64  = &pool64;
    g_BrPool16.frame = 1;
    g_BrPool32.frame = 1;
    BrGfx69580();
    CHECK(g_BrPool16.count == 0);
    CHECK(g_BrPool32.count == 0);
    CHECK(pool64.count == 0);
    CHECK(g_BrPool16.frame == 1);
    CHECK(g_BrPool32.frame == 1);

    /* --- an unwired 64-byte hook must not crash. --- */
    g_pBrPool64 = NULL;
    BrGfx69580();
    CHECK(g_BrPool16.count == 0);
}

int main(void)
{
    test_cmp();
    test_rank();
    test_var();
    test_doppler();
    test_pan();
    test_nearest();
    test_banks();

    if (g_fails == 0)
        printf("test_slice3_41: all checks passed\n");
    else
        printf("test_slice3_41: %d FAILURES\n", g_fails);
    return g_fails != 0;
}
