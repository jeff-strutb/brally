/* test_slice3_40.c -- behavioural tests for slice3_40.c.
 *
 * These assert properties of the ORIGINAL that the port has to keep:
 * clamps that are asymmetric, a store that is immediately killed, two
 * routines that pass the same two points to BrVec3Lerp the opposite way
 * round, an out-parameter block that is 18 bytes and not 20, and so on.
 * Nothing here encodes a number that was invented for the test.
 *
 * Every cross-slice dependency of slice3_40.c is stood in for below.
 * THESE STAND-INS ARE TEST SCAFFOLDING ONLY -- they are not ports of the
 * addresses they carry and must never be linked into the real build.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "slice3_40.h"

static int g_fail = 0;
static int g_checks = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        ++g_checks;                                                        \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);         \
            ++g_fail;                                                      \
        }                                                                  \
    } while (0)

#define CHECK_F(a, b)                                                      \
    do {                                                                   \
        ++g_checks;                                                        \
        if (!(fabs((double)(a) - (double)(b)) <= 1e-5)) {                   \
            printf("FAIL %s:%d  %s (%g) != %s (%g)\n", __FILE__, __LINE__,  \
                   #a, (double)(a), #b, (double)(b));                       \
            ++g_fail;                                                      \
        }                                                                  \
    } while (0)

/* ================================================================== */
/* STAND-INS for the cross-slice dependencies                          */
/* ================================================================== */

/* Globals other slices own. */
BrImgState BrImgTintState;
uint8_t    BrSndMasterVolume;
int32_t    BrG_0B380C;
int32_t    g_brB4E708;
int32_t    g_brB4E70C;

/* 0x1007C8A0 -- truncate toward zero, keep the low dword. */
int32_t BrFtolTrunc(float f)
{
    return (int32_t)(long long)f;
}

/* 0x1003AFA0 -- out = (a - b) * t + b.  Reads each component before it
 * writes it, which BrPathWalkFrom relies on. */
void BrVec3Lerp(BrVec3 *pOut, const BrVec3 *pA, const BrVec3 *pB, float t)
{
    float x = (pA->x - pB->x) * t + pB->x;
    float y = (pA->y - pB->y) * t + pB->y;
    float z = (pA->z - pB->z) * t + pB->z;
    pOut->x = x;
    pOut->y = y;
    pOut->z = z;
}

/* 0x1003BA70 -- scripted. */
static int      g_segResult;
static int      g_segCalls;
static int32_t  g_segIdx[32];
int BrSeg2Intersect(const BrVec2 *pA, const BrVec2 *pB,
                    const BrVec2 *pC, const BrVec2 *pD)
{
    /* recover which table record was used from the pointer we were given */
    const char *base = (const char *)(const void *)&BrPathSegs[0];
    long        off  = (const char *)(const void *)pA - base;
    (void)pB; (void)pC; (void)pD;
    if (g_segCalls < 32) {
        g_segIdx[g_segCalls] = (int32_t)(off / (long)sizeof(BrPathSeg));
    }
    ++g_segCalls;
    return g_segResult;
}

/* 0x100607B0 */
static BrCarState *g_buildDst;
static BrCar      *g_buildCar;
void BrSub100607B0(BrCarState *pDst, BrCar *pCar)
{
    g_buildDst = pDst;
    g_buildCar = pCar;
    memset(pDst, 0, sizeof(*pDst));
    pDst->f00 = 7.5f;
}

/* 0x10005130 */
static BrCarState *g_sendArg;
static int         g_sendCalls;
int BrNetCarStateSend(BrCarState *pState)
{
    g_sendArg = pState;
    ++g_sendCalls;
    return 1;
}

/* 0x10005D30 */
static int32_t g_localSlot;
int32_t BrSub10005D30(void) { return g_localSlot; }

/* 0x100054A0 */
static int         g_predictResult;
static int         g_predictCalls;
static BrCarState  g_predictSrc;
int BrNetSlotPredictOrig(BrCarState *pDst, int32_t slot)
{
    (void)slot;
    ++g_predictCalls;
    *pDst = g_predictSrc;
    return g_predictResult;
}

/* 0x100695D0 */
static void *g_s695D0Dst;
static int   g_s695D0Calls;
void BrSub100695D0(void *pDst220, const BrCarState *pState)
{
    (void)pState;
    g_s695D0Dst = pDst220;
    ++g_s695D0Calls;
}

/* 0x1006F4A0 */
static void *g_sF4A0Arg;
static int   g_sF4A0Calls;
void BrSub1006F4A0(void *pCar164)
{
    g_sF4A0Arg = pCar164;
    ++g_sF4A0Calls;
}

/* 0x100773F0 */
static uint32_t g_inputFlags;
static int32_t  g_inputAxis0;
static int32_t  g_inputAxis1;
uint32_t BrSub100773F0(int32_t *pAxis0, int32_t *pAxis1)
{
    *pAxis0 = g_inputAxis0;
    *pAxis1 = g_inputAxis1;
    return g_inputFlags;
}

/* 0x100742D0 -- marks qDot so the two memcpys can be shown to run after. */
static int g_qdotCalls;
void BrRbQuatDerivative(BrRbState *pS)
{
    ++g_qdotCalls;
    pS->qDot.f04 = 1234.0f;   /* qDot is BrRbState+0x34, so this is +0x38 */
}

/* 0x10074450 */
static int    g_matCalls;
static void  *g_matDst[8];
static const void *g_matSrc[8];
void BrRbBuildMatrix(BrMat4 *pM, const BrRbState *pS)
{
    if (g_matCalls < 8) {
        g_matDst[g_matCalls] = pM;
        g_matSrc[g_matCalls] = pS;
    }
    ++g_matCalls;
}

/* ================================================================== */
/* Helpers                                                             */
/* ================================================================== */

#define CB(c, off) (*((uint8_t *)(void *)(c) + (off)))
static float  carF(const BrCar *c, int off)
{
    float v;
    memcpy(&v, (const uint8_t *)(const void *)c + off, sizeof v);
    return v;
}
static int32_t carI(const BrCar *c, int off)
{
    int32_t v;
    memcpy(&v, (const uint8_t *)(const void *)c + off, sizeof v);
    return v;
}
static uint32_t carU(const BrCar *c, int off)
{
    uint32_t v;
    memcpy(&v, (const uint8_t *)(const void *)c + off, sizeof v);
    return v;
}

static BrNode *NodeNew(int nPts)
{
    BrNode *n = (BrNode *)calloc(1, sizeof(BrNode) +
                                 (size_t)(nPts + 2) * sizeof(BrPathPoint));
    n->count = (uint16_t)nPts;
    return n;
}

/* ================================================================== */
/* 2. Option sliders                                                   */
/* ================================================================== */

static void TestSliders(void)
{
    int i;

    /* Round trip: N up then N down returns both the index and the output. */
    g_brB4E70C = 0;
    BrOptLevelAStepDown();                 /* seed the output at index 0 */
    CHECK(g_brB4E70C == 0);
    CHECK(BrG_0BBAD8 == 0);

    for (i = 0; i < 4; ++i) { BrOptLevelAStepUp(); }
    CHECK(g_brB4E70C == 4);
    CHECK(BrG_0BBAD8 == (uint8_t)BrOptLevelATable[4]);
    for (i = 0; i < 4; ++i) { BrOptLevelAStepDown(); }
    CHECK(g_brB4E70C == 0);
    CHECK(BrG_0BBAD8 == 0);

    /* Clamps: 9 is the top, 0 the bottom, for both sliders. */
    for (i = 0; i < 40; ++i) { BrOptLevelAStepUp(); }
    CHECK(g_brB4E70C == BR_OPT_LEVEL_STEPS - 1);
    CHECK(BrG_0BBAD8 == 0xFF);
    for (i = 0; i < 40; ++i) { BrOptLevelAStepDown(); }
    CHECK(g_brB4E70C == 0);

    g_brB4E708 = 0;
    for (i = 0; i < 40; ++i) { BrOptLevelBStepUp(); }
    CHECK(g_brB4E708 == BR_OPT_LEVEL_STEPS - 1);
    CHECK(BrSndMasterVolume == 0xFF);
    for (i = 0; i < 40; ++i) { BrOptLevelBStepDown(); }
    CHECK(g_brB4E708 == 0);
    CHECK(BrSndMasterVolume == 0);

    /* The clamp is asymmetric in an easily-missed way: at the end of the
     * range the index is NOT written back but the table IS re-read, so the
     * call is a "reapply", not a no-op. */
    g_brB4E70C = 9;
    BrG_0BBAD8 = 0x11;
    BrOptLevelAStepUp();
    CHECK(g_brB4E70C == 9);
    CHECK(BrG_0BBAD8 == (uint8_t)BrOptLevelATable[9]);

    g_brB4E708 = 0;
    BrSndMasterVolume = 0x22;
    BrOptLevelBStepDown();
    CHECK(g_brB4E708 == 0);
    CHECK(BrSndMasterVolume == (uint8_t)BrOptLevelBTable[0]);

    /* The two tables are genuinely different curves: A is linear, B is not.
     * (If they were ever collapsed into one table this would catch it.) */
    CHECK(BrOptLevelATable[0] == 0 && BrOptLevelBTable[0] == 0);
    CHECK(BrOptLevelATable[BR_OPT_LEVEL_STEPS - 1] == 0xFF);
    CHECK(BrOptLevelBTable[BR_OPT_LEVEL_STEPS - 1] == 0xFF);
    CHECK(BrOptLevelBTable[1] > BrOptLevelATable[1]);
    for (i = 1; i < BR_OPT_LEVEL_STEPS; ++i) {
        CHECK(BrOptLevelATable[i] > BrOptLevelATable[i - 1]);
        CHECK(BrOptLevelBTable[i] > BrOptLevelBTable[i - 1]);
    }
}

/* ================================================================== */
/* 3. Controller-state translation                                     */
/* ================================================================== */

static void TestGfx60E00(void)
{
    uint8_t out[8];

    /* the axes are the LOW BYTE of each out-param, and land at 2 and 3 */
    memset(out, 0xEE, sizeof out);
    g_inputFlags = 0;
    g_inputAxis0 = 0x1234;
    g_inputAxis1 = -1;
    BrGfx60E00(out);
    CHECK(out[0] == 0 && out[1] == 0);
    CHECK(out[2] == 0x34);
    CHECK(out[3] == 0xFF);
    CHECK(out[4] == 0xEE);           /* nothing past byte 3 is touched */

    /* 0x0010 SEEDS the pair by assignment */
    g_inputAxis0 = 0; g_inputAxis1 = 0;
    g_inputFlags = 0x0010;
    BrGfx60E00(out);
    CHECK(out[0] == 0x00 && out[1] == 0x84);

    /* ... and every other flag ORs on top of that seed */
    g_inputFlags = 0x0010 | 0x0004;
    BrGfx60E00(out);
    CHECK(out[0] == 0x00);
    CHECK(out[1] == (uint8_t)(0x84 | 0x88));   /* 0x0004 sets TWO bits */

    /* order independence: the seed wins regardless of what else is set */
    g_inputFlags = 0x0001 | 0x0010;
    BrGfx60E00(out);
    CHECK(out[1] == (uint8_t)(0x84 | 0x02));

    /* individual mappings */
    g_inputFlags = 0x0001; BrGfx60E00(out); CHECK(out[1] == 0x02);
    g_inputFlags = 0x0002; BrGfx60E00(out); CHECK(out[1] == 0x01);
    g_inputFlags = 0x0008; BrGfx60E00(out); CHECK(out[1] == 0x40);
    g_inputFlags = 0x8000; BrGfx60E00(out); CHECK(out[1] == 0x10);
    g_inputFlags = 0x0100; BrGfx60E00(out); CHECK(out[0] == 0x08);
    g_inputFlags = 0x0200; BrGfx60E00(out); CHECK(out[0] == 0x02);
    g_inputFlags = 0x0400; BrGfx60E00(out); CHECK(out[0] == 0x04);
    g_inputFlags = 0x0020; BrGfx60E00(out); CHECK(out[0] == 0x10);
    g_inputFlags = 0x0040; BrGfx60E00(out); CHECK(out[0] == 0x20);

    /* bits 0x0800..0x4000 are never examined */
    g_inputFlags = 0x7800;
    BrGfx60E00(out);
    CHECK(out[0] == 0 && out[1] == 0);

    /* the two halves are disjoint: no flag writes both bytes */
    {
        unsigned bit;
        for (bit = 0; bit < 16; ++bit) {
            uint8_t b0, b1;
            g_inputFlags = 1u << bit;
            BrGfx60E00(out);
            b0 = out[0];
            b1 = out[1];
            CHECK(b0 == 0 || b1 == 0 || (1u << bit) == 0x0010u);
        }
    }
}

/* ================================================================== */
/* 4. Node mark / clear pass                                           */
/* ================================================================== */

static void TestNodePass(void)
{
    BrNode a, b, c;

    /* a -f04-> b, a -f00-> c */
    memset(&a, 0, sizeof a);
    memset(&b, 0, sizeof b);
    memset(&c, 0, sizeof c);
    a.f04 = &b;
    a.f00 = &c;
    a.f11 = 2; b.f11 = 2; c.f11 = 2;

    /* BrG_0B380C is neither 3 nor 9: the marks go on, f11 is left alone */
    BrG_0B380C = 1;
    BrNodeMarkPass(&a);
    CHECK((a.flags & BR_NODE_FLAG_MARK) != 0);
    CHECK((b.flags & BR_NODE_FLAG_MARK) != 0);
    CHECK((c.flags & BR_NODE_FLAG_MARK) != 0);
    CHECK(a.f11 == 2 && b.f11 == 2 && c.f11 == 2);

    /* round trip: the clear pass restores every flag word */
    BrNodeClearMarkPass(&a);
    CHECK(a.flags == 0 && b.flags == 0 && c.flags == 0);

    /* BrG_0B380C == 3 clears f11 -- but only where it was exactly 2 */
    b.f11 = 5;
    BrG_0B380C = 3;
    BrNodeMarkPass(&a);
    CHECK(a.f11 == 0);
    CHECK(b.f11 == 5);
    CHECK(c.f11 == 0);
    BrNodeClearMarkPass(&a);

    /* 9 behaves like 3 */
    a.f11 = 2; c.f11 = 2;
    BrG_0B380C = 9;
    BrNodeMarkPass(&a);
    CHECK(a.f11 == 0 && c.f11 == 0);
    BrNodeClearMarkPass(&a);

    /* a node carrying the SKIP flag is not entered and neither is its f00
     * subtree, but the f04 chain continues through it */
    a.f11 = 2; b.f11 = 2; c.f11 = 2;
    a.flags = BR_NODE_FLAG_SKIP;
    BrG_0B380C = 3;
    BrNodeMarkPass(&a);
    CHECK(a.f11 == 2);                       /* skipped */
    CHECK(c.f11 == 2);                       /* its subtree too */
    CHECK(b.f11 == 0);                       /* the chain went on */
    CHECK((a.flags & BR_NODE_FLAG_MARK) == 0);
    BrNodeClearMarkPass(&a);
    a.flags = 0;

    /* the mark is set BEFORE the recursion, so a cycle terminates.  If it
     * were set afterwards this call would never return. */
    memset(&a, 0, sizeof a);
    memset(&b, 0, sizeof b);
    a.f00 = &b;
    b.f00 = &a;
    BrNodeMarkPass(&a);
    CHECK((a.flags & BR_NODE_FLAG_MARK) != 0);
    CHECK((b.flags & BR_NODE_FLAG_MARK) != 0);
    BrNodeClearMarkPass(&a);
    CHECK(a.flags == 0 && b.flags == 0);

    /* the combined pass leaves no marks behind, which is the whole point */
    memset(&a, 0, sizeof a);
    memset(&b, 0, sizeof b);
    a.f04 = &b;
    a.f11 = 2; b.f11 = 2;
    BrG_6C7CB8 = &a;
    BrG_0B380C = 3;
    BrNodeRunMarkPass();
    CHECK(a.flags == 0 && b.flags == 0);
    CHECK(a.f11 == 0 && b.f11 == 0);

    /* NULL is a no-op on all three */
    BrNodeMarkPass(NULL);
    BrNodeClearMarkPass(NULL);
    BrG_6C7CB8 = NULL;
    BrNodeRunMarkPass();
}

/* ================================================================== */
/* 5a. BrZeroRegions                                                   */
/* ================================================================== */

static void TestZeroRegions(void)
{
    uint8_t buf[64];
    BrZeroRegion list[4];

    memset(buf, 0xAA, sizeof buf);
    list[0].p = buf + 4;   list[0].size = 8;
    list[1].p = buf + 32;  list[1].size = 0;    /* size 0 -> no-op */
    list[2].p = buf + 40;  list[2].size = 3;
    list[3].p = NULL;      list[3].size = 999;  /* terminator, size ignored */

    BrZeroRegions(list);

    CHECK(buf[3] == 0xAA);
    CHECK(buf[4] == 0 && buf[11] == 0);
    CHECK(buf[12] == 0xAA);
    CHECK(buf[32] == 0xAA);                       /* size 0 really is 0 */
    CHECK(buf[40] == 0 && buf[42] == 0);
    CHECK(buf[43] == 0xAA);

    /* an immediately-empty list returns without touching anything */
    memset(buf, 0xBB, sizeof buf);
    list[0].p = NULL;
    BrZeroRegions(list);
    CHECK(buf[0] == 0xBB);
}

/* ================================================================== */
/* 5b. BrCarInitTables / BrCarClear29C8                                */
/* ================================================================== */

static void TestCarInit(void)
{
    static BrCar car;
    uint8_t *p = (uint8_t *)(void *)&car;
    int i, j, k;

    memset(&car, 0xAA, sizeof car);
    /* the seed for the (dead) float chain */
    memcpy(p + 0x140, &(int32_t){ 7 }, 4);

    BrCarInitTables(&car);

    /* THE DEAD STORE.  0x10AC..0x10B8 are written with a float chain and
     * then zeroed by the next loop; only the zeros survive. */
    for (i = 0; i < 4; ++i) {
        CHECK(carI(&car, 0x10AC + 4 * i) == 0);
        CHECK(carI(&car, 0x10BC + 4 * i) == 2);
        CHECK(carI(&car, 0x10CC + 4 * i) == 0);
        CHECK(carI(&car, 0x10DC + 4 * i) == 0);
        CHECK_F(carF(&car, 0x106C + 4 * i), (float)i * 0.15f);
    }

    /* the three arrays are exactly adjacent; that is what makes the
     * 144-entry counts and the 6- and 32-byte strides consistent */
    CHECK(0x1120 + 0x20 * 0x90 == 0x2320);
    CHECK(0x2320 + 6 * 0x90 == 0x2680);

    for (j = 0; j < 0x90; ++j) {
        const uint8_t *pW = p + 0x2320 + 6 * j;
        const uint8_t *pR = p + 0x1120 + 0x20 * j;
        CHECK(pW[0] == 0 && pW[1] == 0);
        CHECK(pW[2] == 0 && pW[3] == 0);
        CHECK(pW[4] == 0 && pW[5] == 0);
        CHECK(carI(&car, 0x1120 + 0x20 * j + 0x00) == 0);
        CHECK(carI(&car, 0x1120 + 0x20 * j + 0x04) == 0);
        CHECK(carI(&car, 0x1120 + 0x20 * j + 0x08) == 0);
        CHECK(carI(&car, 0x1120 + 0x20 * j + 0x14) == 0);
        CHECK(carI(&car, 0x1120 + 0x20 * j + 0x18) == 0);
        CHECK(carI(&car, 0x1120 + 0x20 * j + 0x1C) == 0);
        /* +0x0C and +0x10 are deliberately NOT cleared */
        CHECK(pR[0x0C] == 0xAA && pR[0x10] == 0xAA);
    }

    for (k = 0; k < 0x12; ++k) {
        CHECK(carU(&car, 0x2680 + 4 * k) == 0x00020002u);
    }
    /* exactly 18 dwords, no more */
    CHECK(carU(&car, 0x2680 + 4 * 0x12) == 0xAAAAAAAAu);

    /* BrCarClear29C8 covers 18 bytes, not 20 */
    memset(&car, 0xAA, sizeof car);
    BrCarClear29C8(&car);
    for (i = 0x29C8; i < 0x29D8 + 2; ++i) {
        CHECK(CB(&car, i) == 0);
    }
    CHECK(CB(&car, 0x29DA) == 0xAA);
    CHECK(CB(&car, 0x29C7) == 0xAA);
}

/* ================================================================== */
/* 5c. BrCarBuildMatrices                                              */
/* ================================================================== */

static void TestBuildMatrices(void)
{
    static BrCar   car;
    static uint8_t sub[4][0x200];
    uint8_t *p = (uint8_t *)(void *)&car;
    int i;
    void *q;

    memset(&car, 0, sizeof car);
    for (i = 0; i < 4; ++i) {
        q = sub[i];
        BR_CAR_SUBPTR(&car, i) = q;
    }

    g_matCalls = 0;
    g_sF4A0Calls = 0;
    BrCarBuildMatrices(&car);

    CHECK(g_sF4A0Calls == 1);
    CHECK(g_sF4A0Arg == (void *)(p + 0x164));
    CHECK(g_matCalls == 4);
    for (i = 0; i < 4; ++i) {
        CHECK(g_matDst[i] == (void *)(sub[i] + BR_CARSUB_MAT));
        CHECK(g_matSrc[i] == (const void *)(sub[i] + BR_CARSUB_RB));
    }
    /* the matrix is built FROM the rigid-body state, so dst must be the
     * higher of the two offsets, not the other way round */
    CHECK(BR_CARSUB_MAT > BR_CARSUB_RB);
    CHECK(BR_CARSUB_MAT - BR_CARSUB_RB == 0x44);   /* sizeof BrRbState */
}

/* ================================================================== */
/* 1. BrCarApplyState / BrCarPredictRemote / BrCarNetSendState         */
/* ================================================================== */

static void TestApplyState(void)
{
    static BrCar car;
    uint8_t *p = (uint8_t *)(void *)&car;
    BrCarState st;
    uint32_t   flags;
    uint32_t  *pFlags = &flags;
    int i;

    memset(&car, 0, sizeof car);
    memcpy(p + 0x29C0, &pFlags, sizeof pFlags);

    for (i = 0; i < BR_CARSTATE_FLOATS; ++i) {
        ((float *)(void *)&st)[i] = (float)(i + 1);
    }

    flags = 0;
    g_qdotCalls = 0;
    g_s695D0Calls = 0;
    BrCarApplyState(&car, &st);

    /* the rigid-body block: quaternion, position, velocity, angular vel */
    CHECK_F(carF(&car, 0x1F4), st.f00);
    CHECK_F(carF(&car, 0x1F8), st.f04);
    CHECK_F(carF(&car, 0x1FC), st.f08);
    CHECK_F(carF(&car, 0x200), st.f0C);
    CHECK_F(carF(&car, 0x1DC), st.f10);
    CHECK_F(carF(&car, 0x1E0), st.f14);
    CHECK_F(carF(&car, 0x1E4), st.f18);
    CHECK_F(carF(&car, 0x1E8), st.f1C);
    CHECK_F(carF(&car, 0x1EC), st.f20);
    CHECK_F(carF(&car, 0x1F0), st.f24);
    CHECK_F(carF(&car, 0x204), st.f28);
    CHECK_F(carF(&car, 0x208), st.f2C);
    CHECK_F(carF(&car, 0x20C), st.f30);

    /* f38 goes to TWO places */
    CHECK_F(carF(&car, 0x73C), st.f38);
    CHECK_F(carF(&car, 0xB54), st.f38);

    /* the __ftol'd fields: f4C = 20.0f -> 20 */
    CHECK(carI(&car, 0x524) == (int32_t)st.f4C);
    CHECK(CB(&car, 0x510) == (uint8_t)(int32_t)st.f5C);
    CHECK(CB(&car, 0x36A) == (uint8_t)(int32_t)st.f9C);

    /* BrSub100695D0 is handed pCar + 0x220 */
    CHECK(g_s695D0Calls == 1);
    CHECK(g_s695D0Dst == (void *)(p + 0x220));

    /* the derivative runs BEFORE the two snapshots, so the snapshots
     * contain its output */
    CHECK(g_qdotCalls == 1);
    CHECK_F(carF(&car, 0x1DC + 0x38), 1234.0f);      /* qDot.x in place  */
    CHECK(memcmp(p + 0x278, p + 0x1DC, 0x44) == 0);
    CHECK(memcmp(p + 0x2BC, p + 0x1DC, 0x44) == 0);

    /* --- the two "== 0.0f" booleans ---------------------------------- */
    st.f70 = 0.0f;
    st.f74 = 0.0f;
    flags = 0xFFFFFFFFu;
    BrCarApplyState(&car, &st);
    CHECK((flags & 0x00040000u) == 0);               /* zero CLEARS */
    CHECK_F(carF(&car, 0xE68), 1.0f);

    st.f70 = 0.25f;
    st.f74 = 0.25f;
    flags = 0;
    BrCarApplyState(&car, &st);
    CHECK((flags & 0x00040000u) != 0);               /* non-zero SETS */
    CHECK_F(carF(&car, 0xE68), -1.0f);

    /* a NaN sets C3 just like equality does, so it takes the ZERO branch */
    st.f70 = (float)NAN;
    st.f74 = (float)NAN;
    flags = 0xFFFFFFFFu;
    BrCarApplyState(&car, &st);
    CHECK((flags & 0x00040000u) == 0);
    CHECK_F(carF(&car, 0xE68), 1.0f);
    CHECK(flags == 0xFFFBFFFFu);                     /* no other bit moved */

    /* --- the pCar+0xFF4 guard ---------------------------------------- */
    st.f70 = 0.0f; st.f74 = 0.0f;

    /* current <= 0 -> always overwritten */
    memcpy(p + 0xFF4, &(float){ 0.0f }, 4);
    st.f78 = 5.0f;
    BrCarApplyState(&car, &st);
    CHECK_F(carF(&car, 0xFF4), 5.0f);

    /* current + 1000 <= incoming -> NOT overwritten */
    memcpy(p + 0xFF4, &(float){ 10.0f }, 4);
    st.f78 = 1e9f;
    BrCarApplyState(&car, &st);
    CHECK_F(carF(&car, 0xFF4), 10.0f);

    /* current + 1000 > incoming -> overwritten, even downward */
    memcpy(p + 0xFF4, &(float){ 10.0f }, 4);
    st.f78 = 3.0f;
    BrCarApplyState(&car, &st);
    CHECK_F(carF(&car, 0xFF4), 3.0f);

    /* exactly at the boundary the guard does NOT fire (it is a strict >) */
    memcpy(p + 0xFF4, &(float){ 10.0f }, 4);
    st.f78 = 1010.0f;
    BrCarApplyState(&car, &st);
    CHECK_F(carF(&car, 0xFF4), 10.0f);
}

static void TestPredictRemote(void)
{
    static BrCar car;
    static uint8_t sub[4][0x200];
    uint32_t  flags = 0;
    uint32_t *pFlags = &flags;
    int32_t r;
    int i;

    memset(&car, 0, sizeof car);
    memset(&g_predictSrc, 0, sizeof g_predictSrc);
    memcpy((uint8_t *)(void *)&car + 0x29C0, &pFlags, sizeof pFlags);
    for (i = 0; i < 4; ++i) {
        BR_CAR_SUBPTR(&car, i) = sub[i];
    }

    /* the local slot short-circuits BEFORE any prediction */
    g_localSlot = 3;
    BrG_6909B4 = 0;
    g_predictCalls = 0;
    r = BrCarPredictRemote(&car, 3);
    CHECK(r == 1);
    CHECK(g_predictCalls == 0);

    /* so does the 0x106909B4 gate */
    g_localSlot = 3;
    BrG_6909B4 = 1;
    r = BrCarPredictRemote(&car, 4);
    CHECK(r == 1);
    CHECK(g_predictCalls == 0);

    /* a failed prediction is the ONLY thing that returns 0 */
    BrG_6909B4 = 0;
    g_predictResult = 0;
    g_matCalls = 0;
    r = BrCarPredictRemote(&car, 4);
    CHECK(r == 0);
    CHECK(g_predictCalls == 1);
    CHECK(g_matCalls == 0);              /* nothing applied */

    /* success applies and rebuilds */
    g_predictResult = 1;
    g_predictSrc.f34 = 42.0f;
    g_matCalls = 0;
    r = BrCarPredictRemote(&car, 4);
    CHECK(r == 1);
    CHECK_F(carF(&car, 0x338), 42.0f);
    CHECK(g_matCalls == 4);
}

static void TestNetSendState(void)
{
    static BrCar car;

    g_sendCalls = 0;
    BrCarNetSendState(&car);
    CHECK(g_sendCalls == 1);
    CHECK(g_buildCar == &car);
    /* the same stack buffer is built and then sent */
    CHECK(g_buildDst == g_sendArg);
    CHECK_F(g_sendArg->f00, 7.5f);
}

/* ================================================================== */
/* 6. Path walking                                                     */
/* ================================================================== */

/* four points, segment lengths 10, 20, 30 */
static BrNode *MakePath(void)
{
    BrNode *n = NodeNew(3);
    int i;
    static const float dist[4] = { 60.0f, 50.0f, 30.0f, 0.0f };
    for (i = 0; i < 4; ++i) {
        n->pts[i].pos.x = (float)i;
        n->pts[i].pos.y = (float)(2 * i);
        n->pts[i].pos.z = 0.0f;
        n->pts[i].f18 = dist[i];
    }
    return n;
}

static void TestPathWalk(void)
{
    BrNode *n = MakePath();
    BrNode *skip = NodeNew(0);
    BrVec3  fromWalk;

    BrPathSegCount = 0;      /* disable the crossing test for now */

    /* t = 0 lands exactly on pts[0] */
    BrPathWalkNode = NULL;
    BrPathWalk(n, 0.0f);
    CHECK(BrPathWalkNode == n);
    CHECK(BrPathWalkIndex == 0);
    CHECK_F(BrPathWalkPoint.x, 0.0f);
    CHECK_F(BrPathWalkPoint.y, 0.0f);

    /* t == the first segment length lands exactly on pts[1], still on
     * index 0 -- the compare is <=, not < */
    BrPathWalk(n, 10.0f);
    CHECK(BrPathWalkIndex == 0);
    CHECK_F(BrPathWalkPoint.x, 1.0f);
    CHECK_F(BrPathWalkPoint.y, 2.0f);

    /* halfway along the first segment */
    BrPathWalk(n, 5.0f);
    CHECK(BrPathWalkIndex == 0);
    CHECK_F(BrPathWalkPoint.x, 0.5f);

    /* into the second segment */
    BrPathWalk(n, 20.0f);
    CHECK(BrPathWalkIndex == 1);
    CHECK_F(BrPathWalkPoint.x, 1.5f);

    /* running past the end leaves the outputs alone -- they are only
     * written on the hit path */
    BrPathWalkNode = NULL;
    BrPathWalkIndex = -7;
    BrPathWalkPoint.x = 99.0f;
    BrPathWalk(n, 1.0e9f);
    CHECK(BrPathWalkNode == NULL);
    CHECK(BrPathWalkIndex == -7);
    CHECK_F(BrPathWalkPoint.x, 99.0f);

    /* a SKIP node hands over to its f04 */
    skip->flags = BR_NODE_FLAG_SKIP;
    skip->f04 = n;
    BrPathWalk(skip, 5.0f);
    CHECK(BrPathWalkNode == n);
    CHECK_F(BrPathWalkPoint.x, 0.5f);

    /* --- BrPathWalkFrom with s == 1 must agree exactly with BrPathWalk.
     * The two run DIFFERENT lerps (mirrored operands plus a second pass),
     * and they only coincide because s == 1; this is the cross-check that
     * both operand orders were transcribed the right way round. */
    BrPathWalk(n, 7.0f);
    fromWalk = BrPathWalkPoint;
    BrPathWalkPoint.x = BrPathWalkPoint.y = BrPathWalkPoint.z = 0.0f;
    BrPathWalkFrom(n, 0, 1.0f, 7.0f);
    CHECK_F(BrPathWalkPoint.x, fromWalk.x);
    CHECK_F(BrPathWalkPoint.y, fromWalk.y);
    CHECK(BrPathWalkIndex == 0);

    BrPathWalk(n, 25.0f);
    fromWalk = BrPathWalkPoint;
    BrPathWalkFrom(n, 0, 1.0f, 25.0f);
    CHECK_F(BrPathWalkPoint.x, fromWalk.x);
    CHECK(BrPathWalkIndex == 1);

    /* resuming from an index skips everything before it */
    BrPathWalkFrom(n, 1, 1.0f, 0.0f);
    CHECK(BrPathWalkIndex == 1);
    CHECK_F(BrPathWalkPoint.x, 1.0f);

    /* s scales the FIRST segment only: with s = 0.5 the first segment is
     * worth 5 units, so t = 5 exhausts it exactly */
    BrPathWalkFrom(n, 0, 0.5f, 5.0f);
    CHECK(BrPathWalkIndex == 0);
    BrPathWalkFrom(n, 0, 0.5f, 6.0f);
    CHECK(BrPathWalkIndex == 1);          /* 1 unit into segment 1 of 20 */
    CHECK_F(BrPathWalkPoint.x, 1.0f + 1.0f / 20.0f);

    free(n);
    free(skip);
}

static void TestPathCrossings(void)
{
    BrNode *n = MakePath();
    int i;

    for (i = 0; i < BR_PATH_SEG_MAX; ++i) {
        BrPathSegs[i].a.x = (float)i;
        BrPathSegs[i].b.x = (float)i + 0.5f;
    }

    /* modulus 0 disables the test entirely (it guards the idiv) */
    BrPathSegCount = 0;
    g_segResult = 1;
    g_segCalls = 0;
    BrPathWalk(n, 25.0f);
    CHECK(g_segCalls == 0);
    CHECK(BrPathCrossCount == 0);
    CHECK(BrPathWrapCount == 0);

    /* nothing intersects -> the counters stay at their reset value but the
     * test is still run once per whole segment plus once for the partial */
    BrPathSegCount = 3;
    g_segResult = 0;
    g_segCalls = 0;
    BrPathWalk(n, 25.0f);
    CHECK(g_segCalls == 2);            /* one whole segment + the partial */
    CHECK(BrPathCrossCount == 0);
    CHECK(BrPathWrapCount == 0);
    CHECK(g_segIdx[0] == 1 && g_segIdx[1] == 1);   /* index never advances */

    /* everything intersects -> the index walks (n+1) % 3 and index 0 is
     * what bumps the wrap counter */
    g_segResult = 1;
    g_segCalls = 0;
    BrPathWalk(n, 55.0f);              /* 10 + 20 whole, then partial of 30 */
    CHECK(g_segCalls == 3);
    CHECK(BrPathCrossCount == 3);
    CHECK(g_segIdx[0] == 1 && g_segIdx[1] == 2 && g_segIdx[2] == 0);
    CHECK(BrPathWrapCount == 1);       /* only the index-0 hit counts */

    /* both counters are reset on entry, not accumulated across calls */
    g_segResult = 0;
    BrPathWalk(n, 5.0f);
    CHECK(BrPathCrossCount == 0);
    CHECK(BrPathWrapCount == 0);

    /* BrPathWalkFrom never touches the counters */
    g_segResult = 1;
    BrPathCrossCount = 77;
    BrPathWrapCount = 88;
    g_segCalls = 0;
    BrPathWalkFrom(n, 0, 1.0f, 25.0f);
    CHECK(g_segCalls == 0);
    CHECK(BrPathCrossCount == 77);
    CHECK(BrPathWrapCount == 88);

    free(n);
}

/* ================================================================== */
/* 7. Image tint scale                                                 */
/* ================================================================== */

static void TestTintScale(void)
{
    memset(&BrImgTintState, 0x5A, sizeof BrImgTintState);
    BrImgTintSetScale(11, 22, 33);
    CHECK(BrImgTintState.scaleR == 11);
    CHECK(BrImgTintState.scaleG == 22);
    CHECK(BrImgTintState.scaleB == 33);
    /* the slots are 0x00, 0x08 and 0x1C -- the gaps must NOT be written */
    CHECK(BrImgTintState.f04 == 0x5A5A5A5A);
    CHECK(BrImgTintState.f0C == 0x5A5A5A5A);
    CHECK(BrImgTintState.f18 == 0x5A5A5A5A);
    CHECK(BrImgTintState.width == 0x5A5A5A5A);
}

/* ================================================================== */

int main(void)
{
    TestSliders();
    TestGfx60E00();
    TestNodePass();
    TestZeroRegions();
    TestCarInit();
    TestBuildMatrices();
    TestApplyState();
    TestPredictRemote();
    TestNetSendState();
    TestPathWalk();
    TestPathCrossings();
    TestTintScale();

    printf("%s: %d checks, %d failures\n", g_fail ? "FAILED" : "ok",
           g_checks, g_fail);
    return g_fail ? 1 : 0;
}
