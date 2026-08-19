/* test_slice1_02.c -- behaviour tests for the 0x100049C0-0x100079E0 slice.
 *
 * These assert properties the ORIGINAL has (clamp endpoints, sentinel values,
 * sign conventions, bit budgets, aliasing), not merely what this port happens
 * to compute. The bit budgets in particular are independent evidence that the
 * decoders read their fields in the right order and widths: any transposition
 * would change them.
 *
 * This file also supplies the three external symbols the module declares but
 * does not own (the bit reader from a later pass, the announce hook from a later pass,
 * and the mutex hooks that replace the KERNEL32 calls).
 */

#include "slice1_02.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int g_fails;
static int g_checks;

#define CHECK(cond)                                                          \
    do {                                                                     \
        ++g_checks;                                                          \
        if (!(cond)) {                                                       \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);           \
            ++g_fails;                                                       \
        }                                                                    \
    } while (0)

static int Near(double a, double b, double eps)
{
    double d = a - b;
    return (d < 0 ? -d : d) <= eps;
}

/* ---------------------------------------------------------------------
 * Stand-ins for the symbols owned by other slices
 * ------------------------------------------------------------------- */

/* MSB-first, matching 0x10073C90: take from the top of the current byte
 * downwards, return right-aligned, advance to the next byte at bit 8. */
struct BrBitReader {
    int            bitPos;
    int            byteIndex;
    const uint8_t *pData;
};

uint32_t BrBitReaderRead(BrBitReader *pReader, unsigned nBits)
{
    uint32_t acc = 0;

    while (nBits != 0u) {
        int      avail    = 8 - pReader->bitPos;
        int      take     = (avail > (int)nBits) ? (int)nBits : avail;
        int      leftover = avail - take;
        uint32_t mask     = (((1u << take) - 1u) << leftover);
        uint32_t bits     = ((uint32_t)pReader->pData[pReader->byteIndex] & mask)
                            >> leftover;

        acc = (acc << take) | bits;
        pReader->bitPos += take;
        if (pReader->bitPos >= 8) {
            pReader->bitPos = 0;
            pReader->byteIndex += 1;
        }
        nBits -= (unsigned)take;
    }
    return acc;
}

static int ReaderBitsConsumed(const BrBitReader *r)
{
    return r->byteIndex * 8 + r->bitPos;
}

static char g_lastAnnounce[0x400];
static int  g_announceCount;

void BrNetAnnounce(const char *pszText)
{
    ++g_announceCount;
    strncpy(g_lastAnnounce, pszText, sizeof g_lastAnnounce - 1);
    g_lastAnnounce[sizeof g_lastAnnounce - 1] = '\0';
}

static int g_lockDepth;
static int g_lockPeak;
static int g_lockCount;
static int g_lockUnbalanced;

void BrNetMutexLock(void *hMutex)
{
    (void)hMutex;
    ++g_lockCount;
    ++g_lockDepth;
    if (g_lockDepth > g_lockPeak)
        g_lockPeak = g_lockDepth;
}

void BrNetMutexUnlock(void *hMutex)
{
    (void)hMutex;
    if (g_lockDepth == 0)
        ++g_lockUnbalanced;
    else
        --g_lockDepth;
}

/* ---------------------------------------------------------------------
 * Bit writer, for building streams the decoders can eat
 * ------------------------------------------------------------------- */

static uint8_t g_stream[64];
static int     g_streamBits;

static void StreamReset(void)
{
    memset(g_stream, 0, sizeof g_stream);
    g_streamBits = 0;
}

static void PutBits(uint32_t v, int n)
{
    int i;

    for (i = n - 1; i >= 0; --i) {
        int bit  = (int)((v >> i) & 1u);
        int byte = g_streamBits >> 3;
        int off  = 7 - (g_streamBits & 7);

        if (bit)
            g_stream[byte] |= (uint8_t)(1u << off);
        g_streamBits += 1;
    }
}

static void StreamOpen(BrBitReader *r)
{
    r->bitPos    = 0;
    r->byteIndex = 0;
    r->pData     = g_stream;
}

/* ---------------------------------------------------------------------
 * 1. Fixed-point codecs
 * ------------------------------------------------------------------- */

static void TestPackClamps(void)
{
    /* Endpoints of the three clamps, exactly as the original spells them. */
    CHECK(BrFixPackU24Q13(-1.0f) == 0);
    CHECK(BrFixPackU24Q13(1.0e9f) == 16777215);
    CHECK(BrFixPackU24Q13(2047.9999f) == 16777215 ||
          BrFixPackU24Q13(2047.9999f) == 16777215 - 1);

    CHECK(BrFixPackS24Q1(1.0e9f) == 8388607);
    CHECK(BrFixPackS24Q1(-1.0e9f) == -8388608);

    CHECK(BrFixPackS16Q7(1.0e6f) == 32767);
    CHECK(BrFixPackS16Q7(-1.0e6f) == -32768);
    CHECK(BrFixPackS16Q7(255.9999f) == 32767);

    /* GOTCHA, and it is the original's, not the port's: 0x10006730 and
     * 0x10006770 clamp the INTEGER that __ftol produced, and __ftol keeps only
     * the low dword of the 64-bit conversion. Once 0.5 + scale*v leaves int32
     * range the sign flips underneath the clamp, so a huge POSITIVE input
     * saturates to the NEGATIVE limit. The safe domain of 0x10006770 is
     * |v| < 2^31/128 = 16777216, far outside anything the game feeds it. */
    CHECK(BrFixPackS16Q7(1.0e9f) == -32768);
    CHECK(BrFixPackS16Q7(-1.0e9f) == 32767);
    /* 0x100066E0 is immune: it clamps the float first, so __ftol never
     * overflows there. */
    CHECK(BrFixPackU24Q13(1.0e30f) == 16777215);

    /* NaN takes the "unordered" arm of the low guard in 0x100066E0. */
    CHECK(BrFixPackU24Q13((float)NAN) == 0);
}

static void TestPackRoundingIsHalfUp(void)
{
    /* floor(0.5 + x), so exact halves go UP, not away from zero. This is the
     * same asymmetry documented for BrPackNormalByte in br_vecd.h. */
    CHECK(BrFixPackS16Q7(0.5f / 128.0f) == 1);
    CHECK(BrFixPackS16Q7(-0.5f / 128.0f) == 0);
    CHECK(BrFixPackS16Q7(-1.5f / 128.0f) == -1);
    CHECK(BrFixPackS24Q1(0.25f) == 1);      /* floor(0.5 + 0.5) */
    CHECK(BrFixPackS24Q1(-0.25f) == 0);     /* floor(0.5 - 0.5) */
}

static void TestUnpackSignConventions(void)
{
    /* Negative scales -- the whole point of these two. */
    CHECK(BrFixUnpackS6Q7Neg(0x20) == 0.25f);          /* sext6 = -32 */
    CHECK(BrFixUnpackS6Q7Neg(0x1F) == -31.0f / 128.0f);
    CHECK(BrFixUnpackS6Q7Neg(0x00) == 0.0f);
    /* Bits above the 6-bit field must be ignored, both ways. */
    CHECK(BrFixUnpackS6Q7Neg(0xC1) == BrFixUnpackS6Q7Neg(0x01));

    CHECK(BrFixUnpackS16Q15Neg(0x8000) == 1.0f);
    CHECK(BrFixUnpackS16Q15Neg(0x7FFF) == -32767.0f / 32768.0f);
    CHECK(BrFixUnpackS16Q15Neg(0x00010000) == 0.0f);   /* high half ignored */

    /* Ordinary positive scales. */
    CHECK(BrFixUnpackS8Q3(0x80) == -16.0f);
    CHECK(BrFixUnpackS8Q3(0x7F) == 15.875f);
    CHECK(BrFixUnpackS16Q7(0x8000) == -256.0f);
    CHECK(BrFixUnpackS16Q8(0x8000) == -128.0f);
    CHECK(BrFixUnpackS24Q1(0x800000u) == -4194304.0f);
    CHECK(BrFixUnpackS24Q1(0x7FFFFFu) == 4194303.5f);
    /* bit 24 and above are masked away before the sign test */
    CHECK(BrFixUnpackS24Q1(0xFF7FFFFFu) == 4194303.5f);

    /* 0x10007310 widens through a zero high dword: unsigned, never negative. */
    CHECK(BrFixUnpackU32Q13(0x80000000u) > 0.0f);
    CHECK(BrFixUnpackU32Q13(8192u) == 1.0f);
}

static void TestUnpackTables(void)
{
    CHECK(BrFixUnpackLevel(0) == 0.0f);
    CHECK(BrFixUnpackLevel(1) == 170.0f);
    CHECK(BrFixUnpackLevel(2) == 212.0f);
    CHECK(BrFixUnpackLevel(3) == 255.0f);
    /* the default arm, not a 2-bit lookup */
    CHECK(BrFixUnpackLevel(4) == 255.0f);
    CHECK(BrFixUnpackLevel(255) == 255.0f);
    /* only the low byte participates */
    CHECK(BrFixUnpackLevel(0x100) == 0.0f);

    CHECK(Near(BrFixUnpackU8Angle(0), 0.0, 1e-6));
    CHECK(Near(BrFixUnpackU8Angle(240), 338.4375, 1e-4));
    CHECK(Near(BrFixUnpackU8Angle(0x1F0), 338.4375, 1e-4));  /* masked to 0xFF */

    /* the 6-bit field its caller uses spans exactly [400, 8000] */
    CHECK(Near(BrFixUnpackU8Range(0), 400.0, 1e-3));
    CHECK(Near(BrFixUnpackU8Range(63), 8000.0, 0.01));
    CHECK(BrFixUnpackU8Range(63) > BrFixUnpackU8Range(62));
}

static void TestPackUnpackRoundTrip(void)
{
    /* Each pack/unpack pair must agree to within half a quantisation step. */
    static const float aV[] = { 0.0f, 0.5f, 1.0f, 3.25f, 100.0f, 1999.5f };
    size_t i;

    for (i = 0; i < sizeof aV / sizeof aV[0]; ++i) {
        float v = aV[i];
        CHECK(Near(BrFixUnpackU32Q13((uint32_t)BrFixPackU24Q13(v)), v,
                   1.0 / 8192.0));
        CHECK(Near(BrFixUnpackS24Q1((uint32_t)BrFixPackS24Q1(v)), v, 0.5));
        CHECK(Near(BrFixUnpackS24Q1((uint32_t)BrFixPackS24Q1(-v)), -v, 0.5));
        if (v <= 255.0f) {
            CHECK(Near(BrFixUnpackS16Q7((int32_t)BrFixPackS16Q7(v)), v,
                       1.0 / 128.0));
            CHECK(Near(BrFixUnpackS16Q7((int32_t)BrFixPackS16Q7(-v)), -v,
                       1.0 / 128.0));
        }
    }
}

/* ---------------------------------------------------------------------
 * 2. Car-state packet
 * ------------------------------------------------------------------- */

/* Build the exact field sequence 0x10006EC0 expects. */
static void BuildFullPacket(void)
{
    StreamReset();
    PutBits(0x80, 8);  PutBits(0x00, 8);  PutBits(0x00, 8);  PutBits(0x00, 8);
    PutBits(6400, 17);
    PutBits(0, 17);
    PutBits(0x1000, 15);
    PutBits(0x8000, 16);
    PutBits(0x0100, 16);
    PutBits(0x10, 5);  PutBits(0x01, 5);  PutBits(0x00, 5);
    PutBits(0x08, 4);                       /* f34 */
    PutBits(0x08, 4);                       /* f38 */
    PutBits(0x0F, 4);                       /* angle -> f3C/f40/f44/f48 */
    PutBits(1, 1);  PutBits(0, 1);  PutBits(1, 1);  PutBits(0, 1);
    PutBits(1, 1);                          /* f6C */
    PutBits(0, 1);                          /* f70 */
    PutBits(1, 1);                          /* f74 */
    PutBits(0xFFFF38u, 24);                 /* f78 */
    PutBits(63, 6);                         /* f7C */
    PutBits(1, 2);                          /* f80 */
    PutBits(3, 2);                          /* f84 */
    PutBits(1, 1);  PutBits(0, 1);  PutBits(1, 1);
    PutBits(0, 1);  PutBits(1, 1);  PutBits(1, 1);
}

static void TestDecodeFull(void)
{
    BrCarState  cs;
    BrBitReader r;
    float      *p = (float *)&cs;
    int         i;

    for (i = 0; i < BR_CARSTATE_FLOATS; ++i)
        p[i] = -12345.0f;               /* marker: untouched fields keep it */

    BuildFullPacket();
    StreamOpen(&r);
    BrCarStateDecode(&cs, &r);

    /* Bit budget: 4*8 + 2*17 + 15 + 2*16 + 3*5 + 3*4 + 4*1 + 3*1 + 24 + 6
     * + 2*2 + 6*1 = 187. Any mis-sized or reordered field breaks this. */
    CHECK(ReaderBitsConsumed(&r) == 187);

    CHECK(cs.f00 == 1.0f);              /* 0x80 through the negative scale */
    CHECK(cs.f04 == 0.0f);
    CHECK(cs.f10 == 100.0f);            /* 6400 * 128 / 8192 */
    CHECK(cs.f14 == 0.0f);
    CHECK(cs.f18 == 64.0f);             /* (0x1000 << 1) / 128 */
    CHECK(cs.f1C == -128.0f);           /* sext16(0x8000) / 256 */
    CHECK(cs.f20 == 1.0f);
    CHECK(cs.f24 == 0.0f);              /* written without consuming bits */
    CHECK(cs.f28 == -16.0f);
    CHECK(cs.f2C == 1.0f);
    CHECK(cs.f30 == 0.0f);
    CHECK(cs.f34 == -16.0f);
    CHECK(cs.f38 == 0.25f);             /* negative-scale field */

    /* Angle invariants: the raw value lands in two fields, the +35 wrap in
     * two others, and the wrap is a single conditional subtract. */
    CHECK(cs.f3C == cs.f40);
    CHECK(cs.f44 == cs.f48);
    CHECK(Near(cs.f3C, 338.4375, 1e-4));
    CHECK(Near(cs.f44, 338.4375 + 35.0 - 360.0, 1e-4));
    CHECK(cs.f44 >= 0.0f && cs.f44 < 360.0f);

    CHECK(cs.f4C == 1.0f && cs.f50 == 0.0f && cs.f54 == 1.0f && cs.f58 == 0.0f);

    /* Never written by this routine. */
    CHECK(cs.f5C == -12345.0f);
    CHECK(cs.f60 == -12345.0f);
    CHECK(cs.f64 == -12345.0f);
    CHECK(cs.f68 == -12345.0f);

    /* Two different "true" constants: 128.0f for f6C, 1.0f for f70/f74. */
    CHECK(cs.f6C == 128.0f);
    CHECK(cs.f70 == 0.0f);
    CHECK(cs.f74 == 1.0f);

    CHECK(cs.f78 == -100.0f);           /* sext24(0xFFFF38) * 0.5 */
    CHECK(Near(cs.f7C, 8000.0, 0.01));
    CHECK(cs.f80 == 170.0f);
    CHECK(cs.f84 == 255.0f);
    CHECK(cs.f88 == 128.0f && cs.f8C == 0.0f && cs.f90 == 128.0f);
    CHECK(cs.f94 == 0.0f && cs.f98 == 128.0f && cs.f9C == 128.0f);
}

static void TestDecodeDelta(void)
{
    BrCarState  ref;
    BrCarState  cs;
    BrBitReader r;
    float      *p = (float *)&cs;
    uint32_t    q10, q18, q78;
    int         i;

    memset(&ref, 0, sizeof ref);
    ref.f10 = 100.0f;
    ref.f14 = 1500.25f;
    ref.f18 = -1.0f;
    ref.f78 = -100.0f;

    for (i = 0; i < BR_CARSTATE_FLOATS; ++i)
        p[i] = -12345.0f;

    /* Re-derive what the encoder would have sent for "no change": delta code
     * 0 plus the reference's own low bits. */
    q10 = (uint32_t)BrFixPackU24Q13(ref.f10) >> 7;
    /* BrFixPackS16Q7 is already clamped into s16, so `sar ax,1` is just >>1. */
    q18 = (uint32_t)(BrFixPackS16Q7(ref.f18) >> 1);
    q78 = (uint32_t)BrFixPackS24Q1(ref.f78);

    StreamReset();
    PutBits(0x00, 8);  PutBits(0x00, 8);  PutBits(0x00, 8);  PutBits(0x00, 8);
    PutBits(q10 & 0xFFFu, 14);                                    /* f10 */
    PutBits(((uint32_t)BrFixPackU24Q13(ref.f14) >> 7) & 0xFFFu, 14); /* f14 */
    PutBits(q18 & 0x1FFu, 11);                                    /* f18 */
    PutBits(q78 & 0x7Fu, 9);                                      /* f78 */
    PutBits(0, 6);                                                /* f7C */
    PutBits(0, 2);  PutBits(0, 2);
    PutBits(0, 1);  PutBits(0, 1);  PutBits(0, 1);
    PutBits(0, 1);  PutBits(0, 1);  PutBits(0, 1);

    StreamOpen(&r);
    BrCarStateDecodeDelta(&cs, &ref, &r);

    /* Bit budget: 4*8 + 2*14 + 11 + 9 + 6 + 2*2 + 6*1 = 96, exactly 12 bytes.
     * The delta packet being byte-aligned is a strong cross-check. */
    CHECK(ReaderBitsConsumed(&r) == 96);

    /* Code 0 with matching low bits must reproduce the reference to within
     * the field's own quantisation step. */
    CHECK(Near(cs.f10, ref.f10, 1.0 / 64.0));
    CHECK(Near(cs.f14, ref.f14, 1.0 / 64.0));
    CHECK(Near(cs.f18, ref.f18, 1.0 / 64.0));   /* negative reference */
    CHECK(Near(cs.f78, ref.f78, 0.5));

    /* Fields this routine never touches must still hold the marker. */
    CHECK(cs.f1C == -12345.0f);
    CHECK(cs.f20 == -12345.0f);
    CHECK(cs.f24 == -12345.0f);
    CHECK(cs.f3C == -12345.0f);
    CHECK(cs.f4C == -12345.0f);
    CHECK(cs.f6C == -12345.0f);
    CHECK(cs.f70 == -12345.0f);
    CHECK(cs.f74 == -12345.0f);
}

static void TestDecodeDeltaPageCodes(void)
{
    BrCarState  ref;
    BrCarState  cs;
    BrBitReader r;
    uint32_t    q10;
    float       step = 4096.0f / 8192.0f * 128.0f;  /* one page = 0x1000 << 7 */

    memset(&ref, 0, sizeof ref);
    memset(&cs, 0, sizeof cs);
    ref.f10 = 100.0f;
    q10 = (uint32_t)BrFixPackU24Q13(ref.f10) >> 7;

    /* code 3 means MINUS one page, not plus three. */
    StreamReset();
    PutBits(0, 8); PutBits(0, 8); PutBits(0, 8); PutBits(0, 8);
    PutBits(0x3000u | (q10 & 0xFFFu), 14);
    PutBits(0, 14); PutBits(0, 11); PutBits(0, 9); PutBits(0, 6);
    PutBits(0, 2); PutBits(0, 2);
    PutBits(0, 1); PutBits(0, 1); PutBits(0, 1);
    PutBits(0, 1); PutBits(0, 1); PutBits(0, 1);
    StreamOpen(&r);
    BrCarStateDecodeDelta(&cs, &ref, &r);
    CHECK(Near(cs.f10, ref.f10 - step, 1.0 / 64.0));

    /* code 2 means plus two pages. */
    StreamReset();
    PutBits(0, 8); PutBits(0, 8); PutBits(0, 8); PutBits(0, 8);
    PutBits(0x2000u | (q10 & 0xFFFu), 14);
    PutBits(0, 14); PutBits(0, 11); PutBits(0, 9); PutBits(0, 6);
    PutBits(0, 2); PutBits(0, 2);
    PutBits(0, 1); PutBits(0, 1); PutBits(0, 1);
    PutBits(0, 1); PutBits(0, 1); PutBits(0, 1);
    StreamOpen(&r);
    BrCarStateDecodeDelta(&cs, &ref, &r);
    CHECK(Near(cs.f10, ref.f10 + 2.0f * step, 1.0 / 64.0));
}

static void FillRamp(BrCarState *p, float base)
{
    float *f = (float *)p;
    int    i;

    for (i = 0; i < BR_CARSTATE_FLOATS; ++i)
        f[i] = base + (float)i;
}

static void TestLerp(void)
{
    BrCarState a, b, d;
    float     *pa = (float *)&a, *pb = (float *)&b, *pd = (float *)&d;
    int        i;

    FillRamp(&a, 100.0f);
    FillRamp(&b, 200.0f);
    a.f00 = 0.25f;
    b.f00 = 0.75f;                       /* same sign: no negation */

    /* t = 0 reproduces A everywhere -- except f9C, which is always B's. */
    BrCarStateLerp(&d, 0.0f, &a, &b);
    for (i = 0; i < BR_CARSTATE_FLOATS - 1; ++i)
        CHECK(pd[i] == pa[i]);
    CHECK(d.f9C == b.f9C);
    CHECK(d.f9C != a.f9C);

    /* t = 1 reproduces B everywhere. */
    BrCarStateLerp(&d, 1.0f, &a, &b);
    for (i = 0; i < BR_CARSTATE_FLOATS; ++i)
        CHECK(Near(pd[i], pb[i], 1e-3));

    /* Clamps: below 0 behaves as 0, above 10 behaves as 10, NaN as 0. */
    BrCarStateLerp(&d, -5.0f, &a, &b);
    CHECK(d.f10 == a.f10);
    BrCarStateLerp(&d, 1000.0f, &a, &b);
    CHECK(Near(d.f10, a.f10 + 10.0f * (b.f10 - a.f10), 1e-2));
    BrCarStateLerp(&d, (float)NAN, &a, &b);
    CHECK(d.f10 == a.f10);

    /* Extrapolation past t = 1 is allowed on purpose. */
    BrCarStateLerp(&d, 2.0f, &a, &b);
    CHECK(Near(d.f10, a.f10 + 2.0f * (b.f10 - a.f10), 1e-2));
}

static void TestLerpNegation(void)
{
    BrCarState a, b, d;

    FillRamp(&a, 100.0f);
    FillRamp(&b, 200.0f);

    /* Opposite signs and at least 1.0 apart -> the first four components
     * interpolate toward -B. */
    a.f00 = 1.0f;   a.f04 = 2.0f;   a.f08 = 3.0f;   a.f0C = 4.0f;
    b.f00 = -1.0f;  b.f04 = 5.0f;   b.f08 = 6.0f;   b.f0C = 7.0f;

    BrCarStateLerp(&d, 1.0f, &a, &b);
    CHECK(Near(d.f00, 1.0f, 1e-5));      /* -(-1) */
    CHECK(Near(d.f04, -5.0f, 1e-5));
    CHECK(Near(d.f08, -6.0f, 1e-5));
    CHECK(Near(d.f0C, -7.0f, 1e-5));
    /* the tail is never negated */
    CHECK(Near(d.f10, b.f10, 1e-3));

    /* Opposite signs but LESS than 1.0 apart -> no negation. */
    a.f00 = 0.25f;
    b.f00 = -0.25f;
    BrCarStateLerp(&d, 1.0f, &a, &b);
    CHECK(Near(d.f00, -0.25f, 1e-5));
    CHECK(Near(d.f04, 5.0f, 1e-5));

    /* Same sign, far apart -> no negation either. */
    a.f00 = 0.0f;
    b.f00 = 2.0f;
    BrCarStateLerp(&d, 1.0f, &a, &b);
    CHECK(Near(d.f00, 2.0f, 1e-5));
}

static void TestLerpAliasing(void)
{
    BrCarState a, b, ref, d;
    float     *pd = (float *)&d, *pref = (float *)&ref;
    int        i;

    FillRamp(&a, 100.0f);
    FillRamp(&b, 200.0f);
    a.f00 = 0.25f;
    b.f00 = 0.75f;

    BrCarStateLerp(&ref, 0.5f, &a, &b);

    /* The original writes each element after reading it, so dst may alias
     * either source. */
    d = a;
    BrCarStateLerp(&d, 0.5f, &d, &b);
    for (i = 0; i < BR_CARSTATE_FLOATS; ++i)
        CHECK(pd[i] == pref[i]);

    /* dst == B agrees for every field EXCEPT f9C: the trailing "copy f9C from
     * B" is a self-assignment in that case, so f9C keeps its interpolated
     * value instead of B's. The original has the same asymmetry. */
    d = b;
    BrCarStateLerp(&d, 0.5f, &a, &d);
    for (i = 0; i < BR_CARSTATE_FLOATS - 1; ++i)
        CHECK(pd[i] == pref[i]);
    CHECK(Near(d.f9C, a.f9C + 0.5f * (b.f9C - a.f9C), 1e-3));
    CHECK(d.f9C != pref[BR_CARSTATE_FLOATS - 1]);
}

/* Decode then interpolate against itself must be the identity (bar f9C). */
static void TestDecodeThenLerpIdentity(void)
{
    BrCarState  cs, out;
    BrBitReader r;
    float      *pc = (float *)&cs, *po = (float *)&out;
    int         i;

    memset(&cs, 0, sizeof cs);
    BuildFullPacket();
    StreamOpen(&r);
    BrCarStateDecode(&cs, &r);

    BrCarStateLerp(&out, 0.5f, &cs, &cs);
    for (i = 0; i < BR_CARSTATE_FLOATS; ++i)
        CHECK(po[i] == pc[i]);
}

/* ---------------------------------------------------------------------
 * 3. Player slot table
 * ------------------------------------------------------------------- */

static BrNetState g_net;

static void TestNetReset(void)
{
    int i;
    int locksBefore;

    memset(&g_net, 0, sizeof g_net);
    for (i = 0; i < BR_NET_SLOTS; ++i) {
        g_net.aSlots[i].hMutex = (void *)(size_t)(i + 1);
        g_net.aSlots[i].f004   = 0x11110000 + i;   /* must survive */
        g_net.aSlots[i].f008   = 0x22222222;
        g_net.aSlots[i].f00C[3] = 0x33333333;
        g_net.aSlots[i].f02C   = 0x3F;
        g_net.aSlots[i].f030   = 0x44444444;       /* must survive */
        g_net.aSlots[i].f034   = 0x55555555;       /* must survive */
        g_net.aSlots[i].f038[7] = 0x66666666;
        g_net.aSlots[i].f560   = 0;
        g_net.aSlots[i].f974   = 0x77777777;
    }

    g_lockDepth = g_lockPeak = g_lockCount = g_lockUnbalanced = 0;
    locksBefore = g_lockCount;
    CHECK(BrNetReset(&g_net) == 1);

    /* Every lock taken is released, and the original never nests them. */
    CHECK(g_lockDepth == 0);
    CHECK(g_lockUnbalanced == 0);
    CHECK(g_lockPeak == 1);
    /* 16 slot mutexes + 9 global mutexes. */
    CHECK(g_lockCount - locksBefore == BR_NET_SLOTS + 9);

    for (i = 0; i < BR_NET_SLOTS; ++i) {
        CHECK(g_net.aSlots[i].f008 == 0);
        CHECK(g_net.aSlots[i].f00C[3] == 0);
        CHECK(g_net.aSlots[i].f02C == 0);
        CHECK(g_net.aSlots[i].f038[7] == 0);
        CHECK(g_net.aSlots[i].f974 == 0);
        /* -1 is the empty sentinel, not 0. */
        CHECK(g_net.aSlots[i].f560 == -1);
        /* Deliberately skipped by the original's two stosd runs. */
        CHECK(g_net.aSlots[i].f004 == 0x11110000 + i);
        CHECK(g_net.aSlots[i].f030 == 0x44444444);
        CHECK(g_net.aSlots[i].f034 == 0x55555555);
    }

    CHECK(g_net.f1022AEF8 == -1);
    CHECK(g_net.f10220DD4 == -1);
    CHECK(g_net.f10221318 == -1);
    CHECK(g_net.f1022AF00 == -1);
    CHECK(g_net.f1022AF3C == -1);
    CHECK(g_net.f1022AF08 == 0);
    CHECK(g_net.f1022AAA8 == 0);
    CHECK(g_net.f1022AF20 == 0);
    CHECK(g_net.f106909D8 == 0);
}

static void TestNetAccessors(void)
{
    int32_t v;

    /* The setter's value is the SECOND argument: slot 3 must change and
     * nothing else may. */
    BrNetSlotSetF02C(&g_net, 3, 0x1234);
    v = BrNetSlotGetF02C(&g_net, 3);
    CHECK(v == 0x1234);
    CHECK(g_net.aSlots[3].f02C == 0x1234);
    CHECK(g_net.aSlots[2].f02C == 0);
    CHECK(g_net.aSlots[4].f02C == 0);

    g_net.aSlots[5].f004 = 0x99;
    CHECK(BrNetSlotGetF004(&g_net, 5) == 0x99);

    BrNetSlotSetF02C(&g_net, 3, 0);
}

static void TestNetSlotName(void)
{
    char *p0, *p1;

    strcpy(g_net.aSlots[2].f570, "Bagpuss");
    strcpy(g_net.aSlots[9].f570, "Emily");

    p0 = BrNetSlotName(&g_net, 2);
    CHECK(strcmp(p0, "Bagpuss") == 0);

    /* The buffer is shared, not per-slot: the second call clobbers the first
     * result in place, which is exactly what the original does. */
    p1 = BrNetSlotName(&g_net, 9);
    CHECK(p0 == p1);
    CHECK(strcmp(p1, "Emily") == 0);
    CHECK(strcmp(p0, "Emily") == 0);
}

static void TestNetDropMatching(void)
{
    int i;

    BrNetReset(&g_net);
    for (i = 0; i < BR_NET_SLOTS; ++i) {
        g_net.aSlots[i].f004 = (i < 4) ? 7 : 8;
        g_net.aSlots[i].f02C = 0;
        g_net.aSlots[i].f570[0] = '\0';
    }
    /* Only slots 1 and 3 have a low-6-bit flag set and key 7. */
    g_net.aSlots[1].f02C = 0x01;
    g_net.aSlots[3].f02C = 0x20;
    g_net.aSlots[2].f02C = 0x40;        /* bit 6: outside the 0x3F mask */
    g_net.aSlots[5].f02C = 0x3F;        /* right flags, wrong key */
    strcpy(g_net.aSlots[1].f570, "Yaffle");
    strcpy(g_net.aSlots[3].f570, "Gabriel");

    g_announceCount = 0;
    g_lastAnnounce[0] = '\0';
    BrNetDropMatching(&g_net, 7);

    CHECK(g_announceCount == 2);
    CHECK(g_net.f10221318 == 1);        /* two pushes from -1 */
    CHECK(g_net.a10221288[0] == 1);
    CHECK(g_net.a10221288[1] == 3);
    CHECK(g_net.aSlots[1].f02C == 0);
    CHECK(g_net.aSlots[3].f02C == 0);
    /* untouched: wrong flag bit, wrong key */
    CHECK(g_net.aSlots[2].f02C == 0x40);
    CHECK(g_net.aSlots[5].f02C == 0x3F);

    /* "%%" in the .rdata literal renders as one '%'. */
    CHECK(strcmp(g_lastAnnounce, "%15Gabriel left the game.") == 0);
}

/* ---------------------------------------------------------------------
 * 4. Palette fetch
 * ------------------------------------------------------------------- */

static void TestPalFetch(void)
{
    /* First three records of the table at 0x100B37D0. */
    static const uint8_t aTable[] = {
        0, 127, 0,
        255, 175, 0,
        0, 0, 175,
    };
    uint8_t aOut[3];

    aOut[0] = aOut[1] = aOut[2] = 0xAA;
    BrPalFetch(aTable, 1, aOut);
    /* Straight 3-byte copy, no channel reversal. */
    CHECK(aOut[0] == 255 && aOut[1] == 175 && aOut[2] == 0);

    BrPalFetch(aTable, 2, aOut);
    CHECK(aOut[0] == 0 && aOut[1] == 0 && aOut[2] == 175);

    /* The index is signed and unchecked in the original (its source global is
     * initialised to -1), so negative indices step BACKWARDS by 3 bytes. */
    BrPalFetch(aTable + 3, -1, aOut);
    CHECK(aOut[0] == 0 && aOut[1] == 127 && aOut[2] == 0);
}

int main(void)
{
    TestPackClamps();
    TestPackRoundingIsHalfUp();
    TestUnpackSignConventions();
    TestUnpackTables();
    TestPackUnpackRoundTrip();

    TestDecodeFull();
    TestDecodeDelta();
    TestDecodeDeltaPageCodes();
    TestLerp();
    TestLerpNegation();
    TestLerpAliasing();
    TestDecodeThenLerpIdentity();

    TestNetReset();
    TestNetAccessors();
    TestNetSlotName();
    TestNetDropMatching();

    TestPalFetch();

    printf("%s: %d checks, %d failures\n",
           g_fails ? "FAILED" : "ok", g_checks, g_fails);
    return g_fails != 0;
}
