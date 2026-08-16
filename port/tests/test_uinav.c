/* test_uinav.c -- the menu navigation chain: 0x100484F0, 0x10048530,
 * 0x10048180, 0x10047A60 and the two hooks, over br_ui.h's model.
 *
 * WHAT IS ASSERTED, AND WHY IT IS NOT VOLUME
 *
 * Every check below is either a boundary of the original's arithmetic or an
 * invariant that a plausible-but-wrong port breaks. In particular:
 *
 *  - The selection clamp is a WRAP with an asymmetry: cursor >= cSel writes
 *    the global back to 0, cursor < 0 writes it to cSel-1, and the in-range
 *    arm deliberately does NOT write the global at all. A port that "tidies"
 *    the third arm into an unconditional store still passes any test that
 *    only looks at page->iSel, so the global is checked separately from the
 *    page field. A round trip -- down cSel times returns to where it started
 *    -- is asserted rather than a table of expected indices.
 *
 *  - -1 and 0 are DIFFERENT: 0 is a valid index and -1 means "one before the
 *    first". The clamp is fed a genuine -1 (as an int16 in an unsigned word)
 *    and must produce cSel-1, not 65535 and not 0.
 *
 *  - Exactly ONE control carries the CURRENT bit after a frame, and it is the
 *    control whose ORDINAL equals the cursor -- not the control whose INDEX
 *    equals it. The two differ here on purpose: the test page puts an inert
 *    (0x8) control first, so index and ordinal are permanently off by one,
 *    and a port that confuses them fails.
 *
 *  - The ACTIVATE bit is raised on the current control only, the +0x08 hook
 *    runs once, and the bit is CLEARED afterwards -- so holding the key for
 *    two frames fires the hook twice and releasing it fires it not at all.
 *
 *  - A +0x08 hook that returns 0 stops the page walk, which is the mechanism
 *    the BACK hook relies on.
 *
 * The stand-ins at the bottom are the functions br_uinav.c calls that live in
 * other packets. They are HERE and nowhere else.
 */
#include "br_uinav.h"
#include "br_uivt.h"   /* BrUiPageCtor_10048470 / g_pBrUiPageVtbl */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_checks, g_fails;
#define CHECK(cond, msg) do { g_checks++; if (!(cond)) { \
    g_fails++; printf("  [FAIL] %s (%s:%d)\n", (msg), __FILE__, __LINE__); } } while (0)

/* ==========================================================================
 * The fixture: one phase, one page, N controls
 * ========================================================================== */

#define FIX_CTL 5

static BrScrGlobals  s_scr;
static BrActiveFlags s_active;
static BrObjAA2E80   s_obj;
static BrUiNav       s_nav;
static int32_t       s_cursor[2] = { -1, -1 };

static BrPhase_   s_phase;
static BrUiPage_ *s_page;
static BrUiCtl_  *s_aCtl[FIX_CTL];

static BrPhaseVtbl_  s_phaseVtbl;
static BrUiPageVtbl_ s_pageVtbl;
static BrUiCtlVtbl_  s_ctlVtbl;
static BrTextBoxVtbl s_boxVtbl;

static int s_nHook08[FIX_CTL];
static int s_hook08Ret = 1;
static int s_nDraw;

static void      FixDraw(BrUiCtl_ *p)             { (void)p; s_nDraw++; }
static void      FixDrawRect(BrUiCtl_ *p, void *r){ (void)p; (void)r; s_nDraw++; }
static void      FixBoxDraw(BrTextBox *p)         { (void)p; s_nDraw++; }
static void     *FixPageDel(BrUiPage_ *p, int32_t f) { (void)f; return p; }
static void     *FixCtlDel(BrUiCtl_ *p, int32_t f)   { (void)f; return p; }

static int32_t FixHook08(BrUiCtl_ *pCtl)
{
    int i;
    for (i = 0; i < FIX_CTL; i++)
        if (s_aCtl[i] == pCtl) s_nHook08[i]++;
    return s_hook08Ret;
}

/* Control 0 is INERT (0x8): 0x10047A60 returns before touching the ordinal
 * counter, so it can never be selected and it shifts index away from ordinal
 * for every control after it. Controls 1..4 are ordinary menu rows, which is
 * the 0x102001 the sixteen builders actually pass. */
static void FixBuild(void)
{
    int i;

    memset(&s_scr, 0, sizeof s_scr);
    memset(&s_active, 0, sizeof s_active);
    memset(&s_obj, 0, sizeof s_obj);
    memset(&s_nav, 0, sizeof s_nav);
    memset(s_nHook08, 0, sizeof s_nHook08);
    s_nDraw = 0;
    s_hook08Ret = 1;

    s_scr.pAA2E80 = &s_obj;
    s_scr.w0AB3DC = 0;

    s_nav.pG      = &s_scr;
    s_nav.pCursor = s_cursor;
    s_nav.pActive = &s_active;
    s_nav.apHot[0] = BR_UI_STYLE(0x100AB448);
    s_nav.apHot[1] = BR_UI_STYLE(0x100AB418);
    s_nav.apHot[2] = BR_UI_STYLE(0x100AB428);
    g_pBrUiNav = &s_nav;

    memset(&s_ctlVtbl, 0, sizeof s_ctlVtbl);
    BrUiNavInstallCtlVtbl(&s_ctlVtbl);
    s_ctlVtbl.f00 = (void *)FixCtlDel;
    s_ctlVtbl.f18 = FixDrawRect;
    s_ctlVtbl.f1C = FixDraw;
    g_pBrUiCtlVtbl = &s_ctlVtbl;

    memset(&s_boxVtbl, 0, sizeof s_boxVtbl);
    s_boxVtbl.pfn10 = FixBoxDraw;

    memset(&s_pageVtbl, 0, sizeof s_pageVtbl);
    BrUiNavInstallPageVtbl(&s_pageVtbl);
    s_pageVtbl.f00 = FixPageDel;
    g_pBrUiPageVtbl = &s_pageVtbl;

    memset(&s_phaseVtbl, 0, sizeof s_phaseVtbl);
    memset(&s_phase, 0, sizeof s_phase);
    s_phase.pVtbl  = &s_phaseVtbl;
    s_phase.nPages = 1;

    s_page = (BrUiPage_ *)calloc(1, BR_UI_PAGE_ALLOC_SIZE);
    (void)BrUiPageCtor_10048470(s_page);
    s_page->pOwner  = &s_phase;
    s_phase.aPages[0] = s_page;
    s_phase.pCur      = s_page;
    s_phase.aFlags[0] = 1;

    for (i = 0; i < FIX_CTL; i++) {
        BrUiCtl_ *c = (BrUiCtl_ *)calloc(1, BR_UI_CTL_ALLOC_SIZE);
        (void)BrUiCtlCtor(c);
        c->pOwner  = &s_phase;
        c->pfn08   = FixHook08;
        c->aText[0].pVtbl = &s_boxVtbl;
        c->aText[1].pVtbl = &s_boxVtbl;
        c->aText[2].pVtbl = &s_boxVtbl;
        /* The place flags the builders use, OR-ed into the ctor's 1. */
        c->flags1C = (i == 0) ? 0x100009 : 0x102001;
        c->flags28 = 5;
        /* Rectangles far from the parked cursor, so nothing is "moused". */
        c->rcLeft = 100; c->rcTop = 100 + 20 * i;
        c->rcRight = 200; c->rcBottom = 120 + 20 * i;
        s_aCtl[i] = c;
        s_page->apCtl[i] = c;
        s_page->cCtl++;
        if (i != 0) s_page->cSel++;
    }
}

static void FixFree(void)
{
    int i;
    for (i = 0; i < FIX_CTL; i++) free(s_aCtl[i]);
    free(s_page);
}

static int FixCurrent(void)
{
    int i, found = -1, n = 0;
    for (i = 0; i < FIX_CTL; i++)
        if (((uint32_t)s_aCtl[i]->flags1C & 0x20u) != 0) { found = i; n++; }
    return (n == 1) ? found : -1000 - n;   /* -1000-n encodes "not exactly 1" */
}

/* One frame with a direction and/or the fire button held, exactly as the
 * host's scripted-input mode drives it: the move happens BEFORE the frame,
 * because 0x100603A0 runs before the phase's page walk. */
static void FixFrame(int dir, int fire)
{
    BrUiNavMove(&s_nav, dir);
    BrUiNavSetActivate(&s_nav, fire);
    (void)BrUiNavPageFrame_10048530(&s_nav, s_page);
    BrUiNavSetStep(&s_nav, 0);
    BrUiNavSetActivate(&s_nav, 0);
}

/* ==========================================================================
 * 0x100484F0
 * ========================================================================== */

static void TestSelectClamp(void)
{
    FixBuild();

    /* In range: page->iSel follows, the GLOBAL is left alone. Seeded with a
     * value the page cannot produce so "left alone" is observable. */
    s_scr.wAA286C = 2;
    s_page->iSel  = 0x5A5A;
    CHECK(BrUiNavPageSelect_100484F0(&s_nav, s_page) == 1,
          "0x100484F0 always returns 1");
    CHECK(s_page->iSel == 2, "in-range cursor lands in iSel");
    CHECK(s_scr.wAA286C == 2, "in-range cursor does NOT rewrite the global");

    /* One past the top wraps to 0, and the global IS written. */
    s_scr.wAA286C = (uint16_t)s_page->cSel;
    (void)BrUiNavPageSelect_100484F0(&s_nav, s_page);
    CHECK(s_page->iSel == 0 && s_scr.wAA286C == 0,
          "cursor == cSel wraps to 0 in both places");

    /* -1 is a genuine -1, not 65535 and not 0: it wraps to the LAST entry. */
    s_scr.wAA286C = (uint16_t)(int16_t)-1;
    (void)BrUiNavPageSelect_100484F0(&s_nav, s_page);
    CHECK((int)s_page->iSel == (int)s_page->cSel - 1,
          "cursor -1 wraps to cSel-1 (sign-extended, not 65535)");
    CHECK(s_scr.wAA286C == (uint16_t)(s_page->cSel - 1),
          "the -1 arm writes the global too");

    /* Any cursor at or above cSel goes to 0, however far above -- the test is
     * `>=`, not `==`. */
    s_scr.wAA286C = (uint16_t)(s_page->cSel + 7);
    (void)BrUiNavPageSelect_100484F0(&s_nav, s_page);
    CHECK(s_page->iSel == 0, "cursor far past the end still wraps to 0");

    /* A page with no selectable rows. This one was written the other way
     * round first -- "cSel == 0 collapses every cursor to 0" -- and that was
     * an EXPECTATION, not a property of the code, which is the failure mode
     * CONVENTIONS.md warns about. The original does this:
     *
     *   cursor 0 with cSel 0 -> 0 >= 0, so it wraps to 0.
     *   cursor -1 with cSel 0 -> neither arm applies, so it takes cSel-1,
     *   which is 0 - 1 == 0xFFFF, and stores 0xFFFF in BOTH places.
     *
     * A page with nothing selectable therefore leaves a nonsensical
     * selection, and keeps it, because 0xFFFF sign-extends back to -1 on the
     * next call. Reproduced, not repaired. */
    s_page->cSel = 0;
    s_scr.wAA286C = 0;
    (void)BrUiNavPageSelect_100484F0(&s_nav, s_page);
    CHECK(s_page->iSel == 0 && s_scr.wAA286C == 0,
          "cSel == 0: a cursor of 0 wraps to 0");
    s_scr.wAA286C = (uint16_t)(int16_t)-1;
    (void)BrUiNavPageSelect_100484F0(&s_nav, s_page);
    CHECK(s_page->iSel == 0xFFFF && s_scr.wAA286C == 0xFFFF,
          "cSel == 0: a cursor of -1 becomes 0xFFFF and STAYS there");
    (void)BrUiNavPageSelect_100484F0(&s_nav, s_page);
    CHECK(s_page->iSel == 0xFFFF,
          "and it is a fixed point -- the page never recovers on its own");

    FixFree();
}

/* ==========================================================================
 * 0x10048530 + 0x10047A60: which control is CURRENT
 * ========================================================================== */

static void TestCurrentMark(void)
{
    int i;

    FixBuild();
    FixFrame(0, 0);

    CHECK(FixCurrent() == 1,
          "the cursor's ORDINAL 0 is control INDEX 1, because index 0 is inert");
    CHECK(((uint32_t)s_aCtl[0]->flags1C & 0x20u) == 0,
          "an inert (0x8) control never becomes current");
    for (i = 2; i < FIX_CTL; i++)
        CHECK(((uint32_t)s_aCtl[i]->flags1C & 0x20u) == 0,
              "every non-selected control has the CURRENT bit cleared");

    /* The ordinal counter counted the four non-inert controls and not the
     * inert one -- which is what makes cSel and the ordinal space agree. */
    CHECK(s_scr.wAA2870 == (uint16_t)s_page->cSel,
          "the ordinal counter ends equal to cSel");

    FixFree();
}

static void TestMove(void)
{
    int i, seen[FIX_CTL];

    FixBuild();
    FixFrame(0, 0);
    CHECK(FixCurrent() == 1, "starts on the first selectable control");

    FixFrame(+1, 0);
    CHECK(FixCurrent() == 2, "one down moves one control");
    FixFrame(+1, 0);
    CHECK(FixCurrent() == 3, "two down moves two controls");

    /* Round trip: cSel more downs from here returns to the same control, and
     * every selectable control is visited exactly once on the way. */
    memset(seen, 0, sizeof seen);
    for (i = 0; i < (int)s_page->cSel; i++) {
        FixFrame(+1, 0);
        seen[FixCurrent()]++;
    }
    CHECK(FixCurrent() == 3, "cSel downs is the identity (it wraps)");
    for (i = 1; i < FIX_CTL; i++)
        CHECK(seen[i] == 1, "each selectable control is visited exactly once");
    CHECK(seen[0] == 0, "the inert control is never visited");

    /* Up from the first wraps to the last, which is the -1 arm of the clamp
     * reached through the frame rather than called directly. */
    s_scr.wAA286C = 0;
    FixFrame(0, 0);
    CHECK(FixCurrent() == 1, "reset to the top");
    FixFrame(-1, 0);
    CHECK(FixCurrent() == FIX_CTL - 1, "up from the top wraps to the bottom");

    /* A frame with no direction held does not move: the step is the whole
     * input, and a port that moved on every frame would pass every test
     * above and fail this one. */
    {
        int before = FixCurrent();
        FixFrame(0, 0);
        CHECK(FixCurrent() == before, "an idle frame does not move the selection");
    }

    FixFree();
}

/* ==========================================================================
 * Activation
 * ========================================================================== */

static void TestActivate(void)
{
    int i;

    FixBuild();
    FixFrame(0, 0);
    CHECK(s_nHook08[1] == 0, "no hook fires while nothing is held");

    FixFrame(0, 1);
    CHECK(s_nHook08[1] == 1, "the current control's +0x08 hook fires once");
    for (i = 0; i < FIX_CTL; i++)
        if (i != 1)
            CHECK(s_nHook08[i] == 0, "no other control's hook fires");
    CHECK(((uint32_t)s_aCtl[1]->flags1C & 0x2u) == 0,
          "0x10048180 clears the ACTIVATE bit after calling the hook");

    /* Released: the next frame must not re-fire. */
    FixFrame(0, 0);
    CHECK(s_nHook08[1] == 1, "releasing stops the hook re-firing");

    /* Held for a second frame: the original re-raises the bit every frame,
     * so a held key really does fire again. Reproduced, not "fixed". */
    FixFrame(0, 1);
    CHECK(s_nHook08[1] == 2, "a held activate fires again on the next frame");

    /* Move, then activate: the hook follows the selection. */
    FixFrame(+1, 0);
    FixFrame(0, 1);
    CHECK(s_nHook08[2] == 1, "after moving, the NEW current control's hook fires");
    CHECK(s_nHook08[1] == 2, "and the old one does not");

    FixFree();
}

static void TestHookZeroStopsPage(void)
{
    FixBuild();
    FixFrame(0, 0);

    /* A +0x08 hook returning 0 makes 0x10048180 return 0, which makes
     * 0x10048530 return 0 and abandon the rest of the page. That is the
     * mechanism 0x10046C90 (BACK) depends on: it tears its own screen down
     * and must not have the walk continue over the wreckage. */
    s_hook08Ret = 0;
    s_scr.bAA28A8 = 0x5A;
    FixFrame(0, 1);
    CHECK(BrUiNavPageFrame_10048530(&s_nav, s_page) == 1,
          "a page whose hooks say yes returns 1");
    s_hook08Ret = 0;
    BrUiNavSetActivate(&s_nav, 1);
    CHECK(BrUiNavPageFrame_10048530(&s_nav, s_page) == 0,
          "a +0x08 hook returning 0 stops the page walk");
    CHECK(s_scr.bAA28A8 == 0,
          "and 0x10AA28A8 is cleared on that exit");
    BrUiNavSetActivate(&s_nav, 0);

    FixFree();
}

/* ==========================================================================
 * The override in 0x1003E080 reaches all the way through
 * ========================================================================== */

static void TestActiveOverride(void)
{
    FixBuild();
    FixFrame(0, 0);

    /* br_state.h records that 0x10AA285C INVERTS the predicate: set, it forces
     * the answer to 0 whatever else is set. 0x10047A60 consults it before the
     * predicate, so a set override must stop activation entirely. */
    s_active.override = 1;
    FixFrame(0, 1);
    CHECK(s_nHook08[1] == 0,
          "the 0x10AA285C override suppresses activation");
    s_active.override = 0;
    FixFrame(0, 1);
    CHECK(s_nHook08[1] == 1, "clearing it restores activation");

    FixFree();
}

/* ==========================================================================
 * 0x100489A0 -- the PHASE's frame, the top of the chain.
 *
 * What is asserted here is the part a hand-written reproduction of the page
 * loop gets wrong by omission, because the page loop is the easy half:
 *
 *  - +0x68 is a VETO. Clear it and the frame does no work at all: no phase
 *    method, no poll, no page. It runs the teardown instead, and the teardown
 *    is two specific calls in a fixed order plus a zeroed +0x12.
 *  - the SAME teardown runs on the second exit, when +0x68 was still set at
 *    entry and something cleared it during the page walk. Both exits return 0
 *    and they are distinguishable only by whether the page ran.
 *  - pCur is published BEFORE the NULL test, so a NULL page slot leaves the
 *    phase pointing at NULL. A tidied port that tests first leaves the
 *    PREVIOUS page there, which is a different and much harder bug.
 *  - the poll happens with the current phase swapped to the root and restored
 *    straight after, so a callee sees the root and the caller does not.
 * ========================================================================== */

/* Counted by the two teardown stand-ins at the bottom of this file. */
static int       s_n3E310, s_n6A4A0;

static int       s_nPhF04, s_nPhF08, s_nPhF18, s_nPoll;
static BrPhase_ *s_pollSaw;
static BrPhase_  s_root;
static int       s_pageTailClearsF68;

static int32_t FixPhF04(BrPhase_ *p) { (void)p; s_nPhF04++; return 1; }
static int32_t FixPhF08(BrPhase_ *p) { (void)p; s_nPhF08++; return 1; }
static void    FixPhF18(BrPhase_ *p, void *pArg)
{ (void)p; (void)pArg; s_nPhF18++; }

static void FixPoll(BrUiNav *pNav) { s_nPoll++; s_pollSaw = pNav->pAA2904; }

/* The page's +0x08 tail hook, which 0x10048530 runs only when the whole page
 * walked. Clearing +0x68 from it is how a real screen asks to be dropped. */
static void FixPageTail(void)
{
    if (s_pageTailClearsF68) s_phase.f68 = 0;
}

static void FixPhaseBuild(void)
{
    FixBuild();
    s_nPhF04 = s_nPhF08 = s_nPhF18 = s_nPoll = 0;
    s_n3E310 = s_n6A4A0 = 0;
    s_pollSaw = NULL;
    s_pageTailClearsF68 = 0;

    BrUiNavInstallPhaseVtbl(&s_phaseVtbl);
    s_phaseVtbl.f04 = FixPhF04;
    s_phaseVtbl.f08 = FixPhF08;
    s_phaseVtbl.f18 = FixPhF18;

    memset(&s_root, 0, sizeof s_root);
    s_nav.pfnPoll = FixPoll;
    s_nav.pAA2904 = &s_phase;
    s_nav.pAA2908 = &s_root;

    s_phase.f68  = 1;
    s_phase.iPage = 0x5A5A;      /* a value the frame must overwrite */
}

static void TestPhaseFrame(void)
{
    /* --- the success path, reached through the vtable slot the original
     * reaches it through --------------------------------------------------- */
    FixPhaseBuild();
    CHECK(s_phaseVtbl.f0C != NULL,
          "BrUiNavInstallPhaseVtbl fills +0x0C");
    CHECK(s_phase.pVtbl->f0C(&s_phase) == 1,
          "a phase with +0x68 set and one flagged page reports success");
    CHECK(s_nPhF04 == 1 && s_nPhF08 == 1,
          "+0x04 and +0x08 each dispatched exactly once");
    CHECK(s_nPoll == 1, "the frame polled for input exactly once");
    CHECK(s_pollSaw == &s_root,
          "the poll sees the ROOT phase as current (the swap)");
    CHECK(s_nav.pAA2904 == &s_phase, "...and the swap is restored");
    CHECK(s_phase.pCur == s_page && s_phase.iPage == 0,
          "the page loop published page 0");
    CHECK(FixCurrent() == 1, "the page actually ran: control 1 is current");
    CHECK(s_nPhF18 == 0 && s_n3E310 == 0 && s_n6A4A0 == 0,
          "no teardown on the success path");
    CHECK(s_scr.nAA2868 == 0,
          "0x10AA2868 is 0 while the current phase is not the root");
    FixFree();

    /* 0x10AA2868 is the one thing the frame writes about the phase SLOTS. */
    FixPhaseBuild();
    s_nav.pAA2908 = &s_phase;
    (void)s_phase.pVtbl->f0C(&s_phase);
    CHECK(s_scr.nAA2868 == 1,
          "0x10AA2868 is 1 when the current phase IS the root");
    FixFree();

    /* --- the +0x68 veto, which is the whole first half of the function --- */
    FixPhaseBuild();
    s_phase.f68 = 0;
    CHECK(s_phase.pVtbl->f0C(&s_phase) == 0, "+0x68 clear fails the frame");
    CHECK(s_n3E310 == 1 && s_n6A4A0 == 1 && s_nPhF18 == 1,
          "...and runs the two-call teardown, then vtable +0x18");
    CHECK(s_phase.iPage == 0, "the teardown zeroes +0x12");
    CHECK(s_nPhF04 == 0 && s_nPoll == 0,
          "nothing else runs: no +0x04, no poll");
    CHECK(FixCurrent() == -1000,
          "no control became current -- the page never ran");
    FixFree();

    /* --- the SECOND exit: +0x68 cleared during the page walk -------------- */
    FixPhaseBuild();
    s_page->pfn08 = FixPageTail;
    s_pageTailClearsF68 = 1;
    CHECK(s_phase.pVtbl->f0C(&s_phase) == 0,
          "a page that clears +0x68 fails the frame");
    CHECK(FixCurrent() == 1, "...but the page HAD already run");
    CHECK(s_nPhF08 == 1, "the tick still ran before the second +0x68 test");
    CHECK(s_n3E310 == 1 && s_n6A4A0 == 1 && s_nPhF18 == 1,
          "the same teardown runs on the second exit");
    FixFree();

    /* --- pCur is published BEFORE the NULL test --------------------------- */
    FixPhaseBuild();
    s_phase.pCur      = s_page;
    s_phase.aPages[0] = NULL;
    CHECK(s_phase.pVtbl->f0C(&s_phase) == 0, "a NULL page slot fails the frame");
    CHECK(s_phase.pCur == NULL,
          "pCur was written with the NULL page before the test");
    CHECK(s_nPhF08 == 0 && s_nPhF18 == 0,
          "the NULL exit is a bare return: no tick, no teardown");
    s_phase.aPages[0] = s_page;
    FixFree();

    /* --- a page whose parallel flag is clear is published but not run ----- */
    FixPhaseBuild();
    s_phase.aFlags[0] = 0;
    CHECK(s_phase.pVtbl->f0C(&s_phase) == 1, "a flag-clear page still succeeds");
    CHECK(s_phase.pCur == s_page, "...is still published in pCur");
    CHECK(FixCurrent() == -1000, "...and did not run");
    FixFree();
}

/* ==========================================================================
 * Stand-ins for the cross-module callees. HERE and nowhere else.
 * ========================================================================== */

int32_t BrSub10075020(void) { return 0; }
void    BrSub10072AF0(int a, int b) { (void)a; (void)b; }

/* slice3_39.o is linked for the style pool and the embedded list, and it in
 * turn wants these three. They are never reached from this suite. */
void    BrTextBoxDtor(BrTextBox *pBox) { (void)pBox; }
int32_t BrDikGetDeviceState(uint8_t *pState) { (void)pState; return 0; }
char    g_aBr39B720[0x104];

/* The two-call teardown 0x100489A0 runs on its failure exits: write the
 * options block, then write the config file. Both live in other packets, and
 * the phase-frame test below drives the exit that reaches them, so these
 * COUNT rather than merely satisfy the link. */
void BrSub1003E310(void)                    { ++s_n3E310; }
void BrSub1006A4A0(void *pThis, void *pArg) { (void)pThis; (void)pArg;
                                              ++s_n6A4A0; }

/* 0x10048710 and 0x1004F700 are only reached by BrUiNavHook_10045AF0, which
 * this suite does not exercise -- the host does, and that is where the
 * forward transition is observed. Declared so the module links alone. */
BrPhase_ *BrOptObjCtor(BrPhase_ *pThis) { return pThis; }
void      BrExt_1004F700(BrPhase_ *pSelf) { (void)pSelf; }

int main(void)
{
    TestSelectClamp();
    TestCurrentMark();
    TestMove();
    TestActivate();
    TestHookZeroStopsPage();
    TestActiveOverride();
    TestPhaseFrame();

    printf("uinav: %d checks, %d failures\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
