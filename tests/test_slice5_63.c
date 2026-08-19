/* test_slice5_63.c -- behaviour tests for pass-63's packet (slice 5).
 *
 * Everything under "Stand-ins" is TEST-ONLY. The packet is almost entirely
 * glue: it forwards to bodies other slices own and it moves globals around.
 * So most of what is worth asserting here is the FORWARDING CONTRACT -- which
 * callee, with which arguments, in which order, under which condition -- plus
 * the arithmetic identities of the one real computation in the packet
 * (0x10074090, the quaternion product).
 *
 * The stand-ins for BrOptSave and BrOptAvailB (slice1_06) are the exception:
 * BrOptSave's stand-in performs the REAL interleave, copied from slice1_06.c,
 * because the thing under test is whether this packet gathers the globals into
 * the right slots of the struct it hands over.
 */
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "slice5_63.h"
#include "slice2_25.h"
#include "slice1_03.h"
#include "br_crt.h"

static int g_nFail = 0;
static int g_nRun  = 0;

#define CHECK(cond, msg)                                            \
    do {                                                            \
        ++g_nRun;                                                   \
        if (!(cond)) {                                              \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, (msg));  \
            ++g_nFail;                                              \
        }                                                           \
    } while (0)

/* ======================================================================
 * Stand-ins: slice1_06 (0x1003E310 / 0x1003F2B0 / 0x1003F320 bodies)
 *
 * Declarations repeated from slice1_06.h for the same reason slice5_63.c
 * repeats them -- slice1_06.h and slice2_25.h cannot be included together.
 * ====================================================================== */
#ifndef SLICE1_06_H
#define BR_OPT_CFG_COUNT     6
#define BR_OPT_SEL_COUNT     7
#define BR_OPT_SCRATCH_COUNT 12

typedef struct BrOptState {
    int32_t aCfg[BR_OPT_CFG_COUNT];
    int32_t aSel[BR_OPT_SEL_COUNT];
} BrOptState;

typedef struct BrOptScratch {
    int32_t a[BR_OPT_SCRATCH_COUNT];
} BrOptScratch;

typedef struct BrOptCaps {
    int32_t  mode;
    int32_t  fForceAvailA;
    int32_t  fLowAlwaysB;
    int32_t  fRebaseB;
    int32_t  fLowAlways;
    int32_t  fAlt;
    uint32_t maskPair;
    uint32_t maskA;
    uint32_t maskAMode;
    uint32_t maskB;
    uint32_t maskBMode6;
    int16_t  maskBDefault;
    int32_t  nAlwaysB;
} BrOptCaps;
#endif

void    BrOptSave(BrOptScratch *pDst, const BrOptState *pSrc);
int32_t BrOptAvailB(const BrOptCaps *pCaps, uint32_t n);

/* The real interleave (slice1_06.c). */
void BrOptSave(BrOptScratch *pDst, const BrOptState *pSrc)
{
    pDst->a[0]  = pSrc->aCfg[0];
    pDst->a[1]  = pSrc->aSel[0];
    pDst->a[2]  = pSrc->aSel[2];
    pDst->a[3]  = pSrc->aCfg[1];
    pDst->a[4]  = pSrc->aCfg[2];
    pDst->a[5]  = pSrc->aCfg[3];
    pDst->a[6]  = pSrc->aSel[3];
    pDst->a[7]  = pSrc->aCfg[4];
    pDst->a[8]  = pSrc->aSel[4];
    pDst->a[9]  = pSrc->aSel[5];
    pDst->a[10] = pSrc->aCfg[5];
    pDst->a[11] = pSrc->aSel[6];
}

/* Availability is driven from a bitmap the tests set, so BrExt_1003E510's
 * search behaviour can be steered exactly. Also records the caps it saw so
 * BrSub1003F320's gathering can be checked. */
static uint32_t  g_availB   = 0xFFFFFFFFu;   /* bit n => index n available */
static BrOptCaps g_lastCaps;
static int       g_capsSeen = 0;
static uint32_t  g_lastN    = 0;

int32_t BrOptAvailB(const BrOptCaps *pCaps, uint32_t n)
{
    g_lastCaps = *pCaps;
    g_capsSeen = 1;
    g_lastN    = n;
    return (n < 32u && (g_availB & (1u << n)) != 0u) ? (int32_t)(1u << n) : 0;
}

/* 0x1003F2B0 lives outside this packet too; same idea. `g_availAafter` is an
 * alternative mode: reject the first N calls, then accept, so the search can
 * be watched across a wrap. */
static uint32_t g_availA = 0xFFFFFFFFu;
static int      g_availAafter = -1;
static int      g_nAvailA;
static int      g_lastAvailAidx;
int BrSub1003F2B0(int index)
{
    ++g_nAvailA;
    g_lastAvailAidx = index;
    if (g_availAafter >= 0) {
        return g_nAvailA > g_availAafter;
    }
    return (index >= 0 && index < 32 && (g_availA & (1u << index)) != 0u);
}

/* ======================================================================
 * Stand-ins: globals other slices own
 * ====================================================================== */
int32_t g_br0AC648, g_br0AC64C, g_br0AC650, g_br0AC654, g_br0AC658, g_br0AC65C;
int32_t g_br0BD3E0;
int32_t g_brAA2A00, g_brAA2A08, g_brAA2A0C, g_brAA2A18;
int32_t g_brAA2A1C, g_brAA2A20, g_brAA2A24, g_brAA2A28;
int32_t g_brB4E708, g_brB4E70C;
int32_t g_br094350, g_br094354, g_br094358, g_br09435C;
int32_t g_br0B380C, g_br22B34C, g_br22B350;
int32_t g_brB4E1D0, g_brB4E1D8, g_brB4E1DC, g_brB4E1E0, g_brB4E728, g_brB4E7A0;
void   *g_brB4E1D4;
int32_t g_br0AA010, g_brAA289C, g_brAA28FC, g_brAD0984;
int32_t g_brA9CFFC, g_brA9D000, g_brAA287C;
int32_t g_aBrAA26F0[BR_OPT_AA26F0_COUNT];
unsigned char g_aBrB4DF30[BR_OPT_B4DF30_COUNT][BR_OPT_B4DF30_STRIDE];

BrOptObj *g_brPAA2904, *g_brPAA2948;
BrObj29D4 *g_brPAA29D4;
BrDPlay   *g_brP277B40;
void      *g_brP680584;

int       g_brCdEnabled;
int32_t   BrG_6C65E4;
void     *BrG_6C2CF8;
uint32_t  g_brA9BFDC;

/* The DLL's read-only tables. Values are arbitrary but distinct so the tests
 * can tell which slot was read. */
const int32_t g_aBrAC420[32] = {
    1000,1001,1002,1003,1004,1005,1006,1007,1008,1009,1010,1011,
    1012,1013,1014,1015,1016,1017,1018,1019,1020,1021,1022,1023,
    1024,1025,1026,1027,1028,1029,1030,1031
};
const int32_t g_aBrAC4A0[4]  = { 2000, 2001, 2002, 2003 };
const int32_t g_aBrAC4B0[4]  = { 3000, 3001, 3002, 3003 };
const int32_t g_aBrAC4C0[6]  = { 4000, 4001, 4002, 4003, 4004, 4005 };
const int32_t g_aBrAC4D8[16] = {
    5000,5001,5002,5003,5004,5005,5006,5007,
    5008,5009,5010,5011,5012,5013,5014,5015
};
const int32_t g_aBrAC518[2]  = { 6000, 6001 };

/* 0x100B3820: 2-byte records. Record k holds { 10+k, 50+k }, 64 of them. */
#define R1(k)  (uint8_t)(10 + (k)), (uint8_t)(50 + (k))
#define R8(b)  R1((b)+0), R1((b)+1), R1((b)+2), R1((b)+3), \
               R1((b)+4), R1((b)+5), R1((b)+6), R1((b)+7)
const uint8_t g_aBr0B3820[] = {
    R8(0), R8(8), R8(16), R8(24), R8(32), R8(40), R8(48), R8(56)
};

static const char g_fmt[] = "%d";
const char *g_pszBr0A73C4 = g_fmt;

/* ======================================================================
 * Stand-ins: functions other slices own
 * ====================================================================== */
static int g_seq = 0;

static int g_nE3A0, g_seqE3A0;
void BrSub1003E3A0(void) { ++g_nE3A0; g_seqE3A0 = ++g_seq; }

static int g_n44540;
void BrSub10044540(void) { ++g_n44540; }

static int g_n5FCF0, g_seq5FCF0;
void BrSub1005FCF0(void) { ++g_n5FCF0; g_seq5FCF0 = ++g_seq; }

static int g_nC020, g_seqC020;
void BrSub1003C020(void) { ++g_nC020; g_seqC020 = ++g_seq; }

static int   g_nCC70, g_seqCC70;
static void *g_argCC70;
void BrSub1003CC70(void *p) { ++g_nCC70; g_seqCC70 = ++g_seq; g_argCC70 = p; }

static int g_n2870, g_n27F0, g_lastTrack;
void BrSub10002870(int track) { ++g_n2870; g_lastTrack = track; }
void BrSub100027F0(int track) { ++g_n27F0; g_lastTrack = track; }

/* No stand-in for 0x1007A840: the Glide reference build has no such function
 * and 0x10058F90 does not call it.  A stub here would let the gate creep back
 * in unnoticed. */
static int g_ret7A940, g_n7A940;
int BrSub1007A940(void) { ++g_n7A940; return g_ret7A940; }

/* Variadic in the original; the two call sites pass one int. */
static char g_sprintfLast[64];
static int  g_nSprintf;
int BrSprintf(char *pszDest, const char *pszFmt, ...)
{
    va_list ap;
    int n;
    va_start(ap, pszFmt);
    n = vsprintf(pszDest, pszFmt, ap);
    va_end(ap);
    ++g_nSprintf;
    strncpy(g_sprintfLast, pszDest, sizeof g_sprintfLast - 1);
    g_sprintfLast[sizeof g_sprintfLast - 1] = '\0';
    return n;
}

/* Distinct pointer per string id, so the id can be recovered. */
static char g_strPool[512];
const char *BrStrGet(int id)
{
    return (id >= 0 && id < (int)sizeof g_strPool) ? &g_strPool[id] : NULL;
}
static int Br63StrId(const char *psz)
{
    return (psz == NULL) ? -1 : (int)(psz - g_strPool);
}

/* operator new: a small arena so allocation failure can be simulated. */
static unsigned char g_arena[512];
static int  g_arenaUsed;
static int  g_newFails;
void *BrOperatorNew(uint32_t cb)
{
    if (g_newFails || g_arenaUsed + (int)cb > (int)sizeof g_arena) {
        return NULL;
    }
    g_arenaUsed += (int)cb;
    return &g_arena[g_arenaUsed - (int)cb];
}
void BrOperatorDelete(void *p) { (void)p; }

static const BrOptObjVtbl g_vtbl = {   /* nine slots (br_phase.h); none is driven by this test */
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};
static int g_nCtor;
BrOptObj *BrOptObjCtor(BrOptObj *pThis)
{
    ++g_nCtor;
    memset(pThis, 0, sizeof *pThis);
    pThis->pVtbl = &g_vtbl;
    return pThis;
}

static int       g_nFn56FF0, g_seqFn56FF0;
static BrOptObj *g_thisFn56FF0;
void BrOptFn10056FF0(BrOptObj *pThis)
{
    ++g_nFn56FF0;
    g_seqFn56FF0  = ++g_seq;
    g_thisFn56FF0 = pThis;
}

/* --- slice1_03 -------------------------------------------------------- */
static BrTextState g_text;
BrTextState *BrTextGetState(void) { return &g_text; }
extern signed char g_br4B035C;

typedef struct TimeEntry {
    int   idLabel;
    const char *pszPrefix;
    float f;
    int   x, y;
} TimeEntry;
static TimeEntry g_aTime[8];
static int       g_nTime;
void BrHudDrawTimeEntry(const char *pszLabel, const char *pszPrefix,
                        float fSeconds, int x, int y)
{
    if (g_nTime < (int)(sizeof g_aTime / sizeof g_aTime[0])) {
        g_aTime[g_nTime].idLabel   = Br63StrId(pszLabel);
        g_aTime[g_nTime].pszPrefix = pszPrefix;
        g_aTime[g_nTime].f         = fSeconds;
        g_aTime[g_nTime].x         = x;
        g_aTime[g_nTime].y         = y;
    }
    ++g_nTime;
}

/* --- slice2_15 -------------------------------------------------------- */
static BrScreenInfo g_screen;
BrScreenInfo *BrScreenGet(void) { return &g_screen; }

static BrHudEnv g_env;
BrHudEnv *BrHudGetEnv(void) { return &g_env; }

static BrGfxCmd g_dl[64];
static BrGfxOut g_out;
BrGfxOut *BrGfxGetOut(void) { return &g_out; }

static int g_n19290;
void BrSub_10019290(void) { ++g_n19290; }

/* ======================================================================
 * 0x10074090 -- quaternion product
 * ====================================================================== */

static BrVec4 Q(float a, float b, float c, float d)
{
    BrVec4 q;
    q.f00 = a; q.f04 = b; q.f08 = c; q.f0C = d;
    return q;
}

static int QNear(const BrVec4 *p, const BrVec4 *q, float tol)
{
    return fabsf(p->f00 - q->f00) <= tol && fabsf(p->f04 - q->f04) <= tol
        && fabsf(p->f08 - q->f08) <= tol && fabsf(p->f0C - q->f0C) <= tol;
}

static float QNorm(const BrVec4 *p)
{
    return sqrtf(p->f00 * p->f00 + p->f04 * p->f04
               + p->f08 * p->f08 + p->f0C * p->f0C);
}

static void TestQuat(void)
{
    const BrVec4 id = Q(1.0f, 0.0f, 0.0f, 0.0f);
    BrVec4 a = Q(0.3f, -1.7f, 2.5f, 0.9f);
    BrVec4 b = Q(-2.1f, 0.4f, 1.1f, -0.6f);
    BrVec4 c = Q(1.3f, 2.2f, -0.7f, 0.5f);
    BrVec4 r, s, t, u;

    /* Element 0 is the scalar: the identity is (1,0,0,0) on BOTH sides. */
    BrSub10074090(&r, &a, &id);
    CHECK(QNear(&r, &a, 1e-5f), "q * 1 == q");
    BrSub10074090(&r, &id, &a);
    CHECK(QNear(&r, &a, 1e-5f), "1 * q == q");

    /* |ab| == |a||b| -- true of the Hamilton product, false of most
     * sign-flipped near-misses. */
    BrSub10074090(&r, &a, &b);
    CHECK(fabsf(QNorm(&r) - QNorm(&a) * QNorm(&b)) < 1e-3f,
          "norm is multiplicative");

    /* q * conj(q) == (|q|^2, 0, 0, 0). Pins down WHICH element is scalar. */
    {
        BrVec4 conj = Q(a.f00, -a.f04, -a.f08, -a.f0C);
        BrSub10074090(&r, &a, &conj);
        CHECK(fabsf(r.f00 - QNorm(&a) * QNorm(&a)) < 1e-3f,
              "q*conj(q) scalar part is |q|^2");
        CHECK(fabsf(r.f04) < 1e-4f && fabsf(r.f08) < 1e-4f
              && fabsf(r.f0C) < 1e-4f, "q*conj(q) vector part is zero");
    }

    /* Associative, and NOT commutative. */
    BrSub10074090(&r, &a, &b);
    BrSub10074090(&s, &r, &c);        /* (ab)c */
    BrSub10074090(&t, &b, &c);
    BrSub10074090(&u, &a, &t);        /* a(bc) */
    CHECK(QNear(&s, &u, 1e-3f), "product is associative");

    BrSub10074090(&r, &a, &b);
    BrSub10074090(&s, &b, &a);
    CHECK(!QNear(&r, &s, 1e-3f), "product is NOT commutative (order matters)");

    /* Aliasing: the original loads both inputs before the first store. */
    BrSub10074090(&r, &a, &b);
    s = a;
    BrSub10074090(&s, &s, &b);
    CHECK(QNear(&r, &s, 0.0f), "dst may alias arg 2");
    t = b;
    BrSub10074090(&t, &a, &t);
    CHECK(QNear(&r, &t, 0.0f), "dst may alias arg 3");
    s = a;
    BrSub10074090(&s, &s, &s);
    BrSub10074090(&r, &a, &a);
    CHECK(QNear(&r, &s, 0.0f), "dst may alias both");
}

/* ======================================================================
 * 0x1003E310 -- the gather half of the scratch save
 * ====================================================================== */
static void TestOptSave(void)
{
    int i;

    g_br0AC648 = 100; g_br0AC64C = 101; g_br0AC650 = 102;
    g_br0AC654 = 103; g_br0AC658 = 104; g_br0AC65C = 105;
    g_brAA2A00 = 200; g_brAA2A08 = 202; g_brAA2A0C = 203;
    g_brAA2A10 = 204; g_brAA2A14 = 205; g_brAA2A18 = 206;

    for (i = 0; i < BR63_SCRATCH_COUNT; ++i) {
        g_aBrB4E710[i] = -1;
    }
    BrSub1003E310();

    /* cfg0 sel0 sel2 cfg1 cfg2 cfg3 sel3 cfg4 sel4 sel5 cfg5 sel6 */
    CHECK(g_aBrB4E710[0]  == 100, "scratch[0]  <- 0x100AC648");
    CHECK(g_aBrB4E710[1]  == 200, "scratch[1]  <- 0x10AA2A00");
    CHECK(g_aBrB4E710[2]  == 202, "scratch[2]  <- 0x10AA2A08");
    CHECK(g_aBrB4E710[3]  == 101, "scratch[3]  <- 0x100AC64C");
    CHECK(g_aBrB4E710[4]  == 102, "scratch[4]  <- 0x100AC650");
    CHECK(g_aBrB4E710[5]  == 103, "scratch[5]  <- 0x100AC654");
    CHECK(g_aBrB4E710[6]  == 203, "scratch[6]  <- 0x10AA2A0C");
    CHECK(g_aBrB4E710[7]  == 104, "scratch[7]  <- 0x100AC658");
    CHECK(g_aBrB4E710[8]  == 204, "scratch[8]  <- 0x10AA2A10");
    CHECK(g_aBrB4E710[9]  == 205, "scratch[9]  <- 0x10AA2A14");
    CHECK(g_aBrB4E710[10] == 105, "scratch[10] <- 0x100AC65C");
    CHECK(g_aBrB4E710[11] == 206, "scratch[11] <- 0x10AA2A18");
}

/* ======================================================================
 * 0x1003F320 -- the caps gather
 * ====================================================================== */
static void TestAvailAdapter(void)
{
    int r;

    g_br0AA010 = 6; g_brAA28F8 = 7; g_brAA28FC = 8; g_brAA28F4 = 9;
    g_brAA28F0 = 10; g_brAA289C = 11; g_brAA27E0 = 0xDEAD1234u;
    g_brA9D010 = 0x11u; g_br0AB3EC = 0x22u; g_brAA2598 = 0x33u;
    g_br0AB3E8 = 0x44u; g_br0AB3E4 = (int16_t)-5; g_brAD0984 = 12;

    g_capsSeen = 0;
    g_availB   = 1u << 3;
    r = BrSub1003F320(3);

    CHECK(g_capsSeen, "0x1003F320 reaches BrOptAvailB");
    CHECK(g_lastN == 3u, "index is passed through unchanged");
    CHECK(r == (int)(1u << 3), "the MASKED BIT is returned, not 0/1");
    CHECK(g_lastCaps.mode == 6 && g_lastCaps.fForceAvailA == 7
          && g_lastCaps.fLowAlwaysB == 8 && g_lastCaps.fRebaseB == 9
          && g_lastCaps.fLowAlways == 10 && g_lastCaps.fAlt == 11,
          "scalar caps gathered in the right slots");
    CHECK(g_lastCaps.maskPair == 0xDEAD1234u,
          "0x10AA27E0 is handed over as ONE dword (both halves)");
    CHECK(g_lastCaps.maskA == 0x11u && g_lastCaps.maskAMode == 0x22u
          && g_lastCaps.maskB == 0x33u && g_lastCaps.maskBMode6 == 0x44u
          && g_lastCaps.maskBDefault == (int16_t)-5
          && g_lastCaps.nAlwaysB == 12, "masks gathered in the right slots");

    /* A negative index must survive as a negative int32 (BrOptAvailB compares
     * signed), not be clamped here. */
    (void)BrSub1003F320(-1);
    CHECK((int32_t)g_lastN == -1, "negative index survives the adapter");
}

/* ======================================================================
 * 0x1003E510
 * ====================================================================== */
static void ResetE510(void)
{
    g_seq = 0;
    g_nE3A0 = g_n44540 = g_n5FCF0 = 0;
    g_br0AA010 = 1;
    g_br0AC64C = 1; g_br0AC650 = 2; g_brAA2A08 = 1; g_brAA2A00 = 3;
    g_br0AC658 = 42; g_br0AC65C = 77;
    g_brAA28FC = 0;
    g_availA = 0xFFFFFFFFu;
    g_availB = 0xFFFFFFFFu;
    g_availAafter = -1;
    g_nAvailA = 0;
}

static void TestE510(void)
{
    /* --- ordering and the mode-6 side call ---------------------------- */
    ResetE510();
    g_br0AC654 = 5; g_br0AC648 = 2;
    BrExt_1003E510();
    CHECK(g_nE3A0 == 1 && g_seqE3A0 == 1, "0x1003E3A0 runs first");
    CHECK(g_n44540 == 0, "0x10044540 is skipped unless mode == 6");
    CHECK(g_n5FCF0 == 1 && g_seq5FCF0 > g_seqE3A0, "0x1005FCF0 runs last");
    CHECK(g_br094350 == 77, "0x10094350 <- 0x100AC65C");
    CHECK(g_br22B34C == g_aBrAC420[5],  "0x1022B34C <- table[0x100AC654]");
    CHECK(g_br09435C == g_aBrAC4A0[1],  "0x1009435C <- table[0x100AC64C]");
    CHECK(g_br094358 == g_aBrAC4B0[2],  "0x10094358 <- table[0x100AC650]");
    CHECK(g_br094354 == g_aBrAC518[1],  "0x10094354 <- table[0x10AA2A08]");
    CHECK(g_br0B380C == g_aBrAC4D8[2],  "0x100B380C <- table[0x100AC648]");
    CHECK(g_br22B350 == g_aBrAC4C0[3],  "0x1022B350 <- table[0x10AA2A00]");
    CHECK(g_br0BD3E0 == 42, "0x100BD3E0 <- 0x100AC658");

    ResetE510();
    g_br0AA010 = 6;
    g_br0AC654 = 0; g_br0AC648 = 0;
    BrExt_1003E510();
    CHECK(g_n44540 == 1, "0x10044540 runs when mode == 6");

    /* --- the track search wraps at 0x1F ------------------------------- */
    ResetE510();
    g_br0AC654 = 0x1F;
    g_availB = 1u << 3;          /* only index 3 is selectable */
    g_br0AC648 = 0;
    BrExt_1003E510();
    CHECK(g_br0AC654 == 3, "the track search wraps 0x1F -> 0 and lands on 3");

    /* --- a full circle gives up on the index it started from ---------- */
    ResetE510();
    g_br0AC654 = 7;
    g_availB = 0;                /* nothing is selectable */
    g_br0AC648 = 0;
    BrExt_1003E510();
    CHECK(g_br0AC654 == 7, "a full circle leaves the original index in place");
    CHECK(g_br22B34C == g_aBrAC420[7],
          "...and that rejected index is used anyway");

    /* --- 0x100AC648's bound is 11 normally, 14 with 0x10AA28FC -------- */
    ResetE510();
    g_br0AC654 = 0;
    g_br0AC648 = 11;
    g_availA = 1u << 12;         /* only index 12 is selectable */
    g_brAA28FC = 0;              /* bound 11: 12 is out of range */
    BrExt_1003E510();
    CHECK(g_br0AC648 == 11,
          "bound 11: index 12 is unreachable, the search circles back");

    ResetE510();
    g_br0AC654 = 0;
    g_br0AC648 = 11;
    g_availA = 1u << 12;
    g_brAA28FC = 1;              /* bound 14: 12 is reachable */
    BrExt_1003E510();
    CHECK(g_br0AC648 == 12, "bound 14: index 12 is reachable");

    /* --- the two searches give up DIFFERENTLY -------------------------
     * The 0x100AC654 loop's wrap falls through into the "back where we
     * started" test; the 0x100AC648 loop's wrap jumps past it. So starting
     * from index 0, the second search must NOT stop when it wraps to 0. */
    ResetE510();
    g_br0AC654 = 0;
    g_br0AC648 = 0;
    g_availAafter = 13;   /* reject calls 1..13, accept from call 14 */
    BrExt_1003E510();
    CHECK(g_nAvailA == 14,
          "the 0x100AC648 search runs past its wrap back onto index 0");
    CHECK(g_br0AC648 == 1,
          "...and lands on index 1 on the second lap, not on 0 at the wrap");

    /* The 0x100AC654 loop, by contrast, DOES stop at its wrap onto 0 -- if
     * it did not, this call would never return. */
    ResetE510();
    g_br0AC654 = 0;
    g_br0AC648 = 0;
    g_availB = 0;         /* nothing selectable at all */
    BrExt_1003E510();
    CHECK(g_br0AC654 == 0,
          "the 0x100AC654 search terminates at its wrap onto index 0");

    /* --- mode 0 takes the 0x100B3820 table instead ------------------- */
    {
        uint32_t idx;
        ResetE510();
        g_br0AA010     = 0;
        g_br0AC654     = 0;
        g_aBrAA26F4[0] = 2;
        g_aBrAA26F4[1] = 5;
        idx = 5u + 12u * 2u;     /* == 29 */
        BrExt_1003E510();
        CHECK(g_br0B380C == (int32_t)g_aBr0B3820[idx * 2],
              "mode 0: 0x100B380C <- record byte 0");
        CHECK(g_br22B350 == (int32_t)g_aBr0B3820[idx * 2 + 1],
              "mode 0: 0x1022B350 <- record byte 1");
        CHECK(g_n5FCF0 == 1, "mode 0 still ends in 0x1005FCF0");
    }
}

/* ======================================================================
 * 0x10031688
 * ====================================================================== */
static void TestFillRect(void)
{
    uint32_t colour;

    g_out.pCur = g_dl;
    BrG_6C65E4 = 0;
    memset(g_dl, 0, sizeof g_dl);

    BrSub_10031688(10, 20, 30, 40, 0xFF, 0x80, 0x40);

    CHECK(g_out.pCur == g_dl + 7, "seven commands are emitted");
    CHECK(g_dl[0].w0 == 0xE7000000u && g_dl[0].w1 == 0u, "cmd 0 pipe sync");
    CHECK(g_dl[1].w0 == 0xB900031Du && g_dl[1].w1 == 0x0F0A4000u, "cmd 1");
    CHECK(g_dl[2].w0 == 0xBA001402u && g_dl[2].w1 == 0x00300000u, "cmd 2");

    colour = ((0xFFu << 8) & 0xF800u) | ((0x80u << 3) & 0x7C0u)
           | ((0x40u >> 2) & 0x3Eu) | 1u;
    CHECK(g_dl[3].w0 == 0xF7000000u, "cmd 3 is set-fill-colour");
    CHECK(g_dl[3].w1 == (colour | (colour << 16)),
          "the 16-bit fill colour is duplicated into both halves");
    CHECK((g_dl[3].w1 & 1u) == 1u, "bit 0 of the fill colour is forced on");

    /* Integer corners, lower-right inclusive (minus one). */
    CHECK(g_dl[4].w0 == (0xE1000000u | ((10u + 30u - 1u) << 12) | (20u + 40u - 1u)),
          "cmd 4 lower-right = x+w-1, y+h-1");
    CHECK(g_dl[4].w1 == ((10u << 12) | 20u), "cmd 4 upper-left = x, y");

    CHECK(g_dl[5].w0 == 0xE7000000u && g_dl[5].w1 == 0u, "cmd 5 pipe sync");
    CHECK(g_dl[6].w0 == 0xBA001402u && g_dl[6].w1 == 0u,
          "cmd 6 restores cycle type 0 (payload 0, not 0x00300000)");

    /* The asymmetric double scale. */
    g_out.pCur = g_dl;
    BrG_6C65E4 = 1;
    BrSub_10031688(10, 20, 30, 40, 0, 0, 0);
    CHECK(g_dl[4].w1 == ((20u << 12) | 40u),
          "hi-res: upper-left is doubled ONCE");
    CHECK(g_dl[4].w0 == (0xE1000000u | ((((20u + 60u) << 1) - 1u) << 12)
                                     | (((40u + 80u) << 1) - 1u)),
          "hi-res: lower-right is doubled TWICE (the original's asymmetry)");

    /* c2's shift is arithmetic: a negative c2 must not read as a huge value. */
    g_out.pCur = g_dl;
    BrG_6C65E4 = 0;
    BrSub_10031688(0, 0, 0, 0, 0, 0, -4);
    CHECK((g_dl[3].w1 & 0x3Eu) == ((uint32_t)(-4 >> 2) & 0x3Eu),
          "c2 >> 2 is an arithmetic shift");
}

/* ======================================================================
 * 0x100027C0, 0x1007AC00, the text pokes, 0x1003D130
 * ====================================================================== */
static void TestSmall(void)
{
    unsigned char desc[0x100];
    int32_t zero;

    /* --- CD dispatch: the test is == 1, not != 0 ---------------------- */
    g_n2870 = g_n27F0 = 0;
    g_brCdEnabled = 1;  BrCdTrackPlay(4);
    CHECK(g_n2870 == 1 && g_n27F0 == 0 && g_lastTrack == 4,
          "g_brCdEnabled == 1 selects 0x10002870");
    g_brCdEnabled = 0;  BrCdTrackPlay(5);
    CHECK(g_n27F0 == 1 && g_lastTrack == 5, "0 selects 0x100027F0");
    g_brCdEnabled = 2;  BrCdTrackPlay(6);
    CHECK(g_n27F0 == 2 && g_n2870 == 1,
          "2 also selects 0x100027F0 -- the test is ==1, not !=0");

    /* --- 0x10058F90: NO GATE ------------------------------------------
     * These two assertions replace a pair that asserted the OPPOSITE -- that
     * 0x1007A940 is skipped when the D3D-only enumerator 0x1007A840 returns
     * zero.  That is the D3D build's behaviour (0x1007AC00, 22 bytes); the
     * Glide reference is 0x10058F90, twelve bytes and five instructions, with
     * no gate at all.  The old test passed because the port had transcribed
     * the wrong build, which is exactly the shape CONVENTIONS.md warns about:
     * a test written to pin a reading converts it into a regression guard
     * defending itself.
     *
     * The property asserted is "the callee runs EVERY time, whatever it
     * returns", which is what the absence of a branch means.  Both return
     * values are exercised because a reinstated gate could test either
     * polarity. */
    g_n7A940 = 0;
    g_ret7A940 = 0;
    BrExt_1007AC00();
    CHECK(g_n7A940 == 1,
          "0x10058F90 calls 0x10058E20 unconditionally (returns 0)");
    g_ret7A940 = 7;
    BrExt_1007AC00();
    CHECK(g_n7A940 == 2,
          "0x10058F90 calls 0x10058E20 unconditionally (returns non-zero)");

    /* --- text pokes --------------------------------------------------- */
    g_text.align = 9; g_text.scale = 9; g_br4B0358 = 9;
    BrSub_10019260();
    CHECK(g_br4B0358 == 0 && g_text.align == 9 && g_text.scale == 9,
          "0x10019260 touches 0x104B0358 only");
    BrSub_10019270();
    CHECK(g_text.align == BR_TEXT_ALIGN_CENTER, "0x10019270 -> centre");
    BrSub_10019280();
    CHECK(g_br4B035C == BR_TEXT_ALIGN_LEFT, "0x10019280 -> left");
    BrSub_100192F0(0x14);
    CHECK(g_text.scale == 0x14, "0x100192F0 -> scale");

    /* --- 0x1003D130: the <= 1 guard ----------------------------------- */
    memset(desc, 0xCC, sizeof desc);
    strcpy(g_aBrA9D018, "");
    BrSub1003D130(desc);
    memcpy(&zero, desc + BR63_DESC_ZERO_OFF, sizeof zero);
    CHECK(desc[0] == 0xCC, "empty name: nothing is copied");
    CHECK(zero == 0, "empty name: +0xC8 is still zeroed");

    memset(desc, 0xCC, sizeof desc);
    strcpy(g_aBrA9D018, "A");
    BrSub1003D130(desc);
    CHECK(desc[0] == 0xCC,
          "one-character name: STILL not copied (the guard is <= 1)");

    memset(desc, 0xCC, sizeof desc);
    strcpy(g_aBrA9D018, "AB");
    BrSub1003D130(desc);
    CHECK(memcmp(desc, "AB", 3) == 0,
          "two characters: copied, with the terminator");
    memcpy(&zero, desc + BR63_DESC_ZERO_OFF, sizeof zero);
    CHECK(zero == 0, "+0xC8 is zeroed on the copying path too");
}

/* ======================================================================
 * 0x10043E70
 * ====================================================================== */
static void TestEnterOptions(void)
{
    BrOptObj *pFirst;

    g_seq = 0;
    g_arenaUsed = 0; g_newFails = 0; g_nCtor = 0; g_nFn56FF0 = 0; g_nC020 = 0;
    g_brPAA2948 = NULL; g_brPAA2904 = NULL;
    g_brA9CFFC = 0; g_brA9D000 = 0; g_brAA287C = 0;

    BrExt_10043E70(0);
    pFirst = g_brPAA2948;
    CHECK(pFirst != NULL && g_nCtor == 1, "first call constructs the object");
    CHECK(g_brPAA2904 == pFirst, "0x10AA2904 points at it");
    CHECK(pFirst->pfnEnter == BrOptFn10056FF0, "pfnEnter is installed");
    CHECK(g_nFn56FF0 == 1 && g_thisFn56FF0 == pFirst, "pfnEnter is called");
    CHECK(pFirst->f0C == 1 && pFirst->f68 == 1, "+0x0C and +0x68 are set");
    CHECK(g_nC020 == 1 && g_seqC020 > g_seqFn56FF0,
          "0x1003C020 runs after, with the gates clear");

    /* Second call: the already-built path does much less. */
    pFirst->f0C = 5; pFirst->f68 = 5;
    g_brPAA2904 = NULL;
    BrExt_10043E70(0);
    CHECK(g_nCtor == 1 && g_nFn56FF0 == 1,
          "second call neither constructs nor re-calls pfnEnter");
    CHECK(g_brPAA2904 == pFirst, "second call only re-points 0x10AA2904");
    CHECK(pFirst->f0C == 5 && pFirst->f68 == 5,
          "second call does NOT re-set +0x0C / +0x68");

    /* Each gate suppresses 0x1003C020. */
    g_nC020 = 0; g_brA9CFFC = 1;  BrExt_10043E70(0);
    CHECK(g_nC020 == 0, "0x10A9CFFC blocks 0x1003C020");
    g_brA9CFFC = 0; g_brA9D000 = 1; BrExt_10043E70(0);
    CHECK(g_nC020 == 0, "0x10A9D000 blocks 0x1003C020");
    g_brA9D000 = 0; g_brAA287C = 2; BrExt_10043E70(0);
    CHECK(g_nC020 == 0, "0x10AA287C == 2 blocks 0x1003C020");
    g_brAA287C = 1; BrExt_10043E70(0);
    CHECK(g_nC020 == 1, "0x10AA287C == 1 allows it");

    /* Allocation failure returns BEFORE the 0x1003C020 gate. */
    g_brPAA2948 = NULL; g_brPAA2904 = (BrOptObj *)0x1;
    g_newFails = 1; g_nC020 = 0; g_brAA287C = 0;
    BrExt_10043E70(0);
    CHECK(g_brPAA2948 == NULL && g_brPAA2904 == NULL,
          "an allocation failure NULLs both pointers");
    CHECK(g_nC020 == 0, "an allocation failure returns before 0x1003C020");
    g_newFails = 0;
}

/* ======================================================================
 * 0x1003C1E0
 * ====================================================================== */
static void *g_timerHwnd;
static uint32_t g_timerId, g_timerMs;
static int g_nTimer, g_seqTimer;
static uint32_t TimerStub(void *hWnd, uint32_t id, uint32_t ms, void *pfn)
{
    (void)pfn;
    ++g_nTimer; g_seqTimer = ++g_seq;
    g_timerHwnd = hWnd; g_timerId = id; g_timerMs = ms;
    return 0x1234u;
}
uint32_t (*g_pfnBrPlatSetTimer)(void *, uint32_t, uint32_t, void *) = TimerStub;

static BrObj29D4 g_obj29D4;

static void TestSessionStart(void)
{
    g_seq = 0;
    g_nC020 = g_nTimer = g_nCC70 = 0;
    g_brA9CFFC = 0;
    g_brP680584 = (void *)0x680584;
    g_brP277B40 = (BrDPlay *)0x277B40;
    g_brPAA29D4 = NULL;

    BrSub1003C1E0();
    CHECK(g_nC020 == 1 && g_seqC020 < g_seqTimer,
          "0x1003C020 runs before SetTimer");
    CHECK(g_nTimer == 1 && g_timerHwnd == (void *)0x680584
          && g_timerId == 1u && g_timerMs == 1000u,
          "SetTimer(0x10680584, 1, 1000, NULL)");
    CHECK(g_brA9BFDC == 0x1234u, "the timer id is stored at 0x10A9BFDC");
    CHECK(g_brA9CFFC == 1, "0x10A9CFFC is raised");
    CHECK(g_nCC70 == 0, "0x1003CC70 is skipped while 0x10AA29D4 is NULL");

    g_brPAA29D4 = &g_obj29D4;
    g_brA9CFFC = 0;
    BrSub1003C1E0();
    CHECK(g_nCC70 == 1 && g_argCC70 == (void *)0x277B40,
          "0x1003CC70 is called with 0x10277B40");
    CHECK(g_seqCC70 > g_seqTimer, "...after the timer is armed");
}

/* ======================================================================
 * 0x10017290
 * ====================================================================== */
static unsigned char g_raceBlock[0x1100];

static void SetRaceFloat(uint32_t off, float f)
{
    memcpy(g_raceBlock + off, &f, sizeof f);
}
static void SetRaceInt(uint32_t off, int32_t v)
{
    memcpy(g_raceBlock + off, &v, sizeof v);
}

static void TestHudTimes(void)
{
    BrHudView aViews[4];
    static const char szPrefix[] = "%ww";

    memset(aViews, 0, sizeof aViews);
    aViews[0].y = 100;
    aViews[1].y = 200;

    BrG_6C2CF8 = g_raceBlock;
    SetRaceFloat(0x0FB0, 1.5f);
    SetRaceFloat(0x0FE4, 2.5f);
    SetRaceFloat(0x0FEC, 3.5f);
    SetRaceInt(0x0FA8, 3);
    g_br0BD3E0 = 9;                 /* count < this => the 0xE9 arm */
    g_env.pszSplitPrefix = szPrefix;
    g_screen.cx = 320; g_screen.cy = 240;

    /* Gate. */
    g_br0BD3EC = 0; g_nTime = 0; g_n19290 = 0;
    BrSub_10017290(aViews);
    CHECK(g_nTime == 0 && g_n19290 == 0,
          "0x100BD3EC == 0 returns before anything, including the pokes");

    /* One view: two lines, the second offset by 0x1E. */
    g_br0BD3EC = 1;
    g_screen.cViews = 1; g_screen.iView = 0;
    g_br0AA010 = 0;
    g_nTime = 0; g_n19290 = 0; g_text.scale = 0;
    BrSub_10017290(aViews);
    CHECK(g_n19290 == 1 && g_text.scale == 0x0F,
          "the three state pokes run before the drawing");
    CHECK(g_nTime == 2, "one view draws both lines");
    CHECK(g_aTime[0].idLabel == 0xE7 && g_aTime[0].y == 100 + 0x14
          && g_aTime[0].x == 320 - 0x10 && g_aTime[0].f == 3.5f,
          "line 1 = string 0xE7 at (cx-0x10, y+0x14) with +0xFEC");
    CHECK(g_aTime[1].idLabel == 0xE9 && g_aTime[1].y == 100 + 0x14 + 0x1E
          && g_aTime[1].f == 1.5f,
          "line 2 = string 0xE9, offset 0x1E, with +0xFB0");
    CHECK(g_aTime[0].pszPrefix == szPrefix && g_aTime[1].pszPrefix == szPrefix,
          "both lines share the 0x100A73C0 prefix");

    /* Split screen: only the second line, and dy collapses to 0. */
    g_screen.cViews = 2; g_screen.iView = 1;
    g_nTime = 0;
    BrSub_10017290(aViews);
    CHECK(g_nTime == 1, "split screen draws only the second line");
    CHECK(g_aTime[0].idLabel == 0xE9 && g_aTime[0].y == 200 + 0x14,
          "...at y + 0x14 with NO 0x1E offset");

    /* count >= 0x100BD3E0 swaps 0xE9/+0xFB0 for 0xE8/+0xFE4. */
    g_screen.cViews = 2; g_screen.iView = 0;
    g_br0BD3E0 = 3;
    g_nTime = 0;
    BrSub_10017290(aViews);
    CHECK(g_nTime == 1 && g_aTime[0].idLabel == 0xE8
          && g_aTime[0].f == 2.5f, "count >= 0x100BD3E0 selects 0xE8 / +0xFE4");

    /* Mode 3 has its own arm: 0xE8/+0xFE4 first, 0xE9/+0xFB0 second. */
    g_br0AA010 = 3;
    g_screen.cViews = 1; g_screen.iView = 0;
    g_nTime = 0;
    BrSub_10017290(aViews);
    CHECK(g_nTime == 2 && g_aTime[0].idLabel == 0xE8
          && g_aTime[1].idLabel == 0xE9, "mode 3 draws 0xE8 then 0xE9");
    CHECK(g_aTime[1].y == 100 + 0x14 + 0x1E, "mode 3 offsets the second line");

    /* Modes 4 and 5 draw nothing; so does anything above 6. */
    {
        int mode;
        for (mode = 4; mode <= 5; ++mode) {
            g_br0AA010 = mode;
            g_nTime = 0; g_n19290 = 0;
            BrSub_10017290(aViews);
            CHECK(g_nTime == 0, "modes 4 and 5 draw nothing");
            CHECK(g_n19290 == 1, "...but the state pokes still ran");
        }
        g_br0AA010 = 7;
        g_nTime = 0;
        BrSub_10017290(aViews);
        CHECK(g_nTime == 0, "mode 7 draws nothing");
        g_br0AA010 = -1;
        g_nTime = 0;
        BrSub_10017290(aViews);
        CHECK(g_nTime == 0, "a negative mode draws nothing (the test is `ja`)");
    }

    /* Mode 6 shares the mode-0 arm. */
    g_br0AA010 = 6;
    g_br0BD3E0 = 9;
    g_nTime = 0;
    BrSub_10017290(aViews);
    CHECK(g_nTime == 2 && g_aTime[0].idLabel == 0xE7,
          "mode 6 shares the mode-0 arm");
}

/* ======================================================================
 * 0x1005FBC0
 * ====================================================================== */
static void TestPublish(void)
{
    int i;
    unsigned char *pRaw = (unsigned char *)g_aBrAA26F0;

    g_aBrAA26F4[0] = 3;
    g_aBrAA26F4[1] = 6;
    g_brAA27EC = 11; g_brAA27F0 = 12; g_brAA27F4 = 13;
    g_aBrAA26F0[0] = 4;
    g_brAA27E0 = 0xABCD1234u;
    g_brAA2A10 = 0x000F0000;
    g_brAA2A14 = 0x0000000F;
    g_nSprintf = 0;

    g_brAA27F8 = 2;
    BrExt_1005FBC0(0);
    CHECK(g_brAA28B8 == 3, "0x10AA28B8 <- byte 0 of 0x10AA26F4");
    CHECK(g_brAA28A4 == 6, "0x10AA28A4 <- byte 1 (0x10AA26F5), zero-extended");
    CHECK(g_br094354 == 11 && g_br09435C == 12 && g_br094358 == 13,
          "the three derived slots are published");
    CHECK(g_brB4E1D0 == 2, "0x10B4E1D0 <- 0x10AA27F8 (before the decrements)");
    CHECK(g_brAA28A0 == 4, "0x10AA28A0 <- 0x10AA26F0[0]");
    CHECK(g_brAA28AC == g_brAA28A4, "0x10AA28AC mirrors 0x10AA28A4");
    CHECK(g_brB4E1D4 == &g_aBrB4DF30[2], "value 2 selects record 2");
    CHECK(g_nSprintf == 2, "two strings are formatted");
    CHECK(strcmp(g_sprintfLast, "7") == 0,
          "the second counter is printed PLUS ONE");

    /* 0 and anything above 3 both fall through to record 0. */
    g_brAA27F8 = 0;  BrExt_1005FBC0(0);
    CHECK(g_brB4E1D4 == &g_aBrB4DF30[0], "value 0 selects record 0");
    g_brAA27F8 = 4;  BrExt_1005FBC0(0);
    CHECK(g_brB4E1D4 == &g_aBrB4DF30[0], "value 4 also selects record 0");
    g_brAA27F8 = 1;  BrExt_1005FBC0(0);
    CHECK(g_brB4E1D4 == &g_aBrB4DF30[1], "value 1 selects record 1");
    g_brAA27F8 = 3;  BrExt_1005FBC0(0);
    CHECK(g_brB4E1D4 == &g_aBrB4DF30[3], "value 3 selects record 3");

    /* The masks are OR-ed in, not assigned, and the halves do not cross. */
    g_brAA2A10 = 0x000F0000;
    g_brAA2A14 = 0x0000000F;
    BrExt_1005FBC0(0);
    CHECK(g_brAA2A10 == (int32_t)(0x000F0000u | 0x1234u),
          "0x10AA2A10 |= the LOW half of 0x10AA27E0");
    CHECK(g_brAA2A14 == (int32_t)(0x0000000Fu | 0xABCDu),
          "0x10AA2A14 |= the HIGH half of 0x10AA27E0");

    /* The four-halfword sum is gated by the argument. */
    for (i = 0; i < 8; ++i) {
        pRaw[0x1E + 3 * 8 + i] = (unsigned char)(i + 1);
    }
    g_aBrAA26F4[0] = 3;
    g_brAA28C4 = -1;
    BrExt_1005FBC0(0);
    CHECK(g_brAA28C4 == -1, "argument 0 leaves 0x10AA28C4 alone");
    BrExt_1005FBC0(1);
    {
        uint32_t sum = 0;
        for (i = 0; i < 4; ++i) {
            uint16_t hw;
            memcpy(&hw, pRaw + 0x1E + 3 * 8 + i * 2, sizeof hw);
            sum += hw;
        }
        CHECK(g_brAA28C4 == (int32_t)sum,
              "argument 1 sums FOUR CONSECUTIVE halfwords (stride 2, not 8)");
    }
}

/* ====================================================================== */
int main(void)
{
    memset(g_strPool, 0, sizeof g_strPool);
    g_out.pCur = g_dl;

    TestQuat();
    TestOptSave();
    TestAvailAdapter();
    TestE510();
    TestFillRect();
    TestSmall();
    TestEnterOptions();
    TestSessionStart();
    TestHudTimes();
    TestPublish();

    printf("%s: %d checks, %d failures\n",
           g_nFail ? "FAILED" : "ok", g_nRun, g_nFail);
    return g_nFail ? 1 : 0;
}
