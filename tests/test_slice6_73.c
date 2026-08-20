/* test_slice6_73.c -- behavioural tests for packet 73.
 *
 * Every assertion below is a PROPERTY of the original -- an invariant, a
 * round-trip, an asymmetry, a defect -- not a count this port happened to
 * produce.  The two places a raw number IS asserted (the control counts and
 * the row offsets) are asserted as RELATIONS wherever a relation exists:
 * "the second menu row sits exactly 19 below the first" rather than "the
 * second menu row is at 149".
 *
 * Every stand-in for a cross-slice symbol lives in THIS file and nowhere
 * else, as the contract requires.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "slice6_73.h"

static int g_fails;

#define CHECK(cond)                                                           \
    do {                                                                      \
        if (!(cond)) {                                                        \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);            \
            ++g_fails;                                                        \
        }                                                                     \
    } while (0)

/* ==========================================================================
 * STAND-INS for cross-slice symbols.  Test scaffolding only.
 * ========================================================================== */

/* --- br_crt.h ------------------------------------------------------------ */

static int      s_fNewFails;        /* make the next operator new return NULL */
static uint32_t s_cbLastNew;

/* 0x1007DFE0.  `operator new` DOES NOT ZERO, so the stand-in poisons the
 * block: anything this packet relies on zeroing would show up at once. */
void *BrOperatorNew(uint32_t cb)
{
    void *p;

    s_cbLastNew = cb;
    if (s_fNewFails) {
        return NULL;
    }
    p = malloc(cb);
    if (p != NULL) {
        memset(p, 0xCD, cb);
    }
    return p;
}

void BrOperatorDelete(void *p) { free(p); }

/* 0x1007C8A0.  Truncates toward zero. */
int32_t BrFtolTrunc(float f) { return (int32_t)f; }

/* --- slice1_06.h --------------------------------------------------------- */

const BrErrEnt g_aBrErrTable[BR_ERR_COUNT] = { {0,0},{0,0},{0,0},{0,0},
                                               {1,0},{0,0},{0,0},{0,0},{0,0} };

static int s_cErr;
static int s_aErrIdx[16];

void BrErrShow(const BrErrHost *pHost, int32_t idx)
{
    (void)pHost;
    if (s_cErr < 16) { s_aErrIdx[s_cErr] = (int)idx; }
    ++s_cErr;
}

static BrErrHost s_errHost;

/* 0x1005CB90 */
BrNameList *BrNameListInit(BrNameList *pThis, const void *pVtbl,
                           const char *pszFill)
{
    int i;

    pThis->pVtbl = pVtbl;
    for (i = 0; i < BR_NAMELIST_COUNT; ++i) {
        memset(pThis->asz[i], 0, BR_NAMELIST_STRIDE);
        if (pszFill != NULL) {
            strncpy(pThis->asz[i], pszFill, BR_NAMELIST_STRIDE - 1);
        }
    }
    return pThis;
}

static int s_cPairBufReset;
int BrPairBufReset(BrPairBuf *pBuf) { (void)pBuf; ++s_cPairBufReset; return 1; }

/* --- slice5_61.h --------------------------------------------------------- */

int32_t        g_br0AB3F4;
unsigned char *g_brPAA29D0;
char           g_aBrA9D018[256];
char           g_aBrA9D078[256];
/* One dword, not two bytes -- slice5_61.h now spells g_brAA26F4/F5 as
 * g_aBrAA26F4[0] and [1], the alias slice5_63.c owns. Same fix as
 * test_slice5_61.c. */
uint8_t        g_aBrAA26F4[4];
const uint8_t  g_aBr0B3820[8] = { 0x02, 0x00, 0x04, 0x00, 0, 0, 0, 0 };
float          g_br4BC198;
const void *(*g_brPfnDerefW1)(uint32_t w1);
int32_t      (*g_brPfn1003D0B0)(void *pObj, void **ppvOut);

void BrSub_10019290(void) {}
void BrSub1003E3A0(void) {}
void BrSub1003CE80(void) {}
BrGfxWords *BrGbiCall10024260(BrGfxWords *pCmd) { return pCmd; }
int32_t BrExt_10042410(void *pArg) { (void)pArg; return 1; }

static int s_c1003E510;
void BrSub1003E510(void) { ++s_c1003E510; }

/* --- slice6_73's own cross-slice callees ---------------------------------- */

char    g_aBr39B720[256];
int32_t g_brAA28D8;

/* 0x10074030.  Ids are echoed back as "s<hex>" so a test can tell them apart;
 * a NULL is returned for id 0x7FFF so the NULL-format path is exercised. */
static char s_aszStr[8][32];
static int  s_iStr;

const char *BrStrGet(int id)
{
    char *p;

    if (id == 0x7FFF) { return NULL; }
    p = s_aszStr[s_iStr & 7];
    s_iStr++;
    snprintf(p, sizeof(s_aszStr[0]), "s%X", (unsigned)id);
    return p;
}

/* ------------------------------------------------------------------------ */
/* The recording control / page constructors and vtables                     */
/* ------------------------------------------------------------------------ */

typedef struct F38Rec {
    BrUiCtl_ *pCtl;
    BrPhase_ *pOwner;
    float     x, y;
    int32_t   flags, a4, a5, a6, a7;
} F38Rec;

typedef struct F34Rec {
    BrUiCtl_   *pCtl;
    const void *pText;
    int32_t     a2, a3;
    const void *pStyle;
} F34Rec;

#define MAXREC 256
static F38Rec s_a38[MAXREC];
static int    s_c38;
static F34Rec s_a34[MAXREC];
static int    s_c34;

static void TestF34(BrUiCtl_ *pThis, const void *pText, int32_t a2, int32_t a3,
                    const void *pStyle)
{
    if (s_c34 < MAXREC) {
        s_a34[s_c34].pCtl = pThis;  s_a34[s_c34].pText = pText;
        s_a34[s_c34].a2 = a2;       s_a34[s_c34].a3 = a3;
        s_a34[s_c34].pStyle = pStyle;
    }
    ++s_c34;
}

static void TestF38(BrUiCtl_ *pThis, BrPhase_ *pOwner, float x, float y,
                    int32_t flags, int32_t a4, int32_t a5,
                    int32_t a6, int32_t a7)
{
    if (s_c38 < MAXREC) {
        F38Rec *r = &s_a38[s_c38];
        r->pCtl = pThis; r->pOwner = pOwner; r->x = x; r->y = y;
        r->flags = flags; r->a4 = a4; r->a5 = a5; r->a6 = a6; r->a7 = a7;
    }
    ++s_c38;
}

/* br_ui.h's BrUiCtlVtbl_ names all sixteen slots; only +0x34 and +0x38 are
 * reached from this packet. */
static const BrUiCtlVtbl_ s_ctlVtbl = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    TestF34,        /* +0x34 */
    TestF38,        /* +0x38 */
    0               /* +0x3C */
};

/* the +0x3838 sub-object */
static int         s_c10, s_c14;
static const void *s_apF10Text[64];
static int32_t     s_aF14[8][5];

static int32_t TestSubF10(BrTextList *pThis, const void *pText, int32_t a2,
                          int32_t a3, const void *pStyle, int32_t a5)
{
    (void)pThis; (void)a2; (void)a3; (void)pStyle; (void)a5;
    if (s_c10 < 64) { s_apF10Text[s_c10] = pText; }
    ++s_c10;
    return 1;
}

static int32_t TestSubF14(BrTextList *pThis, int32_t a1, const void *pStyle,
                          int32_t a3, int32_t a4, int32_t a5)
{
    (void)pThis; (void)pStyle;
    if (s_c14 < 8) {
        s_aF14[s_c14][0] = a1; s_aF14[s_c14][1] = 0;
        s_aF14[s_c14][2] = a3; s_aF14[s_c14][3] = a4; s_aF14[s_c14][4] = a5;
    }
    ++s_c14;
    return 1;
}

/* The object at control +0x3838 is slice3_39.h's whole BrTextList (br_ui.h
 * ADJ-6), so this is two slots of ITS vtable. */
static const BrTextListVtbl s_subVtbl = {
    0, 0, 0, 0,
    TestSubF10,     /* +0x10 */
    TestSubF14,     /* +0x14 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static int s_cItemF04;
static void TestItemF04(BrTextBox *pThis) { (void)pThis; ++s_cItemF04; }
/* 0x1008F728 -- slice3_39.h's BrTextBoxVtbl.  Only +0x04 is reached here. */
static const BrTextBoxVtbl s_itemVtbl = {
    0, TestItemF04, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* 0x100476C0 */
BrUiCtl_ *BrUiCtlCtor(BrUiCtl_ *pThis)
{
    memset(pThis, 0, sizeof(*pThis));
    pThis->pVtbl        = &s_ctlVtbl;
    pThis->list.pVtbl   = &s_subVtbl;
    pThis->aText[0].pVtbl = &s_itemVtbl;
    pThis->list.f1A99C[11].f = 0.0f;   /* +0x1E200 */
    pThis->list.f1A99C[12].f = 11.0f;  /* +0x1E204 */
    return pThis;
}

/* 0x10048470 */
static const BrUiPageVtbl_ s_pageVtbl;
BrUiPage_ *BrUiPageCtor_10048470(BrUiPage_ *pThis)
{
    memset(pThis, 0, sizeof(*pThis));
    pThis->pVtbl = &s_pageVtbl;
    return pThis;
}

/* the name-list vtable slot 0x10050060 calls */
static int         s_cListRefresh;
static const char *s_pszListPattern;
static void TestListF04(BrNameList *pThis, const char *pszPattern)
{
    (void)pThis;
    s_pszListPattern = pszPattern;
    ++s_cListRefresh;
}
static const BrNameListVtbl_ s_listVtbl = { 0, TestListF04 };

/* the phase's own vtable (0x1008F700); only its identity is asserted */
static const BrPhaseVtbl_ s_phaseVtbl = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };

/* --- slice1_05.h --------------------------------------------------------- */
/* 0x10031140's real body, which slice6_73.c only adapts. */
void BrMat4Translate(BrMat4 *pM, float tx, float ty, float tz)
{
    pM->m[0][0]=1; pM->m[0][1]=0; pM->m[0][2]=0; pM->m[0][3]=0;
    pM->m[1][0]=0; pM->m[1][1]=1; pM->m[1][2]=0; pM->m[1][3]=0;
    pM->m[2][0]=0; pM->m[2][1]=0; pM->m[2][2]=1; pM->m[2][3]=0;
    pM->m[3][0]=tx;pM->m[3][1]=ty;pM->m[3][2]=tz;pM->m[3][3]=1;
}

/* --- slice2_11.h: the collision grid ------------------------------------- */

#define TRI_MAX 8
static BrCollPlane s_aGrid[BR73_COLL_CELLS * BR_COLL_CELL_PLANES];
static uint16_t    s_aGridCount[BR73_COLL_CELLS];
static BrVec3      s_aVerts[16];
static uint16_t    s_aTriIdx[4 * TRI_MAX];
static uint8_t     s_aTriFlags[TRI_MAX];

BrCollPlane   *g_pBrCollGrid       = s_aGrid;
const uint16_t *g_pBrCollGridCount = s_aGridCount;
const uint16_t *g_pBrCollTriIdx    = s_aTriIdx;
BrVec3         *g_pBrCollVerts     = s_aVerts;
const uint8_t  *g_pBrCollTriFlags  = s_aTriFlags;

/* 0x10002DE0 (slice1_01.h) -- the CSR row for the cell: first index in the
 * low 16 bits, count in the high 16.  The stand-in ignores (x, y) and hands
 * back a fixed row so the cache behaviour, not the hashing, is what is under
 * test. */
static uint16_t s_cTriPerCell = 2;
static uint16_t s_aTriTable[TRI_MAX + 1] = { 1, 2, 0, 0, 0, 0, 0, 0, 0 };

uint32_t BrGrid64Sample(const uint16_t *pGrid, float x, float y)
{
    (void)pGrid; (void)x; (void)y;
    return ((uint32_t)s_cTriPerCell << 16);
}

/* 0x10002EF0 -- read-and-advance, exactly as slice1_01.h documents it. */
uint16_t BrU16CursorNext(const uint16_t *pTable, BrU16Cursor *pCur)
{
    uint16_t v;

    if (pCur->remaining == 0) { return 0; }
    v = pTable[pCur->pos];
    pCur->pos++;
    pCur->remaining--;
    return v;
}

/* 0x10074250 -- slice1_09.h is explicit that there is NO zero-length guard. */
void BrVec3Normalise(BrVec3 *pV)
{
    float inv = 1.0f / sqrtf(pV->x * pV->x + pV->y * pV->y + pV->z * pV->z);
    pV->x *= inv; pV->y *= inv; pV->z *= inv;
}

/* the two hooks 0x10071550 calls */
static int s_a71550[2];
static int s_i71550;
/* 0x10071550 calls these two directly, not through the g_br73.pfn* slots --
 * the original has no indirection and no null test.  They are therefore real
 * external symbols now rather than installable hooks; neither body is
 * decompiled yet, so these stand in and still record call order. */
void BrSub10071560(void) { s_a71550[s_i71550++ & 1] = 1560; }
void BrSub10071630(void) { s_a71550[s_i71550++ & 1] = 1630; }

/* ==========================================================================
 * Fixture
 * ========================================================================== */

static BrUi73Hooks s_hooks;

/* every hook slot gets its own distinct address so a mix-up is visible */
static void HookBody(void *p) { (void)p; }

static BrPhase_ *NewPhase(void)
{
    BrPhase_ *p = (BrPhase_ *)malloc(sizeof(BrPhase_));
    memset(p, 0xCD, sizeof(*p));
    return BrOptObjCtor(p);
}

static void ResetRecorders(void)
{
    s_c38 = 0; s_c34 = 0; s_c10 = 0; s_c14 = 0;
    s_cItemF04 = 0; s_cErr = 0; s_iStr = 0;
}

static void Setup(void)
{
    /* Every slot in BrUi73Hooks is a function pointer -- most control hooks,
     * two page hooks and three list hooks, all the same width -- so the block
     * can still be filled positionally. */
    BrUiCtlHookFn_ *pf = (BrUiCtlHookFn_ *)(void *)&s_hooks;
    size_t      n  = sizeof(s_hooks) / sizeof(BrUiCtlHookFn_);
    size_t      i;

    for (i = 0; i < n; ++i) { pf[i] = (BrUiCtlHookFn_)(void *)HookBody; }

    memset(&g_br73, 0, sizeof(g_br73));
    g_br73.pErrHost      = &s_errHost;
    g_br73.pHooks        = &s_hooks;
    g_br73.pNameListVtbl = &s_listVtbl;
    g_br73.pPhaseVtbl    = &s_phaseVtbl;
    g_br73.n0AB428       = 0;
    g_br73.n0AB42C       = 380;
    strcpy(g_aBr39B720, "");
}

/* every f38 in the packet passes 2 and 5, and the owner is the PHASE */
static void CheckF38Invariants(BrPhase_ *pPhase)
{
    int i;
    int n = (s_c38 < MAXREC) ? s_c38 : MAXREC;

    for (i = 0; i < n; ++i) {
        CHECK(s_a38[i].a4 == 2);
        CHECK(s_a38[i].a5 == 5);
        CHECK(s_a38[i].pOwner == pPhase);
    }
}

/* the page's array agrees with its counter, and holds no gaps */
static void CheckPage(BrUiPage_ *pPage, int cExpect, int cSelExpect)
{
    int i;

    CHECK(pPage != NULL);
    if (pPage == NULL) { return; }
    CHECK(pPage->cCtl == (uint16_t)cExpect);
    CHECK(pPage->cSel == (uint16_t)cSelExpect);
    CHECK(pPage->cSel <= pPage->cCtl);
    CHECK(pPage->fX == 195.0f);
    CHECK(pPage->fY == 130.0f);
    CHECK(pPage->f10 == 0);
    for (i = 0; i < (int)pPage->cCtl; ++i) {
        CHECK(pPage->apCtl[i] != NULL);
    }
}

/* ==========================================================================
 * 0x10048710
 * ========================================================================== */

static void TestPhaseCtor(void)
{
    BrPhase_   *p = (BrPhase_ *)malloc(sizeof(BrPhase_));
    void       *pPoison;
    BrNameList *pL0, *pL1;
    int         i;

    memset(p, 0xCD, sizeof(*p));
    pPoison = p->pfnEnter;

    CHECK(BrOptObjCtor(p) == p);            /* returns `this` */
    CHECK(p->pVtbl == g_br73.pPhaseVtbl);

    /* +0x04 is NEVER written, and operator new does not zero: the caller's
     * garbage survives construction.  slice2_26.c depends on this. */
    CHECK(p->pfnEnter == pPoison);

    CHECK(p->pfnHook == NULL);
    CHECK(p->f0C == 0);                     /* the installers set this to 1 */
    CHECK(p->f68 == 1);                     /* ...but f68 is already 1 here */
    CHECK(p->nPages == 0);
    CHECK(p->iPage == 0);
    CHECK(p->pCur == NULL);
    CHECK(p->fBC == 0);

    /* twenty dwords at +0x6C, which is what fixes BR_PHASE_PAGES at 20 */
    for (i = 0; i < BR_PHASE_PAGES; ++i) { CHECK(p->aFlags[i] == 0); }

    /* two DISTINCT name lists, each with all 100 slots formatted */
    pL0 = (BrNameList *)p->fC0;
    pL1 = (BrNameList *)p->fC4;
    CHECK(pL0 != NULL && pL1 != NULL && pL0 != pL1);
    if (pL0 != NULL && pL1 != NULL) {
        /* The invariant that matters: every slot of BOTH lists was written,
         * and the two lists agree slot for slot -- the fill loop drives them
         * from the same format and the same index. */
        for (i = 0; i < BR_NAMELIST_COUNT; ++i) {
            CHECK(pL0->asz[i][0] != '\0');
            CHECK(strcmp(pL0->asz[i], pL1->asz[i]) == 0);
        }
    }

    /* the allocation size is never below the original's 0x6594 */
    CHECK(s_cbLastNew >= 0x6594u);

    free(pL0); free(pL1); free(p);

    /* failure path: error index 6, twice, and no crash */
    s_cErr = 0;
    s_fNewFails = 1;
    p = (BrPhase_ *)malloc(sizeof(BrPhase_));
    memset(p, 0xCD, sizeof(*p));
    BrOptObjCtor(p);
    s_fNewFails = 0;
    CHECK(s_cErr == 2);
    CHECK(s_aErrIdx[0] == 6 && s_aErrIdx[1] == 6);
    CHECK(p->fC0 == NULL && p->fC4 == NULL);
    CHECK(p->f68 == 1);
    free(p);
}

/* ==========================================================================
 * The builders
 * ========================================================================== */

static void TestBuilder_1004F2B0(void)
{
    BrPhase_  *pPhase = NewPhase();
    BrUiPage_ *pPage;
    int        i;

    ResetRecorders();
    BrExt_1004F2B0(pPhase);

    CHECK(pPhase->nPages == 1);
    CHECK(pPhase->iPage == 0);
    CHECK(pPhase->aFlags[0] == 1);
    pPage = pPhase->aPages[0];
    CheckPage(pPage, 6, 3);
    CHECK(pPage->pOwner == pPhase);
    CheckF38Invariants(pPhase);
    CHECK(s_c38 == 6);

    /* the root control is placed at the origin with flags 9 */
    CHECK(s_a38[0].x == 0.0f && s_a38[0].y == 0.0f && s_a38[0].flags == 9);

    /* the three menu rows: the cursor steps +19 and then JUMPS to +114.
     * The skipped rows are the original's, not a transcription slip. */
    CHECK(s_a38[2].y == pPage->fY);
    CHECK(s_a38[3].y - s_a38[2].y == 19.0f);
    CHECK(s_a38[4].y - s_a38[2].y == 114.0f);
    /* all three share the page's x */
    for (i = 2; i <= 4; ++i) { CHECK(s_a38[i].x == pPage->fX); }

    /* every label went through BrStrGet, and the styles alternate title /
     * body exactly as the flag words do */
    CHECK(s_c34 == 4);
    CHECK(s_a34[0].pStyle == g_br73.aStyles.p0AB508);
    for (i = 1; i < 4; ++i) { CHECK(s_a34[i].pStyle == g_br73.aStyles.p0AB448); }

    free(pPhase);
}

static void TestBuilder_1004D640(void)
{
    BrPhase_  *pPhase = NewPhase();
    BrUiPage_ *pPage;

    ResetRecorders();
    BrExt_1004D640(pPhase);

    CHECK(pPhase->nPages == 1);
    pPage = pPhase->aPages[0];
    CheckPage(pPage, 7, 3);
    CheckF38Invariants(pPhase);

    /* the list control takes a7 == 0, the only one in the packet that does;
     * every other selectable row takes -1 */
    CHECK(s_a38[4].flags == 0x3001);
    CHECK(s_a38[4].a7 == 0);
    CHECK(s_c14 == 1);
    CHECK(s_aF14[0][0] == 0x40001);
    CHECK(s_aF14[0][2] == 7);

    /* 0x10AA29F0 gets the list; 0x10AA29C8 is written TWICE and keeps the
     * LAST of the two rows, not the first */
    CHECK(g_br73.pAA29F0 == pPage->apCtl[4]);
    CHECK(g_br73.pAA29C8 == pPage->apCtl[6]);

    /* the last two rows are +95 and +114 below the page origin */
    CHECK(s_a38[5].y - pPage->fY == 95.0f);
    CHECK(s_a38[6].y - pPage->fY == 114.0f);

    free(pPhase);
}

static void TestBuilder_1004DFC0(void)
{
    BrPhase_  *pPhase;
    BrUiPage_ *pPage;
    BrUiCtl_  *pList;
    int        i;
    static const char *aszCar[12] = { "c0","c1","c2","c3","c4","c5",
                                      "c6","c7","c8","c9","cA","cB" };

    for (i = 0; i < 12; ++i) { g_br73.apCarName[i] = aszCar[i]; }

    /* --- in-range cursor -------------------------------------------------- */
    pPhase = NewPhase();
    g_br73.nAA2A34 = 0;
    ResetRecorders();
    BrExt_1004DFC0(pPhase);

    pPage = pPhase->aPages[0];
    CheckPage(pPage, 12, 4);
    CheckF38Invariants(pPhase);

    /* all twelve car names were appended, in order */
    CHECK(s_c10 == 12);
    for (i = 0; i < 12; ++i) { CHECK(s_apF10Text[i] == aszCar[i]); }

    /* the list cursor is passed through to f14 */
    CHECK(s_c14 == 1);
    CHECK(s_aF14[0][3] == 0);

    pList = pPage->apCtl[3];
    /* n == 0 lands exactly on the low endpoint */
    CHECK(pList->list.f1A99C[5].f == pList->list.f1A99C[11].f);
    CHECK(pList->list.f1A998 == pList->list.f1A990 + 0x10);
    free(pPhase);

    /* --- the interpolation is n/11 of the way, despite the -1/11 constant -- */
    pPhase = NewPhase();
    g_br73.nAA2A34 = 11;
    ResetRecorders();
    BrExt_1004DFC0(pPhase);
    pList = pPhase->aPages[0]->apCtl[3];
    CHECK(fabs((double)(pList->list.f1A99C[5].f - pList->list.f1A99C[12].f)) < 1e-3);
    CHECK(s_aF14[0][3] == 11);
    free(pPhase);

    /* --- both clamps, and the fact that they are SEPARATE reads ----------- */
    pPhase = NewPhase();
    g_br73.nAA2A34 = -5;
    ResetRecorders();
    BrExt_1004DFC0(pPhase);
    pList = pPhase->aPages[0]->apCtl[3];
    CHECK(s_aF14[0][3] == 0);                       /* clamped up for f14   */
    CHECK(pList->list.f1A99C[5].f == pList->list.f1A99C[11].f);          /* low endpoint         */
    free(pPhase);

    pPhase = NewPhase();
    g_br73.nAA2A34 = 500;
    ResetRecorders();
    BrExt_1004DFC0(pPhase);
    pList = pPhase->aPages[0]->apCtl[3];
    CHECK(s_aF14[0][3] == 0x0B);                    /* clamped down for f14 */
    CHECK(pList->list.f1A99C[5].f == pList->list.f1A99C[12].f);          /* high endpoint        */
    free(pPhase);

    g_br73.nAA2A34 = 0;
}

static void TestBuilder_10050060(void)
{
    BrPhase_  *pPhase = NewPhase();
    BrPhase_  *pCur   = NewPhase();
    BrUiPage_ *pP0, *pP1;
    int        i;

    g_br73.pAA2908 = pCur;
    g_br0AB3F4     = 12345;
    s_cListRefresh = 0;
    g_br73.aStyles.p0AD348 = "RallySeason*.BRF";

    ResetRecorders();
    BrExt_10050060(pPhase);

    /* the prologue refreshed the CURRENT phase's name list, with the pattern,
     * and left 0x10AA2848 back at zero */
    CHECK(s_cListRefresh == 1);
    CHECK(s_pszListPattern == g_br73.aStyles.p0AD348);
    CHECK(g_br73.nAA2848 == 0);
    CHECK(g_br0AB3F4 == -1);

    /* TWO pages, and the second one's flag is 0 -- the only place in the
     * packet where a page is registered with anything but 1 */
    CHECK(pPhase->nPages == 2);
    CHECK(pPhase->aFlags[0] == 1);
    CHECK(pPhase->aFlags[1] == 0);

    pP0 = pPhase->aPages[0];
    pP1 = pPhase->aPages[1];
    CHECK(pP0 != pP1);
    CheckPage(pP0, 12, 3);
    CheckPage(pP1, 1, 0);        /* the second page bumps no cSel at all */
    CHECK(pP0->pOwner == pPhase && pP1->pOwner == pPhase);
    CheckF38Invariants(pPhase);

    /* the season list got one row per name-list slot */
    CHECK(s_c10 == BR_NAMELIST_COUNT);
    for (i = 0; i < 8; ++i) {
        CHECK(s_apF10Text[i] == ((BrNameList *)pCur->fC0)->asz[i]);
    }

    /* the one control in the packet with f1E20C == 2 and a3 == 0 */
    CHECK(pP0->apCtl[3]->w1E20C == 2);
    CHECK(g_br73.pAA29F4 == pP0->apCtl[4]);
    CHECK(g_br73.pAA29C0 == pP1->apCtl[0]);

    free(pPhase);
}

static void TestBuilder_10054B50(void)
{
    BrPhase_  *pPhase = NewPhase();
    BrUiPage_ *pPage;
    BrUiCtl_  *pR[3];
    int        i;

    ResetRecorders();
    BrExt_10054B50(pPhase);

    pPage = pPhase->aPages[0];
    CheckPage(pPage, 20, 2);
    CheckF38Invariants(pPhase);

    /* the only builder that gives the PAGE its own two hooks */
    CHECK(pPage->pfn04 != NULL);
    CHECK(pPage->pfn08 != NULL);

    pR[0] = pPage->apCtl[4];
    pR[1] = pPage->apCtl[5];
    pR[2] = pPage->apCtl[6];

    for (i = 0; i < 3; ++i) {
        /* the rectangle is always 0x7F wide and 0x21 tall */
        CHECK(pR[i]->rcRight - pR[i]->rcLeft == 0x7F);
        CHECK(pR[i]->rcBottom - pR[i]->rcTop == 0x21);
        CHECK(pR[i]->f2968 == 0);
        /* f54 is the truncation of the very float handed to f38 */
        CHECK(pR[i]->rcTop == (int32_t)s_a38[4 + i].y);
        CHECK(s_a38[4 + i].flags == 0x402001);
    }

    /* the DEFECT: only the first rectangle computes its left edge; the other
     * two reuse the register it left behind, so all three share f50/f58 even
     * though nothing re-derives them */
    CHECK(pR[0]->rcLeft == pR[1]->rcLeft && pR[1]->rcLeft == pR[2]->rcLeft);
    CHECK(pR[0]->rcRight == pR[1]->rcRight && pR[1]->rcRight == pR[2]->rcRight);

    /* the row cursor advances by 33 twice and then stops: rect 3 is one step
     * below rect 2, not two */
    CHECK(s_a38[5].y - s_a38[4].y == 33.0f);
    CHECK(s_a38[6].y - s_a38[5].y == 33.0f);

    /* the three f2A42 codes are distinct */
    CHECK(pR[0]->aStepId[1] == 0x79 && pR[1]->aStepId[1] == 0x53 && pR[2]->aStepId[1] == 0x55);

    free(pPhase);
}

static void TestBuilder_100558A0(void)
{
    BrPhase_  *pPhase = NewPhase();
    BrUiPage_ *pPage;
    BrUiCtl_  *pS0, *pS1;

    ResetRecorders();
    BrOptFn100558A0(pPhase);

    CHECK(g_br73.n0AA010 == 6);
    pPage = pPhase->aPages[0];
    CheckPage(pPage, 17, 6);
    CheckF38Invariants(pPhase);

    /* the four consecutive menu rows step +19 then +19 then JUMP to +114 */
    CHECK(s_a38[2].y == pPage->fY);
    CHECK(s_a38[3].y - s_a38[2].y == 19.0f);
    CHECK(s_a38[4].y - s_a38[2].y == 38.0f);
    CHECK(s_a38[5].y - s_a38[2].y == 114.0f);

    /* the two spinners: identical geometry except for the row band, and the
     * +0x2F80 mirrors carry the same four numbers as +0x50 */
    pS0 = pPage->apCtl[10];
    pS1 = pPage->apCtl[13];
    CHECK(pS0->rcLeft == pS0->aText[0].left && pS0->rcTop == pS0->aText[0].f428);
    CHECK(pS0->rcRight == pS0->aText[0].right && pS0->rcBottom == pS0->aText[0].f430);
    CHECK(pS1->rcLeft == pS1->aText[0].left && pS1->rcBottom == pS1->aText[0].f430);
    CHECK(pS0->rcLeft == pS1->rcLeft && pS0->rcRight == pS1->rcRight);
    CHECK(pS0->rcTop != pS1->rcTop);
    /* width == (right - left) - 0x10, computed in 16 bits */
    CHECK(pS0->aText[0].f41C
          == (int16_t)(uint16_t)(pS0->aText[0].right - pS0->aText[0].left - 0x10));
    CHECK(pS1->aText[0].f41C == pS0->aText[0].f41C);
    /* each spinner poked its own item sub-object once */
    CHECK(s_cItemF04 == 2);

    /* the "next slot" counter names the slot this control does NOT occupy */
    CHECK(pPage->apCtl[15]->aChild[0] == 16);
    CHECK(pPage->apCtl[15]->cChild == 1);

    /* two controls take their text straight from the edit buffer, and one
     * from the 0x100AD300 blob -- none of the three through BrStrGet */
    CHECK(s_a34[7].pText  == g_aBr39B720);
    CHECK(s_a34[9].pText  == g_aBr39B720);
    CHECK(s_a34[10].pText == g_br73.aStyles.p0AD300);
    CHECK(s_c34 == 11);

    free(pPhase);
}

/* a failed control allocation still advances the cursor and still leaves the
 * NULL in the array -- the original's behaviour, and the reason the port
 * bails out rather than dereferencing it */
static void TestBuilderAllocFailure(void)
{
    BrPhase_ *pPhase = NewPhase();

    ResetRecorders();
    s_fNewFails = 1;
    BrExt_1004F2B0(pPhase);
    s_fNewFails = 0;

    /* the PAGE allocation is the one that fails first: the phase's counter
     * still advanced and the slot holds NULL */
    CHECK(pPhase->nPages == 1);
    CHECK(pPhase->aPages[0] == NULL);
    CHECK(pPhase->aFlags[0] == 1);
    CHECK(s_cErr >= 1 && s_aErrIdx[0] == 4);
    CHECK(s_c38 == 0);

    free(pPhase);
}

/* ==========================================================================
 * 0x10041A00 / 0x100424D0
 * ========================================================================== */

static int      s_cClearSub70;
static void    *s_pLastArg;
static void ClearSub70(void *pArg) { s_pLastArg = pArg; ++s_cClearSub70; }

static void TestNameSwap(void)
{
    static unsigned char aRecCC[BR61_REC29D0_STRIDE * 4];
    static unsigned char aRecD0[BR61_REC29D0_STRIDE * 4];
    int32_t flag;
    char    szArg;

    memset(aRecCC, 0, sizeof(aRecCC));
    memset(aRecD0, 0, sizeof(aRecD0));
    g_br73.pAA29CC     = aRecCC;
    g_brPAA29D0        = aRecD0;
    g_br73.pfnClearSub70 = ClearSub70;
    g_br0AB3F4         = 1;
    s_cClearSub70      = 0;

    strcpy((char *)aRecCC + BR61_REC29D0_STRIDE + BR61_REC29D0_OFF_NAME, "OLD");
    strcpy(g_aBr39B720, "NEW");
    strcpy(g_aBrA9D078, "");

    CHECK(BrExt_10041A00(&szArg) == 1);
    CHECK(s_cClearSub70 == 1 && s_pLastArg == &szArg);

    /* the flag was zero, so the swap happened and 0x10AA28D8 says 1 */
    CHECK(g_brAA28D8 == 1);
    CHECK(strcmp(g_aBrA9D078, "OLD") == 0);
    CHECK(strcmp((char *)aRecCC + BR61_REC29D0_STRIDE
                 + BR61_REC29D0_OFF_NAME, "NEW") == 0);

    /* the read-modify-write means a second call flips the flag back to 0 and
     * does NOT swap -- and 0x10AA28D8 is written on both paths */
    strcpy(g_aBr39B720, "NEWER");
    CHECK(BrExt_10041A00(&szArg) == 1);
    CHECK(g_brAA28D8 == 0);
    CHECK(strcmp(g_aBrA9D078, "OLD") == 0);
    CHECK(strcmp((char *)aRecCC + BR61_REC29D0_STRIDE
                 + BR61_REC29D0_OFF_NAME, "NEW") == 0);

    /* the flag really does live 0x14 bytes PAST record n's stride */
    memcpy(&flag, aRecCC + BR61_REC29D0_STRIDE + BR61_REC29D0_OFF_FLAG,
           sizeof(flag));
    CHECK(BR61_REC29D0_OFF_FLAG > BR61_REC29D0_STRIDE);
    CHECK(flag == 0);

    /* 0x10041A00 touched ONLY the 0x10AA29CC array; the 0x10AA29D0 array,
     * which is what 0x100424D0 writes, is untouched.  That asymmetry is the
     * original's and is easy to "tidy" away by accident. */
    CHECK(aRecD0[BR61_REC29D0_STRIDE + BR61_REC29D0_OFF_NAME] == 0);

    /* the restore half writes the OTHER array and reloads the edit buffer */
    g_brAA28D8 = 1;
    strcpy(g_aBrA9D078, "SAVED");
    strcpy(g_aBr39B720, "FROMBUF");
    s_cClearSub70 = 0;
    CHECK(BrExt_100424D0(&szArg) == 1);
    CHECK(s_cClearSub70 == 1);
    CHECK(g_br73.nAA28EC == 0);
    CHECK(strcmp((char *)aRecD0 + BR61_REC29D0_STRIDE
                 + BR61_REC29D0_OFF_NAME, "SAVED") == 0);
    CHECK(strcmp(g_aBrA9D078, "FROMBUF") == 0);

    /* with the latch clear it is a no-op apart from 0x10AA28EC */
    g_brAA28D8 = 0;
    g_br73.nAA28EC = 99;
    strcpy(g_aBrA9D078, "KEEP");
    CHECK(BrExt_100424D0(&szArg) == 1);
    CHECK(g_br73.nAA28EC == 0);
    CHECK(strcmp(g_aBrA9D078, "KEEP") == 0);

    g_br73.pfnClearSub70 = NULL;
}

/* ==========================================================================
 * 0x1003E680, 0x1003D030, 0x10071550, 0x10031140
 * ========================================================================== */

static void TestReset(void)
{
    static int32_t a26F0[0x53], a9DBD8[0x53], a220B20[0x46];
    static char    szA[64], szB[64];
    BrPairBuf      pb;
    int            i;

    for (i = 0; i < 0x53; ++i) { a26F0[i] = -7; a9DBD8[i] = -7; }
    for (i = 0; i < 0x46; ++i) { a220B20[i] = -7; }

    g_br73.aAA26F0   = a26F0;
    g_br73.aA9DBD8   = a9DBD8;
    g_br73.a220B20   = a220B20;
    g_br73.szAA2518  = szA;
    g_br73.szA9D618  = szB;
    g_br73.cbScratch = sizeof(szA);
    g_br73.pPairBuf  = &pb;
    g_br73.nAA28A4   = 41;      /* proves the "+1" reads the ZEROED value */
    s_c1003E510      = 0;
    s_cPairBufReset  = 0;

    BrSub1003E680();

    CHECK(g_br73.n0AC648 == 2);
    CHECK(g_br73.n0AC64C == 1 && g_br73.n0AC650 == 1 && g_br73.n0AC654 == 1);
    CHECK(g_br73.n0AC658 == 3);
    CHECK(g_br73.nAA28A4 == 0);
    CHECK(g_br73.wAA27E0 == 0x0102);
    CHECK(s_cPairBufReset == 1);
    CHECK(s_c1003E510 == 1);

    /* both scratch strings say "1": the second reads 0x10AA28A4 AFTER it has
     * been zeroed four instructions earlier, so 41 never reaches it */
    CHECK(strcmp(szA, "1") == 0);
    CHECK(strcmp(szB, "1") == 0);

    /* THIS LOOP USED TO ASSERT ALL 83 ENTRIES ARE ZERO, and in doing so it
     * enshrined a real divergence: the equivalence audit found that
     * 0x10AA27E0 lies INSIDE this block at dword index 60
     * ((0x10AA27E0 - 0x10AA26F0) / 4 == 60), so the original's
     * `mov word ptr [0x10AA27E0], 0x102` at 0x1003E784 re-dirties the block it
     * just cleared -- exactly as the 0x10220B20 case two lines below does, and
     * that one was always asserted correctly.
     *
     * A test that asserts the wrong answer is worse than no test: fixing the
     * code broke this, which is how it was found, but for as long as it stood
     * it certified the bug as intended behaviour. */
    for (i = 0; i < 0x53; ++i) {
        CHECK(a9DBD8[i] == 0);
        if (i == BR73_AA27E0_INDEX) {
            CHECK((a26F0[i] & 0xFFFF) == 0x0102);   /* the aliased word */
            CHECK((uint32_t)a26F0[i] >> 16 == 0);   /* upper half stays cleared */
        } else {
            CHECK(a26F0[i] == 0);
        }
    }
    /* the third block is cleared and then its FIRST element is re-dirtied */
    CHECK(a220B20[0] == -1);
    for (i = 1; i < 0x46; ++i) { CHECK(a220B20[i] == 0); }

    /* the second name is the same body */
    g_br73.nAA28A4 = 5;
    s_c1003E510 = 0;
    BrExt_1003E680();
    CHECK(s_c1003E510 == 1);
    CHECK(g_br73.nAA28A4 == 0);

    g_br73.pPairBuf = NULL;
}

static void TestJoinBlob(void)
{
    unsigned char aSrc[16];
    unsigned char aDst[16];
    void         *ap[2];
    int           i;

    for (i = 0; i < 16; ++i) { aSrc[i] = (unsigned char)(i + 1); }
    memset(aDst, 0, sizeof(aDst));

    ap[0] = NULL;
    ap[1] = aSrc;
    g_br73.apJoinBlob = ap;

    /* no array at all -> 0, and nothing written */
    g_br73.apJoinBlob = NULL;
    CHECK(BrSub1003D030(aDst) == 0);
    CHECK(aDst[0] == 0);

    /* a NULL entry -> 0, and STILL nothing written.  Every exit is 0, which
     * is why the caller's HRESULT test never fires. */
    g_br73.apJoinBlob = ap;
    g_br73.nAA2880 = 0;
    CHECK(BrSub1003D030(aDst) == 0);
    CHECK(aDst[0] == 0);

    g_br73.nAA2880 = 1;
    CHECK(BrSub1003D030(aDst) == 0);
    CHECK(memcmp(aDst, aSrc, 16) == 0);

    g_br73.apJoinBlob = NULL;
}

static void TestMisc(void)
{
    BrMat4  m;
    int32_t ax, ay;
    float   fx = 3.5f, fy = -12.25f;
    int     i, j;

    s_i71550 = 0;
    BrSub10071550();
    CHECK(s_a71550[0] == 1560 && s_a71550[1] == 1630);

    /* the adapter reinterprets the two int32 bit patterns as floats */
    memcpy(&ax, &fx, sizeof(ax));
    memcpy(&ay, &fy, sizeof(ay));
    memset(&m, 0x5A, sizeof(m));
    BrSub_10031140(&m, ax, ay, 7.5f);

    for (i = 0; i < 3; ++i) {
        for (j = 0; j < 4; ++j) {
            CHECK(m.m[i][j] == ((i == j) ? 1.0f : 0.0f));
        }
    }
    CHECK(m.m[3][0] == fx);
    CHECK(m.m[3][1] == fy);
    CHECK(m.m[3][2] == 7.5f);
    CHECK(m.m[3][3] == 1.0f);
}

/* ==========================================================================
 * 0x1006F720
 * ========================================================================== */

static void SetupCollGeometry(void)
{
    int i;

    memset(s_aGrid, 0, sizeof(s_aGrid));
    memset(s_aGridCount, 0, sizeof(s_aGridCount));
    memset(s_aTriIdx, 0, sizeof(s_aTriIdx));

    /* a right-angled triangle in the z = 0 plane, plus a second one tilted */
    s_aVerts[0].x = 0; s_aVerts[0].y = 0; s_aVerts[0].z = 0;
    s_aVerts[1].x = 1; s_aVerts[1].y = 0; s_aVerts[1].z = 0;
    s_aVerts[2].x = 0; s_aVerts[2].y = 1; s_aVerts[2].z = 0;
    s_aVerts[3].x = 2; s_aVerts[3].y = 3; s_aVerts[3].z = 5;
    s_aVerts[4].x = 7; s_aVerts[4].y = 1; s_aVerts[4].z = 2;
    s_aVerts[5].x = 1; s_aVerts[5].y = 9; s_aVerts[5].z = 4;

    for (i = 0; i < TRI_MAX; ++i) {
        s_aTriIdx[4 * i + 0] = 0;
        s_aTriIdx[4 * i + 1] = 1;
        s_aTriIdx[4 * i + 2] = 2;
        s_aTriFlags[i] = (uint8_t)(0xF0 | (i & 7));
    }
    /* triangle 2 uses the tilted vertices */
    s_aTriIdx[4 * 2 + 0] = 3;
    s_aTriIdx[4 * 2 + 1] = 4;
    s_aTriIdx[4 * 2 + 2] = 5;

    g_pBrTriTable = s_aTriTable;
    g_pBrGrid64   = s_aTriTable;    /* unused by the stand-in */

    memset(g_aBrCollGridStamp, 0, sizeof(g_aBrCollGridStamp));
    memset(g_aBrCollGridKey, 0, sizeof(g_aBrCollGridKey));
    g_brCollGridClock = 0;
}

static float Dot(const BrVec3 *a, const BrVec3 *b)
{
    return a->x * b->x + a->y * b->y + a->z * b->z;
}

static void TestCollGrid(void)
{
    short  s0, s1, s2;
    int    i;
    BrVec3 e0, e1;

    SetupCollGeometry();

    /* a NON-NEGATIVE cell key: the second query hits the cache and returns
     * the same slot */
    s0 = BrCollGridCellAcquire(100.0f, 100.0f);
    CHECK(s0 >= 0 && s0 < BR73_COLL_CELLS);
    CHECK(s_aGridCount[s0] == s_cTriPerCell);
    s1 = BrCollGridCellAcquire(100.0f, 100.0f);
    CHECK(s1 == s0);

    /* THE DEFECT: the stored key is compared zero-extended against a
     * sign-extended request, so a negative key can never match its own cache
     * entry.  Two identical queries therefore land in two DIFFERENT slots. */
    SetupCollGeometry();
    s0 = BrCollGridCellAcquire(-100.0f, -100.0f);
    s1 = BrCollGridCellAcquire(-100.0f, -100.0f);
    s2 = BrCollGridCellAcquire(-100.0f, -100.0f);
    CHECK(s1 != s0);
    CHECK(s2 != s1);

    /* the clock advances on every call, hit or miss */
    CHECK(g_brCollGridClock == 3);

    /* the planes of slot k live at grid[150*k..] and nowhere else */
    SetupCollGeometry();
    s_cTriPerCell = 2;
    s0 = BrCollGridCellAcquire(64.0f, 64.0f);
    CHECK(s_aGridCount[s0] == 2);
    for (i = 0; i < 2; ++i) {
        BrCollPlane *p = &s_aGrid[s0 * BR_COLL_CELL_PLANES + i];
        BrVec3       n;

        CHECK(p->pV0 != NULL && p->pV1 != NULL && p->pV2 != NULL);

        /* the surface byte is masked with 7 */
        CHECK((p->flags & ~7u) == 0);

        n.x = p->nx; n.y = p->ny; n.z = p->nz;

        /* the normal is perpendicular to both edges -- the identity that
         * pins the cross product's operand order down */
        e0.x = p->pV1->x - p->pV0->x;
        e0.y = p->pV1->y - p->pV0->y;
        e0.z = p->pV1->z - p->pV0->z;
        e1.x = p->pV2->x - p->pV0->x;
        e1.y = p->pV2->y - p->pV0->y;
        e1.z = p->pV2->z - p->pV0->z;
        CHECK(fabs((double)Dot(&n, &e0)) < 1e-4);
        CHECK(fabs((double)Dot(&n, &e1)) < 1e-4);

        /* it is a UNIT normal after 0x10074250 */
        CHECK(fabs((double)(Dot(&n, &n) - 1.0f)) < 1e-4);

        /* d == -dot(n, V0), i.e. V0 is ON the plane */
        CHECK(fabs((double)(Dot(&n, p->pV0) + p->d)) < 1e-4);
    }

    /* an empty cell records a count of zero and still claims a slot */
    SetupCollGeometry();
    s_cTriPerCell = 0;
    s0 = BrCollGridCellAcquire(4096.0f, 0.0f);
    CHECK(s0 >= 0 && s0 < BR73_COLL_CELLS);
    CHECK(s_aGridCount[s0] == 0);
    s_cTriPerCell = 2;
}

/* ==========================================================================
 * main
 * ========================================================================== */

int main(void)
{
    Setup();

    TestPhaseCtor();
    TestBuilder_1004F2B0();
    TestBuilder_1004D640();
    TestBuilder_1004DFC0();
    TestBuilder_10050060();
    TestBuilder_10054B50();
    TestBuilder_100558A0();
    TestBuilderAllocFailure();
    TestNameSwap();
    TestReset();
    TestJoinBlob();
    TestMisc();
    TestCollGrid();

    if (g_fails == 0) {
        printf("test_slice6_73: all checks passed\n");
        return 0;
    }
    printf("test_slice6_73: %d FAILED\n", g_fails);
    return 1;
}
