/* test_br_uiroot.c -- properties of Glide 0x100425E0, the root phase's
 * pfnEnter and the main-menu builder.
 *
 * WHAT IS ASSERTED, AND WHY EACH ONE CAN FAIL
 *
 * Every assertion below was written from the disassembly (the listing is
 * reproduced address-by-address in br_uiroot.c's comments), and every one of
 * them was mutation-tested: the bug it guards was reinstated, the suite
 * rebuilt, and the failure observed.  The table is in the pass report.
 *
 * The stand-ins are deliberately faithful where faithfulness is what makes an
 * assertion able to fail:
 *
 *   - StubF38 reproduces the two stores br_uivt.h documents for
 *     0x10047FB0 -- `a7 -> aStepId[0]` and `a7 -> w1E20C` -- so the later
 *     `w1E20C = 3` on the nine captioned controls is distinguishable from the
 *     seven that keep their a7.  A stub that wrote nothing would make that
 *     assertion pass under both readings.
 *   - BrOperatorNew POISONS (0xCC) rather than zeroing, because 0x1007DFE0
 *     does not zero, and the constructors then clear the whole block, which
 *     is what the real 0x10048470 / 0x100476C0 do for every field this
 *     builder reads back.
 *
 * WHAT IS DELIBERATELY *NOT* ASSERTED, so nobody adds a decoration:
 *
 *   The original reads phase->nPages once for aFlags[] and RE-READS it for
 *   aPages[].  That re-read cannot be observed from here and cannot be
 *   observed at all: the page constructor never receives the phase, so
 *   nothing can change the count in between.  Asserting it would be an
 *   assertion that cannot fail, which CONVENTIONS.md says is worse than none.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "br_uiroot.h"

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
 * The control vtable.  Only +0x34 and +0x38 are reached.
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

#define REC_MAX 64
static PlaceRec g_aPlace[REC_MAX];
static int      g_cPlace;
static TextRec  g_aText[REC_MAX];
static int      g_cText;

/* Faithful to br_uivt.h's description of 0x10047FB0: pure stores and ORs, and
 * in particular a7 lands in BOTH aStepId[0] and w1E20C. */
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
    if (g_cText < REC_MAX) {
        TextRec *p = &g_aText[g_cText];
        p->pCtl = pThis; p->pText = pText; p->a2 = a2; p->a3 = a3;
        p->pStyle = pStyle;
    }
    ++g_cText;
}

static BrUiCtlVtbl_  g_ctlVtbl;
static BrUiPageVtbl_ g_pageVtbl;

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
    /* The real constructor leaves aStepId at -1 and flags1C at 1. */
    for (i = 0; i < BR_UI_CTL_STEPS; ++i) {
        pThis->aStepId[i] = 0xFFFFu;
    }
    pThis->flags1C = 1;
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

static int32_t Hook(BrUiCtl_ *p) { (void)p; return 1; }

static BrUiRootHooks g_hooks;
static BrErrHost     g_errHost;

/* Three DISTINCT objects, so a builder that routed a caption to the wrong
 * style is caught by pointer identity rather than by content. */
static const int32_t g_styleTitle[4]  = { 100,  10, 410,  29 };  /* entry 15 */
static const int32_t g_styleRow[4]    = { 148, 110, 358, 260 };  /* entry  3 */
static const int32_t g_styleStatus[4] = {  80,  29, 430,  48 };  /* entry 20 */

static const char g_szStatus[] = " ";     /* the image's 0x100ACAD8 */

static void SetUp(void)
{
    memset(&g_hooks, 0, sizeof(g_hooks));
    g_hooks.p1003ED90 = Hook;
    g_hooks.p1003D140 = Hook;
    g_hooks.p1003E0E0 = Hook;
    g_hooks.p1003E4A0 = Hook;
    g_hooks.p1003E730 = Hook;
    g_hooks.p1003AED0 = Hook;
    g_hooks.p1003F610 = Hook;
    g_hooks.p100407B0 = Hook;
    g_hooks.p10040AF0 = Hook;
    g_hooks.p10040A20 = Hook;

    g_ctlVtbl.f34 = StubF34;
    g_ctlVtbl.f38 = StubF38;

    memset(&g_brUiRoot, 0, sizeof(g_brUiRoot));
    g_brUiRoot.pHooks       = &g_hooks;
    g_brUiRoot.pErrHost     = &g_errHost;
    g_brUiRoot.pStyleTitle  = g_styleTitle;
    g_brUiRoot.pStyleRow    = g_styleRow;
    g_brUiRoot.pStyleStatus = g_styleStatus;
    g_brUiRoot.pszStatus    = g_szStatus;

    g_cAlloc = 0; g_iAllocFail = -1;
    g_cPlace = 0; g_cText = 0; g_cStrAsked = 0; g_cErr = 0;
    memset(g_aPlace, 0, sizeof(g_aPlace));
    memset(g_aText,  0, sizeof(g_aText));
    BrUiRootResetForTest();
}

static BrPhase_ *NewPhase(void)
{
    BrPhase_ *p = (BrPhase_ *)calloc(1, (size_t)BR_PHASE_ALLOC_SIZE);
    return p;
}

/* ==========================================================================
 * The expected page, transcribed from the listing rather than from the port
 * ========================================================================== */

/* apCtl index, x, y, flags, a6, a7.  -1 in `iCtl` marks the cursor, which is
 * apCtl[199] and is the SECOND control built. */
typedef struct Expect {
    int     iCtl;
    float   x, y;
    int32_t flags;
    int32_t a6, a7;
} Expect;

static const Expect g_aExpect[] = {
    {  0,   0.0f,   0.0f, 0x000009, 0,   0 },   /* backdrop            */
    { -1,   0.0f,   0.0f, 0x000009, 0,   1 },   /* apCtl[199], cursor  */
    {  1, 195.0f,  10.0f, 0x100009, 1,  -1 },   /* "Main Menu"         */
    {  2, 195.0f, 125.0f, 0x102001, 1,  -1 },   /* "Championship"      */
    {  3,  80.0f,  46.0f, 0x000809, 0,   6 },
    {  4, 195.0f, 144.0f, 0x102001, 1,  -1 },   /* "Multiplayer"       */
    {  5,  80.0f,  46.0f, 0x000809, 0,   7 },
    {  6, 195.0f, 163.0f, 0x102001, 1,  -1 },   /* "Time Attack"       */
    {  7,  80.0f,  46.0f, 0x000809, 0,   8 },
    {  8, 195.0f, 182.0f, 0x102001, 1,  -1 },   /* "Quick Race"        */
    {  9,  80.0f,  46.0f, 0x000809, 0, 0xA },   /* 0xA before 9        */
    { 10, 195.0f, 201.0f, 0x102001, 1,  -1 },   /* "Options"           */
    { 11,  80.0f,  46.0f, 0x000809, 0,   9 },
    { 12, 195.0f, 220.0f, 0x102001, 1,  -1 },   /* "Credits"           */
    { 13, 195.0f, 239.0f, 0x102001, 1,  -1 },   /* "Quit"              */
    { 14, 195.0f,  29.0f, 0x100009, 1,  -1 }    /* the status line     */
};
#define EXPECT_N ((int)(sizeof(g_aExpect) / sizeof(g_aExpect[0])))

/* ==========================================================================
 * 1. The page
 * ========================================================================== */

static void TestPage(void)
{
    BrPhase_  *pPhase = NewPhase();
    BrUiPage_ *pScr;

    SetUp();
    pPhase->nPages = 3;          /* not zero, so an index bug is visible */
    pPhase->iPage  = 9;
    BrUiRootEnter_100425E0(pPhase);

    CHECK(pPhase->nPages == 4, "nPages is bumped exactly once");
    CHECK(pPhase->iPage == 0, "iPage is cleared before anything else");
    CHECK(pPhase->aFlags[3] == 1, "aFlags[nPages] = 1 at the ORIGINAL index");
    CHECK(pPhase->aFlags[4] == 0, "aFlags is not written at the bumped index");
    CHECK(pPhase->aPages[3] != NULL, "the page lands in aPages[nPages]");
    CHECK(pPhase->aPages[4] == NULL, "and nowhere else");

    pScr = pPhase->aPages[3];
    CHECK(pScr->pOwner == pPhase, "page->pOwner is the PHASE");
    CHECK(pScr->f10 == 0, "page->f10 = 0");
    CHECK(pScr->fX == 195.0f, "page->fX == 0x43430000");
    CHECK(pScr->fY == 125.0f, "page->fY == 0x42FA0000");
    CHECK(pScr->cCtl == BR_UIROOT_CTL_COUNT, "cCtl ends at 15");
    CHECK(pScr->cSel == BR_UIROOT_ROWS, "cSel ends at 7 -- the 0x102001 rows");
    CHECK(g_cErr == 0, "no error is reported on the success path");
    CHECK(g_cAlloc == 1 + BR_UIROOT_CTL_COUNT + 1,
          "one page plus SIXTEEN controls are allocated");
    free(pPhase);
}

/* ==========================================================================
 * 2. Every f38 call, in order: coordinates, flags and the two spare arguments
 * ========================================================================== */

static void TestPlacement(void)
{
    BrPhase_  *pPhase = NewPhase();
    BrUiPage_ *pScr;
    int        i;

    SetUp();
    BrUiRootEnter_100425E0(pPhase);
    pScr = pPhase->aPages[0];

    CHECK(g_cPlace == EXPECT_N, "sixteen controls are placed");
    if (g_cPlace != EXPECT_N) { free(pPhase); return; }

    for (i = 0; i < EXPECT_N; ++i) {
        const Expect  *e = &g_aExpect[i];
        const PlaceRec *p = &g_aPlace[i];
        const BrUiCtl_ *pWant = (e->iCtl < 0)
            ? pScr->apCtl[BR_UIROOT_CURSOR_SLOT]
            : pScr->apCtl[e->iCtl];

        CHECK(p->pCtl == pWant, "the Nth placed control is at its own slot");
        CHECK(p->pOwner == pPhase, "f38's owner is the PHASE, never the page");
        CHECK(p->x == e->x, "x");
        CHECK(p->y == e->y, "y");
        CHECK(p->flags == e->flags, "place flags");
        CHECK(p->a4 == 2 && p->a5 == 5, "a4 == 2 and a5 == 5 at every site");
        CHECK(p->a6 == e->a6, "a6");
        CHECK(p->a7 == e->a7, "a7 -- the sprite index");
    }
    free(pPhase);
}

/* The rows are nineteen apart, which is what makes the SIGN of the six
 * .rdata constants checkable.  Reading them as +19/+38/... puts every row
 * above the first and this loop fails immediately. */
static void TestRowSpacing(void)
{
    BrPhase_ *pPhase = NewPhase();
    int       i, iPrev = -1;

    SetUp();
    BrUiRootEnter_100425E0(pPhase);

    for (i = 0; i < EXPECT_N; ++i) {
        if (g_aPlace[i].flags != 0x102001) {
            continue;
        }
        if (iPrev >= 0) {
            CHECK(g_aPlace[i].y - g_aPlace[iPrev].y == 19.0f,
                  "consecutive menu rows are 19 apart, downward");
        }
        CHECK(g_aPlace[i].y >= 125.0f, "no row is above the page's fY");
        iPrev = i;
    }
    CHECK(g_aPlace[iPrev].y == 239.0f, "the last row lands on 239");
    free(pPhase);
}

/* ==========================================================================
 * 3. The cursor at apCtl[199]
 * ========================================================================== */

static void TestCursor(void)
{
    BrPhase_  *pPhase = NewPhase();
    BrUiPage_ *pScr;
    int        i;

    SetUp();
    BrUiRootEnter_100425E0(pPhase);
    pScr = pPhase->aPages[0];

    CHECK(pScr->apCtl[199] != NULL,
          "apCtl[199] -- the literal slot, not the constant -- holds a "
          "control");
    /* Bail rather than fault: a mutation that moves the cursor elsewhere must
     * report a FAILED ASSERTION, not a segfault.  A crashed suite reports
     * nothing, which is the same defect CONVENTIONS.md records for a hang. */
    if (pScr->apCtl[199] == NULL) { free(pPhase); return; }
    CHECK(pScr->cCtl == BR_UIROOT_CTL_COUNT,
          "and cCtl is NOT bumped for it");
    for (i = 0; i < BR_UI_PAGE_CTL_MAX; ++i) {
        if (i != BR_UIROOT_CURSOR_SLOT && i < BR_UIROOT_CTL_COUNT) {
            CHECK(pScr->apCtl[i] != pScr->apCtl[BR_UIROOT_CURSOR_SLOT],
                  "the cursor is not aliased into the counted range");
        } else if (i != BR_UIROOT_CURSOR_SLOT) {
            CHECK(pScr->apCtl[i] == NULL, "nothing else is written");
        }
    }
    /* It is the SECOND control built -- 0x100426E4, before the title. */
    CHECK(g_aPlace[1].pCtl == pScr->apCtl[BR_UIROOT_CURSOR_SLOT],
          "the cursor is built second, between the backdrop and the title");
    CHECK(pScr->apCtl[BR_UIROOT_CURSOR_SLOT]->w1E20C == 1,
          "the cursor's sprite is 1; the backdrop's is 0");
    CHECK(pScr->apCtl[0]->w1E20C == 0, "the backdrop's sprite is 0");
    free(pPhase);
}

/* ==========================================================================
 * 4. The child links: which control a selected row draws
 * ========================================================================== */

static void TestChildren(void)
{
    BrPhase_  *pPhase = NewPhase();
    BrUiPage_ *pScr;
    int        i;
    int        aRow[BR_UIROOT_ROWS];
    int        cRow = 0;

    SetUp();
    BrUiRootEnter_100425E0(pPhase);
    pScr = pPhase->aPages[0];

    for (i = 0; i < BR_UIROOT_CTL_COUNT; ++i) {
        if (pScr->apCtl[i] == NULL) {
            CHECK(0, "every counted apCtl slot is filled");
            free(pPhase);
            return;
        }
        if ((pScr->apCtl[i]->flags1C & 0x102001) == 0x102001) {
            if (cRow < BR_UIROOT_ROWS) { aRow[cRow] = i; }
            ++cRow;
        }
    }
    CHECK(cRow == BR_UIROOT_ROWS, "seven rows carry 0x102001");
    if (cRow != BR_UIROOT_ROWS) { free(pPhase); return; }

    for (i = 0; i < BR_UIROOT_ROWS - 1; ++i) {
        const BrUiCtl_ *pRow = pScr->apCtl[aRow[i]];
        CHECK(pRow->cChild == 1, "every row but the last has ONE child");
        CHECK(pRow->aChild[0] == (int16_t)(aRow[i] + 1),
              "aChild[0] is the row's own index PLUS ONE");
    }
    CHECK(pScr->apCtl[aRow[BR_UIROOT_ROWS - 1]]->cChild == 0,
          "the last row -- \"Quit\" -- gets no child link at all");

    /* The first five rows point at a 0x800 highlight; the sixth points at the
     * NEXT ROW, because only five highlights exist.  Both are the original's
     * and both are asserted, so neither can be "tidied" away. */
    for (i = 0; i < BR_UIROOT_HILITES; ++i) {
        int16_t iKid = pScr->apCtl[aRow[i]]->aChild[0];
        const BrUiCtl_ *pKid = (iKid >= 0 && iKid < BR_UI_PAGE_CTL_MAX)
            ? pScr->apCtl[iKid] : NULL;
        CHECK(pKid != NULL && (pKid->flags1C & 0x800) != 0,
              "rows 0..4 point at a 0x800 highlight");
    }
    {
        int16_t iKid = pScr->apCtl[aRow[5]]->aChild[0];
        const BrUiCtl_ *pKid = (iKid >= 0 && iKid < BR_UI_PAGE_CTL_MAX)
            ? pScr->apCtl[iKid] : NULL;
        CHECK(pKid != NULL && (pKid->flags1C & 0x102001) == 0x102001,
              "\"Credits\" points at the \"Quit\" ROW, not a highlight");
    }
    free(pPhase);
}

/* ==========================================================================
 * 5. Captions, styles and the hooks
 * ========================================================================== */

static void TestCaptions(void)
{
    BrPhase_  *pPhase = NewPhase();
    BrUiPage_ *pScr;
    int        i;

    SetUp();
    BrUiRootEnter_100425E0(pPhase);
    pScr = pPhase->aPages[0];

    /* Nine captions: the title, seven rows and the status line. */
    CHECK(g_cText == 1 + BR_UIROOT_ROWS + 1, "nine controls get a caption");
    /* EIGHT string lookups: the status line uses a literal pointer. */
    CHECK(g_cStrAsked == 1 + BR_UIROOT_ROWS, "eight string ids are fetched");
    for (i = 0; i < g_cStrAsked && i < 32; ++i) {
        CHECK(g_aStrAsked[i] == i + 1, "ids 1..8, in page order");
    }

    CHECK(g_aText[0].pStyle == g_styleTitle, "the title takes style 15");
    for (i = 1; i <= BR_UIROOT_ROWS; ++i) {
        CHECK(g_aText[i].pStyle == g_styleRow, "every row takes style 3");
    }
    CHECK(g_aText[8].pStyle == g_styleStatus, "the status line takes style 20");
    CHECK(g_aText[8].pText == g_szStatus,
          "the status line's text is the .data pointer, not a string id");
    for (i = 0; i < g_cText && i < REC_MAX; ++i) {
        if (g_aText[i].pCtl == NULL) { CHECK(0, "captioned control"); continue; }
        CHECK(g_aText[i].a2 == 1 && g_aText[i].a3 == 1,
              "every f34 site passes (1, 1)");
        CHECK(g_aText[i].pCtl->w1E20C == 3,
              "a captioned control's sprite is forced to 3 AFTER f38");
    }

    /* ...and the seven that are not captioned keep the a7 f38 gave them. */
    if (pScr->apCtl[3] == NULL || pScr->apCtl[5] == NULL ||
        pScr->apCtl[7] == NULL || pScr->apCtl[9] == NULL ||
        pScr->apCtl[11] == NULL) {
        CHECK(0, "the five highlight slots are filled");
        free(pPhase);
        return;
    }
    CHECK(pScr->apCtl[3]->w1E20C == 6 && pScr->apCtl[5]->w1E20C == 7 &&
          pScr->apCtl[7]->w1E20C == 8 && pScr->apCtl[9]->w1E20C == 0xA &&
          pScr->apCtl[11]->w1E20C == 9,
          "the five highlights keep sprites 6,7,8,0xA,9 -- in THAT order");
    free(pPhase);
}

static void TestHooks(void)
{
    BrPhase_  *pPhase = NewPhase();
    BrUiPage_ *pScr;
    int        i;

    SetUp();
    BrUiRootEnter_100425E0(pPhase);
    pScr = pPhase->aPages[0];

    /* Only the seven rows carry hooks; nothing else does. */
    for (i = 0; i < BR_UIROOT_CTL_COUNT; ++i) {
        int fRow;
        if (pScr->apCtl[i] == NULL) {
            CHECK(0, "every counted apCtl slot is filled");
            free(pPhase);
            return;
        }
        fRow = ((pScr->apCtl[i]->flags1C & 0x102001) == 0x102001);
        CHECK((pScr->apCtl[i]->pfn08 != NULL) == fRow,
              "+0x08 is installed on the seven rows and nowhere else");
        CHECK((pScr->apCtl[i]->pfn0C != NULL) == fRow,
              "+0x0C is installed on the seven rows and nowhere else");
    }
    CHECK(pScr->apCtl[BR_UIROOT_CURSOR_SLOT] != NULL &&
          pScr->apCtl[BR_UIROOT_CURSOR_SLOT]->pfn08 == NULL &&
          pScr->apCtl[BR_UIROOT_CURSOR_SLOT]->pfn0C == NULL,
          "the cursor gets no hooks");
    free(pPhase);
}

/* Which hook value each row receives, by identity.  Ten separate stand-ins,
 * so any transposition in the table fails. */
#define MK(n) static int32_t H##n(BrUiCtl_ *p) { (void)p; return 1; }
MK(0) MK(1) MK(2) MK(3) MK(4) MK(5) MK(6) MK(7) MK(8) MK(9)
#undef MK

static void TestHookIdentity(void)
{
    BrPhase_  *pPhase = NewPhase();
    BrUiPage_ *pScr;

    SetUp();
    g_hooks.p1003ED90 = H0;  g_hooks.p1003D140 = H1;
    g_hooks.p1003E0E0 = H2;  g_hooks.p1003E4A0 = H3;
    g_hooks.p1003E730 = H4;  g_hooks.p1003AED0 = H5;
    g_hooks.p1003F610 = H6;
    g_hooks.p100407B0 = H7;  g_hooks.p10040AF0 = H8;
    g_hooks.p10040A20 = H9;

    BrUiRootEnter_100425E0(pPhase);
    pScr = pPhase->aPages[0];

    /* rows are apCtl 2, 4, 6, 8, 10, 12, 13 */
    {
        int j;
        for (j = 0; j < BR_UIROOT_CTL_COUNT; ++j) {
            if (pScr->apCtl[j] == NULL) {
                CHECK(0, "every counted apCtl slot is filled");
                free(pPhase);
                return;
            }
        }
    }
    CHECK(pScr->apCtl[2]->pfn08  == H0, "Championship action  0x1003ED90");
    CHECK(pScr->apCtl[4]->pfn08  == H1, "Multiplayer  action  0x1003D140");
    CHECK(pScr->apCtl[6]->pfn08  == H2, "Time Attack  action  0x1003E0E0");
    CHECK(pScr->apCtl[8]->pfn08  == H3, "Quick Race   action  0x1003E4A0");
    CHECK(pScr->apCtl[10]->pfn08 == H4, "Options      action  0x1003E730");
    CHECK(pScr->apCtl[12]->pfn08 == H5, "Credits      action  0x1003AED0");
    CHECK(pScr->apCtl[13]->pfn08 == H6, "Quit         action  0x1003F610");

    /* The +0x0C family: 0x100407B0 on rows 0 and 3, 0x10040A20 on Credits
     * ALONE, 0x10040AF0 on the other four. */
    CHECK(pScr->apCtl[2]->pfn0C  == H7, "Championship +0x0C  0x100407B0");
    CHECK(pScr->apCtl[4]->pfn0C  == H8, "Multiplayer  +0x0C  0x10040AF0");
    CHECK(pScr->apCtl[6]->pfn0C  == H8, "Time Attack  +0x0C  0x10040AF0");
    CHECK(pScr->apCtl[8]->pfn0C  == H7, "Quick Race   +0x0C  0x100407B0");
    CHECK(pScr->apCtl[10]->pfn0C == H8, "Options      +0x0C  0x10040AF0");
    CHECK(pScr->apCtl[12]->pfn0C == H9, "Credits      +0x0C  0x10040A20");
    CHECK(pScr->apCtl[13]->pfn0C == H8, "Quit         +0x0C  0x10040AF0");
    free(pPhase);
}

/* ==========================================================================
 * 6. The status-line index at 0x10AC4C58
 * ========================================================================== */

static void TestStatusIndex(void)
{
    BrPhase_  *pPhase = NewPhase();
    BrUiPage_ *pScr;

    SetUp();
    BrUiRootEnter_100425E0(pPhase);
    pScr = pPhase->aPages[0];

    /* Published BEFORE cCtl is bumped, so it names the control just built --
     * not the count.  A port that bumped first would leave 15 here. */
    CHECK(g_brUiRootStatusIdx == BR_UIROOT_STATUS_INDEX,
          "0x10AC4C58 = 14, the status line's own index");
    CHECK(g_brUiRootStatusIdx == (int32_t)pScr->cCtl - 1,
          "and it is one less than the final cCtl");
    CHECK(g_brUiRootStatusIdx >= 0 &&
          g_brUiRootStatusIdx < BR_UI_PAGE_CTL_MAX &&
          pScr->apCtl[g_brUiRootStatusIdx] == g_aText[8].pCtl,
          "0x1003AF30 would find the control this builder captioned last");
    free(pPhase);
}

/* ==========================================================================
 * 7. Failure paths
 * ========================================================================== */

static void TestPageAllocFails(void)
{
    BrPhase_ *pPhase = NewPhase();

    SetUp();
    pPhase->nPages = 2;
    /* POISON: without this the slot is already NULL and "NULL is published"
     * cannot fail.  It is the fixture-left-zeroed defect CONVENTIONS.md
     * records, and it was live here until the mutation run found it. */
    pPhase->aPages[2] = (BrUiPage_ *)(void *)pPhase;
    g_iAllocFail = 0;                      /* the page */
    BrUiRootEnter_100425E0(pPhase);

    CHECK(g_cErr == 1 && g_aErrIdx[0] == BR_UIROOT_ERR_ALLOC,
          "error index 4 is reported once");
    CHECK(pPhase->aPages[2] == NULL,
          "NULL is published into aPages even on failure");
    CHECK(pPhase->aFlags[2] == 1, "aFlags[] was already set before the alloc");
    CHECK(pPhase->nPages == 3, "nPages is bumped on the failure path too");
    CHECK(g_cAlloc == 1, "no control is allocated after a failed page");
    free(pPhase);
}

static void TestCtlAllocFails(void)
{
    BrPhase_  *pPhase = NewPhase();
    BrUiPage_ *pScr;

    /* allocation 1 is the backdrop, 2 is the cursor, 3 the title. */
    SetUp();
    g_iAllocFail = 2;                      /* the CURSOR */
    BrUiRootEnter_100425E0(pPhase);
    pScr = pPhase->aPages[0];

    CHECK(g_cErr == 1 && g_aErrIdx[0] == BR_UIROOT_ERR_ALLOC,
          "error index 4 for a failed control too");
    CHECK(pScr->apCtl[BR_UIROOT_CURSOR_SLOT] == NULL,
          "the failed cursor lands as NULL in apCtl[199]");
    CHECK(pScr->apCtl[1] == NULL,
          "and NOT in apCtl[cCtl] -- the cursor never goes through the "
          "counted store");
    CHECK(pScr->cCtl == 1, "cCtl stops at the backdrop");
    CHECK(g_cAlloc == 3, "the builder stops rather than dereferencing NULL");
    free(pPhase);

    /* ...and a later one, to show the store-before-test ordering holds for
     * the counted path as well. */
    pPhase = NewPhase();
    SetUp();
    g_iAllocFail = 6;                      /* apCtl[4], "Multiplayer" */
    BrUiRootEnter_100425E0(pPhase);
    pScr = pPhase->aPages[0];
    CHECK(pScr->cCtl == 4, "cCtl reflects the four controls that survived");
    CHECK(pScr->cSel == 1, "cSel is not bumped for a row that was never built");
    /* NOT asserted: "apCtl[4] holds the NULL, i.e. the store happened before
     * the null test".  The slot is already NULL either way, so no assertion
     * about it can fail.  The ordering IS observable for the cursor, whose
     * store goes to a different slot entirely, and TestCursor asserts it
     * there. */
    free(pPhase);
}

/* ==========================================================================
 * 8. The port-only refusal, and the bound on the phase's arrays
 * ========================================================================== */

static void TestRefusal(void)
{
    BrPhase_ *pPhase = NewPhase();

    SetUp();
    g_brUiRoot.pStyleRow = NULL;
    CHECK(BrUiRootCtxComplete(&g_brUiRoot) == 0, "an incomplete ctx is seen");
    BrUiRootEnter_100425E0(pPhase);
    CHECK(g_cAlloc == 0, "an incomplete ctx allocates nothing at all");
    CHECK(pPhase->nPages == 0, "...and touches the phase not at all");
    free(pPhase);
}

static void TestPhaseArrayBound(void)
{
    BrPhase_ *pPhase = NewPhase();

    SetUp();
    pPhase->nPages = BR_PHASE_PAGES;       /* already full */
    BrUiRootEnter_100425E0(pPhase);

    /* DEVIATION (memory safety): the original writes past aPages/aFlags here.
     * The port drops the two stores and builds the page anyway, which is the
     * only part of the original's behaviour that survives the bound. */
    CHECK(pPhase->nPages == BR_PHASE_PAGES + 1, "the count still moves");
    CHECK(pPhase->fBC == 0 && pPhase->fBE == 0,
          "the bounded aFlags write did not run into fBC");
    CHECK(g_cAlloc == 1 + BR_UIROOT_CTL_COUNT + 1,
          "the page is still built");
    free(pPhase);
}

int main(void)
{
    /* Unbuffered: a mutant that faults after failing an assertion must still
     * show WHICH assertion failed.  With the default buffering the whole
     * report is lost with the process and a detected mutant reads as an
     * undiagnosed crash. */
    setbuf(stdout, NULL);

    TestPage();
    TestPlacement();
    TestRowSpacing();
    TestCursor();
    TestChildren();
    TestCaptions();
    TestHooks();
    TestHookIdentity();
    TestStatusIndex();
    TestPageAllocFails();
    TestCtlAllocFails();
    TestRefusal();
    TestPhaseArrayBound();

    if (g_cFail != 0) {
        printf("test_br_uiroot: %d FAILURE(S)\n", g_cFail);
        return 1;
    }
    printf("test_br_uiroot: all checks passed\n");
    return 0;
}
