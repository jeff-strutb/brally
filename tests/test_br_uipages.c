/* test_br_uipages.c -- properties of Glide 0x10043050 (the quit confirmation)
 * and Glide 0x1004F290 (the multiplayer name screen).
 *
 * WHAT IS ASSERTED, AND WHY EACH ONE CAN FAIL
 *
 * Every expected value below was written from the DISASSEMBLY -- the listing
 * is reproduced address-by-address in br_uipages.c's comments and tabulated in
 * br_uipages.h -- and never from a banner or a comment.  Every assertion was
 * mutation-tested: the bug it guards was reinstated in br_uipages.c, the suite
 * rebuilt, and the failure observed.  The table is in the pass report.
 *
 * The stand-ins are faithful exactly where faithfulness is what makes an
 * assertion able to fail:
 *
 *   - StubF38 reproduces the two stores br_ui.h documents for 0x10047FB0 --
 *     `a7 -> aStepId[0]` and `a7 -> w1E20C` -- so that the later `w1E20C = 3`
 *     (and the label's `w1E20C = 0x34`) is distinguishable from a control that
 *     keeps its a7.  A stub that wrote nothing would make those pass under
 *     both readings.
 *   - BrOperatorNew POISONS (0xCC) rather than zeroing, because 0x1007DFE0
 *     does not zero and the constructors are what clear the block.
 *   - StubTextPfn04 only COUNTS.  The real 0x1005B0D0 measures the string; a
 *     stand-in that measured would be inventing font metrics this module has
 *     nothing to do with.
 *
 * WHAT IS DELIBERATELY *NOT* ASSERTED, so nobody adds a decoration:
 *
 *   - The re-read of phase->nPages between aFlags[] and aPages[].  Nothing can
 *     change the count in between (the page constructor never receives the
 *     phase), so an assertion could not fail.  Same reasoning as
 *     test_br_uiroot.c.
 *   - That the "Continue" control is published BEFORE cCtl is bumped.  The
 *     pointer published is the same object either way, so the ordering is
 *     unobservable from here.  WHICH control is published is asserted; WHEN is
 *     not, because it cannot fail.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "br_uipages.h"

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
 * The two buffers this module reaches through, owned elsewhere in the tree
 * ========================================================================== */

char  g_aBr39B720[0x104];          /* slice2_25.c owns the real storage     */
char *g_brPB4E2E8;                 /* slice4_50.c owns the real pointer     */
static char g_aNameBuf[0x104];     /* what the host would bind it to        */

/* ==========================================================================
 * Allocator and constructors
 * ========================================================================== */

static int g_cAlloc;
static int g_iAllocFail = -1;      /* 0-based allocation index that fails */

void *BrOperatorNew(uint32_t cb)
{
    void *p;
    if (g_cAlloc++ == g_iAllocFail) {
        return NULL;
    }
    p = malloc(cb);
    if (p != NULL) {
        memset(p, 0xCC, cb);       /* 0x1007DFE0 does NOT zero */
    }
    return p;
}

/* ==========================================================================
 * The vtables.  Only the control's +0x34 / +0x38 and the text box's +0x04 are
 * reached by either builder.
 * ========================================================================== */

typedef struct PlaceRec {
    const BrUiCtl_ *pCtl;
    const BrPhase_ *pOwner;
    float           x, y;
    int32_t         flags, a4, a5, a6, a7;
} PlaceRec;

typedef struct TextRec {
    const BrUiCtl_ *pCtl;
    const void     *pText;
    int32_t         a2, a3;
    const void     *pStyle;
} TextRec;

#define REC_MAX 32
static PlaceRec g_aPlace[REC_MAX];
static int      g_cPlace;
static TextRec  g_aTextRec[REC_MAX];
static int      g_cTextRec;

static void StubF38(BrUiCtl_ *pThis, BrPhase_ *pOwner, float x, float y,
                    int32_t flags, int32_t a4, int32_t a5,
                    int32_t a6, int32_t a7)
{
    if (g_cPlace < REC_MAX) {
        PlaceRec *p = &g_aPlace[g_cPlace];
        p->pCtl = pThis; p->pOwner = pOwner; p->x = x; p->y = y;
        p->flags = flags; p->a4 = a4; p->a5 = a5; p->a6 = a6; p->a7 = a7;
    }
    ++g_cPlace;

    /* Faithful to br_ui.h's description of 0x10047FB0. */
    pThis->pOwner      = pOwner;
    pThis->x           = x;
    pThis->y           = y;
    pThis->flags1C    |= flags;
    pThis->flags24    |= a4;
    pThis->flags28    |= a5;
    pThis->f2968       = a6;
    pThis->aStepId[0]  = (uint16_t)a7;
    pThis->w1E20C      = (uint16_t)a7;
}

static void StubF34(BrUiCtl_ *pThis, const void *pText, int32_t a2, int32_t a3,
                    const void *pStyle)
{
    if (g_cTextRec < REC_MAX) {
        TextRec *p = &g_aTextRec[g_cTextRec];
        p->pCtl = pThis; p->pText = pText; p->a2 = a2; p->a3 = a3;
        p->pStyle = pStyle;
    }
    ++g_cTextRec;
}

static int             g_cTextMeasure;
static const BrTextBox *g_pLastMeasured;

static void StubTextPfn04(BrTextBox *pThis)
{
    ++g_cTextMeasure;
    g_pLastMeasured = pThis;
}

static BrUiCtlVtbl_  g_ctlVtbl;
static BrUiPageVtbl_ g_pageVtbl;
static BrTextBoxVtbl g_textVtbl;

BrUiPage_ *BrUiPageCtor_10048470(BrUiPage_ *pThis)
{
    memset(pThis, 0, (size_t)BR_UI_PAGE_ALLOC_SIZE);
    pThis->pVtbl = &g_pageVtbl;
    return pThis;
}

BrUiCtl_ *BrUiCtlCtor(BrUiCtl_ *pThis)
{
    int i;
    memset(pThis, 0, (size_t)BR_UI_CTL_ALLOC_SIZE);
    pThis->pVtbl = &g_ctlVtbl;
    /* The real constructor leaves aStepId at -1 and flags1C at 1, and plants
     * 0x1008F728 in each text box's +0x00 (slice3_39.h). */
    for (i = 0; i < BR_UI_CTL_STEPS; ++i) {
        pThis->aStepId[i] = 0xFFFFu;
    }
    pThis->flags1C = 1;
    for (i = 0; i < BR_UI_CTL_TEXTS; ++i) {
        pThis->aText[i].pVtbl = &g_textVtbl;
        pThis->aText[i].f08   = 1;
    }
    return pThis;
}

/* ==========================================================================
 * BrStrGet and BrErrShow
 * ========================================================================== */

static int  g_aStrAsked[32];
static int  g_cStrAsked;
static char g_aszStr[32][24];

const char *BrStrGet(int id)
{
    int slot = g_cStrAsked % 32;
    if (g_cStrAsked < 32) {
        g_aStrAsked[g_cStrAsked] = id;
    }
    ++g_cStrAsked;
    sprintf(g_aszStr[slot], "str%d", id);
    return g_aszStr[slot];
}

static int g_cErr;
static int g_aErrIdx[8];

void BrErrShow(const BrErrHost *pHost, int32_t idx)
{
    (void)pHost;
    if (g_cErr < 8) {
        g_aErrIdx[g_cErr] = (int)idx;
    }
    ++g_cErr;
}

/* ==========================================================================
 * The context
 * ========================================================================== */

static int32_t Hook0C(BrUiCtl_ *p)   { (void)p; return 1; }
static int32_t HookQuitYes(BrUiCtl_ *p) { (void)p; return 1; }
static int32_t HookQuitBack(BrUiCtl_ *p) { (void)p; return 1; }
static int32_t HookMultiGo(BrUiCtl_ *p) { (void)p; return 1; }
static int32_t HookMultiBack(BrUiCtl_ *p) { (void)p; return 1; }
static int32_t HookEdit08(BrUiCtl_ *p) { (void)p; return 1; }
static int32_t HookEdit04(BrUiCtl_ *p) { (void)p; return 1; }
static int32_t HookEdit10(BrUiCtl_ *p) { (void)p; return 1; }

static BrUiPagesHooks g_hooks;
static BrErrHost      g_errHost;

/* Two DISTINCT objects, so a builder that routed a caption to the wrong style
 * is caught by pointer identity rather than by content. */
static const int32_t g_styleTitle[4] = { 100,  10, 410,  29 };  /* entry 15 */
static const int32_t g_styleRow[4]   = { 148, 110, 358, 260 };  /* entry  3 */

/* Where the multiplayer builder publishes its "Continue" control.  Seeded
 * with a sentinel so "never written" is distinguishable from "written NULL". */
static BrUiCtl_  g_sentinelCtl;
static BrUiCtl_ *g_pPublished;

static void SetUp(void)
{
    memset(&g_hooks, 0, sizeof(g_hooks));
    g_hooks.p100407B0 = Hook0C;
    g_hooks.p1003CC60 = HookQuitYes;
    g_hooks.p1003F940 = HookQuitBack;
    g_hooks.p1003D220 = HookMultiGo;
    g_hooks.p1003D510 = HookMultiBack;
    g_hooks.p1003BFF0 = HookEdit08;
    g_hooks.p10038420 = HookEdit04;
    g_hooks.p10038490 = HookEdit10;

    g_ctlVtbl.f34   = StubF34;
    g_ctlVtbl.f38   = StubF38;
    g_textVtbl.pfn04 = StubTextPfn04;

    g_pPublished = &g_sentinelCtl;

    memset(&g_brUiPages, 0, sizeof(g_brUiPages));
    g_brUiPages.pHooks        = &g_hooks;
    g_brUiPages.pErrHost      = &g_errHost;
    g_brUiPages.pStyleTitle   = g_styleTitle;
    g_brUiPages.pStyleRow     = g_styleRow;
    g_brUiPages.ppCtlContinue = &g_pPublished;

    strcpy(g_aNameBuf, "Ayrton");
    g_brPB4E2E8 = g_aNameBuf;
    memset(g_aBr39B720, 0, sizeof(g_aBr39B720));

    g_cAlloc = 0; g_iAllocFail = -1;
    g_cPlace = 0; g_cTextRec = 0; g_cStrAsked = 0; g_cErr = 0;
    g_cTextMeasure = 0; g_pLastMeasured = NULL;
    memset(g_aPlace,   0, sizeof(g_aPlace));
    memset(g_aTextRec, 0, sizeof(g_aTextRec));
}

/* A sentinel the builders must overwrite.  A calloc'd phase is already zero
 * everywhere, so "the builder wrote 0" and "the builder wrote nothing" are the
 * same observation unless the field is seeded first -- and two mutants
 * (aPages[] stored only on success, iPage never zeroed) survived a version of
 * this suite that did not seed. */
static BrUiPage_ *const SENTINEL_PAGE = (BrUiPage_ *)(void *)&g_ctlVtbl;
#define SENTINEL_IPAGE 7u

static BrPhase_ *NewPhase(void)
{
    BrPhase_ *p = (BrPhase_ *)calloc(1, (size_t)BR_PHASE_ALLOC_SIZE);
    if (p != NULL) {
        p->iPage    = SENTINEL_IPAGE;
        p->aPages[0] = SENTINEL_PAGE;
        p->aPages[1] = SENTINEL_PAGE;
    }
    return p;
}

/* Every control the page reports must exist before anything dereferences one.
 * Returns 0 (and reports) if any slot is NULL, so a transcription that stopped
 * storing into apCtl fails an assertion instead of faulting the suite. */
static int CtlsPresent(const BrUiPage_ *pScr, int n, const char *pszWho)
{
    int i, ok = 1;
    for (i = 0; i < n; ++i) {
        if (pScr->apCtl[i] == NULL) {
            printf("FAIL %s: apCtl[%d] is NULL\n", pszWho, i);
            ++g_cFail;
            ok = 0;
        }
    }
    return ok;
}

/* ==========================================================================
 * The expected pages, transcribed from the listings rather than from the port
 * ========================================================================== */

typedef struct Expect {
    float   x, y;
    int32_t flags;
    int32_t a6, a7;
} Expect;

/* Glide 0x10043050 -- 1004313D / 100431A4 / 1004323C / 100432EB */
static const Expect g_aQuit[BR_UIQUIT_CTL_COUNT] = {
    {   0.0f,   0.0f, 0x000009, 0,  0 },   /* backdrop    */
    { 195.0f,  10.0f, 0x100009, 1, -1 },   /* "Quit Game" */
    { 195.0f, 130.0f, 0x102001, 1, -1 },   /* "Yes, Quit" */
    { 195.0f, 244.0f, 0x102001, 1, -1 }    /* "Back"      */
};

/* Glide 0x1004F290 -- 1004F3CE / 1004F3E5 / 1004F47D / 1004F517 /
 *                     1004F588 / 1004F700 / 1004F7C2 / 1004F87A */
static const Expect g_aMulti[BR_UIMULTI_CTL_COUNT] = {
    {   0.0f,   0.0f, 0x000009, 0,    0 },  /* backdrop            */
    { 190.0f,  10.0f, 0x100009, 1,   -1 },  /* "Multi-Player Name" */
    { 190.0f, 130.0f, 0x100009, 1,   -1 },  /* "Player Name"       */
    { 156.0f, 172.0f, 0x000009, 0, 0x39 },  /* decoration          */
    { 190.0f, 174.0f, 0x200001, 1,   -1 },  /* the edit box        */
    { 190.0f, 225.0f, 0x102001, 1,   -1 },  /* "Continue"          */
    { 190.0f, 244.0f, 0x102001, 1,   -1 },  /* "Back"              */
    {  80.0f,  46.0f, 0x000009, 0,    7 }   /* decoration          */
};

static void CheckPlacements(const Expect *pExp, int n, const BrPhase_ *pPhase,
                            const char *pszWho)
{
    int i;
    for (i = 0; i < n; ++i) {
        const PlaceRec *p = &g_aPlace[i];
        char msg[96];
        sprintf(msg, "%s ctl %d placement", pszWho, i);
        CHECK(p->x == pExp[i].x, msg);
        CHECK(p->y == pExp[i].y, msg);
        CHECK(p->flags == pExp[i].flags, msg);
        CHECK(p->a6 == pExp[i].a6, msg);
        CHECK(p->a7 == pExp[i].a7, msg);
        /* Every f38 site in both builders passes 2 and 5, and the OWNER is
         * the PHASE, never the page. */
        CHECK(p->a4 == 2 && p->a5 == 5, msg);
        CHECK(p->pOwner == pPhase, msg);
    }
}

/* ==========================================================================
 * 1. The quit confirmation
 * ========================================================================== */

static void TestQuit(void)
{
    BrPhase_ *pPhase = NewPhase();
    int i;

    SetUp();
    BrUiQuitEnter_10043050(pPhase);

    CHECK(pPhase->nPages == 1, "quit: one page");
    CHECK(pPhase->iPage == 0, "quit: iPage zeroed");
    CHECK(pPhase->aFlags[0] == 1, "quit: aFlags[0] == 1");
    CHECK(pPhase->aPages[0] != NULL && pPhase->aPages[0] != SENTINEL_PAGE,
          "quit: page published");
    if (pPhase->aPages[0] == NULL || pPhase->aPages[0] == SENTINEL_PAGE) {
        free(pPhase); return;
    }
    if (!CtlsPresent(pPhase->aPages[0], BR_UIQUIT_CTL_COUNT, "quit")) {
        free(pPhase); return;
    }

    CHECK(pPhase->aPages[0]->pOwner == pPhase, "quit: page owner is the phase");
    CHECK(pPhase->aPages[0]->f10 == 0, "quit: page f10 zeroed");
    CHECK(pPhase->aPages[0]->fX == 195.0f, "quit: fX == 195.0 (0x43430000)");
    CHECK(pPhase->aPages[0]->fY == 130.0f, "quit: fY == 130.0 (0x43020000)");

    CHECK(pPhase->aPages[0]->cCtl == BR_UIQUIT_CTL_COUNT, "quit: 4 controls");
    CHECK(pPhase->aPages[0]->cSel == BR_UIQUIT_SEL_COUNT, "quit: 2 selectable");
    CHECK(g_cPlace == BR_UIQUIT_CTL_COUNT, "quit: 4 placements");
    CHECK(g_cErr == 0, "quit: no error reported");

    CheckPlacements(g_aQuit, BR_UIQUIT_CTL_COUNT, pPhase, "quit");

    /* Captions: three of the four controls get one, in this id order, and the
     * title takes a DIFFERENT style object from the two rows. */
    CHECK(g_cTextRec == 3, "quit: three captions");
    CHECK(g_cStrAsked == 3, "quit: three string lookups");
    CHECK(g_aStrAsked[0] == BR_UISTR_QUIT_TITLE, "quit: title id 0x0E");
    CHECK(g_aStrAsked[1] == BR_UISTR_QUIT_YES,   "quit: row 0 id 0x0F");
    CHECK(g_aStrAsked[2] == BR_UISTR_BACK,       "quit: row 1 id 0x0C");
    CHECK(g_aTextRec[0].pStyle == g_styleTitle, "quit: title style");
    CHECK(g_aTextRec[1].pStyle == g_styleRow,   "quit: row 0 style");
    CHECK(g_aTextRec[2].pStyle == g_styleRow,   "quit: row 1 style");
    for (i = 0; i < 3; ++i) {
        CHECK(g_aTextRec[i].a2 == 1 && g_aTextRec[i].a3 == 1,
              "quit: every caption is (1, 1)");
    }
    /* The captions go to apCtl[1], [2] and [3] -- not to the backdrop. */
    CHECK(g_aTextRec[0].pCtl == pPhase->aPages[0]->apCtl[1], "quit: caption 0");
    CHECK(g_aTextRec[1].pCtl == pPhase->aPages[0]->apCtl[2], "quit: caption 1");
    CHECK(g_aTextRec[2].pCtl == pPhase->aPages[0]->apCtl[3], "quit: caption 2");

    /* Hooks.  The backdrop gets none; the title gets none; the two rows share
     * +0x0C and differ in +0x08. */
    CHECK(pPhase->aPages[0]->apCtl[0]->pfn08 == NULL, "quit: backdrop no action");
    CHECK(pPhase->aPages[0]->apCtl[0]->pfn0C == NULL, "quit: backdrop no caption hook");
    CHECK(pPhase->aPages[0]->apCtl[1]->pfn08 == NULL, "quit: title no action");
    CHECK(pPhase->aPages[0]->apCtl[1]->pfn0C == NULL, "quit: title no caption hook");
    CHECK(pPhase->aPages[0]->apCtl[2]->pfn0C == Hook0C,      "quit: row 0 pfn0C");
    CHECK(pPhase->aPages[0]->apCtl[2]->pfn08 == HookQuitYes, "quit: row 0 pfn08");
    CHECK(pPhase->aPages[0]->apCtl[3]->pfn0C == Hook0C,      "quit: row 1 pfn0C");
    CHECK(pPhase->aPages[0]->apCtl[3]->pfn08 == HookQuitBack,"quit: row 1 pfn08");

    /* w1E20C: 3 on the three captioned controls, and the backdrop keeps the
     * a7 that f38 put there. */
    CHECK(pPhase->aPages[0]->apCtl[0]->w1E20C == 0, "quit: backdrop keeps a7");
    CHECK(pPhase->aPages[0]->apCtl[1]->w1E20C == 3, "quit: title w1E20C 3");
    CHECK(pPhase->aPages[0]->apCtl[2]->w1E20C == 3, "quit: row 0 w1E20C 3");
    CHECK(pPhase->aPages[0]->apCtl[3]->w1E20C == 3, "quit: row 1 w1E20C 3");

    /* Nothing on this screen touches a text box's own vtable. */
    CHECK(g_cTextMeasure == 0, "quit: no text box re-measured");

    free(pPhase);
}

/* ==========================================================================
 * 2. The multiplayer name screen
 * ========================================================================== */

static void TestMulti(void)
{
    BrPhase_  *pPhase = NewPhase();
    BrUiPage_ *pScr;
    BrUiCtl_  *pEdit;

    SetUp();
    BrUiMultiEnter_1004F290(pPhase);

    CHECK(pPhase->nPages == 1, "multi: one page");
    CHECK(pPhase->aFlags[0] == 1, "multi: aFlags[0] == 1");
    CHECK(pPhase->iPage == 0, "multi: iPage zeroed");
    pScr = pPhase->aPages[0];
    CHECK(pScr != NULL && pScr != SENTINEL_PAGE, "multi: page published");
    if (pScr == NULL || pScr == SENTINEL_PAGE) { free(pPhase); return; }
    if (!CtlsPresent(pScr, BR_UIMULTI_CTL_COUNT, "multi")) {
        free(pPhase); return;
    }

    /* THE distinguishing number: this builder's page origin is 190, not the
     * 195 every other builder in the corpus uses. */
    CHECK(pScr->fX == 190.0f, "multi: fX == 190.0 (0x433E0000)");
    CHECK(pScr->fY == 130.0f, "multi: fY == 130.0 (0x43020000)");

    CHECK(pScr->cCtl == BR_UIMULTI_CTL_COUNT, "multi: 8 controls");
    CHECK(pScr->cSel == BR_UIMULTI_SEL_COUNT, "multi: 3 selectable");
    CHECK(g_cPlace == BR_UIMULTI_CTL_COUNT, "multi: 8 placements");
    CHECK(g_cErr == 0, "multi: no error reported");

    CheckPlacements(g_aMulti, BR_UIMULTI_CTL_COUNT, pPhase, "multi");

    /* Five captions: title, label, the edit box's buffer, and the two rows. */
    CHECK(g_cTextRec == 5, "multi: five captions");
    CHECK(g_aStrAsked[0] == BR_UISTR_MULTI_TITLE,  "multi: title id 0x5C");
    CHECK(g_aStrAsked[1] == BR_UISTR_PLAYER_NAME,  "multi: label id 0x3C");
    CHECK(g_aStrAsked[2] == BR_UISTR_CONTINUE,     "multi: row 0 id 0x1E");
    CHECK(g_aStrAsked[3] == BR_UISTR_BACK,         "multi: row 1 id 0x0C");
    CHECK(g_cStrAsked == 4, "multi: four string lookups (the name is seeded)");

    CHECK(g_aTextRec[0].pStyle == g_styleTitle, "multi: title style");
    CHECK(g_aTextRec[1].pStyle == g_styleRow,   "multi: label style");
    CHECK(g_aTextRec[2].pStyle == g_styleRow,   "multi: edit style");
    CHECK(g_aTextRec[3].pStyle == g_styleRow,   "multi: row 0 style");
    CHECK(g_aTextRec[4].pStyle == g_styleRow,   "multi: row 1 style");

    /* The label is the ONE caption in either screen whose a3 is not 1. */
    CHECK(g_aTextRec[0].a2 == 1 && g_aTextRec[0].a3 == 1, "multi: title (1,1)");
    CHECK(g_aTextRec[1].a2 == 1 && g_aTextRec[1].a3 == 4, "multi: label (1,4)");
    CHECK(g_aTextRec[2].a2 == 1 && g_aTextRec[2].a3 == 1, "multi: edit (1,1)");
    CHECK(g_aTextRec[3].a2 == 1 && g_aTextRec[3].a3 == 1, "multi: row 0 (1,1)");
    CHECK(g_aTextRec[4].a2 == 1 && g_aTextRec[4].a3 == 1, "multi: row 1 (1,1)");

    /* ...and the ONE whose w1E20C is not 3.  0x34 is a different font sheet,
     * i.e. a different colour (CONVENTIONS.md). */
    CHECK(pScr->apCtl[1]->w1E20C == 3,    "multi: title w1E20C 3");
    CHECK(pScr->apCtl[2]->w1E20C == 0x34, "multi: label w1E20C 0x34");
    CHECK(pScr->apCtl[4]->w1E20C == 3,    "multi: edit w1E20C 3");
    CHECK(pScr->apCtl[5]->w1E20C == 3,    "multi: row 0 w1E20C 3");
    CHECK(pScr->apCtl[6]->w1E20C == 3,    "multi: row 1 w1E20C 3");
    /* The two decorations keep the a7 f38 put in w1E20C. */
    CHECK(pScr->apCtl[3]->w1E20C == 0x39, "multi: decoration keeps a7 0x39");
    CHECK(pScr->apCtl[7]->w1E20C == 7,    "multi: decoration keeps a7 7");

    /* The edit box's text is the SHARED BUFFER ITSELF, not a string-table
     * result -- pointer identity, which a string lookup could not satisfy. */
    CHECK(g_aTextRec[2].pText == (const void *)g_aBr39B720,
          "multi: the edit box is captioned with 0x1039B720 itself");

    /* Hooks. */
    pEdit = pScr->apCtl[4];
    CHECK(pEdit->pfn08 == HookEdit08, "multi: edit +0x08");
    CHECK(pEdit->pfn04 == HookEdit04, "multi: edit +0x04");
    CHECK(pEdit->pfn10 == HookEdit10, "multi: edit +0x10");
    CHECK(pEdit->pfn0C == NULL, "multi: edit has NO caption hook");
    CHECK(pScr->apCtl[5]->pfn0C == Hook0C,        "multi: row 0 pfn0C");
    CHECK(pScr->apCtl[5]->pfn08 == HookMultiGo,   "multi: row 0 pfn08");
    CHECK(pScr->apCtl[6]->pfn0C == Hook0C,        "multi: row 1 pfn0C");
    CHECK(pScr->apCtl[6]->pfn08 == HookMultiBack, "multi: row 1 pfn08");

    /* The edit box's rectangle, on the control AND on aText[0]. */
    CHECK(pEdit->rcLeft   == 0xBA,  "multi: rcLeft");
    CHECK(pEdit->rcTop    == 0xAC,  "multi: rcTop");
    CHECK(pEdit->rcRight  == 0x13C, "multi: rcRight");
    CHECK(pEdit->rcBottom == 0xBC,  "multi: rcBottom");
    CHECK(pEdit->aText[0].left  == 0xBA,  "multi: aText[0].left");
    CHECK(pEdit->aText[0].f428  == 0xAC,  "multi: aText[0].f428 (top)");
    CHECK(pEdit->aText[0].right == 0x13C, "multi: aText[0].right");
    CHECK(pEdit->aText[0].f430  == 0xBC,  "multi: aText[0].f430 (bottom)");
    CHECK(pEdit->aText[0].f41C  == 0x72,  "multi: f41C == right-left-0x10");

    /* Only aText[0] is touched: the other two boxes keep their poison-free
     * constructed state.  A transcription that wrote aText[1] instead would
     * pass every rectangle check above if this were not here. */
    CHECK(pEdit->aText[1].left == 0 && pEdit->aText[1].right == 0,
          "multi: aText[1] untouched");

    /* The name went into the box, and the box was re-measured exactly once
     * through its OWN vtable. */
    CHECK(strcmp(pEdit->aText[0].sz, "Ayrton") == 0,
          "multi: the saved name is in the box");
    CHECK(g_cTextMeasure == 1, "multi: aText[0] re-measured once");
    CHECK(g_pLastMeasured == &pEdit->aText[0], "multi: ...and it was aText[0]");

    /* The published control is the "Continue" row, not the next slot. */
    CHECK(g_pPublished == pScr->apCtl[5], "multi: 0x10AC5D00 names apCtl[5]");

    free(pPhase);
}

/* ==========================================================================
 * 3. The name seeding, at its boundary
 *
 * `cmp ecx, 1 / ja` is UNSIGNED on the strlen, so lengths 0 and 1 take the
 * copy and 2 does not.  Three cases, because a >= / > slip shows up only at
 * the boundary.
 * ========================================================================== */

static void TestNameSeed(const char *pszStart, const char *pszWant,
                         int fLookupExpected, const char *pszWho)
{
    BrPhase_ *pPhase = NewPhase();

    SetUp();
    strcpy(g_aNameBuf, pszStart);
    BrUiMultiEnter_1004F290(pPhase);

    CHECK(strcmp(g_brPB4E2E8, pszWant) == 0, pszWho);
    /* Four ids for the four captions, plus 0xC0 only when the seed ran. */
    CHECK(g_cStrAsked == 4 + fLookupExpected, pszWho);
    if (fLookupExpected) {
        CHECK(g_aStrAsked[2] == BR_UISTR_DEFAULT_NAME,
              "seed: the default name is id 0xC0");
    }
    if (pPhase->aPages[0] != NULL && pPhase->aPages[0]->apCtl[4] != NULL) {
        CHECK(strcmp(pPhase->aPages[0]->apCtl[4]->aText[0].sz, pszWant) == 0,
              pszWho);
    }
    free(pPhase);
}

static void TestNameSeeding(void)
{
    /* BrStrGet's stand-in returns "str192" for 0xC0. */
    TestNameSeed("",   "str192", 1, "seed: empty name is replaced");
    TestNameSeed("X",  "str192", 1, "seed: one-character name is replaced");
    TestNameSeed("XY", "XY",     0, "seed: two characters is left alone");
}

/* ==========================================================================
 * 4. Refusals -- the frontier must be a refusal, never a half-built screen
 * ========================================================================== */

static void TestRefusals(void)
{
    BrPhase_ *pPhase;

    /* No hooks: both builders refuse and nothing is allocated. */
    pPhase = NewPhase();
    SetUp();
    g_brUiPages.pHooks = NULL;
    BrUiQuitEnter_10043050(pPhase);
    BrUiMultiEnter_1004F290(pPhase);
    CHECK(pPhase->nPages == 0 && g_cAlloc == 0, "refuse: no hooks");
    free(pPhase);

    /* No row style: both refuse. */
    pPhase = NewPhase();
    SetUp();
    g_brUiPages.pStyleRow = NULL;
    BrUiQuitEnter_10043050(pPhase);
    BrUiMultiEnter_1004F290(pPhase);
    CHECK(pPhase->nPages == 0 && g_cAlloc == 0, "refuse: no row style");
    free(pPhase);

    /* No published-control slot: the MULTIPLAYER builder refuses and the quit
     * builder does not, because the quit screen never publishes anything. */
    pPhase = NewPhase();
    SetUp();
    g_brUiPages.ppCtlContinue = NULL;
    BrUiMultiEnter_1004F290(pPhase);
    CHECK(pPhase->nPages == 0 && g_cAlloc == 0,
          "refuse: multi needs the 0x10AC5D00 slot");
    BrUiQuitEnter_10043050(pPhase);
    CHECK(pPhase->nPages == 1, "refuse: quit does NOT need it");
    free(pPhase);

    /* No name buffer: same split. */
    pPhase = NewPhase();
    SetUp();
    g_brPB4E2E8 = NULL;
    BrUiMultiEnter_1004F290(pPhase);
    CHECK(pPhase->nPages == 0 && g_cAlloc == 0,
          "refuse: multi needs the name buffer");
    BrUiQuitEnter_10043050(pPhase);
    CHECK(pPhase->nPages == 1, "refuse: quit does NOT need it");
    free(pPhase);
}

/* ==========================================================================
 * 5. Allocation failure -- the original's order is visible here and only here
 * ========================================================================== */

static void TestAllocFailure(void)
{
    BrPhase_ *pPhase;

    /* The PAGE fails.  aFlags[] is written from the count read BEFORE the
     * allocation, aPages[] is written with the NULL, the error is reported,
     * and the count is bumped anyway. */
    pPhase = NewPhase();
    SetUp();
    g_iAllocFail = 0;
    BrUiQuitEnter_10043050(pPhase);
    CHECK(pPhase->aFlags[0] == 1, "pagefail: aFlags[0] written first");
    /* The slot was seeded with SENTINEL_PAGE, so this distinguishes "stored
     * the NULL" from "never stored". */
    CHECK(pPhase->aPages[0] == NULL, "pagefail: NULL stored before the test");
    CHECK(pPhase->nPages == 1, "pagefail: count bumped anyway");
    /* The literal `push 4` at 1004F306, not the module's own macro -- a test
     * that read the macro would move with a mutation of it. */
    CHECK(g_cErr == 1 && g_aErrIdx[0] == 4, "pagefail: error index 4");
    CHECK(g_cPlace == 0, "pagefail: no control placed");
    free(pPhase);

    /* The THIRD control fails (allocation index 3: page, ctl0, ctl1, ctl2).
     * apCtl[2] is NULL, cCtl still names it, and index 4 is reported. */
    pPhase = NewPhase();
    SetUp();
    g_iAllocFail = 3;
    BrUiQuitEnter_10043050(pPhase);
    CHECK(pPhase->aPages[0] != NULL && pPhase->aPages[0] != SENTINEL_PAGE,
          "ctlfail: the page still built");
    if (pPhase->aPages[0] != NULL && pPhase->aPages[0] != SENTINEL_PAGE) {
        CHECK(pPhase->aPages[0]->apCtl[2] == NULL,
              "ctlfail: the NULL is stored into apCtl");
        CHECK(pPhase->aPages[0]->cCtl == 2, "ctlfail: cCtl not bumped");
        CHECK(pPhase->aPages[0]->cSel == 0, "ctlfail: cSel not bumped");
    }
    CHECK(g_cErr == 1 && g_aErrIdx[0] == 4, "ctlfail: error index 4");
    CHECK(g_cPlace == 2, "ctlfail: only the first two placed");
    free(pPhase);
}

/* ==========================================================================
 * 6. Two phases in a row -- the builders own no state between calls
 * ========================================================================== */

static void TestTwice(void)
{
    BrPhase_ *pPhase = NewPhase();

    SetUp();
    BrUiQuitEnter_10043050(pPhase);
    /* Disturbed between the two builds, so "re-zeroed every time" is a claim
     * that can fail rather than one the first call already satisfied. */
    pPhase->iPage = 5;
    BrUiQuitEnter_10043050(pPhase);

    CHECK(pPhase->nPages == 2, "twice: two pages on one phase");
    CHECK(pPhase->iPage == 0, "twice: iPage re-zeroed");
    CHECK(pPhase->aFlags[1] == 1, "twice: aFlags[1] == 1");
    CHECK(pPhase->aPages[1] != NULL && pPhase->aPages[1] != SENTINEL_PAGE,
          "twice: the second page was published");
    if (pPhase->aPages[1] != NULL && pPhase->aPages[1] != SENTINEL_PAGE) {
        CHECK(pPhase->aPages[1]->cCtl == BR_UIQUIT_CTL_COUNT,
              "twice: the second page is complete too");
    }
    free(pPhase);
}

int main(void)
{
    TestQuit();
    TestMulti();
    TestNameSeeding();
    TestRefusals();
    TestAllocFailure();
    TestTwice();

    printf("test_br_uipages: %d failures\n", g_cFail);
    return g_cFail != 0;
}
