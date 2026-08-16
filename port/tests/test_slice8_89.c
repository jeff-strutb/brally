/* test_slice8_89.c -- behaviour and invariant tests for slice8_89.c.
 *
 * WHAT IS STOOD IN AND WHY
 *
 * This binary links slice8_89.o ALONE. Every cross-module symbol it needs is
 * defined here, which is the convention build.sh's banner describes ("many
 * tests deliberately define their own stand-ins for dependencies"). Three of
 * them are stand-ins on purpose rather than for convenience:
 *
 *   BrOperatorNew   so an allocation FAILURE can be provoked. The activate's
 *                   three outcomes (already built / just built / could not
 *                   allocate) are otherwise unreachable from a test, and the
 *                   failing one is the path that writes NULL into BOTH globals
 *                   and returns 0 -- which 0x10045050 then DISCARDS.
 *   BrOptObjCtor    with a SENTINEL in f68 rather than the real constructor's
 *                   literal 1, so "did the activate write f68" is observable.
 *                   1 == 1 would make it invisible.
 *   the two enter   so "did the builder run, how many times, and on which
 *   hooks           object" is a counter rather than a guess -- and so a
 *                   builder can move the current phase or fill 0x10AA29B4
 *                   mid-flight, which is what pins the two documented
 *                   read-ordering GOTCHAs.
 *
 * EVERY assertion below is a property of the transcription, not of this
 * harness, and each was checked by inverting the line it covers and watching
 * the suite go red -- see the INVERSION note above each group.
 */
#include "slice8_89.h"

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

BrUiNav *g_pBrUiNav;
int32_t  g_brAA28C8;    /* 0x10AA28C8 -- slice4_50.c owns it in the host */
int32_t  g_brAA28CC;    /* 0x10AA28CC */

static BrUiNav g_nav;

/* --- the allocator, with a failure switch -------------------------------- */
static int g_allocFail;
static int g_nAlloc;

void *BrOperatorNew(uint32_t cb)
{
    g_nAlloc++;
    if (g_allocFail) {
        return NULL;
    }
    /* `operator new` does NOT zero (CONVENTIONS.md). Poison instead, so a body
     * that relies on a field the constructor did not write shows up. */
    {
        void *p = malloc(cb);
        if (p != NULL) {
            memset(p, 0xA5, cb);
        }
        return p;
    }
}

/* --- an ordered call log, so "in this order" can be asserted -------------- */
#define LOGMAX 32
static const char *g_log[LOGMAX];
static int         g_nLog;
static void Log(const char *s) { if (g_nLog < LOGMAX) g_log[g_nLog++] = s; }
static int  LogIs(int i, const char *s)
{
    return i < g_nLog && strcmp(g_log[i], s) == 0;
}

/* --- 0x10048710, the phase constructor ------------------------------------ */
#define BR89_F68_SENTINEL 0x5A5A

static const BrPhaseVtbl_ *g_pPhaseVtbl;

BrPhase_ *BrOptObjCtor(BrPhase_ *pThis)
{
    if (pThis == NULL) {
        return NULL;
    }
    memset(pThis, 0, sizeof(*pThis));
    pThis->pVtbl = g_pPhaseVtbl;
    /* The real 0x10048710 stores a literal 1 in f68. A SENTINEL is used here,
     * and only here, because the thing under test is whether the ACTIVATE
     * writes f68 -- and onto WHICH object. 1 == 1 would hide both. */
    pThis->f68 = BR89_F68_SENTINEL;
    /* +0x04 is deliberately NOT written; br_phase.h records that the real
     * constructor leaves it garbage. The memset above is the port's floor. */
    return pThis;
}

/* --- the two enter hooks -------------------------------------------------- */
static int       g_nEnter59BB0, g_nEnter4A580;
static BrPhase_ *g_pLastEnterArg;
static BrPhase_ *g_pRepointTo;    /* non-NULL: the builder repoints 0x10AA2904 */
static BrUiCtl_ *g_pFillAA29B4;   /* non-NULL: the builder fills 0x10AA29B4    */

void BrExt_10059BB0(BrPhase_ *pSelf)
{
    g_nEnter59BB0++;
    g_pLastEnterArg = pSelf;
    Log("enter59BB0");
    if (g_pRepointTo != NULL) {
        g_pBrUiNav->pAA2904 = g_pRepointTo;
    }
}

void BrPhaseEnterPlaceholder_1004A580(BrPhase_ *pSelf)
{
    g_nEnter4A580++;
    g_pLastEnterArg = pSelf;
    Log("enter4A580");
    if (g_pRepointTo != NULL) {
        g_pBrUiNav->pAA2904 = g_pRepointTo;
    }
    /* 0x10AA29B4 is written by the SCREEN BUILDERS in the original
     * (slice3_33.h lists it among the five builders' writes). Reproducing that
     * here is what makes "0x10045050 reads 0x10AA29B4 AFTER the activate"
     * a testable claim rather than a comment. */
    if (g_pFillAA29B4 != NULL) {
        g_brHook89.pAA29B4 = g_pFillAA29B4;
    }
}

/* ==========================================================================
 * A phase whose vtable slots are observable
 * ========================================================================== */

static int       g_nRelease1C, g_nDelete00;
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
    if (p != NULL) {
        p->pVtbl = &g_phaseVtbl;
    }
    return p;
}

static BrUiCtl_ *NewCtl(BrPhase_ *pOwner)
{
    BrUiCtl_ *p = (BrUiCtl_ *)calloc(1, sizeof(BrUiCtl_));
    if (p != NULL) {
        p->pOwner = pOwner;
    }
    return p;
}

/* ==========================================================================
 * Fixture
 * ========================================================================== */

static void Setup(void)
{
    memset(&g_nav, 0, sizeof(g_nav));

    g_phaseVtbl.f00 = VtDelete00;
    g_phaseVtbl.f1C = VtRelease1C;
    g_pPhaseVtbl    = &g_phaseVtbl;

    g_pBrUiNav      = &g_nav;
    g_allocFail     = 0;
    g_nAlloc        = 0;
    g_nEnter59BB0   = g_nEnter4A580 = 0;
    g_pLastEnterArg = NULL;
    g_pRepointTo    = NULL;
    g_pFillAA29B4   = NULL;
    g_nRelease1C    = g_nDelete00 = 0;
    g_pLastReleased = g_pLastDeleted = NULL;
    g_lastDeleteFlag = 0;
    g_nLog          = 0;

    g_brAA28C8 = 0;
    g_brAA28CC = 0;

    BrUiHook89Reset();
}

/* ==========================================================================
 * 1. 0x10044D00 -- the prologue runs on EVERY path
 *
 * INVERSION: delete either store, or move both inside the just-built branch,
 * and the already-built and alloc-failure cases below go red.
 * ========================================================================== */

static void TestPrologueOnEveryPath(void)
{
    BrPhase_ *pPre;

    /* (a) just-built */
    Setup();
    g_brAA28C8 = 7; g_brAA28CC = 9;
    CHECK(BrUiHook89_10044D00(NULL) == 1, "just-built returns 1");
    CHECK(g_brAA28C8 == 0 && g_brAA28CC == 0, "prologue zeroed both, just-built");

    /* (b) already built */
    g_brAA28C8 = 7; g_brAA28CC = 9;
    pPre = g_brHook89.pAA2964;
    CHECK(pPre != NULL, "the slot is filled after the first call");
    CHECK(BrUiHook89_10044D00(NULL) == 1, "already-built returns 1");
    CHECK(g_brAA28C8 == 0 && g_brAA28CC == 0, "prologue zeroed both, cached");

    /* (c) allocation failure */
    Setup();
    g_allocFail = 1;
    g_brAA28C8 = 7; g_brAA28CC = 9;
    CHECK(BrUiHook89_10044D00(NULL) == 0, "alloc failure returns 0, not 1");
    CHECK(g_brAA28C8 == 0 && g_brAA28CC == 0, "prologue zeroed both, failed");

    free(pPre);
}

/* ==========================================================================
 * 2. 0x10044D00 -- build once, republish thereafter
 *
 * INVERSION: drop the `if (*ppSlot != NULL)` early return and the builder
 * count and the allocation count both go to 2.
 * ========================================================================== */

static void TestActivateBuildsOnce(void)
{
    BrPhase_ *pBuilt;

    Setup();
    CHECK(BrUiHook89_10044D00(NULL) == 1, "first call returns 1");
    pBuilt = g_brHook89.pAA2964;
    CHECK(pBuilt != NULL, "0x10AA2964 filled");
    CHECK(g_nav.pAA2904 == pBuilt, "0x10AA2904 published");
    CHECK(g_nAlloc == 1, "exactly one allocation");
    CHECK(g_nEnter59BB0 == 1, "the 0x10059BB0 builder ran once");
    CHECK(g_pLastEnterArg == pBuilt, "the builder got the slot's object");
    CHECK(pBuilt->pfnEnter == BrExt_10059BB0, "pfnEnter is 0x10059BB0");
    CHECK(pBuilt->f0C == 1, "f0C set on the just-built path");
    CHECK(pBuilt->f68 == 1, "f68 set on the just-built path");

    /* Republish: the current phase is moved away, then the hook must bring it
     * back WITHOUT rebuilding. */
    g_nav.pAA2904 = NULL;
    CHECK(BrUiHook89_10044D00(NULL) == 1, "cached call returns 1");
    CHECK(g_nav.pAA2904 == pBuilt, "cached call republished the same object");
    CHECK(g_nAlloc == 1, "cached call allocated nothing");
    CHECK(g_nEnter59BB0 == 1, "cached call did NOT re-run the builder");

    free(pBuilt);
}

/* ==========================================================================
 * 3. 0x10044D00 -- the allocation-failure path writes NULL into BOTH globals
 *
 * INVERSION: make the failure path leave the CURRENT phase alone and the
 * third check goes red; make it return 1 and the first does.
 *
 * HONESTY, measured rather than assumed: the matching store to the SLOT is
 * NOT observable from any test, and no fixture can make it so. The failure
 * path is only reachable when the slot was already NULL -- the already-built
 * early return guarantees it -- so `*ppSlot = NULL` and "leave the slot alone"
 * are the same program. The store is transcribed because the original has it,
 * and this note is here so nobody reads the check below as evidence for it.
 * ========================================================================== */

static void TestActivateAllocFailure(void)
{
    BrPhase_ *pStale = NewPhase();

    Setup();
    g_nav.pAA2904      = pStale;
    g_allocFail        = 1;

    CHECK(BrUiHook89_10044D00(NULL) == 0, "returns 0, NOT 1");
    CHECK(g_brHook89.pAA2964 == NULL, "the slot was written with NULL");
    CHECK(g_nav.pAA2904 == NULL, "the CURRENT phase was written with NULL too");
    CHECK(g_nEnter59BB0 == 0, "the builder did not run");

    free(pStale);
}

/* ==========================================================================
 * 4. The three re-reads: f0C and f68 land on the CURRENT phase as it stands
 *    AFTER the builder, not on the object just constructed.
 *
 * INVERSION: write `p->f0C = 1; p->f68 = 1;` instead of re-reading and both
 * halves of this test go red at once -- the built object gets the flags and
 * the repoint target does not.
 * ========================================================================== */

static void TestFlagsFollowTheReReadCurrent(void)
{
    BrPhase_ *pOther = NewPhase();
    BrPhase_ *pBuilt;

    Setup();
    g_pRepointTo = pOther;

    CHECK(BrUiHook89_10044D00(NULL) == 1, "still returns 1");
    pBuilt = g_brHook89.pAA2964;
    CHECK(pBuilt != NULL, "the object was still built and stored");

    CHECK(pOther->f0C == 1, "f0C landed on the REPOINTED current phase");
    CHECK(pOther->f68 == 1, "f68 landed on the REPOINTED current phase");

    /* And the object actually constructed keeps the constructor's values --
     * which is why the sentinel exists. */
    CHECK(pBuilt->f0C == 0, "the built object did NOT get f0C");
    CHECK(pBuilt->f68 == BR89_F68_SENTINEL, "the built object did NOT get f68");

    free(pOther);
    free(pBuilt);
}

/* ==========================================================================
 * 5. 0x10044D00 ignores its argument
 *
 * The body's four `[esp+4]` reads are all the SEH link, not the argument.
 * INVERSION: read the argument (e.g. dereference pCtl) and the NULL call
 * faults, which regress.sh reports as a non-zero exit.
 * ========================================================================== */

static void TestArgumentIsIgnored(void)
{
    BrPhase_ *pOwner = NewPhase();
    BrUiCtl_ *pRow   = NewCtl(pOwner);
    BrPhase_ *pWithCtl, *pWithNull;

    CHECK(pOwner != NULL && pRow != NULL, "fixture allocated");

    Setup();
    CHECK(BrUiHook89_10044D00(pRow) == 1, "with a control: 1");
    pWithCtl = g_brHook89.pAA2964;

    Setup();
    CHECK(BrUiHook89_10044D00(NULL) == 1, "with NULL: 1, and no fault");
    pWithNull = g_brHook89.pAA2964;

    CHECK(pWithCtl != NULL && pWithNull != NULL, "both calls built an object");
    CHECK(pRow->pfn08 == NULL, "the control was not touched at all");

    free(pWithCtl); free(pWithNull); free(pRow); free(pOwner);
}

/* ==========================================================================
 * 6. 0x10045050 -- the installer
 *
 * INVERSION: drop the pfn08 store and the wiring check goes red; leave
 * n0AC304 at 0 and its check does; return the activate's result instead of 1
 * and the alloc-failure case below does.
 * ========================================================================== */

static void TestHook10045050(void)
{
    BrPhase_ *pOwner  = NewPhase();
    BrUiCtl_ *pRow    = NewCtl(pOwner);
    BrUiCtl_ *pTarget = NewCtl(pOwner);
    BrPhase_ *pBuilt;

    CHECK(pOwner && pRow && pTarget, "fixture allocated");

    Setup();
    g_nav.n0AA010      = 5;
    g_brHook89.n0AC304 = 3;
    g_brHook89.pAA29B4 = pTarget;

    CHECK(BrUiHook89_10045050(pRow) == 1, "returns 1");
    pBuilt = g_brHook89.pAA2914;
    CHECK(pBuilt != NULL, "0x10AA2914 was built");
    CHECK(g_nav.pAA2904 == pBuilt, "and published as the current phase");
    CHECK(g_nEnter4A580 == 1, "the 0x1004A580 builder ran once");
    CHECK(pBuilt->pfnEnter == BrPhaseEnterPlaceholder_1004A580,
          "pfnEnter is 0x1004A580");
    CHECK(g_brHook89.n0AC304 == 1, "0x100AC304 ends at 1");
    CHECK(pTarget->pfn08 == BrUiHook89_10046CD0,
          "0x10AA29B4's +0x08 wired to 0x10046CD0");
    CHECK(g_nav.n0AA010 == 0, "0x100AA010 cleared");
    CHECK(pRow->pfn08 == NULL, "the hook's OWN control was not rewired");

    free(pBuilt); free(pTarget); free(pRow); free(pOwner);
}

/* ==========================================================================
 * 7. 0x10045050 reads 0x10AA29B4 AFTER the activate, not before
 *
 * The screen builder is what fills 0x10AA29B4 in the original, so a read
 * hoisted above the activate would see NULL and wire nothing.
 *
 * INVERSION: move the `pTarget = g_brHook89.pAA29B4;` line above the activate
 * call and this test goes red while every other test still passes -- which is
 * exactly the discrimination this fixture exists for.
 * ========================================================================== */

static void TestAA29B4ReadAfterActivate(void)
{
    BrPhase_ *pOwner  = NewPhase();
    BrUiCtl_ *pRow    = NewCtl(pOwner);
    BrUiCtl_ *pTarget = NewCtl(pOwner);

    Setup();
    g_brHook89.pAA29B4 = NULL;      /* nothing to wire when the hook starts */
    g_pFillAA29B4      = pTarget;   /* the BUILDER fills it, mid-call       */

    CHECK(BrUiHook89_10045050(pRow) == 1, "returns 1");
    CHECK(g_brHook89.pAA29B4 == pTarget, "the builder filled 0x10AA29B4");
    CHECK(pTarget->pfn08 == BrUiHook89_10046CD0,
          "the hook was wired into the control the BUILDER supplied");

    free(g_brHook89.pAA2914); free(pTarget); free(pRow); free(pOwner);
}

/* ==========================================================================
 * 8. 0x10045050 discards the activate's result, and survives a NULL target
 *
 * INVERSION: `return BrUiHook89Activate_10045110();` and the first check goes
 * red. Remove the NULL guard and the second block faults.
 * ========================================================================== */

static void TestHook10045050SwallowsFailure(void)
{
    BrPhase_ *pOwner = NewPhase();
    BrUiCtl_ *pRow   = NewCtl(pOwner);
    BrUiCtl_ *pTarget = NewCtl(pOwner);

    /* (a) the allocation fails -- the hook still reports success. */
    Setup();
    g_allocFail        = 1;
    g_nav.n0AA010      = 5;
    g_brHook89.pAA29B4 = pTarget;

    CHECK(BrUiHook89_10045050(pRow) == 1,
          "returns 1 even though the activate returned 0");
    CHECK(g_brHook89.pAA2914 == NULL, "the slot really did stay NULL");
    CHECK(g_nEnter4A580 == 0, "the builder never ran");
    CHECK(g_brHook89.n0AC304 == 1, "0x100AC304 still ends at 1");
    CHECK(pTarget->pfn08 == BrUiHook89_10046CD0, "the wiring still happened");
    CHECK(g_nav.n0AA010 == 0, "0x100AA010 still cleared");

    /* (b) 0x10AA29B4 is NULL -- the DEVIATION guard. The original faults. */
    Setup();
    g_brHook89.pAA29B4 = NULL;
    g_nav.n0AA010      = 5;
    CHECK(BrUiHook89_10045050(pRow) == 1, "NULL target: still 1, no fault");
    CHECK(g_brHook89.n0AC304 == 1, "NULL target: 0x100AC304 still 1");
    CHECK(g_nav.n0AA010 == 0, "NULL target: 0x100AA010 still cleared");

    free(g_brHook89.pAA2914); free(pTarget); free(pRow); free(pOwner);
}

/* ==========================================================================
 * 9. 0x10046CD0 -- the leave prologue, in order
 *
 * INVERSION: swap the two calls and the LogIs pair goes red; pass the owner
 * to f00 instead of the current phase and the g_pLastDeleted check does; pass
 * 0 instead of 1 and the flag check does.
 * ========================================================================== */

static void TestLeavePrologueOrder(void)
{
    BrPhase_ *pOwner = NewPhase();
    BrPhase_ *pCur   = NewPhase();
    BrUiCtl_ *pRow   = NewCtl(pOwner);

    CHECK(pOwner && pCur && pRow, "fixture allocated");

    Setup();
    g_nav.pAA2904 = pCur;          /* deliberately NOT the owner */

    CHECK(BrUiHook89_10046CD0(pRow) == 0, "the leave returns 0");
    CHECK(g_nRelease1C == 1 && g_nDelete00 == 1, "one call each");
    CHECK(LogIs(0, "f1C") && LogIs(1, "f00"), "+0x1C runs BEFORE +0x00");
    CHECK(g_pLastReleased == pOwner, "+0x1C got the control's OWNING phase");
    CHECK(g_pLastDeleted == pCur, "+0x00 got the CURRENT phase, not the owner");
    CHECK(g_lastDeleteFlag == 1, "+0x00 got the literal 1");

    free(pOwner); free(pCur); free(pRow);
}

/* ==========================================================================
 * 10. 0x10046CD0 -- destination and clears
 *
 * INVERSION: clear the wrong word (say pAA2930) and the destination check
 * goes red; forget either clear and its check does.
 *
 * HONESTY: the original loads 0x10AA2930 BEFORE clearing 0x10AA2914 and
 * 0x10AA29B4, and this fixture CANNOT tell that order from the opposite one,
 * because the routine never clears the word it reads. The order is preserved
 * in the source for fidelity; only the outcome is asserted here.
 * ========================================================================== */

static void TestLeaveDestination(void)
{
    BrPhase_ *pOwner = NewPhase();
    BrPhase_ *pCur   = NewPhase();
    BrPhase_ *pNext  = NewPhase();
    BrUiCtl_ *pRow   = NewCtl(pOwner);
    BrUiCtl_ *pB4    = NewCtl(pOwner);
    BrPhase_ *pSlot  = NewPhase();

    Setup();
    g_nav.pAA2904      = pCur;
    g_brHook89.pAA2930 = pNext;
    g_brHook89.pAA2914 = pSlot;
    g_brHook89.pAA29B4 = pB4;

    CHECK(BrUiHook89_10046CD0(pRow) == 0, "returns 0");
    CHECK(g_nav.pAA2904 == pNext, "0x10AA2904 <- 0x10AA2930");
    CHECK(g_brHook89.pAA2914 == NULL, "0x10AA2914 cleared");
    CHECK(g_brHook89.pAA29B4 == NULL, "0x10AA29B4 cleared");
    CHECK(g_brHook89.pAA2930 == pNext, "0x10AA2930 itself is NOT cleared");

    /* CONFLICT 3 in the header: nothing in the tree builds 0x10AA2930, so in
     * the host today this repoints the current phase at NULL. That is the
     * ORIGINAL's behaviour for an unbuilt slot; pinned here so a future pass
     * that wires a writer sees this line and updates the header note. */
    Setup();
    g_nav.pAA2904      = pCur;
    g_brHook89.pAA2930 = NULL;
    CHECK(BrUiHook89_10046CD0(pRow) == 0, "returns 0 with an unbuilt slot");
    CHECK(g_nav.pAA2904 == NULL, "an unbuilt 0x10AA2930 makes 0x10AA2904 NULL");

    free(pOwner); free(pCur); free(pNext); free(pRow); free(pB4); free(pSlot);
}

/* ==========================================================================
 * 11. 0x10046CD0 -- the current phase's NULL test is the original's own
 *
 * INVERSION: drop the `pCur != NULL` test and this faults.
 * ========================================================================== */

static void TestLeaveWithNoCurrentPhase(void)
{
    BrPhase_ *pOwner = NewPhase();
    BrUiCtl_ *pRow   = NewCtl(pOwner);

    Setup();
    g_nav.pAA2904 = NULL;

    CHECK(BrUiHook89_10046CD0(pRow) == 0, "returns 0");
    CHECK(g_nRelease1C == 1, "+0x1C still ran");
    CHECK(g_nDelete00 == 0, "+0x00 was skipped");

    free(pOwner); free(pRow);
}

/* ==========================================================================
 * 12. Round trip: 0x10045050 opens a screen and wires its way back out, and
 *     the wire is live.
 *
 * This is the property the whole packet exists for -- a menu that can leave a
 * screen. It calls the installed pointer rather than the function by name.
 * ========================================================================== */

static void TestRoundTrip(void)
{
    BrPhase_ *pOwner = NewPhase();
    BrPhase_ *pHome  = NewPhase();
    BrUiCtl_ *pRow   = NewCtl(pOwner);
    BrUiCtl_ *pBack  = NewCtl(pOwner);
    BrPhase_ *pBuilt;

    Setup();
    g_brHook89.pAA29B4 = pBack;
    g_brHook89.pAA2930 = pHome;      /* where "Back" goes */

    /* Forward. */
    CHECK(BrUiHook89_10045050(pRow) == 1, "forward returns 1");
    pBuilt = g_brHook89.pAA2914;
    CHECK(pBuilt != NULL && g_nav.pAA2904 == pBuilt, "the new screen is current");
    CHECK(pBack->pfn08 != NULL, "the back row got a hook");

    /* Back, through the pointer the forward hook installed. */
    CHECK(pBack->pfn08(pBack) == 0, "the back hook returns 0");
    CHECK(g_nav.pAA2904 == pHome, "0x10AA2904 is back at 0x10AA2930");
    CHECK(g_brHook89.pAA2914 == NULL, "the screen's slot was dropped");
    CHECK(g_brHook89.pAA29B4 == NULL, "and so was the back row");

    /* And because the slot was dropped, going forward again REBUILDS. */
    g_brHook89.pAA29B4 = pBack;
    CHECK(BrUiHook89_10045050(pRow) == 1, "forward again returns 1");
    CHECK(g_nEnter4A580 == 2, "the builder ran a second time");

    free(pBuilt); free(g_brHook89.pAA2914);
    free(pOwner); free(pHome); free(pRow); free(pBack);
}

/* ==========================================================================
 * 13. Reset really does put every word back
 * ========================================================================== */

static void TestReset(void)
{
    BrPhase_ *pA = NewPhase();
    BrUiCtl_ *pC = NewCtl(NULL);

    Setup();
    g_brHook89.pAA2914 = pA;
    g_brHook89.pAA2930 = pA;
    g_brHook89.pAA2964 = pA;
    g_brHook89.pAA29B4 = pC;
    g_brHook89.n0AC304 = 42;

    BrUiHook89Reset();
    CHECK(g_brHook89.pAA2914 == NULL && g_brHook89.pAA2930 == NULL &&
          g_brHook89.pAA2964 == NULL && g_brHook89.pAA29B4 == NULL &&
          g_brHook89.n0AC304 == 0, "reset cleared every owned word");

    free(pA); free(pC);
}

/* ==========================================================================
 * 14. Install fills exactly two slots and leaves every other one a hole
 *
 * INVERSION: add any third assignment to BrUiHook89Install72 and the count
 * check goes red -- which is the point: a slot this module has not ported
 * must stay VISIBLY empty rather than become a silent no-op.
 * ========================================================================== */

static void TestInstall(void)
{
    BrUi72Hooks h;
    const BrUiCtlHookFn_ *p;
    size_t i, n, nSet = 0;

    memset(&h, 0, sizeof(h));
    BrUiHook89Install72(&h);

    CHECK(h.p10044D00 == BrUiHook89_10044D00, "0x10044D00 wired");
    CHECK(h.p10045050 == BrUiHook89_10045050, "0x10045050 wired");

    /* The slots this module must NOT claim: three are slice8_84.h's remaining
     * NOT DONE (D) holes, four are slice7_80.c's, and the rest belong to
     * BrUiHook84Install72. */
    CHECK(h.p10046260 == NULL, "0x10046260 still a hole");
    CHECK(h.p10047250 == NULL, "0x10047250 still a hole");
    CHECK(h.p100474B0 == NULL, "0x100474B0 still a hole");
    CHECK(h.p10043590 == NULL, "slice7_80.c's slot untouched");
    CHECK(h.p10044C70 == NULL, "slice8_84.c's slot untouched");
    CHECK(h.p10047360 == NULL, "br_sprfont.c's slot untouched");

    /* BrUi72Hooks is all function pointers of one width, so the table can be
     * counted directly rather than listed. */
    p = (const BrUiCtlHookFn_ *)(const void *)&h;
    n = sizeof(h) / sizeof(BrUiCtlHookFn_);
    for (i = 0; i < n; i++) {
        if (p[i] != NULL) {
            nSet++;
        }
    }
    CHECK(nSet == 2, "exactly two slots filled, the rest left NULL");

    /* ...and installing into NULL must not fault. */
    BrUiHook89Install72(NULL);
}

int main(void)
{
    TestPrologueOnEveryPath();
    TestActivateBuildsOnce();
    TestActivateAllocFailure();
    TestFlagsFollowTheReReadCurrent();
    TestArgumentIsIgnored();
    TestHook10045050();
    TestAA29B4ReadAfterActivate();
    TestHook10045050SwallowsFailure();
    TestLeavePrologueOrder();
    TestLeaveDestination();
    TestLeaveWithNoCurrentPhase();
    TestRoundTrip();
    TestReset();
    TestInstall();

    if (g_fail != 0) {
        printf("test_slice8_89: %d failures\n", g_fail);
        return 1;
    }
    printf("test_slice8_89: all tests passed\n");
    return 0;
}
