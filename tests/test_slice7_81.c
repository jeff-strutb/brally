/* test_slice7_81.c -- behaviour and invariant tests for slice7_81.c.
 *
 * WHAT IS STOOD IN AND WHY
 *
 * This binary links slice7_81.o ALONE. Every cross-module symbol it needs is
 * defined here, which is the convention build.sh's banner describes ("many
 * tests deliberately define their own stand-ins for dependencies"). Two of
 * them are stand-ins on purpose rather than for convenience:
 *
 *   BrOperatorNew   so an allocation FAILURE can be provoked. The original's
 *                   three-outcome return (already built / just built / could
 *                   not allocate) is otherwise unreachable from a test, and
 *                   the failing path is the one that writes NULL into BOTH
 *                   globals and returns 0 rather than 1.
 *   the four enter  so "did the builder run, and how many times" is a counter
 *   hooks           rather than a guess. The already-built path returning 1
 *                   WITHOUT rebuilding is the family's central GOTCHA.
 *
 * The assertions are properties of the code, not of this harness: call
 * ordering, the three documented re-reads, the return-value constants, the
 * read-before-clear / read-after-clear split between the plain and the
 * name-reset leaves, and the "an unwired hook slot stays NULL" invariant.
 */
#include "slice7_81.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail;

#define CHECK(cond, msg)                                                      \
    do {                                                                      \
        if (!(cond)) {                                                        \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, (msg));            \
            g_fail++;                                                         \
        }                                                                     \
    } while (0)

/* ==========================================================================
 * Stand-ins for the storage other modules own
 * ========================================================================== */

BrUi73Ctx      g_br73;
BrS71Globals   g_brS71;
const BrS71Env *g_brS71Env;
char           g_aBr39B720[64];
int32_t        g_br0AB3F4;
BrUiNav       *g_pBrUiNav;

static BrUiNav g_nav;

/* --- the allocator, with a failure switch -------------------------------- */
static int g_allocFail;
static int g_nAlloc;

void *BrOperatorNew(uint32_t cb)
{
    g_nAlloc++;
    if (g_allocFail)
        return NULL;
    /* `operator new` does NOT zero (CONVENTIONS.md). Poison instead, so a
     * body that relies on a field the constructor did not write shows up. */
    {
        void *p = malloc(cb);
        if (p != NULL)
            memset(p, 0xA5, cb);
        return p;
    }
}

/* --- an ordered call log, so "in this order" can be asserted -------------- */
#define LOGMAX 64
static const char *g_log[LOGMAX];
static int         g_nLog;
static void Log(const char *s) { if (g_nLog < LOGMAX) g_log[g_nLog++] = s; }
static void LogReset(void) { g_nLog = 0; }
static int  LogIs(int i, const char *s)
{
    return i < g_nLog && strcmp(g_log[i], s) == 0;
}

/* --- 0x10048710, the phase constructor ----------------------------------- */
#define BR81_F68_SENTINEL 0x5A5A

static const BrPhaseVtbl_ *g_pPhaseVtbl;

BrPhase_ *BrOptObjCtor(BrPhase_ *pThis)
{
    if (pThis == NULL)
        return NULL;
    memset(pThis, 0, sizeof(*pThis));
    pThis->pVtbl      = g_pPhaseVtbl;
    /* The real 0x10048710 stores a literal 1 here. A SENTINEL is used
     * instead, and only here, because the thing under test is whether the
     * ACTIVATE writes f68 -- the second object 0x10045C90 builds is the one
     * that must not be written, and 1 == 1 would make that unobservable. */
    pThis->f68        = BR81_F68_SENTINEL;
    pThis->aFlags[0]  = 1;
    /* +0x04 is deliberately NOT written; br_phase.h records that the real
     * constructor leaves it garbage. memset above is the port's floor. */
    return pThis;
}

/* --- the four enter hooks ------------------------------------------------- */
static int g_nEnter509F0, g_nEnter49F40, g_nEnter50060, g_nEnterBDC0;
static BrPhase_ *g_pRepointTo;          /* non-NULL: 0x100509F0 repoints
                                         * 0x10AA2904 at this, to exercise
                                         * the f0C/f68 re-read GOTCHA */

void BrExt_100509F0(BrPhase_ *pSelf)
{
    (void)pSelf;
    g_nEnter509F0++;
    Log("enter509F0");
    if (g_pRepointTo != NULL)
        g_pBrUiNav->pAA2904 = g_pRepointTo;
}
void BrExt_10049F40(BrPhase_ *pSelf)
{ (void)pSelf; g_nEnter49F40++; Log("enter49F40"); }
void BrExt_10050060(BrPhase_ *pSelf)
{ (void)pSelf; g_nEnter50060++; Log("enter50060"); }
void BrPhaseEnterPlaceholder_1004BDC0(BrPhase_ *pSelf)
{ (void)pSelf; g_nEnterBDC0++; Log("enterBDC0"); }

/* --- the plain callees ---------------------------------------------------- */
static int g_n3E680, g_n3E510, g_n419D0;
static const void *g_p419D0Arg;

void BrExt_1003E680(void) { g_n3E680++; Log("1003E680"); }
void BrSub1003E510(void)  { g_n3E510++; Log("1003E510"); }
void BrExt_100419D0(void *p)
{ g_n419D0++; g_p419D0Arg = p; Log("100419D0"); }

/* ==========================================================================
 * A phase whose vtable slots are observable
 * ========================================================================== */

static int g_nRelease1C, g_nDelete00;
static BrPhase_ *g_pLastReleased, *g_pLastDeleted;
static int32_t   g_lastDeleteFlag;

static void VtRelease1C(BrPhase_ *p)
{ g_nRelease1C++; g_pLastReleased = p; Log("f1C"); }
static void *VtDelete00(BrPhase_ *p, int32_t f)
{ g_nDelete00++; g_pLastDeleted = p; g_lastDeleteFlag = f; Log("f00"); return p; }

static BrPhaseVtbl_ g_phaseVtbl;

static BrPhase_ *NewPhase(void)
{
    BrPhase_ *p = (BrPhase_ *)calloc(1, sizeof(BrPhase_));
    if (p != NULL)
        p->pVtbl = &g_phaseVtbl;
    return p;
}

static BrUiCtl_ *NewCtl(BrPhase_ *pOwner)
{
    BrUiCtl_ *p = (BrUiCtl_ *)calloc(1, sizeof(BrUiCtl_));
    if (p != NULL)
        p->pOwner = pOwner;
    return p;
}

/* ==========================================================================
 * Fixture
 * ========================================================================== */

static char g_scratchA[32], g_scratchB[32];

static void Setup(void)
{
    memset(&g_nav, 0, sizeof(g_nav));
    memset(&g_br73, 0, sizeof(g_br73));
    memset(&g_brS71, 0, sizeof(g_brS71));
    memset(g_scratchA, 0, sizeof(g_scratchA));
    memset(g_scratchB, 0, sizeof(g_scratchB));

    g_phaseVtbl.f00 = VtDelete00;
    g_phaseVtbl.f1C = VtRelease1C;
    g_pPhaseVtbl    = &g_phaseVtbl;

    g_br73.szAA2518  = g_scratchA;
    g_br73.szA9D618  = g_scratchB;
    g_br73.cbScratch = sizeof(g_scratchA);

    g_pBrUiNav  = &g_nav;
    g_allocFail = 0;
    g_nAlloc    = 0;
    g_nEnter509F0 = g_nEnter49F40 = g_nEnter50060 = g_nEnterBDC0 = 0;
    g_n3E680 = g_n3E510 = g_n419D0 = 0;
    g_nRelease1C = g_nDelete00 = 0;
    g_pRepointTo = NULL;
    g_br0AB3F4 = 0;
    LogReset();
    BrUiHook81Reset();
}

/* ==========================================================================
 * 1. ACTIVATE: build once, republish for ever
 * ========================================================================== */

static void TestActivateBuildsOnce(void)
{
    BrPhase_ *pFirst;

    Setup();
    CHECK(BrUiHook81Activate_10045BC0() == 1, "first activate returns 1");
    CHECK(g_nEnter50060 == 1, "builder ran exactly once");
    pFirst = g_nav.pAA2904;
    CHECK(pFirst != NULL, "current phase published");
    CHECK(g_brHook81.pAA2928 == pFirst, "singleton is the same object");
    CHECK(pFirst->f0C == 1 && pFirst->f68 == 1, "both flags set on build");

    /* The GOTCHA: a second activation must NOT rebuild. */
    g_nav.pAA2904 = NULL;
    CHECK(BrUiHook81Activate_10045BC0() == 1, "second activate returns 1");
    CHECK(g_nEnter50060 == 1, "builder did NOT run again");
    CHECK(g_nav.pAA2904 == pFirst, "already-built path republishes the slot");
}

/* ==========================================================================
 * 2. ACTIVATE: allocation failure writes BOTH globals and returns 0
 * ========================================================================== */

static void TestActivateAllocFailure(void)
{
    Setup();
    g_nav.pAA2904   = (BrPhase_ *)0x1;     /* a stale non-NULL */
    g_brHook81.pAA2928 = NULL;
    g_allocFail = 1;

    CHECK(BrUiHook81Activate_10045BC0() == 0, "failure returns 0, not 1");
    CHECK(g_brHook81.pAA2928 == NULL, "slot written even on failure");
    CHECK(g_nav.pAA2904 == NULL, "current written even on failure");
    CHECK(g_nEnter50060 == 0, "builder not reached");
}

/* ==========================================================================
 * 3. 0x10045C90 builds TWO objects, and only the first is published
 * ========================================================================== */

static void TestActivate10045C90(void)
{
    BrPhase_ *pA, *pB;

    Setup();
    CHECK(BrUiHook81Activate_10045C90() == 1, "returns 1");
    pA = g_brHook81.pAA292C;
    pB = g_brHook81.pAA2974;
    CHECK(pA != NULL && pB != NULL, "both objects built");
    CHECK(pA != pB, "and they are two distinct objects");
    CHECK(g_nav.pAA2904 == pA, "only the FIRST is the current phase");
    CHECK(pA->f0C == 1 && pA->f68 == 1, "first gets both flags");
    CHECK(pB->f0C == 1, "second gets f0C");
    CHECK(pB->f68 == BR81_F68_SENTINEL, "second does NOT get f68");
    CHECK(g_nEnter509F0 == 1 && g_nEnter49F40 == 1, "both builders ran once");
    CHECK(LogIs(0, "enter509F0") && LogIs(1, "enter49F40"),
          "first object is built before the second");

    /* Already-built path: neither builder runs again, and the second object
     * is not rebuilt either -- the whole tail is on the just-built path. */
    CHECK(BrUiHook81Activate_10045C90() == 1, "second call returns 1");
    CHECK(g_nEnter509F0 == 1 && g_nEnter49F40 == 1, "no rebuild of either");
    CHECK(g_brHook81.pAA2974 == pB, "second slot untouched");
}

/* ==========================================================================
 * 4. The re-read GOTCHA: f0C/f68 land on whatever the enter hook left in
 *    0x10AA2904, not on the object that was just constructed.
 * ========================================================================== */

static void TestFlagsFollowTheReReadCurrent(void)
{
    BrPhase_ *pHijack;

    Setup();
    pHijack = NewPhase();
    CHECK(pHijack != NULL, "fixture allocated");
    g_pRepointTo = pHijack;

    CHECK(BrUiHook81Activate_10045C90() == 1, "returns 1");
    CHECK(pHijack->f0C == 1 && pHijack->f68 == 1,
          "the flags followed the re-read current phase");
    CHECK(g_brHook81.pAA292C != NULL, "the built object is still in its slot");
    CHECK(g_brHook81.pAA292C->f0C == 0
       && g_brHook81.pAA292C->f68 == BR81_F68_SENTINEL,
          "and did NOT get the flags");
    free(pHijack);
}

/* ==========================================================================
 * 5. 0x10045AA0 -- the "New Season" installer
 * ========================================================================== */

static void TestHook10045AA0(void)
{
    BrPhase_ *pOwner;
    BrUiCtl_ *pRow, *pBack;

    Setup();
    pOwner = NewPhase();
    pRow   = NewCtl(pOwner);
    pBack  = NewCtl(pOwner);
    CHECK(pOwner && pRow && pBack, "fixture allocated");

    g_nav.n0AA010 = 7;
    g_nav.nACED34 = 9;
    g_brS71.pAA29B0 = pBack;

    CHECK(BrUiHook81_10045AA0(pRow) == 1, "returns 1");
    CHECK(g_nav.n0AA010 == 0, "n0AA010 cleared");
    CHECK(g_nav.nACED34 == 0, "nACED34 cleared");
    CHECK(g_n3E680 == 1 && g_n3E510 == 1, "both globals routines run once");
    CHECK(LogIs(0, "1003E680"), "0x1003E680 runs FIRST, before the activate");
    CHECK(g_log[g_nLog - 1] != NULL && LogIs(g_nLog - 1, "1003E510"),
          "0x1003E510 runs LAST, after the activate");
    CHECK(pBack->pfn08 == BrUiHook81_10046D70,
          "the back row of the new screen is wired to 0x10046D70");
    CHECK(g_nav.pAA2904 == g_brHook81.pAA292C,
          "the current phase is the 0x100509F0 object");

    /* DEVIATION under test: the original faults here; the port must not. */
    Setup();
    g_brS71.pAA29B0 = NULL;
    CHECK(BrUiHook81_10045AA0(pRow) == 1, "survives a NULL 0x10AA29B0");

    free(pOwner); free(pRow); free(pBack);
}

/* ==========================================================================
 * 6. The shared LEAVE prologue: owner +0x1C, THEN current +0x00 with 1
 * ========================================================================== */

static void TestLeavePrologueOrder(void)
{
    BrPhase_ *pOwner, *pCur;
    BrUiCtl_ *pRow;

    Setup();
    pOwner = NewPhase();
    pCur   = NewPhase();
    pRow   = NewCtl(pOwner);
    CHECK(pOwner && pCur && pRow, "fixture allocated");

    g_nav.pAA2904 = pCur;
    CHECK(BrUiHook81_100463C0(pRow) == 0, "a LEAVE returns 0");
    CHECK(LogIs(0, "f1C") && LogIs(1, "f00"),
          "release-pages runs before the current phase is notified");
    CHECK(g_pLastReleased == pOwner, "+0x1C is called on the control's OWNER");
    CHECK(g_pLastDeleted == pCur && g_lastDeleteFlag == 1,
          "+0x00 is called on the CURRENT phase, with 1");

    /* The NULL test is on the current phase only, and it is the original's. */
    Setup();
    g_nav.pAA2904 = NULL;
    LogReset();
    CHECK(BrUiHook81_100463C0(pRow) == 0, "returns 0 with no current phase");
    CHECK(g_nRelease1C == 1 && g_nDelete00 == 0,
          "owner still released, current not notified");

    free(pOwner); free(pCur); free(pRow);
}

/* ==========================================================================
 * 7. Destinations, and the read-before-clear order
 * ========================================================================== */

static void TestLeaveDestinations(void)
{
    BrPhase_ *pOwner, *pDest;
    BrUiCtl_ *pRow, *pWired;

    Setup();
    pOwner = NewPhase();
    pDest  = NewPhase();
    pRow   = NewCtl(pOwner);
    pWired = NewCtl(pOwner);
    CHECK(pOwner && pDest && pRow && pWired, "fixture allocated");

    /* 0x10046D70 -> 0x10AA291C, clearing 0x10AA292C / 0x10AA29B0 / 0x10AA2974 */
    g_brHook81.pAA291C = pDest;
    g_brHook81.pAA292C = pOwner;
    g_brHook81.pAA2974 = pOwner;
    g_brS71.pAA29B0    = pWired;
    CHECK(BrUiHook81_10046D70(pRow) == 0, "0x10046D70 returns 0");
    CHECK(g_nav.pAA2904 == pDest, "-> 0x10AA291C");
    CHECK(g_brHook81.pAA292C == NULL && g_brHook81.pAA2974 == NULL,
          "both 0x10045C90 objects dropped");
    CHECK(g_brS71.pAA29B0 == NULL, "the wired control is dropped too");

    /* 0x100470E0 -> 0x10AA2938, clearing 0x10AA293C */
    Setup();
    g_brHook81.pAA2938 = pDest;
    g_brHook81.pAA293C = pOwner;
    CHECK(BrUiHook81_100470E0(pRow) == 0, "0x100470E0 returns 0");
    CHECK(g_nav.pAA2904 == pDest, "-> 0x10AA2938");
    CHECK(g_brHook81.pAA293C == NULL, "0x10AA293C cleared");

    /* 0x10046AD0 -> 0x10AA293C, clearing 0x10AA2918 */
    Setup();
    g_brHook81.pAA293C = pDest;
    g_brHook81.pAA2918 = pOwner;
    CHECK(BrUiHook81_10046AD0(pRow) == 0, "0x10046AD0 returns 0");
    CHECK(g_nav.pAA2904 == pDest, "-> 0x10AA293C");
    CHECK(g_brHook81.pAA2918 == NULL, "0x10AA2918 cleared");

    /* 0x100463C0 -> 0x10AA2958, clearing 0x10AA2940 */
    Setup();
    g_brHook81.pAA2958 = pDest;
    g_brHook81.pAA2940 = pOwner;
    CHECK(BrUiHook81_100463C0(pRow) == 0, "0x100463C0 returns 0");
    CHECK(g_nav.pAA2904 == pDest, "-> 0x10AA2958");
    CHECK(g_brHook81.pAA2940 == NULL, "0x10AA2940 cleared");

    /* 0x10046620 -> 0x10AA2980, clearing 0x10AA2990 and 0x10AA29F0 */
    Setup();
    g_brHook81.pAA2980 = pDest;
    g_brHook81.pAA2990 = pOwner;
    g_br73.pAA29F0     = pWired;
    CHECK(BrUiHook81_10046620(pRow) == 0, "0x10046620 returns 0");
    CHECK(g_nav.pAA2904 == pDest, "-> 0x10AA2980");
    CHECK(g_brHook81.pAA2990 == NULL && g_br73.pAA29F0 == NULL,
          "both 0x10046620 clears");

    free(pOwner); free(pDest); free(pRow); free(pWired);
}

/* ==========================================================================
 * 8. The two name-reset leaves differ ONLY in their destination
 * ========================================================================== */

static void TestNameResetLeaves(void)
{
    BrPhase_ *pOwner, *pTo93C, *pTo934;
    BrUiCtl_ *pRow, *pWired;

    Setup();
    pOwner = NewPhase();
    pTo93C = NewPhase();
    pTo934 = NewPhase();
    pRow   = NewCtl(pOwner);
    pWired = NewCtl(pOwner);
    CHECK(pOwner && pTo93C && pTo934 && pRow && pWired, "fixture allocated");

    strcpy(g_aBr39B720, "Driver");
    strcpy(g_scratchA, "stale-a");
    strcpy(g_scratchB, "stale-b");
    g_brHook81.pAA293C = pTo93C;
    g_brHook81.pAA2928 = pOwner;
    g_brHook81.nAA28E4 = 5;
    g_nav.pAA29C0      = pWired;
    g_br73.pAA29CC     = (unsigned char *)pWired;
    g_br0AB3F4         = 3;

    CHECK(BrUiHook81_10046B10(pRow) == 0, "0x10046B10 returns 0");
    CHECK(g_nav.pAA2904 == pTo93C, "-> 0x10AA293C");
    CHECK(strcmp(g_scratchA, "Driver") == 0, "0x10AA2518 reloaded");
    CHECK(strcmp(g_scratchB, "Driver") == 0, "0x10A9D618 reloaded");
    CHECK(g_br0AB3F4 == -1, "0x100AB3F4 set to -1");
    CHECK(g_brHook81.pAA2928 == NULL && g_brHook81.nAA28E4 == 0,
          "0x10AA2928 and 0x10AA28E4 cleared");
    CHECK(g_nav.pAA29C0 == NULL && g_br73.pAA29CC == NULL,
          "0x10AA29C0 and 0x10AA29CC cleared");

    /* The twin: same reset, different destination. */
    Setup();
    strcpy(g_aBr39B720, "Driver");
    g_brHook81.pAA2934 = pTo934;
    g_brHook81.pAA2928 = pOwner;
    g_br0AB3F4         = 3;
    CHECK(BrUiHook81_10046EB0(pRow) == 0, "0x10046EB0 returns 0");
    CHECK(g_nav.pAA2904 == pTo934, "-> 0x10AA2934, the ONLY difference");
    CHECK(g_br0AB3F4 == -1 && g_brHook81.pAA2928 == NULL,
          "the same reset ran");

    /* DEVIATION under test: the original's copy is an unbounded rep movs. */
    Setup();
    memset(g_aBr39B720, 'x', sizeof(g_aBr39B720) - 1);
    g_aBr39B720[sizeof(g_aBr39B720) - 1] = '\0';
    CHECK(BrUiHook81_10046B10(pRow) == 0, "long name returns 0");
    CHECK(strlen(g_scratchA) == sizeof(g_scratchA) - 1,
          "the copy is bounded by cbScratch and NUL-terminated");

    free(pOwner); free(pTo93C); free(pTo934); free(pRow); free(pWired);
}

/* ==========================================================================
 * 9. 0x100450F0 -- dispatch to ANOTHER control, with THIS one as the argument
 * ========================================================================== */

static BrUiCtl_ *g_pDispatchArg;
static int       g_nDispatch;
static int32_t   DispatchTarget(BrUiCtl_ *p)
{ g_nDispatch++; g_pDispatchArg = p; return 1; }

static void TestDispatch100450F0(void)
{
    BrPhase_ *pOwner;
    BrUiCtl_ *pRow, *pTarget;

    Setup();
    pOwner  = NewPhase();
    pRow    = NewCtl(pOwner);
    pTarget = NewCtl(pOwner);
    CHECK(pOwner && pRow && pTarget, "fixture allocated");

    pTarget->pfn08 = DispatchTarget;
    g_br73.pAA29F4 = pTarget;
    g_nav.n0AA010  = 4;
    g_nDispatch    = 0;

    CHECK(BrUiHook81_100450F0(pRow) == 0,
          "0x100450F0 returns 0, unlike its neighbours");
    CHECK(g_nDispatch == 1, "the other control's +0x08 ran");
    CHECK(g_pDispatchArg == pRow, "...with the CALLER's control as argument");
    CHECK(g_nav.n0AA010 == 0, "n0AA010 cleared afterwards");

    /* DEVIATION under test. */
    Setup();
    g_br73.pAA29F4 = NULL;
    CHECK(BrUiHook81_100450F0(pRow) == 0, "survives a NULL 0x10AA29F4");

    free(pOwner); free(pRow); free(pTarget);
}

/* ==========================================================================
 * 10. Round trip: build a screen, then leave it again
 * ========================================================================== */

static void TestRoundTrip(void)
{
    BrPhase_ *pOwner, *pHome, *pBuilt;
    BrUiCtl_ *pRow, *pBack;

    Setup();
    pOwner = NewPhase();
    pHome  = NewPhase();
    pRow   = NewCtl(pOwner);
    pBack  = NewCtl(pOwner);
    CHECK(pOwner && pHome && pRow && pBack, "fixture allocated");

    g_brHook81.pAA291C = pHome;          /* where 0x10046D70 goes back to */
    g_brS71.pAA29B0    = pBack;
    g_nav.pAA2904      = pHome;

    CHECK(BrUiHook81_10045AA0(pRow) == 1, "forward returns 1");
    pBuilt = g_brHook81.pAA292C;
    CHECK(pBuilt != NULL && g_nav.pAA2904 == pBuilt, "moved to the new phase");
    CHECK(pBack->pfn08 != NULL, "the back row got a hook");

    /* Fire the hook that was installed, from the control it was installed on. */
    CHECK(pBack->pfn08(pBack) == 0, "the installed back hook returns 0");
    CHECK(g_nav.pAA2904 == pHome, "and we are home again");
    CHECK(g_brHook81.pAA292C == NULL && g_brHook81.pAA2974 == NULL,
          "both built objects dropped on the way out");

    free(pOwner); free(pHome); free(pRow); free(pBack);
}

/* ==========================================================================
 * 11. Install touches exactly the slots this module ports
 * ========================================================================== */

static void TestInstall(void)
{
    BrUi73Hooks h;
    const BrUiCtlHookFn_ *p;
    size_t i, n, nSet = 0;

    memset(&h, 0, sizeof(h));
    BrUiHook81Install(&h);

    CHECK(h.p10045AA0 == BrUiHook81_10045AA0, "0x10045AA0 wired");
    CHECK(h.p10045880 == BrUiHook81_10045880, "0x10045880 wired");
    CHECK(h.p100458A0 == BrUiHook81_100458A0, "0x100458A0 wired");
    CHECK(h.p100450F0 == BrUiHook81_100450F0, "0x100450F0 wired");
    CHECK(h.p100463C0 == BrUiHook81_100463C0, "0x100463C0 wired");
    CHECK(h.p10046620 == BrUiHook81_10046620, "0x10046620 wired");
    CHECK(h.p10046EB0 == BrUiHook81_10046EB0, "0x10046EB0 wired");
    CHECK(h.p100470E0 == BrUiHook81_100470E0, "0x100470E0 wired");

    /* The invariant that matters more than the eight above: everything this
     * module does NOT port must still be a visible hole. BrUi73Hooks is all
     * function pointers of one width, so the table can be counted directly. */
    p = (const BrUiCtlHookFn_ *)(const void *)&h;
    n = sizeof(h) / sizeof(BrUiCtlHookFn_);
    for (i = 0; i < n; i++)
        if (p[i] != NULL)
            nSet++;
    CHECK(nSet == 8, "exactly eight slots filled, the rest left NULL");

    /* ...and installing into NULL must not fault. */
    BrUiHook81Install(NULL);
}

/* ==========================================================================
 * 12. Every installer returns 1 and every leave returns 0 (CONFLICT 4)
 * ========================================================================== */

static void TestReturnConstants(void)
{
    BrPhase_ *pOwner;
    BrUiCtl_ *pRow;

    Setup();
    pOwner = NewPhase();
    pRow   = NewCtl(pOwner);
    CHECK(pOwner && pRow, "fixture allocated");

    CHECK(BrUiHook81_10045AA0(pRow) == 1, "0x10045AA0 -> 1");
    Setup();
    CHECK(BrUiHook81_100458A0(pRow) == 1, "0x100458A0 -> 1");
    Setup();
    CHECK(BrUiHook81_10045880(pRow) == 1, "0x10045880 -> 1");
    Setup();
    CHECK(BrUiHook81_100450F0(pRow) == 0, "0x100450F0 -> 0");

    Setup();
    CHECK(BrUiHook81_100463C0(pRow) == 0, "0x100463C0 -> 0");
    Setup();
    CHECK(BrUiHook81_10046620(pRow) == 0, "0x10046620 -> 0");
    Setup();
    CHECK(BrUiHook81_10046AD0(pRow) == 0, "0x10046AD0 -> 0");
    Setup();
    CHECK(BrUiHook81_10046B10(pRow) == 0, "0x10046B10 -> 0");
    Setup();
    CHECK(BrUiHook81_10046D70(pRow) == 0, "0x10046D70 -> 0");
    Setup();
    CHECK(BrUiHook81_10046EB0(pRow) == 0, "0x10046EB0 -> 0");
    Setup();
    CHECK(BrUiHook81_100470E0(pRow) == 0, "0x100470E0 -> 0");

    free(pOwner); free(pRow);
}

/* ==========================================================================
 * 13. 0x100451E0's prologue, and the two sibling installers' wiring
 * ========================================================================== */

static void TestSiblingInstallers(void)
{
    BrPhase_ *pOwner;
    BrUiCtl_ *pRow, *pTargetF4, *pTargetC8;
    static const char szSpace[] = " ";

    Setup();
    pOwner    = NewPhase();
    pRow      = NewCtl(pOwner);
    pTargetF4 = NewCtl(pOwner);
    pTargetC8 = NewCtl(pOwner);
    CHECK(pOwner && pRow && pTargetF4 && pTargetC8, "fixture allocated");

    g_br73.pAA29F4 = pTargetF4;
    CHECK(BrUiHook81_100458A0(pRow) == 1, "0x100458A0 returns 1");
    CHECK(g_nEnter50060 == 1, "it built the 0x10050060 screen");
    CHECK(g_nav.pAA2904 == g_brHook81.pAA2928, "...and published it");
    CHECK(pTargetF4->pfn08 == BrUiHook81_10046B10,
          "0x10AA29F4's +0x08 wired to 0x10046B10");

    Setup();
    g_br73.aStyles.p0AD300 = szSpace;
    g_br73.pAA29C8 = pTargetC8;
    CHECK(BrUiHook81_10045880(pRow) == 1, "0x10045880 returns 1");
    CHECK(g_n419D0 == 1, "0x100451E0's prologue ran 0x100419D0");
    CHECK(g_p419D0Arg == (const void *)szSpace,
          "...with the 0x100AD300 string ADDRESS");
    CHECK(LogIs(0, "100419D0"), "the prologue runs before the build");
    CHECK(g_nEnterBDC0 == 1, "it built the 0x1004BDC0 screen");
    CHECK(pTargetC8->pfn08 == BrUiHook81_10046AD0,
          "0x10AA29C8's +0x08 wired to 0x10046AD0");

    free(pOwner); free(pRow); free(pTargetF4); free(pTargetC8);
}

int main(void)
{
    TestActivateBuildsOnce();
    TestActivateAllocFailure();
    TestActivate10045C90();
    TestFlagsFollowTheReReadCurrent();
    TestHook10045AA0();
    TestLeavePrologueOrder();
    TestLeaveDestinations();
    TestNameResetLeaves();
    TestDispatch100450F0();
    TestRoundTrip();
    TestInstall();
    TestReturnConstants();
    TestSiblingInstallers();

    if (g_fail != 0) {
        printf("test_slice7_81: %d failure(s)\n", g_fail);
        return 1;
    }
    printf("test_slice7_81: all tests passed\n");
    return 0;
}
