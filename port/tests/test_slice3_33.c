/* test_slice3_33.c -- behaviour tests for slice3_33.c.
 *
 * The five functions under test do not compute anything, so the properties
 * worth asserting are structural: the row cursor arithmetic (which is where
 * the x87 sign convention could have been got backwards), the bookkeeping
 * invariants that hold across all five, and the four places where the
 * repeated pattern deliberately breaks. Each break is asserted as the
 * ORIGINAL behaves, not as it "should".
 *
 * Every stand-in below is a TEST-ONLY substitute for a cross-slice callee.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "slice3_33.h"

static int g_cFail;

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);      \
            g_cFail++;                                                  \
        }                                                               \
    } while (0)

/* ==========================================================================
 * Recording vtables
 * ========================================================================== */

typedef struct PlaceRec {
    BrUiCtl   *pCtl;
    BrUiPhase *pOwner;
    float      x, y;
    int32_t    flags, a4, a5, a6, a7;
} PlaceRec;

typedef struct TextRec {
    BrUiCtl    *pCtl;
    const void *pText;
    int32_t     a2, a3;
    const void *pStyle;
} TextRec;

typedef struct RowRec {
    const void *pText;
    int32_t     a2, a3, a5;
    const void *pStyle;
} RowRec;

#define MAXREC 64

static PlaceRec g_aPlace[MAXREC];
static int      g_cPlace;
static TextRec  g_aText[MAXREC];
static int      g_cText;
static RowRec   g_aRow[MAXREC];
static int      g_cRow;
static int      g_cListCfg;

static void RecPlace(BrUiCtl *pThis, BrUiPhase *pOwner, float x, float y,
                     int32_t flags, int32_t a4, int32_t a5,
                     int32_t a6, int32_t a7)
{
    PlaceRec *p;
    if (g_cPlace >= MAXREC) { abort(); }
    p = &g_aPlace[g_cPlace++];
    p->pCtl = pThis; p->pOwner = pOwner; p->x = x; p->y = y;
    p->flags = flags; p->a4 = a4; p->a5 = a5; p->a6 = a6; p->a7 = a7;
}

static void RecText(BrUiCtl *pThis, const void *pText,
                    int32_t a2, int32_t a3, const void *pStyle)
{
    TextRec *p;
    if (g_cText >= MAXREC) { abort(); }
    p = &g_aText[g_cText++];
    p->pCtl = pThis; p->pText = pText;
    p->a2 = a2; p->a3 = a3; p->pStyle = pStyle;
}

static void RecRow(BrUiCtlSub *pThis, const void *pText, int32_t a2,
                   int32_t a3, const void *pStyle, int32_t a5)
{
    RowRec *p;
    (void)pThis;
    if (g_cRow >= MAXREC) { abort(); }
    p = &g_aRow[g_cRow++];
    p->pText = pText; p->a2 = a2; p->a3 = a3; p->pStyle = pStyle; p->a5 = a5;
}

static void RecListCfg(BrUiCtlSub *pThis, int32_t a1, const void *pStyle,
                       int32_t a3, int32_t a4, int32_t a5)
{
    (void)pThis; (void)pStyle;
    CHECK(a1 == 0x40001);
    CHECK(a3 == 5);
    CHECK(a4 == 0);
    CHECK(a5 == -1);
    g_cListCfg++;
}

static const BrUiCtlVtbl    g_Vtbl    = { { 0 }, RecText, RecPlace };
static const BrUiCtlSubVtbl g_SubVtbl = { { 0 }, RecRow, RecListCfg };

/* ==========================================================================
 * Stand-ins for the cross-slice callees (TEST ONLY)
 * ========================================================================== */

/* XSLICE 0x1007DFE0 stand-in. The original does NOT zero; the constructors
 * below do, which is what the real 0x10048470 / 0x100476C0 must also do for
 * the +0x2AB4 increment to mean anything. */
static int g_cAlloc;
static int g_iAllocFail = -1;   /* fail the Nth call when >= 0 */

void *BrOperatorNew(uint32_t cb)
{
    if (g_iAllocFail >= 0 && g_cAlloc == g_iAllocFail) {
        g_cAlloc++;
        return NULL;
    }
    g_cAlloc++;
    return malloc(cb);
}

/* XSLICE 0x10048470 stand-in. */
BrUiScreen *BrUiScreenCtor(BrUiScreen *pThis)
{
    memset(pThis, 0, sizeof(*pThis));
    return pThis;
}

/* XSLICE 0x100476C0 stand-in. */
BrUiCtl *BrUiCtlCtor(BrUiCtl *pThis)
{
    memset(pThis, 0, sizeof(*pThis));
    pThis->pVtbl        = &g_Vtbl;
    pThis->f3838.pVtbl  = &g_SubVtbl;
    return pThis;
}

/* XSLICE 0x10074030 stand-in: ids 1..0x12E resolve, everything else is NULL,
 * matching the real bounds check documented in br_bits.h. */
static char g_aStrPool[0x130];
static int  g_cStrGet;

const char *BrStrGet(int id)
{
    g_cStrGet++;
    if (id < 1 || id >= 0x12F) {
        return NULL;
    }
    return &g_aStrPool[id];
}

/* XSLICE 0x1003E260 stand-in (slice1_06.c is not linked into this test). */
static int g_cErr;
static int g_idxErr = -1;

void BrErrShow(const BrErrHost *pHost, int32_t idx)
{
    (void)pHost;
    g_cErr++;
    g_idxErr = idx;
}

/* XSLICE 0x10040330 stand-in. */
static int32_t g_kindSeen = -1;
static int32_t FindConflicts(int32_t kind)
{
    g_kindSeen = kind;
    return 0x5A5A;
}

/* Distinct hook identities for the few installs the tests assert on. */
static void Hook1(void *p) { (void)p; }
static void Hook2(void *p) { (void)p; }
static void Hook3(void *p) { (void)p; }
static void Hook4(void *p) { (void)p; }

/* ==========================================================================
 * Fixture
 * ========================================================================== */

static BrUiBuildHooks g_Hooks;
static BrUiBuildCtx   g_Ctx;
static BrUiPhase      g_Phase;
static BrUiStrEnt     g_aTbl[BR_UI_AB330_COUNT];
static int32_t        g_aAC520[8];

static void Reset(void)
{
    int i;

    memset(&g_Hooks, 0, sizeof(g_Hooks));
    g_Hooks.p10047360 = Hook1;
    g_Hooks.p10042B30 = Hook2;
    g_Hooks.p100464E0 = Hook3;
    g_Hooks.p1003EC80 = Hook4;

    for (i = 0; i < BR_UI_AB330_COUNT; ++i) {
        g_aTbl[i].idText = i + 1;   /* all resolvable by default */
        g_aTbl[i].f04    = 0;
    }
    for (i = 0; i < 8; ++i) {
        g_aAC520[i] = 100 + i;
    }

    memset(&g_Ctx, 0, sizeof(g_Ctx));
    g_Ctx.pHooks       = &g_Hooks;
    g_Ctx.aAB330       = g_aTbl;
    g_Ctx.aAC520       = g_aAC520;
    g_Ctx.pfn10040330  = FindConflicts;
    g_Ctx.nAB428       = 0;      /* the image's value */
    g_Ctx.nAB42C       = 380;    /* the image's value */
    g_Ctx.p0AB448      = "AB448";
    g_Ctx.p0AB458      = "AB458";
    g_Ctx.p0AB508      = "AB508";
    g_Ctx.p0AD274      = "AD274";
    g_Ctx.p0AD300      = "AD300";
    g_Ctx.p0AB4D8      = "AB4D8";

    memset(&g_Phase, 0, sizeof(g_Phase));

    g_cPlace = 0; g_cText = 0; g_cRow = 0; g_cListCfg = 0;
    g_cAlloc = 0; g_iAllocFail = -1;
    g_cErr = 0; g_idxErr = -1; g_cStrGet = 0; g_kindSeen = -1;
}

static BrUiScreen *TheScreen(void)
{
    return g_Phase.apScreen[0];
}

/* ==========================================================================
 * Shared invariants
 * ========================================================================== */

/* a4 == 2 and a5 == 5 at every single placement, in all five functions. */
static void CheckPlacementConstants(void)
{
    int i;
    for (i = 0; i < g_cPlace; ++i) {
        CHECK(g_aPlace[i].a4 == 2);
        CHECK(g_aPlace[i].a5 == 5);
        CHECK(g_aPlace[i].pOwner == &g_Phase);   /* the phase, not the screen */
    }
}

/* Every control the screen kept must have been placed. */
static void CheckAllRegisteredWerePlaced(void)
{
    BrUiScreen *pScr = TheScreen();
    int i, j;

    for (i = 0; i < (int)pScr->cCtl; ++i) {
        int fFound = 0;
        for (j = 0; j < g_cPlace; ++j) {
            if (g_aPlace[j].pCtl == pScr->apCtl[i]) {
                fFound = 1;
            }
        }
        CHECK(fFound);
    }
}

/* The phase-side bookkeeping, identical in all five. */
static void CheckPhaseBookkeeping(void)
{
    CHECK(g_Phase.cScreen == 1);
    CHECK(g_Phase.f12 == 0);
    CHECK(g_Phase.aF6C[0] == 1);
    CHECK(TheScreen() != NULL);
    CHECK(TheScreen()->pOwner == &g_Phase);
    CHECK(TheScreen()->f10 == 0);
    CHECK(TheScreen()->fX == 195.0f);
}

/* ==========================================================================
 * Tests
 * ========================================================================== */

/* 0x1004BDC0: the row cursor. The five menu rows sit at fY + {0,19,38,57,114}
 * -- if the `fsub` had been read as a subtraction they would run upward
 * instead, so this is the test that pins the x87 sign convention. */
static void TestRowGeometry(void)
{
    BrUiScreen *pScr;
    float       fY;
    int         i;
    static const float aExpect[5] = { 0.0f, 19.0f, 38.0f, 57.0f, 114.0f };

    Reset();
    BrExt_1004BDC0(&g_Ctx, &g_Phase);

    CheckPhaseBookkeeping();
    CheckPlacementConstants();
    CheckAllRegisteredWerePlaced();

    pScr = TheScreen();
    fY   = pScr->fY;
    CHECK(fY == 130.0f);

    /* g_aPlace[0] is the root, [1] the title, [2..6] the menu rows. */
    CHECK(g_cPlace == 10);
    for (i = 0; i < 5; ++i) {
        CHECK(g_aPlace[2 + i].x == pScr->fX);
        CHECK(g_aPlace[2 + i].y == fY + aExpect[i]);
        CHECK(g_aPlace[2 + i].flags == 0x102001);
    }
    /* Exactly the five hooked rows bump cSel; the other five controls do not. */
    CHECK(pScr->cCtl == 10);
    CHECK(pScr->cSel == 5);

    /* The root control is placed at the origin with no text at all. */
    CHECK(g_aPlace[0].x == 0.0f && g_aPlace[0].y == 0.0f);
    CHECK(g_aPlace[0].flags == 9);
    CHECK(g_aPlace[0].a6 == 0 && g_aPlace[0].a7 == 0);

    /* The last row is the one published to 0x10AA29C8. */
    CHECK(g_Ctx.pAA29C8 == g_aPlace[6].pCtl);
}

/* 0x1004C4A0 is the same shape with one row fewer. */
static void TestSiblingScreen(void)
{
    BrUiScreen *pScr;

    Reset();
    BrExt_1004C4A0(&g_Ctx, &g_Phase);
    CheckPhaseBookkeeping();
    CheckPlacementConstants();
    CheckAllRegisteredWerePlaced();

    pScr = TheScreen();
    CHECK(g_cPlace == 9);
    CHECK(pScr->cCtl == 9);
    CHECK(pScr->cSel == 4);
    CHECK(g_aPlace[2].y == pScr->fY);
    CHECK(g_aPlace[3].y == pScr->fY + 19.0f);
    CHECK(g_aPlace[4].y == pScr->fY + 38.0f);
    CHECK(g_aPlace[5].y == pScr->fY + 114.0f);   /* -57 and -76 skipped here */
}

/* 0x1004B430: the abandoned control, the conditional blocks, and the row
 * cursor that is left at 0.0f when 0x100AC304 is clear. */
static void TestAbandonedControl(void)
{
    BrUiScreen *pScr;
    BrUiCtl    *pOrphan;
    int         i, fFound;

    Reset();
    g_Ctx.n0AC304 = 0;
    BrExt_1004B430(&g_Ctx, &g_Phase);
    CheckPhaseBookkeeping();
    CheckPlacementConstants();
    CheckAllRegisteredWerePlaced();

    pScr = TheScreen();

    /* Eleven controls are placed but only ten are counted: one is dropped. */
    CHECK(g_cPlace == 11);
    CHECK(pScr->cCtl == 10);
    CHECK(pScr->cSel == 4);

    /* The dropped one is g_aPlace[5]; it must not survive in apCtl, because
     * the control built after it reused the same slot. */
    pOrphan = g_aPlace[5].pCtl;
    fFound  = 0;
    for (i = 0; i < (int)pScr->cCtl; ++i) {
        if (pScr->apCtl[i] == pOrphan) {
            fFound = 1;
        }
    }
    CHECK(!fFound);
    CHECK(pScr->apCtl[5] == g_aPlace[6].pCtl);

    /* With 0x100AC304 clear the cursor starts at 0, so the first counted row
     * lands on fY itself; the steps are then +19, +57, +19 -- not uniform. */
    CHECK(g_aPlace[2].y == pScr->fY);
    CHECK(g_aPlace[3].y == pScr->fY + 19.0f);
    CHECK(g_aPlace[4].y == pScr->fY + 76.0f);   /* 19 + 57 */
    CHECK(g_aPlace[5].y == pScr->fY + 95.0f);   /* 76 + 19 */
    /* The row after the orphan uses the SCREEN's fY, not the cursor. */
    CHECK(g_aPlace[6].y == pScr->fY + 114.0f);

    /* Now with the flag set: three more controls, one more selectable. */
    Reset();
    g_Ctx.n0AC304 = 1;
    BrExt_1004B430(&g_Ctx, &g_Phase);
    CHECK(g_cPlace == 14);
    CHECK(TheScreen()->cCtl == 13);
    CHECK(TheScreen()->cSel == 5);
    /* The gated first row starts the cursor at 19. */
    CHECK(g_aPlace[2].y == TheScreen()->fY);
    CHECK(g_aPlace[3].y == TheScreen()->fY + 19.0f);
}

/* 0x1004A580: the three rectangle controls. */
static void TestRectControls(void)
{
    BrUiScreen *pScr;
    BrUiCtl    *pA, *pB, *pC;
    int         i;

    /* n0AA010 != 0 skips the first rect control. */
    Reset();
    g_Ctx.n0AA010 = 1;
    BrExt_1004A580(&g_Ctx, &g_Phase);
    CheckPhaseBookkeeping();
    CheckPlacementConstants();
    CheckAllRegisteredWerePlaced();
    pScr = TheScreen();
    CHECK(pScr->fY == 111.0f);
    CHECK(pScr->cCtl == 21);
    CHECK(pScr->cSel == 7);

    /* g_aPlace[9] and [10] are the two rect controls that always run. */
    pB = g_aPlace[9].pCtl;
    pC = g_aPlace[10].pCtl;

    /* The rect is always 127 wide and 33 tall, from the truncated float. */
    CHECK(pB->f50 == (int32_t)g_aPlace[9].x);
    CHECK(pB->f54 == (int32_t)g_aPlace[9].y);
    CHECK(pB->f58 == pB->f50 + 0x7F);
    CHECK(pB->f5C == pB->f54 + 0x21);

    /* GOTCHA: the third rect reuses the second's left and right edges and
     * starts exactly where the second ended. */
    CHECK(pC->f50 == pB->f50);
    CHECK(pC->f58 == pB->f58);
    CHECK(pC->f54 == pB->f5C);
    CHECK(pC->f5C == pC->f54 + 0x21);

    /* The float cursor and the int rect agree: +33 either way. */
    CHECK(g_aPlace[10].y == g_aPlace[9].y + 33.0f);

    /* Both carry the marker values and a cleared +0x2968. */
    CHECK(pB->f2A42 == 0x53);
    CHECK(pC->f2A42 == 0x55);
    CHECK(pB->f2968 == 0 && pC->f2968 == 0);

    /* The hooks the first menu row installs. */
    CHECK(g_aPlace[2].pCtl->pfn0C == Hook1);
    CHECK(g_aPlace[2].pCtl->pfn08 == Hook2);
    /* The row published to 0x10AA29B4 is the one carrying 0x100464E0. */
    CHECK(g_Ctx.pAA29B4 != NULL);
    CHECK(g_Ctx.pAA29B4->pfn08 == Hook3);

    /* Every f2AB6 that was written equals its own screen index plus one. */
    for (i = 0; i < (int)pScr->cCtl; ++i) {
        if (pScr->apCtl[i]->f2AB4 != 0) {
            CHECK(pScr->apCtl[i]->f2AB6 == (uint16_t)(i + 1));
        }
    }

    /* n0AA010 == 0 adds the third rect control ahead of the other two. */
    Reset();
    g_Ctx.n0AA010 = 0;
    BrExt_1004A580(&g_Ctx, &g_Phase);
    CHECK(TheScreen()->cCtl == 22);
    CHECK(TheScreen()->cSel == 7);   /* the rects are not selectable */
    pA = g_aPlace[9].pCtl;
    CHECK(pA->f2A42 == 0x79);
    CHECK(pA->f54 == 380);           /* (float)nAB42C, truncated back */
    CHECK(pA->f50 == 0);             /* (float)nAB428, truncated back */
    CHECK(g_aPlace[10].y == g_aPlace[9].y + 33.0f);
}

/* 0x1004CAC0: the list control's loop. */
static void TestListLoop(void)
{
    int i;

    /* nAA2A0C == 3 flags the first two records only. */
    Reset();
    g_Ctx.nAA2A0C = 3;
    BrExt_1004CAC0(&g_Ctx, &g_Phase);
    CheckPhaseBookkeeping();
    CheckPlacementConstants();
    CheckAllRegisteredWerePlaced();

    CHECK(g_cListCfg == 1);
    CHECK(g_cRow == BR_UI_AB330_COUNT);
    for (i = 0; i < g_cRow; ++i) {
        CHECK(g_aRow[i].a2 == ((i == 0 || i == 1) ? 0x10 : 0));
        CHECK(g_aRow[i].a3 == 1);
        CHECK(g_aRow[i].a5 == 0);
        CHECK(g_aRow[i].pStyle == g_Ctx.p0AB4D8);
        CHECK(g_aRow[i].pText == BrStrGet(g_aTbl[i].idText));
    }
    CHECK(g_Ctx.nAA2840 == 2);
    CHECK(TheScreen()->cCtl == 10);
    CHECK(TheScreen()->cSel == 3);
    CHECK(g_aPlace[2].flags == 0x3001);
    CHECK(g_aPlace[2].pCtl->pfn04 == Hook4);
    CHECK(g_aPlace[2].pCtl->f1E1F4 == 1);
    /* -95 is used here and nowhere else in the packet. */
    CHECK(g_aPlace[3].y == TheScreen()->fY + 95.0f);
    CHECK(g_aPlace[4].y == TheScreen()->fY + 114.0f);
    /* The tail call is fed aAC520[nAA2A0C] and its result is published. */
    CHECK(g_kindSeen == g_aAC520[3]);
    CHECK(g_Ctx.nAA2850 == 0x5A5A);

    /* Records whose id does not resolve contribute no row at all, and the
     * flag never fires when nAA2A0C is not 3. */
    Reset();
    g_Ctx.nAA2A0C   = 1;
    g_aTbl[0].idText = 0;        /* reserved: BrStrGet returns NULL */
    g_aTbl[5].idText = 0x1000;   /* out of range */
    BrExt_1004CAC0(&g_Ctx, &g_Phase);
    CHECK(g_cRow == BR_UI_AB330_COUNT - 2);
    for (i = 0; i < g_cRow; ++i) {
        CHECK(g_aRow[i].a2 == 0);
    }
    CHECK(g_Ctx.nAA2840 == 0);
}

/* Allocation failure: the error is reported with index 4 and nothing is
 * dereferenced. The phase counter is still advanced, as in the original. */
static void TestAllocFailure(void)
{
    Reset();
    g_iAllocFail = 0;            /* the screen itself */
    BrExt_1004BDC0(&g_Ctx, &g_Phase);
    CHECK(g_cErr == 1);
    CHECK(g_idxErr == 4);
    CHECK(g_Phase.cScreen == 1);
    CHECK(g_Phase.apScreen[0] == NULL);
    CHECK(g_cPlace == 0);

    Reset();
    g_iAllocFail = 3;            /* the third control */
    BrExt_1004BDC0(&g_Ctx, &g_Phase);
    CHECK(g_cErr == 1);
    CHECK(g_idxErr == 4);
    CHECK(TheScreen() != NULL);
    /* The failing slot is written with NULL before the error is reported. */
    CHECK(TheScreen()->apCtl[TheScreen()->cCtl] == NULL);
    CHECK(g_cPlace == 2);

    /* Every other function survives the same failure. */
    Reset(); g_iAllocFail = 2; BrExt_1004A580(&g_Ctx, &g_Phase);
    CHECK(g_cErr == 1);
    Reset(); g_iAllocFail = 2; BrExt_1004B430(&g_Ctx, &g_Phase);
    CHECK(g_cErr == 1);
    Reset(); g_iAllocFail = 2; BrExt_1004C4A0(&g_Ctx, &g_Phase);
    CHECK(g_cErr == 1);
    Reset(); g_iAllocFail = 2; BrExt_1004CAC0(&g_Ctx, &g_Phase);
    CHECK(g_cErr == 1);
}

/* The label call always carries (1, 1) except at the single 0x1004BCC1 site,
 * which uses (1, 3) together with f1E20C == 5. */
static void TestLabelArguments(void)
{
    int i;
    int cOdd = 0;

    Reset();
    g_Ctx.n0AC304 = 1;
    BrExt_1004B430(&g_Ctx, &g_Phase);
    for (i = 0; i < g_cText; ++i) {
        CHECK(g_aText[i].a2 == 1);
        if (g_aText[i].a3 != 1) {
            CHECK(g_aText[i].a3 == 3);
            CHECK(g_aText[i].pStyle == g_Ctx.p0AB458);
            CHECK(g_aText[i].pCtl->f1E20C == 5);
            cOdd++;
        } else {
            CHECK(g_aText[i].pCtl->f1E20C == 3);
        }
    }
    CHECK(cOdd == 1);

    Reset();
    BrExt_1004BDC0(&g_Ctx, &g_Phase);
    for (i = 0; i < g_cText; ++i) {
        CHECK(g_aText[i].a2 == 1);
        CHECK(g_aText[i].a3 == 1);
        CHECK(g_aText[i].pCtl->f1E20C == 3);
    }
}

int main(void)
{
    TestRowGeometry();
    TestSiblingScreen();
    TestAbandonedControl();
    TestRectControls();
    TestListLoop();
    TestAllocFailure();
    TestLabelArguments();

    if (g_cFail != 0) {
        printf("%d check(s) failed\n", g_cFail);
        return 1;
    }
    printf("test_slice3_33: all checks passed\n");
    return 0;
}
