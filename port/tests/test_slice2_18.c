/* test_slice2_18.c -- behaviour tests for the agent-18 packet.
 *
 * These assert invariants that come out of the ORIGINAL's arithmetic (fixed
 * point scale factors, clamp asymmetries, ring wraparound, guFog identities),
 * not the particular numbers this port happens to produce.  Where an exact
 * constant is asserted it is one the original stores literally.
 *
 * All cross-slice callees are stood in for below; every stand-in is marked.
 */

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "slice2_18.h"

/* ================================================================== */
/* STAND-INS for cross-slice callees.  TEST-ONLY, not decompiled code. */
/* ================================================================== */

static int g_nStub8B80;
static int g_n42AF0;
static int g_n31227, g_n69580, g_n2C210, g_n60E00;
static int g_n35CE0, g_n35FC0;
static int g_nF900;
static const char *g_pszFatal;
static int32_t g_tick;
static uintptr_t g_lastSubmit;
static int g_nSubmit;

void BrStub8B80_0(void) { g_nStub8B80++; }
void BrStub8B80_1i(int32_t a0) { (void)a0; g_nStub8B80++; }
void BrStub8B80_1p(const void *p0) { (void)p0; g_nStub8B80++; }
void BrStub8B80_5i(int32_t a, int32_t b, int32_t c, int32_t d, int32_t e)
{ (void)a; (void)b; (void)c; (void)d; (void)e; g_nStub8B80++; }

void BrGfx42AF0_1(void *p0) { (void)p0; g_n42AF0++; }
void BrGfx42AF0_3(void *p0, int32_t a1, int32_t a2)
{ (void)p0; (void)a1; (void)a2; g_n42AF0++; }

void BrGfx2F900(uint32_t *pCmd,
                int32_t a01, int32_t a02, int32_t a03, int32_t a04,
                int32_t a05, int32_t a06, int32_t a07, int32_t a08,
                int32_t a09, int32_t a10, int32_t a11, int32_t a12,
                int32_t a13, int32_t a14, int32_t a15, int32_t a16)
{
    (void)a01; (void)a02; (void)a03; (void)a04;
    (void)a05; (void)a06; (void)a07; (void)a08;
    (void)a09; (void)a10; (void)a11; (void)a12;
    (void)a13; (void)a14; (void)a15; (void)a16;
    pCmd[0] = 0xDEADBEEFu;   /* the real one fills the slot; prove it got one */
    pCmd[1] = 0u;
    g_nF900++;
}

void BrGfx31227(void) { g_n31227++; }
void BrGfx69580(void) { g_n69580++; }
void BrGfx2C210(void) { g_n2C210++; }
void BrGfx60E00(void *p0) { (void)p0; g_n60E00++; }
int32_t BrTimeNow(void) { return ++g_tick; }
void BrFatal(const char *pszMsg) { g_pszFatal = pszMsg; }
void BrEnt35CE0(void *pThis) { (void)pThis; g_n35CE0++; }
void BrEnt35FC0(void *pThis) { (void)pThis; g_n35FC0++; }

/* Real 2D distance -- this one is cheap to reproduce exactly. */
float BrVec3DistXY(const BrVec3 *pA, const BrVec3 *pB)
{
    float dx = pA->x - pB->x;
    float dy = pA->y - pB->y;
    return (float)sqrt((double)dx * dx + (double)dy * dy);
}

void BrVec3Normalise(BrVec3 *pV)
{
    float l = (float)sqrt((double)pV->x * pV->x + (double)pV->y * pV->y
                          + (double)pV->z * pV->z);
    if (l != 0.0f) { pV->x /= l; pV->y /= l; pV->z /= l; }
}

/* Stand-in transform: emits (0, 0, g_clipZ, g_clipW) so the fog test can
 * drive z/w directly. */
static float g_clipZ = 0.0f, g_clipW = 1.0f;
void BrMat4TransformPoint4(float pOut[4], const BrVec3 *pV, const float *pM)
{
    (void)pV; (void)pM;
    pOut[0] = 0.0f; pOut[1] = 0.0f; pOut[2] = g_clipZ; pOut[3] = g_clipW;
}

static void SubmitHook(uintptr_t p) { g_lastSubmit = p; g_nSubmit++; }

/* ================================================================== */
/* Fixtures                                                           */
/* ================================================================== */

static uint8_t  g_dlArena[0x8000];
static float    g_camRec[24];    /* BrG_6C6490 points here; +0x30, +0x34 used */
static float    g_curveRec[24];  /* BrG_6C2CF8 points here; +0x38 used */
static uint8_t  g_hwBuf[4096];
static int32_t  g_frameRec[BR_S18_FRAMEREC_DWORDS];

static void ResetAll(void)
{
    memset(g_dlArena, 0, sizeof g_dlArena);
    memset(g_camRec, 0, sizeof g_camRec);
    memset(g_curveRec, 0, sizeof g_curveRec);
    memset(g_frameRec, 0, sizeof g_frameRec);
    memset(BrG_6C1788, 0, sizeof BrG_6C1788);
    memset(BrG_6C1588, 0, sizeof BrG_6C1588);

    BrG_6C0944 = g_dlArena;
    BrG_6C65EC = 0;
    BrG_6C0680 = (uint32_t *)(void *)(g_dlArena + 0x200);

    BrG_6C661C = BrG_6C6620 = BrG_6C6624 = BrG_6C6618 = 0;
    BrG_0B4050 = 0;
    BrG_0A79CC = 0;
    BrG_0B380C = 0;
    BrG_6C6490 = g_camRec;
    BrG_6C2CF8 = g_curveRec;
    BrG_4B0378.x = BrG_4B0378.y = BrG_4B0378.z = 0.0f;
    BrG_6C7C80 = 0.0f;
    BrG_6C7C84 = 1.0f;
    BrG_6C7CC8[0] = BrG_6C7CC8[1] = BrG_6C7CC8[2] = 0;

    BrG_6C65E0 = BrG_6C65E4 = BrG_6C65E8 = 0;
    BrG_0AA8B4 = 0;
    BrG_0A81C0 = 640;
    BrG_0A81C4 = 480;
    BrG_6C299C = 480;
    BrG_6C0684 = 640;
    BrG_0AA890 = 0;
    BrG_0AA730 = NULL;
    BrG_0AA884 = 0;
    BrG_0AA770 = g_hwBuf;
    BrG_6C65FC = 0;
    BrG_6C6604 = 0;

    BrG_575508 = 0;    BrG_575500 = 640;
    BrG_57550C = 0;    BrG_5754FC = 480;

    BrG_6C6654 = 0;
    BrG_6C3364 = 0;
    BrG_6C1174 = 0;

    BrG_6C56E8 = 0;
    BrG_0B5D90 = 0xFFFF;
    BrG_691000 = g_hwBuf;
    BrG_6C65A0 = g_hwBuf;
    BrG_6C6678 = g_hwBuf;

    BrG_0AA728 = 0x1000;
    BrG_0AA72C = 0x2000;
    BrG_6C6668 = 0;
    BrG_6C6660 = BrG_6C6658 = BrG_6C665C = 0;
    BrG_2E5EC8 = g_hwBuf;      BrG_363FF0 = g_hwBuf + 80;   /* -> 10 */
    BrG_3643BC = g_hwBuf;      BrG_364304 = g_hwBuf + 320;  /* -> 10 */
    BrG_6C1170 = 0;
    BrG_6C6664 = 1;
    BrG_6C33A0 = g_hwBuf;
    BrG_6C3380 = g_hwBuf;
    BrG_6C198C = NULL;
    BrG_6C1608 = 0;
    BrG_6C020C = BrG_6C0208 = BrG_6C1620 = 0;
    BrG_0ADFC0 = 0;
    BrG_AA4020 = BrG_AA3760 = BrG_AA3D50 = BrG_AA3490 = g_hwBuf;
    BrG_6C65F4 = BrG_6C65F8 = 0;
    BrG_6C6598 = 0;
    BrG_6C56E4 = 0;
    BrG_B501D0 = SubmitHook;

    g_pszFatal = NULL;
    g_tick = 0;
    g_nSubmit = 0;
    g_lastSubmit = 0;
    g_nStub8B80 = g_n42AF0 = g_n31227 = g_n69580 = 0;
    g_n2C210 = g_n60E00 = g_n35CE0 = g_n35FC0 = g_nF900 = 0;
}

static uint32_t *DlStart(void) { return (uint32_t *)(void *)(g_dlArena + 0x200); }
static ptrdiff_t DlWords(void) { return BrG_6C0680 - DlStart(); }

static int g_nFail;
#define CHECK(cond) do {                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);         \
            g_nFail++;                                                     \
        }                                                                  \
    } while (0)

/* ================================================================== */
/* 0x10031866  BrFogUpdate                                            */
/* ================================================================== */

/* The "everything off" branch stores near=0, far=1000 literally, so the guFog
 * pair is forced: fm = 500*256/1000 = 128, fo = (500-0)*256/1000 = 128. */
static void TestFogOffBranch(void)
{
    uint32_t *p;
    ResetAll();
    BrFogUpdate();

    CHECK(BrG_6C3398 == 0);
    CHECK(BrG_6C64D8 == 1000);
    CHECK(BrG_6C2CF4 == 128);
    CHECK(BrG_6C1618 == 128);
    CHECK(BrG_690BE8 == 0xFF);
    CHECK(BrG_6C0260 == 0 && BrG_6C1614 == 0 && BrG_6C0200 == 0);

    CHECK(DlWords() == 4);                 /* exactly two 8-byte commands */
    p = DlStart();
    CHECK(p[0] == 0xBC000008u);            /* G_MOVEWD / G_MW_FOG */
    CHECK(p[1] == ((128u << 16) | 128u));
    CHECK(p[2] == 0xF8000000u);            /* G_SETFOGCOLOR */
    CHECK(p[3] == 0x000000FFu);            /* alpha is forced to 0xFF */
}

/* guFog identity, checked across every branch: whatever near/far a branch
 * picks, fm and fo must satisfy fm == 500*256/span and fo == (500-near)*256
 * /span, and both must fit the 16-bit fields the command packs them into. */
static void TestFogGuFogIdentity(void)
{
    int32_t *apSel[4];
    int i, j;

    apSel[0] = &BrG_6C661C;
    apSel[1] = &BrG_6C6620;
    apSel[2] = &BrG_6C6624;
    apSel[3] = &BrG_6C6618;

    for (i = 0; i < 4; i++) {
        for (j = 0; j <= 2; j += 2) {      /* BrG_0B4050 both ways */
            int32_t span;
            uint32_t *p;

            ResetAll();
            *apSel[i] = 1;
            BrG_0B4050 = j;
            BrFogUpdate();

            span = BrG_6C64D8 - BrG_6C3398;
            CHECK(span > 0);
            CHECK(BrG_6C2CF4 == 0x1F400 / span);
            CHECK(BrG_6C1618 == ((0x1F4 - BrG_6C3398) << 8) / span);
            CHECK(BrG_6C2CF4 >= 0 && BrG_6C2CF4 <= 0xFFFF);

            p = DlStart();
            CHECK(p[0] == 0xBC000008u);
            CHECK((p[1] >> 16) == ((uint32_t)BrG_6C2CF4 & 0xFFFFu));
            CHECK(p[2] == 0xF8000000u);
            CHECK((p[3] & 0xFFu) == 0xFFu);
            /* the packed colour must be exactly the three globals */
            CHECK((p[3] >> 24) == BrG_6C0260);
            CHECK(((p[3] >> 16) & 0xFFu) == BrG_6C1614);
            CHECK(((p[3] >> 8) & 0xFFu) == BrG_6C0200);
        }
    }
}

/* Branch priority: 0x106C661C beats everything below it. */
static void TestFogBranchPriority(void)
{
    ResetAll();
    BrG_6C661C = 1; BrG_6C6620 = 1; BrG_6C6624 = 1; BrG_6C6618 = 1;
    BrFogUpdate();
    CHECK(BrG_6C0260 == 0 && BrG_6C1614 == 0 && BrG_6C0200 == 0);
    CHECK(BrG_690BE8 == 0x40);
    CHECK(BrG_6C64D8 == 0x3FC);

    ResetAll();
    BrG_6C6620 = 1; BrG_6C6624 = 1; BrG_6C6618 = 1;
    BrFogUpdate();
    CHECK(BrG_6C0260 == 0xB8 && BrG_6C1614 == 0xB8 && BrG_6C0200 == 0xD8);
}

/* The 0x106C6624 lerp only runs for a POSITIVE ODD 0x100A79CC; and at
 * distance 0 the weight is exactly 1, which lands the colour exactly on the
 * three literal targets 240/248/255. */
static void TestFogDistanceLerpEndpoints(void)
{
    ResetAll();
    BrG_6C6624 = 1;
    BrG_0A79CC = 2;                       /* even -> no lerp */
    BrFogUpdate();
    CHECK(BrG_6C0260 == 0x60 && BrG_6C1614 == 0x68 && BrG_6C0200 == 0x70);

    ResetAll();
    BrG_6C6624 = 1;
    BrG_0A79CC = -1;                      /* negative -> no lerp, despite &1 */
    BrFogUpdate();
    CHECK(BrG_6C0260 == 0x60);

    ResetAll();
    BrG_6C6624 = 1;
    BrG_0A79CC = 1;
    g_camRec[0x30 / 4] = 0.0f;            /* distance 0 -> t == 1 */
    g_camRec[0x34 / 4] = 0.0f;
    BrFogUpdate();
    CHECK(BrG_6C0260 == 240);
    CHECK(BrG_6C1614 == 248);
    CHECK(BrG_6C0200 == 255);

    ResetAll();
    BrG_6C6624 = 1;
    BrG_0A79CC = 1;
    g_camRec[0x30 / 4] = 1.0e6f;          /* far away -> t -> 0 */
    BrFogUpdate();
    CHECK(BrG_6C0260 == 0x60);
    CHECK(BrG_6C1614 == 0x68);
    CHECK(BrG_6C0200 == 0x70);
}

/* The 0x106C6618 shade is a clamped 0..255 with the boundaries taken from the
 * original's own compares (jge 0 / jle 0xFF). */
static void TestFogShadeClamp(void)
{
    ResetAll();
    BrG_6C6618 = 1;
    BrG_0B380C = 1;                        /* colours copied verbatim */
    BrG_6C7CC8[0] = 11; BrG_6C7CC8[1] = 22; BrG_6C7CC8[2] = 33;
    BrG_6C7C80 = 0.0f; BrG_6C7C84 = 1.0f;

    g_curveRec[0x38 / 4] = -5.0f;          /* -> below zero */
    BrFogUpdate();
    CHECK(BrG_690BE8 == 0);
    CHECK(BrG_6C0260 == 11 && BrG_6C1614 == 22 && BrG_6C0200 == 33);

    ResetAll();
    BrG_6C6618 = 1; BrG_0B380C = 1;
    BrG_6C7C80 = 0.0f; BrG_6C7C84 = 1.0f;
    g_curveRec[0x38 / 4] = 5.0f;           /* -> above 255 */
    BrFogUpdate();
    CHECK(BrG_690BE8 == 0xFF);

    ResetAll();
    BrG_6C6618 = 1; BrG_0B380C = 1;
    BrG_6C7C80 = 0.0f; BrG_6C7C84 = 1.0f;
    g_curveRec[0x38 / 4] = 0.5f;           /* 0.5*255 = 127.5 -> trunc 127 */
    BrFogUpdate();
    CHECK(BrG_690BE8 == 127);
}

/* ================================================================== */
/* 0x10031D3F  BrFogFactorAtPoint                                     */
/* ================================================================== */

static void TestFogFactor(void)
{
    BrVec3 pt = { 1.0f, 2.0f, 3.0f };
    float a, b;

    ResetAll();
    BrG_6C6618 = 0;
    g_clipZ = 0.5f; g_clipW = 1.0f;
    CHECK(BrFogFactorAtPoint(&pt) == 0.0f);   /* fog off short-circuits */

    /* With fog on, the factor must use exactly the fm/fo pair the RDP got. */
    ResetAll();
    BrG_6C6618 = 1;
    BrFogUpdate();                            /* sets 6C2CF4 / 6C1618 */
    CHECK(BrG_6C2CF4 > 0);

    g_clipW = 1.0f;

    /* Clamped at both ends, and monotone non-decreasing in z between. */
    g_clipZ = -1.0e6f;
    CHECK(BrFogFactorAtPoint(&pt) == 0.0f);
    g_clipZ = 1.0e6f;
    CHECK(BrFogFactorAtPoint(&pt) == 1.0f);

    {
        float prev = 0.0f;
        int i;
        for (i = 0; i <= 40; i++) {
            float v;
            g_clipZ = (float)i / 40.0f;
            v = BrFogFactorAtPoint(&pt);
            CHECK(v >= 0.0f && v <= 1.0f);
            CHECK(v >= prev);
            prev = v;
        }
    }

    /* Agreement with the closed form the original computes. */
    g_clipZ = 0.25f; g_clipW = 2.0f;
    a = BrFogFactorAtPoint(&pt);
    b = (float)((((0.25 / 2.0) * (double)(float)BrG_6C2CF4)
                 + (double)(float)BrG_6C1618) * 0.003921568859368563);
    if (b < 0.0f) b = 0.0f;
    if (b > 1.0f) b = 1.0f;
    CHECK(fabs((double)a - (double)b) < 1e-6);
}

/* ================================================================== */
/* 0x10031DCF  BrHudColorsUpdate                                      */
/* ================================================================== */

/* The default ramp branch is a 1/4, 1/2, 3/4, 1 staircase of the base
 * colour, and the packed words must be a faithful repacking of it. */
static void TestHudRamp(void)
{
    int i;

    ResetAll();
    BrHudColorsUpdate();                 /* "all selectors off" branch */
    CHECK(BrG_6C1580 == 0xFF && BrG_6C335C == 0xFF && BrG_6C0968 == 0xCC);
    CHECK(BrG_690BF0 == 0x66 && BrG_6C0960 == 0x66 && BrG_6C65BC == 0x77);

    CHECK(BrG_690FF8[3] == BrG_6C1580);
    CHECK(BrG_690FF8[1] == (uint8_t)(BrG_6C1580 >> 1));
    CHECK(BrG_690FF8[0] == (uint8_t)(BrG_6C1580 >> 2));
    CHECK(BrG_690FF8[2] == (uint8_t)(BrG_690FF8[1] + BrG_690FF8[0]));

    for (i = 0; i < 4; i++) {
        CHECK((BrG_6C0950[i] >> 24) == BrG_690FF8[i]);
        CHECK(((BrG_6C0950[i] >> 16) & 0xFFu) == BrG_6C6494[i]);
        CHECK(((BrG_6C0950[i] >> 8) & 0xFFu) == BrG_6C3358[i]);
        CHECK((BrG_6C0950[i] & 0xFFu) == 0u);   /* alpha is never set here */
    }
    CHECK((BrG_6C5AB0 >> 24) == BrG_6C1580);
    CHECK((BrG_6C29E8 >> 24) == BrG_690BF0);

    /* The ramp is monotone non-decreasing. */
    for (i = 1; i < 4; i++) {
        CHECK(BrG_690FF8[i] >= BrG_690FF8[i - 1]);
        CHECK(BrG_6C6494[i] >= BrG_6C6494[i - 1]);
    }
}

/* The 0x106C6618 branch is an integer blend weighted by 0x10690BE8/255, so
 * the two endpoints of that weight are exact. */
static void TestHudBlendEndpoints(void)
{
    ResetAll();
    BrG_6C6618 = 1;
    BrG_690BE8 = 0;                     /* weight 0 -> the fixed colour */
    BrG_6C0260 = 0; BrG_6C1614 = 0; BrG_6C0200 = 0;
    BrHudColorsUpdate();
    CHECK(BrG_6C1580 == 0xFF);
    CHECK(BrG_6C335C == 0xFF);
    CHECK(BrG_6C0968 == 0xCC);
    CHECK(BrG_690BF0 == 0x66);
    CHECK(BrG_6C65BC == 0x77);

    ResetAll();
    BrG_6C6618 = 1;
    BrG_690BE8 = 0xFF;                  /* weight 1 -> the derived term */
    BrG_6C0260 = 0xFF; BrG_6C1614 = 0xFF; BrG_6C0200 = 0xFF;
    BrHudColorsUpdate();
    CHECK(BrG_6C1580 == 0xFF);          /* (255+255)>>1 == 255 */
    CHECK(BrG_6C0968 == (uint8_t)((0xFF + 0xCC) >> 1));
    CHECK(BrG_690BF0 == (uint8_t)((0xFF * 4) / 5));

    /* Whatever the weight, every output stays a byte. */
    {
        int a;
        for (a = 0; a <= 255; a += 17) {
            ResetAll();
            BrG_6C6618 = 1;
            BrG_690BE8 = (uint8_t)a;
            BrG_6C0260 = 0xFF; BrG_6C1614 = 0x80; BrG_6C0200 = 0x00;
            BrHudColorsUpdate();
            CHECK(BrG_6C1580 <= 0xFF && BrG_690BF0 <= 0xFF);
        }
    }
}

/* ================================================================== */
/* 0x1003289F  BrScissorSet                                           */
/* ================================================================== */

static void DecodeScissor(uint32_t *p, int32_t *px0, int32_t *py0,
                          int32_t *px1, int32_t *py1)
{
    *px0 = (int32_t)((p[0] >> 12) & 0xFFFu);
    *py0 = (int32_t)(p[0] & 0xFFFu);
    *px1 = (int32_t)((p[1] >> 12) & 0xFFFu);
    *py1 = (int32_t)(p[1] & 0xFFFu);
}

static void TestScissorClamp(void)
{
    uint32_t *p;
    int32_t x0, y0, x1, y1;

    /* Inside the bounds: passes through untouched. */
    ResetAll();
    BrScissorSet(10, 20, 100, 50);
    CHECK(DlWords() == 4);
    p = DlStart();
    CHECK(p[0] == 0xE7000000u && p[1] == 0u);      /* pipesync comes first */
    DecodeScissor(p + 2, &x0, &y0, &x1, &y1);
    CHECK(x0 == 10 && y0 == 20 && x1 == 110 && y1 == 70);

    /* Overhanging the high edge: only the extent shrinks. */
    ResetAll();
    BrScissorSet(600, 400, 200, 200);
    DecodeScissor(DlStart() + 2, &x0, &y0, &x1, &y1);
    CHECK(x0 == 600 && x1 == 640);
    CHECK(y0 == 400 && y1 == 480);

    /* GOTCHA: entirely off the LOW edge collapses to zero size AT the low
     * edge -- the origin moves, the extent absorbs the whole correction and
     * then floors at 0. */
    ResetAll();
    BrG_575508 = 100; BrG_57550C = 100;
    BrScissorSet(0, 0, 50, 50);
    DecodeScissor(DlStart() + 2, &x0, &y0, &x1, &y1);
    CHECK(x0 == 100 && x1 == 100);
    CHECK(y0 == 100 && y1 == 100);

    /* A negative extent floors at zero, never wraps. */
    ResetAll();
    BrScissorSet(50, 50, -30, -30);
    DecodeScissor(DlStart() + 2, &x0, &y0, &x1, &y1);
    CHECK(x0 == 50 && x1 == 50 && y0 == 50 && y1 == 50);

    /* GOTCHA, and the mirror image of the previous case: a rectangle entirely
     * off the HIGH edge is NOT pulled back.  Only the extent is corrected
     * there, so the origin keeps its out-of-bounds position and the rectangle
     * collapses to zero size WHERE IT IS, outside the bounds.  The low edge
     * moves the origin; the high edge never does. */
    ResetAll();
    BrScissorSet(800, 700, 300, 300);
    DecodeScissor(DlStart() + 2, &x0, &y0, &x1, &y1);
    CHECK(x0 == 800 && x1 == 800);        /* > BrG_575500 (640), on purpose */
    CHECK(y0 == 700 && y1 == 700);        /* > BrG_5754FC (480), on purpose */

    /* What DOES hold for arbitrary inputs: the origin never falls below the
     * low bound, the extent is never negative, and the far edge is either
     * inside the high bound or the rectangle has collapsed to nothing. */
    {
        int i, j;
        for (i = -200; i <= 800; i += 137) {
            for (j = -200; j <= 800; j += 173) {
                ResetAll();
                BrScissorSet(i, j, 300, 300);
                DecodeScissor(DlStart() + 2, &x0, &y0, &x1, &y1);
                CHECK(x0 >= BrG_575508);
                CHECK(x1 >= x0);
                CHECK(x1 <= BrG_575500 || x1 == x0);
                CHECK(y0 >= BrG_57550C);
                CHECK(y1 >= y0);
                CHECK(y1 <= BrG_5754FC || y1 == y0);
            }
        }
    }
}

/* Hi-res doubles the whole rectangle AFTER clamping, so the emitted numbers
 * are twice the clamped ones -- and can therefore exceed the bounds. */
static void TestScissorHiRes(void)
{
    int32_t x0, y0, x1, y1;

    ResetAll();
    BrG_6C65E4 = 1;
    BrScissorSet(10, 20, 100, 50);
    DecodeScissor(DlStart() + 2, &x0, &y0, &x1, &y1);
    CHECK(x0 == 20 && y0 == 40 && x1 == 220 && y1 == 140);
}

/* ================================================================== */
/* 0x10032A42 / 0x10032C38 / 0x10032DF2  viewports                    */
/* ================================================================== */

static void TestViewportEdges(void)
{
    BrVpRec *pVp;
    int32_t left, right;

    /* vscale/vtrans are half-extent and centre in 2.2 fixed point, so the
     * two edges are recoverable exactly: centre -+ half == edge * 4. */
    ResetAll();
    BrViewportSet(32, 16, 200, 100, 0);
    pVp = &BrG_6C1788[BrG_6C6654];
    CHECK(pVp->vscale[0] == 400);        /* (w/2) * 4 */
    CHECK(pVp->vscale[1] == 200);
    CHECK(pVp->vscale[2] == 0x1FF);
    CHECK(pVp->vscale[3] == 0);
    CHECK(pVp->vtrans[2] == 0x1FF);
    CHECK(pVp->vtrans[3] == 0);

    left  = pVp->vtrans[0] - pVp->vscale[0];
    right = pVp->vtrans[0] + pVp->vscale[0];
    CHECK(left  == 32 * 4);
    CHECK(right == (32 + 200) * 4);
    left  = pVp->vtrans[1] - pVp->vscale[1];
    right = pVp->vtrans[1] + pVp->vscale[1];
    CHECK(left  == 16 * 4);
    CHECK(right == (16 + 100) * 4);

    CHECK(BrG_6C62D8 == 0x1FF);
    CHECK(BrG_6C65B8 == 0x1FF);
    CHECK(BrG_6C1174 == 0);
}

/* A negative width mirrors: the flag is raised, vscale.x goes negative, and
 * the centre is still computed from |w| so the covered span is unchanged. */
static void TestViewportMirror(void)
{
    BrVpRec *pVp;
    int16_t s, t;

    ResetAll();
    BrViewportSet(32, 16, 200, 100, 0);
    pVp = &BrG_6C1788[BrG_6C6654];
    s = pVp->vscale[0];
    t = pVp->vtrans[0];

    ResetAll();
    BrViewportSet(32, 16, -200, 100, 0);
    pVp = &BrG_6C1788[BrG_6C6654];
    CHECK(BrG_6C1174 == 1);
    CHECK(pVp->vscale[0] == -s);
    CHECK(pVp->vtrans[0] == t);          /* same centre, mirrored scale */

    /* BrG_6C3364 negates vscale.x in BOTH paths, so it flips an already
     * mirrored viewport back to unmirrored. */
    ResetAll();
    BrG_6C3364 = 1;
    BrViewportSet(32, 16, -200, 100, 0);
    pVp = &BrG_6C1788[BrG_6C6654];
    CHECK(BrG_6C1174 == 1);
    CHECK(pVp->vscale[0] == s);
}

static void TestViewportRing(void)
{
    int32_t first, i;

    ResetAll();
    BrViewportSet(0, 0, 64, 64, 0);
    first = BrG_6C6654;
    for (i = 0; i < BR_S18_VP_SLOTS; i++) {
        BrViewportSet(0, 0, 64, 64, 0);
        CHECK(BrG_6C6654 >= 0 && BrG_6C6654 < BR_S18_VP_SLOTS);
    }
    CHECK(BrG_6C6654 == first);          /* 32 advances wrap exactly */
}

static void TestViewportScissorUsesAbsWidth(void)
{
    int32_t x0, y0, x1, y1;

    /* fScissor with a negative width must scissor |w|, not w. */
    ResetAll();
    BrViewportSet(10, 10, -100, 60, 1);
    /* pipesync + scissor + movemem = 6 words */
    CHECK(DlWords() == 6);
    DecodeScissor(DlStart() + 2, &x0, &y0, &x1, &y1);
    CHECK(x0 == 10 && x1 == 110);
    CHECK(y0 == 10 && y1 == 70);
}

static void TestViewportFullIgnoresArgs(void)
{
    BrVpRec a, b;

    ResetAll();
    BrViewportSetFull(0, 0, 1, 1, 0);
    a = BrG_6C1788[BrG_6C6654];

    ResetAll();
    BrViewportSetFull(-999, 12345, 7, -7, 0);
    b = BrG_6C1788[BrG_6C6654];

    CHECK(memcmp(&a, &b, sizeof a) == 0);
    CHECK(a.vscale[0] == (int16_t)(640 * 2));
    CHECK(a.vscale[1] == (int16_t)(480 * 2));
    CHECK(a.vtrans[0] == (int16_t)((640 / 2) * 4));
    CHECK(a.vtrans[1] == (int16_t)((480 / 2) * 4));
    CHECK(BrG_6C1174 == 0);

    /* And it always emits its own pipesync when scissoring, unlike
     * BrViewportSet. */
    ResetAll();
    BrViewportSetFull(0, 0, 100, 100, 1);
    CHECK(DlStart()[0] == 0xE7000000u);
}

static void TestViewportReEmitIsPure(void)
{
    BrVpRec before[BR_S18_VP_SLOTS];
    int32_t idx;
    uint32_t *p;

    ResetAll();
    BrViewportSet(8, 8, 128, 96, 0);
    idx = BrG_6C6654;
    memcpy(before, BrG_6C1788, sizeof before);
    BrG_6C0680 = DlStart();

    BrViewportReEmit();
    CHECK(BrG_6C6654 == idx);                       /* no ring advance */
    CHECK(memcmp(before, BrG_6C1788, sizeof before) == 0);
    CHECK(DlWords() == 2);
    p = DlStart();
    CHECK(p[0] == 0x03800010u);
    CHECK(p[1] == (uint32_t)(uintptr_t)&BrG_6C1788[idx]);
    CHECK(BrG_6C62D8 == 0x1FF && BrG_6C65B8 == 0x1FF);
}

/* ================================================================== */
/* 0x100322E6  BrFrameBegin                                           */
/* ================================================================== */

static void TestFrameBegin(void)
{
    ResetAll();
    BrG_0AA8B4 = 1;
    BrG_6C65E4 = 0;
    BrG_6C0680 = NULL;                    /* prove it gets reset */
    BrFrameBegin(g_frameRec, 0);

    CHECK(BrG_6C0680 > DlStart());        /* reset then advanced */
    CHECK(g_frameRec[0] == 0 && g_frameRec[1] == 0);
    CHECK(g_frameRec[2] == 640 && g_frameRec[3] == 480);
    CHECK(g_n31227 == 1 && g_n69580 == 1 && g_nF900 == 1);
    CHECK(DlStart()[0] == 0xBC000006u);   /* G_MOVEWD / G_MW_SEGMENT first */
    CHECK(BrG_6C65E0 == 0);               /* flag unchanged -> no reload */

    /* Changing the flag latches it and arms the countdown. */
    ResetAll();
    BrG_0AA8B4 = 1;
    BrFrameBegin(g_frameRec, 1);
    CHECK(BrG_6C65E0 == 1);
    CHECK(BrG_6C65E4 == 1);

    /* Mode 2 fills the far fields too. */
    ResetAll();
    BrG_0AA8B4 = 2;
    BrG_6C299C = 480;
    BrG_6C0684 = 640;
    BrFrameBegin(g_frameRec, 0);
    CHECK(g_frameRec[0] == 8 && g_frameRec[1] == 8);
    CHECK(g_frameRec[2] == 640 - 0x60);
    CHECK(g_frameRec[3] == 240 - 8);
    CHECK(g_frameRec[0x58 / 4] == 8);
    CHECK(g_frameRec[0x5C / 4] == 241);
    CHECK(g_frameRec[0x60 / 4] == 640 - 0x60);
    CHECK(g_frameRec[0x64 / 4] == 240 - 8);

    /* Any other mode leaves the record alone. */
    ResetAll();
    BrG_0AA8B4 = 7;
    memset(g_frameRec, 0x5A, sizeof g_frameRec);
    BrFrameBegin(g_frameRec, 0);
    CHECK(g_frameRec[0] == 0x5A5A5A5A);

    /* 0x100AA890 selects 0x2000 or 0, nothing else. */
    ResetAll();
    BrG_0AA890 = 99;
    BrFrameBegin(g_frameRec, 0);
    CHECK(BrG_6C0258 == 0x2000u);
    ResetAll();
    BrG_0AA890 = 0;
    BrFrameBegin(g_frameRec, 0);
    CHECK(BrG_6C0258 == 0u);
}

static void TestFrameBeginWrappers(void)
{
    ResetAll();
    BrFrameBeginRec(g_frameRec);
    CHECK(BrG_6C65E4 == 0);

    ResetAll();
    BrFrameBeginHiRes();
    CHECK(BrG_6C65E4 == 1);

    BrFrameNop();     /* empty in this build; must simply return */
    BrGfxNopA();
    BrGfxNopB();
}

/* ================================================================== */
/* 0x10033838  BrFrameEnd                                             */
/* ================================================================== */

static void TestFrameEndTask(void)
{
    BrOsTask *pT;
    ptrdiff_t words;

    ResetAll();
    BrG_6C6664 = 1;
    BrG_6C0680 = DlStart() + 20;          /* pretend 10 commands are queued */
    BrFrameEnd();

    pT = &BrG_6C1588[0];                  /* index used is the PRE-flip one */
    CHECK(pT->type == 1);
    CHECK(pT->flags == 6);                /* 2 then |= 4 */
    CHECK(pT->ucode_size == 0x1000);
    CHECK(pT->ucode_data_size == 0x800);
    CHECK(pT->dram_stack_size == 0x400);
    CHECK(pT->dram_stack == 0x106C2D00u); /* the folded align-up constant */
    CHECK(pT->data_ptr == (uintptr_t)DlStart());

    words = 20 + 4;                       /* the two closing commands */
    CHECK(pT->data_size == (uint32_t)(words * 4));
    CHECK((pT->data_size & 7u) == 0u);    /* always a multiple of 8 */
    CHECK(BrG_6C1170 == (int32_t)(words / 2));
    CHECK(BrG_6C6660 == BrG_6C1170);      /* high-water picked up */
    CHECK(BrG_6C6658 == 10);
    CHECK(BrG_6C665C == 10);

    CHECK(g_nSubmit == 1);
    CHECK(g_lastSubmit == pT->data_ptr);
    CHECK(BrG_6C65EC == 1);               /* buffer index flipped */
    CHECK(g_pszFatal == NULL);
}

/* The closing pair really is G_RDPFULLSYNC then G_ENDDL. */
static void TestFrameEndClosers(void)
{
    uint32_t *p;
    ResetAll();
    BrG_6C0680 = DlStart();
    BrFrameEnd();
    p = DlStart();
    CHECK(p[0] == 0xE9000000u && p[1] == 0u);
    CHECK(p[2] == 0xB8000000u && p[3] == 0u);
}

/* GOTCHA: the frame is DROPPED while 0x106C6664 is zero -- the counter is
 * merely bumped and nothing is submitted. */
static void TestFrameEndSkipsFirstFrame(void)
{
    ResetAll();
    BrG_6C6664 = 0;
    BrG_6C0680 = DlStart() + 8;
    BrFrameEnd();
    CHECK(BrG_6C6664 == 1);
    CHECK(g_n2C210 == 0);
    CHECK(BrG_6C65EC == 1);               /* but the buffer still flips */
    CHECK(g_nSubmit == 1);                /* and the task is still submitted */
}

/* The DL-length guard fires above 0x2EE0 commands and not at it. */
static void TestFrameEndGlistGuard(void)
{
    ResetAll();
    BrG_6C0944 = g_dlArena;
    BrG_6C0680 = DlStart() + (BR_S18_GLIST_LIMIT - 2) * 2;   /* +2 closers */
    BrFrameEnd();
    CHECK(BrG_6C1170 == BR_S18_GLIST_LIMIT);
    CHECK(g_pszFatal == NULL);            /* jle -> exactly at the limit is ok */

    ResetAll();
    BrG_6C0680 = DlStart() + (BR_S18_GLIST_LIMIT - 1) * 2;
    BrFrameEnd();
    CHECK(BrG_6C1170 == BR_S18_GLIST_LIMIT + 1);
    CHECK(g_pszFatal != NULL);
    CHECK(strcmp(g_pszFatal, "HUGE GLIST ERROR") == 0);
}

/* The countdown only fires its one-shot on the transition to zero. */
static void TestFrameEndCountdown(void)
{
    ResetAll();
    BrG_6C6664 = 1;
    BrG_6C65E0 = 2;
    BrG_6C65E4 = 1;
    BrG_6C65E8 = 0;
    BrG_6C0680 = DlStart();
    BrFrameEnd();
    CHECK(BrG_6C65E0 == 1);
    CHECK(BrG_6C65E8 == 0);               /* not yet */

    BrG_6C65EC = 0;
    BrG_6C0680 = DlStart();
    BrFrameEnd();
    CHECK(BrG_6C65E0 == 0);
    CHECK(BrG_6C65E8 == 1);               /* latched on reaching zero */
}

/* 0x106C65F8 is a saturating-at-zero countdown. */
static void TestFrameEndF8Countdown(void)
{
    ResetAll();
    BrG_6C6664 = 1;
    BrG_6C65F8 = 2;
    BrG_6C0680 = DlStart();
    BrFrameEnd();
    CHECK(BrG_6C65F8 == 1);
    BrG_6C65EC = 0; BrG_6C0680 = DlStart();
    BrFrameEnd();
    CHECK(BrG_6C65F8 == 0);
    BrG_6C65EC = 0; BrG_6C0680 = DlStart();
    BrFrameEnd();
    CHECK(BrG_6C65F8 == 0);               /* never goes negative */
}

/* The one-shot at 0x106C198C runs once and clears itself. */
static int g_nOneShot;
static void OneShot(void) { g_nOneShot++; }

static void TestFrameEndOneShot(void)
{
    ResetAll();
    g_nOneShot = 0;
    BrG_6C6664 = 1;
    BrG_6C198C = OneShot;
    BrG_6C0680 = DlStart();
    BrFrameEnd();
    CHECK(g_nOneShot == 1);
    CHECK(BrG_6C198C == NULL);

    BrG_6C65EC = 0; BrG_6C0680 = DlStart();
    BrFrameEnd();
    CHECK(g_nOneShot == 1);               /* not re-run */
}

/* ================================================================== */
/* HUD text                                                           */
/* ================================================================== */

static void TestHudText(void)
{
    ResetAll();
    CHECK(BrG_6C56E8 == 0);
    BrHudTextBegin();
    CHECK(BrG_6C56E8 == 1);
    CHECK(BrG_0B5D90 == 0);
    CHECK(g_n42AF0 == 1);

    BrHudTextBegin();                     /* idempotent while open */
    CHECK(g_n42AF0 == 1);

    ResetAll();
    BrHudTextEnd();
    CHECK(BrG_6C56E8 == 0);               /* opened then closed */
    CHECK(BrG_0B5D90 == 1);
    CHECK(g_n42AF0 == 2);                 /* the open plus the close */
    CHECK(g_n60E00 == 1);

    /* Closing while already open must not re-open. */
    ResetAll();
    BrHudTextBegin();
    BrHudTextEnd();
    CHECK(g_n42AF0 == 2);
    CHECK(BrG_6C56E8 == 0);

    ResetAll();
    BrHudDrawAll();
    CHECK(g_n35CE0 == 1);                 /* loop bound is a literal 1 */
    CHECK(g_n35FC0 == 1);
    CHECK(BrG_6C56E8 == 0);
}

/* ================================================================== */

int main(void)
{
    TestFogOffBranch();
    TestFogGuFogIdentity();
    TestFogBranchPriority();
    TestFogDistanceLerpEndpoints();
    TestFogShadeClamp();
    TestFogFactor();
    TestHudRamp();
    TestHudBlendEndpoints();
    TestScissorClamp();
    TestScissorHiRes();
    TestViewportEdges();
    TestViewportMirror();
    TestViewportRing();
    TestViewportScissorUsesAbsWidth();
    TestViewportFullIgnoresArgs();
    TestViewportReEmitIsPure();
    TestFrameBegin();
    TestFrameBeginWrappers();
    TestFrameEndTask();
    TestFrameEndClosers();
    TestFrameEndSkipsFirstFrame();
    TestFrameEndGlistGuard();
    TestFrameEndCountdown();
    TestFrameEndF8Countdown();
    TestFrameEndOneShot();
    TestHudText();

    if (g_nFail != 0) {
        printf("slice2_18: %d FAILURE(S)\n", g_nFail);
        return 1;
    }
    printf("slice2_18: all tests passed\n");
    return 0;
}
