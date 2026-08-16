/* test_slice8_83.c -- packet 83.
 *
 * The real quaternion/matrix code is LINKED (slice5_62, slice3_42, slice3_44,
 * br_crt, br_mat, br_vec, slice1_09, br_pool -- see build.d/test_slice8_83.deps)
 * so the marshalling is checked against the actual conversions rather than
 * against a stand-in that would agree with anything.  Everything the ADAPTERS
 * forward to is a stand-in defined here, because the point of an adapter test
 * is "did the right callee get the right arguments", which a real callee would
 * only obscure.
 *
 * The assertions are properties, not volume: the sign-extension asymmetry, the
 * two-bits-read / one-bit-written flag asymmetry, the double store of f38, the
 * NaN polarity of all four unordered compares, the conditional on +0x0FF4, and
 * a round trip through both directions.
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "slice8_83.h"
#include "slice2_12.h"
#include "slice2_25.h"
#include "slice3_42.h"
#include "slice3_44.h"
#include "slice4_53.h"
#include "slice5_62.h"

static int g_fail;

#define CHECK(cond, ...)                                                     \
    do {                                                                     \
        if (!(cond)) {                                                       \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                      \
            printf(__VA_ARGS__);                                             \
            printf("\n");                                                    \
            g_fail++;                                                        \
        }                                                                    \
    } while (0)

/* C99 has no _Static_assert; the tree uses the negative-array-size trick. */
typedef char br83_assert_rb_is_0x44[(sizeof(BrRbState) == 0x44) ? 1 : -1];
typedef char br83_assert_mat_is_64 [(sizeof(BrMat4)    == 64)   ? 1 : -1];
typedef char br83_assert_state_a0  [(sizeof(BrCarState) == 0xA0) ? 1 : -1];

/* ==================================================================== */
/* Stand-ins for everything the adapters forward to, plus the handful of  */
/* cross-module symbols the linked objects want.                         */
/* ==================================================================== */

static int      g_nLock, g_nUnlock;
static void    *g_hLast;

void BrNetMutexLock(void *h)   { g_nLock++;   g_hLast = h; }
void BrNetMutexUnlock(void *h) { g_nUnlock++; g_hLast = h; }

/* BrNetSlotPredict / BrNetSlotGetF030 / BrNetSlotName / BrNetDropMatching */
static int      g_nPredict, g_nGetF030, g_nName, g_nDrop;
static int32_t  g_lastSlot;
static BrNetState *g_lastNet;
static void    *g_lastHandle;
static int32_t  g_lastLocal;
static uint32_t g_lastTicks;
static int32_t  g_lastKey;
static char     g_szName[8] = "slot";

int BrNetSlotPredict(BrCarState *pDst, int32_t slot, BrNetState *pNet,
                     void *hGlobal, int32_t localSlot, uint32_t nowTicks)
{
    (void)pDst;
    g_nPredict++;
    g_lastSlot   = slot;
    g_lastNet    = pNet;
    g_lastHandle = hGlobal;
    g_lastLocal  = localSlot;
    g_lastTicks  = nowTicks;
    return 7;
}

int32_t BrNetSlotGetF030(BrNetState *pNet, int32_t slot, uint8_t *pb34,
                         uint8_t *pb35, uint8_t *pb36)
{
    g_nGetF030++;
    g_lastNet  = pNet;
    g_lastSlot = slot;
    *pb34 = 0x11; *pb35 = 0x22; *pb36 = 0x33;
    return 5;
}

char *BrNetSlotName(BrNetState *pNet, int32_t slot)
{
    g_nName++;
    g_lastNet  = pNet;
    g_lastSlot = slot;
    return g_szName;
}

void BrNetDropMatching(BrNetState *pNet, int32_t key)
{
    g_nDrop++;
    g_lastNet = pNet;
    g_lastKey = key;
}

int32_t BrNetSlotGetF02C(BrNetState *pNet, int32_t slot)
{ (void)pNet; (void)slot; return 0; }

/* BrHookSetC */
static int g_nHookSet;
static BrHooks *g_lastHooks;
static void (*g_lastPfn)(void);
void BrHookSetC(BrHooks *pH, void (*pfn)(void))
{ g_nHookSet++; g_lastHooks = pH; g_lastPfn = pfn; }

/* BrRcaFixupArray */
static int g_nFixup, g_lastFixupN;
static const BrRcaFixup *g_lastFixupCtx;
static void *g_lastFixupPv;
void BrRcaFixupArray(const BrRcaFixup *pCtx, void *pv, int count)
{ g_nFixup++; g_lastFixupCtx = pCtx; g_lastFixupPv = pv; g_lastFixupN = count; }

/* BrUiFn1003DFC0 */
static int g_nStartup;
static BrStartupState *g_lastStartup;
static void *g_lastB4DF30;
void BrUiFn1003DFC0(BrStartupState *pState, void *pB4DF30)
{ g_nStartup++; g_lastStartup = pState; g_lastB4DF30 = pB4DF30; }

/* The two screen installers -- kept DISTINCT so the pairing is testable. */
static int g_n296C, g_n2970;
int BrOptOpen296C(BrGameObj *p) { (void)p; g_n296C++; return 1; }
int BrOptOpen2970(BrGameObj *p) { (void)p; g_n2970++; return 1; }

/* BrPhaseLeave_10044970 */
static int g_nLeave;
static BrPhaseCtx *g_lastCtx;
static void *g_lastEntity;
BrPhaseCtx *g_pBrSlice4PhaseCtx = NULL;
int BrPhaseLeave_10044970(BrPhaseCtx *pCtx, void *pEntity)
{ g_nLeave++; g_lastCtx = pCtx; g_lastEntity = pEntity; return 1; }

/* Cross-module symbols the linked objects reference but this test never
 * exercises.  Values are irrelevant; presence is not. */
int32_t g_br0BD3E0;
int32_t g_br094294;
int32_t g_br277B48;
int32_t g_brAD0854;
void   *g_brPB4E2E8;
void BrCarStatePack(BrCarPacked *a, const BrCarState *b)   { (void)a; (void)b; }
void BrCarStateUnpack(BrCarState *a, const BrCarPacked *b) { (void)a; (void)b; }
void BrGbiCall10075330(void *pv)              { (void)pv; }
void BrNetSend4760(void)                      { }
void BrStub8B80_1p(void *p)                   { (void)p; }

/* ==================================================================== */
/* Helpers                                                               */
/* ==================================================================== */

#define CAR_BYTES 0x2B68

static uint8_t g_car[CAR_BYTES];
static uint32_t g_flags;

static void car_stf(unsigned off, float f)  { memcpy(g_car + off, &f, 4); }
static float car_ldf(unsigned off)          { float f; memcpy(&f, g_car + off, 4); return f; }
static void car_st32(unsigned off, uint32_t v) { memcpy(g_car + off, &v, 4); }
static uint32_t car_ld32(unsigned off)      { uint32_t v; memcpy(&v, g_car + off, 4); return v; }

static void car_reset(void)
{
    uint32_t *p = &g_flags;

    memset(g_car, 0, sizeof g_car);
    g_flags = 0;
    memcpy(g_car + 0x29C0, &p, sizeof p);
}

static int bits_equal(float a, float b)
{
    uint32_t x, y;
    memcpy(&x, &a, 4);
    memcpy(&y, &b, 4);
    return x == y;
}

/* ==================================================================== */
/* 1. 0x100695A0 -- argument order and the row-3 translation             */
/* ==================================================================== */

static void test_state_from_matrix(void)
{
    BrMat4     m;
    BrCarState st;
    BrVec4     q;
    int        i, j;

    for (i = 0; i < 4; ++i)
        for (j = 0; j < 4; ++j)
            m.m[i][j] = (i == j) ? 1.0f : 0.0f;
    m.m[3][0] = 3.5f; m.m[3][1] = -2.25f; m.m[3][2] = 11.0f;

    memset(&st, 0, sizeof st);
    BrCarStateFromMatrix83(&st, &m);

    /* Translation is ROW 3, copied verbatim. */
    CHECK(st.f10 == 3.5f && st.f14 == -2.25f && st.f18 == 11.0f,
          "translation %g %g %g", (double)st.f10, (double)st.f14, (double)st.f18);

    /* And the first four floats are exactly what 0x100765E0 produces for the
     * same matrix -- which is what proves the (matrix, dest) argument order
     * was not silently swapped to the tree's usual dest-first convention. */
    memset(&q, 0, sizeof q);
    BrSub100765E0(&m, &q);
    CHECK(bits_equal(st.f00, q.f00) && bits_equal(st.f04, q.f04) &&
          bits_equal(st.f08, q.f08) && bits_equal(st.f0C, q.f0C),
          "quaternion does not match 0x100765E0");

    /* Identity rotation: the scalar dominates and the vector part is zero. */
    CHECK(st.f00 != 0.0f && st.f04 == 0.0f && st.f08 == 0.0f && st.f0C == 0.0f,
          "identity quat %g %g %g %g", (double)st.f00, (double)st.f04,
          (double)st.f08, (double)st.f0C);
}

/* ==================================================================== */
/* 2. 0x100607B0 -- record -> state                                      */
/* ==================================================================== */

static void test_to_state(void)
{
    BrCarState st;
    uint32_t   raw = 0x7F800001u;    /* not a value any float path produces */

    car_reset();

    car_stf(0x1E8, 1.0f); car_stf(0x1EC, 2.0f); car_stf(0x1F0, 3.0f);
    car_stf(0x204, 4.0f); car_stf(0x208, 5.0f); car_stf(0x20C, 6.0f);

    car_st32(0x338, raw);            /* raw dword, must survive bit-exact  */
    car_st32(0x73C, 0xDEADBEEFu);
    car_st32(0x524, 0xFFFFFFFFu);    /* fild dword -> -1.0f (SIGNED)       */
    g_car[0x510] = 0xFFu;            /* movsx      -> -1.0f (SIGNED)       */
    g_car[0x362] = 0xFFu;            /* mov al     -> 255.0f (UNSIGNED)    */
    g_car[0x36D] = 0x80u;            /* mov dl     -> 128.0f (UNSIGNED)    */

    g_flags = 0x00080000u;           /* the HIGH bit of the 0xC0000 pair   */
    car_stf(0xE68, -0.5f);
    car_stf(0xFF4, 42.0f);
    car_st32(0xFA8, 3);
    g_br0BD3E0 = 9;                  /* not the final lap                  */

    memset(&st, 0xAA, sizeof st);
    BrCarRecordToState(&st, g_car);

    CHECK(st.f1C == 1.0f && st.f20 == 2.0f && st.f24 == 3.0f, "velocity");
    CHECK(st.f28 == 4.0f && st.f2C == 5.0f && st.f30 == 6.0f, "angular");

    { uint32_t v; memcpy(&v, &st.f34, 4);
      CHECK(v == raw, "raw dword mangled: %08x", v); }
    { uint32_t v; memcpy(&v, &st.f38, 4);
      CHECK(v == 0xDEADBEEFu, "+0x73C mangled: %08x", v); }

    CHECK(st.f4C == -1.0f, "fild signed: %g", (double)st.f4C);
    CHECK(st.f5C == -1.0f, "movsx byte should be SIGNED: %g", (double)st.f5C);
    CHECK(st.f80 == 255.0f, "+0x362 should be UNSIGNED: %g", (double)st.f80);
    CHECK(st.f6C == 128.0f, "+0x36D should be UNSIGNED: %g", (double)st.f6C);

    /* The reader tests TWO bits; only one of them is ever written back. */
    CHECK(st.f70 == 1.0f, "0x80000 alone must set f70: %g", (double)st.f70);

    CHECK(st.f74 == 1.0f, "E68 < 0 -> f74 = 1: %g", (double)st.f74);
    car_stf(0xE68, 0.5f);
    BrCarRecordToState(&st, g_car);
    CHECK(st.f74 == 0.0f, "E68 > 0 -> f74 = 0: %g", (double)st.f74);

    /* Unordered sets C0, so NaN takes the 1.0f side. */
    car_stf(0xE68, (float)NAN);
    BrCarRecordToState(&st, g_car);
    CHECK(st.f74 == 1.0f, "NaN E68 must take the TRUE side: %g", (double)st.f74);

    CHECK(st.f78 == 42.0f, "off the final lap f78 is the time: %g", (double)st.f78);
    g_br0BD3E0 = 3;                  /* now it IS the final lap            */
    BrCarRecordToState(&st, g_car);
    CHECK(st.f78 == 4188888.0f, "final lap must give the sentinel: %g",
          (double)st.f78);

    /* Both stub names are one function. */
    { BrCarState st2;
      memset(&st2, 0x55, sizeof st2);
      BrSub100607B0(&st2, g_car);
      CHECK(memcmp(&st, &st2, sizeof st) == 0,
            "BrSub100607B0 must be BrCarRecordToState"); }
}

/* ==================================================================== */
/* 3. 0x10060A10 -- state -> record                                      */
/* ==================================================================== */

static void test_from_state(void)
{
    BrCarState st;

    car_reset();
    memset(&st, 0, sizeof st);
    st.f00 = 1.0f;                        /* unit quaternion, no NaNs      */
    st.f10 = 1.5f; st.f14 = 2.5f; st.f18 = 3.5f;
    st.f1C = 7.0f; st.f20 = 8.0f; st.f24 = 9.0f;
    memcpy(&st.f38, "\xEF\xBE\xAD\xDE", 4);
    st.f5C = -1.7f;                       /* ftol truncates toward zero    */
    st.f80 = 255.0f;
    st.f70 = 1.0f;
    st.f74 = 1.0f;
    st.f78 = 12.0f;

    g_flags = 0x00080000u;                /* the bit the writer must NOT touch */

    BrCarRecordFromState(g_car, &st);

    CHECK(car_ldf(0x1DC) == 1.5f && car_ldf(0x1E0) == 2.5f &&
          car_ldf(0x1E4) == 3.5f, "position");
    CHECK(car_ldf(0x1F4) == 1.0f, "quaternion scalar");
    CHECK(car_ldf(0x1E8) == 7.0f && car_ldf(0x1EC) == 8.0f &&
          car_ldf(0x1F0) == 9.0f, "velocity");

    /* f38 lands in TWO places. */
    CHECK(car_ld32(0x73C) == 0xDEADBEEFu && car_ld32(0xB54) == 0xDEADBEEFu,
          "f38 must be stored at BOTH +0x73C and +0xB54: %08x %08x",
          car_ld32(0x73C), car_ld32(0xB54));

    CHECK(g_car[0x510] == 0xFFu, "ftol(-1.7) truncates to -1: %02x", g_car[0x510]);
    CHECK(g_car[0x362] == 0xFFu, "255 -> 0xFF: %02x", g_car[0x362]);

    /* Reads 0xC0000, writes 0x40000: the high bit survives untouched, and
     * the writer must not set it either. */
    CHECK(g_flags == 0x000C0000u,
          "writer must set only 0x40000 and preserve 0x80000: %08x", g_flags);
    g_flags = 0;
    BrCarRecordFromState(g_car, &st);
    CHECK(g_flags == 0x00040000u,
          "writer must set EXACTLY 0x40000, not the 0xC0000 pair: %08x", g_flags);
    g_flags = 0x000C0000u;
    st.f70 = 0.0f;
    BrCarRecordFromState(g_car, &st);
    CHECK(g_flags == 0x00080000u,
          "writer must clear only 0x40000: %08x", g_flags);

    /* NaN is neither < 0 nor > 0, so it takes the CLEAR side -- the same
     * side as zero.  Spelling this as `f70 != 0.0f` would set instead. */
    st.f70 = (float)NAN;
    g_flags = 0xFFFFFFFFu;
    BrCarRecordFromState(g_car, &st);
    CHECK((g_flags & 0x40000u) == 0, "NaN f70 must CLEAR: %08x", g_flags);

    /* +0xE68 is a sign, and the polarity is inverted relative to the flag. */
    st.f74 = 1.0f;
    BrCarRecordFromState(g_car, &st);
    CHECK(car_ldf(0xE68) == -1.0f, "f74 set -> -1.0f: %g", (double)car_ldf(0xE68));
    st.f74 = 0.0f;
    BrCarRecordFromState(g_car, &st);
    CHECK(car_ldf(0xE68) == 1.0f, "f74 clear -> +1.0f: %g", (double)car_ldf(0xE68));

    /* The RB block is mirrored twice, byte for byte. */
    CHECK(memcmp(g_car + 0x278, g_car + 0x1DC, 0x44) == 0, "mirror at +0x278");
    CHECK(memcmp(g_car + 0x2BC, g_car + 0x1DC, 0x44) == 0, "mirror at +0x2BC");

    /* qDot is filled by BrRbQuatDerivative and is not left at zero for a
     * non-zero angular velocity. */
    car_reset();
    memset(&st, 0, sizeof st);
    st.f00 = 1.0f;
    st.f28 = 2.0f;                        /* angVel.x                      */
    BrCarRecordFromState(g_car, &st);
    CHECK(car_ldf(0x1DC + 0x34) != 0.0f || car_ldf(0x1DC + 0x38) != 0.0f,
          "qDot was not computed");
}

static void test_time_gate(void)
{
    BrCarState st;

    memset(&st, 0, sizeof st);
    st.f00 = 1.0f;

    /* No time yet (0.0f is not > 0) -> always take the new one. */
    car_reset(); car_stf(0xFF4, 0.0f); st.f78 = 55.0f;
    BrCarRecordFromState(g_car, &st);
    CHECK(car_ldf(0xFF4) == 55.0f, "empty slot must take: %g", (double)car_ldf(0xFF4));

    /* Ours is 10; 10 + 1000 is strictly better than 5, so keep ours. */
    car_reset(); car_stf(0xFF4, 10.0f); st.f78 = 5.0f;
    BrCarRecordFromState(g_car, &st);
    CHECK(car_ldf(0xFF4) == 10.0f, "10 vs 5 must keep: %g", (double)car_ldf(0xFF4));

    /* Ours is 10; 1010 is not better than 2000, so take theirs. */
    car_reset(); car_stf(0xFF4, 10.0f); st.f78 = 2000.0f;
    BrCarRecordFromState(g_car, &st);
    CHECK(car_ldf(0xFF4) == 2000.0f, "10 vs 2000 must take: %g",
          (double)car_ldf(0xFF4));

    /* Exactly on the boundary: 1010 > 1010 is false, so it takes. */
    car_reset(); car_stf(0xFF4, 10.0f); st.f78 = 1010.0f;
    BrCarRecordFromState(g_car, &st);
    CHECK(car_ldf(0xFF4) == 1010.0f, "boundary must take: %g",
          (double)car_ldf(0xFF4));

    /* Unordered takes the assign side in both comparisons. */
    car_reset(); car_stf(0xFF4, 10.0f); st.f78 = (float)NAN;
    BrCarRecordFromState(g_car, &st);
    CHECK(car_ldf(0xFF4) != car_ldf(0xFF4), "NaN f78 must be taken");

    car_reset(); car_stf(0xFF4, (float)NAN); st.f78 = 3.0f;
    BrCarRecordFromState(g_car, &st);
    CHECK(car_ldf(0xFF4) == 3.0f, "NaN current must be replaced: %g",
          (double)car_ldf(0xFF4));
}

/* ==================================================================== */
/* 4. Round trip through both directions                                 */
/* ==================================================================== */

static void test_round_trip(void)
{
    BrCarState a, b;

    car_reset();
    g_br0BD3E0 = 99;                 /* never the final lap in this record */

    memset(&a, 0, sizeof a);
    a.f00 = 1.0f;
    a.f10 = -4.0f; a.f14 = 5.0f; a.f18 = -6.0f;
    a.f1C = 1.0f;  a.f20 = -2.0f; a.f24 = 3.0f;
    a.f28 = 0.5f;  a.f2C = -0.25f; a.f30 = 0.125f;
    memcpy(&a.f34, "\x01\x02\x03\x04", 4);
    memcpy(&a.f38, "\x05\x06\x07\x08", 4);
    memcpy(&a.f3C, "\x09\x0A\x0B\x0C", 4);
    memcpy(&a.f7C, "\x0D\x0E\x0F\x10", 4);
    a.f4C = -3.0f; a.f50 = 4.0f; a.f54 = -5.0f; a.f58 = 6.0f;
    a.f5C = -7.0f; a.f60 = 8.0f; a.f64 = -9.0f; a.f68 = 10.0f;
    a.f6C = 200.0f;                  /* unsigned on the way back           */
    a.f80 = 1.0f;  a.f84 = 2.0f;  a.f88 = 3.0f;  a.f8C = 4.0f;
    a.f90 = 5.0f;  a.f94 = 6.0f;  a.f98 = 7.0f;  a.f9C = 8.0f;
    a.f78 = 1.0f;                    /* the record has no time yet         */

    BrCarRecordFromState(g_car, &a);
    memset(&b, 0xCC, sizeof b);
    BrCarRecordToState(&b, g_car);

    CHECK(b.f1C == a.f1C && b.f20 == a.f20 && b.f24 == a.f24, "vel round trip");
    CHECK(b.f28 == a.f28 && b.f2C == a.f2C && b.f30 == a.f30, "ang round trip");
    CHECK(bits_equal(b.f34, a.f34) && bits_equal(b.f38, a.f38) &&
          bits_equal(b.f3C, a.f3C) && bits_equal(b.f7C, a.f7C),
          "raw dword round trip");
    CHECK(b.f4C == a.f4C && b.f50 == a.f50 && b.f54 == a.f54 && b.f58 == a.f58,
          "int32 round trip");
    CHECK(b.f5C == a.f5C && b.f60 == a.f60 && b.f64 == a.f64 && b.f68 == a.f68,
          "signed byte round trip");
    CHECK(b.f6C == a.f6C, "unsigned byte round trip: %g", (double)b.f6C);
    CHECK(b.f80 == a.f80 && b.f84 == a.f84 && b.f88 == a.f88 && b.f8C == a.f8C &&
          b.f90 == a.f90 && b.f94 == a.f94 && b.f98 == a.f98 && b.f9C == a.f9C,
          "unsigned byte block round trip");
    CHECK(b.f78 == a.f78, "time round trip: %g", (double)b.f78);

    /* The position survives through the matrix, which is the only part of
     * the round trip that is not a copy. */
    CHECK(fabs((double)(b.f10 - a.f10)) < 1e-4 &&
          fabs((double)(b.f14 - a.f14)) < 1e-4 &&
          fabs((double)(b.f18 - a.f18)) < 1e-4,
          "position round trip: %g %g %g", (double)b.f10, (double)b.f14,
          (double)b.f18);

    /* f70 does NOT round trip when the untouched high bit is set, and that
     * asymmetry is the original's, not this port's. */
    a.f70 = 0.0f;
    g_flags = 0x00080000u;
    BrCarRecordFromState(g_car, &a);
    BrCarRecordToState(&b, g_car);
    CHECK(b.f70 == 1.0f,
          "f70 must NOT round trip through a set 0x80000: %g", (double)b.f70);
}

/* ==================================================================== */
/* 5. BrNetAnnounce                                                      */
/* ==================================================================== */

static void test_announce(void)
{
    char szLong[BR83_ANNOUNCE_MAX + 64];

    g_nLock = g_nUnlock = 0;
    g_brAnnouncePending83 = 0;
    g_hBrAnnounce83 = (void *)&g_nLock;

    BrNetAnnounce("hello");
    CHECK(strcmp(g_aBrAnnounce83, "hello") == 0, "text: '%s'", g_aBrAnnounce83);
    CHECK(g_brAnnouncePending83 == 1, "pending flag");
    CHECK(g_nLock == 1 && g_nUnlock == 1, "lock/unlock %d/%d", g_nLock, g_nUnlock);
    CHECK(g_hLast == (void *)&g_nLock, "the module's own handle is used");

    memset(szLong, 'x', sizeof szLong - 1);
    szLong[sizeof szLong - 1] = '\0';
    BrNetAnnounce(szLong);
    CHECK(strlen(g_aBrAnnounce83) == BR83_ANNOUNCE_MAX - 1,
          "bounded copy: %u", (unsigned)strlen(g_aBrAnnounce83));

    BrNetAnnounce("");
    CHECK(g_aBrAnnounce83[0] == '\0', "empty message clears the buffer");
    CHECK(g_brAnnouncePending83 == 1, "empty message still raises the flag");
}

/* ==================================================================== */
/* 6. Adapters                                                           */
/* ==================================================================== */

static void adapter_dummy(void) { }

static void test_adapters_unbound(void)
{
    BrCarState st;
    unsigned char b0 = 0, b1 = 0, b2 = 0;

    g_pBrNetState83      = NULL;
    g_pBrHooks83         = NULL;
    g_pBrStartupState83  = NULL;
    g_pBrRcaFixup83      = NULL;
    g_pBrSlice4PhaseCtx  = NULL;

    g_nPredict = g_nGetF030 = g_nName = g_nDrop = 0;
    g_nHookSet = g_nFixup = g_nStartup = g_nLeave = 0;

    CHECK(BrNetSlotPredictOrig(&st, 0) == 0, "unbound predict must answer 0");
    CHECK(BrX10005DE0((void *)(intptr_t)0, &b0, &b1, &b2) == 0,
          "unbound getF030 must answer 0");
    CHECK(BrX10005E70((void *)(intptr_t)0) != NULL,
          "unbound name must never be NULL -- the caller strcpy()s it");
    CHECK(BrX10005E70((void *)(intptr_t)0)[0] == '\0', "unbound name is empty");
    BrSub10005FE0(1);
    BrX10034C66(adapter_dummy);
    BrSwapRec24Array(g_car, 3);
    BrExt_1003DFC0();
    BrOptFn10044970(g_car);

    CHECK(g_nPredict == 0 && g_nGetF030 == 0 && g_nName == 0 && g_nDrop == 0 &&
          g_nHookSet == 0 && g_nFixup == 0 && g_nStartup == 0 && g_nLeave == 0,
          "an unbound adapter must not call through");
}

static void test_adapters_bound(void)
{
    BrCarState  st;
    BrNetState  net;
    BrHooks     hooks;
    BrStartupState startup;
    BrRcaFixup  fixup;
    BrPhaseCtx  ctx;
    unsigned char b0 = 0, b1 = 0, b2 = 0;
    int         handle = 0, b4df30 = 0, entity = 0;

    memset(&net, 0, sizeof net);
    memset(&hooks, 0, sizeof hooks);
    memset(&startup, 0, sizeof startup);
    memset(&fixup, 0, sizeof fixup);
    memset(&ctx, 0, sizeof ctx);

    g_pBrNetState83     = &net;
    g_hBrNet1022AF34_83 = &handle;
    g_brLocalSlot83     = 4;
    g_brNowTicks83      = 1234u;
    g_pBrHooks83        = &hooks;
    g_pBrStartupState83 = &startup;
    g_pBrB4DF3083       = &b4df30;
    g_pBrRcaFixup83     = &fixup;
    g_pBrSlice4PhaseCtx = &ctx;

    g_nPredict = g_nGetF030 = g_nName = g_nDrop = 0;
    g_nHookSet = g_nFixup = g_nStartup = g_nLeave = 0;
    g_n296C = g_n2970 = 0;

    CHECK(BrNetSlotPredictOrig(&st, 3) == 7, "bound predict forwards its result");
    CHECK(g_nPredict == 1 && g_lastSlot == 3 && g_lastNet == &net &&
          g_lastHandle == &handle && g_lastLocal == 4 && g_lastTicks == 1234u,
          "predict must supply all four lifted globals");

    CHECK(BrX10005DE0((void *)(intptr_t)5, &b0, &b1, &b2) == 5, "getF030 result");
    CHECK(g_nGetF030 == 1 && g_lastSlot == 5, "the argument is an INDEX");
    CHECK(b0 == 0x11 && b1 == 0x22 && b2 == 0x33, "three out-bytes");

    CHECK(strcmp(BrX10005E70((void *)(intptr_t)2), "slot") == 0, "name result");
    CHECK(g_nName == 1 && g_lastSlot == 2, "name index");

    /* Out of range -- which is what a real host pointer looks like -- is
     * refused rather than indexed 0x978 bytes past the table. */
    g_nName = 0;
    CHECK(BrX10005E70((void *)&net)[0] == '\0', "an out-of-range index is refused");
    CHECK(g_nName == 0, "and does not call through");
    CHECK(BrX10005E70((void *)(intptr_t)BR_NET_SLOTS)[0] == '\0',
          "slot == BR_NET_SLOTS is out of range");
    CHECK(BrX10005E70((void *)(intptr_t)-1)[0] == '\0', "negative is out of range");

    BrSub10005FE0(0xABCDu);
    CHECK(g_nDrop == 1 && g_lastKey == (int32_t)0xABCD && g_lastNet == &net,
          "drop forwards the player id unchanged (it is a key, not an index)");

    BrX10034C66(adapter_dummy);
    CHECK(g_nHookSet == 1 && g_lastHooks == &hooks && g_lastPfn == adapter_dummy,
          "hook set");

    BrSwapRec24Array(g_car, 6);
    CHECK(g_nFixup == 1 && g_lastFixupCtx == &fixup && g_lastFixupPv == g_car &&
          g_lastFixupN == 6, "fixup array");

    BrExt_1003DFC0();
    CHECK(g_nStartup == 1 && g_lastStartup == &startup && g_lastB4DF30 == &b4df30,
          "startup state and the 0x10B4DF30 object");

    BrOptFn10044970(&entity);
    CHECK(g_nLeave == 1 && g_lastCtx == &ctx && g_lastEntity == &entity,
          "phase leave");

    /* The pairing that packet 82 could only call "probable": 0x10043260 and
     * 0x10043330 go to DIFFERENT installers, in this order. */
    BrExt_10043260(NULL);
    CHECK(g_n296C == 1 && g_n2970 == 0, "0x10043260 -> BrOptOpen296C");
    BrExt_10043330(NULL);
    CHECK(g_n296C == 1 && g_n2970 == 1, "0x10043330 -> BrOptOpen2970");
}

int main(void)
{
    test_state_from_matrix();
    test_to_state();
    test_from_state();
    test_time_gate();
    test_round_trip();
    test_announce();
    test_adapters_unbound();
    test_adapters_bound();

    if (g_fail == 0)
        printf("test_slice8_83: all checks passed\n");
    else
        printf("test_slice8_83: %d FAILURES\n", g_fail);
    return g_fail != 0;
}
