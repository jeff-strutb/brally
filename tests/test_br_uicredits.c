/* test_br_uicredits.c -- properties of Glide 0x1003AED0, the main menu's
 * "Credits" row action.
 *
 * WHAT IS ASSERTED, AND WHY EACH ONE CAN FAIL
 *
 * Every assertion below was written from the disassembly (reproduced
 * address-by-address in br_uicredits.h) and every one was mutation-tested:
 * the bug it guards was reinstated in br_uicredits.c, the tree rebuilt, the
 * failure observed, and the source restored.  The table is in the pass
 * report.  Nothing here was written from a banner.
 *
 * The two stand-ins are faithful exactly where faithfulness is what lets an
 * assertion fail:
 *
 *   - BrExt_100419D0 (the status setter, 0x1003AF30) SNAPSHOTS the game mode
 *     when it is called.  Without that, "the status line is blanked before
 *     the mode is set" is unobservable and a body that called it last would
 *     pass.
 *   - the phase vtable's +0x18 (shutdown) snapshots f68, the mode and the
 *     selector.  Without that, "the teardown happens after every global is
 *     written" is unobservable, and so is "f68 is cleared BEFORE the call"
 *     rather than by the callee.
 *
 * WHAT IS DELIBERATELY *NOT* ASSERTED, so nobody adds a decoration:
 *
 *   The original reads pCtl->pOwner TWICE (0x1003AF13 and 0x1003AF1C).
 *   Nothing between the two reads can change the field, so the re-read is
 *   unobservable from here and unobservable at all.  An assertion about it
 *   could not fail, and CONVENTIONS.md says that is worse than no assertion.
 *
 *   Likewise the `push edx` at 0x1003AF12 happening before the +0x68 store:
 *   edx is a register, the push is the argument marshal, and no ordering
 *   between them is observable in C.
 *
 * ONE ASSERTION WAS WRITTEN AND THEN DELETED, and it is named here rather
 * than left as decoration: "the hook does not allocate".  There is no
 * allocator on this module's link line at all, so the check could not fail
 * under any mutation of the body.  The fact it was meant to record -- that
 * this row, alone among the seven, publishes no phase -- is in the header
 * where it belongs.
 */

#include <stdio.h>
#include <string.h>

#include "br_uicredits.h"

static int g_cFail;

#define CHECK(cond, msg)                                                \
    do {                                                                \
        if (!(cond)) {                                                  \
            printf("FAIL %s:%d  %s  (%s)\n",                            \
                   __FILE__, __LINE__, msg, #cond);                     \
            ++g_cFail;                                                  \
        }                                                               \
    } while (0)

/* ==========================================================================
 * The four globals, as host storage owned by THIS TEST.
 *
 * The module owns none of them -- see br_uicredits.h's storage table -- so a
 * test binds its own and the host binds the real models.
 * ========================================================================== */

static int32_t g_nGameMode;
static int32_t g_nMovieSel;
static int32_t g_nOutroFlag;
static int32_t g_nAA289C;

/* 0x100ACAD8: the shipped image holds a single space. */
static const char g_szStatus[] = " ";

/* ==========================================================================
 * Stand-in 1: 0x1003AF30 / D3D 0x100419D0, SetStatusText.
 *
 * slice5_62.c owns the real body; this test links alone and supplies its own,
 * which is what every other suite in this tree that calls it does
 * (test_slice2_26.c, test_slice3_31.c, test_slice4_50.c, test_slice7_81.c,
 * test_slice8_84.c all define their own).
 * ========================================================================== */

static int         g_cStatus;
static const void *g_pStatusArg;
static int32_t     g_nModeAtStatus;   /* the mode AS SEEN by the setter */

void BrExt_100419D0(void *p)
{
    ++g_cStatus;
    g_pStatusArg   = p;
    g_nModeAtStatus = g_nGameMode;
}

/* ==========================================================================
 * Stand-in 2: the phase vtable's +0x18, br_phase.h's `f18`.
 * ========================================================================== */

static int             g_cShutdown;
static const BrPhase_ *g_pShutdownThis;
static const void     *g_pShutdownArg;
static int32_t         g_nF68AtShutdown;
static int32_t         g_nModeAtShutdown;
static int32_t         g_nSelAtShutdown;

static void StubF18(BrPhase_ *pThis, void *pArg)
{
    ++g_cShutdown;
    g_pShutdownThis  = pThis;
    g_pShutdownArg   = pArg;
    g_nF68AtShutdown = pThis->f68;
    g_nModeAtShutdown = g_nGameMode;
    g_nSelAtShutdown  = g_nMovieSel;
}

/* The other eight slots are never reached by this body; leaving them NULL
 * makes a mutation that calls the wrong one crash loudly rather than pass. */
static const BrPhaseVtbl_ g_vtbl = {
    NULL, NULL, NULL, NULL, NULL, NULL, StubF18, NULL, NULL
};

/* ==========================================================================
 * The fixture.  A real BrUiCtl_ is 0x1E214 bytes in the original and larger
 * on LP64, so it is static rather than automatic.
 * ========================================================================== */

static BrUiCtl_ g_ctl;
static BrPhase_ g_phase;

/* Seeded to values the body must overwrite, so "was written" is
 * distinguishable from "was already right". */
#define SEED_MODE     0x5A5A
#define SEED_SEL      0x5A5B
#define SEED_AA289C   0x5A5C
#define SEED_F68      1

static void Reset(int32_t nOutroFlag)
{
    memset(&g_ctl,   0, sizeof(g_ctl));
    memset(&g_phase, 0, sizeof(g_phase));

    g_phase.pVtbl = &g_vtbl;
    g_phase.f68   = SEED_F68;
    g_ctl.pOwner  = &g_phase;

    g_nGameMode  = SEED_MODE;
    g_nMovieSel  = SEED_SEL;
    g_nAA289C    = SEED_AA289C;
    g_nOutroFlag = nOutroFlag;

    g_cStatus = 0; g_pStatusArg = NULL; g_nModeAtStatus = -1;
    g_cShutdown = 0; g_pShutdownThis = NULL; g_pShutdownArg = (void *)&g_ctl;
    g_nF68AtShutdown = -1; g_nModeAtShutdown = -1; g_nSelAtShutdown = -1;

    memset(&g_brUiCredits, 0, sizeof(g_brUiCredits));
    g_brUiCredits.pszStatus   = g_szStatus;
    g_brUiCredits.pnGameMode  = &g_nGameMode;
    g_brUiCredits.pnMovieSel  = &g_nMovieSel;
    g_brUiCredits.pnOutroFlag = &g_nOutroFlag;
    g_brUiCredits.pnAA289C    = &g_nAA289C;
}

/* ==========================================================================
 * 1. The flag == 0 arm -- 0x1003AF04.
 *
 * Selector becomes 1 ("RallyCredits.dat") and 0x10AC5BF4 IS NOT TOUCHED.
 * The untouched half is the discriminating one: the outro arm clears it, and
 * a body that clears it on both arms is a real and plausible transcription
 * error that only this assertion catches.
 * ========================================================================== */
static void TestCreditsArm(void)
{
    int32_t r;

    Reset(0);
    r = BrUiCreditsAction_1003AED0(&g_ctl);

    CHECK(g_nGameMode == BR_UICREDITS_GAME_MODE,
          "0x1003AEE6: the game mode is 4 on the credits arm");
    CHECK(g_nMovieSel == BR_UICREDITS_MOVIE_CREDITS,
          "0x1003AF04: flag == 0 selects the CREDITS movie (1)");
    CHECK(g_nAA289C == SEED_AA289C,
          "0x1003AF04: the credits arm leaves 0x10AC5BF4 alone");
    CHECK(r == 0, "0x1003AF27: the hook returns 0");
}

/* ==========================================================================
 * 2. The flag != 0 arm -- 0x1003AEF2 / 0x1003AEFC.
 * ========================================================================== */
static void TestOutroArm(void)
{
    int32_t r;

    Reset(1);
    r = BrUiCreditsAction_1003AED0(&g_ctl);

    CHECK(g_nGameMode == BR_UICREDITS_GAME_MODE,
          "0x1003AEE6: the game mode is 4 on the outro arm too");
    CHECK(g_nMovieSel == BR_UICREDITS_MOVIE_OUTRO,
          "0x1003AEF2: flag != 0 selects the OUTRO movie (2)");
    CHECK(g_nAA289C == 0,
          "0x1003AEFC: the outro arm clears 0x10AC5BF4");
    CHECK(r == 0, "0x1003AF27: the hook returns 0 on this arm as well");

    /* `cmp eax, edx` against zero, not a test of the low bit and not a
     * bounded compare: ANY non-zero value takes this arm. -1 is the value a
     * "is this a boolean" misreading would get wrong. */
    Reset(-1);
    (void)BrUiCreditsAction_1003AED0(&g_ctl);
    CHECK(g_nMovieSel == BR_UICREDITS_MOVIE_OUTRO,
          "0x1003AEE4: the compare is against zero, so -1 is the outro arm");
}

/* ==========================================================================
 * 3. The teardown -- 0x1003AF19 and 0x1003AF24.
 * ========================================================================== */
static void TestTeardown(void)
{
    Reset(0);
    (void)BrUiCreditsAction_1003AED0(&g_ctl);

    CHECK(g_phase.f68 == 0,
          "0x1003AF19: the owning phase's +0x68 is cleared");
    CHECK(g_cShutdown == 1,
          "0x1003AF24: the phase vtable's +0x18 runs exactly once");
    CHECK(g_pShutdownThis == &g_phase,
          "0x1003AF24: `this` is the CONTROL'S OWNER, from +0x2AE8");
    CHECK(g_pShutdownArg == NULL,
          "0x1003AF12: the pushed argument is the zero edx has held "
          "since 0x1003AEDF, not the control");
    CHECK(g_nF68AtShutdown == 0,
          "0x1003AF19 precedes 0x1003AF24: +0x68 is already clear when the "
          "shutdown slot runs");
}

/* ==========================================================================
 * 4. Sequencing across the whole body.
 *
 * Two snapshots pin the two ends: the status setter must see the OLD mode
 * (it runs first, at 0x1003AED5, before 0x1003AEE6), and the shutdown slot
 * must see BOTH globals already written (it runs last, at 0x1003AF24).
 * ========================================================================== */
static void TestOrder(void)
{
    Reset(1);
    (void)BrUiCreditsAction_1003AED0(&g_ctl);

    CHECK(g_cStatus == 1,
          "0x1003AED5: the status setter runs exactly once");
    CHECK(g_pStatusArg == (const void *)g_szStatus,
          "0x1003AED0: it is handed 0x100ACAD8, the status text");
    CHECK(g_nModeAtStatus == SEED_MODE,
          "0x1003AED5 precedes 0x1003AEE6: the status setter sees the OLD "
          "game mode");
    CHECK(g_nModeAtShutdown == BR_UICREDITS_GAME_MODE,
          "0x1003AEE6 precedes 0x1003AF24: the shutdown sees mode 4");
    CHECK(g_nSelAtShutdown == BR_UICREDITS_MOVIE_OUTRO,
          "0x1003AEF2 precedes 0x1003AF24: the shutdown sees the selector");
}

/* ==========================================================================
 * 5. The PORT-ONLY refusal.  No counterpart in the original; asserted so it
 * cannot silently become a stand-in that half-writes the globals.
 * ========================================================================== */
static void TestRefusal(void)
{
    int32_t r;

    Reset(0);
    g_brUiCredits.pnMovieSel = NULL;
    CHECK(BrUiCreditsCtxComplete(&g_brUiCredits) == 0,
          "an incomplete ctx is seen");

    r = BrUiCreditsAction_1003AED0(&g_ctl);
    CHECK(r == 0, "the refusal returns the original's 0");
    CHECK(g_nGameMode == SEED_MODE && g_nAA289C == SEED_AA289C,
          "the refusal writes NOTHING -- not even the mode");
    CHECK(g_cStatus == 0 && g_cShutdown == 0,
          "the refusal calls nothing");
    CHECK(g_phase.f68 == SEED_F68,
          "the refusal does not tear the phase down");

    Reset(0);
    CHECK(BrUiCreditsCtxComplete(&g_brUiCredits) != 0,
          "a complete ctx is seen");
    CHECK(BrUiCreditsCtxComplete(NULL) == 0, "a NULL ctx is incomplete");
}

/* ==========================================================================
 * 6. The hook is assignable into br_uiroot.h's table with no cast.
 *
 * This is not decoration: it is the property slice8_90.h had to DECLINE for
 * six sibling hooks, and if the signature ever drifts the assignment stops
 * compiling rather than being discovered by a wiring pass.  It can fail --
 * changing the return type or the argument type in the header breaks the
 * build, which is a louder failure than an assertion.
 * ========================================================================== */
static void TestHookType(void)
{
    /* The ASSIGNMENT is the assertion, and it is a compile-time one: if the
     * signature drifts, this line stops compiling.  A runtime CHECK that the
     * pointer equals itself was written here, mutation-tested, found unable
     * to fail under any mutation of the body, and DELETED rather than left as
     * decoration -- CONVENTIONS.md's rule, applied to this suite's own
     * output. */
    BrUiCtlHookFn_ pfn = BrUiCreditsAction_1003AED0;

    /* This one can fail, and M5 in the mutation table shows it failing. */
    Reset(0);
    CHECK(pfn(&g_ctl) == 0,
          "the hook runs through a BrUiCtlHookFn_ slot with no marshal");
}

int main(void)
{
    /* Unbuffered: a mutant that faults after failing an assertion must still
     * show WHICH assertion failed. */
    setbuf(stdout, NULL);

    TestCreditsArm();
    TestOutroArm();
    TestTeardown();
    TestOrder();
    TestRefusal();
    TestHookType();

    if (g_cFail != 0) {
        printf("test_br_uicredits: %d FAILURE(S)\n", g_cFail);
        return 1;
    }
    printf("test_br_uicredits: all checks passed\n");
    return 0;
}
