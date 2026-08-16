/* test_slice6_71.c -- properties of packet 71.
 *
 * The stand-ins for every cross-slice callee live HERE and nowhere else, as
 * the rest of the tree does it. They are deliberately dumb: each records what
 * it was handed so the tests can assert ORDER and ARGUMENTS, which is where
 * all the content of these seven functions actually is.
 *
 * Assertions are invariants of the original, not volume counts for their own
 * sake: the mode gate in 0x1003BF60, the read-before-clear ordering in
 * 0x10038F30, the branchless AutoSave select in 0x1004F700, the
 * `strlen > 1` (not `!= 0`) rule in 0x100575F0, the 15/9 boundary of the step
 * table in 0x10051D30, and the +0x80/+0x80 rectangle that distinguishes it
 * from the +0x7F/+0x21 one every other builder writes.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "slice6_71.h"

static int g_cFail;

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);      \
            ++g_cFail;                                                  \
        }                                                               \
    } while (0)

/* ==========================================================================
 * A trace of every stand-in call, in order
 * ========================================================================== */

#define TRACE_MAX 512
static const char *g_aTrace[TRACE_MAX];
static int         g_cTrace;

static void Trace(const char *psz)
{
    if (g_cTrace < TRACE_MAX) {
        g_aTrace[g_cTrace] = psz;
    }
    ++g_cTrace;
}

/* index of the first occurrence of `psz`, or -1 */
static int TraceFind(const char *psz)
{
    int i;
    for (i = 0; i < g_cTrace && i < TRACE_MAX; ++i) {
        if (strcmp(g_aTrace[i], psz) == 0) {
            return i;
        }
    }
    return -1;
}

static int TraceCount(const char *psz)
{
    int i, n = 0;
    for (i = 0; i < g_cTrace && i < TRACE_MAX; ++i) {
        if (strcmp(g_aTrace[i], psz) == 0) {
            ++n;
        }
    }
    return n;
}

/* ==========================================================================
 * Allocator / constructor stand-ins
 * ========================================================================== */

/* The contract says 0x1007DFE0 does NOT zero. The stand-in poisons instead,
 * so any field the port forgets to set shows up. The constructor stand-ins
 * then zero the WHOLE allocation, which is what the real ctors do for every
 * field this packet reads back. */
static int g_cAlloc;
static int g_iAllocFail = -1;   /* 0-based call index that returns NULL */

void *BrOperatorNew(uint32_t cb)
{
    void *p;
    if (g_cAlloc++ == g_iAllocFail) {
        return NULL;
    }
    p = malloc(cb);
    if (p != NULL) {
        memset(p, 0xCC, cb);
    }
    return p;
}

/* --- the control vtable ---------------------------------------------------
 * Every f38 / f34 call is recorded so the tests can read back the exact
 * coordinates, flags and ids. */
typedef struct PlaceRec {
    BrUiCtl_  *pCtl;
    BrPhase_  *pOwner;
    float      x, y;
    int32_t    flags, a4, a5, a6, a7;
} PlaceRec;

typedef struct TextRec {
    BrUiCtl_   *pCtl;
    const void *pText;
    int32_t     a2, a3;
    const void *pStyle;
} TextRec;

#define REC_MAX 64
static PlaceRec g_aPlace[REC_MAX];
static int      g_cPlace;
static TextRec  g_aText[REC_MAX];
static int      g_cText;

static void StubF38(BrUiCtl_ *pThis, BrPhase_ *pOwner, float x, float y,
                    int32_t flags, int32_t a4, int32_t a5,
                    int32_t a6, int32_t a7)
{
    Trace("f38");
    if (g_cPlace < REC_MAX) {
        PlaceRec *p = &g_aPlace[g_cPlace];
        p->pCtl = pThis; p->pOwner = pOwner; p->x = x; p->y = y;
        p->flags = flags; p->a4 = a4; p->a5 = a5; p->a6 = a6; p->a7 = a7;
    }
    ++g_cPlace;
}

static void StubF34(BrUiCtl_ *pThis, const void *pText, int32_t a2, int32_t a3,
                    const void *pStyle)
{
    Trace("f34");
    if (g_cText < REC_MAX) {
        TextRec *p = &g_aText[g_cText];
        p->pCtl = pThis; p->pText = pText; p->a2 = a2; p->a3 = a3;
        p->pStyle = pStyle;
    }
    ++g_cText;
}

static BrUiCtlVtbl_ g_ctlVtbl;

/* --- the BrTextList embedded at control +0x3838 --------------------------- */
static int         g_cSubRows;
static const void *g_pLastRow;
static int32_t     g_aSubCfg[5];

/* Both slots return int32_t -- 0x1005BC10 and 0x1005B910 end `mov eax,1`.
 * slice6_71.c discards the result, so the stubs return the original's 1. */
static int32_t StubSubF10(BrTextList *pThis, const void *pText, int32_t a2,
                          int32_t a3, const void *pStyle, int32_t a5)
{
    (void)pThis; (void)a2; (void)a3; (void)pStyle; (void)a5;
    g_pLastRow = pText;
    ++g_cSubRows;
    return 1;
}

static int32_t StubSubF14(BrTextList *pThis, int32_t a1, const void *pStyle,
                          int32_t a3, int32_t a4, int32_t a5)
{
    (void)pThis;
    Trace("sub.f14");
    g_aSubCfg[0] = a1;
    g_aSubCfg[1] = (pStyle != NULL);
    g_aSubCfg[2] = a3;
    g_aSubCfg[3] = a4;
    g_aSubCfg[4] = a5;
    return 1;
}

static BrTextListVtbl g_subVtbl;

/* --- the FIRST of the three text boxes at control +0x2B5C ----------------- */
static int g_cItemF04;
static void StubItemF04(BrTextBox *pThis) { (void)pThis; ++g_cItemF04; }
static BrTextBoxVtbl g_itemVtbl;

/* --- the page/control constructors ----------------------------------------
 * The real 0x10048470 / 0x100476C0 both zero more than they leave alone, and
 * the packet reads back only zeroed fields, so a blanket memset is faithful
 * enough for this module's purposes. The two vtables the packet dispatches
 * through -- the control's own and the ones the constructor's sub-object
 * constructors plant at +0x2B5C and +0x3838 -- are planted here, because that
 * is what 0x1005B050 and 0x1005B7F0 do inside 0x100476C0. */
BrUiPage_ *BrUiPageCtor_10048470(BrUiPage_ *pThis)
{
    memset(pThis, 0, (size_t)BR_UI_PAGE_ALLOC_SIZE);
    Trace("page.ctor");
    return pThis;
}

BrUiCtl_ *BrUiCtlCtor(BrUiCtl_ *pThis)
{
    int i;
    memset(pThis, 0, (size_t)BR_UI_CTL_ALLOC_SIZE);
    pThis->pVtbl      = &g_ctlVtbl;
    pThis->list.pVtbl = &g_subVtbl;
    for (i = 0; i < BR_UI_CTL_TEXTS; ++i) {
        pThis->aText[i].pVtbl = &g_itemVtbl;
    }
    /* The real constructor leaves aStepId at -1 and aStepMs at 0; the tests
     * below use the tail of both arrays to show 0x10051D30 writes 24 entries
     * of a FIFTY-entry array, so the poison has to be the original's. */
    for (i = 0; i < BR_UI_CTL_STEPS; ++i) {
        pThis->aStepId[i] = 0xFFFFu;
    }
    Trace("ctl.ctor");
    return pThis;
}

/* --- the string table ------------------------------------------------------ */
static int  g_aStrAsked[64];
static int  g_cStrAsked;
static char g_szStr[64][32];

const char *BrStrGet(int id)
{
    int slot = g_cStrAsked % 64;
    if (g_cStrAsked < 64) {
        g_aStrAsked[g_cStrAsked] = id;
    }
    ++g_cStrAsked;
    sprintf(g_szStr[slot], "str%d", id);
    return g_szStr[slot];
}

/* --- 0x1003E260 ------------------------------------------------------------ */
static int g_cErr;
static int g_idxErr = -1;

void BrErrShow(const BrErrHost *pHost, int32_t idx)
{
    (void)pHost;
    g_idxErr = idx;
    ++g_cErr;
    Trace("err");
}

/* --- 0x1007C8A0 ------------------------------------------------------------ */
int32_t BrFtolTrunc(float f) { return (int32_t)f; }

/* --- 0x1003F2B0's real body, which lives in slice1_06.c -------------------- */
static const BrOptCaps *g_pCapsSeen;
static uint32_t         g_nSeen;

int32_t BrOptAvailA(const BrOptCaps *pCaps, uint32_t n)
{
    g_pCapsSeen = pCaps;
    g_nSeen     = n;
    if (n == 12u) {
        return 0;
    }
    /* enough of the real shape to make the adapter's forwarding observable */
    return (int32_t)((1u << (n & 31u)) & pCaps->maskA);
}

/* ==========================================================================
 * Environment stand-ins (BrS71Env)
 * ========================================================================== */

static void *g_hKillWnd;
static uint32_t g_idKill;
static int32_t  g_a10005BE0;
static int32_t  g_codeExit;
static void    *g_this10008970;
static void    *g_pFileOpened;
static int      g_fFopenOk = 1;
static char     g_fileBody;

static void E_100586A0(void) { Trace("100586A0"); }
static void E_KillTimer(void *h, uint32_t id)
{ Trace("KillTimer"); g_hKillWnd = h; g_idKill = id; }
static void E_10072270(void) { Trace("10072270"); }
static void E_1003C550(void) { Trace("1003C550"); }
static void E_1002C4A0(void) { Trace("1002C4A0"); }
static void E_10016990(void) { Trace("10016990"); }
static void E_10079550(void) { Trace("10079550"); }
static void E_10078BC0(void) { Trace("10078BC0"); }
static void E_10078DB0(void) { Trace("10078DB0"); }
static void E_10073730(void) { Trace("10073730"); }
static void E_10005BE0(int32_t a) { Trace("10005BE0"); g_a10005BE0 = a; }
static void E_1003BFD0(void) { Trace("1003BFD0"); }
static void E_10002CF0(void) { Trace("10002CF0"); }
static void E_10008B80(void) { Trace("10008B80"); }
static void E_10061620(void) { Trace("10061620"); }
static void E_10008970(void *p) { Trace("10008970"); g_this10008970 = p; }
static void E_1002AEA0(void) { Trace("1002AEA0"); }
static void E_10074050(void) { Trace("10074050"); }
static void E_CoUninit(void) { Trace("CoUninitialize"); }
static void E_Exit(int32_t c) { Trace("exit"); g_codeExit = c; }
static void *E_Fopen(const char *pszPath, const char *pszMode)
{
    (void)pszPath; (void)pszMode;
    Trace("fopen");
    g_pFileOpened = g_fFopenOk ? (void *)&g_fileBody : NULL;
    return g_pFileOpened;
}
static void E_Fclose(void *p) { (void)p; Trace("fclose"); }

static const BrS71Env g_env = {
    E_100586A0, E_KillTimer, E_10072270, E_1003C550,
    E_1002C4A0, E_10016990, E_10079550, E_10078BC0, E_10078DB0, E_10073730,
    E_10005BE0, E_1003BFD0, E_10002CF0, E_10008B80, E_10061620, E_10008970,
    E_1002AEA0, E_10074050, E_CoUninit, E_Exit,
    E_Fopen, E_Fclose
};

/* ==========================================================================
 * Globals
 * ========================================================================== */

/* Distinct, non-null hook bodies so the tests can assert WHICH address goes
 * into WHICH slot -- that mapping is most of what these builders encode. */
static int32_t H_1003EAE0(BrUiCtl_ *p) { (void)p; return 0; }
static int32_t H_1003F210(BrUiCtl_ *p) { (void)p; return 0; }
static int32_t H_1003F280(BrUiCtl_ *p) { (void)p; return 0; }
static int32_t H_10040A50(BrUiCtl_ *p) { (void)p; return 0; }
static int32_t H_10040AC0(BrUiCtl_ *p) { (void)p; return 0; }
static int32_t H_10041300(BrUiCtl_ *p) { (void)p; return 0; }
static int32_t H_10041890(BrUiCtl_ *p) { (void)p; return 0; }
static int32_t H_100443E0(BrUiCtl_ *p) { (void)p; return 0; }
static int32_t H_100444C0(BrUiCtl_ *p) { (void)p; return 0; }
static int32_t H_10042B00(BrUiCtl_ *p) { (void)p; return 0; }
static int32_t H_10045090(BrUiCtl_ *p) { (void)p; return 0; }
static int32_t H_100450C0(BrUiCtl_ *p) { (void)p; return 0; }
static int32_t H_10046E10(BrUiCtl_ *p) { (void)p; return 0; }
static int32_t H_10046F60(BrUiCtl_ *p) { (void)p; return 0; }
static int32_t H_10046FC0(BrUiCtl_ *p) { (void)p; return 0; }
static int32_t H_100471B0(BrUiCtl_ *p) { (void)p; return 0; }
static int32_t H_10047360(BrUiCtl_ *p) { (void)p; return 0; }

/* This one alone lands in the embedded list's +0x04 slot, not in a control
 * hook slot, so it carries slice3_39.h's BrTextListCbFn shape. */
static void H_10042170(void) { }

static BrS71Hooks g_hooks;
static BrOptCaps  g_caps;
static BrErrHost  g_errHost;

/* the phase whose +0xC0 holds the season list, and its vtable */
static int          g_cListScan;
static const char  *g_pszListPattern;

static void ListF04(BrS71FileList *pThis, const char *pszPattern)
{
    (void)pThis;
    Trace("list.rescan");
    g_pszListPattern = pszPattern;
    ++g_cListScan;
}
static BrS71FileListVtbl g_listVtbl;

/* 0x10AA2904's vtable: only +0x18 is used by 0x10038F30 */
static int      g_cPhaseF18;
static void    *g_pPhaseF18Arg;
static int      g_f68AtF18;
static BrPhase_ g_phase2904;
static void PhaseF18(BrPhase_ *pThis, void *pArg)
{
    Trace("phase.f18");
    g_f68AtF18 = (int)pThis->f68;
    g_pPhaseF18Arg = pArg;
    ++g_cPhaseF18;
}
static BrPhaseVtbl_ g_phaseVtbl;

static uint8_t g_entityKind;
static int32_t g_entityFlags;
static char    g_szSessionName[BR71_A9D018_SIZE];
static char    g_a9da50;
static char    g_style438, g_style448, g_style468, g_style478;
static char    g_style488, g_style4D8, g_style508, g_style538;
static char    g_szFill[8] = "";

static void ResetAll(void)
{
    g_cTrace = 0;
    g_cAlloc = 0; g_iAllocFail = -1;
    g_cPlace = 0; g_cText = 0;
    g_cSubRows = 0; g_pLastRow = NULL;
    g_cItemF04 = 0;
    g_cStrAsked = 0;
    g_cErr = 0; g_idxErr = -1;
    g_cPhaseF18 = 0; g_pPhaseF18Arg = (void *)&g_env; g_f68AtF18 = -1;
    g_cListScan = 0; g_pszListPattern = NULL;
    g_a10005BE0 = -1; g_codeExit = -1; g_this10008970 = NULL;
    g_hKillWnd = NULL; g_idKill = 0;
    g_fFopenOk = 1; g_pFileOpened = NULL;

    g_ctlVtbl.f34 = StubF34;
    g_ctlVtbl.f38 = StubF38;
    g_subVtbl.f10 = StubSubF10;
    g_subVtbl.f14 = StubSubF14;
    g_itemVtbl.pfn04 = StubItemF04;
    g_listVtbl.f04 = ListF04;
    g_phaseVtbl.f18 = PhaseF18;

    memset(&g_phase2904, 0, sizeof g_phase2904);
    g_phase2904.pVtbl = &g_phaseVtbl;
    g_phase2904.f68   = 7;
    g_phase2904.fC0   = NULL;

    memset(&g_brS71, 0, sizeof g_brS71);
    g_brS71.pErrHost   = &g_errHost;
    g_brS71.pHooks     = &g_hooks;
    g_brS71.pOptCaps   = &g_caps;
    g_brS71.pA9DA50    = &g_a9da50;
    g_brS71.pA9D018    = g_szSessionName;
    g_brS71.p0AB438    = &g_style438;
    g_brS71.p0AB448    = &g_style448;
    g_brS71.p0AB468    = &g_style468;
    g_brS71.p0AB478    = &g_style478;
    g_brS71.p0AB488    = &g_style488;
    g_brS71.p0AB4D8    = &g_style4D8;
    g_brS71.p0AB508    = &g_style508;
    g_brS71.p0AB538    = &g_style538;
    g_brS71.p39B720    = g_szFill;
    g_brS71.pszRallySeasonBrf = "RallySeason*.BRF";
    g_brS71.pszAutoSaveBrf    = "AutoSave.brf";
    g_brS71.pszFopenMode      = "rb";
    g_brS71.pAA29D8_b2B64 = &g_entityKind;
    g_brS71.pAA29D8_f1C   = &g_entityFlags;

    g_hooks.p1003EAE0 = H_1003EAE0;
    g_hooks.p1003F210 = H_1003F210;
    g_hooks.p1003F280 = H_1003F280;
    g_hooks.p1003F720 = NULL;          /* not reached by these four */
    g_hooks.p10040A50 = H_10040A50;
    g_hooks.p10040AC0 = H_10040AC0;
    g_hooks.p10041300 = H_10041300;
    g_hooks.p10041890 = H_10041890;
    g_hooks.p100443E0 = H_100443E0;
    g_hooks.p100444C0 = H_100444C0;
    g_hooks.p10042170 = H_10042170;
    g_hooks.p10042B00 = H_10042B00;
    g_hooks.p10045090 = H_10045090;
    g_hooks.p100450C0 = H_100450C0;
    g_hooks.p10046E10 = H_10046E10;
    g_hooks.p10046F60 = H_10046F60;
    g_hooks.p10046FC0 = H_10046FC0;
    g_hooks.p100471B0 = H_100471B0;
    g_hooks.p10047360 = H_10047360;

    g_brS71Env = &g_env;
}

/* A freshly zeroed phase for the builders to fill in. */
static BrPhase_ g_self;
static void ResetSelf(void)
{
    memset(&g_self, 0, sizeof g_self);
    g_self.pVtbl = &g_phaseVtbl;
    g_self.iPage = 0x1234;      /* must be cleared */
}

static BrUiPage_ *Page0(void)
{
    return g_self.aPages[0];
}

/* ==========================================================================
 * 0x1003F2B0
 * ========================================================================== */
static void Test3F2B0(void)
{
    ResetAll();
    g_caps.maskA = 0xFFFFFFFFu;

    /* The adapter must hand BrOptAvailA the module's own state and the index
     * unchanged -- that, and the `int` -> `uint32_t` conversion, is the whole
     * of its contract. */
    CHECK(BrSub1003F2B0(3) == (int)BrOptAvailA(&g_caps, 3u));
    CHECK(g_pCapsSeen == &g_caps);
    CHECK(g_nSeen == 3u);

    /* GOTCHA: 12 is a reserved sentinel and returns 0 even with every bit of
     * the mask set. */
    CHECK(BrSub1003F2B0(12) == 0);

    /* The result is the MASKED BIT, not a normalised 0/1. */
    g_caps.maskA = 0x100u;
    CHECK(BrSub1003F2B0(8) == 0x100);
    CHECK(BrSub1003F2B0(9) == 0);

    /* A negative index reaches BrOptAvailA as the same 32-bit pattern the
     * original's `shl cl` would have seen. */
    (void)BrSub1003F2B0(-1);
    CHECK(g_nSeen == 0xFFFFFFFFu);
}

/* ==========================================================================
 * 0x1003BF60
 * ========================================================================== */
static void Test3BF60(void)
{
    /* --- the ordinary path -------------------------------------------- */
    ResetAll();
    g_brS71.hWnd680584 = (void *)&g_env;
    g_brS71.nA9BFDC    = 0x4321;
    g_brS71.nAA2884    = 1;
    g_brS71.nAA287C    = 0;
    g_brS71.nA9CFFC    = 9;
    g_brS71.n22AF18    = 9;
    g_brS71.nAA2888    = 9;
    g_entityKind  = 0xEE;
    g_entityFlags = 0x1F;

    BrSub1003BF60();

    /* order: the slot table is reset FIRST, then the timer dies, then the
     * two teardown calls. */
    CHECK(TraceFind("100586A0") == 0);
    CHECK(TraceFind("KillTimer") == 1);
    CHECK(TraceFind("10072270") < TraceFind("1003C550"));
    CHECK(g_hKillWnd == (void *)&g_env && g_idKill == 0x4321);

    CHECK(g_entityKind == 0);
    CHECK(g_entityFlags == 0x0F);      /* only bit 0x10 is dropped */
    CHECK(g_brS71.nA9CFFC == 0);
    CHECK(g_brS71.nAA2884 == 0);
    CHECK(g_brS71.n22AF18 == 0);
    CHECK(g_brS71.nAA2888 == 0);

    /* Idempotence, and the reason for it: 0x10AA2884 is the gate on
     * 0x10072270 and the function clears it, so a second teardown must not
     * run that call again. */
    g_cTrace = 0;
    BrSub1003BF60();
    CHECK(TraceFind("10072270") < 0);

    /* --- the mode gate ------------------------------------------------- */
    {
        int32_t mode;
        for (mode = 0; mode <= 4; ++mode) {
            ResetAll();
            g_brS71.nAA287C = mode;
            g_entityKind  = 0xEE;
            g_entityFlags = 0x1F;
            BrSub1003BF60();
            if (mode == 2 || mode == 3) {
                CHECK(g_entityKind == 0xEE && g_entityFlags == 0x1F);
            } else {
                CHECK(g_entityKind == 0 && g_entityFlags == 0x0F);
            }
        }
    }

    /* --- the null entity ------------------------------------------------ */
    ResetAll();
    g_brS71.pAA29D8_b2B64 = NULL;
    g_brS71.nA9CFFC = 5;
    BrSub1003BF60();               /* must not fault */
    CHECK(g_brS71.nA9CFFC == 0);

    /* the two wanted names are one function */
    ResetAll();
    g_brS71.nAA2888 = 3;
    BrExt_1003BF60();
    CHECK(g_brS71.nAA2888 == 0);
}

/* ==========================================================================
 * 0x10038F30
 * ========================================================================== */
static void Test38F30(void)
{
    ResetAll();
    g_brS71.pAA2904 = &g_phase2904;
    g_brS71.n0AC300 = 1;
    g_brS71.n22AF18 = 1;
    g_brS71.n0940A4 = 1;
    g_brS71.pA99780 = (void *)&g_caps;

    BrExt_10038F30(0x55);

    /* the phase is told to stop with +0x68 ALREADY cleared, and with NULL */
    CHECK(g_cPhaseF18 == 1);
    CHECK(g_f68AtF18 == 0);
    CHECK(g_pPhaseF18Arg == NULL);

    /* This is the ordering that matters: 0x1022AF18 is TESTED before
     * 0x1003BF60 clears it, so 0x10005BE0 does run on a live session. */
    CHECK(TraceFind("10005BE0") >= 0);
    CHECK(g_a10005BE0 == 1);
    CHECK(TraceFind("10005BE0") < TraceFind("1003BFD0"));
    CHECK(TraceFind("1003BFD0") < TraceFind("100586A0"));   /* i.e. 1003BF60 */
    CHECK(g_brS71.n22AF18 == 0);

    CHECK(TraceFind("10002CF0") >= 0);       /* gated on 0x100940A4 */
    CHECK(g_this10008970 == (void *)&g_caps);
    CHECK(TraceFind("CoUninitialize") < TraceFind("exit"));
    CHECK(g_codeExit == 0x55);
    CHECK(TraceFind("exit") == g_cTrace - 1);

    /* the stub at 0x10008B80 is still called -- the call graph is faithful */
    CHECK(TraceCount("10008B80") == 1);

    /* --- both gates are required for the phase call --------------------- */
    ResetAll();
    g_brS71.pAA2904 = &g_phase2904;
    g_brS71.n0AC300 = 0;
    BrExt_10038F30(0);
    CHECK(g_cPhaseF18 == 0);
    CHECK(g_phase2904.f68 == 7);             /* not even cleared */

    ResetAll();
    g_brS71.n0AC300 = 1;                     /* phase NULL */
    BrExt_10038F30(0);
    CHECK(g_cPhaseF18 == 0);

    /* --- the optional hooks --------------------------------------------- */
    ResetAll();
    g_brS71.n22AF18 = 0;
    g_brS71.n0940A4 = 0;
    BrExt_10038F30(3);
    CHECK(TraceFind("10005BE0") < 0);
    CHECK(TraceFind("10002CF0") < 0);
    CHECK(g_codeExit == 3);
}

/* ==========================================================================
 * Shared builder invariants
 * ========================================================================== */
static void CheckPagePrologue(float fY)
{
    BrUiPage_ *pScr = Page0();
    CHECK(g_self.nPages == 1);
    CHECK(g_self.iPage == 0);
    CHECK(g_self.aFlags[0] == 1);
    CHECK(pScr != NULL);
    CHECK(pScr->pOwner == &g_self);
    CHECK(pScr->f10 == 0);
    CHECK(pScr->fX == 195.0f);
    CHECK(pScr->fY == fY);
    /* the root control is placed against the PHASE, not the page */
    CHECK(g_cPlace > 0);
    CHECK(g_aPlace[0].pOwner == &g_self);
    CHECK(g_aPlace[0].x == 0.0f && g_aPlace[0].y == 0.0f);
    CHECK(g_aPlace[0].flags == 9);
    CHECK(g_aPlace[0].a4 == 2 && g_aPlace[0].a5 == 5);
    CHECK(g_aPlace[0].a6 == 0 && g_aPlace[0].a7 == 0);
    CHECK(g_cErr == 0);
}

/* a4 == 2 and a5 == 5 at every single f38 site in the packet */
static void CheckPlaceConstants(void)
{
    int i;
    for (i = 0; i < g_cPlace && i < REC_MAX; ++i) {
        CHECK(g_aPlace[i].a4 == 2);
        CHECK(g_aPlace[i].a5 == 5);
        CHECK(g_aPlace[i].pOwner == &g_self);
    }
}

/* ==========================================================================
 * 0x10049F40
 * ========================================================================== */
static void Test49F40(void)
{
    BrUiPage_ *pScr;

    ResetAll();
    ResetSelf();
    BrExt_10049F40(&g_self);

    pScr = Page0();
    CheckPagePrologue(130.0f);
    CheckPlaceConstants();

    CHECK(pScr->cCtl == 4);
    CHECK(pScr->cSel == 2);          /* only the two 0x102001 rows */

    /* the title sits at (fX, 10) and is NOT selectable */
    CHECK(g_aPlace[1].x == 195.0f && g_aPlace[1].y == 10.0f);
    CHECK(g_aPlace[1].flags == 0x100009);
    CHECK(g_aStrAsked[0] == 0x10);

    /* the two rows: fY and fY + 19 (the .rdata constant is -19) */
    CHECK(g_aPlace[2].y == 130.0f);
    CHECK(g_aPlace[3].y == 149.0f);
    CHECK(g_aPlace[2].flags == 0x102001 && g_aPlace[3].flags == 0x102001);
    CHECK(g_aStrAsked[1] == 0x11);
    CHECK(g_aStrAsked[2] == 0x12);

    /* the third control -- and only it -- is published to 0x10AA29B0 */
    CHECK(g_brS71.pAA29B0 != NULL);
    CHECK(g_brS71.pAA29B0 == pScr->apCtl[2]);
    CHECK(pScr->apCtl[2]->pfn0C == H_10047360);
    CHECK(pScr->apCtl[2]->pfn08 == H_10046F60);
    CHECK(pScr->apCtl[3]->pfn08 == H_10046FC0);

    /* every text row uses (1, 1) */
    CHECK(g_cText == 3);
    CHECK(g_aText[0].a2 == 1 && g_aText[0].a3 == 1);
    CHECK(g_aText[0].pStyle == &g_style438);
    CHECK(g_aText[1].pStyle == &g_style448);

    /* --- the page allocation fails ------------------------------------- */
    ResetAll();
    ResetSelf();
    g_iAllocFail = 0;
    BrExt_10049F40(&g_self);
    /* the original still bumps the counter and still reports index 4 */
    CHECK(g_self.nPages == 1);
    CHECK(g_self.aPages[0] == NULL);
    CHECK(g_cErr == 1 && g_idxErr == 4);
    CHECK(g_cPlace == 0);

    /* --- a control allocation fails ------------------------------------ */
    ResetAll();
    ResetSelf();
    g_iAllocFail = 2;               /* page, root control, then this one */
    BrExt_10049F40(&g_self);
    CHECK(g_cErr == 1 && g_idxErr == 4);
    /* the NULL is stored into the page BEFORE the report, as in the original */
    CHECK(Page0()->apCtl[1] == NULL);
    CHECK(Page0()->cCtl == 1);
}

/* ==========================================================================
 * 0x10051D30
 * ========================================================================== */
static void Test51D30(void)
{
    BrUiPage_ *pScr;
    BrUiCtl_  *pX;
    int        k;

    ResetAll();
    ResetSelf();
    BrOptFn10051D30(&g_self);

    pScr = Page0();
    CheckPagePrologue(130.0f);
    CheckPlaceConstants();

    CHECK(pScr->cCtl == 3);
    CHECK(pScr->cSel == 1);
    CHECK(g_cText == 1);            /* the odd control has NO text at all */
    CHECK(g_aStrAsked[0] == 0x43);

    /* 0x1008F660 == 8.0 and 0x1008F6A8 == 12.0 are POSITIVE, so unlike every
     * row offset in this packet these two really do subtract. */
    CHECK(g_aPlace[2].x == 195.0f - 8.0f);
    CHECK(g_aPlace[2].y == 130.0f - 12.0f);
    CHECK(g_aPlace[2].flags == 0x22001);
    CHECK(g_aPlace[2].a6 == 0 && g_aPlace[2].a7 == 0x50);

    pX = pScr->apCtl[2];
    CHECK(pX->pfn08 == H_100471B0);
    CHECK(pX->p1E210 == &g_a9da50);
    CHECK(pX->f2968 == 1);
    CHECK(pX->f296C == 1);
    CHECK(pX->w1E20C == 0x50);

    /* The step table: the builder walks entries 0..23, split 15/9 at the
     * code and uniform in the duration. The boundary is the point of the
     * assertion.
     *
     * The ARRAY is fifty long (br_ui.h ADJ-3), which is a separate claim
     * this module used to get wrong: it declared 24, the high-water mark of
     * this very loop. Entries 24..49 must still hold what the constructor
     * left, and asserting that is what makes the two claims independent --
     * with a 24-entry array the loop would have written the whole thing and
     * there would be nothing left to check. */
    for (k = 0; k < BR_UI_CTL_STEPS; ++k) {
        if (k < 24) {
            CHECK(pX->aStepMs[k] == 0x3C);
            CHECK(pX->aStepId[k] == (uint16_t)((k < 15) ? 0x50 : 0x51));
        } else {
            CHECK(pX->aStepMs[k] == 0);
            CHECK(pX->aStepId[k] == 0xFFFFu);
        }
    }

    /* the rectangle is +0x80/+0x80 off the PAGE's origin -- not the
     * +0x7F/+0x21 pair the other builders write. */
    CHECK(pX->rcLeft   == 195);
    CHECK(pX->rcTop    == 130);
    CHECK(pX->rcRight  == 195 + 0x80);
    CHECK(pX->rcBottom == 130 + 0x80);
}

/* ==========================================================================
 * 0x1004F700
 * ========================================================================== */
static void Test4F700(void)
{
    BrUiPage_ *pScr;
    BrPhase_   phase2908;

    /* --- AutoSave.brf present ------------------------------------------ */
    ResetAll();
    ResetSelf();
    memset(&phase2908, 0, sizeof phase2908);
    {
        static BrS71FileList listObj;
        listObj.pVtbl = &g_listVtbl;
        phase2908.fC0 = &listObj;
    }
    g_brS71.pAA2908 = &phase2908;
    g_fFopenOk = 1;

    BrExt_1004F700(&g_self);

    pScr = Page0();
    CheckPagePrologue(130.0f);
    CheckPlaceConstants();

    /* the rescan runs BEFORE anything is allocated, with 0x10AA2848 raised
     * and lowered again around it */
    CHECK(TraceFind("list.rescan") == 0);
    CHECK(g_cListScan == 1);
    CHECK(strcmp(g_pszListPattern, "RallySeason*.BRF") == 0);
    CHECK(g_brS71.n0AB3F4 == -1);
    CHECK(g_brS71.nAA2848 == 0);

    CHECK(pScr->cCtl == 13);
    CHECK(pScr->cSel == 4);

    /* 0x6590 == 100 * 0x104 - 4, so the walk stops after exactly 100 rows
     * and the last one starts at 99 strides in. */
    CHECK(g_cSubRows == BR71_LIST_ROWS);
    CHECK(g_pLastRow == (const void *)((const char *)phase2908.fC0 +
                        (BR71_LIST_ROWS - 1) * BR71_LIST_STRIDE + 4));
    /* control +0x383C and +0x1E1F4 are the embedded list's f04 and
     * f1A99C[8] -- 0x3838 + 4 and 0x3838 + 0x1A99C + 8*4 (br_ui.h ADJ-6). */
    CHECK(pScr->apCtl[2]->list.f04 == H_10042170);
    CHECK(pScr->apCtl[2]->pfn04 == H_1003EAE0);
    CHECK(pScr->apCtl[2]->list.f1A99C[8].i == 1);
    CHECK(g_aSubCfg[0] == 0x40001);
    CHECK(g_aSubCfg[2] == 4 && g_aSubCfg[3] == 0 && g_aSubCfg[4] == -1);
    CHECK(TraceFind("sub.f14") < TraceFind("fopen"));

    /* the AutoSave row, with the file present */
    CHECK(g_aPlace[4].flags == 0x102001);
    CHECK(g_aText[2].a3 == 1);
    CHECK(g_aStrAsked[2] == 0x35);
    CHECK(TraceCount("fclose") == 1);

    /* the four fixed-coordinate rows on the right-hand column */
    CHECK(g_aPlace[7].x == 330.0f && g_aPlace[7].y == 153.0f);
    CHECK(g_aPlace[8].x == 330.0f && g_aPlace[8].y == 97.0f);
    CHECK(g_aPlace[9].x == 440.0f && g_aPlace[9].y == 181.0f);
    CHECK(g_aPlace[12].x == 440.0f && g_aPlace[12].y == 224.0f);

    /* the three 0x1039B720 rows take the literal, never the string table */
    CHECK(g_aText[5].pText == g_szFill);
    CHECK(g_aText[7].pText == g_szFill);
    CHECK(g_aText[9].pText == g_szFill);
    CHECK(g_aText[9].a3 == 4);

    /* --- AutoSave.brf missing ------------------------------------------ */
    ResetAll();
    ResetSelf();
    {
        static BrS71FileList listObj2;
        listObj2.pVtbl = &g_listVtbl;
        phase2908.fC0 = &listObj2;
    }
    g_brS71.pAA2908 = &phase2908;
    g_fFopenOk = 0;

    BrExt_1004F700(&g_self);

    /* the branchless select: 0x102011 / w1E20C 2 / a3 0 */
    CHECK(g_aPlace[4].flags == 0x102011);
    CHECK(g_aText[2].a3 == 0);
    CHECK(TraceCount("fclose") == 0);
    CHECK(Page0()->cCtl == 13);       /* the row is still built */
}

/* ==========================================================================
 * 0x100575F0
 * ========================================================================== */
static void RunName(const char *pszName)
{
    ResetAll();
    ResetSelf();
    memset(g_szSessionName, 0, sizeof g_szSessionName);
    strcpy(g_szSessionName, pszName);
    BrOptFn100575F0(&g_self);
}

static void Test575F0(void)
{
    BrUiPage_ *pScr;
    BrUiCtl_  *pX;

    RunName("Sunday Cup");

    pScr = Page0();
    CheckPagePrologue(130.0f);
    CheckPlaceConstants();

    /* the slot table is reset before the page is built */
    CHECK(TraceFind("100586A0") == 0);
    CHECK(TraceFind("100586A0") < TraceFind("page.ctor"));

    CHECK(pScr->cCtl == 8);
    CHECK(pScr->cSel == 4);

    /* the second row asks for a3 == 4, not 1, and w1E20C 0x34 */
    CHECK(g_aText[1].a3 == 4);
    CHECK(g_aStrAsked[1] == 0x63);
    CHECK(pScr->apCtl[2]->w1E20C == 0x34);

    /* the fixed pair, no text */
    CHECK(g_aPlace[3].x == 156.0f && g_aPlace[3].y == 172.0f);
    CHECK(g_aPlace[3].a7 == 0x39);

    /* the name field */
    pX = pScr->apCtl[4];
    CHECK(g_aPlace[4].y == 174.0f);
    CHECK(g_aPlace[4].flags == 0x200001);
    /* three hook slots, one of which (+0x10) no other builder in the family
     * ever writes */
    CHECK(pX->pfn08 == H_10042B00);
    CHECK(pX->pfn04 == H_1003F210);
    CHECK(pX->pfn10 == H_1003F280);
    /* the block at +0x2B5C is aText[0], and the five +0x2F7x/+0x2F8x fields
     * are that box's own f41C / left / f428 / right / f430 (br_ui.h ADJ-2).
     * The two the vtable's own +0x34 slot also writes -- left and right --
     * are the reason this must be ONE object and not a control-side copy. */
    CHECK(strcmp(pX->aText[0].sz, "Sunday Cup") == 0);
    CHECK(g_cItemF04 == 1);
    CHECK(pX->rcLeft   == 0xC5   && pX->aText[0].left  == 0xC5);
    CHECK(pX->rcTop    == 0xAC   && pX->aText[0].f428  == 0xAC);
    CHECK(pX->rcRight  == 0x135  && pX->aText[0].right == 0x135);
    CHECK(pX->rcBottom == 0xBC   && pX->aText[0].f430  == 0xBC);
    CHECK(pX->aText[0].f41C == (int16_t)(0x135 - 0xC5 - 0x10));

    /* ... and only aText[0]. The other two boxes the constructor builds at
     * +0x2B5C are untouched, which is what makes ADJ-1's three-element array
     * observable from this side rather than only from the constructor. */
    CHECK(pX->aText[1].sz[0] == '\0');
    CHECK(pX->aText[2].sz[0] == '\0');

    /* the sixth control -- and only it -- is published to 0x10AA29BC */
    CHECK(g_brS71.pAA29BC == pScr->apCtl[5]);

    /* the last row is the fixed pair at (80, 46) with a7 == 7 */
    CHECK(g_aPlace[7].x == 80.0f && g_aPlace[7].y == 46.0f);
    CHECK(g_aPlace[7].a7 == 7);

    /* --- the `strlen > 1` boundary --------------------------------------
     * A ONE-character name is treated as absent. This is the original's test
     * (`cmp ecx,1 / ja`), not a `!= 0`, and it is the only interesting
     * boundary in this function. */
    RunName("ab");
    pX = Page0()->apCtl[4];
    CHECK(strcmp(pX->aText[0].sz, "ab") == 0);

    RunName("a");
    pX = Page0()->apCtl[4];
    CHECK(strcmp(pX->aText[0].sz, "str193") == 0);   /* BrStrGet(0xC1) */

    RunName("");
    pX = Page0()->apCtl[4];
    CHECK(strcmp(pX->aText[0].sz, "str193") == 0);
}

/* ========================================================================== */

int main(void)
{
    Test3F2B0();
    Test3BF60();
    Test38F30();
    Test49F40();
    Test51D30();
    Test4F700();
    Test575F0();

    if (g_cFail != 0) {
        printf("%d FAILURE(S)\n", g_cFail);
        return 1;
    }
    printf("slice6_71: all checks passed\n");
    return 0;
}
