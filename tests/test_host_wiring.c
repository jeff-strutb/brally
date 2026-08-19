/* test_host_wiring.c -- THE SUITE THAT LINKS port/host.
 *
 * ==========================================================================
 * WHY IT EXISTS
 * ==========================================================================
 *
 * Until this file, NO suite linked port/host at all. Every wiring decision in
 * this port -- which hook tables get installed, which module context a builder
 * is bound to, which object the frame loop reads as "the current phase" -- sat
 * outside the test gate, because port/host/brally.c carried `main` and a test
 * binary brings its own.
 *
 * The measured cost: an agent unified 0x10AA2904 (six host objects for one
 * dword), then mutation-tested the fix by reinstating the ENTIRE split, and
 * 131 suites plus `./build/brally -all` 16/16 reported no change. "N suites
 * pass" and "16 of 16 builders run clean" were both true of a tree whose menu
 * did nothing.
 *
 * ==========================================================================
 * WHAT IS ASSERTED, AND WHY EACH ONE CAN FAIL
 * ==========================================================================
 *
 * Nothing here asserts that a wiring function was CALLED. Every assertion is
 * an observable consequence downstream of the wiring:
 *
 *   - the dword a ported activate routine published is the dword the frame
 *     loop reads (M6);
 *   - a named control slot holds the named function, not merely something
 *     non-NULL;
 *   - a builder produced controls, or refused to;
 *   - the selection moved to the row it should have moved to.
 *
 * And nothing here asserts a fact this file has just established two lines
 * earlier. Where a state has to be set up to be tested (the UNBOUND direction
 * of a context check), the setup and the assertion are about OPPOSITE things:
 * the file clears a slot and asserts a REFUSAL, then restores it and asserts a
 * BUILD, and it is the pair that distinguishes a wired tree from an unwired
 * one. Either half alone cannot.
 *
 * ==========================================================================
 * WHAT THIS SUITE DOES NOT COVER, stated rather than left to be discovered
 * ==========================================================================
 *
 *   - The window loop, the Metal renderer and the screenshot path. They need
 *     a device and a compositor; `./build/brally -shot` is their evidence.
 *   - `-race`. It is a different program inside the same binary and has no
 *     wiring in common with the menu beyond the audio bank.
 *   - The BODIES of the installers. The mirror check below re-runs the six
 *     slice installers into a local table and compares -- so it catches an
 *     installer call being dropped from the host, and does NOT catch a bug
 *     inside an installer. Those have their own suites; this one is about the
 *     host calling them.
 *   - port/host/br_stubs.c. Another pass owns that file; its findings are in
 *     this pass's report, not in assertions here.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "br_phase.h"
#include "br_ui.h"
#include "br_phasecur.h"
#include "br_uiroot.h"
#include "br_uipages.h"
#include "slice6_73.h"     /* g_br73, BrUi73Hooks, BrOptObjCtor            */
#include "br_sprfont.h"    /* BrSprFontKindHook_10047360                   */
#include "br_uicredits.h"  /* BrUiCreditsAction_1003AED0                   */
#include "br_uinav.h"      /* BrUiNavHook_10045AF0 / _10046C90             */
#include "slice7_81.h"     /* BrUiHook81Install                            */
#include "slice8_85.h"     /* BrUiHook85Install                            */
#include "slice7_80.h"     /* BrUiOptInstall73                             */
#include "slice8_90.h"     /* BrUiHook90Install73                          */
#include "slice8_87.h"     /* BrUiHook87Install73                          */
#include "slice8_88.h"     /* BrUiHook88Install73                          */
#include "slice6_71.h"     /* g_brS71, g_brS71Env  -- br_wire71.c's target */
#include "slice6_72.h"     /* g_pBr72Env           -- br_wire72.c's target */
#include "slice4_52.h"     /* g_apBrStrTable       -- br_wire77.c's target */
#include "br_wireaudio.h"  /* BrHostWireAudio                              */

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
 * port/host/brally.c's seams. Declared rather than reached through a header
 * because brally.c has none -- it is a program, and the reason it has no
 * header is the same reason its own banner gives for declaring the builders
 * locally: the slice headers it uses cannot all coexist.
 * ========================================================================== */
void      BrStubReport(void);   /* port/host/br_stubs.c */
void      BrHostWireAll(void);
BrPhase_ *BrHostRootPhaseEnter(void);
BrPhase_ *BrHostNavKey(BrPhase_ *phCur, char key);
BrPhase_ **BrHostNavCurSlot(void);
int       BrHostPollCount(void);
int       BrHostUiRootReady(void);
int       BrHostBuilderCount(void);
const char *BrHostBuilderName(int i);
int       BrHostBuilderRun(int i, BrPhase_ *ph);

/* port/host/br_wire78.c's ten adapters -- the functions br_wire79.c is
 * supposed to have put in the main menu's control slots. Comparing a slot
 * against the NAME is what makes "wired to the wrong slot" detectable; a
 * non-NULL check cannot tell two neighbouring rows apart. */
int32_t BrHostRootAct_1003ED90(BrUiCtl_ *pCtl);
int32_t BrHostRootAct_1003D140(BrUiCtl_ *pCtl);
int32_t BrHostRootAct_1003E0E0(BrUiCtl_ *pCtl);
int32_t BrHostRootAct_1003E4A0(BrUiCtl_ *pCtl);
int32_t BrHostRootAct_1003E730(BrUiCtl_ *pCtl);
int32_t BrHostRootAct_1003F610(BrUiCtl_ *pCtl);
int32_t BrHostRootTick_10040AF0(BrUiCtl_ *pCtl);
int32_t BrHostRootTick_10040A20(BrUiCtl_ *pCtl);

/* slice4_50.c's player-name buffer, 0x10B4E2E8. Declared here with the
 * DEFINING type (`char *`) rather than by including slice4_50.h, which pulls
 * slice2_25.h and redefines slice1_06.h's BrDPlayVtbl -- the clash
 * br_wire78.c's WHY TWO FILES banner describes, still live. */
extern char *g_brPB4E2E8;

/* port/host/br_wire75.c -- 0x10074030 under slice3_31.h's name for it. */
void *BrExt_10074030(int32_t nId);

/* ==========================================================================
 * Small helpers
 * ========================================================================== */

static BrPhase_ *NewPhase(void)
{
    BrPhase_ *p = (BrPhase_ *)calloc(1, BR_PHASE_ALLOC_SIZE);
    return (p != NULL) ? BrOptObjCtor(p) : NULL;
}

static const BrUiPage_ *Page0(const BrPhase_ *ph)
{
    if (ph == NULL || ph->nPages == 0) return NULL;
    return ph->aPages[0];
}

/* Which control carries the CURRENT bit (+0x1C bit 0x20), by index, or -1.
 * Read out of the flags the ported navigation wrote -- see CONVENTIONS.md's
 * note that 0x20 is `current`. */
static int CurrentCtl(const BrUiPage_ *pg)
{
    int j;
    if (pg == NULL) return -1;
    for (j = 0; j < (int)pg->cCtl && j < BR_UI_PAGE_CTL_MAX; j++)
        if (pg->apCtl[j] != NULL &&
            ((uint32_t)pg->apCtl[j]->flags1C & 0x20u) != 0)
            return j;
    return -1;
}

/* ==========================================================================
 * 1. THE CURRENT-PHASE SLOT, 0x10AA2904 -- the M6 case
 *
 * The original has ONE dword. This tree had six host objects for it, and the
 * host's `BrPhaseCurBind(&g_nav.pAA2904)` is what collapses them. Removing
 * that one call is the M6 mutation, and it must fail here.
 *
 * The identity check below is the cheap half. The expensive half -- the one
 * that says the collapse is USEFUL rather than merely true -- is in section 6,
 * where a ported activate routine publishes a phase and the field the frame
 * loop reads is asked whether it saw it.
 * ========================================================================== */
static void TestCurSlotIsOneObject(void)
{
    BrPhase_ **ppNav  = BrHostNavCurSlot();
    BrPhase_ **ppCur  = BrPhaseCurSlot();

    CHECK(ppNav != NULL, "the frame loop's slot exists");
    CHECK(ppCur != NULL, "BR_PHASE_CUR names some slot (never NULL)");
    /* slice2_26 / slice3_31 / br_phaseact / slice6_71 all reach the dword as
     * BR_PHASE_CUR, and slice2_25.h makes g_brPAA2904 a macro over it, so
     * this one comparison covers every name in the tree. */
    CHECK(ppCur == ppNav,
          "0x10AA2904 is ONE object: BR_PHASE_CUR and the frame loop's "
          "g_nav.pAA2904 are the same dword");
}

/* ==========================================================================
 * 2. THE ROOT MENU BUILDS -- the bound direction of a context check
 *
 * br_uiroot.c's first statement is `if (!BrUiRootCtxComplete(&g_brUiRoot))
 * return;`. Before br_wire79.c existed, nothing outside port/tests ever filled
 * that context, so 0x100425E0 returned on its first line in every run of the
 * host and there was no main menu at all -- while `-all` still said 16/16.
 * ========================================================================== */
static BrPhase_ *g_pRoot;

static void TestRootMenuBuilds(void)
{
    const BrUiPage_ *pg;

    CHECK(BrHostUiRootReady() != 0,
          "the host filled g_brUiRoot, so 0x100425E0 will build rather than "
          "refuse on its first line");

    g_pRoot = BrHostRootPhaseEnter();
    CHECK(g_pRoot != NULL, "the root phase exists and was entered");
    if (g_pRoot == NULL) return;

    CHECK(g_pRoot->nPages == 1, "the root phase has its one page");
    pg = Page0(g_pRoot);
    CHECK(pg != NULL, "root page 0");
    if (pg == NULL) return;

    /* The counts are br_uiroot.h's, derived from the disassembly and
     * re-derived by its own compile-time assertions -- not counted off this
     * run's output. */
    CHECK(pg->cCtl == BR_UIROOT_CTL_COUNT,
          "the main menu built all fifteen controls");
    CHECK(pg->cSel == BR_UIROOT_ROWS,
          "seven selectable rows");
    CHECK(pg->fX == BR_UIROOT_PAGE_X && pg->fY == BR_UIROOT_PAGE_Y,
          "the page's two immediates");
}

/* ==========================================================================
 * 3. THE MOUSE CURSOR AT SLOT 199
 *
 * 0x100425E0's sixteenth control is the cursor and it goes in apCtl[199]
 * WITHOUT bumping cCtl. That is correct and deliberate; this asserts it so
 * that a later pass "fixing" the count fails a test instead of passing one.
 * ========================================================================== */
static void TestCursorSlot(void)
{
    const BrUiPage_ *pg = Page0(g_pRoot);
    if (pg == NULL) { CHECK(0, "root page 0 for the cursor check"); return; }

    CHECK(pg->apCtl[BR_UIROOT_CURSOR_SLOT] != NULL,
          "the mouse cursor is parked at apCtl[199]");
    CHECK(pg->cCtl == BR_UIROOT_CTL_COUNT,
          "and the cursor is NOT counted in cCtl -- deliberate, do not fix");
    CHECK(pg->apCtl[BR_UIROOT_CTL_COUNT] == NULL,
          "nothing sits immediately past the counted controls");
}

/* ==========================================================================
 * 4. HOOK INSTALLATION, part one: THE MAIN MENU'S CONTROL SLOTS
 *
 * Each row is checked against the NAMED function br_wire79.c is supposed to
 * have installed. A non-NULL check would pass with every row wired to row 0's
 * action -- every row would still "work", just not the row the player chose --
 * so identity is the property, not presence.
 *
 * The row indices are apCtl positions, and they are not contiguous: the five
 * 0x809 highlights are interleaved (br_uiroot.h OBSERVATION 2), which is why
 * the last two rows are 12 and 13 rather than 12 and 14.
 * ========================================================================== */
static void TestRootHookIdentity(void)
{
    static const int aiRow[BR_UIROOT_ROWS] = { 2, 4, 6, 8, 10, 12, 13 };
    const BrUiPage_ *pg = Page0(g_pRoot);
    BrUiCtlHookFn_ aAct[BR_UIROOT_ROWS];
    int i, j;

    if (pg == NULL) { CHECK(0, "root page 0 for the hook check"); return; }

    for (i = 0; i < BR_UIROOT_ROWS; i++) {
        if (pg->apCtl[aiRow[i]] == NULL) {
            CHECK(0, "every menu row control exists");
            return;
        }
        aAct[i] = pg->apCtl[aiRow[i]]->pfn08;
    }

    /* +0x08, the ACTION, one per row in the page's order. */
    CHECK(aAct[0] == BrHostRootAct_1003ED90, "row 0 Championship action");
    CHECK(aAct[1] == BrHostRootAct_1003D140, "row 1 Multiplayer action");
    CHECK(aAct[2] == BrHostRootAct_1003E0E0, "row 2 Time Attack action");
    CHECK(aAct[3] == BrHostRootAct_1003E4A0, "row 3 Quick Race action");
    CHECK(aAct[4] == BrHostRootAct_1003E730, "row 4 Options action");
    CHECK(aAct[5] == BrUiCreditsAction_1003AED0, "row 5 Credits action");
    CHECK(aAct[6] == BrHostRootAct_1003F610, "row 6 Quit action");

    /* No two rows may share an action. This is the assertion that survives
     * somebody renaming the adapters: seven rows, seven destinations. */
    for (i = 0; i < BR_UIROOT_ROWS; i++)
        for (j = i + 1; j < BR_UIROOT_ROWS; j++)
            CHECK(aAct[i] != aAct[j],
                  "two menu rows must not share one action hook");

    /* +0x0C, the per-frame caption setter. Three distinct values over the
     * seven rows -- 0x100407B0 on rows 0 and 3, 0x10040AF0 on 1, 2, 4 and 6,
     * 0x10040A20 on row 5 (br_uiroot.h). */
    CHECK(pg->apCtl[aiRow[0]]->pfn0C == BrSprFontKindHook_10047360 &&
          pg->apCtl[aiRow[3]]->pfn0C == BrSprFontKindHook_10047360,
          "rows 0 and 3 get 0x100407B0");
    CHECK(pg->apCtl[aiRow[1]]->pfn0C == BrHostRootTick_10040AF0 &&
          pg->apCtl[aiRow[2]]->pfn0C == BrHostRootTick_10040AF0 &&
          pg->apCtl[aiRow[4]]->pfn0C == BrHostRootTick_10040AF0 &&
          pg->apCtl[aiRow[6]]->pfn0C == BrHostRootTick_10040AF0,
          "rows 1, 2, 4 and 6 get 0x10040AF0");
    CHECK(pg->apCtl[aiRow[5]]->pfn0C == BrHostRootTick_10040A20,
          "row 5 gets 0x10040A20");
}

/* ==========================================================================
 * 5. HOOK INSTALLATION, part two: THE slice6_73 TABLE
 *
 * The host installs six packets into the fifty-one-slot BrUi73Hooks and then
 * plants three navigation hooks by hand. Three of those installers had been
 * written, tested and never called by anything -- which fills no slot at run
 * time and shows up as no behaviour at all.
 *
 * The check builds the expectation INDEPENDENTLY, by running the same six
 * installers into a local zeroed table, and compares slot by slot. So it
 * fails when the host drops an installer call, and it deliberately says
 * nothing about whether an installer's own body is right -- that is the
 * installer's suite's job, not this one's.
 * ========================================================================== */
typedef void (*BrHookAnyFn)(void);
#define BR73_HOOK_SLOTS  (sizeof(BrUi73Hooks) / sizeof(BrHookAnyFn))

static void TestHooks73Table(void)
{
    BrUi73Hooks want;
    unsigned char aWant[sizeof(BrUi73Hooks)], aGot[sizeof(BrUi73Hooks)];
    size_t i, cbSlot = sizeof(BrHookAnyFn);
    int nDiff = 0, iFirst = -1, nFilled = 0;

    CHECK(g_br73.pHooks != NULL,
          "slice6_73's context points at a hook table at all");
    if (g_br73.pHooks == NULL) return;

    memset(&want, 0, sizeof want);
    BrUiHook81Install(&want);
    BrUiHook85Install(&want);
    BrUiOptInstall73(&want);
    BrUiHook90Install73(&want);
    BrUiHook87Install73(&want);
    BrUiHook88Install73(&want);
    want.p10045AF0 = BrUiNavHook_10045AF0;
    want.p10046C90 = BrUiNavHook_10046C90;
    want.p10047360 = BrSprFontKindHook_10047360;

    memcpy(aWant, &want, sizeof aWant);
    memcpy(aGot,  g_br73.pHooks, sizeof aGot);
    for (i = 0; i < BR73_HOOK_SLOTS; i++) {
        const unsigned char *w = aWant + i * cbSlot;
        const unsigned char *g = aGot  + i * cbSlot;
        BrHookAnyFn fnW;
        memcpy(&fnW, w, cbSlot);
        if (fnW != NULL) ++nFilled;
        if (memcmp(w, g, cbSlot) != 0) {
            if (nDiff == 0) iFirst = (int)i;
            ++nDiff;
        }
    }

    if (nDiff != 0)
        printf("  hook table: %d of %u slots differ, first at index %d\n",
               nDiff, (unsigned)BR73_HOOK_SLOTS, iFirst);
    CHECK(nDiff == 0,
          "every slot the six installers and the three nav hooks own holds "
          "the function the host is supposed to have put there");

    /* A table that has gone all-NULL would satisfy the comparison above only
     * if the expectation had too -- i.e. if every installer had become a
     * no-op. This says the expectation itself is not vacuous. */
    CHECK(nFilled > 30,
          "the installers between them fill most of the fifty-one slots");

    /* The three the host plants by hand, by name, so a swap between them is
     * caught rather than averaged away by the memcmp. */
    CHECK(g_br73.pHooks->p10045AF0 == BrUiNavHook_10045AF0,
          "0x10045AF0, the FORWARD hook");
    CHECK(g_br73.pHooks->p10046C90 == BrUiNavHook_10046C90,
          "0x10046C90, the BACK hook");
    CHECK(g_br73.pHooks->p10047360 == BrSprFontKindHook_10047360,
          "0x10047360, the +0x0C recolour hook every selectable row gets");
}

/* ==========================================================================
 * 5b. THE PER-SLICE MODULE CONTEXTS
 *
 * br_wire71.c, br_wire72.c and br_wire77.c each publish one module's context.
 * Their absence is not subtle at run time -- the builders fault on a NULL
 * context pointer, which is the design (an unwired slot must be a visible
 * hole, not a silent no-op) -- but a bare SIGSEGV in a suite says only "the
 * host is broken somewhere". These name WHICH wiring is missing, and they run
 * before anything dereferences it.
 *
 * br_wire71.c's own banner makes the distinction these guard: `pH->p10047360`
 * faulting with pHooks == NULL is a NULL STRUCT POINTER, not a NULL function
 * pointer, and the two look identical in a debugger.
 * ========================================================================== */
static void TestSliceContextsBound(void)
{
    /* --- br_wire71.c ------------------------------------------------------ */
    CHECK(g_brS71.pHooks != NULL,
          "br_wire71: slice6_71's hook table is a table, not a NULL pointer");
    CHECK(g_brS71.pAA2908 != NULL,
          "br_wire71: 0x10AA2908, which 0x1004F700 reads as its very first act");
    CHECK(g_brS71.pAA2908 == NULL || g_brS71.pAA2908->fC0 != NULL,
          "br_wire71: and its +0xC0 season list is hung off it");
    CHECK(g_brS71.pA9D018 != NULL, "br_wire71: 0x10A9D018");
    CHECK(g_brS71.pszAutoSaveBrf != NULL, "br_wire71: the AutoSave.brf name");
    CHECK(g_brS71Env != NULL, "br_wire71: slice6_71's injected-callee block");

    /* --- br_wire72.c ------------------------------------------------------ *
     * The two constructors are checked by IDENTITY, not for non-NULL: this
     * context reaches them through pointers precisely so the module need not
     * redeclare addresses another header owns, and a pointer to the WRONG
     * constructor would build a control-shaped object of the wrong kind. */
    CHECK(g_pBr72Env != NULL, "br_wire72: slice6_72's context");
    if (g_pBr72Env != NULL) {
        CHECK(g_pBr72Env->pfnCtlCtor  == BrUiCtlCtor,
              "br_wire72: 0x100476C0, the control constructor");
        CHECK(g_pBr72Env->pfnPageCtor == BrUiPageCtor_10048470,
              "br_wire72: 0x10048470, the page constructor");
        CHECK(g_pBr72Env->pHooks  != NULL, "br_wire72: the hook table");
        CHECK(g_pBr72Env->pAA2908 != NULL, "br_wire72: 0x10AA2908");
    }

    /* --- br_wire77.c ------------------------------------------------------ *
     * 0x11829370, the localised string table. Handle 0 is the reserved
     * "no string" and must stay NULL; everything else must not, because
     * BrOptFn100575F0 does not merely pass a lookup along -- it strlen's it. */
    CHECK(g_apBrStrTable[0] == NULL,
          "br_wire77: handle 0 stays the reserved null");
    CHECK(BrStrGet(BR_UISTR_BACK) != NULL,
          "br_wire77: the string table is filled, so a builder that "
          "DEREFERENCES a caption does not strlen a NULL");

    /* --- br_wire75.c ------------------------------------------------------ *
     * 0x10074030 is reached under two names. slice3_31.h calls it
     * BrExt_10074030 and the original bakes the table address into the
     * instruction; br_wire75.c supplies the table the original hardcodes, and
     * it must be the one slice4_52.c owns rather than a second array. So the
     * property is that the two names return the SAME STRING for the same id --
     * an object identity, which is what "one address, one storage" means here
     * and what a non-NULL check would miss entirely. */
    CHECK(BrExt_10074030(BR_UISTR_QUIT_TITLE) ==
          (void *)(size_t)(const void *)BrStrGet(BR_UISTR_QUIT_TITLE),
          "br_wire75: 0x10074030's two host names read one table");
}

/* ==========================================================================
 * 6. THE M6 CASE, BEHAVIOURALLY: A ROW'S ACTION AND THE FRAME LOOP'S DWORD
 *
 * `-keys root ".ddj"` walks to "Time Attack" and activates it. The row's +0x08
 * hook runs slice4_53 -> slice2_26, which constructs a phase, runs the ported
 * enter hook 0x10059760, and PUBLISHES it as the current phase.
 *
 * The assertion is not that the hook ran. It is that the field the host's
 * frame loop reads afterwards names the new screen, and that the new screen is
 * the one 0x10059760 builds. With 0x10AA2904 split -- which is what removing
 * BrPhaseCurBind does -- the hook still runs, the phase is still constructed,
 * and this field still names the main menu. That is exactly what "the menu
 * appears inert" was.
 *
 * The frame itself is the game's: NavFrame dispatches phase vtable +0x0C into
 * the ported 0x100489A0, which calls back for input at 0x10060260's own site.
 * Nothing in this file moves a selection cursor.
 * ========================================================================== */

/* What 0x10059760 (slice6_72) builds: one page, six controls, three of them
 * selectable -- backdrop, title, New Race, Load Race, Back, and the screen's
 * decoration. Read off the ported builder, not off this run. */
#define BR_TIMEATTACK_CTL   6
#define BR_TIMEATTACK_SEL   3

static void TestMenuTransitionReachesTheFrameLoop(void)
{
    BrPhase_ *ph;
    const BrUiPage_ *pg;
    int nPollBefore = BrHostPollCount();

    if (g_pRoot == NULL) { CHECK(0, "root phase for the nav test"); return; }

    /* The mode drivers in brally.c publish the built phase before the first
     * frame; so does this. */
    *BrHostNavCurSlot() = g_pRoot;
    ph = g_pRoot;

    /* One idle frame. The CURRENT bit is raised by 0x10047A60 from inside the
     * frame, so no control carries it until a frame has run. */
    ph = BrHostNavKey(ph, '.');
    CHECK(ph == g_pRoot, "an idle frame does not change screen");
    CHECK(CurrentCtl(Page0(ph)) == 2,
          "after one frame the CURRENT bit is on row 0, apCtl[2]");

    ph = BrHostNavKey(ph, 'd');
    CHECK(CurrentCtl(Page0(ph)) == 4, "down moves the current row to apCtl[4]");
    ph = BrHostNavKey(ph, 'd');
    CHECK(CurrentCtl(Page0(ph)) == 6, "down again reaches apCtl[6], Time Attack");

    CHECK(BrHostPollCount() == nPollBefore + 3,
          "the frame reached the input seam once per frame -- the key gets in "
          "through 0x10060260's own call site, not past it");

    ph = BrHostNavKey(ph, 'j');

    /* THE ASSERTION THIS WHOLE FILE EXISTS FOR. */
    CHECK(*BrHostNavCurSlot() != g_pRoot,
          "M6: the row's action published a new phase and the FRAME LOOP'S "
          "0x10AA2904 saw it");
    CHECK(ph != NULL && ph != g_pRoot,
          "M6: the driver's next frame would run the new screen");
    if (ph == NULL || ph == g_pRoot) return;

    pg = Page0(ph);
    CHECK(pg != NULL, "the published phase has a page");
    if (pg == NULL) return;
    /* Not merely "a different pointer": the screen 0x10059760 builds. A split
     * dword cannot produce this, and neither can a stale pointer. */
    CHECK(pg->cCtl == BR_TIMEATTACK_CTL && pg->cSel == BR_TIMEATTACK_SEL,
          "and it is the Time Attack screen 0x10059760 builds");
}

/* ==========================================================================
 * 7. AN UNBOUND BUILD CONTEXT: THE REFUSAL IS THE CORRECT ANSWER
 *
 * Row 3 "Quick Race" reaches slice2_26's ported activate, which constructs a
 * phase and calls slice8_86's placeholder enter hook. That hook returns
 * immediately while `g_pBrUiBuildCtx86` is NULL, and br_wire79.c deliberately
 * does NOT bind it -- slice3_33.h's BrUiPhase is a SHIFTED model of the phase
 * object, so binding it would corrupt what it built rather than build it.
 *
 * So the two things worth asserting are: the action really did publish (the
 * wiring works), and what it published is EMPTY (the refusal is intact). A
 * test that only checked "a phase appeared" would pass on a tree that had
 * bound the context and was quietly writing over vtable pointers.
 *
 * The action is called directly rather than navigated to, because section 6
 * has already left the front end on another screen; it is the same exported
 * adapter br_wire79.c installs in the row.
 * ========================================================================== */
static void TestQuickRaceRefusesAndStillPublishes(void)
{
    BrPhase_ *pBefore = *BrHostNavCurSlot();
    BrPhase_ *pAfter;
    int32_t   rc;

    rc = BrHostRootAct_1003E4A0(NULL);
    CHECK(rc != 0, "0x10044F50 reports success");

    pAfter = *BrHostNavCurSlot();
    CHECK(pAfter != NULL && pAfter != pBefore,
          "M6: a ported activate called outside the menu still publishes into "
          "the dword the frame loop reads");
    if (pAfter == NULL || pAfter == pBefore) return;

    CHECK(pAfter->nPages == 0,
          "and the screen is EMPTY, because g_pBrUiBuildCtx86 is deliberately "
          "unbound -- br_wire79.c says why");
}

/* ==========================================================================
 * 8. CONTEXT BINDING, BOTH DIRECTIONS
 *
 * Two contexts the host does NOT fill, and one it does.
 *
 *   g_brUiPages   nothing in port/host wires it, so both of its builders
 *                 refuse. The multiplayer builder additionally refuses
 *                 without `ppCtlContinue` (0x10AA29A8's owner) and without
 *                 slice4_50.c's `g_brPB4E2E8` (0x10B4E2E8).
 *   g_brUiRoot    the host fills it; section 2 covers the built direction.
 *
 * Each is tested in both states, because a test that only checks the happy
 * path cannot tell a wired tree from an unwired one -- and neither can a test
 * that only checks the refusal.
 * ========================================================================== */
static const BrUiPagesHooks g_pagesHooks;      /* all NULL: a hook slot the
                                                * builders only STORE         */
static void PagesErrShow(void *pUser, uint32_t id, const char *psz)
{
    (void)pUser; (void)id; (void)psz;
}
static const BrErrHost g_pagesErr = { NULL, NULL, NULL, NULL };

static void TestUiPagesContextBothWays(void)
{
    BrUiPagesCtx  save = g_brUiPages;
    char         *pszSaveName = g_brPB4E2E8;
    BrUiCtl_     *pContinue = NULL;
    char          aName[64];
    BrPhase_     *ph;

    (void)g_pagesHooks; (void)PagesErrShow;

    /* --- the host's ACTUAL state: unwired, so both builders refuse -------- */
    CHECK(BrUiPagesCtxComplete(&g_brUiPages) == 0,
          "nothing in port/host wires g_brUiPages today");
    ph = NewPhase();
    CHECK(ph != NULL, "phase for the refusal probe");
    if (ph == NULL) return;
    BrUiQuitEnter_10043050(ph);
    CHECK(ph->nPages == 0,
          "an unwired g_brUiPages makes 0x10043050 refuse rather than build a "
          "plausible quit screen");
    free(ph);

    /* --- bound: it builds ------------------------------------------------ */
    g_brUiPages.pHooks     = &g_pagesHooks;
    g_brUiPages.pErrHost   = &g_pagesErr;
    g_brUiPages.pStyleTitle = BR_UI_STYLE(0x100AB508);
    g_brUiPages.pStyleRow   = BR_UI_STYLE(0x100AB448);
    CHECK(BrUiPagesCtxComplete(&g_brUiPages) != 0, "now complete");

    ph = NewPhase();
    CHECK(ph != NULL, "phase for the build probe");
    if (ph != NULL) {
        BrUiQuitEnter_10043050(ph);
        CHECK(ph->nPages == 1, "a bound context builds the quit screen");
        CHECK(Page0(ph) != NULL &&
              Page0(ph)->cCtl == BR_UIQUIT_CTL_COUNT &&
              Page0(ph)->cSel == BR_UIQUIT_SEL_COUNT,
              "with the four controls and two rows 0x10043050 builds");
        free(ph);
    }

    /* --- the multiplayer builder's TWO EXTRA slots, each on its own ------- *
     * ppCtlContinue (0x10AA29A8's owner) and g_brPB4E2E8 (0x10B4E2E8). The
     * quit screen needs neither, so a context that builds one and refuses the
     * other is exactly the state these two slots distinguish. */
    g_brUiPages.ppCtlContinue = NULL;
    g_brPB4E2E8               = NULL;
    CHECK(BrUiMultiCtxComplete(&g_brUiPages) == 0,
          "the multiplayer builder's two extra slots are not filled");
    ph = NewPhase();
    if (ph != NULL) {
        BrUiMultiEnter_1004F290(ph);
        CHECK(ph->nPages == 0,
              "so 0x1004F290 refuses -- it will not build a screen whose "
              "published-control slot nobody owns");
        free(ph);
    }

    /* one at a time, so neither slot can be carried by the other */
    g_brUiPages.ppCtlContinue = &pContinue;
    CHECK(BrUiMultiCtxComplete(&g_brUiPages) == 0,
          "ppCtlContinue alone is not enough");

    memset(aName, 0, sizeof aName);
    g_brPB4E2E8 = aName;
    CHECK(BrUiMultiCtxComplete(&g_brUiPages) != 0,
          "with the name buffer as well, it is");

    ph = NewPhase();
    if (ph != NULL) {
        BrUiMultiEnter_1004F290(ph);
        CHECK(ph->nPages == 1, "and 0x1004F290 builds");
        CHECK(Page0(ph) != NULL &&
              Page0(ph)->cCtl == BR_UIMULTI_CTL_COUNT &&
              Page0(ph)->cSel == BR_UIMULTI_SEL_COUNT,
              "with its eight controls and three rows");
        CHECK(pContinue != NULL,
              "and it published its Continue control into the slot the host "
              "supplied -- the whole reason the builder demands one");
        free(ph);
    }

    g_brUiPages  = save;
    g_brPB4E2E8  = pszSaveName;
}

/* ==========================================================================
 * 9. THE ROOT CONTEXT, UNBOUND
 *
 * The mirror of section 2, and it is the pair that has meaning. Clearing ONE
 * slot must be enough: br_uiroot.c refuses on the whole context, not on the
 * slot it happens to reach first.
 * ========================================================================== */
static void TestRootContextUnbound(void)
{
    BrUiRootCtx save = g_brUiRoot;
    BrPhase_ *ph;

    g_brUiRoot.pStyleRow = NULL;
    CHECK(BrUiRootCtxComplete(&g_brUiRoot) == 0,
          "one missing slot makes the root context incomplete");
    CHECK(BrHostUiRootReady() == 0,
          "and the host reports it, rather than letting the menu be "
          "mysteriously empty");

    ph = NewPhase();
    if (ph != NULL) {
        BrUiRootEnter_100425E0(ph);
        CHECK(ph->nPages == 0,
              "0x100425E0 refuses on an incomplete context rather than "
              "building a plausible main menu");
        free(ph);
    }

    g_brUiRoot = save;
    CHECK(BrHostUiRootReady() != 0, "and the host's own context is restored");
}

/* ==========================================================================
 * 10. THE BUILDER TABLE
 *
 * `-all` runs sixteen builders and reports how many did not crash. That is a
 * liveness check, not a wiring check: a builder whose context is unwired can
 * still return without crashing. This asserts the two module contexts the
 * host binds are actually reachable from a builder -- the slice6_71 and
 * slice6_72 packets, which br_wire71.c and br_wire72.c own.
 * ========================================================================== */
static int BuilderIndex(const char *pszName)
{
    int i;
    for (i = 0; i < BrHostBuilderCount(); i++)
        if (strcmp(BrHostBuilderName(i), pszName) == 0) return i;
    return -1;
}

static void TestBuilderContexts(void)
{
    /* 0x1004E830 is slice6_72's, and br_wire72.c is what gives it a page and
     * control constructor; with pfnCtlCtor NULL it cannot make a control at
     * all. 0x1004F700 is slice6_71's and reads g_brS71.pAA2908->fC0 as its
     * very first act -- br_wire71.c is what makes that non-NULL. */
    static const char *const apszName[] = { "BrExt_1004E830", "BrExt_1004F700" };
    size_t k;

    CHECK(BrHostBuilderCount() == 16, "the sixteen ported screen builders");

    for (k = 0; k < sizeof apszName / sizeof apszName[0]; k++) {
        int       i  = BuilderIndex(apszName[k]);
        BrPhase_ *ph;
        const BrUiPage_ *pg;

        CHECK(i >= 0, apszName[k]);
        if (i < 0) continue;
        ph = NewPhase();
        CHECK(ph != NULL, "phase for the builder probe");
        if (ph == NULL) continue;

        CHECK(BrHostBuilderRun(i, ph) != 0, "the builder ran");
        CHECK(ph->nPages >= 1, apszName[k]);
        pg = Page0(ph);
        CHECK(pg != NULL && pg->cCtl > 0,
              "the builder produced controls -- its module context is bound");
        free(ph);
    }
}

/* ========================================================================== */

int main(void)
{
    /* Unbuffered: a mutant that faults after failing an assertion must still
     * show WHICH assertion failed. */
    setbuf(stdout, NULL);

    /* Exactly what port/host/brally_main.c's entry point does, in its order:
     * audio first (it is the one piece `-race` needs and the menu wiring is
     * not), then everything else. */
    BrHostWireAudio();
    BrHostWireAll();

    TestCurSlotIsOneObject();
    TestRootMenuBuilds();
    TestCursorSlot();
    TestRootHookIdentity();
    TestHooks73Table();
    TestSliceContextsBound();
    TestBuilderContexts();
    TestUiPagesContextBothWays();
    TestMenuTransitionReachesTheFrameLoop();
    TestQuickRaceRefusesAndStillPublishes();
    TestRootContextUnbound();

    /* Which unported functions the wired front end actually reached. Printed
     * rather than asserted: the list is a work queue, not a contract, and
     * pinning it would make porting a function fail a test. It belongs here
     * because this suite runs the same wiring the game runs, so it measures
     * the same demand `./build/brally` measures. */
    BrStubReport();

    printf("test_host_wiring: %d failures\n", g_cFail);
    return g_cFail != 0;
}
