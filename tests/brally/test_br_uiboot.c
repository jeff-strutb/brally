/* test_br_uiboot.c -- Glide 0x10056260, the pre-loop gate.
 *
 * WHAT IS ASSERTED, AND WHY EACH IS A PROPERTY OF THE CODE
 *
 *  - THE GATE'S ONLY ZERO.  RallyMain exits without running a frame when this
 *    returns 0, so "what makes it zero" is the single most load-bearing fact
 *    in the function.  The test drives every failure the function can see --
 *    a failed 0xC8 allocation, a failed 0x400 allocation, 145 failed path
 *    allocations -- and asserts that ONLY the first answers zero.  A
 *    transcription that let any of the others propagate would boot a game
 *    that the original boots, or refuse one it does not.
 *
 *  - THE FAILURE PATH STILL PUBLISHES.  Both phase globals are written before
 *    the branch, with NULL.  A port that returned early without publishing
 *    would leave whatever was there from a previous run, which on the second
 *    call (state 4's mode change) is a dangling pointer to the phase the
 *    original just replaced.
 *
 *  - THE CALL ORDER.  Seven concerns whose order is observable only through
 *    side effects: the table clear must precede the path fill (or the paths
 *    are wiped), and 0x10058540 must run after everything (it is the last
 *    instruction before `mov eax,1`).  A trace log pins the whole sequence.
 *
 *  - THE OVERLAP IN CONCERN D.  The -1 fill and the strided zero write into
 *    the same 720 bytes.  Asserted as { -1, -1, 0 } x 60 with the last dword
 *    of the fill being one the strided pass reaches -- which is the check
 *    that catches both "0xB4 read as bytes" and "the loop bound read as
 *    0x10AC4AD0".
 *
 *  - THE SINGLETON IS BUILT ONCE AND THE PHASE IS BUILT EVERY TIME.  That
 *    asymmetry is what the `if ([0x10AC5C58] == 0)` guard means, and it is
 *    only observable across two calls.
 *
 *  - THE HOOK GOES IN.  br_phase.h records that neither operator new nor the
 *    constructor initialises +0x04, so the test pre-poisons the slot with a
 *    different function and asserts the gate overwrites it.
 *
 * BOUNDING: every loop in this file is bounded by BR_UIIMG_COUNT or
 * BR_UIBOOT_REC_TOTAL, both compile-time constants.
 */
#include "br_uiboot.h"
#include "br_uiimg.h"
#include "slice1_06.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fails;

#define CHECK(cond, what)                                                  \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("  [FAIL] %s  (%s:%d)\n", (what), __FILE__, __LINE__);  \
            ++g_fails;                                                     \
        }                                                                  \
    } while (0)

/* ==========================================================================
 * The harness: a trace log plus a controllable allocator.
 * ========================================================================== */

#define TRACE_MAX 8

static struct {
    const char *asz[TRACE_MAX];
    int         n;

    int         nAlloc;
    long        cbLast;
    int         fFailC8;
    int         fFail400;
    int         fFailPaths;

    BrPhase_   *pPublishedPhase;
    int         nPublishPhase;
    void       *pObj400;
    int         nPublishObj400;
    int         nErrShow;
    int32_t     idxErr;

    BrPhase_   *pLastCtorArg;
    void       *pLastBigAlloc;   /* the last non-path, non-0x400 block */
    uint32_t    cbLastBigAlloc;
    int         nCtor;

    void       *apOwned[BR_UIIMG_COUNT + 8];
    int         nOwned;
} g_h;

static void Trace(const char *psz)
{
    if (g_h.n < TRACE_MAX) {
        g_h.asz[g_h.n] = psz;
    }
    ++g_h.n;
}

static void Own(void *p)
{
    if (p != NULL && g_h.nOwned < (int)(sizeof g_h.apOwned / sizeof g_h.apOwned[0])) {
        g_h.apOwned[g_h.nOwned++] = p;
    }
}

static void ReleaseOwned(void)
{
    int i;

    for (i = 0; i < g_h.nOwned; ++i) {
        free(g_h.apOwned[i]);
    }
    g_h.nOwned = 0;
}

static void *HAlloc(void *pUser, uint32_t cb)
{
    void *p;

    (void)pUser;
    ++g_h.nAlloc;
    g_h.cbLast = (long)cb;

    if (cb == BR_UIBOOT_OBJ400_SIZE && g_h.fFail400) {
        return NULL;
    }
    if (cb == BR_UIIMG_PATH_MAX && g_h.fFailPaths) {
        return NULL;
    }
    if (cb != BR_UIBOOT_OBJ400_SIZE && cb != BR_UIIMG_PATH_MAX &&
        g_h.fFailC8) {
        return NULL;                 /* the 0xC8 (== BR_PHASE_ALLOC_SIZE) */
    }

    p = malloc(cb);
    if (p != NULL) {
        memset(p, 0xA5, cb);         /* operator new does not zero */
        Own(p);
    }
    if (cb != BR_UIBOOT_OBJ400_SIZE && cb != BR_UIIMG_PATH_MAX) {
        g_h.pLastBigAlloc  = p;      /* the 0xC8 phase block */
        g_h.cbLastBigAlloc = cb;
    }
    return p;
}

static void HFree(void *pUser, void *p)
{
    (void)pUser;
    (void)p;
    /* Deliberately does nothing: ReleaseOwned() cleans up, so the test can
     * still inspect a "freed" phase.  Nothing in the gate frees anyway. */
}

/* Stands in for 0x10041B60 / BrOptObjCtor.  Zeroes the object the way the
 * real constructor does for the fields it touches -- but NOT +0x04, which is
 * the whole point of the hook assertion below. */
static BrPhase_ *HPhaseCtor(void *pUser, void *pRaw)
{
    BrPhase_ *p = (BrPhase_ *)pRaw;

    (void)pUser;
    Trace("phasector");
    ++g_h.nCtor;
    p->pVtbl   = NULL;
    p->f0C     = 1;
    p->nPages  = 0;
    p->iPage   = 0;
    p->f68     = 1;
    g_h.pLastCtorArg = p;
    return p;
}

static void HPublishPhase(void *pUser, BrPhase_ *pPhase)
{
    (void)pUser;
    Trace("publishphase");
    ++g_h.nPublishPhase;
    g_h.pPublishedPhase = pPhase;
}

static void *HGetObj400(void *pUser)      { (void)pUser; return g_h.pObj400; }

static void HPublishObj400(void *pUser, void *p)
{
    (void)pUser;
    Trace("publish400");
    ++g_h.nPublishObj400;
    g_h.pObj400 = p;
}

static void HErrShow(void *pUser, int32_t idx)
{
    (void)pUser;
    Trace("errshow");
    ++g_h.nErrShow;
    g_h.idxErr = idx;
}

static void HTables64(void *pUser)  { (void)pUser; Trace("tables64"); }
static void HRects(void *pUser)     { (void)pUser; Trace("rects"); }

static void HPhaseEnter(BrPhase_ *pSelf) { (void)pSelf; }
static void HOtherEnter(BrPhase_ *pSelf) { (void)pSelf; }

static char g_szSeason[BR_UIIMG_PATH_MAX];
static char g_szGhost[BR_UIIMG_PATH_MAX];

static void Ops(BrUiBootOps *pOps)
{
    memset(pOps, 0, sizeof *pOps);
    pOps->pfnAlloc          = HAlloc;
    pOps->pfnFree           = HFree;
    pOps->pfnPhaseCtor      = HPhaseCtor;
    pOps->pfnPublishPhase   = HPublishPhase;
    pOps->pfnGetObj400      = HGetObj400;
    pOps->pfnPublishObj400  = HPublishObj400;
    pOps->pfnErrShow        = HErrShow;
    pOps->pfnTables64Clear  = HTables64;
    pOps->pfnRectTablesInit = HRects;
    pOps->pfnPhaseEnter     = HPhaseEnter;
    pOps->pszSeasonBuf      = g_szSeason;
    pOps->cbSeasonBuf       = sizeof g_szSeason;
    pOps->pszGhostBuf       = g_szGhost;
    pOps->cbGhostBuf        = sizeof g_szGhost;
    pOps->pUser             = NULL;
}

static void ResetAll(void)
{
    ReleaseOwned();
    memset(&g_h, 0, sizeof g_h);
    memset(g_szSeason, 0, sizeof g_szSeason);
    memset(g_szGhost, 0, sizeof g_szGhost);
    BrUiImgTableClear();
    BrUiBootResetForTest();
}

static int TraceIs(int i, const char *psz)
{
    return (i < g_h.n && i < TRACE_MAX && strcmp(g_h.asz[i], psz) == 0);
}

/* ==========================================================================
 * Concern D -- the overlap
 * ========================================================================== */

static void test_scratch(void)
{
    int i;
    int nRec = 0;

    printf("concern D -- the -1 fill and the strided zero that overlaps it\n");

    for (i = 0; i < BR_UIBOOT_REC_TOTAL; ++i) {
        g_aBrUiBootRec[i] = 0x5A5A5A5A;
    }
    for (i = 0; i < BR_UIBOOT_ZERO_DWORDS; ++i) {
        g_aBrUiBootZero[i] = 0x5A5A5A5A;
    }

    BrUiBootScratchInit();

    for (i = 0; i < BR_UIBOOT_REC_COUNT; ++i) {
        const int32_t *p = &g_aBrUiBootRec[i * BR_UIBOOT_REC_DWORDS];

        if (p[0] == -1 && p[1] == -1 && p[2] == 0) {
            ++nRec;
        }
    }
    CHECK(nRec == BR_UIBOOT_REC_COUNT,
          "all 60 twelve-byte records read { -1, -1, 0 }");

    /* The two ends, stated separately, because they are what a mis-read
     * count moves.  Reading 0xB4 as a BYTE count leaves index 45 upwards
     * untouched; reading the loop bound as 0x10AC4AD0 stops the strided pass
     * one record early. */
    CHECK(g_aBrUiBootRec[0] == -1 && g_aBrUiBootRec[1] == -1,
          "the fill reaches the first record");
    CHECK(g_aBrUiBootRec[BR_UIBOOT_REC_TOTAL - 2] == -1,
          "the fill reaches the 180th dword");
    CHECK(g_aBrUiBootRec[BR_UIBOOT_REC_TOTAL - 1] == 0,
          "the strided zero's LAST store is the fill's LAST dword");

    for (i = 0; i < BR_UIBOOT_ZERO_DWORDS; ++i) {
        if (g_aBrUiBootZero[i] != 0) {
            CHECK(0, "all 100 dwords of the second block are zero");
            break;
        }
    }
}

/* ==========================================================================
 * The happy path
 * ========================================================================== */

static void test_gate_success(void)
{
    BrUiBootOps ops;
    int i;
    int nPaths = 0;

    printf("the gate, everything succeeding\n");

    ResetAll();
    Ops(&ops);

    CHECK(BrUiBootPreLoopGate(&ops) == 1, "the gate returns 1");

    /* The order, end to end.  Only these five are observable through ops. */
    CHECK(TraceIs(0, "tables64"),
          "0x10058FA0 runs after the table clear and before the paths");
    CHECK(TraceIs(1, "phasector"), "the 0xC8 constructor is next");
    CHECK(TraceIs(2, "publishphase"), "then both phase globals are published");
    CHECK(TraceIs(3, "publish400"), "then the 0x400 singleton");
    CHECK(TraceIs(4, "rects"), "0x10058540 is LAST, just before `mov eax,1`");
    CHECK(g_h.n == 5, "nothing else is called");

    CHECK(g_h.nErrShow == 0, "no error is reported");
    CHECK(g_h.pPublishedPhase != NULL, "a phase object was published");
    /* The constructor is handed the block that was just allocated, and the
     * pointer that reaches the two globals is the constructor's return.
     * 0x10041B60 returns `this`, so the two are the same value -- which is
     * why the assertion is that the CONSTRUCTOR SAW THE ALLOCATION, not that
     * the two pointers differ.  A first draft asserted `published ==
     * ctorArg`, which is true under every transcription and could not fail. */
    CHECK(g_h.pLastCtorArg == (BrPhase_ *)g_h.pLastBigAlloc,
          "the constructor is called on the 0xC8 block, not on something else");
    CHECK(g_h.pPublishedPhase == (BrPhase_ *)g_h.pLastBigAlloc,
          "and that same object reaches both phase globals");

    /* CONVENTIONS.md: "0xC8 under-allocates the phase object by 104 bytes on
     * a 64-bit host".  The gate must ask for BR_PHASE_ALLOC_SIZE, never the
     * literal, and on this host the two differ -- so this assertion has real
     * content here and would be vacuous on a 32-bit one. */
    CHECK(g_h.cbLastBigAlloc >= (uint32_t)sizeof(BrPhase_),
          "the phase allocation is sizeof(BrPhase_), not the 0xC8 literal");
    CHECK(g_h.pObj400 != NULL, "the 0x400 singleton exists");

    /* THE HOOK.  The constructor above deliberately leaves +0x04 alone.
     * Guarded, so that a transcription which never publishes reports THIS
     * assertion rather than dying in the test harness -- a suite that
     * segfaults still fails, but it does not say what broke. */
    CHECK(g_h.pPublishedPhase != NULL &&
          g_h.pPublishedPhase->pfnEnter == HPhaseEnter,
          "0x100425E0 is stored into phase +0x04");

    /* Concern A + C really ran: the table clear and the 145 paths. */
    for (i = 0; i < BR_UIIMG_COUNT; ++i) {
        if (g_aBrUiImg[i].pszPath != NULL &&
            strcmp(g_aBrUiImg[i].pszPath, g_apszBrUiAssets[i]) == 0) {
            ++nPaths;
        }
    }
    CHECK(nPaths == BR_UIIMG_COUNT, "all 145 image paths are in the table");

    /* Concern D ran. */
    CHECK(g_aBrUiBootRec[0] == -1 && g_aBrUiBootRec[2] == 0,
          "the scratch blocks were filled");

    /* Concern G ran. */
    CHECK(strcmp(g_szSeason, "c:\\RallySeason.dat") == 0,
          "the season buffer was seeded");
    CHECK(strcmp(g_szGhost, "c:\\RallyGhost.dat") == 0,
          "the ghost buffer was seeded");

    /* 145 paths + 0xC8 + 0x400. */
    CHECK(g_h.nAlloc == BR_UIIMG_COUNT + 2,
          "147 allocations: 145 paths, the phase, the singleton");
}

/* ==========================================================================
 * THE ZERO
 * ========================================================================== */

static void test_gate_zero(void)
{
    BrUiBootOps ops;

    printf("the ONLY zero -- the 0xC8 allocation\n");

    ResetAll();
    Ops(&ops);
    g_h.fFailC8 = 1;

    CHECK(BrUiBootPreLoopGate(&ops) == 0,
          "a failed 0xC8 allocation makes the gate answer ZERO");
    CHECK(g_h.nCtor == 0, "the constructor is NOT called on a NULL block");
    CHECK(g_h.nPublishPhase == 1,
          "both phase globals are STILL published -- with NULL");
    CHECK(g_h.pPublishedPhase == NULL, "...and the value published is NULL");
    CHECK(g_h.nPublishObj400 == 0, "the singleton is never reached");
    CHECK(g_h.nErrShow == 0, "no error box on this path");
    CHECK(TraceIs(g_h.n - 1, "publishphase"),
          "0x10058540 does NOT run: the function returned before it");
}

static void test_obj400_failure_is_not_a_zero(void)
{
    BrUiBootOps ops;

    printf("a failed 0x400 allocation reports error 1 and returns ONE\n");

    ResetAll();
    Ops(&ops);
    g_h.fFail400 = 1;

    CHECK(BrUiBootPreLoopGate(&ops) == 1,
          "the singleton's failure does NOT gate the boot");
    CHECK(g_h.nErrShow == 1, "0x100378C0 is called exactly once");
    CHECK(g_h.idxErr == BR_UIBOOT_ERR_OBJ400,
          "with error index 1");
    CHECK(g_h.pObj400 == NULL, "NULL is published into 0x10AC5C58");
    CHECK(TraceIs(g_h.n - 1, "rects"),
          "and the function still runs 0x10058540 and returns 1");
    CHECK(strcmp(g_szGhost, "c:\\RallyGhost.dat") == 0,
          "...and still seeds the save paths");
}

static void test_path_failure_is_not_a_zero(void)
{
    BrUiBootOps ops;
    int i;
    int nNull = 0;

    printf("145 failed path allocations still return ONE\n");

    ResetAll();
    Ops(&ops);
    g_h.fFailPaths = 1;

    CHECK(BrUiBootPreLoopGate(&ops) == 1,
          "the original never tests the path allocations, so neither does this");
    for (i = 0; i < BR_UIIMG_COUNT; ++i) {
        if (g_aBrUiImg[i].pszPath == NULL) {
            ++nNull;
        }
    }
    CHECK(nNull == BR_UIIMG_COUNT, "every path slot is NULL");
    CHECK(g_h.pPublishedPhase != NULL, "the phase was still built");
    CHECK(g_h.nErrShow == 0, "no error is reported for a path failure");
}

/* ==========================================================================
 * Called twice -- state 4's mode change
 * ========================================================================== */

static void test_second_run(void)
{
    BrUiBootOps ops;
    BrPhase_   *pFirst;
    void       *pObjFirst;
    char       *pszPathFirst;

    printf("the second call -- 0x1001CE88, the mode change\n");

    ResetAll();
    Ops(&ops);

    CHECK(BrUiBootPreLoopGate(&ops) == 1, "first run");
    pFirst       = g_h.pPublishedPhase;
    pObjFirst    = g_h.pObj400;
    pszPathFirst = g_aBrUiImg[0].pszPath;

    g_h.n = 0;
    CHECK(BrUiBootPreLoopGate(&ops) == 1, "second run");

    CHECK(g_h.pPublishedPhase != pFirst,
          "the phase is rebuilt: a SECOND 0xC8 object, the first leaked");
    CHECK(g_h.nCtor == 2, "the constructor ran twice");
    CHECK(g_h.pObj400 == pObjFirst,
          "the 0x400 singleton is NOT rebuilt -- its slot was already set");
    CHECK(g_h.nPublishObj400 == 1,
          "...and 0x10AC5C58 is written only once, ever");
    CHECK(g_aBrUiImg[0].pszPath != pszPathFirst,
          "the 145 paths are reallocated, the first 145 leaked");
    CHECK(g_h.nAlloc == 2 * BR_UIIMG_COUNT + 3,
          "290 paths + 2 phases + 1 singleton");
}

/* ==========================================================================
 * The port-only refusal
 * ========================================================================== */

static void test_incomplete_ops(void)
{
    BrUiBootOps ops;

    printf("an incomplete BrUiBootOps is refused, not half-run\n");

    ResetAll();
    Ops(&ops);
    ops.pfnPhaseCtor = NULL;

    CHECK(BrUiBootOpsComplete(&ops) == 0, "the completeness check says no");
    CHECK(BrUiBootPreLoopGate(&ops) == 0, "and the gate refuses");
    CHECK(g_h.n == 0, "nothing at all was called");
    CHECK(g_aBrUiImg[0].pszPath == NULL, "and no state was touched");

    CHECK(BrUiBootOpsComplete(NULL) == 0, "NULL ops is incomplete");
    CHECK(BrUiBootPreLoopGate(NULL) == 0, "NULL ops is refused");

    Ops(&ops);
    CHECK(BrUiBootOpsComplete(&ops) == 1, "a full ops is complete");

    /* pfnPhaseEnter and pfnFree are NOT required. */
    ops.pfnPhaseEnter = NULL;
    ops.pfnFree = NULL;
    CHECK(BrUiBootOpsComplete(&ops) == 1,
          "the hook value and the host-only free are optional");
    ResetAll();
    Ops(&ops);
    ops.pfnPhaseEnter = NULL;
    CHECK(BrUiBootPreLoopGate(&ops) == 1, "a NULL hook still boots");
    CHECK(g_h.pPublishedPhase != NULL && g_h.pPublishedPhase->pfnEnter == NULL,
          "...and installs a NULL slot, which is what the engine does");
}

static void test_hook_overwrites(void)
{
    BrUiBootOps ops;

    printf("the hook overwrites whatever +0x04 held\n");

    ResetAll();
    Ops(&ops);
    ops.pfnPhaseEnter = HOtherEnter;

    CHECK(BrUiBootPreLoopGate(&ops) == 1, "gate ok");
    CHECK(g_h.pPublishedPhase != NULL &&
          g_h.pPublishedPhase->pfnEnter == HOtherEnter,
          "the stored value is the ops' hook, not a leftover");
}

int main(void)
{
    test_scratch();
    test_gate_success();
    test_gate_zero();
    test_obj400_failure_is_not_a_zero();
    test_path_failure_is_not_a_zero();
    test_second_run();
    test_incomplete_ops();
    test_hook_overwrites();

    ReleaseOwned();

    printf("test_br_uiboot: %d failures\n", g_fails);
    return (g_fails != 0) ? 1 : 0;
}
