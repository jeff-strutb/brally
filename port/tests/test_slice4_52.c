/* test_slice4_52.c -- behavioural tests for port/src/slice4_52.c.
 *
 * Everything outside slice4_52.c is a TEST STAND-IN, clearly marked below.
 * The stand-ins that carry real behaviour (BrHandleLookup, BrTables64Clear,
 * BrDPlayRandStep, BrAdler32) implement exactly what the owning header's
 * contract says, so the assertions test the WRAPPER -- which is what this
 * packet actually contributes -- rather than re-testing somebody else's code.
 */

#include <assert.h>
#include "br_tmpfile.h"
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "slice4_52.h"
#include "slice3_33.h"
#include "slice3_39.h"
#include "slice1_07.h"
#include "slice2_22.h"
#include "slice2_14.h"
#include "slice1_01.h"

/* ==========================================================================
 * TEST STAND-INS
 * ========================================================================== */

/* --- br_bits.h 0x10074030 ------------------------------------------------ */
static void *const *g_lastTable;
static uint32_t     g_lastHandle;

void *BrHandleLookup(void *const *apTable, uint32_t handle)
{
    g_lastTable  = apTable;
    g_lastHandle = handle;
    if (handle < BR_HANDLE_MIN || handle > BR_HANDLE_MAX) {
        return NULL;
    }
    return apTable[handle];
}

/* --- slice1_07.h 0x1005FF30 body ----------------------------------------- */
void BrTables64Clear(uint32_t *pA, uint32_t *pB, uint32_t *pC)
{
    memset(pA, 0, BR_TABLE64_COUNT * sizeof(uint32_t));
    memset(pB, 0, BR_TABLE64_COUNT * sizeof(uint32_t));
    memset(pC, 0, BR_TABLE64_COUNT * sizeof(uint32_t));
}

/* --- slice3_39.h globals ------------------------------------------------- */
uint8_t  g_BrDikState[BR_DIK_COUNT];
int32_t  g_BrDikPrev [BR_DIK_COUNT];
int32_t  g_BrDikEdge [BR_DIK_COUNT];
BrPointI g_testAA2E80;
BrPointI *g_pBrAA2E80 = &g_testAA2E80;

/* --- 0x100603A0 ---------------------------------------------------------- */
void *g_brP680584;
static void *g_603A0This;
static void *g_603A0Arg;
static int   g_603A0Calls;

void BrSub100603A0(void *pThis, void *pArg)
{
    g_603A0This = pThis;
    g_603A0Arg  = pArg;
    g_603A0Calls++;
}

/* --- slice2_22.h --------------------------------------------------------- */
uint32_t BrDPlayRandStep(uint32_t *pSeed)
{
    /* slice2_22.h's contract: s <- (16807 * s) mod 2^27, plain uint32 maths. */
    *pSeed = (*pSeed * 16807u) & 0x07FFFFFFu;
    return *pSeed;
}

static const BrDPlayLink *g_tag3Link;
static int32_t            g_tag3Gate;
static int                g_tag3Calls;

int BrDPlaySendTag3(const BrDPlayLink *pLink, int32_t fGate)
{
    g_tag3Link = pLink;
    g_tag3Gate = fGate;
    g_tag3Calls++;
    return 0;
}

int32_t g_brAA288C;

/* --- slice1_01.h adler32 ------------------------------------------------- */
unsigned long BrAdler32(unsigned long adler, const unsigned char *pBuf,
                        unsigned int len)
{
    unsigned long a, b;
    unsigned int  i;

    if (pBuf == NULL) {
        return 1UL;
    }
    a = adler & 0xFFFFUL;
    b = (adler >> 16) & 0xFFFFUL;
    for (i = 0; i < len; ++i) {
        a = (a + pBuf[i]) % 65521UL;
        b = (b + a) % 65521UL;
    }
    return (b << 16) | a;
}

/* --- slice2_25.h globals the save routine writes ------------------------- */
const int32_t *g_brPACED34;
int32_t g_brAA2A08, g_br0AC64C, g_br0AC650, g_br0AC654, g_br0AC65C;

/* --- slice1_06.h --------------------------------------------------------- */
/* Not a compile-time initialiser: the path carries the pid so concurrent test
 * runs cannot truncate each other's file (see br_tmpfile.h). Assigned at the
 * top of main() instead, before anything under test reads it. */
static char g_seasonPath[256];
const char *const g_pszBrRallySeasonDat = g_seasonPath;

static const BrErrHost *g_errHost;
static int32_t          g_errIdx;
static int              g_errCalls;

void BrErrShow(const BrErrHost *pHost, int32_t idx)
{
    g_errHost = pHost;
    g_errIdx  = idx;
    g_errCalls++;
}

/* --- slice3_33.h allocation ---------------------------------------------- */
void *BrOperatorNew(uint32_t cb)
{
    /* Deliberately NOT zeroed -- 0x1007DFE0 is not calloc. */
    void *p = malloc(cb);
    if (p != NULL) {
        memset(p, 0xA5, cb);
    }
    return p;
}

/* Recorded f34 / f38 traffic. */
typedef struct CtlCall {
    BrUiCtl    *pCtl;
    BrUiPhase  *pOwner;
    float       x, y;
    int32_t     flags, a4, a5, a6, a7;
} CtlCall;

typedef struct TextCall {
    BrUiCtl    *pCtl;
    const void *pText;
    int32_t     a2, a3;
    const void *pStyle;
} TextCall;

static CtlCall  g_place[16];
static int      g_cPlace;
static TextCall g_text[16];
static int      g_cText;

static void TestF34(BrUiCtl *pThis, const void *pText, int32_t a2, int32_t a3,
                    const void *pStyle)
{
    g_text[g_cText].pCtl   = pThis;
    g_text[g_cText].pText  = pText;
    g_text[g_cText].a2     = a2;
    g_text[g_cText].a3     = a3;
    g_text[g_cText].pStyle = pStyle;
    g_cText++;
}

static void TestF38(BrUiCtl *pThis, BrUiPhase *pOwner, float x, float y,
                    int32_t flags, int32_t a4, int32_t a5,
                    int32_t a6, int32_t a7)
{
    g_place[g_cPlace].pCtl   = pThis;
    g_place[g_cPlace].pOwner = pOwner;
    g_place[g_cPlace].x      = x;
    g_place[g_cPlace].y      = y;
    g_place[g_cPlace].flags  = flags;
    g_place[g_cPlace].a4     = a4;
    g_place[g_cPlace].a5     = a5;
    g_place[g_cPlace].a6     = a6;
    g_place[g_cPlace].a7     = a7;
    g_cPlace++;
}

static const BrUiCtlVtbl g_testCtlVtbl = { { 0 }, TestF34, TestF38 };

BrUiCtl *BrUiCtlCtor(BrUiCtl *pThis)
{
    memset(pThis, 0, sizeof(*pThis));
    pThis->pVtbl = &g_testCtlVtbl;
    return pThis;
}

/* ==========================================================================
 * 0x10074030  BrStrGet
 * ========================================================================== */

static void test_strget(void)
{
    static const char s1[]  = "one";
    static const char sMax[] = "max";

    g_apBrStrTable[1]             = (void *)(size_t)0;  /* set below */
    g_apBrStrTable[1]             = (void *)s1;
    g_apBrStrTable[BR_HANDLE_MAX] = (void *)sMax;

    /* Delegation: the wrapper's whole job is to supply the hardcoded table. */
    assert(BrStrGet(1) == s1);
    assert(g_lastTable == g_apBrStrTable);
    assert(g_lastHandle == 1u);

    assert(BrStrGet(BR_HANDLE_MAX) == sMax);

    /* Boundaries actually present in the original: 0 is the reserved "none",
     * BR_HANDLE_MAX+1 is out of range, and the compare is UNSIGNED so a
     * negative id is rejected rather than wrapping into the table. */
    assert(BrStrGet(0) == NULL);
    assert(BrStrGet(BR_HANDLE_MAX + 1) == NULL);
    assert(BrStrGet(-1) == NULL);
    assert(BrStrGet(-0x7FFFFFFF) == NULL);
}

/* ==========================================================================
 * 0x10010960 / 0x10010980
 * ========================================================================== */

static void test_polydist(void)
{
    BrScrPt pt;

    memset(&pt, 0, sizeof(pt));
    pt.f00 = 1.0f; pt.f04 = 2.0f; pt.f08 = 3.0f;
    pt.f0C = -7.5f;
    pt.f10 = 1024.0f;

    /* A bare field load: no clamp, no abs, and the negative survives (which is
     * what makes the point count as OUTSIDE in BrPolyClipPlane). */
    assert(BrPolyDistX((const struct BrScrPt *)&pt) == -7.5f);
    assert(BrPolyDistY((const struct BrScrPt *)&pt) == 1024.0f);

    /* The two read different fields -- guard against a copy/paste swap. */
    pt.f0C = 5.0f;
    assert(BrPolyDistX((const struct BrScrPt *)&pt) == 5.0f);
    assert(BrPolyDistY((const struct BrScrPt *)&pt) == 1024.0f);
}

/* ==========================================================================
 * 0x1003BD50  BrRandom
 * ========================================================================== */

static void test_random(void)
{
    int i;

    /* The wrapper's contract: advance the ORIGINAL's own state word and hand
     * back the new state. */
    g_brA9BFD0 = 1u;
    assert(BrRandom() == 16807);
    assert(g_brA9BFD0 == 16807u);

    /* 0 is absorbing and unguarded. */
    g_brA9BFD0 = 0u;
    assert(BrRandom() == 0);
    assert(g_brA9BFD0 == 0u);

    /* Modulus 2^27, so the result never has the sign bit set even though the
     * declared return type is signed. */
    g_brA9BFD0 = 0x07FFFFFFu;
    for (i = 0; i < 1000; ++i) {
        int v = BrRandom();
        assert(v >= 0);
        assert((uint32_t)v <= 0x07FFFFFFu);
        assert((uint32_t)v == g_brA9BFD0);
    }
}

/* ==========================================================================
 * 0x1005FF30  BrMenuSub1005FF30
 * ========================================================================== */

static void test_tables64(void)
{
    int i;

    memset(g_BrDikState, 0xFF, sizeof(g_BrDikState));
    for (i = 0; i < BR_DIK_COUNT; ++i) {
        g_BrDikEdge[i] = -1;
        g_BrDikPrev[i] = -1;
    }

    BrMenuSub1005FF30();

    /* 0x40 dwords == the WHOLE 256-byte key-state buffer ... */
    for (i = 0; i < BR_DIK_COUNT; ++i) {
        assert(g_BrDikState[i] == 0);
    }
    /* ... but only the first 64 entries of the two DWORD arrays.  This
     * asymmetry is the original's; entries 64..255 are never cleared. */
    for (i = 0; i < 64; ++i) {
        assert(g_BrDikEdge[i] == 0);
        assert(g_BrDikPrev[i] == 0);
    }
    for (i = 64; i < BR_DIK_COUNT; ++i) {
        assert(g_BrDikEdge[i] == -1);
        assert(g_BrDikPrev[i] == -1);
    }
}

/* ==========================================================================
 * 0x10048470  BrUiScreenCtor
 * ========================================================================== */

static void test_screenctor(void)
{
    BrUiScreen scr;
    BrUiScreen *pRet;
    int i;

    memset(&scr, 0xAA, sizeof(scr));

    pRet = (BrUiScreen *)BrUiScreenCtor((struct BrUiScreen *)&scr);

    assert(pRet == &scr);          /* returns `this` */
    assert(scr.f10 == 0);
    assert(scr.cCtl == 0);
    assert(scr.cSel == 0);
    assert(scr.pOwner == NULL);
    assert(scr.fX == 0.0f);
    assert(scr.fY == 0.0f);
    for (i = 0; i < BR_UI_SCREEN_CTL_MAX; ++i) {
        assert(scr.apCtl[i] == NULL);
    }
}

/* ==========================================================================
 * 0x10060260  BrSub10060260
 * ========================================================================== */

static void test_60260(void)
{
    int marker = 0;

    g_brP680584  = &marker;
    g_603A0Calls = 0;

    /* The declared parameter has no counterpart in the original: both operands
     * are read from globals, so whatever is passed must be ignored. */
    BrSub10060260((void *)&marker);
    assert(g_603A0Calls == 1);
    assert(g_603A0This == (void *)g_pBrAA2E80);
    assert(g_603A0Arg  == (void *)&marker);

    BrSub10060260(NULL);
    assert(g_603A0Calls == 2);
    assert(g_603A0This == (void *)g_pBrAA2E80);
    assert(g_603A0Arg  == (void *)&marker);
}

/* ==========================================================================
 * 0x1005F530  BrSub1005F530
 * ========================================================================== */

static int g_relCount;
static BrUiAssetObj *g_relLast;

static void TestRelease(BrUiAssetObj *pThis)
{
    g_relCount++;
    g_relLast = pThis;
}

/* Shrink the table from inside a Release, to exercise the re-read. */
static void TestReleaseShrink(BrUiAssetObj *pThis)
{
    g_relCount++;
    g_relLast = pThis;
    g_brAA28D4 = 1u;
}

static const BrUiAssetObjVtbl g_relVtbl       = { { 0, 0 }, TestRelease };
static const BrUiAssetObjVtbl g_relVtblShrink = { { 0, 0 }, TestReleaseShrink };

static void test_5F530(void)
{
    BrUiAssetObj objs[4];
    int i;

    for (i = 0; i < 4; ++i) {
        objs[i].pVtbl = &g_relVtbl;
    }

    /* Gate 1: g_brA9D070 == 0 does nothing at all. */
    memset(g_aBrUiAssetRec, 0, sizeof(g_aBrUiAssetRec));
    for (i = 0; i < 4; ++i) { g_aBrUiAssetRec[i].pObj = &objs[i]; }
    g_brA9D070 = 0;
    g_brAA28D4 = 4u;
    g_relCount = 0;
    BrSub1005F530();
    assert(g_relCount == 0);
    assert(g_aBrUiAssetRec[0].pObj == &objs[0]);

    /* Gate 2: a zero count (tested as a WORD) does nothing. */
    g_brA9D070 = 1;
    g_brAA28D4 = 0x10000u;   /* low 16 bits are zero */
    g_relCount = 0;
    BrSub1005F530();
    assert(g_relCount == 0);
    assert(g_aBrUiAssetRec[0].pObj == &objs[0]);

    /* Normal walk: every non-null slot is released exactly once and nulled;
     * a null slot is skipped without any call. */
    g_aBrUiAssetRec[2].pObj = NULL;
    g_brAA28D4 = 4u;
    g_relCount = 0;
    BrSub1005F530();
    assert(g_relCount == 3);
    for (i = 0; i < 4; ++i) {
        assert(g_aBrUiAssetRec[i].pObj == NULL);
    }

    /* The bound is re-read after each Release, so shrinking it stops early. */
    for (i = 0; i < 4; ++i) { g_aBrUiAssetRec[i].pObj = &objs[i]; }
    objs[0].pVtbl = &g_relVtblShrink;
    g_brAA28D4 = 4u;
    g_relCount = 0;
    BrSub1005F530();
    assert(g_relCount == 1);
    assert(g_aBrUiAssetRec[0].pObj == NULL);
    assert(g_aBrUiAssetRec[1].pObj == &objs[1]);
    objs[0].pVtbl = &g_relVtbl;
}

/* ==========================================================================
 * 0x1003D9F0  BrSub1003D9F0
 * ========================================================================== */

static void test_3D9F0(void)
{
    BrDPlayLink link;

    memset(&link, 0, sizeof(link));

    /* The wrapper's only job: hand the object through and read the gate out of
     * 0x10AA288C rather than taking it as an argument. */
    g_brAA288C  = 0;
    g_tag3Calls = 0;
    BrSub1003D9F0((struct BrOptUi *)(void *)&link);
    assert(g_tag3Calls == 1);
    assert(g_tag3Link == &link);
    assert(g_tag3Gate == 0);

    g_brAA288C = 7;
    BrSub1003D9F0((struct BrOptUi *)(void *)&link);
    assert(g_tag3Calls == 2);
    assert(g_tag3Gate == 7);

    /* NULL is legal and still reaches the sender, which does its own test. */
    BrSub1003D9F0(NULL);
    assert(g_tag3Calls == 3);
    assert(g_tag3Link == NULL);
}

/* ==========================================================================
 * 0x100709A0  BrMenuSub100709A0
 * ========================================================================== */

static uint32_t rd32(const unsigned char *p)
{
    /* Byte-wise: the file is written with the host's own dword layout, and
     * this test only compares it against a value read back the same way. */
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void test_save(void)
{
    static int32_t block[BR_SEASON_BLOCK_SIZE / 4];
    unsigned char  file[1024];
    unsigned char  expect[4];
    unsigned long  sum;
    size_t         n;
    FILE          *pf;
    int            i;

    for (i = 0; i < (int)(sizeof(block) / sizeof(block[0])); ++i) {
        block[i] = i * 3 + 1;
    }
    g_brPACED34 = block;
    g_brAA2A08 = 0x11111111; g_br0AC64C = 0x22222222;
    g_br0AC650 = 0x33333333; g_br0AC654 = 0x44444444;
    g_br0AC65C = 0x55555555;
    for (i = 0; i < BR_SEASON_TAIL_SIZE; ++i) {
        g_brAD0990[i] = (unsigned char)(0x80 + i);
    }

    remove(g_pszBrRallySeasonDat);
    BrMenuSub100709A0();

    pf = fopen(g_pszBrRallySeasonDat, "rb");
    assert(pf != NULL);
    n = fread(file, 1, sizeof(file), pf);
    fclose(pf);
    remove(g_pszBrRallySeasonDat);

    /* 4 magic + 4 checksum + 0x200 block + 5*4 loose dwords + 0x80 tail. */
    assert(n == 4u + 4u + BR_SEASON_BLOCK_SIZE + 20u + BR_SEASON_TAIL_SIZE);

    /* The magic comes out of the writable global, not a literal, and no NUL
     * is written: exactly four bytes. */
    memcpy(expect, g_brB5D94, 4);
    assert(memcmp(file, expect, 4) == 0);

    /* The checksum covers ONLY the 0x200 block, and it precedes the data. */
    sum = BrAdler32(BrAdler32(0, NULL, 0),
                    (const unsigned char *)block, BR_SEASON_BLOCK_SIZE);
    assert(rd32(file + 4) == (uint32_t)(int32_t)sum);

    assert(memcmp(file + 8, block, BR_SEASON_BLOCK_SIZE) == 0);

    /* The five loose dwords, in the original's order. */
    assert(rd32(file + 8 + BR_SEASON_BLOCK_SIZE +  0) == 0x11111111u);
    assert(rd32(file + 8 + BR_SEASON_BLOCK_SIZE +  4) == 0x22222222u);
    assert(rd32(file + 8 + BR_SEASON_BLOCK_SIZE +  8) == 0x33333333u);
    assert(rd32(file + 8 + BR_SEASON_BLOCK_SIZE + 12) == 0x44444444u);
    assert(rd32(file + 8 + BR_SEASON_BLOCK_SIZE + 16) == 0x55555555u);

    assert(memcmp(file + 8 + BR_SEASON_BLOCK_SIZE + 20,
                  g_brAD0990, BR_SEASON_TAIL_SIZE) == 0);
}

/* ==========================================================================
 * 0x10038F30  BrSub10038F30
 * ========================================================================== */

static int     g_trace[64];
static int     g_cTrace;
static jmp_buf g_exitJmp;
static int     g_exitCode;

static void trace(int id) { g_trace[g_cTrace++] = id; }

static void S_1002C4A0(void) { trace(1); }
static void S_10016990(void) { trace(2); }
static void S_B501CC(void)   { trace(3); }
static void S_10079550(void) { trace(4); }
static void S_10078BC0(void) { trace(5); }
static void S_10078DB0(void) { trace(6); }
static void S_10073730(void) { trace(7); }
static void S_10005BE0(int a){ assert(a == 1); trace(8); }
static void S_1003BFD0(void) { trace(9); }
static void S_1003BF60(void) { trace(10); }
static void S_10002CF0(void) { trace(11); }
static void S_10008B80(void) { trace(12); }
static void S_18AA0D0(void)  { trace(13); }
static void S_690A28(void)   { trace(14); }
static void S_10061620(void) { trace(15); }
static void S_10008970(void) { trace(16); }
static void S_1002AEA0(void) { trace(17); }
static void S_10074050(void) { trace(18); }
static void S_CoUninit(void) { trace(19); }
static void S_Exit(int code) { g_exitCode = code; trace(20); longjmp(g_exitJmp, 1); }

static int g_slot6Calls;
static int g_slot6Arg;
static BrShutObj *g_slot6This;

static void S_Slot6(BrShutObj *pThis, int a)
{
    g_slot6Calls++;
    g_slot6This = pThis;
    g_slot6Arg  = a;
    trace(0);
}

static void test_shutdown(void)
{
    static const BrShutObjVtbl vtbl = { { 0, 0, 0, 0, 0, 0 }, S_Slot6 };
    static BrShutObj  obj;
    static BrShutObj *pObj;
    static int32_t    n0AC300, n22AF18, n0940A4;
    static BrShutdownHost host;
    static const int full[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
                                13, 14, 15, 16, 17, 18, 19, 20 };
    static const int bare[] = { 1, 2, 4, 5, 6, 7, 9, 10, 12, 15, 16, 17,
                                18, 19, 20 };
    int i;

    obj.pVtbl = &vtbl;
    obj.f68   = 0x1234;
    pObj      = &obj;

    host.ppAA2904 = &pObj;
    host.pn0AC300 = &n0AC300;
    host.pn22AF18 = &n22AF18;
    host.pn0940A4 = &n0940A4;
    host.pfn1002C4A0 = S_1002C4A0;
    host.pfn10016990 = S_10016990;
    host.pfnB501CC   = S_B501CC;
    host.pfn10079550 = S_10079550;
    host.pfn10078BC0 = S_10078BC0;
    host.pfn10078DB0 = S_10078DB0;
    host.pfn10073730 = S_10073730;
    host.pfn10005BE0 = S_10005BE0;
    host.pfn1003BFD0 = S_1003BFD0;
    host.pfn1003BF60 = S_1003BF60;
    host.pfn10002CF0 = S_10002CF0;
    host.pfn10008B80 = S_10008B80;
    host.pfn18AA0D0  = S_18AA0D0;
    host.pfn690A28   = S_690A28;
    host.pfn10061620 = S_10061620;
    host.pfn10008970 = S_10008970;
    host.pfn1002AEA0 = S_1002AEA0;
    host.pfn10074050 = S_10074050;
    host.pfnCoUninitialize = S_CoUninit;
    host.pfnExit     = S_Exit;
    g_pBrShutdownHost = &host;

    /* Everything enabled: the full sequence, in order, ending in exit(). */
    n0AC300 = 1; n22AF18 = 1; n0940A4 = 1;
    g_cTrace = 0; g_slot6Calls = 0; g_exitCode = -1;
    if (setjmp(g_exitJmp) == 0) {
        BrSub10038F30(3);
        assert(0 && "0x1007CC00 is exit(); must not return");
    }
    assert(g_exitCode == 3);
    assert(g_slot6Calls == 1);
    assert(g_slot6This == &obj);
    assert(g_slot6Arg == 0);
    assert(obj.f68 == 0);            /* zeroed before the virtual call */
    assert(g_cTrace == (int)(sizeof(full) / sizeof(full[0])));
    for (i = 0; i < g_cTrace; ++i) {
        assert(g_trace[i] == full[i]);
    }

    /* Every guard off: the first block needs BOTH the object and 0x100AC300,
     * and the three nullable function-pointer globals drop out. */
    obj.f68 = 0x1234;
    n0AC300 = 0; n22AF18 = 0; n0940A4 = 0;
    host.pfnB501CC  = NULL;
    host.pfn18AA0D0 = NULL;
    host.pfn690A28  = NULL;
    g_cTrace = 0; g_slot6Calls = 0;
    if (setjmp(g_exitJmp) == 0) {
        BrSub10038F30(0);
        assert(0);
    }
    assert(g_slot6Calls == 0);
    assert(obj.f68 == 0x1234);       /* untouched when the gate is closed */
    assert(g_cTrace == (int)(sizeof(bare) / sizeof(bare[0])));
    for (i = 0; i < g_cTrace; ++i) {
        assert(g_trace[i] == bare[i]);
    }

    /* A null object closes the same gate even with 0x100AC300 set. */
    pObj = NULL;
    n0AC300 = 1;
    g_cTrace = 0; g_slot6Calls = 0;
    if (setjmp(g_exitJmp) == 0) {
        BrSub10038F30(0);
        assert(0);
    }
    assert(g_slot6Calls == 0);
    pObj = &obj;
}

/* ==========================================================================
 * 0x10008CF0  BrLogPrint
 * ========================================================================== */

static int        g_logTrace[16];
static int        g_cLogTrace;
static uint32_t  *g_dlCursor;
static int32_t    g_screenW;
static const char *g_drawnText;
static int        g_drawnX, g_drawnY;
static void      *g_submitted;
static int        g_keyCalls;
static int        g_keyReturn;
static int        g_sleepCalls;
static uint32_t  *g_dlBase;
static jmp_buf    g_logJmp;

static void L_16990(void)  { g_logTrace[g_cLogTrace++] = 1; }
static void L_19260(void)  { g_logTrace[g_cLogTrace++] = 2;
                             g_dlBase = g_dlCursor; }
static void L_19270(void)  { g_logTrace[g_cLogTrace++] = 3; }
static void L_192F0(int a) { assert(a == 0x14); g_logTrace[g_cLogTrace++] = 4; }

static void L_Draw(const char *psz, int x, int y)
{
    g_logTrace[g_cLogTrace++] = 5;
    g_drawnText = psz; g_drawnX = x; g_drawnY = y;
}

static void L_Submit(void *pDl)
{
    g_logTrace[g_cLogTrace++] = 6;
    g_submitted = pDl;
}

static void L_Shutdown(int code) { assert(code == 1); longjmp(g_logJmp, 1); }
static int  L_Key(int vk) { assert(vk == BR_LOGPRINT_VK_ESCAPE);
                            g_keyCalls++; return g_keyReturn; }
static void L_Sleep(unsigned ms) { assert(ms == 1); g_sleepCalls++; }

static void runLogPrint(const char *psz)
{
    static BrLogHost host;

    host.pfn10016990 = L_16990;
    host.pfn10019260 = L_19260;
    host.pfn10019270 = L_19270;
    host.pfn100192F0 = L_192F0;
    host.pfnTextDraw = L_Draw;
    host.pfnSubmit   = L_Submit;
    host.pfnShutdown = L_Shutdown;
    host.pfnKeyAsync = L_Key;
    host.pfnSleep    = L_Sleep;
    host.pnScreenW   = &g_screenW;
    host.ppDlCursor  = &g_dlCursor;
    g_pBrLogHost = &host;

    g_cLogTrace = 0; g_keyCalls = 0; g_sleepCalls = 0;
    g_dlCursor = NULL; g_dlBase = NULL;
    if (setjmp(g_logJmp) == 0) {
        BrLogPrint(psz);
        assert(0 && "BrLogPrint must not return");
    }
}

static void test_logprint(void)
{
    static const char msg[] = "LoadCar()";
    static const int  order[] = { 1, 2, 3, 4, 5, 6 };
    int i;

    /* ESC held from the first poll: one poll, no sleep, shutdown(1). */
    g_screenW   = 640;
    g_keyReturn = 0x8000;
    runLogPrint(msg);

    assert(g_cLogTrace == (int)(sizeof(order) / sizeof(order[0])));
    for (i = 0; i < g_cLogTrace; ++i) {
        assert(g_logTrace[i] == order[i]);
    }
    assert(g_drawnText == msg);
    assert(g_drawnX == 320);
    assert(g_drawnY == BR_LOGPRINT_TEXT_Y);
    assert(g_keyCalls == 1);
    assert(g_sleepCalls == 0);

    /* The cursor global points at the local list before the terminator is
     * appended, and is advanced by exactly two dwords (eight bytes). */
    assert(g_dlBase != NULL);
    assert(g_dlCursor == g_dlBase + 2);
    assert(g_dlBase[0] == 0xB8000000u);   /* G_ENDDL */
    assert(g_dlBase[1] == 0u);
    assert(g_submitted == (void *)g_dlBase);

    /* Signed halving, not a logical shift: a negative width truncates toward
     * zero exactly as `cdq / sub / sar` does. */
    g_screenW   = -3;
    g_keyReturn = 0x8000;
    runLogPrint(msg);
    assert(g_drawnX == -1);
}

/* Spins a few times before ESC, to prove the loop polls then sleeps. */
static int g_spinLimit;
static int L_KeySpin(int vk)
{
    assert(vk == BR_LOGPRINT_VK_ESCAPE);
    g_keyCalls++;
    return (g_keyCalls >= g_spinLimit) ? 1 : 0x10000;
}

static void test_logprint_spin(void)
{
    static BrLogHost host;
    static const char msg[] = "x";

    host.pfn10016990 = L_16990;
    host.pfn10019260 = L_19260;
    host.pfn10019270 = L_19270;
    host.pfn100192F0 = L_192F0;
    host.pfnTextDraw = L_Draw;
    host.pfnSubmit   = L_Submit;
    host.pfnShutdown = L_Shutdown;
    host.pfnKeyAsync = L_KeySpin;
    host.pfnSleep    = L_Sleep;
    host.pnScreenW   = &g_screenW;
    host.ppDlCursor  = &g_dlCursor;
    g_pBrLogHost = &host;

    g_screenW = 100;
    g_cLogTrace = 0; g_keyCalls = 0; g_sleepCalls = 0; g_spinLimit = 4;
    g_dlCursor = NULL;
    if (setjmp(g_logJmp) == 0) {
        BrLogPrint(msg);
        assert(0);
    }
    assert(g_keyCalls == 4);
    assert(g_sleepCalls == 3);   /* one sleep per non-ESC poll */
}

/* ==========================================================================
 * 0x10051990  BrOptFn10051990
 * ========================================================================== */

static void test_51990(void)
{
    static const char label[] = "SCREEN";
    static const char style;
    static BrUi51990Ctx ctx;
    BrUiPhase  phase;
    BrUiScreen *pScr;
    int i;

    memset(&phase, 0, sizeof(phase));
    g_apBrStrTable[0x42] = (void *)label;

    ctx.pErrHost  = NULL;
    ctx.p0AB438   = &style;
    ctx.p1003F440 = (void (*)(void *))0;
    ctx.p1003F540 = (void (*)(void *))0;
    ctx.p100471F0 = (void (*)(void *))0;
    ctx.p10047120 = (void (*)(void *))0;
    ctx.p10047360 = (void (*)(void *))0;
    g_pBrUi51990Ctx = &ctx;

    g_cPlace = 0;
    g_cText  = 0;
    g_errCalls = 0;

    BrOptFn10051990((struct BrOptObj *)(void *)&phase);

    /* One screen, registered in the phase, counter advanced. */
    assert(phase.cScreen == 1);
    assert(phase.f12 == 0);
    assert(phase.aF6C[0] == 1);
    pScr = phase.apScreen[0];
    assert(pScr != NULL);
    assert(pScr->pOwner == &phase);
    assert(pScr->fX == 195.0f);
    assert(pScr->fY == 130.0f);

    /* Six controls; only the last one is selectable. */
    assert(g_cPlace == 6);
    assert(pScr->cCtl == 6);
    assert(pScr->cSel == 1);
    assert(g_errCalls == 0);

    /* f38's fourth and fifth arguments are the literals 2 and 5 everywhere,
     * and the owner is the PHASE, never the screen. */
    for (i = 0; i < 6; ++i) {
        assert(g_place[i].a4 == 2);
        assert(g_place[i].a5 == 5);
        assert(g_place[i].pOwner == &phase);
        assert(g_place[i].pCtl == pScr->apCtl[i]);
    }

    /* The root control. */
    assert(g_place[0].x == 0.0f && g_place[0].y == 0.0f);
    assert(g_place[0].flags == 9 && g_place[0].a6 == 0 && g_place[0].a7 == 0);

    /* Controls 2 and 3 are ABSOLUTE, not fX/fY-relative -- the only two in
     * the whole family that are. */
    assert(g_place[1].x == 0.0f  && g_place[1].y == 29.0f);
    assert(g_place[1].a7 == 0x4E && g_place[1].a6 == 0);
    assert(g_place[2].x == 13.0f && g_place[2].y == 7.0f);
    assert(g_place[2].a7 == 0x4F && g_place[2].a6 == 0);

    assert(g_place[3].x == 16.0f  && g_place[3].y == 153.0f);
    assert(g_place[3].a6 == 1 && g_place[3].a7 == 0x47);
    assert(g_place[4].x == 392.0f && g_place[4].y == 181.0f);
    assert(g_place[4].a6 == 1 && g_place[4].a7 == 0x48);

    /* The last control is the only one placed relative to fX, the only one
     * with text, and the only one whose f1E20C is 2 rather than 3. */
    assert(g_place[5].x == pScr->fX && g_place[5].y == 460.0f);
    assert(g_place[5].flags == 0x102001);
    assert(g_place[5].a6 == 0 && g_place[5].a7 == -1);
    assert(pScr->apCtl[5]->f1E20C == 2);

    assert(g_cText == 1);
    assert(g_text[0].pCtl == pScr->apCtl[5]);
    assert(g_text[0].pText == label);   /* BrStrGet(0x42) */
    assert(g_text[0].a2 == 1);
    assert(g_text[0].a3 == 0);          /* 0, not 1 as in slice3_33's twins */
    assert(g_text[0].pStyle == &style);

    /* Only the last control gets a text style; the first five have none. */
    for (i = 0; i < 5; ++i) {
        assert(pScr->apCtl[i]->f1E20C == 0);
    }

    for (i = 0; i < 6; ++i) {
        free(pScr->apCtl[i]);
    }
    free(pScr);
}

/* ========================================================================== */

int main(void)
{
    /* The POINTER is const (slice1_06.h declares it so); the buffer is not.
     * Filled here rather than at file scope because the name carries the pid. */
    snprintf(g_seasonPath, sizeof g_seasonPath, "%s",
             BrTmpPath(0, "test_slice4_52_season"));
    test_strget();
    test_polydist();
    test_random();
    test_tables64();
    test_screenctor();
    test_60260();
    test_5F530();
    test_3D9F0();
    test_save();
    test_shutdown();
    test_logprint();
    test_logprint_spin();
    test_51990();

    printf("slice4_52: all tests passed\n");
    return 0;
}
