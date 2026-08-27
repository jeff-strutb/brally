/* test_slice3_45.c -- behaviour tests for slice3_45.c.
 *
 * Everything asserted here is a property of the ORIGINAL that the port has to
 * keep: a mirroring invariant, an asymmetric clamp, an inverted condition, a
 * skipped array slot. Nothing here encodes a number that was chosen by the
 * port rather than read out of the disassembly.
 *
 * All cross-slice callees are stubbed BELOW, in this file only.
 */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "slice3_45.h"

/* ====================================================================== */
/* Cross-slice stand-ins (TEST ONLY)                                       */
/* ====================================================================== */

static int      g_nBuildMatrix;
static BrMat4  *g_pLastBuildDst;
static const BrRbState *g_pLastBuildSrc;

void BrRbBuildMatrix(BrMat4 *pM, const BrRbState *pS)
{
    ++g_nBuildMatrix;
    g_pLastBuildDst = pM;
    g_pLastBuildSrc = pS;
    /* Enough structure to see that it ran: translation row from pos. */
    pM->m[3][0] = pS->pos.x;
    pM->m[3][1] = pS->pos.y;
    pM->m[3][2] = pS->pos.z;
    pM->m[3][3] = 1.0f;
}

static int g_nSetLastColumn;

void BrMat4SetLastColumn(BrMat4 *pM)
{
    ++g_nSetLastColumn;
    pM->m[0][3] = 0.0f;
    pM->m[1][3] = 0.0f;
    pM->m[2][3] = 0.0f;
    pM->m[3][3] = 1.0f;
}

static int g_nNormalise;

void BrVec4Normalise(BrVec4 *pV)
{
    double n;
    ++g_nNormalise;
    n = sqrt((double)pV->f00 * pV->f00 + (double)pV->f04 * pV->f04 +
             (double)pV->f08 * pV->f08 + (double)pV->f0C * pV->f0C);
    if (n != 0.0) {
        pV->f00 = (float)(pV->f00 / n);
        pV->f04 = (float)(pV->f04 / n);
        pV->f08 = (float)(pV->f08 / n);
        pV->f0C = (float)(pV->f0C / n);
    }
}

/* 0x100765E0: matrix -> BrVec4. The stub just stamps a recognisable value. */
static int g_n765E0;
void BrSub100765E0(const BrMat4 *pSrc, BrVec4 *pDst)
{
    ++g_n765E0;
    pDst->f00 = pSrc->m[0][0];
    pDst->f04 = pSrc->m[0][1];
    pDst->f08 = pSrc->m[0][2];
    pDst->f0C = pSrc->m[1][0];
}

/* 0x10074090: modelled as the Hamilton product dst = a (x) b, scalar first.
 * The port makes NO claim about the real order -- this stub only has to be a
 * consistent associative product for the accumulation test below. */
void BrSub10074090(BrVec4 *pDst, const BrVec4 *pA, const BrVec4 *pB)
{
    float aw = pA->f00, ax = pA->f04, ay = pA->f08, az = pA->f0C;
    float bw = pB->f00, bx = pB->f04, by = pB->f08, bz = pB->f0C;
    pDst->f00 = aw * bw - ax * bx - ay * by - az * bz;
    pDst->f04 = aw * bx + ax * bw + ay * bz - az * by;
    pDst->f08 = aw * by - ax * bz + ay * bw + az * bx;
    pDst->f0C = aw * bz + ax * by - ay * bx + az * bw;
}

static BrCarGfx *g_pColourCar;
static int       g_colR, g_colG, g_colB, g_nColour;

void BrCarGfxSetColour(BrCarGfx *pCar, int r, int g, int b)
{
    ++g_nColour;
    g_pColourCar = pCar;
    g_colR = r; g_colG = g; g_colB = b;
}

static int   g_n62C50;
static BrEnt *g_p62C50;
void BrSub10062C50(BrEnt *pE) { ++g_n62C50; g_p62C50 = pE; }

static int g_nStub8B80;
void BrExt_10008B80(void) { ++g_nStub8B80; }

/* Globals owned by slice2_25 / slice2_11. */
int32_t g_brB4E1D0;
int32_t g_brB4E1E0;
int     g_brFlag6909E0;
void   *g_brP680584;
/* slice3_45.o references this pad-mode table (owned by slice2_19, whose full
 * link closure pulls Mat4/Pool/etc.); stand it in with a real 8-byte buffer so
 * the module's g_BrPadModeBytes[i] reads are valid rather than a NULL deref. */
static unsigned char s_padModeBytes_stub[8];
const unsigned char *g_BrPadModeBytes = s_padModeBytes_stub;

/* ====================================================================== */
/* Fake COM objects                                                        */
/* ====================================================================== */

static int  g_nAcquire, g_nUnacquire, g_nRelease, g_nStart, g_nStop, g_nSetParam;
static long g_hrAcquire;
static uint32_t g_lastSetParamFlags;
static const BrDiEffect *g_pLastSetParamEff;

static uint32_t g_lastProp;
static unsigned char g_lastPropBuf[64];
static long g_hrSetProperty;
static int  g_nSetProperty;

static long FakeAcquire(BrDiObj *p)   { (void)p; ++g_nAcquire;   return g_hrAcquire; }
static long FakeUnacquire(BrDiObj *p) { (void)p; ++g_nUnacquire; return 0; }
static long FakeRelease(BrDiObj *p)   { (void)p; ++g_nRelease;   return 0; }

static long FakeSetProperty(BrDiObj *p, uint32_t prop, const void *pdiph)
{
    (void)p;
    ++g_nSetProperty;
    g_lastProp = prop;
    memcpy(g_lastPropBuf, pdiph, 24);
    return g_hrSetProperty;
}

static long FakeSetParameters(BrDiObj *p, const BrDiEffect *pEff, uint32_t flags)
{
    (void)p;
    ++g_nSetParam;
    g_pLastSetParamEff = pEff;
    g_lastSetParamFlags = flags;
    return 0;
}
static long FakeStart(BrDiObj *p, uint32_t it, uint32_t fl)
{
    (void)p; (void)it; (void)fl; ++g_nStart; return 0;
}
static long FakeStop(BrDiObj *p) { (void)p; ++g_nStop; return 0; }

static long FakeCreateEffect(BrDiObj *p, const void *rguid,
                             const BrDiEffect *pEff, BrDiObj **ppEff,
                             void *pUnk);

static BrDiDevVtbl g_devVtbl;
static BrDiEffVtbl g_effVtbl;
static BrDiRootVtbl g_rootVtbl;

static BrDiObj g_dev;
static BrDiObj g_effA;
static BrDiObj g_effB;
static BrDiObj g_root;

static int g_nCreateEffect;
static long g_hrCreateEffect;

static long FakeCreateEffect(BrDiObj *p, const void *rguid,
                             const BrDiEffect *pEff, BrDiObj **ppEff,
                             void *pUnk)
{
    (void)p; (void)rguid; (void)pEff; (void)pUnk;
    ++g_nCreateEffect;
    *ppEff = (g_nCreateEffect == 1) ? &g_effA : &g_effB;
    return g_hrCreateEffect;
}

static int  g_nEnum;
static int  g_enumProduceDevice;
static long g_hrEnum;
static void *g_lastPvRef;
static uint32_t g_lastEnumFlags;

static long FakeEnumDevices(BrDiObj *p, uint32_t devType, BrDiEnumDevicesCb cb,
                            void *pvRef, uint32_t flags)
{
    (void)p; (void)devType; (void)cb;
    ++g_nEnum;
    g_lastPvRef = pvRef;
    g_lastEnumFlags = flags;
    if (g_enumProduceDevice) {
        g_brFfb.pDevice = &g_dev;
    }
    return g_hrEnum;
}

static void SetupCom(void)
{
    memset(&g_devVtbl, 0, sizeof g_devVtbl);
    memset(&g_effVtbl, 0, sizeof g_effVtbl);
    memset(&g_rootVtbl, 0, sizeof g_rootVtbl);

    g_devVtbl.pfnAcquire       = FakeAcquire;
    g_devVtbl.pfnUnacquire     = FakeUnacquire;
    g_devVtbl.pfnRelease       = FakeRelease;
    g_devVtbl.pfnSetProperty   = FakeSetProperty;
    g_devVtbl.pfnCreateEffect  = FakeCreateEffect;

    g_effVtbl.pfnRelease       = FakeRelease;
    g_effVtbl.pfnSetParameters = FakeSetParameters;
    g_effVtbl.pfnStart         = FakeStart;
    g_effVtbl.pfnStop          = FakeStop;

    g_rootVtbl.pfnRelease      = FakeRelease;
    g_rootVtbl.pfnEnumDevices  = FakeEnumDevices;

    g_dev.pVtbl  = (const BrDiVtbl *)(const void *)&g_devVtbl;
    g_effA.pVtbl = (const BrDiVtbl *)(const void *)&g_effVtbl;
    g_effB.pVtbl = (const BrDiVtbl *)(const void *)&g_effVtbl;
    g_root.pVtbl = (const BrDiVtbl *)(const void *)&g_rootVtbl;
}

/* ====================================================================== */
/* Helpers                                                                 */
/* ====================================================================== */

static int Close(float a, float b) { return fabs((double)a - b) < 1e-5; }

static int g_fail;
#define CHECK(c) do { if (!(c)) { \
        printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); g_fail = 1; } } while (0)

static BrEnt g_ent;

static void EntClear(void) { memset(&g_ent, 0, sizeof g_ent); }

/* ====================================================================== */
/* 1. Entity setters                                                       */
/* ====================================================================== */

static void TestSetPos(void)
{
    EntClear();
    g_nBuildMatrix = 0;
    BrEntSetPos(&g_ent, 1.0f, 2.0f, 3.0f);

    /* All five mirrors, including mat0's translation ROW. */
    CHECK(g_ent.mat0.m[3][0] == 1.0f && g_ent.mat0.m[3][1] == 2.0f &&
          g_ent.mat0.m[3][2] == 3.0f);
    CHECK(g_ent.f26C8[0] == 1.0f && g_ent.f26C8[2] == 3.0f);
    CHECK(g_ent.st.pos.x == 1.0f && g_ent.st.pos.z == 3.0f);
    CHECK(g_ent.stA.pos.y == 2.0f && g_ent.stB.pos.y == 2.0f);

    /* Rebuilt from st into `matrix`, not into mat0. */
    CHECK(g_nBuildMatrix == 1);
    CHECK(g_pLastBuildDst == &g_ent.matrix);
    CHECK(g_pLastBuildSrc == &g_ent.st);

    /* Nothing else moved. */
    CHECK(g_ent.st.vel.x == 0.0f && g_ent.st.quat.f00 == 0.0f);
}

static void TestSetHeading(void)
{
    const float a = 0.7f;
    float dot, len1;

    EntClear();
    g_nBuildMatrix = 0;
    BrEntSetHeading(&g_ent, a);

    CHECK(Close(g_ent.mat0.m[0][0], cosf(a)));
    CHECK(Close(g_ent.mat0.m[0][1], sinf(a)));
    CHECK(g_ent.mat0.m[0][2] == 0.0f);
    CHECK(g_ent.mat0.m[2][0] == 0.0f && g_ent.mat0.m[2][1] == 0.0f &&
          g_ent.mat0.m[2][2] == 1.0f);

    /* Row 1 is (cos(a+pi/2), sin(a+pi/2), 0) == (-sin a, cos a, 0) to within
     * the error of the extra transcendental pair. Assert the INVARIANT
     * (orthonormal, right-handed) rather than the exact bits. */
    dot  = g_ent.mat0.m[0][0] * g_ent.mat0.m[1][0] +
           g_ent.mat0.m[0][1] * g_ent.mat0.m[1][1];
    len1 = g_ent.mat0.m[1][0] * g_ent.mat0.m[1][0] +
           g_ent.mat0.m[1][1] * g_ent.mat0.m[1][1];
    CHECK(fabs((double)dot) < 1e-5);
    CHECK(fabs((double)len1 - 1.0) < 1e-5);
    CHECK(g_ent.mat0.m[1][0] < 0.0f);   /* -sin(0.7) is negative */
    CHECK(g_ent.mat0.m[1][2] == 0.0f);

    /* Half-angle quaternion about Z, scalar first, unit. */
    CHECK(Close(g_ent.st.quat.f00, cosf(a * 0.5f)));
    CHECK(g_ent.st.quat.f04 == 0.0f && g_ent.st.quat.f08 == 0.0f);
    CHECK(Close(g_ent.st.quat.f0C, sinf(a * 0.5f)));

    /* Both mirrors got the WHOLE quaternion, z included. */
    CHECK(g_ent.stA.quat.f00 == g_ent.st.quat.f00);
    CHECK(g_ent.stA.quat.f0C == g_ent.st.quat.f0C);
    CHECK(g_ent.stB.quat.f00 == g_ent.st.quat.f00);
    CHECK(g_ent.stB.quat.f0C == g_ent.st.quat.f0C);

    CHECK(g_nBuildMatrix == 1);

    /* mat0's fourth column is NOT touched -- BrEntReset is what fixes it. */
    CHECK(g_ent.mat0.m[3][3] == 0.0f);
}

static void TestSetMatrix(void)
{
    BrMat4 src;
    int i, j;

    EntClear();
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            src.m[i][j] = (float)(i * 4 + j);
        }
    }
    g_nBuildMatrix = 0;
    g_n765E0 = 0;
    g_ent.st.pos.x = 9.0f;      /* stays put: the setter does not touch pos */

    BrEntSetMatrix(&g_ent, &src);

    CHECK(memcmp(&g_ent.mat0, &src, sizeof src) == 0);
    CHECK(g_n765E0 == 1);
    CHECK(g_nBuildMatrix == 1);
    CHECK(g_ent.st.pos.x == 9.0f);
    /* ...so the rebuilt matrix carries the OLD translation. */
    CHECK(g_ent.matrix.m[3][0] == 9.0f);
    CHECK(g_ent.stA.quat.f00 == g_ent.st.quat.f00);
    CHECK(g_ent.stB.quat.f0C == g_ent.st.quat.f0C);
}

static void TestSetVelAngVel(void)
{
    EntClear();
    BrEntSetVel(&g_ent, 4.0f, 5.0f, 6.0f);
    CHECK(g_ent.st.vel.x == 4.0f && g_ent.st.vel.z == 6.0f);
    CHECK(g_ent.stA.vel.y == 5.0f && g_ent.stB.vel.y == 5.0f);
    CHECK(g_ent.f1024[0] == 4.0f && g_ent.f1024[2] == 6.0f);
    /* No matrix rebuild for velocity. */
    g_nBuildMatrix = 0;
    BrEntSetVel(&g_ent, 0.0f, 0.0f, 0.0f);
    CHECK(g_nBuildMatrix == 0);

    BrEntSetAngVel(&g_ent, 7.0f, 8.0f, 9.0f);
    CHECK(g_ent.st.angVel.x == 7.0f && g_ent.st.angVel.z == 9.0f);
    CHECK(g_ent.stA.angVel.y == 8.0f && g_ent.stB.angVel.y == 8.0f);
}

static void TestSetOrientation(void)
{
    const float q = 1.5707963f;   /* pi/2 about Z, twice, should give pi */

    EntClear();
    g_ent.st.quat.f00 = 1.0f;     /* identity */
    g_nBuildMatrix = 0;
    g_nNormalise = 0;

    BrEntSetOrientation(&g_ent, q, 0.0f, 0.0f);
    BrEntSetOrientation(&g_ent, q, 0.0f, 0.0f);

    /* Accumulation, not assignment: two quarter turns make a half turn. */
    CHECK(fabs((double)g_ent.st.quat.f00) < 1e-4);
    CHECK(fabs((double)g_ent.st.quat.f0C) > 0.999);
    CHECK(fabs((double)g_ent.st.quat.f04) < 1e-4);
    CHECK(fabs((double)g_ent.st.quat.f08) < 1e-4);

    CHECK(g_nNormalise == 2);
    /* The one setter that does NOT rebuild the matrix. */
    CHECK(g_nBuildMatrix == 0);
    CHECK(g_ent.stA.quat.f0C == g_ent.st.quat.f0C);
    CHECK(g_ent.stB.quat.f0C == g_ent.st.quat.f0C);
}

static void TestColourAndRecord(void)
{
    EntClear();
    g_ent.r = 0xFF; g_ent.g = 0x07; g_ent.b = 0x80;
    g_nColour = 0; g_n62C50 = 0;
    BrEntRefreshColour(&g_ent);

    /* 8 -> 5 bits by TRUNCATION, and the argument order is (r, g, b). */
    CHECK(g_colR == 31 && g_colG == 0 && g_colB == 16);
    CHECK(g_nColour == 1 && g_n62C50 == 1 && g_p62C50 == &g_ent);

    /* Index 0 is the table base, and the stride is BR_HUDSPRITE_STRIDE. */
    BrEntSetRecord(&g_ent, 0);
    CHECK((const unsigned char *)(const void *)g_ent.pRec == g_aBrC12A0);
    CHECK(g_pColourCar == g_ent.pRec);

    /* 89992 == slice2_15.h's BR_HUDSPRITE_STRIDE for the same table. */
    BrEntSetRecord(&g_ent, 3);
    CHECK((size_t)((const unsigned char *)(const void *)g_ent.pRec -
                   g_aBrC12A0) == (size_t)3 * 89992u);
}

static void TestReset(void)
{
    static unsigned char rec[0x200];
    int i;

    EntClear();
    memset(rec, 0, sizeof rec);
    for (i = 0; i < 12; ++i) {
        rec[0x98 + i * 4] = (unsigned char)(i + 1);
    }
    for (i = 0; i < 4; ++i) {
        rec[0xC8 + i * 4 + 1] = (unsigned char)(i + 1);
    }
    rec[0x96] = 0xFF;   /* -1 after movsx */
    rec[0x97] = 0x80;   /* -128 after movsx */
    rec[0xD8] = 0x7F;   /* +127 */

    g_ent.pRec = (BrCarGfx *)(void *)rec;
    g_ent.fE9C = 0xABCDEF01u;

    /* Sentinels that must survive in the SKIPPED frame slot. */
    g_ent.aFrames[4].f40 = 12345.0f;
    g_ent.aFrames[4].m.m[3][3] = 999.0f;
    /* And one that must NOT survive in a live slot. */
    g_ent.aFrames[3].f40 = 12345.0f;

    g_nSetLastColumn = 0;
    BrEntReset(&g_ent);

    /* mat0 + frames 0,1,2,3,5 + mat40/80/C0/100 == 10 calls, NOT 11. */
    CHECK(g_nSetLastColumn == 10);

    /* aFrames[4] is skipped by the original. */
    CHECK(g_ent.aFrames[4].f40 == 12345.0f);
    CHECK(g_ent.aFrames[4].m.m[3][3] == 999.0f);
    for (i = 0; i < 6; ++i) {
        if (i == 4) continue;
        CHECK(Close(g_ent.aFrames[i].f40, 0.5235987901687622f));
        CHECK(g_ent.aFrames[i].m.m[3][3] == 1.0f);
    }

    CHECK(g_ent.p2734 == &g_ent.aFrames[0]);
    CHECK(g_ent.st.vel.x == 0.0f && g_ent.f1024[2] == 0.0f);
    CHECK(g_ent.fF8C == 0 && g_ent.fF90 == 0 && g_ent.f2738 == 0);

    /* Twelve CONTIGUOUS dwords out of the record, +0x98..+0xC4. */
    for (i = 0; i < 12; ++i) {
        CHECK(g_ent.fE28[i] == (uint32_t)(i + 1));
    }
    for (i = 0; i < 4; ++i) {
        CHECK(g_ent.f340[i] == (uint32_t)((i + 1) << 8));
    }

    /* Sign extension, all three. */
    CHECK(g_ent.fE58 == 127);
    CHECK(g_ent.fE5C == -1);
    CHECK(g_ent.fE64 == -128);

    /* The self-copy. */
    CHECK(g_ent.fE60 == 0xABCDEF01u);

    /* And the record pointer is dropped -- a second call would fault. */
    CHECK(g_ent.pRec == NULL);
}

/* ====================================================================== */
/* 2. 0x10077090                                                           */
/* ====================================================================== */

static void TestSet680598(void)
{
    /* The stub runs unless (lo != 0 && hi == 0). */
    g_nStub8B80 = 0;
    BrSet680598(0x00000000u);          /* lo 0            -> stub  */
    CHECK(g_nStub8B80 == 1);
    CHECK(g_br680598 == 0 && g_br68059C == 0 && g_br6805A0 == 0);

    BrSet680598(0x00010000u);          /* lo 0, hi set    -> stub  */
    CHECK(g_nStub8B80 == 2);
    CHECK(g_br68059C == 0 && g_br6805A0 == 1);

    BrSet680598(0x00020001u);          /* both set        -> stub  */
    CHECK(g_nStub8B80 == 3);
    CHECK(g_br68059C == 1 && g_br6805A0 == 2);

    BrSet680598(0x00000280u);          /* lo set, hi 0    -> NO stub */
    CHECK(g_nStub8B80 == 3);
    CHECK(g_br680598 == 0x280u && g_br68059C == 0x280u && g_br6805A0 == 0);
}

/* ====================================================================== */
/* 3. Input bindings                                                       */
/* ====================================================================== */

static BrInputBinding g_binds[28];

static void InputClear(void)
{
    memset(&g_brInput, 0, sizeof g_brInput);
    memset(g_binds, 0, sizeof g_binds);
    /* An unbound entry is code 0 / kind 0, which IS a live binding to scan
     * code 0. Park the alternates on a key that is never pressed. */
    g_brInput.pBindings = g_binds;
    g_brInput.iKeyCur = 1;
    g_brInput.iKeyPrev = 0;
    g_brInput.iJoyCur = 1;
    g_brInput.iJoyPrev = 0;
    g_brInput.iMouseCur = 1;
    g_brInput.iMousePrev = 0;
}

static void TestIsDown(void)
{
    InputClear();

    /* Keyboard, primary. The result is 0x80, never 1. */
    g_binds[2].kind0 = BR_BIND_KEY;
    g_binds[2].code0 = 0x11;
    g_binds[2].code1 = 0xFE; g_binds[2].kind1 = 0xFF;  /* alternate disabled */
    g_binds[2].code2 = 0xFD; g_binds[2].kind2 = 0xFF;
    CHECK(BrInputIsDown(2) == 0);
    g_brInput.aKeys[1][0x11] = 0x80;
    CHECK(BrInputIsDown(2) == 0x80);
    /* The PREVIOUS buffer is irrelevant to "is down". */
    g_brInput.aKeys[1][0x11] = 0;
    g_brInput.aKeys[0][0x11] = 0x80;
    CHECK(BrInputIsDown(2) == 0);

    /* An alternate whose kind byte is 0 is consulted and ORed in. */
    g_binds[2].kind1 = 0;
    g_binds[2].code1 = 0x22;
    g_brInput.aKeys[1][0x22] = 0x80;
    CHECK(BrInputIsDown(2) == 0x80);
    g_binds[2].kind1 = 0xFF;
    CHECK(BrInputIsDown(2) == 0);

    /* Axis dead band: exactly +-50 is NOT pressed on either side. */
    g_binds[3].kind0 = BR_BIND_JOYXPOS;
    g_binds[3].kind1 = 0xFF; g_binds[3].kind2 = 0xFF;
    g_binds[4].kind0 = BR_BIND_JOYXNEG;
    g_binds[4].kind1 = 0xFF; g_binds[4].kind2 = 0xFF;

    g_brInput.aJoy[1].lX = 50;
    CHECK(BrInputIsDown(3) == 0);
    g_brInput.aJoy[1].lX = 51;
    CHECK(BrInputIsDown(3) == 0x80);
    g_brInput.aJoy[1].lX = -50;
    CHECK(BrInputIsDown(4) == 0);
    g_brInput.aJoy[1].lX = -51;
    CHECK(BrInputIsDown(4) == 0x80);

    /* An unrecognised kind reads as "not pressed" rather than falling into
     * some neighbouring case. */
    g_binds[5].kind0 = 0x02;
    g_binds[5].kind1 = 0xFF; g_binds[5].kind2 = 0xFF;
    CHECK(BrInputIsDown(5) == 0);
    g_binds[5].kind0 = 0x8C;
    CHECK(BrInputIsDown(5) == 0);
}

static void TestJustPressed(void)
{
    InputClear();

    /* Button/key edges return 1. */
    g_binds[6].kind0 = BR_BIND_KEY;
    g_binds[6].code0 = 0x30;
    g_binds[6].kind1 = 0xFF; g_binds[6].kind2 = 0xFF;

    CHECK(BrInputJustPressed(6) == 0);
    g_brInput.aKeys[1][0x30] = 0x80;             /* down this frame only */
    CHECK(BrInputJustPressed(6) == 1);
    g_brInput.aKeys[0][0x30] = 0x80;             /* held */
    CHECK(BrInputJustPressed(6) == 0);
    g_brInput.aKeys[1][0x30] = 0;                /* released */
    CHECK(BrInputJustPressed(6) == 0);

    /* Axis edges return 0x80, not 1. This asymmetry is in the original. */
    g_binds[7].kind0 = BR_BIND_MOUYPOS;
    g_binds[7].kind1 = 0xFF; g_binds[7].kind2 = 0xFF;

    g_brInput.aMouse[0].y = 0;
    g_brInput.aMouse[1].y = 100;
    CHECK(BrInputJustPressed(7) == 0x80);
    g_brInput.aMouse[0].y = 100;                 /* already over last frame */
    CHECK(BrInputJustPressed(7) == 0);
    /* The previous-frame test uses <= 50, so 50 still counts as "was off". */
    g_brInput.aMouse[0].y = 50;
    CHECK(BrInputJustPressed(7) == 0x80);
    g_brInput.aMouse[0].y = 51;
    CHECK(BrInputJustPressed(7) == 0);

    /* Joystick buttons use the joystick index pair, not the keyboard one. */
    g_binds[8].kind0 = BR_BIND_JOYBTN;
    g_binds[8].code0 = 9;
    g_binds[8].kind1 = 0xFF; g_binds[8].kind2 = 0xFF;
    g_brInput.aJoy[1].rgbButtons[9] = 0x80;
    CHECK(BrInputJustPressed(8) == 1);
    g_brInput.aJoy[0].rgbButtons[9] = 0x80;
    CHECK(BrInputJustPressed(8) == 0);
}

/* ====================================================================== */
/* 4. DirectInput                                                          */
/* ====================================================================== */

static void TestAcquire(void)
{
    SetupCom();
    g_nAcquire = 0;

    g_brFfb.pDevice = NULL;
    CHECK(BrDiAcquire() == 0);          /* no device -> 0, and no call */
    CHECK(g_nAcquire == 0);

    g_brFfb.pDevice = &g_dev;
    g_hrAcquire = 0;
    CHECK(BrDiAcquire() == 1);
    g_hrAcquire = 1;                    /* S_FALSE is still >= 0 */
    CHECK(BrDiAcquire() == 1);
    g_hrAcquire = -1;
    CHECK(BrDiAcquire() == 0);
    CHECK(g_nAcquire == 3);
}

static void TestKeyboardShutdown(void)
{
    SetupCom();
    g_nUnacquire = 0; g_nRelease = 0;

    /* Nested: only the transition to 0 tears down. */
    g_pBr18ABDD0 = &g_dev;
    g_br18ABDD8 = 2;
    BrDiKeyboardShutdown();
    CHECK(g_br18ABDD8 == 1 && g_pBr18ABDD0 == &g_dev && g_nRelease == 0);
    BrDiKeyboardShutdown();
    CHECK(g_br18ABDD8 == 0 && g_pBr18ABDD0 == NULL);
    CHECK(g_nUnacquire == 1 && g_nRelease == 1);

    /* THE UNDERFLOW CLAMP DOES NOT TEAR DOWN. Starting from 0, the count is
     * pinned back to 0 and the function RETURNS. */
    g_pBr18ABDD0 = &g_dev;
    g_br18ABDD8 = 0;
    BrDiKeyboardShutdown();
    CHECK(g_br18ABDD8 == 0);
    CHECK(g_pBr18ABDD0 == &g_dev);      /* untouched */
    CHECK(g_nRelease == 1);
}

static void TestSetProp(void)
{
    uint32_t w[6];

    SetupCom();
    g_hrSetProperty = 0;
    g_nSetProperty = 0;

    CHECK(BrDiSetPropRange(&g_dev, 4u, 4u, 1u, -0x80, 0x80) == 0);
    memcpy(w, g_lastPropBuf, sizeof w);
    CHECK(g_lastProp == 4u);
    CHECK(w[0] == 0x18u && w[1] == 0x10u);      /* DIPROPRANGE sizes */
    CHECK(w[2] == 4u && w[3] == 1u);            /* dwObj, dwHow */
    CHECK((int32_t)w[4] == -0x80 && (int32_t)w[5] == 0x80);

    CHECK(BrDiSetPropDword(&g_dev, 5u, 4u, 1u, 7u) == 0);
    memcpy(w, g_lastPropBuf, sizeof w);
    CHECK(g_lastProp == 5u);
    CHECK(w[0] == 0x14u && w[1] == 0x10u);      /* DIPROPDWORD sizes */
    CHECK(w[2] == 4u && w[3] == 1u && w[4] == 7u);

    g_hrSetProperty = -1;
    CHECK(BrDiSetPropRange(&g_dev, 4u, 0u, 1u, 0, 1) == -1);
    CHECK(g_nSetProperty == 3);
}

/* ====================================================================== */
/* 5. Force feedback                                                       */
/* ====================================================================== */

static void FfbEnable(int on)
{
    g_brB4E1D0 = on ? 1 : 0;
    g_brB4E1E0 = on ? 1 : 0;
    g_br18ABDBC = on ? 1 : 0;
    g_brFlag6909E0 = 0;
}

static void TestFfbGuard(void)
{
    SetupCom();
    FfbEnable(1);

    g_br0BD430[0] = 0;
    BrFfbSetDirection(7);
    CHECK(g_br0BD430[0] == 7);

    /* Every leg of the guard blocks it independently. */
    g_brFlag6909E0 = 1;
    BrFfbSetDirection(8);
    CHECK(g_br0BD430[0] == 7);
    g_brFlag6909E0 = 0;
    g_br18ABDBC = 0;
    BrFfbSetDirection(8);
    CHECK(g_br0BD430[0] == 7);
    g_br18ABDBC = 1;
    g_brB4E1E0 = 0;
    BrFfbSetDirection(8);
    CHECK(g_br0BD430[0] == 7);
    g_brB4E1E0 = 1;

    /* 3 is "enabled" for BrFfbInit but NOT for this guard. */
    g_brB4E1D0 = 3;
    BrFfbSetDirection(8);
    CHECK(g_br0BD430[0] == 7);
    g_brB4E1D0 = 2;
    BrFfbSetDirection(8);
    CHECK(g_br0BD430[0] == 8);

    FfbEnable(1);
    BrFfbSetDurationLong();
    CHECK(g_br0BD438 == 250000);
    BrFfbSetDurationShort();
    CHECK(g_br0BD438 == 125000);

    /* Commit copies the pending duration in and restarts the effect. */
    g_nSetParam = 0;
    g_brFfb.pEffectSquare = &g_effB;
    BrFfbCommitDuration();
    CHECK(g_brDiEffSquare.dwDuration == 125000u);
    CHECK(g_nSetParam == 1);
    CHECK(g_lastSetParamFlags == 0x20000041u);
    CHECK(g_pLastSetParamEff == &g_brDiEffSquare);

    /* No effect object -> the duration is still copied, but nothing is sent. */
    g_brFfb.pEffectSquare = NULL;
    g_br0BD438 = 999;
    BrFfbCommitDuration();
    CHECK(g_brDiEffSquare.dwDuration == 999u);
    CHECK(g_nSetParam == 1);
    g_br0BD438 = 125000;
}

static void TestFfbSpringCoeff(void)
{
    SetupCom();
    memset(g_brDiSpringCond, 0, sizeof g_brDiSpringCond);
    g_brDiSpringCond[1].lPositiveCoefficient = 4444;
    g_nSetParam = 0;

    /* No guard at all on this one -- it runs with force feedback "off". */
    FfbEnable(0);
    g_brFfb.pEffectSpring = &g_effA;
    BrFfbSetSpringCoeff(1234);

    CHECK(g_brDiSpringCond[0].lPositiveCoefficient == 1234);
    CHECK(g_brDiSpringCond[0].lNegativeCoefficient == 1234);
    /* The SECOND axis is left behind. */
    CHECK(g_brDiSpringCond[1].lPositiveCoefficient == 4444);
    CHECK(g_nSetParam == 1);
    CHECK(g_lastSetParamFlags == 0x100u);
    CHECK(g_pLastSetParamEff == &g_brDiEffSpring);
}

static void TestFfbUpdateSpring(void)
{
    SetupCom();
    FfbEnable(1);
    g_brFfb.pEffectSpring = &g_effA;
    g_br0BD424 = 10000;
    g_br0BD428 = 2000;
    g_nStart = 0; g_nStop = 0; g_nSetParam = 0;

    /* enable == 0 stops the effect exactly once, however often it is called. */
    g_br18ABDF8 = 0;
    BrFfbUpdateSpring(0, 0, 0);
    CHECK(g_nStop == 1 && g_br18ABDF8 == 1);
    BrFfbUpdateSpring(0, 0, 0);
    CHECK(g_nStop == 1);

    /* The scaled value is ((decay + 8) * g_br0BD424 * 1000) / 10000. */
    CHECK(g_br0BD42C == ((0 + 8) * 10000 * 1000) / 10000);

    /* Re-enabling restarts, and sets the LONG duration (250000). */
    g_br0BD438 = 125000;
    g_br18ABD78 = 5000;
    BrFfbUpdateSpring(0, 1, 0);
    CHECK(g_nStart == 1 && g_br18ABDF8 == 0);
    CHECK(g_br0BD438 == 250000);

    /* up == 0 INCREMENTS by g_br0BD424 / 10 == 1000. */
    CHECK(g_br18ABD78 == 6000);
    CHECK(g_nSetParam == 1);            /* the change was pushed down */

    /* up != 0 DECREMENTS -- the sense of the flag is inverted. */
    BrFfbUpdateSpring(1, 1, 0);
    CHECK(g_br18ABD78 == 5000);

    /* Lower clamp on the decrementing branch: g_br0BD428 * g_br0BD424 / 10000
     * == 2000. Six more steps of 1000 would reach -1000; it stops at 2000. */
    {
        int i;
        for (i = 0; i < 10; ++i) {
            BrFfbUpdateSpring(1, 1, 0);
        }
        CHECK(g_br18ABD78 == 2000);
    }
    /* ...and once it is pinned, nothing more is pushed down. */
    g_nSetParam = 0;
    BrFfbUpdateSpring(1, 1, 0);
    CHECK(g_nSetParam == 0);

    /* Upper clamp on the incrementing branch, from g_br0BD42C. decay 0 gives
     * scaled 8000, so the bound is 8000 * 10000 / 10000 == 8000. */
    {
        int i;
        for (i = 0; i < 20; ++i) {
            BrFfbUpdateSpring(0, 1, 0);
        }
        CHECK(g_br18ABD78 == 8000);
    }

    /* The guard applies here too. */
    FfbEnable(0);
    g_br18ABD78 = 1234;
    BrFfbUpdateSpring(0, 1, 0);
    CHECK(g_br18ABD78 == 1234);
}

static void TestFfbInit(void)
{
    SetupCom();
    memset(&g_brFfb, 0, sizeof g_brFfb);
    g_pBr18ABD70 = &g_root;
    g_hrEnum = 0;
    g_hrSetProperty = 0;
    g_hrCreateEffect = 0;
    g_hrAcquire = 0;
    g_nEnum = 0;
    g_nCreateEffect = 0;
    g_enumProduceDevice = 1;

    /* Disabled -> 0, and the counter is NOT touched. */
    g_brB4E1D0 = 0;
    CHECK(BrFfbInit() == 0);
    CHECK(g_brFfb.initCount == 0);
    CHECK(g_nEnum == 0);

    /* Exclusive path: pvRef 5, flags 0x101. */
    g_brB4E1D0 = 2;
    g_brB4E1E0 = 1;
    CHECK(BrFfbInit() == 2);
    CHECK(g_brFfb.initCount == 1);
    CHECK(g_nEnum == 1);
    CHECK((uintptr_t)g_lastPvRef == 5u && g_lastEnumFlags == 0x101u);
    CHECK(g_br18ABDBC == 1);
    CHECK(g_brFfb.pEffectSpring == &g_effA);
    CHECK(g_brFfb.pEffectSquare == &g_effB);
    /* Both DIEFFECTs share ONE axis array (a stack local in the original). */
    CHECK(g_brDiEffSpring.rgdwAxes == g_brDiEffSquare.rgdwAxes);
    CHECK(g_brDiEffSpring.rgdwAxes[0] == 0u && g_brDiEffSpring.rgdwAxes[1] == 4u);
    CHECK(g_brDiEffSpring.dwSize == 0x34u && g_brDiEffSpring.dwFlags == 0x12u);
    CHECK(g_brDiEffSpring.dwDuration == 0xFFFFFFFFu);
    CHECK(g_brDiEffSpring.cbTypeSpecificParams == 0x30u);
    CHECK(g_brDiEffSquare.cbTypeSpecificParams == 0x10u);
    CHECK(g_brDiSquarePeriod.dwPeriod == 250000u);

    /* Re-entry returns g_brB4E1D0, NOT 0, and enumerates nothing. */
    CHECK(BrFfbInit() == 2);
    CHECK(g_brFfb.initCount == 2);
    CHECK(g_nEnum == 1);

    /* Non-exclusive fallback: EnumDevices produced no device the first time,
     * so the second, DIFFERENT enumeration runs (pvRef 6, flags 1). */
    memset(&g_brFfb, 0, sizeof g_brFfb);
    g_nEnum = 0;
    g_enumProduceDevice = 0;
    CHECK(BrFfbInit() == 0);            /* still no device -> 0 */
    CHECK(g_nEnum == 2);
    CHECK((uintptr_t)g_lastPvRef == 6u && g_lastEnumFlags == 1u);
    CHECK(g_br18ABDBC == 0);

    /* A SetProperty failure releases the device but LEAVES THE COUNT RAISED
     * -- exactly the "raised count, NULL device" state slice1_10.h names. */
    memset(&g_brFfb, 0, sizeof g_brFfb);
    g_enumProduceDevice = 1;
    g_hrSetProperty = -1;
    g_nRelease = 0; g_nUnacquire = 0;
    CHECK(BrFfbInit() == 0);
    CHECK(g_brFfb.pDevice == NULL);
    CHECK(g_brFfb.initCount == 1);
    CHECK(g_nUnacquire == 1 && g_nRelease == 1);
    g_hrSetProperty = 0;
}

/* ====================================================================== */

int main(void)
{
    TestSetPos();
    TestSetHeading();
    TestSetMatrix();
    TestSetVelAngVel();
    TestSetOrientation();
    TestColourAndRecord();
    TestReset();
    TestSet680598();
    TestIsDown();
    TestJustPressed();
    TestAcquire();
    TestKeyboardShutdown();
    TestSetProp();
    TestFfbGuard();
    TestFfbSpringCoeff();
    TestFfbUpdateSpring();
    TestFfbInit();

    if (g_fail) {
        printf("slice3_45: FAILURES\n");
        return 1;
    }
    printf("slice3_45: all tests passed\n");
    return 0;
}
