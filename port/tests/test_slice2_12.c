/* test_slice2_12.c -- behaviour tests for slice2_12.c.
 *
 * Everything between the STAND-IN banners is NOT part of the port. Those
 * symbols belong to other slices (slice1_02, slice1_09) or are XSLICE
 * dependencies; they are reproduced here only so this file links and so the
 * round-trip properties can be checked. The reproductions follow the
 * behaviour those slices' headers document, nothing more.
 */

#include "slice2_12.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_cFail;

#define CHECK(cond)                                                         \
    do {                                                                    \
        if (!(cond)) {                                                      \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);          \
            ++g_cFail;                                                      \
        }                                                                   \
    } while (0)

#define CHECK_NEAR(a, b, tol)                                               \
    do {                                                                    \
        double va_ = (double)(a), vb_ = (double)(b);                        \
        if (!(fabs(va_ - vb_) <= (double)(tol))) {                          \
            printf("FAIL %s:%d  %s (%g) !~ %s (%g)\n",                      \
                   __FILE__, __LINE__, #a, va_, #b, vb_);                   \
            ++g_cFail;                                                      \
        }                                                                   \
    } while (0)

/* ===================================================================== */
/* STAND-INS begin -- other slices' code, reproduced from their headers   */
/* ===================================================================== */

/* --- slice1_02: the three quantisers a later pass owns ------------------- */

int32_t BrFixPackU24Q13(float v)
{
    double d = floor(0.5 + 8192.0 * (double)v);
    if (d < 0.0)          d = 0.0;          /* clamped as a float */
    if (d > 16777215.0)   d = 16777215.0;
    return (int32_t)d;
}

int32_t BrFixPackS24Q1(float v)
{
    double d = floor(0.5 + 2.0 * (double)v);
    int32_t n = (int32_t)d;
    if (n < -0x800000) n = -0x800000;       /* clamped as an int */
    if (n >  0x7FFFFF) n =  0x7FFFFF;
    return n;
}

int32_t BrFixPackS16Q7(float v)
{
    double d = floor(0.5 + 128.0 * (double)v);
    int32_t n = (int32_t)d;
    if (n < -32768) n = -32768;
    if (n >  32767) n =  32767;
    return n;
}

/* --- slice1_02: the matching dequantisers ---------------------------- */

static int32_t SExt(int32_t v, int bits)
{
    int32_t m = (int32_t)1 << (bits - 1);
    v &= (int32_t)(((uint32_t)1 << bits) - 1u);
    return (v ^ m) - m;
}

float BrFixUnpackS6Q7Neg(int32_t v)   { return (float)SExt(v, 6)  * -0.0078125f; }
float BrFixUnpackS16Q15Neg(int32_t v) { return (float)SExt(v, 16) * -3.0517578125e-05f; }
float BrFixUnpackS16Q7(int32_t v)     { return (float)SExt(v, 16) *  0.0078125f; }
float BrFixUnpackS16Q8(int32_t v)     { return (float)SExt(v, 16) *  0.00390625f; }
float BrFixUnpackS8Q3(int32_t v)      { return (float)SExt(v, 8)  *  0.125f; }
float BrFixUnpackU8Angle(int32_t v)   { return (float)(v & 0xFF)  *  1.41015625f; }
float BrFixUnpackU8Range(int32_t v)   { return 400.0f + (float)(v & 0xFF) * 120.63491821289062f; }
float BrFixUnpackU32Q13(uint32_t v)   { return (float)((double)v / 8192.0); }
float BrFixUnpackS24Q1(uint32_t v)    { return (float)SExt((int32_t)v, 24) * 0.5f; }

float BrFixUnpackLevel(int32_t v)
{
    switch (v & 0xFF) {
    case 0:  return 0.0f;
    case 1:  return 170.0f;
    case 2:  return 212.0f;
    default: return 255.0f;
    }
}

/* --- slice1_02 / slice1_09: the rest --------------------------------- */

static int   g_cLerp;
static float g_lerpT;
static const BrCarState *g_pLerpA;
static const BrCarState *g_pLerpB;

void BrCarStateLerp(BrCarState *pDst, float t,
                    const BrCarState *pA, const BrCarState *pB)
{
    ++g_cLerp;
    g_lerpT  = t;
    g_pLerpA = pA;
    g_pLerpB = pB;
    *pDst = *pB;                /* enough for the tests that follow */
}

static int g_cNormalise;

void BrVec4Normalise(BrVec4 *pV)
{
    double n = sqrt((double)pV->f00 * pV->f00 + (double)pV->f04 * pV->f04
                  + (double)pV->f08 * pV->f08 + (double)pV->f0C * pV->f0C);
    ++g_cNormalise;
    if (n != 0.0) {
        pV->f00 = (float)(pV->f00 / n);
        pV->f04 = (float)(pV->f04 / n);
        pV->f08 = (float)(pV->f08 / n);
        pV->f0C = (float)(pV->f0C / n);
    }
}

static int g_cLockDepth;
static int g_cLockMax;

void BrNetMutexLock(void *hMutex)
{
    (void)hMutex;
    if (++g_cLockDepth > g_cLockMax)
        g_cLockMax = g_cLockDepth;
}

void BrNetMutexUnlock(void *hMutex)
{
    (void)hMutex;
    --g_cLockDepth;
}

int32_t BrNetSlotGetF02C(BrNetState *pNet, int32_t slot)
{
    int32_t v;
    BrNetMutexLock(pNet->aSlots[slot].hMutex);      /* re-entrant, on purpose */
    v = pNet->aSlots[slot].f02C;
    BrNetMutexUnlock(pNet->aSlots[slot].hMutex);
    return v;
}

/* --- XSLICE stand-ins ------------------------------------------------- */

#define MAX_WRITES 64
static int32_t g_aVal[MAX_WRITES];
static int32_t g_aBits[MAX_WRITES];
static int     g_cWrites;

void BrBitStreamWriteBits(BrBitStream *pBs, int32_t value, int32_t nBits)
{
    (void)pBs;
    if (g_cWrites < MAX_WRITES) {
        g_aVal[g_cWrites]  = value;
        g_aBits[g_cWrites] = nBits;
    }
    ++g_cWrites;
}

static float g_aProbe[2];
static int   g_cProbe;

float BrProbe1006F310(const float av3[3])
{
    (void)av3;
    return g_aProbe[(g_cProbe++) & 1];
}

void BrPodWriterMakeName(void *pStream, const char *pszSrc, char *pszDst)
{
    size_t i;
    (void)pStream;
    for (i = 0; i < 64 && pszSrc[i] != '\0'; ++i)
        pszDst[i] = pszSrc[i];
    for (; i < 64; ++i)
        pszDst[i] = '\0';
}

/* ===================================================================== */
/* STAND-INS end                                                         */
/* ===================================================================== */

/* --------------------------------------------------------------------- */
/* 1. Clamps                                                             */
/* --------------------------------------------------------------------- */

static void TestClamps(void)
{
    float v;

    v =  5.0f; BrCarClampUnit(&v); CHECK(v ==  1.0f);
    v = -5.0f; BrCarClampUnit(&v); CHECK(v == -1.0f);
    v =  0.25f; BrCarClampUnit(&v); CHECK(v == 0.25f);
    /* Exactly on either bound is left alone. */
    v =  1.0f; BrCarClampUnit(&v); CHECK(v ==  1.0f);
    v = -1.0f; BrCarClampUnit(&v); CHECK(v == -1.0f);

    v =  9000.0f; BrCarClampPosXY(&v); CHECK(v == 2048.0f);
    v = -1.0f;    BrCarClampPosXY(&v); CHECK(v == 0.0f);
    v =  2048.0f; BrCarClampPosXY(&v); CHECK(v == 2048.0f);

    v =  400.0f;  BrCarClampPosZ(&v); CHECK(v ==  256.0f);
    v = -400.0f;  BrCarClampPosZ(&v); CHECK(v == -256.0f);

    /* The asymmetry that matters: the low bound is applied with a test NaN
     * satisfies, the high bound with one it does not, so NaN comes out LOW. */
    v = (float)NAN; BrCarClampUnit(&v);   CHECK(v == -1.0f);
    v = (float)NAN; BrCarClampPosXY(&v);  CHECK(v ==  0.0f);
    v = (float)NAN; BrCarClampPosZ(&v);   CHECK(v == -256.0f);

    /* Idempotent, as any clamp must be. */
    v = 12345.0f;
    BrCarClampPosXY(&v);
    {
        float once = v;
        BrCarClampPosXY(&v);
        CHECK(v == once);
    }
}

/* --------------------------------------------------------------------- */
/* 2. Quantisers -- exact inverses of another module's dequantisers            */
/* --------------------------------------------------------------------- */

static void TestQuantRoundTrip(void)
{
    int32_t n;

    /* Every representable code must survive code -> float -> code. That is
     * the property that makes the pair a codec rather than two functions. */
    for (n = -32; n <= 31; ++n)
        CHECK(BrFixPackS6Q7Neg(BrFixUnpackS6Q7Neg(n)) == n);

    for (n = -128; n <= 127; ++n)
        CHECK(BrFixPackS8Q3(BrFixUnpackS8Q3(n)) == n);

    for (n = -32768; n <= 32767; ++n)
        CHECK(BrFixPackS16Q8(BrFixUnpackS16Q8(n)) == n);

    for (n = -32767; n <= 32767; ++n)
        CHECK(BrFixPackS16Q15Neg(BrFixUnpackS16Q15Neg(n)) == n);

    for (n = 0; n <= 255; ++n)
        CHECK(BrFixPackU8Angle(BrFixUnpackU8Angle(n)) == n);

    for (n = 0; n <= 63; ++n)
        CHECK(BrFixPackU8Range(BrFixUnpackU8Range(n)) == n);

    for (n = 0; n <= 3; ++n)
        CHECK(BrFixPackLevel(BrFixUnpackLevel(n)) == n);
}

static void TestQuantClamps(void)
{
    /* Saturation, at the documented widths. */
    CHECK(BrFixPackS6Q7Neg(-100.0f)  ==  31);
    CHECK(BrFixPackS6Q7Neg( 100.0f)  == -32);
    CHECK(BrFixPackS8Q3(  100.0f)    == 127);
    CHECK(BrFixPackS8Q3( -100.0f)    == -128);
    CHECK(BrFixPackS16Q8( 1000.0f)   == 32767);
    CHECK(BrFixPackS16Q8(-1000.0f)   == -32768);
    CHECK(BrFixPackS16Q15Neg(-2.0f)  == 32767);
    CHECK(BrFixPackS16Q15Neg( 2.0f)  == -32768);
    CHECK(BrFixPackU8Angle(-10.0f)   == 0);
    CHECK(BrFixPackU8Angle(1e6f)     == 255);
    CHECK(BrFixPackU8Range(0.0f)     == 0);      /* below the 400 origin */
    CHECK(BrFixPackU8Range(1e6f)     == 63);

    /* The negative-scale pair really does invert the sign. */
    CHECK(BrFixPackS16Q15Neg(1.0f) < 0);
    CHECK(BrFixPackS6Q7Neg(0.25f)  < 0);
    /* ...while the positive-scale ones do not. */
    CHECK(BrFixPackS16Q8(1.0f) > 0);
    CHECK(BrFixPackS8Q3(1.0f)  > 0);

    /* Rounding is floor(x + 0.5): halves go UP, not away from zero. */
    CHECK(BrFixPackS8Q3( 0.0625f) == 1);   /* 8*0.0625 = 0.5  -> 1 */
    CHECK(BrFixPackS8Q3(-0.0625f) == 0);   /* -0.5            -> 0 */

    /* The 4-level classifier's thresholds sit one above each output of its
     * inverse, which is what makes the round trip above exact. */
    CHECK(BrFixPackLevel(127.9f) == 0);
    CHECK(BrFixPackLevel(128.0f) == 1);
    CHECK(BrFixPackLevel(170.0f) == 1);
    CHECK(BrFixPackLevel(171.0f) == 2);
    CHECK(BrFixPackLevel(212.0f) == 2);
    CHECK(BrFixPackLevel(213.0f) == 3);
    CHECK(BrFixPackLevel((float)NAN) == 0);     /* unordered takes arm one */

    /* Monotonic (non-decreasing) over its domain. */
    {
        int i;
        int32_t prev = BrFixPackU8Angle(0.0f);
        for (i = 1; i <= 360; ++i) {
            int32_t cur = BrFixPackU8Angle((float)i);
            CHECK(cur >= prev);
            prev = cur;
        }
    }
}

/* --------------------------------------------------------------------- */
/* 3. Bitstream encoders                                                 */
/* --------------------------------------------------------------------- */

static void MakeState(BrCarState *p, float bias)
{
    float *f = &p->f00;
    int    i;
    for (i = 0; i < BR_CARSTATE_FLOATS; ++i)
        f[i] = 0.0f;
    p->f00 = 0.5f;  p->f04 = -0.25f; p->f08 = 0.125f; p->f0C = -0.5f;
    p->f10 = 100.0f + bias;
    p->f14 = 200.0f + bias;
    p->f18 = 12.5f;
    p->f1C = 1.5f;  p->f20 = -2.5f;
    p->f28 = 1.0f;  p->f2C = -1.0f;  p->f30 = 2.0f;   p->f34 = 3.0f;
    p->f38 = 0.1f;  p->f3C = 90.0f;
    p->f4C = 1.0f;  p->f50 = 0.0f;   p->f54 = 1.0f;   p->f58 = 0.0f;
    p->f6C = 128.0f; p->f70 = 0.0f;  p->f74 = 1.0f;
    p->f78 = 4.0f;  p->f7C = 3000.0f;
    p->f80 = 200.0f; p->f84 = 0.0f;
    p->f88 = 128.0f; p->f8C = 0.0f;  p->f90 = 128.0f; p->f94 = 0.0f;
    p->f98 = 128.0f; p->f9C = 0.0f;
}

static void TestEncode(void)
{
    BrCarState  st;
    BrBitStream bs;
    int         i;
    int         cBits = 0;

    memset(&bs, 0, sizeof bs);
    MakeState(&st, 0.0f);

    g_cWrites = 0;
    BrCarStateEncode(&bs, &st);

    /* 32 fields, matching the 32 the decoder at 0x10006EC0 reads. */
    CHECK(g_cWrites == 32);
    for (i = 0; i < g_cWrites && i < MAX_WRITES; ++i)
        cBits += g_aBits[i];
    CHECK(cBits == 187);

    /* Every value must fit the width it is written with, or the wire form
     * would silently lose bits. Signed fields are checked as two's
     * complement in that width. */
    for (i = 0; i < g_cWrites && i < MAX_WRITES; ++i) {
        int32_t w  = g_aBits[i];
        int32_t lo = -((int32_t)1 << (w - 1));
        int32_t hi = ((int32_t)1 << w) - 1;
        CHECK(g_aVal[i] >= lo && g_aVal[i] <= hi);
    }

    /* The booleans really are booleans. */
    CHECK(g_aVal[15] == 1 && g_aBits[15] == 1);     /* f4C = 1.0f  */
    CHECK(g_aVal[16] == 0 && g_aBits[16] == 1);     /* f50 = 0.0f  */
    CHECK(g_aVal[31] == 0 && g_aBits[31] == 1);     /* f9C = 0.0f  */

    /* A NaN flag reads as FALSE, because the original's test is "equal or
     * unordered". */
    st.f4C = (float)NAN;
    g_cWrites = 0;
    BrCarStateEncode(&bs, &st);
    CHECK(g_aVal[15] == 0);
}

static void TestEncodeDelta(void)
{
    BrCarState  cur, ref;
    BrBitStream bs;
    int         i, cBits = 0;
    int32_t     code;

    memset(&bs, 0, sizeof bs);
    MakeState(&cur, 0.0f);
    MakeState(&ref, 0.0f);

    g_cWrites = 0;
    BrCarStateEncodeDelta(&bs, &cur, &ref);

    CHECK(g_cWrites == 17);
    for (i = 0; i < g_cWrites && i < MAX_WRITES; ++i)
        cBits += g_aBits[i];
    CHECK(cBits == 4 * 8 + 14 + 14 + 11 + 9 + 6 + 2 + 2 + 6);

    /* Identical states -> every delta prefix is 0. */
    CHECK(((uint32_t)g_aVal[4] & 0x3000u) == 0u);   /* f10 */
    CHECK(((uint32_t)g_aVal[5] & 0x3000u) == 0u);   /* f14 */
    CHECK(((uint32_t)g_aVal[6] & 0x0600u) == 0u);   /* f18 */
    CHECK(((uint32_t)g_aVal[7] & 0x0180u) == 0u);   /* f78 */

    /* The transmitted domain is q >> 7 where q = 8192*v, i.e. 64 counts per
     * unit, so one 0x1000 step of the high part is 4096/64 = 64.0 in float
     * terms. With ref.f10 = 100 the high part is 0x1000 and one step up lands
     * on 0x2000: the prefix must be exactly 1. */
    cur.f10 = ref.f10 + 64.0f;
    g_cWrites = 0;
    BrCarStateEncodeDelta(&bs, &cur, &ref);
    code = (int32_t)((uint32_t)g_aVal[4] & 0x3000u);
    CHECK(code == 0x1000);

    /* Two steps up saturates the prefix at 2. */
    cur.f10 = ref.f10 + 128.0f;
    g_cWrites = 0;
    BrCarStateEncodeDelta(&bs, &cur, &ref);
    CHECK(((uint32_t)g_aVal[4] & 0x3000u) == 0x2000u);

    /* Ten steps up still reads 2 -- the code is lossy above two steps, which
     * is the asymmetry worth remembering. */
    cur.f10 = ref.f10 + 640.0f;
    g_cWrites = 0;
    BrCarStateEncodeDelta(&bs, &cur, &ref);
    CHECK(((uint32_t)g_aVal[4] & 0x3000u) == 0x2000u);

    /* Downwards is a single code, 3, however far it goes. */
    cur.f10 = ref.f10 - 64.0f;
    g_cWrites = 0;
    BrCarStateEncodeDelta(&bs, &cur, &ref);
    CHECK(((uint32_t)g_aVal[4] & 0x3000u) == 0x3000u);
    cur.f10 = ref.f10 - 100.0f;
    g_cWrites = 0;
    BrCarStateEncodeDelta(&bs, &cur, &ref);
    CHECK(((uint32_t)g_aVal[4] & 0x3000u) == 0x3000u);

    /* The transmitted low bits are the current value's, not the delta. */
    cur.f10 = ref.f10;
    g_cWrites = 0;
    BrCarStateEncodeDelta(&bs, &cur, &ref);
    CHECK((g_aVal[4] & 0xFFF)
          == (int32_t)(((uint32_t)BrFixPackU24Q13(cur.f10) >> 7) & 0xFFFu));

    /* The first four fields are absolute: they match the absolute encoder. */
    {
        int32_t aDelta[4];
        int     k;
        for (k = 0; k < 4; ++k)
            aDelta[k] = g_aVal[k];
        g_cWrites = 0;
        BrCarStateEncode(&bs, &cur);
        for (k = 0; k < 4; ++k)
            CHECK(aDelta[k] == g_aVal[k]);
    }
}

/* --------------------------------------------------------------------- */
/* 4. The 22-byte record                                                 */
/* --------------------------------------------------------------------- */

static void TestPackRoundTrip(void)
{
    BrCarState  st, out;
    BrCarPacked rec;

    MakeState(&st, 0.0f);
    memset(&out, 0, sizeof out);
    memset(&rec, 0xAA, sizeof rec);

    BrCarStatePack(&rec, &st);
    BrCarStateUnpack(&out, &rec);

    /* Orientation survives to one 15-bit step (the low bit is a flag). */
    CHECK_NEAR(out.f00, st.f00, 2.0 / 32768.0);
    CHECK_NEAR(out.f04, st.f04, 2.0 / 32768.0);
    CHECK_NEAR(out.f08, st.f08, 2.0 / 32768.0);
    CHECK_NEAR(out.f0C, st.f0C, 2.0 / 32768.0);

    /* Position: three low bits of axis 1 and two of axis 2 are flags, so the
     * step is 8/8192 and 4/8192 respectively. This is also the test that the
     * angle byte stuffed into the top of each dword does NOT bleed into the
     * position -- with a 90 degree angle, b[0x0B] is non-zero. */
    CHECK(rec.b[0x0B] != 0);
    CHECK_NEAR(out.f10, st.f10, 8.0 / 8192.0);
    CHECK_NEAR(out.f14, st.f14, 4.0 / 8192.0);
    CHECK_NEAR(out.f18, st.f18, 1.0 / 128.0);

    CHECK_NEAR(out.f34, st.f34, 1.0 / 8.0);
    CHECK_NEAR(out.f38, st.f38, 1.0 / 128.0);
    CHECK_NEAR(out.f3C, st.f3C, 1.5);
    CHECK_NEAR(out.f7C, st.f7C, 121.0);

    /* f40 duplicates f3C; f44 and f48 are the same angle 35 degrees on. */
    CHECK(out.f40 == out.f3C);
    CHECK(out.f44 == out.f48);
    CHECK_NEAR(out.f44, out.f3C + 35.0f, 1e-3);

    /* The flags round-trip as truth values, and the decoded TRUE is 128.0f
     * for most of them but 1.0f for f70/f74. */
    CHECK(out.f6C == 128.0f);           /* set   */
    CHECK(out.f70 == 0.0f);             /* clear */
    CHECK(out.f74 == 1.0f);             /* set, and 1.0f not 128.0f */
    CHECK(out.f88 == 128.0f);
    CHECK(out.f8C == 0.0f);
    CHECK(out.f90 == 128.0f);
    CHECK(out.f94 == 0.0f);
    CHECK(out.f98 == 128.0f);
    CHECK(out.f9C == 0.0f);

    /* And the decoded values feed straight back in: a second pass through the
     * codec must be idempotent. */
    {
        BrCarPacked rec2;
        BrCarState  out2;
        memset(&out2, 0, sizeof out2);  /* unpack leaves seven fields alone */
        BrCarStatePack(&rec2, &out);
        CHECK(memcmp(&rec, &rec2, sizeof rec) == 0);
        BrCarStateUnpack(&out2, &rec2);
        CHECK(memcmp(&out, &out2, sizeof out) == 0);
    }
}

static void TestPackBitLayout(void)
{
    BrCarState  st;
    BrCarPacked rec;

    MakeState(&st, 0.0f);
    st.f6C = 0.0f; st.f88 = 0.0f; st.f9C = 0.0f;
    st.f70 = 0.0f; st.f74 = 0.0f;
    st.f8C = 0.0f; st.f90 = 0.0f; st.f94 = 0.0f; st.f98 = 0.0f;
    BrCarStatePack(&rec, &st);
    CHECK((rec.b[0x08] & 0x07u) == 0u);
    CHECK((rec.b[0x0C] & 0x03u) == 0u);
    CHECK((rec.b[0x00] & 1u) == 0u);
    CHECK((rec.b[0x02] & 1u) == 0u);
    CHECK((rec.b[0x04] & 1u) == 0u);
    CHECK((rec.b[0x06] & 1u) == 0u);

    st.f6C = 1.0f; st.f88 = 1.0f; st.f9C = 1.0f;
    st.f70 = 1.0f; st.f74 = 1.0f;
    st.f8C = 1.0f; st.f90 = 1.0f; st.f94 = 1.0f; st.f98 = 1.0f;
    BrCarStatePack(&rec, &st);
    CHECK((rec.b[0x08] & 0x07u) == 0x07u);      /* f6C, f88, f9C */
    CHECK((rec.b[0x0C] & 0x03u) == 0x03u);      /* f70, f74      */
    CHECK((rec.b[0x00] & 1u) == 1u);
    CHECK((rec.b[0x02] & 1u) == 1u);
    CHECK((rec.b[0x04] & 1u) == 1u);
    CHECK((rec.b[0x06] & 1u) == 1u);

    /* b[0x0B] and b[0x0F] are stolen from the top of the two dwords, so a
     * saturated angle must not disturb the position bits below it. */
    st.f3C = 1e6f;                              /* saturates to 255 */
    st.f10 = 1234.5f;
    BrCarStatePack(&rec, &st);
    CHECK(rec.b[0x0B] == 0xFF);
    {
        BrCarState out;
        BrCarStateUnpack(&out, &rec);
        CHECK_NEAR(out.f10, st.f10, 8.0 / 8192.0);
    }
}

/* --------------------------------------------------------------------- */
/* 5. Table accessors                                                    */
/* --------------------------------------------------------------------- */

static void TestEntityCount(void)
{
    static unsigned char aRec[4 * BR_ENTITY_STRIDE];
    uint32_t v;

    memset(aRec, 0, sizeof aRec);
    CHECK(BrEntityCountActive(aRec, 4) == 0);

    v = 1u;  memcpy(aRec + 0 * BR_ENTITY_STRIDE, &v, sizeof v);
    v = 0u;  memcpy(aRec + 1 * BR_ENTITY_STRIDE, &v, sizeof v);
    v = 99u; memcpy(aRec + 2 * BR_ENTITY_STRIDE, &v, sizeof v);
    v = 7u;  memcpy(aRec + 3 * BR_ENTITY_STRIDE, &v, sizeof v);
    CHECK(BrEntityCountActive(aRec, 4) == 3);

    /* The count bounds the walk, so a smaller count sees fewer records. */
    CHECK(BrEntityCountActive(aRec, 3) == 2);
    CHECK(BrEntityCountActive(aRec, 1) == 1);

    /* Guarded at the top: a non-positive count touches nothing. */
    CHECK(BrEntityCountActive(aRec, 0) == 0);
    CHECK(BrEntityCountActive(aRec, -5) == 0);
}

static void TestStackPop(void)
{
    int32_t aStack[4];
    int32_t iTop = 2;

    aStack[0] = 10; aStack[1] = 20; aStack[2] = 30; aStack[3] = 40;

    /* The top element is AT the index, so index 2 yields aStack[2] and index
     * 0 still holds one element. */
    CHECK(BrNetStackPop(NULL, aStack, &iTop) == 30);
    CHECK(iTop == 1);
    CHECK(BrNetStackPop(NULL, aStack, &iTop) == 20);
    CHECK(BrNetStackPop(NULL, aStack, &iTop) == 10);
    CHECK(iTop == -1);
    CHECK(BrNetStackPop(NULL, aStack, &iTop) == -1);
    CHECK(iTop == -1);                  /* empty pops do not decrement */

    /* The lock is taken and released on both paths. */
    g_cLockDepth = 0;
    (void)BrNetStackPop(NULL, aStack, &iTop);
    CHECK(g_cLockDepth == 0);
    iTop = 0;
    (void)BrNetStackPop(NULL, aStack, &iTop);
    CHECK(g_cLockDepth == 0);
}

static BrNetState g_net;

static void TestSlotAccessors(void)
{
    uint8_t b34 = 0, b35 = 0, b36 = 0;
    int32_t flag = 0;

    memset(&g_net, 0, sizeof g_net);

    g_net.aSlots[3].f030 = 0x1234;
    g_net.aSlots[3].f034 = (int32_t)0x00AABBCC;
    CHECK(BrNetSlotGetF030(&g_net, 3, &b34, &b35, &b36) == 0x1234);
    CHECK(b34 == 0xCC && b35 == 0xBB && b36 == 0xAA);

    BrNetSlotSetName(&g_net, 5, "Player One");
    CHECK(strcmp(g_net.aSlots[5].f570, "Player One") == 0);

    g_net.aSlots[2].f974 = 42;
    CHECK(BrNetSlotGetF974(&g_net, 2) == 42);
    g_net.aSlots[2].f974 = -7;
    CHECK(BrNetSlotGetF974(&g_net, 2) == 0);    /* floored, not passed through */

    /* max(0, (f02C & 0x3F) - 4): the mask drops everything above bit 5. */
    g_net.aSlots[1].f02C = 0x3F;
    CHECK(BrNetSlotGetF02CBiased(&g_net, 1) == 0x3F - 4);
    g_net.aSlots[1].f02C = 4;
    CHECK(BrNetSlotGetF02CBiased(&g_net, 1) == 0);
    g_net.aSlots[1].f02C = 3;
    CHECK(BrNetSlotGetF02CBiased(&g_net, 1) == 0);
    g_net.aSlots[1].f02C = (int32_t)0xFFC0 | 5;
    CHECK(BrNetSlotGetF02CBiased(&g_net, 1) == 1);

    /* ...and it nests the slot mutex, which is why the original needs a
     * recursive lock. */
    g_cLockDepth = 0;
    g_cLockMax   = 0;
    (void)BrNetSlotGetF02CBiased(&g_net, 1);
    CHECK(g_cLockMax >= 2);
    CHECK(g_cLockDepth == 0);

    g_net.a102212D0[7] = 77;
    CHECK(BrNetGetA102212D0(&g_net, 7) == 77);

    g_net.f10220DD0 = 0;
    BrNetSetF10220DD0(&g_net);
    CHECK(g_net.f10220DD0 == 1);
    BrNetClearF10220DD0(&g_net);
    CHECK(g_net.f10220DD0 == 0);

    /* The deadline compare is UNSIGNED, so the -1 BrNetReset writes disarms
     * the timer instead of making it permanently overdue. */
    g_net.f1022AF00 = -1;
    flag = 0;
    BrNetCheckDeadline(&g_net, 0u, &flag);
    CHECK(flag == 0);
    BrNetCheckDeadline(&g_net, 0xFFFFFFFEu, &flag);
    CHECK(flag == 0);
    BrNetCheckDeadline(&g_net, 0xFFFFFFFFu, &flag);
    CHECK(flag == 1);

    g_net.f1022AF00 = 100;
    flag = 0;
    BrNetCheckDeadline(&g_net, 99u, &flag);
    CHECK(flag == 0);
    BrNetCheckDeadline(&g_net, 100u, &flag);
    CHECK(flag == 1);
}

/* --------------------------------------------------------------------- */
/* 6. Dead reckoning                                                     */
/* --------------------------------------------------------------------- */

static BrCarState *SlotRecords(BrNetSlot *pSlot)
{
    return (BrCarState *)(void *)pSlot->f058;
}

static void TestPredict(void)
{
    BrNetSlot  *pSlot;
    BrCarState *aRec;
    BrCarState  dst;
    int         rc;

    memset(&g_net, 0, sizeof g_net);
    pSlot = &g_net.aSlots[4];
    aRec  = SlotRecords(pSlot);

    /* Fewer than two samples: f7C is parked at 400.0f -- the BOTTOM of the
     * BrFixUnpackU8Range window, not zero -- and the answer is 0. */
    pSlot->f558 = 1;
    memset(&dst, 0, sizeof dst);
    dst.f7C = 1234.0f;
    g_cLerp = 0;
    rc = BrNetSlotPredict(&dst, 4, &g_net, NULL, -1, 0u);
    CHECK(rc == 0);
    CHECK(dst.f7C == 400.0f);
    CHECK(g_cLerp == 0);

    /* Two samples, extrapolation. seq 100 and 110 -> dt 10; now = 112 gives
     * an age of 2, so t = (2 + 10) / 10. */
    pSlot->f558   = 2;
    pSlot->f560   = -1;
    pSlot->f038[2] = 1; pSlot->f00C[2] = 100;
    pSlot->f038[5] = 1; pSlot->f00C[5] = 110;
    MakeState(&aRec[2], 0.0f);
    MakeState(&aRec[5], 10.0f);

    g_cLerp = 0; g_cProbe = 0;
    g_aProbe[0] = 3.0f; g_aProbe[1] = 1.0f;
    memset(&dst, 0, sizeof dst);
    rc = BrNetSlotPredict(&dst, 4, &g_net, NULL, -1, 112u);
    CHECK(rc == 1);
    CHECK(g_cLerp == 1);
    CHECK_NEAR(g_lerpT, 1.2f, 1e-6);
    CHECK(g_pLerpA == &aRec[2]);        /* the OLDER sample is the "a" end */
    CHECK(g_pLerpB == &aRec[5]);        /* the newest is "b"               */
    /* Two probes, and their difference lands on the third position axis. */
    CHECK(g_cProbe == 2);
    CHECK_NEAR(dst.f18, aRec[5].f18 + (3.0f - 1.0f), 1e-4);
    /* The bookkeeping took the "new best index" reset path. */
    CHECK(pSlot->f560 == 5);
    CHECK(pSlot->f564 == 110);
    CHECK(pSlot->f568 == 0);
    CHECK(pSlot->f56C == 0);

    /* t is never below 1: this extrapolates past the newest sample, and the
     * age term is capped at 6 ticks however stale the data is. */
    g_cProbe = 0;
    rc = BrNetSlotPredict(&dst, 4, &g_net, NULL, -1, 10000u);
    CHECK(rc == 1);
    CHECK_NEAR(g_lerpT, (6.0f + 10.0f) / 10.0f, 1e-6);

    /* Same best index as last time and a zero dt: the newest record is copied
     * verbatim and no interpolation happens at all.
     *
     * Reaching dt == 0 also demonstrates the fallback gotcha: with sample 2
     * vacated, the second scan finds nothing and falls back to index 0, whose
     * sequence number is read even though the slot is unoccupied. Giving
     * index 0 the same sequence as the winner is exactly what a real slot
     * with one live sample and a zeroed table looks like. */
    pSlot->f038[2] = 0;                 /* only one occupied sample now */
    pSlot->f00C[0] = 110;               /* the unoccupied fallback entry */
    pSlot->f560    = 5;
    pSlot->f568    = 0;
    MakeState(&aRec[5], 3.0f);
    aRec[5].f00 = 0.5f; aRec[5].f04 = 0.5f; aRec[5].f08 = 0.5f; aRec[5].f0C = 0.5f;
    g_cLerp = 0;
    memset(&dst, 0xFF, sizeof dst);
    rc = BrNetSlotPredict(&dst, 4, &g_net, NULL, -1, 200u);
    CHECK(rc == 1);
    CHECK(g_cLerp == 0);
    CHECK(pSlot->f568 == 1);            /* the "same index" counter advanced */
    CHECK_NEAR(dst.f10, aRec[5].f10, 1e-4);
    /* The tail still runs: the orientation was normalised. */
    CHECK_NEAR((double)dst.f00 * dst.f00 + (double)dst.f04 * dst.f04
             + (double)dst.f08 * dst.f08 + (double)dst.f0C * dst.f0C, 1.0, 1e-5);

    /* The local player's own slot skips prediction but NOT the tail. */
    g_cLerp = 0; g_cNormalise = 0;
    memset(&dst, 0, sizeof dst);
    dst.f00 = 9.0f;                     /* out of range on purpose */
    dst.f10 = -5.0f;
    dst.f18 = 9999.0f;
    rc = BrNetSlotPredict(&dst, 4, &g_net, NULL, 4, 0u);
    CHECK(rc == 1);
    CHECK(g_cLerp == 0);
    CHECK(g_cNormalise == 1);
    CHECK(dst.f00 == 1.0f);             /* clamped to 1 then normalised */
    CHECK(dst.f10 == 0.0f);
    CHECK(dst.f18 == 256.0f);

    /* A zero orientation is replaced by the identity and NOT normalised --
     * that is the branch that avoids dividing by zero. */
    g_cNormalise = 0;
    memset(&dst, 0, sizeof dst);
    rc = BrNetSlotPredict(&dst, 4, &g_net, NULL, 4, 0u);
    CHECK(rc == 1);
    CHECK(g_cNormalise == 0);
    CHECK(dst.f00 == 1.0f);
    CHECK(dst.f04 == 0.0f && dst.f08 == 0.0f && dst.f0C == 0.0f);

    /* Components that cancel to zero take the same branch, which is the
     * reason the sum order is worth preserving. */
    g_cNormalise = 0;
    memset(&dst, 0, sizeof dst);
    dst.f00 = 0.5f; dst.f04 = -0.5f; dst.f08 = 0.25f; dst.f0C = -0.25f;
    rc = BrNetSlotPredict(&dst, 4, &g_net, NULL, 4, 0u);
    CHECK(rc == 1);
    CHECK(g_cNormalise == 0);
    CHECK(dst.f00 == 1.0f);

    /* Locks balance on every exit. */
    CHECK(g_cLockDepth == 0);
}

/* --------------------------------------------------------------------- */
/* 7. Key cache                                                          */
/* --------------------------------------------------------------------- */

static void TestKeyCache(void)
{
    static BrKeyCache      cache;
    static BrKeyCacheEntry aEnt[3];
    int32_t                aKey[16];
    int                    i;

    memset(&cache, 0, sizeof cache);
    memset(aEnt, 0, sizeof aEnt);
    cache.aEntries = aEnt;
    cache.cEntries = 3;

    for (i = 0; i < 16; ++i) {
        aEnt[0].aKey[i] = i;
        aEnt[1].aKey[i] = 100 + i;
        aEnt[2].aKey[i] = 200 + i;
        aKey[i] = 100 + i;
    }
    /* The three dwords ahead of the key are payload and must not matter. */
    aEnt[1].f00 = 0x1111; aEnt[1].f04 = 0x2222; aEnt[1].f08 = 0x3333;

    CHECK(BrKeyCacheFind(&cache, aKey) == 1);

    aKey[15] = -1;
    CHECK(BrKeyCacheFind(&cache, aKey) == -1);   /* one dword is enough */

    /* An empty cache never matches, whatever the key. */
    cache.cEntries = 0;
    for (i = 0; i < 16; ++i)
        aKey[i] = 100 + i;
    CHECK(BrKeyCacheFind(&cache, aKey) == -1);

    /* The first match wins. */
    cache.cEntries = 3;
    memcpy(aEnt[2].aKey, aEnt[1].aKey, sizeof aEnt[1].aKey);
    CHECK(BrKeyCacheFind(&cache, aKey) == 1);

    /* Reset clears +0x008..+0x014 and the 256-dword table, leaving only the
     * vtable slot and +0x004. */
    cache.aEntries = NULL;              /* not ours to free */
    cache.pFile    = NULL;
    cache.f004     = 0x5A5A;
    cache.f014     = 0x6B6B;
    cache.f420     = 1;
    cache.a020[0]  = 1;
    cache.a020[255] = 1;
    BrKeyCacheReset(&cache);
    CHECK(cache.cEntries == 0);
    CHECK(cache.f420 == 0);
    CHECK(cache.a020[0] == 0 && cache.a020[255] == 0);
    CHECK(cache.f014 == 0);
    CHECK(cache.f004 == 0x5A5A);        /* the only survivor besides the vtable */
}

/* --------------------------------------------------------------------- */
/* 8. POD writer                                                         */
/* --------------------------------------------------------------------- */

static uint32_t Get32(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void TestPodWriter(void)
{
    static BrPodWriter    w;
    static unsigned char  aFile[4096];
    const char           *pszPath = "test_slice2_12.pod";
    const char            szA[] = "hello world";
    const char            szB[] = "0123456789";
    FILE                 *pf;
    size_t                cb;
    uint32_t              cEnt, offDir;
    const unsigned char  *pEnt;

    if (BrPodWriteOpen(&w, pszPath) != 0) {
        printf("SKIP pod writer: cannot create %s\n", pszPath);
        return;
    }
    BrPodWriteAdd(&w, "data/First.bin", szA, (uint32_t)(sizeof szA - 1), 1, 2);
    BrPodWriteAdd(&w, "Second.bin", szB, (uint32_t)(sizeof szB - 1), 3, 4);
    BrPodWriteClose(&w);

    pf = fopen(pszPath, "rb");
    CHECK(pf != NULL);
    if (pf == NULL)
        return;
    cb = fread(aFile, 1, sizeof aFile, pf);
    fclose(pf);
    remove(pszPath);

    /* The header the reader in br_pod.h documents. */
    CHECK(cb > 16);
    CHECK(aFile[0] == 'P' && aFile[1] == 'O' && aFile[2] == 'D');
    CHECK(Get32(aFile + 4) == 0x1F4u);
    cEnt   = Get32(aFile + 8);
    offDir = Get32(aFile + 12);
    CHECK(cEnt == 2);
    CHECK(offDir + 2u * 76u == (uint32_t)cb);

    /* Payloads start at 16, because the header slot is reserved up front. */
    pEnt = aFile + offDir;
    CHECK(Get32(pEnt + 0) == 16u);
    CHECK(Get32(pEnt + 4) == (uint32_t)(sizeof szA - 1));
    CHECK(pEnt[8] == 1 && pEnt[9] == 2);
    CHECK(memcmp(aFile + Get32(pEnt + 0), szA, sizeof szA - 1) == 0);

    /* Names are uppercased in place after they are copied. */
    CHECK(memcmp(pEnt + 12, "DATA/FIRST.BIN", 14) == 0);
    CHECK(pEnt[12 + 14] == 0);          /* and NUL-padded to 64 */

    pEnt = aFile + offDir + 76;
    CHECK(Get32(pEnt + 0) == 16u + (uint32_t)(sizeof szA - 1));
    CHECK(Get32(pEnt + 4) == (uint32_t)(sizeof szB - 1));
    CHECK(pEnt[8] == 3 && pEnt[9] == 4);
    CHECK(memcmp(pEnt + 12, "SECOND.BIN", 10) == 0);
    CHECK(memcmp(aFile + Get32(pEnt + 0), szB, sizeof szB - 1) == 0);

    /* Entries are contiguous: each offset is the previous one plus its size,
     * which is what makes the directory a valid index. */
    CHECK(Get32(aFile + offDir + 76) ==
          Get32(aFile + offDir) + Get32(aFile + offDir + 4));
}

/* --------------------------------------------------------------------- */

int main(void)
{
    TestClamps();
    TestQuantRoundTrip();
    TestQuantClamps();
    TestEncode();
    TestEncodeDelta();
    TestPackRoundTrip();
    TestPackBitLayout();
    TestEntityCount();
    TestStackPop();
    TestSlotAccessors();
    TestPredict();
    TestKeyCache();
    TestPodWriter();

    if (g_cFail != 0) {
        printf("%d FAILURES\n", g_cFail);
        return 1;
    }
    printf("test_slice2_12: all checks passed\n");
    return 0;
}
