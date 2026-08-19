/* test_slice4_53.c -- behaviour tests for packet 53.
 *
 * The stand-ins below are TEST-ONLY replacements for functions and globals
 * that other slices own.  They record what they were called with; the tests
 * assert the forwarding contract (which target, which arguments, in which
 * order), not the targets' own behaviour.
 *
 * BrGbiStackOverflow (0x1007CC00) is deliberately NOT exercised: it is the
 * CRT's exit(), so calling it would end the test run.  That is the point of
 * the gotcha, not an omission.
 */
#include <math.h>
#include "br_tmpfile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "slice4_53.h"
#include "slice2_17.h"
#include "slice2_20.h"
#include "slice2_21.h"
#include "slice2_22.h"
#include "slice2_24.h"

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
 * Stand-ins for other slices
 * ====================================================================== */

static int g_seq = 0;                   /* global call-order counter */

static int g_nBankFlip, g_nCounters, g_nAutoSave;
static void *g_pPoolEmit;      static int g_nPoolEmit;
static void *g_pRcaDest;       static size_t g_cbRca; static int g_iRcaCar, g_nRca;
static const BrDPlayLink *g_pTag4Link; static int32_t g_gateTag4;
static uint32_t g_valTag4;     static int g_nTag4;
static BrPhaseCtx *g_pActCtx;  static int g_nAct;
static BrPhaseCtx *g_pLeaveCtx; static void *g_pLeaveEnt; static int g_nLeave;
static int g_nC020, g_seqC020, g_seqTimer;

void BrS17BankFlip(void)          { ++g_nBankFlip; }
void BrRenderCountersReset(void)  { ++g_nCounters; }
void BrMenuAutoSaveName(void)     { ++g_nAutoSave; }

void BrPoolEmit(void *pvThis)     { g_pPoolEmit = pvThis; ++g_nPoolEmit; }

void BrRcaLoadCar(void *pvDest, size_t cbDest, int iCar)
{
    g_pRcaDest = pvDest;
    g_cbRca    = cbDest;
    g_iRcaCar  = iCar;
    ++g_nRca;
}

int BrDPlaySendTag4(const BrDPlayLink *pLink, int32_t fGate, uint32_t value)
{
    g_pTag4Link = pLink;
    g_gateTag4  = fGate;
    g_valTag4   = value;
    ++g_nTag4;
    return 1;
}

int BrPhaseActivate_10044B90(BrPhaseCtx *pCtx)
{
    g_pActCtx = pCtx;
    ++g_nAct;
    return 1;
}

int BrPhaseLeave_10044A30(BrPhaseCtx *pCtx, void *pEntity)
{
    g_pLeaveCtx = pCtx;
    g_pLeaveEnt = pEntity;
    ++g_nLeave;
    return 0;
}

void BrSub1003C020(void) { ++g_nC020; g_seqC020 = ++g_seq; }

/* Globals slice2_25 owns. */
int32_t  g_brAA288C;
int32_t  g_brA9CFFC;
void    *g_brP680584;

/* ======================================================================
 * 1. x87 one-liners
 * ====================================================================== */

static void TestMath(void)
{
    static const float aV[] = { 0.0f, 1.0f, 0.25f, 2.0f, 1e6f, 1e-8f };
    size_t i;

    for (i = 0; i < sizeof aV / sizeof aV[0]; ++i) {
        float r = BrSqrtF(aV[i]);
        /* r*r == x to within relative rounding: a property of sqrt, not of
         * any particular implementation. */
        CHECK(fabsf(r * r - aV[i]) <= 1e-5f * (aV[i] + 1.0f),
              "BrSqrtF: r*r != x");
        CHECK(r >= 0.0f, "BrSqrtF: negative root");
    }
    CHECK(BrSqrtF(0.0f) == 0.0f, "BrSqrtF(0) != 0");
    CHECK(BrSqrtF(1.0f) == 1.0f, "BrSqrtF(1) != 1");
    CHECK(BrSqrtF(-1.0f) != BrSqrtF(-1.0f), "BrSqrtF(-1) is not NaN");

    CHECK(BrSinF(0.0f) == 0.0f, "BrSinF(0) != 0");
    for (i = 0; i < sizeof aV / sizeof aV[0]; ++i) {
        /* sin is odd -- holds for fsin and for sinf alike. */
        CHECK(fabsf(BrSinF(-aV[i]) + BrSinF(aV[i])) <= 1e-6f,
              "BrSinF: not odd");
        CHECK(fabsf(BrSinF(aV[i])) <= 1.0f, "BrSinF: out of [-1,1]");
        /* 0x10002240 has ONE body; the two names must agree exactly. */
        CHECK(BrSub10002240(aV[i]) == BrSinF(aV[i]),
              "BrSub10002240 differs from BrSinF");
    }
}

/* ======================================================================
 * 2. String table
 * ====================================================================== */

static void TestStringById(void)
{
    static char sz1[] = "one";
    static char szTop[] = "top";
    int i;

    for (i = 0; i < BR_STRING_ID_LIMIT; ++i)
        g_apBrStringTable[i] = NULL;

    g_apBrStringTable[1]                    = sz1;
    g_apBrStringTable[BR_STRING_ID_LIMIT-1] = szTop;

    CHECK(BrStringById(1) == sz1, "id 1 should be in range");
    CHECK(BrStringById(BR_STRING_ID_LIMIT - 1) == szTop,
          "id 0x12E should be in range");

    CHECK(BrStringById(0) == NULL, "id 0 must be rejected");
    CHECK(BrStringById(BR_STRING_ID_LIMIT) == NULL,
          "id 0x12F must be rejected");

    /* The original's bounds tests are UNSIGNED, so negatives fail the upper
     * one.  Either way the answer is NULL and no read happens. */
    CHECK(BrStringById(-1) == NULL, "negative id must be rejected");
    CHECK(BrStringById(-1000000) == NULL, "negative id must be rejected");
}

/* ======================================================================
 * 3. Two-slot vtable relay
 * ====================================================================== */

typedef struct TestObj {
    const BrModelMgrVtbl *pVtbl;
    int                   n;
} TestObj;

static void *g_p0CThis, *g_p0CA, *g_p0CB, *g_p1CThis, *g_p1CArg;
static int   g_seq0C, g_seq1C;

static void *Test0C(void *pThis, void *a, void *b)
{
    g_p0CThis = pThis; g_p0CA = a; g_p0CB = b; g_seq0C = ++g_seq;
    return (void *)(uintptr_t)0xABCD;
}

static void *Test1C(void *pThis, void *p)
{
    g_p1CThis = pThis; g_p1CArg = p; g_seq1C = ++g_seq;
    return (void *)(uintptr_t)0x1234;
}

static void TestRelay(void)
{
    BrModelMgrVtbl vtbl;
    TestObj        obj;
    int            a = 0, b = 0;
    void          *r;

    memset(&vtbl, 0, sizeof vtbl);
    vtbl.pfn0C = Test0C;
    vtbl.pfn1C = Test1C;
    obj.pVtbl  = &vtbl;
    obj.n      = 0;

    g_seq = 0;
    r = BrSub100088B0(&obj, &a, &b);

    CHECK(g_p0CThis == &obj && g_p1CThis == &obj, "this not forwarded");
    /* Argument order: `a` is pushed last in the original and so arrives
     * first.  This is the whole content of the function. */
    CHECK(g_p0CA == &a && g_p0CB == &b, "pfn0C argument order");
    CHECK(g_seq0C < g_seq1C, "pfn0C must run before pfn1C");
    CHECK(g_p1CArg == (void *)(uintptr_t)0xABCD,
          "pfn1C must receive pfn0C's result");
    CHECK(r == (void *)(uintptr_t)0x1234, "return must be pfn1C's result");
}

/* ======================================================================
 * 4. Config writer
 * ====================================================================== */

#define BR_CFG_OBJ_SIZE 0x900

static void TestCfgSave(void)
{
    const char *szPath = BrTmpPath(0, "slice4_53_cfg");
    unsigned char *pSrc;
    unsigned char *pFile;
    long           cbFile;
    size_t         cbRead;
    FILE          *pf;
    int            i;
    size_t         cbTail;

    pSrc = (unsigned char *)malloc(BR_CFG_OBJ_SIZE);
    CHECK(pSrc != NULL, "malloc");
    if (pSrc == NULL)
        return;
    for (i = 0; i < BR_CFG_OBJ_SIZE; ++i)
        pSrc[i] = (unsigned char)((i * 31 + 7) & 0xFF);

    CHECK(BrCfgSave1006A4A0(pSrc, szPath) == 1, "save should succeed");

    pf = fopen(szPath, "rb");
    CHECK(pf != NULL, "reopen");
    if (pf == NULL) { free(pSrc); return; }

    fseek(pf, 0, SEEK_END);
    cbFile = ftell(pf);
    fseek(pf, 0, SEEK_SET);
    pFile = (unsigned char *)malloc((size_t)cbFile);
    CHECK(pFile != NULL, "malloc file");
    if (pFile == NULL) { fclose(pf); free(pSrc); return; }
    cbRead = fread(pFile, 1, (size_t)cbFile, pf);
    fclose(pf);
    CHECK(cbRead == (size_t)cbFile, "short read");

    /* The header: four magic bytes with no terminator, then the version
     * dword emitted byte-wise from .rdata. */
    CHECK(memcmp(pFile, "RCfg", 4) == 0, "magic");
    CHECK(pFile[4] == 0x02 && pFile[5] == 0 && pFile[6] == 0 && pFile[7] == 0,
          "version dword");

    /* Structural property, independent of the offset table: the file ENDS
     * with four 0xA8 blocks that are contiguous in the source, so the last
     * 4*0xA8 bytes reproduce [0, 0x2A0) exactly -- and the dword immediately
     * before them is the source's +0x2A0.  This is the ordering oddity worth
     * pinning: the low part of the object is written last. */
    cbTail = 4u * 0xA8u;
    CHECK((size_t)cbFile > cbTail + 4, "file too short for the tail");
    CHECK(memcmp(pFile + (size_t)cbFile - cbTail, pSrc, cbTail) == 0,
          "tail must reproduce [0, 0x2A0)");
    CHECK(memcmp(pFile + (size_t)cbFile - cbTail - 4, pSrc + 0x2A0, 4) == 0,
          "+0x2A0 must precede the tail");

    /* And the body as a whole is the documented concatenation. */
    {
        static const uint32_t aOff[] = {
            0x2A8,0x2AC,0x2B0,0x2B4,0x3B8,0x7B8,0x7BC,0x7C0,0x7C4,0x7C8,
            0x7D8,0x7DC,0x7E0,0x7E4,0x7E8,0x7EC,0x7F0,0x7F4,0x7F8,0x7FC,
            0x800,0x804,0x808,0x80C,0x810,0x830,0x870,0x2A0,
            0x000,0x0A8,0x150,0x1F8
        };
        static const uint32_t aLen[] = {
            4,4,4,0x104,0x400,4,4,4,4,0x10,
            4,4,4,4,4,4,4,4,4,4,
            4,4,4,4,0x20,0x40,4,4,
            0xA8,0xA8,0xA8,0xA8
        };
        size_t pos = 8;
        size_t n   = sizeof aOff / sizeof aOff[0];
        size_t k;
        int    ok  = 1;

        for (k = 0; k < n; ++k) {
            if (pos + aLen[k] > (size_t)cbFile ||
                memcmp(pFile + pos, pSrc + aOff[k], aLen[k]) != 0) {
                ok = 0;
                break;
            }
            pos += aLen[k];
        }
        CHECK(ok, "field sequence mismatch");
        CHECK(pos == (size_t)cbFile, "trailing or missing bytes");
    }

    /* The wrapper must produce byte-identical output. */
    {
        const char *szPath2 = BrTmpPath(1, "slice4_53_cfg2");
        unsigned char *pFile2;
        long           cb2;

        BrSub1006A4A0(pSrc, (void *)(char *)szPath2);
        pf = fopen(szPath2, "rb");
        CHECK(pf != NULL, "wrapper produced no file");
        if (pf != NULL) {
            fseek(pf, 0, SEEK_END);
            cb2 = ftell(pf);
            fseek(pf, 0, SEEK_SET);
            CHECK(cb2 == cbFile, "wrapper size differs");
            pFile2 = (unsigned char *)malloc((size_t)cb2);
            if (pFile2 != NULL) {
                cbRead = fread(pFile2, 1, (size_t)cb2, pf);
                CHECK(cbRead == (size_t)cb2, "short read 2");
                CHECK(cb2 == cbFile &&
                      memcmp(pFile2, pFile, (size_t)cbFile) == 0,
                      "wrapper output differs");
                free(pFile2);
            }
            fclose(pf);
        }
        remove(szPath2);
    }

    /* A path that cannot be opened: the original returns 0 having closed
     * nothing, because nothing was opened. */
    CHECK(BrCfgSave1006A4A0(pSrc,
              "slice4_53_no_such_dir/x/y.tmp") == 0,
          "unopenable path must return 0");

    remove(szPath);
    free(pFile);
    free(pSrc);
}

/* ======================================================================
 * 5. Forwarders
 * ====================================================================== */

static void TestForwarders(void)
{
    unsigned char car[16];
    unsigned char probe[16];
    unsigned char before[16];
    BrOptUi       ui;
    BrPhaseCtx    ctx;
    BrOptObj      obj;
    int           i;

    g_nBankFlip = g_nCounters = g_nAutoSave = 0;
    BrGfx2C210();
    BrGfx31227();
    BrSub10041B50();
    CHECK(g_nBankFlip == 1, "0x1002C210 -> BrS17BankFlip");
    CHECK(g_nCounters == 1, "0x10031227 -> BrRenderCountersReset");
    CHECK(g_nAutoSave == 1, "0x10041B50 -> BrMenuAutoSaveName");

    g_nPoolEmit = 0;
    BrCarSub9020((struct BrCar *)car);
    CHECK(g_nPoolEmit == 1 && g_pPoolEmit == car,
          "0x10039020 -> BrPoolEmit(this)");

    g_nRca = 0;
    BrSub10037740(car, (void *)(intptr_t)5);
    CHECK(g_nRca == 1, "0x10037740 -> BrRcaLoadCar");
    CHECK(g_pRcaDest == car, "destination forwarded");
    CHECK(g_iRcaCar == 5, "second argument is an index, not a pointer");
    CHECK(g_cbRca == (size_t)BR_RCA_CAR_STRIDE, "bound is the car stride");

    /* 0x1003551B really does nothing: no store, no read of interest. */
    for (i = 0; i < 16; ++i)
        probe[i] = before[i] = (unsigned char)(i * 17 + 3);
    BrSub1003551B(probe);
    CHECK(memcmp(probe, before, sizeof probe) == 0,
          "0x1003551B must not touch its argument");

    /* The gate comes from the global, and BOTH values must pass through --
     * the suppression decision belongs to BrDPlaySendPair, not here. */
    memset(&ui, 0, sizeof ui);
    g_nTag4 = 0;
    g_brAA288C = 0;
    BrSub1003DA40(&ui, 0x77);
    CHECK(g_nTag4 == 1, "0x1003DA40 -> BrDPlaySendTag4");
    CHECK(g_pTag4Link == (const BrDPlayLink *)(const void *)&ui,
          "link forwarded");
    CHECK(g_gateTag4 == 0, "gate 0 forwarded");
    CHECK(g_valTag4 == 0x77u, "value forwarded");

    g_brAA288C = 1;
    BrSub1003DA40(&ui, 0x78);
    CHECK(g_nTag4 == 2, "second send still forwards");
    CHECK(g_gateTag4 == 1, "gate is READ FROM 0x10AA288C, not assumed zero");

    /* Phase forwarders: inert while unwired, exact once wired. */
    memset(&ctx, 0, sizeof ctx);
    memset(&obj, 0, sizeof obj);
    g_nAct = g_nLeave = 0;

    BrSlice4SetPhaseCtx(NULL);
    BrMenuSub10044B90(0);
    BrOptFn10044A30(&obj);
    CHECK(g_nAct == 0 && g_nLeave == 0,
          "unwired forwarders must be inert, not crash");

    BrSlice4SetPhaseCtx(&ctx);
    BrMenuSub10044B90(99);              /* argument is ignored, as in the
                                         * original, which takes none */
    BrOptFn10044A30(&obj);
    CHECK(g_nAct == 1 && g_pActCtx == &ctx, "0x10044B90 forwards the ctx");
    CHECK(g_nLeave == 1 && g_pLeaveCtx == &ctx,
          "0x10044A30 forwards the ctx");
    CHECK(g_pLeaveEnt == &obj,
          "the single declared argument is slice2_26's pEntity");
    BrSlice4SetPhaseCtx(NULL);
}

/* ======================================================================
 * 6. Session timer
 * ====================================================================== */

static void *g_hTimerWnd; static uint32_t g_idTimer, g_msTimer;
static void  *g_procTimer; static int g_nTimer;

static uint32_t TestSetTimer(void *hWnd, uint32_t idEvent,
                             uint32_t uElapseMs, void *pfnProc)
{
    g_hTimerWnd = hWnd; g_idTimer = idEvent; g_msTimer = uElapseMs;
    g_procTimer = pfnProc; ++g_nTimer; g_seqTimer = ++g_seq;
    return 0x4242u;
}

static void TestTimer(void)
{
    int marker = 0;
    BrPlatSetTimerFn pSaved = g_pfnBrPlatSetTimer;

    g_brP680584 = &marker;
    g_brA9CFFC  = 0;
    g_brA9BFDC  = 0;
    g_nC020 = g_nTimer = 0;
    g_seq = 0;
    g_pfnBrPlatSetTimer = TestSetTimer;

    CHECK(BrTimerStart1003C230() == 1, "0x1003C230 returns 1 in eax");

    CHECK(g_nC020 == 1 && g_nTimer == 1, "both calls made exactly once");
    CHECK(g_seqC020 < g_seqTimer, "0x1003C020 runs BEFORE SetTimer");
    CHECK(g_hTimerWnd == &marker, "window comes from 0x10680584");
    CHECK(g_idTimer == 1u && g_msTimer == 1000u && g_procTimer == NULL,
          "SetTimer(hWnd, 1, 1000, NULL)");
    CHECK(g_brA9BFDC == 0x4242u, "SetTimer result stored at 0x10A9BFDC");
    CHECK(g_brA9CFFC == 1, "0x10A9CFFC set to 1");

    /* The void form slice2_25 declares must do the same work. */
    g_brA9CFFC = 0;
    g_brA9BFDC = 0;
    BrSub1003C230();
    CHECK(g_nC020 == 2 && g_nTimer == 2, "void form does the same work");
    CHECK(g_brA9BFDC == 0x4242u && g_brA9CFFC == 1, "void form's effects");

    /* A NULL hook must not be dereferenced. */
    g_pfnBrPlatSetTimer = NULL;
    g_brA9BFDC = 0x11u;
    BrSub1003C230();
    CHECK(g_brA9BFDC == 0u, "NULL hook yields a zero timer id");

    g_pfnBrPlatSetTimer = pSaved;
}

/* ====================================================================== */

int main(void)
{
    TestMath();
    TestStringById();
    TestRelay();
    TestCfgSave();
    TestForwarders();
    TestTimer();

    printf("%s: %d checks, %d failures\n",
           g_nFail ? "FAILED" : "ok", g_nRun, g_nFail);
    return g_nFail ? 1 : 0;
}
