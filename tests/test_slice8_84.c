/* test_slice8_84.c -- behaviour and invariant tests for slice8_84.c.
 *
 * WHAT IS LINKED AND WHAT IS STOOD IN, AND WHY
 *
 * LINKED (build.d/test_slice8_84.deps): slice7_81.o.  The three ACTIVATE
 * bodies the installers here call are slice7_81.c's, and the whole point of
 * an installer test is what happens around a REAL activate -- the already-
 * built path that returns 1 without rebuilding, and the allocation failure
 * that still writes both globals.  A stand-in activate would agree with
 * anything.
 *
 * STOOD IN, here in this file:
 *   the six slice2_25.c cyclers and openers.  These are the ADAPTER targets,
 *   and per port/tests/test_slice8_83.c's banner the point of an adapter test
 *   is "did the right callee get the right arguments", which a real callee
 *   would only obscure.  Each records its own call count and the argument it
 *   was handed, and returns a DISTINCT value so a mis-wired table shows up as
 *   a wrong return rather than as nothing.
 *   0x10047360, for the same reason: the assertion is that the table holds
 *   br_sprfont.c's function ITSELF and not a local adapter, which is a pointer
 *   comparison, not a behaviour.
 *   the allocator, the phase constructor, the four enter hooks and the plain
 *   callees slice7_81.c needs -- the same set port/tests/test_slice7_81.c
 *   stands in, for the same reasons its banner gives.
 *
 * The assertions are properties of the ORIGINAL, each traceable to a listing
 * quoted in slice8_84.h: which vtable slot each leave drives (+0x1C vs +0x18),
 * which object it notifies (the current phase -- except 0x10044F00, which
 * notifies 0x10AA2968 instead), read-before-clear vs read-after-clear, the
 * return-value constants (0 from every leave, 1 from every installer, 0 from
 * 0x100471B0 alone), and "an unwired hook slot stays NULL".
 */
#include "slice8_84.h"

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

BrUi73Ctx        g_br73;
BrS71Globals     g_brS71;
const BrS71Env  *g_brS71Env;
Br72Env         *g_pBr72Env;
char             g_aBr39B720[64];
int32_t          g_br0AB3F4;
BrUiNav         *g_pBrUiNav;

static BrUiNav  g_nav;
static Br72Env  g_env72;
static char     g_scratchA[64], g_scratchB[64];

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

/* ==========================================================================
 * Stand-ins slice7_81.o needs
 * ========================================================================== */

static int g_allocFail;

void *BrOperatorNew(uint32_t cb)
{
    void *p;
    if (g_allocFail)
        return NULL;
    p = malloc(cb);
    if (p != NULL)
        memset(p, 0xA5, cb);   /* operator new does NOT zero */
    return p;
}

static const BrPhaseVtbl_ *g_pPhaseVtbl;

BrPhase_ *BrOptObjCtor(BrPhase_ *pThis)
{
    if (pThis == NULL)
        return NULL;
    memset(pThis, 0, sizeof(*pThis));
    pThis->pVtbl = g_pPhaseVtbl;
    pThis->f68   = 1;
    return pThis;
}

void BrExt_100509F0(BrPhase_ *p)                    { (void)p; Log("e509F0"); }
void BrExt_10049F40(BrPhase_ *p)                    { (void)p; Log("e49F40"); }
void BrExt_10050060(BrPhase_ *p)                    { (void)p; Log("e50060"); }
void BrPhaseEnterPlaceholder_1004BDC0(BrPhase_ *p)  { (void)p; Log("eBDC0"); }

void BrExt_1003E680(void)      { Log("1003E680"); }
void BrSub1003E510(void)       { Log("1003E510"); }
void BrExt_100419D0(void *p)   { (void)p; Log("100419D0"); }

/* ==========================================================================
 * Stand-ins for this module's own ADAPTER targets
 * ========================================================================== */

static int   g_nCycTrack, g_nCycBD3E0, g_nCycAA2A00, g_nCycAA2A18;
static int   g_nOpen2950B, g_nOpen2954;
static void *g_pArg2950B, *g_pArg2954;

/* Distinct returns: a mis-wired slot shows up as the wrong number, not as
 * silence.  The real bodies all return 0 or 1, so these are deliberately
 * outside that range. */
int BrOptCycleTrack (void) { g_nCycTrack++;  Log("cycTrack");  return 11; }
int BrOptCycleBD3E0 (void) { g_nCycBD3E0++;  Log("cycBD3E0");  return 12; }
int BrOptCycleAA2A00(void) { g_nCycAA2A00++; Log("cycAA2A00"); return 13; }
int BrOptCycleAA2A18(void) { g_nCycAA2A18++; Log("cycAA2A18"); return 14; }

struct BrGameObj;
int BrOptOpen2950B(struct BrGameObj *p)
{ g_nOpen2950B++; g_pArg2950B = p; Log("open2950B"); return 15; }
int BrOptOpen2954(struct BrGameObj *p)
{ g_nOpen2954++;  g_pArg2954  = p; Log("open2954");  return 16; }

/* 0x10041BD0 -- a stub in the host build too; here it is counted. */
static int g_n41BD0;
void BrExt_10041BD0(void) { g_n41BD0++; Log("100341BD0"); }

/* 0x10047360.  Never called by this test; the assertion is that the two
 * tables hold THIS pointer, i.e. that no adapter was interposed. */
int32_t BrSprFontKindHook_10047360(BrUiCtl_ *pCtl) { (void)pCtl; return 1; }

/* ==========================================================================
 * A phase whose vtable slots are observable
 * ========================================================================== */

static int       g_n1C, g_n00, g_n18;
static BrPhase_ *g_pLast1C, *g_pLast00, *g_pLast18;
static void     *g_pLast18Arg;
static int32_t   g_last00Flag;
/* What 0x10AA2904 held at the moment the notify ran.  0x10046F60 clears it
 * BEFORE notifying, and that is otherwise unobservable. */
static BrPhase_ *g_curAtNotify;

static void VtRelease1C(BrPhase_ *p)
{ g_n1C++; g_pLast1C = p; Log("f1C"); }

static void *VtDelete00(BrPhase_ *p, int32_t f)
{
    g_n00++; g_pLast00 = p; g_last00Flag = f;
    g_curAtNotify = g_pBrUiNav->pAA2904;
    Log("f00");
    return p;
}

static void VtF18(BrPhase_ *p, void *pArg)
{ g_n18++; g_pLast18 = p; g_pLast18Arg = pArg; Log("f18"); }

static BrPhaseVtbl_ g_phaseVtbl;

static BrPhase_ *NewPhase(void)
{
    BrPhase_ *p = (BrPhase_ *)calloc(1, BR_PHASE_ALLOC_SIZE);
    if (p != NULL)
        p->pVtbl = &g_phaseVtbl;
    return p;
}

static BrUiCtl_ *NewCtl(BrPhase_ *pOwner)
{
    BrUiCtl_ *p = (BrUiCtl_ *)calloc(1, BR_UI_CTL_ALLOC_SIZE);
    if (p != NULL)
        p->pOwner = pOwner;
    return p;
}

/* ==========================================================================
 * A world, rebuilt before every case so no test can inherit another's state
 * ========================================================================== */

static BrPhase_ *g_pOwner, *g_pCur, *g_pRoot, *g_pDest, *g_pOther;
static BrUiCtl_ *g_pCtl, *g_pBackA, *g_pBackB, *g_pBackC;

static void World(void)
{
    memset(&g_phaseVtbl, 0, sizeof(g_phaseVtbl));
    g_phaseVtbl.f00 = VtDelete00;
    g_phaseVtbl.f18 = VtF18;
    g_phaseVtbl.f1C = VtRelease1C;
    g_pPhaseVtbl    = &g_phaseVtbl;

    memset(&g_nav,   0, sizeof(g_nav));
    memset(&g_br73,  0, sizeof(g_br73));
    memset(&g_brS71, 0, sizeof(g_brS71));
    memset(&g_env72, 0, sizeof(g_env72));
    memset(g_scratchA, 'x', sizeof(g_scratchA));
    memset(g_scratchB, 'x', sizeof(g_scratchB));
    memset(g_aBr39B720, 0, sizeof(g_aBr39B720));

    g_br73.szAA2518  = g_scratchA;
    g_br73.szA9D618  = g_scratchB;
    g_br73.cbScratch = sizeof(g_scratchA);
    g_pBrUiNav       = &g_nav;
    g_pBr72Env       = &g_env72;
    g_br0AB3F4       = 0;

    BrUiHook84Reset();
    BrUiHook81Reset();

    g_pOwner = NewPhase();
    g_pCur   = NewPhase();
    g_pRoot  = NewPhase();
    g_pDest  = NewPhase();
    g_pOther = NewPhase();
    g_pCtl   = NewCtl(g_pOwner);
    g_pBackA = NewCtl(g_pOwner);
    g_pBackB = NewCtl(g_pOwner);
    g_pBackC = NewCtl(g_pOwner);

    g_nav.pAA2904 = g_pCur;
    g_nav.pAA2908 = g_pRoot;

    g_n1C = g_n00 = g_n18 = 0;
    g_pLast1C = g_pLast00 = g_pLast18 = NULL;
    g_pLast18Arg = NULL;
    g_curAtNotify = NULL;
    g_allocFail = 0;
    g_n41BD0 = 0;
    g_nCycTrack = g_nCycBD3E0 = g_nCycAA2A00 = g_nCycAA2A18 = 0;
    g_nOpen2950B = g_nOpen2954 = 0;
    g_pArg2950B = g_pArg2954 = (void *)~(size_t)0;
    LogReset();
}

/* The prologue eleven of the thirteen leaves share: +0x1C on the OWNER, then
 * +0x00 on the CURRENT phase with 1. */
static void CheckPrologue(const char *pszWho)
{
    CHECK(g_n1C == 1, pszWho);
    CHECK(g_pLast1C == g_pOwner, pszWho);
    CHECK(g_n00 == 1, pszWho);
    CHECK(g_last00Flag == 1, pszWho);
    CHECK(LogIs(0, "f1C") && LogIs(1, "f00"), pszWho);
}

/* ==========================================================================
 * 1. Installation
 * ========================================================================== */

/* Every slot this module claims, so "and nothing else" can be checked by
 * counting rather than by eye. */
static int Count71NonNull(const BrS71Hooks *p)
{
    const void *const *q = (const void *const *)(const void *)p;
    size_t i, n = sizeof(*p) / sizeof(void *), c = 0;
    for (i = 0; i < n; i++)
        if (q[i] != NULL) c++;
    return (int)c;
}
static int Count72NonNull(const BrUi72Hooks *p)
{
    const void *const *q = (const void *const *)(const void *)p;
    size_t i, n = sizeof(*p) / sizeof(void *), c = 0;
    for (i = 0; i < n; i++)
        if (q[i] != NULL) c++;
    return (int)c;
}

static void TestInstall(void)
{
    BrS71Hooks  h71;
    BrUi72Hooks h72;

    printf("\ninstallation\n");

    memset(&h71, 0, sizeof(h71));
    BrUiHook84Install71(&h71);

    CHECK(h71.p10047360 == BrSprFontKindHook_10047360,
          "0x10047360 is br_sprfont.c's own function, NOT a local adapter");
    CHECK(h71.p10046F60 == BrUiHook84_10046F60, "71 0x10046F60");
    CHECK(h71.p10046FC0 == BrUiHook84_10046FC0, "71 0x10046FC0");
    CHECK(h71.p100471B0 == BrUiHook84_100471B0, "71 0x100471B0");
    CHECK(h71.p10045090 == BrUiHook84_10045090, "71 0x10045090");
    CHECK(h71.p100450C0 == BrUiHook84_100450C0, "71 0x100450C0");
    CHECK(h71.p10046E10 == BrUiHook84_10046E10, "71 0x10046E10");
    CHECK(h71.p100443E0 == BrUiHook84Opt_100443E0, "71 0x100443E0");
    CHECK(Count71NonNull(&h71) == 8,
          "Install71 fills eight slots and no more");

    /* The slots slice8_84.h lists under NOT DONE must stay a visible hole. */
    CHECK(h71.p1003EAE0 == NULL && h71.p1003F210 == NULL
       && h71.p1003F280 == NULL, "71 slice2_23 family stays NULL");
    CHECK(h71.p10040A50 == NULL && h71.p10040AC0 == NULL
       && h71.p10041300 == NULL && h71.p10041890 == NULL,
          "71 slice2_24 family stays NULL");
    CHECK(h71.p10042170 == NULL, "71 0x10042170 has no body anywhere");
    CHECK(h71.p10042B00 == NULL && h71.p100444C0 == NULL,
          "71 the two byte-image slice2_25 hooks stay NULL");
    CHECK(h71.p1003F720 == NULL, "71 0x1003F720 -- no builder installs it");

    memset(&h72, 0, sizeof(h72));
    BrUiHook84Install72(&h72);

    CHECK(h72.p10047360 == BrSprFontKindHook_10047360,
          "0x10047360 is the SAME function in both tables");
    CHECK(h72.p10043F50 == BrUiHook84_10043F50, "72 0x10043F50");
    CHECK(h72.p10044B40 == BrUiHook84_10044B40, "72 0x10044B40");
    CHECK(h72.p10042EE0 == BrUiHook84Opt_10042EE0, "72 0x10042EE0");
    CHECK(h72.p10043180 == BrUiHook84Opt_10043180, "72 0x10043180");
    CHECK(h72.p100430B0 == BrUiHook84Opt_100430B0, "72 0x100430B0");
    CHECK(h72.p10044600 == BrUiHook84Opt_10044600, "72 0x10044600");
    CHECK(h72.p100446D0 == BrUiHook84Opt_100446D0, "72 0x100446D0");
    CHECK(h72.p10047340 == BrUiHook84_10047340, "72 0x10047340");
    CHECK(h72.p10047060 == BrUiHook84_10047060, "72 0x10047060");
    CHECK(h72.p100457E0 == BrUiHook84_100457E0, "72 0x100457E0");
    CHECK(h72.p10043FA0 == BrUiHook84_10043FA0, "72 0x10043FA0");
    CHECK(h72.p100457C0 == BrUiHook84_100457C0, "72 0x100457C0");
    CHECK(h72.p10044C70 == BrUiHook84_10044C70, "72 0x10044C70");
    CHECK(h72.p10044F00 == BrUiHook84_10044F00, "72 0x10044F00");
    CHECK(h72.p10046710 == BrUiHook84_10046710, "72 0x10046710");
    CHECK(Count72NonNull(&h72) == 16,
          "Install72 fills sixteen slots and no more");

    /* slice7_80.c's BrUiOptInstall72 owns these four.  If this installer ever
     * starts writing them the two passes will fight over the table, so the
     * fact that it does NOT is an assertion, not a comment. */
    CHECK(h72.p10043590 == NULL && h72.p100435F0 == NULL
       && h72.p10043650 == NULL && h72.p100436B0 == NULL,
          "72 leaves slice7_80.c's four option toggles alone");
    CHECK(h72.p100474B0 == NULL, "72 0x100474B0 stays NULL -- NOT DONE (D)");
    CHECK(h72.p10046260 == NULL && h72.p10044D00 == NULL
       && h72.p10045050 == NULL, "72 the three big activates stay NULL");
    CHECK(h72.p10042560 == NULL && h72.p10042740 == NULL,
          "72 the two list callbacks have no body anywhere");

    /* An installer handed NULL writes nothing and does not fault. */
    BrUiHook84Install71(NULL);
    BrUiHook84Install72(NULL);
}

/* ==========================================================================
 * 2. The six adapters
 * ========================================================================== */

static void TestAdapters(void)
{
    printf("\nadapters (slice2_25.c bodies, argument provably unread)\n");

    World();
    CHECK(BrUiHook84Opt_10042EE0(g_pCtl) == 11, "0x10042EE0 forwards its result");
    CHECK(g_nCycTrack == 1 && g_nCycBD3E0 == 0 && g_nCycAA2A00 == 0
       && g_nCycAA2A18 == 0, "0x10042EE0 calls BrOptCycleTrack exactly once");

    World();
    CHECK(BrUiHook84Opt_10043180(g_pCtl) == 13, "0x10043180 forwards its result");
    CHECK(g_nCycAA2A00 == 1 && g_nCycTrack == 0,
          "0x10043180 -> BrOptCycleAA2A00, not its neighbour");

    World();
    CHECK(BrUiHook84Opt_100430B0(g_pCtl) == 12, "0x100430B0 forwards its result");
    CHECK(g_nCycBD3E0 == 1, "0x100430B0 -> BrOptCycleBD3E0");

    World();
    CHECK(BrUiHook84Opt_10044600(g_pCtl) == 14, "0x10044600 forwards its result");
    CHECK(g_nCycAA2A18 == 1, "0x10044600 -> BrOptCycleAA2A18");

    World();
    CHECK(BrUiHook84Opt_100443E0(g_pCtl) == 15, "0x100443E0 forwards its result");
    CHECK(g_nOpen2950B == 1 && g_pArg2950B == NULL,
          "0x100443E0 passes NULL -- the body never reads it");

    World();
    CHECK(BrUiHook84Opt_100446D0(g_pCtl) == 16, "0x100446D0 forwards its result");
    CHECK(g_nOpen2954 == 1 && g_pArg2954 == NULL,
          "0x100446D0 passes NULL -- the body never reads it");

    /* Every adapter must survive a NULL control, because the original never
     * touches it and 0x10048180 is free to pass anything. */
    World();
    CHECK(BrUiHook84Opt_10042EE0(NULL) == 11, "adapters ignore the control");
}

/* ==========================================================================
 * 3. The leave family
 * ========================================================================== */

static void TestPlainLeaves(void)
{
    printf("\nleave: the shared prologue, read-before-clear, destination\n");

    /* --- 0x10046710 -> 0x10AA2918, clears 0x10AA2988 --------------------- */
    World();
    g_brHook81.pAA2918 = g_pDest;
    g_brHook84.pAA2988 = g_pOther;
    CHECK(BrUiHook84_10046710(g_pCtl) == 0, "0x10046710 returns 0");
    CheckPrologue("0x10046710 prologue");
    CHECK(g_nav.pAA2904 == g_pDest, "0x10046710 -> 0x10AA2918");
    CHECK(g_brHook84.pAA2988 == NULL, "0x10046710 clears 0x10AA2988");
    CHECK(g_brHook81.pAA2918 == g_pDest,
          "0x10046710 does NOT clear the word it read");

    /* --- 0x10047060 -> 0x10AA292C, clears 0x10AA2930 --------------------- */
    World();
    g_brHook81.pAA292C = g_pDest;
    g_brHook84.pAA2930 = g_pOther;
    CHECK(BrUiHook84_10047060(g_pCtl) == 0, "0x10047060 returns 0");
    CheckPrologue("0x10047060 prologue");
    CHECK(g_nav.pAA2904 == g_pDest, "0x10047060 -> 0x10AA292C");
    CHECK(g_brHook84.pAA2930 == NULL, "0x10047060 clears 0x10AA2930");

    /* --- 0x10046830 -> 0x10AA2930, clears 0x10AA2918 --------------------- *
     * The mirror image of 0x10047060: the two swap which word they read and
     * which they clear, which is exactly the pair whereis.py reports as one
     * body (slice8_84.h CONFLICT 5). */
    World();
    g_brHook84.pAA2930 = g_pDest;
    g_brHook81.pAA2918 = g_pOther;
    CHECK(BrUiHook84_10046830(g_pCtl) == 0, "0x10046830 returns 0");
    CheckPrologue("0x10046830 prologue");
    CHECK(g_nav.pAA2904 == g_pDest, "0x10046830 -> 0x10AA2930");
    CHECK(g_brHook81.pAA2918 == NULL, "0x10046830 clears 0x10AA2918");

    /* --- 0x10044C70 -> the ROOT phase ------------------------------------ */
    World();
    g_brHook84.pAA295C = g_pOther;
    CHECK(BrUiHook84_10044C70(g_pCtl) == 0, "0x10044C70 returns 0");
    CheckPrologue("0x10044C70 prologue");
    CHECK(g_nav.pAA2904 == g_pRoot,
          "0x10044C70 goes to 0x10AA2908, the ROOT, not a singleton");
    CHECK(g_brHook84.pAA295C == NULL, "0x10044C70 clears 0x10AA295C");

    /* --- 0x10043F50: 0x10AA287C = 2 BEFORE the prologue ------------------ */
    World();
    g_brS71.nAA287C    = 7;
    g_brHook84.pAA2948 = g_pDest;
    g_brHook84.pAA298C = g_pOther;
    CHECK(BrUiHook84_10043F50(g_pCtl) == 0, "0x10043F50 returns 0");
    CheckPrologue("0x10043F50 prologue");
    CHECK(g_brS71.nAA287C == 2, "0x10043F50 sets 0x10AA287C to 2");
    CHECK(g_nav.pAA2904 == g_pDest, "0x10043F50 -> 0x10AA2948");
    CHECK(g_brHook84.pAA298C == NULL, "0x10043F50 clears 0x10AA298C");

    /* --- 0x10044B40 clears 0x10AA298C and the CONTROL 0x10AA29E8 --------- */
    World();
    g_brHook81.pAA2940  = g_pDest;
    g_brHook84.pAA298C  = g_pOther;
    g_env72.pAA29E8     = g_pBackA;
    CHECK(BrUiHook84_10044B40(g_pCtl) == 0, "0x10044B40 returns 0");
    CheckPrologue("0x10044B40 prologue");
    CHECK(g_nav.pAA2904 == g_pDest, "0x10044B40 -> 0x10AA2940");
    CHECK(g_brHook84.pAA298C == NULL, "0x10044B40 clears 0x10AA298C");
    CHECK(g_env72.pAA29E8 == NULL,
          "0x10044B40 clears the CONTROL at 0x10AA29E8");

    /* ...and survives an unwired slice6_72 context, which the harness has
     * whenever BrHostWire72 has not run. */
    World();
    g_pBr72Env = NULL;
    g_brHook81.pAA2940 = g_pDest;
    CHECK(BrUiHook84_10044B40(g_pCtl) == 0,
          "0x10044B40 with no slice6_72 context still returns 0");
    CHECK(g_nav.pAA2904 == g_pDest, "...and still repoints the phase");
    g_pBr72Env = &g_env72;
}

static void TestOddLeaves(void)
{
    printf("\nleave: the four that are not the shared shape\n");

    /* --- 0x10046FC0: thirteen bytes, NO prologue ------------------------- */
    World();
    g_brHook81.pAA292C = g_pDest;
    CHECK(BrUiHook84_10046FC0(g_pCtl) == 0,
          "0x10046FC0 returns 0 -- slice3_31.h calls it void (CONFLICT 1)");
    CHECK(g_n1C == 0 && g_n00 == 0 && g_n18 == 0,
          "0x10046FC0 calls NOTHING -- it is three instructions");
    CHECK(g_nav.pAA2904 == g_pDest, "0x10046FC0 -> 0x10AA292C");
    /* It never touches the argument, so NULL must be fine (CONFLICT 2). */
    World();
    g_brHook81.pAA292C = g_pDest;
    CHECK(BrUiHook84_10046FC0(NULL) == 0, "0x10046FC0 ignores the control");
    CHECK(g_nav.pAA2904 == g_pDest, "...and still repoints the phase");

    /* --- 0x10046F60: the current phase is NULL while the notify runs ----- */
    World();
    g_brHook81.pAA292C = g_pOther;      /* the saved phase, notified below */
    g_brHook81.pAA2974 = g_pDest;
    CHECK(BrUiHook84_10046F60(g_pCtl) == 0, "0x10046F60 returns 0");
    CHECK(g_n1C == 1 && g_pLast1C == g_pOwner, "0x10046F60 releases the owner");
    /* TWO +0x00 calls: the prologue's on the current phase, then the saved
     * one's -- and the second runs with 0x10AA2904 already NULL. */
    CHECK(g_n00 == 2, "0x10046F60 notifies twice");
    CHECK(g_pLast00 == g_pOther, "the second notify is on 0x10AA292C");
    CHECK(g_curAtNotify == NULL,
          "0x10AA2904 is cleared BEFORE the saved phase is notified");
    CHECK(g_brHook81.pAA292C == NULL,
          "0x10046F60 clears 0x10AA292C on the non-NULL arm");
    CHECK(g_brHook81.pAA2974 == NULL, "0x10046F60 clears 0x10AA2974");
    CHECK(g_nav.pAA2904 == g_pRoot, "0x10046F60 ends at the root phase");

    /* ...and with nothing saved, the second notify does not happen and
     * 0x10AA292C is not written. */
    World();
    g_brHook81.pAA292C = NULL;
    CHECK(BrUiHook84_10046F60(g_pCtl) == 0, "0x10046F60 (empty) returns 0");
    CHECK(g_n00 == 1, "no saved phase -> only the prologue's notify");
    CHECK(g_nav.pAA2904 == g_pRoot, "...and it still ends at the root");

    /* --- 0x10043FA0: vtable +0x18 with the literal 1, and no notify ------ */
    World();
    CHECK(BrUiHook84_10043FA0(g_pCtl) == 0, "0x10043FA0 returns 0");
    CHECK(g_n18 == 1 && g_pLast18 == g_pOwner,
          "0x10043FA0 drives +0x18 on the owner, not +0x1C");
    CHECK(g_n1C == 0, "0x10043FA0 never touches +0x1C");
    CHECK(g_n00 == 0, "0x10043FA0 does not notify the current phase");
    CHECK(g_pLast18Arg == (void *)(size_t)1u,
          "0x10043FA0 pushes the literal 1 at +0x18 (CONFLICT 4)");
    CHECK(g_nav.pAA2904 == g_pRoot, "0x10043FA0 -> 0x10AA2908");

    /* --- 0x10044F00: notifies 0x10AA2968, NOT the current phase ---------- */
    World();
    g_brHook84.pAA2968 = g_pOther;
    g_brHook84.pAA295C = g_pDest;
    g_nav.n0AA010      = 9;
    CHECK(BrUiHook84_10044F00(g_pCtl) == 0, "0x10044F00 returns 0");
    CHECK(g_n1C == 1 && g_pLast1C == g_pOwner, "0x10044F00 releases the owner");
    CHECK(g_n00 == 1 && g_pLast00 == g_pOther,
          "0x10044F00 notifies 0x10AA2968, NOT 0x10AA2904");
    CHECK(g_pLast00 != g_pCur, "...and the current phase is left un-notified");
    CHECK(g_brHook84.pAA2968 == NULL, "0x10044F00 clears 0x10AA2968");
    CHECK(g_nav.pAA2904 == g_pDest, "0x10044F00 -> 0x10AA295C");
    CHECK(g_nav.n0AA010 == 2, "0x10044F00 sets 0x100AA010 to 2, not 0");
}

static void TestNameResets(void)
{
    printf("\nleave: the two name-resetting bodies, which differ\n");

    /* 0x1039B720 is empty in the shipped image; give it content so the copy
     * is observable at all. */
    strcpy(g_aBr39B720, "NAME");

    /* --- 0x10046870: the FULL reset -------------------------------------- */
    World();
    strcpy(g_aBr39B720, "NAME");
    g_brHook84.pAA2930  = g_pDest;
    g_brHook81.pAA2928  = g_pOther;
    g_brHook81.nAA28E4  = 5;
    g_nav.pAA29C0       = g_pBackA;
    g_br73.pAA29CC      = (unsigned char *)g_scratchA;
    CHECK(BrUiHook84_10046870(g_pCtl) == 0, "0x10046870 returns 0");
    CheckPrologue("0x10046870 prologue");
    CHECK(strcmp(g_scratchA, "NAME") == 0, "0x10046870 fills 0x10AA2518");
    CHECK(strcmp(g_scratchB, "NAME") == 0, "0x10046870 fills 0x10A9D618");
    CHECK(g_brHook81.pAA2928 == NULL, "0x10046870 clears 0x10AA2928");
    CHECK(g_nav.pAA29C0 == NULL, "0x10046870 clears the CONTROL 0x10AA29C0");
    CHECK(g_br73.pAA29CC == NULL, "0x10046870 clears 0x10AA29CC");
    CHECK(g_brHook81.nAA28E4 == 0, "0x10046870 clears 0x10AA28E4");
    CHECK(g_br0AB3F4 == -1, "0x10046870 sets 0x100AB3F4 to -1");
    CHECK(g_nav.pAA2904 == g_pDest,
          "0x10046870 -> 0x10AA2930, read AFTER the clears");

    /* --- 0x10046E10: a strictly SMALLER reset ---------------------------- */
    World();
    strcpy(g_aBr39B720, "NAME");
    g_brHook81.pAA291C = g_pDest;
    g_nav.pAA2924      = g_pOther;
    g_brHook84.nAA28E0 = 4;
    g_brHook81.pAA2928 = g_pOther;
    g_brHook81.nAA28E4 = 5;
    g_nav.pAA29C0      = g_pBackA;
    g_br73.pAA29CC     = (unsigned char *)g_scratchA;
    CHECK(BrUiHook84_10046E10(g_pCtl) == 0, "0x10046E10 returns 0");
    CheckPrologue("0x10046E10 prologue");
    CHECK(g_nav.pAA2924 == NULL, "0x10046E10 clears 0x10AA2924");
    CHECK(g_brHook84.nAA28E0 == 0, "0x10046E10 clears 0x10AA28E0");
    CHECK(g_br0AB3F4 == -1, "0x10046E10 sets 0x100AB3F4 to -1");
    CHECK(strcmp(g_scratchA, "NAME") == 0, "0x10046E10 fills 0x10AA2518");
    CHECK(strcmp(g_scratchB, "NAME") == 0, "0x10046E10 fills 0x10A9D618");
    /* The four 0x10046870 clears that 0x10046E10 does NOT.  This is the whole
     * reason the two are separate bodies rather than one shared helper. */
    CHECK(g_brHook81.pAA2928 == g_pOther,
          "0x10046E10 does NOT clear 0x10AA2928");
    CHECK(g_brHook81.nAA28E4 == 5, "0x10046E10 does NOT clear 0x10AA28E4");
    CHECK(g_nav.pAA29C0 == g_pBackA, "0x10046E10 does NOT clear 0x10AA29C0");
    CHECK(g_br73.pAA29CC != NULL, "0x10046E10 does NOT clear 0x10AA29CC");
    CHECK(g_nav.pAA2904 == g_pDest, "0x10046E10 -> 0x10AA291C");

    /* The copies are BOUNDED and NUL-terminated -- the port's deviation.  A
     * source longer than the destination must not run off the end. */
    World();
    memset(g_aBr39B720, 'Z', sizeof(g_aBr39B720) - 1);
    g_aBr39B720[sizeof(g_aBr39B720) - 1] = '\0';
    g_br73.cbScratch = 8;
    CHECK(BrUiHook84_10046870(g_pCtl) == 0, "bounded copy returns 0");
    CHECK(strlen(g_scratchA) == 7, "0x10AA2518 is bounded by cbScratch");
    CHECK(strlen(g_scratchB) == 7, "0x10A9D618 is bounded by cbScratch");
    memset(g_aBr39B720, 0, sizeof(g_aBr39B720));
}

/* ==========================================================================
 * 4. The installers
 * ========================================================================== */

static void TestInstallers(void)
{
    printf("\ninstallers: activate, then wire another control's +0x08\n");

    /* --- 0x10045090.  The singleton is pre-filled so the REAL activate
     * takes its already-built path and the test is about the wiring. ------ */
    World();
    g_brHook81.pAA292C = g_pOther;      /* 0x10045C90's slot, already built */
    g_brS71.pAA29B0    = g_pBackA;
    g_nav.n0AA010      = 9;
    CHECK(BrUiHook84_10045090(g_pCtl) == 1, "0x10045090 returns 1");
    CHECK(g_n41BD0 == 0, "0x10045090 does NOT call 0x10041BD0");
    CHECK(g_pBackA->pfn08 == BrUiHook84_10046DC0,
          "0x10045090 pokes 0x10046DC0 into 0x10AA29B0's +0x08");
    CHECK(g_nav.n0AA010 == 0, "0x10045090 clears 0x100AA010");
    CHECK(g_nav.pAA2904 == g_pOther,
          "the already-built activate republished the singleton");

    /* --- 0x100450C0 is 0x10045090 plus one leading call ------------------ */
    World();
    g_brHook81.pAA292C = g_pOther;
    g_brS71.pAA29B0    = g_pBackA;
    CHECK(BrUiHook84_100450C0(g_pCtl) == 1, "0x100450C0 returns 1");
    CHECK(g_n41BD0 == 1, "0x100450C0 calls 0x10041BD0");
    CHECK(LogIs(0, "100341BD0"),
          "0x10041BD0 runs FIRST, before the activate");
    CHECK(g_pBackA->pfn08 == BrUiHook84_10046DC0,
          "0x100450C0 wires the same back row");

    /* --- the NULL guard.  The original faults here; the port must not. --- */
    World();
    g_brHook81.pAA292C = g_pOther;
    g_brS71.pAA29B0    = NULL;
    CHECK(BrUiHook84_10045090(g_pCtl) == 1,
          "0x10045090 with no 0x10AA29B0 still returns 1 (DEVIATION)");

    /* --- 0x100457C0 -> 0x10AA29C8 ---------------------------------------- */
    World();
    g_brHook81.pAA2918 = g_pOther;      /* 0x100451E0's slot, already built */
    g_br73.pAA29C8     = g_pBackB;
    CHECK(BrUiHook84_100457C0(g_pCtl) == 1, "0x100457C0 returns 1");
    CHECK(g_pBackB->pfn08 == BrUiHook84_10046830,
          "0x100457C0 pokes 0x10046830 into 0x10AA29C8's +0x08");
    CHECK(g_nav.pAA2904 == g_pOther, "0x100457C0 republished 0x10AA2918");
    /* 0x100451E0's prologue is a call to 0x100419D0 and it runs on BOTH
     * paths, including the already-built one. */
    CHECK(LogIs(0, "100419D0"), "0x100451E0's prologue ran first");

    /* --- 0x100457E0 -> 0x10AA29F4 ---------------------------------------- */
    World();
    g_brHook81.pAA2928 = g_pOther;      /* 0x10045BC0's slot, already built */
    g_br73.pAA29F4     = g_pBackC;
    CHECK(BrUiHook84_100457E0(g_pCtl) == 1, "0x100457E0 returns 1");
    CHECK(g_pBackC->pfn08 == BrUiHook84_10046870,
          "0x100457E0 pokes 0x10046870 into 0x10AA29F4's +0x08");
    CHECK(g_nav.pAA2904 == g_pOther, "0x100457E0 republished 0x10AA2928");

    /* --- 0x100471B0: activate FIRST, release SECOND, and returns 0 ------- */
    World();
    g_brHook81.pAA292C = g_pOther;
    g_brHook84.pAA2970 = g_pDest;
    CHECK(BrUiHook84_100471B0(g_pCtl) == 0,
          "0x100471B0 returns 0, unlike its three neighbours");
    /* The already-built activate logs nothing, so the first entry is the
     * release -- but the CURRENT phase was republished by the activate, and
     * that is what proves the order: the release ran after it. */
    CHECK(g_n1C == 1 && g_pLast1C == g_pOwner, "0x100471B0 releases the owner");
    CHECK(g_n00 == 1 && g_pLast00 == g_pDest,
          "0x100471B0 notifies 0x10AA2970, not the current phase");
    CHECK(g_brHook84.pAA2970 == NULL, "0x100471B0 clears 0x10AA2970");
    CHECK(g_nav.pAA2904 == g_pOther,
          "0x100471B0 leaves 0x10AA2904 where the activate put it");

    /* ...and with nothing to drop, nothing is notified. */
    World();
    g_brHook81.pAA292C = g_pOther;
    g_brHook84.pAA2970 = NULL;
    CHECK(BrUiHook84_100471B0(g_pCtl) == 0, "0x100471B0 (empty) returns 0");
    CHECK(g_n00 == 0, "no 0x10AA2970 -> nothing is notified");
}

/* ==========================================================================
 * 5. 0x10047340, and the reset
 * ========================================================================== */

static void TestMisc(void)
{
    size_t i;
    int fAllZero;

    printf("\n0x10047340 and BrUiHook84Reset\n");

    World();
    memset(g_scratchB, 0xFF, sizeof(g_scratchB));
    g_br73.nAA28A4     = 3;
    g_brHook84.bAA26F5 = 1;
    CHECK(BrUiHook84_10047340(g_pCtl) == 1, "0x10047340 returns 1");
    fAllZero = 1;
    for (i = 0; i < 32; i++)
        if (g_scratchB[i] != 0) fAllZero = 0;
    CHECK(fAllZero,
          "0x10047340's rep stosd fills ZERO -- checked, not assumed");
    CHECK((unsigned char)g_scratchB[32] == 0xFFu,
          "0x10047340 clears exactly 32 bytes, not the whole buffer");
    CHECK(g_br73.nAA28A4 == 0, "0x10047340 clears 0x10AA28A4");
    CHECK(g_brHook84.bAA26F5 == 0, "0x10047340 clears the BYTE 0x10AA26F5");

    /* Bounded: a scratch buffer smaller than the original's 32 must not be
     * overrun. */
    World();
    memset(g_scratchB, 0xFF, sizeof(g_scratchB));
    g_br73.cbScratch = 4;
    CHECK(BrUiHook84_10047340(g_pCtl) == 1, "bounded 0x10047340 returns 1");
    CHECK((unsigned char)g_scratchB[4] == 0xFFu,
          "0x10047340 is bounded by cbScratch (DEVIATION)");

    /* Reset drops every reference and frees nothing -- same contract as
     * BrUiHook81Reset. */
    World();
    g_brHook84.pAA2930 = g_pDest;
    g_brHook84.pAA2970 = g_pDest;
    g_brHook84.nAA28E0 = 7;
    g_brHook84.bAA26F5 = 1;
    BrUiHook84Reset();
    CHECK(g_brHook84.pAA2930 == NULL && g_brHook84.pAA2970 == NULL
       && g_brHook84.nAA28E0 == 0 && g_brHook84.bAA26F5 == 0,
          "BrUiHook84Reset clears every word it owns");
    CHECK(g_pDest->pVtbl == &g_phaseVtbl,
          "BrUiHook84Reset frees nothing -- the phase is still there");
}

/* ==========================================================================
 * 6. A transcript.
 *
 * `brally -keys` cannot reach any of these hooks: the harness installs into
 * BrUi73Hooks only, and port/host/br_wire71.c / br_wire72.c leave BrS71Hooks
 * and BrUi72Hooks all-NULL.  Wiring them is a host change and this pass does
 * not own port/host.  So the state change is shown here instead, in the same
 * before/after form `-keys` prints, driven through the INSTALLED table rather
 * than by calling the hook directly -- which is what makes it evidence that
 * the installer works and not just that the body does.
 * ========================================================================== */

static const char *Name(const BrPhase_ *p)
{
    if (p == NULL)          return "NULL";
    if (p == g_pCur)        return "current";
    if (p == g_pRoot)       return "root(0x10AA2908)";
    if (p == g_pDest)       return "dest";
    if (p == g_pOther)      return "other";
    return "?";
}

static void Transcript(void)
{
    BrUi72Hooks h72;
    BrS71Hooks  h71;

    printf("\ntranscript -- 0x1004E830's \"back\" row (caption string 0x0C),\n"
           "              slice6_72.c:1495, hook 0x10046710\n");

    memset(&h72, 0, sizeof(h72));
    BrUiHook84Install72(&h72);

    World();
    /* What the builder does: the row's +0x08 comes out of the table. */
    g_pCtl->pfn08      = h72.p10046710;
    g_pCtl->pfn0C      = h72.p10047360;
    g_brHook81.pAA2918 = g_pDest;       /* where 0x10046710 goes */
    g_brHook84.pAA2988 = g_pOther;      /* what it drops */

    printf("  '.'  pfn08=%s pfn0C=%s  current=%s  0x10AA2988=%s\n",
           g_pCtl->pfn08 != NULL ? "0x10046710" : "NULL",
           g_pCtl->pfn0C != NULL ? "0x10047360" : "NULL",
           Name(g_nav.pAA2904), Name(g_brHook84.pAA2988));
    CHECK(g_pCtl->pfn08 != NULL,
          "before this pass the row's +0x08 was NULL and 'j' did nothing");

    {
        int32_t r = g_pCtl->pfn08(g_pCtl);
        printf("  'j'  hook returned %d  ** current=%s  0x10AA2988=%s\n",
               (int)r, Name(g_nav.pAA2904), Name(g_brHook84.pAA2988));
        CHECK(r == 0, "a 0 from +0x08 is what stops 0x10048180's frame");
    }
    CHECK(g_nav.pAA2904 == g_pDest,
          "the CURRENT PHASE MOVED -- this row now navigates");
    CHECK(g_brHook84.pAA2988 == NULL, "...and the singleton was dropped");

    printf("\ntranscript -- 0x10049F40's caption-0x12 row,\n"
           "              slice6_71.c:298, hook 0x10046FC0\n");
    memset(&h71, 0, sizeof(h71));
    BrUiHook84Install71(&h71);

    World();
    g_pCtl->pfn08      = h71.p10046FC0;
    g_brHook81.pAA292C = g_pDest;
    printf("  '.'  current=%s\n", Name(g_nav.pAA2904));
    {
        int32_t r = g_pCtl->pfn08(g_pCtl);
        printf("  'j'  hook returned %d  ** current=%s\n",
               (int)r, Name(g_nav.pAA2904));
        CHECK(r == 0, "0x10046FC0 returns 0 (CONFLICT 1)");
    }
    CHECK(g_nav.pAA2904 == g_pDest, "the CURRENT PHASE MOVED");
}

int main(void)
{
    TestInstall();
    TestAdapters();
    TestPlainLeaves();
    TestOddLeaves();
    TestNameResets();
    TestInstallers();
    TestMisc();
    Transcript();

    if (g_fail == 0)
        printf("\ntest_slice8_84: all checks passed\n");
    else
        printf("\ntest_slice8_84: %d FAILURE(S)\n", g_fail);
    return g_fail != 0;
}
