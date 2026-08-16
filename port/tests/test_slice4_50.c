/* test_slice4_50.c -- behaviour tests for slice4_50.c.
 *
 * The build line links ONLY this file and slice4_50.c, so every cross-slice
 * callee it uses is stood in for below. All the stand-ins are marked
 * STAND-IN; none of them is a port of anything.
 *
 * The assertions are properties of the original, not of this port:
 *   - which globals are written on which path, and in what order
 *   - the argument lists that are transcribed verbatim (a dropped or
 *     reordered argument is the failure mode that matters here)
 *   - the lazy-create invariant of the two screen installers: the enter hook
 *     runs exactly once no matter how often the installer is called
 *   - the gates that make a routine do nothing at all
 *   - round trips (BrSprintf's return == strlen of what it wrote)
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "slice4_50.h"

/* ==========================================================================
 * STAND-INS
 * ========================================================================== */

/* --- slice2_25.h globals ------------------------------------------------- */
BrOptObj      *g_brPAA2904;
BrDPlay       *g_brP277B40;
BrOptUi       *g_brPA9D008;
BrOptFlagObj  *g_brPAA29D8;
BrObj29D4     *g_brPAA29D4;
int32_t        g_br22AF18;
int32_t        g_brA9D000;
int32_t        g_brAA288C;
int32_t        g_br22B34C;

/* --- 0x10035BBA (slice2_19.c's BrLogSet) --------------------------------- */
static void *g_pLogArg;
static int    g_cLogSet;
void BrLogSet(void *p) { g_pLogArg = p; g_cLogSet++; }        /* STAND-IN */

/* --- 0x10072AF0 (slice1_08.c's BrSndPlaySimple) -------------------------- */
static int32_t  g_sndGroup;
static uint32_t g_sndPacked;
static int      g_cSnd;
int32_t BrSndPlaySimple(int32_t group, uint32_t packed)       /* STAND-IN */
{
    g_sndGroup = group; g_sndPacked = packed; g_cSnd++;
    return 1;
}

/* --- 0x10030930 (br_mat.c's BrMat4Perspective) --------------------------- */
static struct {
    BrMat4         *pM;
    unsigned short *pPerspNorm;
    float           fovy, aspect, n, f;
    int             cCall;
} g_persp;
static int g_perspRc;
int BrMat4Perspective(BrMat4 *pM, unsigned short *pPerspNorm,               /* STAND-IN */
                      float fovyDegrees, float aspect, float n, float f)
{
    g_persp.pM = pM; g_persp.pPerspNorm = pPerspNorm;
    g_persp.fovy = fovyDegrees; g_persp.aspect = aspect;
    g_persp.n = n; g_persp.f = f; g_persp.cCall++;
    if (pPerspNorm != NULL) { *pPerspNorm = 1; }
    return g_perspRc;
}

/* --- 0x1007DFE0 operator new / 0x10048710 ctor --------------------------- */
static int g_fNewFails;
static int g_cNew;
static int g_cCtor;
void *BrOperatorNew(uint32_t cb)                               /* STAND-IN */
{
    g_cNew++;
    if (g_fNewFails) { return NULL; }
    /* The real one does not zero; malloc does not either. */
    return malloc((size_t)cb);
}
BrOptObj *BrOptObjCtor(BrOptObj *pThis)                        /* STAND-IN */
{
    g_cCtor++;
    memset(pThis, 0, sizeof *pThis);
    return pThis;
}

/* --- the two enter hooks ------------------------------------------------- */
static BrOptObj *g_pEnterA, *g_pEnterB;
static int       g_cEnterA,  g_cEnterB;
static void EnterA(BrOptObj *p) { g_pEnterA = p; g_cEnterA++; }
static void EnterB(BrOptObj *p) { g_pEnterB = p; g_cEnterB++; }

/* --- 0x100419D0 / 0x1003E510 --------------------------------------------- */
static void *g_p419D0Arg;
static int   g_c419D0, g_c3E510;
static int   g_orderTicks;
static int   g_order419D0, g_order3E510;
void BrExt_100419D0(void *p)                                   /* STAND-IN */
{
    g_p419D0Arg = p; g_c419D0++; g_order419D0 = ++g_orderTicks;
}
void BrSub1003E510(void)                                       /* STAND-IN */
{
    g_c3E510++; g_order3E510 = ++g_orderTicks;
}

/* --- network ------------------------------------------------------------- */
static void *g_hLocked;
static int   g_cLock, g_cUnlock, g_orderLock, g_orderUnlock;
void BrNetMutexLock(void *h)   { g_hLocked = h; g_cLock++;   g_orderLock   = ++g_orderTicks; }
void BrNetMutexUnlock(void *h) { (void)h;       g_cUnlock++; g_orderUnlock = ++g_orderTicks; }

static uint32_t g_cActive, g_cPlayers;
uint32_t BrEntityCountActive(const void *pv, int32_t c)        /* STAND-IN */
{
    (void)pv; (void)c; return g_cActive;
}
uint32_t BrDPlayGetCurrentPlayers(void) { return g_cPlayers; } /* STAND-IN */

static struct {
    BrDPlay **ppDPlay;
    int32_t   a1, a2;
    uint8_t   r, g, b;
    int32_t   a6;
    char     *pszText;
    int32_t   a8, a9;
    int       cCall;
} g_send;
void BrNetSend4760(BrDPlay **ppDPlay, int32_t a1, int32_t a2,  /* STAND-IN */
                   uint8_t r, uint8_t g, uint8_t b,
                   int32_t a6, char *pszText, int32_t a8, int32_t a9)
{
    g_send.ppDPlay = ppDPlay; g_send.a1 = a1; g_send.a2 = a2;
    g_send.r = r; g_send.g = g; g_send.b = b;
    g_send.a6 = a6; g_send.pszText = pszText;
    g_send.a8 = a8; g_send.a9 = a9; g_send.cCall++;
}

/* --- DirectPlay host / join ---------------------------------------------- */
static int32_t g_hrHost, g_hrJoinBlob;
static int32_t g_aHrJoin[4];      /* consumed in order by BrSub1003C740 */
static int     g_iHrJoin, g_cJoin;
static int     g_cDescFill, g_c71550, g_c5B10, g_cBF60, g_cC020, g_cCE80;
static int     g_descWasZero;
static char    g_szJoinName[BR50_DPNAME_SIZE];
static void   *g_pJoinBlobSeen;

void BrSub1003D130(void *pDesc)                                /* STAND-IN */
{
    const unsigned char *p = (const unsigned char *)pDesc;
    uint32_t i;
    g_descWasZero = 1;
    for (i = 0; i < BR50_DPDESC_SIZE; ++i) { if (p[i] != 0) { g_descWasZero = 0; } }
    g_cDescFill++;
}
int32_t BrSub1003C5C0(BrDPlay *pDPlay, void *pDesc, BrOptUi *pUi)  /* STAND-IN */
{
    (void)pDPlay; (void)pDesc; (void)pUi; return g_hrHost;
}
int32_t BrSub1003D030(void *pBlob) { g_pJoinBlobSeen = pBlob; return g_hrJoinBlob; }
int32_t BrSub1003C740(BrDPlay *pDPlay, void *pBlob,            /* STAND-IN */
                      char *pszName, BrOptUi *pUi)
{
    (void)pDPlay; (void)pBlob; (void)pUi;
    strncpy(g_szJoinName, pszName, sizeof g_szJoinName - 1);
    g_cJoin++;
    return g_aHrJoin[g_iHrJoin++];
}
void BrSub10071550(void)      { g_c71550++; }
void BrSub10005B10(int32_t a) { (void)a; g_c5B10++; }
void BrSub1003BF60(void)      { g_cBF60++; }
void BrSub1003C020(void)      { g_cC020++; }
void BrSub1003CE80(void)      { g_cCE80++; }

static int g_c42AF0, g_rc42AF0;
static int32_t Fn42AF0(void *p) { (void)p; g_c42AF0++; return g_rc42AF0; }

static const char *g_pszUser = "tester";
int32_t BrPlatGetUserName(char *pszBuf, uint32_t *pcb)         /* STAND-IN */
{
    assert(*pcb == BR50_DPNAME_CB);
    strcpy(pszBuf, g_pszUser);
    *pcb = (uint32_t)strlen(g_pszUser) + 1u;
    return 1;
}

/* --- 0x1000C4D0 (slice1_03.c's BrComCallLocked68) ------------------------ */
static struct {
    BrComObj *pThis;
    void     *a2, *a3, *a4, *a6;
    int32_t   aPacket[2];
    int       cCall;
} g_com;
int BrComCallLocked68(BrComObj *pThis, void *a2, void *a3,     /* STAND-IN */
                      void *a4, void *a5, void *a6)
{
    g_com.pThis = pThis; g_com.a2 = a2; g_com.a3 = a3;
    g_com.a4 = a4; g_com.a6 = a6;
    memcpy(g_com.aPacket, a5, sizeof g_com.aPacket);
    g_com.cCall++;
    return 0;
}

/* --- the clock ----------------------------------------------------------- */
static int64_t g_qpcValue;
static int32_t g_qpfRc = 1, g_qpcRc = 1;
static int64_t g_qpfValue = 1000000;   /* 1 MHz */
static uint32_t g_fallbackMs = 0x5A5A;
int32_t BrPlatQueryPerfFreq(int64_t *p)    { *p = g_qpfValue; return g_qpfRc; }
int32_t BrPlatQueryPerfCounter(int64_t *p) { *p = g_qpcValue; return g_qpcRc; }
uint32_t BrPlatTimeGetTime(void)           { return g_fallbackMs; }

/* ==========================================================================
 * TESTS
 * ========================================================================== */

static int g_cFail;
#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);         \
            g_cFail++;                                                     \
        }                                                                  \
    } while (0)

/* --- 0x10035BBA ---------------------------------------------------------- */
static void test_fatal(void)
{
    const char *psz = "HUGE GLIST ERROR";
    g_cLogSet = 0;
    BrFatal(psz);
    /* It RETURNS -- reaching this line at all is the point. */
    CHECK(g_cLogSet == 1);
    CHECK(g_pLogArg == (const void *)psz);

    BrFatal(NULL);
    CHECK(g_cLogSet == 2);
    CHECK(g_pLogArg == NULL);
}

/* --- 0x1007C830 ---------------------------------------------------------- */
static void test_sprintf(void)
{
    char aBuf[128];
    int  n;

    n = BrSprintf(aBuf, "File %s missing", "tracks/x.rca");
    CHECK(strcmp(aBuf, "File tracks/x.rca missing") == 0);
    /* Round trip: the count excludes the NUL that was also written. */
    CHECK(n == (int)strlen(aBuf));
    CHECK(aBuf[n] == '\0');

    n = BrSprintf(aBuf, "%s%s", "tracks/", "a.rca");
    CHECK(strcmp(aBuf, "tracks/a.rca") == 0);
    CHECK(n == (int)strlen(aBuf));

    /* The one format the two DirectPlay routines use. */
    n = BrSprintf(aBuf, "error 0x%08X", 0x88770820u);
    CHECK(strcmp(aBuf, "error 0x88770820") == 0);
    CHECK(n == (int)strlen(aBuf));

    n = BrSprintf(aBuf, "");
    CHECK(n == 0 && aBuf[0] == '\0');
}

/* --- 0x10072AF0 ---------------------------------------------------------- */
static void test_snd(void)
{
    g_cSnd = 0;
    BrSub10072AF0(2, 0x200020);
    CHECK(g_cSnd == 1);
    CHECK(g_sndGroup == 2);
    CHECK(g_sndPacked == 0x200020u);

    BrSub10072AF0(1, 0x200020);
    CHECK(g_sndGroup == 1);
}

/* --- 0x10034C51 ---------------------------------------------------------- */
static void test_hook(void)
{
    int a = 0, b = 0;

    g_brHook6C0964 = NULL;
    /* GOTCHA reproduced: with the global unset, NULL "is current". */
    CHECK(BrHookIsCurrent(NULL) == 1);
    CHECK(BrHookIsCurrent(&a) == 0);

    g_brHook6C0964 = &a;
    CHECK(BrHookIsCurrent(&a) == 1);
    CHECK(BrHookIsCurrent(&b) == 0);
    CHECK(BrHookIsCurrent(NULL) == 0);
    /* The result is only ever 0 or 1. */
    CHECK((BrHookIsCurrent(&a) | BrHookIsCurrent(&b)) == 1);

    g_brHook6C0964 = NULL;
}

/* --- 0x10030930 ---------------------------------------------------------- */
static void test_persp(void)
{
    BrMat4   m;
    uint16_t perspNorm = 0xFFFF;
    int      rc;

    memset(&g_persp, 0, sizeof g_persp);
    g_perspRc = 0;
    rc = BrMat4Perspective7(&m, &perspNorm, 45.0f, 1.3333334f,
                            10.0f, 2000.0f, 1.0f);
    CHECK(rc == 0);
    CHECK(g_persp.cCall == 1);
    /* Six arguments through verbatim; `scale` is the seventh and is dead. */
    CHECK(g_persp.pM == &m);
    CHECK(g_persp.pPerspNorm == (unsigned short *)&perspNorm);
    CHECK(g_persp.fovy == 45.0f);
    CHECK(g_persp.aspect == 1.3333334f);
    CHECK(g_persp.n == 10.0f);
    CHECK(g_persp.f == 2000.0f);
    CHECK(perspNorm == 1);          /* hardcoded, never computed */

    /* A different `scale` must change nothing at all. */
    g_persp.cCall = 0;
    BrMat4Perspective7(&m, &perspNorm, 45.0f, 1.3333334f, 10.0f, 2000.0f,
                       7.5f);
    CHECK(g_persp.cCall == 1);
    CHECK(g_persp.fovy == 45.0f && g_persp.n == 10.0f && g_persp.f == 2000.0f);

    g_perspRc = -1;
    CHECK(BrMat4Perspective7(&m, &perspNorm, 45.0f, 1.0f, 1.0f, 1.0f, 1.0f)
          == -1);
    g_perspRc = 0;
}

/* --- 0x10044E20 and 0x10043BF0 ------------------------------------------- */
static void reset_installers(void)
{
    free(g_brPAA2968);
    free(g_brPAA2958);
    g_brPAA2968 = NULL;
    g_brPAA2958 = NULL;
    g_brPAA2904 = NULL;
    g_cNew = g_cCtor = 0;
    g_cEnterA = g_cEnterB = 0;
    g_pEnterA = g_pEnterB = NULL;
    g_c419D0 = g_c3E510 = 0;
    g_fNewFails = 0;
    g_brOptEnterHooks.p1005A6E0 = EnterA;
    g_brOptEnterHooks.p100563E0 = EnterB;
}

static void test_menu44E20(void)
{
    BrOptObj *pFirst;

    reset_installers();
    g_brACEE8C = 0x11112222;
    g_brACEE94 = 0x33334444;
    g_brAA28CC = g_brAA28C8 = 0;

    BrMenuSub10044E20(0);
    CHECK(g_brAA28CC == 0x11112222);
    CHECK(g_brAA28C8 == 0x33334444);
    CHECK(g_cNew == 1 && g_cCtor == 1);
    CHECK(g_brPAA2968 != NULL);
    CHECK(g_brPAA2904 == g_brPAA2968);
    CHECK(g_cEnterA == 1 && g_pEnterA == g_brPAA2968);
    CHECK(g_brPAA2968->pfnEnter == EnterA);
    CHECK(g_brPAA2968->f0C == 1 && g_brPAA2968->f68 == 1);
    /* 0x10043BF0's slot is untouched. */
    CHECK(g_brPAA2958 == NULL && g_cEnterB == 0);

    /* Cached path: no second object, no second hook call, but the two
     * copies at the top still happen. */
    pFirst = g_brPAA2968;
    g_brACEE8C = 0x55556666;
    g_brPAA2904 = NULL;
    BrMenuSub10044E20(0);
    CHECK(g_brAA28CC == 0x55556666);
    CHECK(g_cNew == 1 && g_cCtor == 1);
    CHECK(g_cEnterA == 1);
    CHECK(g_brPAA2968 == pFirst);
    CHECK(g_brPAA2904 == pFirst);

    /* The argument is dead: any value behaves identically. */
    BrMenuSub10044E20(-12345);
    CHECK(g_cNew == 1 && g_cEnterA == 1);

    /* Allocation failure leaves both published slots NULL and does not
     * dereference anything. */
    reset_installers();
    g_fNewFails = 1;
    g_brPAA2904 = (BrOptObj *)(void *)&pFirst;   /* poison */
    BrMenuSub10044E20(0);
    CHECK(g_brPAA2968 == NULL);
    CHECK(g_brPAA2904 == NULL);
    CHECK(g_cEnterA == 0);
    reset_installers();
}

static void test_sub43BF0(void)
{
    BrOptObj *pFirst;

    reset_installers();
    g_brP0AD300 = (void *)&pFirst;
    g_orderTicks = 0;

    BrSub10043BF0(NULL);
    /* Both prologue calls happen, in this order, before the object exists. */
    CHECK(g_c419D0 == 1 && g_c3E510 == 1);
    CHECK(g_p419D0Arg == g_brP0AD300);
    CHECK(g_order419D0 < g_order3E510);
    CHECK(g_brPAA2958 != NULL);
    CHECK(g_brPAA2904 == g_brPAA2958);
    CHECK(g_cEnterB == 1 && g_pEnterB == g_brPAA2958);
    CHECK(g_brPAA2958->pfnEnter == EnterB);
    CHECK(g_brPAA2958->f0C == 1 && g_brPAA2958->f68 == 1);
    CHECK(g_brPAA2968 == NULL && g_cEnterA == 0);

    /* Cached path still runs the two prologue calls every time. */
    pFirst = g_brPAA2958;
    BrSub10043BF0(NULL);
    CHECK(g_c419D0 == 2 && g_c3E510 == 2);
    CHECK(g_cNew == 1 && g_cEnterB == 1);
    CHECK(g_brPAA2958 == pFirst && g_brPAA2904 == pFirst);

    reset_installers();
}

/* --- 0x100053F0 ---------------------------------------------------------- */
static void reset_net(void)
{
    memset(&g_send, 0, sizeof g_send);
    g_cLock = g_cUnlock = 0;
    g_orderTicks = 0;
}

static void test_netflush(void)
{
    static char szText[8] = "hi";

    g_brH221324 = (void *)&szText[0];
    g_brPB4E2E8 = szText;
    g_br094294  = 5;
    g_br22B34C  = 0x1234;
    g_br277B48  = 0x99;
    g_brAD0854[0] = 0xFF; g_brAD0854[1] = 0x80; g_brAD0854[2] = 0x00;

    /* Gated off: the mutex is still taken and released, in that order. */
    reset_net();
    g_br22AAA8 = 0;
    BrNetSendFlush();
    CHECK(g_cLock == 1 && g_cUnlock == 1);
    CHECK(g_orderLock < g_orderUnlock);
    CHECK(g_hLocked == g_brH221324);
    CHECK(g_send.cCall == 0);

    /* Enabled, but the two counts disagree. */
    reset_net();
    g_br22AAA8 = 1;
    g_cActive  = 4;
    g_cPlayers = 3;
    BrNetSendFlush();
    CHECK(g_cLock == 1 && g_cUnlock == 1);
    CHECK(g_send.cCall == 0);

    /* The 0xFFFF failure sentinel of 0x1000C670 must never match a count. */
    reset_net();
    g_cActive  = 4;
    g_cPlayers = 0xFFFF;
    BrNetSendFlush();
    CHECK(g_send.cCall == 0);

    /* Counts agree: the ten arguments go through verbatim. */
    reset_net();
    g_cActive = g_cPlayers = 4;
    BrNetSendFlush();
    CHECK(g_send.cCall == 1);
    CHECK(g_send.ppDPlay == &g_brP277B40);    /* the ADDRESS of the global */
    CHECK(g_send.a1 == 5);
    CHECK(g_send.a2 == 0x1234);
    CHECK(g_send.r == 0xFF && g_send.g == 0x80 && g_send.b == 0x00);
    CHECK(g_send.a6 == 0x99);
    CHECK(g_send.pszText == szText);
    CHECK(g_send.a8 == 3 && g_send.a9 == 0);

    /* Zero players on both sides still counts as agreement. */
    reset_net();
    g_cActive = g_cPlayers = 0;
    BrNetSendFlush();
    CHECK(g_send.cCall == 1);

    g_br22AAA8 = 0;
}

/* --- 0x1003C150 ---------------------------------------------------------- */
static BrDPlay  g_dplay;
static BrOptUi  g_optui;

static void test_host(void)
{
    g_brPA9D008 = &g_optui;

    /* No interface: nothing at all happens. */
    g_brP277B40 = NULL;
    g_cDescFill = g_c71550 = g_c5B10 = 0;
    g_br22AF18 = 77;
    BrSub1003C150();
    CHECK(g_cDescFill == 0 && g_c71550 == 0 && g_c5B10 == 0);
    CHECK(g_br22AF18 == 77);

    /* Failure: the descriptor is filled but no state moves. The message is
     * formatted into a stack buffer and dropped -- nothing observable. */
    g_brP277B40 = &g_dplay;
    g_hrHost = (int32_t)0x80004005u;      /* E_FAIL, negative */
    g_cDescFill = g_c71550 = g_c5B10 = 0;
    g_br22AF18 = 77;
    BrSub1003C150();
    CHECK(g_cDescFill == 1);
    CHECK(g_descWasZero == 1);            /* rep stosd zeroes all 0xCC bytes */
    CHECK(g_c71550 == 0 && g_c5B10 == 0);
    CHECK(g_br22AF18 == 77);

    /* Success. */
    g_hrHost = 0;
    g_cDescFill = g_c71550 = g_c5B10 = 0;
    BrSub1003C150();
    CHECK(g_cDescFill == 1);
    CHECK(g_br22AF18 == 2);
    CHECK(g_c71550 == 1 && g_c5B10 == 1);

    /* hr == 0 is a success; only hr < 0 fails. */
    g_hrHost = 0x7FFFFFFF;
    g_br22AF18 = 0;
    BrSub1003C150();
    CHECK(g_br22AF18 == 2);
}

/* --- 0x1003C260 ---------------------------------------------------------- */
static BrOptFlagObj g_flag29D8;
static BrObj29D4   *g_pObj29D4;

static void reset_join(void)
{
    g_iHrJoin = g_cJoin = 0;
    g_c42AF0 = g_cBF60 = g_cC020 = g_cCE80 = g_c5B10 = 0;
    g_aHrJoin[0] = g_aHrJoin[1] = g_aHrJoin[2] = g_aHrJoin[3] = 0;
    g_hrJoinBlob = 0;
    g_br22AF18 = 0;
    g_brA9D000 = 0;
    g_szJoinName[0] = '\0';
    g_brP277B40 = &g_dplay;
    g_brPAA29D8 = &g_flag29D8;
    g_brPAA29D4 = g_pObj29D4;
    g_pObj29D4->f1E164 = 1;
    g_brPfn42AF0_1 = Fn42AF0;
    g_rc42AF0 = 1;
}

static void test_join(void)
{
    reset_join();
    g_brP277B40 = NULL;
    CHECK(BrSub1003C260() == 0);          /* the only 0 that is not a failure */
    CHECK(g_cJoin == 0);

    /* Both early-outs return 1 -- "nothing to do" is success here. */
    reset_join();
    g_brPAA29D8 = NULL;
    CHECK(BrSub1003C260() == 1);
    CHECK(g_cJoin == 0 && g_br22AF18 == 0);

    reset_join();
    g_pObj29D4->f1E164 = 0;
    CHECK(BrSub1003C260() == 1);
    CHECK(g_cJoin == 0 && g_br22AF18 == 0);

    /* Already connected: the whole join is skipped but the tail still runs. */
    reset_join();
    g_brA9D000 = 1;
    CHECK(BrSub1003C260() == 1);
    CHECK(g_cJoin == 0);
    CHECK(g_br22AF18 == 1 && g_c5B10 == 1 && g_cCE80 == 1);

    /* Plain success. */
    reset_join();
    CHECK(BrSub1003C260() == 1);
    CHECK(g_cJoin == 1);
    CHECK(strcmp(g_szJoinName, "tester") == 0);
    CHECK(g_pJoinBlobSeen != NULL);
    CHECK(g_br22AF18 == 1 && g_c5B10 == 1 && g_cCE80 == 1);
    CHECK(g_cBF60 == 0 && g_cC020 == 0);

    /* Blob failure short-circuits to the teardown without ever joining. */
    reset_join();
    g_hrJoinBlob = -1;
    CHECK(BrSub1003C260() == 0);
    CHECK(g_cJoin == 0);
    CHECK(g_cBF60 == 1 && g_cC020 == 1);
    CHECK(g_br22AF18 == 0);

    /* Join failure: teardown, no state change. */
    reset_join();
    g_aHrJoin[0] = (int32_t)0x80004005u;
    CHECK(BrSub1003C260() == 0);
    CHECK(g_cJoin == 1);
    CHECK(g_cBF60 == 1 && g_cC020 == 1);
    CHECK(g_br22AF18 == 0);

    /* 0x88770820 with 0x10042AF0 saying yes: exactly one retry, then win. */
    reset_join();
    g_aHrJoin[0] = (int32_t)0x88770820u;
    g_aHrJoin[1] = 0;
    CHECK(BrSub1003C260() == 1);
    CHECK(g_cJoin == 2);
    CHECK(g_c42AF0 == 1);
    CHECK(g_br22AF18 == 1);

    /* 0x88770820 with 0x10042AF0 saying no: bail out WITHOUT the teardown
     * the ordinary failure path runs. That asymmetry is the original's. */
    reset_join();
    g_aHrJoin[0] = (int32_t)0x88770820u;
    g_rc42AF0 = 0;
    CHECK(BrSub1003C260() == 0);
    CHECK(g_cJoin == 1);
    CHECK(g_c42AF0 == 1);
    CHECK(g_cBF60 == 0 && g_cC020 == 0);
    CHECK(g_br22AF18 == 0);

    /* And a retry that also fails does take the teardown. */
    reset_join();
    g_aHrJoin[0] = (int32_t)0x88770820u;
    g_aHrJoin[1] = (int32_t)0x80004005u;
    CHECK(BrSub1003C260() == 0);
    CHECK(g_cJoin == 2);
    CHECK(g_cBF60 == 1 && g_cC020 == 1);
}

/* --- 0x1003D950 ---------------------------------------------------------- */
static void test_send950(void)
{
    void *aSlots[3];
    BrComObj comObj;
    void *pArg = (void *)&aSlots[1];

    aSlots[0] = &comObj;
    aSlots[1] = NULL;
    aSlots[2] = pArg;

    g_brAA288C = 0;

    memset(&g_com, 0, sizeof g_com);
    BrSub1003D950(NULL, 7);
    CHECK(g_com.cCall == 0);

    aSlots[0] = NULL;
    BrSub1003D950((BrOptUi *)aSlots, 7);
    CHECK(g_com.cCall == 0);
    aSlots[0] = &comObj;

    /* The gate. */
    g_brAA288C = 1;
    BrSub1003D950((BrOptUi *)aSlots, 7);
    CHECK(g_com.cCall == 0);
    g_brAA288C = 0;

    BrSub1003D950((BrOptUi *)aSlots, 0x2A);
    CHECK(g_com.cCall == 1);
    CHECK(g_com.pThis == &comObj);
    CHECK(g_com.a2 == pArg);
    CHECK(g_com.a3 == (void *)(uintptr_t)0u);
    CHECK(g_com.a4 == (void *)(uintptr_t)1u);
    CHECK(g_com.a6 == (void *)(uintptr_t)8u);
    CHECK(g_com.aPacket[0] == (int32_t)0x60000002u);
    CHECK(g_com.aPacket[1] == 0x2A);
    /* The payload really is 8 bytes: two dwords, header first. */
    CHECK(sizeof g_com.aPacket == 8);

    BrSub1003D950((BrOptUi *)aSlots, -1);
    CHECK(g_com.aPacket[1] == -1);
    CHECK(g_com.aPacket[0] == (int32_t)0x60000002u);
}

/* --- 0x10075020 ---------------------------------------------------------- */
static void reset_clock(void)
{
    g_br0BBAD4  = 1;
    g_br18AB120 = 0;
    g_br18AB128 = 0;
    g_br18AB130 = 0;
    g_qpfRc = g_qpcRc = 1;
    g_qpfValue = 1000000;      /* 1 MHz => 1 tick == 1 microsecond */
    g_qpcValue = 0;
}

static void test_clock(void)
{
    int32_t a, b;

    /* The first call calibrates against itself, so it reads as zero. */
    reset_clock();
    g_qpcValue = 123456789;
    a = BrSub10075020();
    CHECK(a == 0);
    CHECK(g_br0BBAD4 == 0);            /* calibration is one-shot */
    CHECK(g_br18AB120 == 1000000);

    /* 1 MHz: advancing by 2500 ticks is 2.5 ms, and the +500 rounds it up. */
    g_qpcValue += 2500;
    b = BrSub10075020();
    CHECK(b == 3);
    /* Monotonic. */
    CHECK(b >= a);
    g_qpcValue += 1000000;
    CHECK(BrSub10075020() == 1003);

    /* The +500 is applied to base and sample alike, so it cancels on an
     * exact-millisecond delta. */
    reset_clock();
    g_qpcValue = 7;
    (void)BrSub10075020();
    g_qpcValue += 5000;
    CHECK(BrSub10075020() == 5);

    /* QueryPerformanceFrequency failing pins the fallback permanently. */
    reset_clock();
    g_qpfRc = 0;
    CHECK(BrSub10075020() == (int32_t)g_fallbackMs);
    g_qpcValue += 1000000;
    CHECK(BrSub10075020() == (int32_t)g_fallbackMs);

    /* A per-call QueryPerformanceCounter failure falls back too, even though
     * the frequency query succeeded. */
    reset_clock();
    (void)BrSub10075020();
    g_qpcRc = 0;
    CHECK(BrSub10075020() == (int32_t)g_fallbackMs);
    g_qpcRc = 1;
    g_qpcValue += 2000000;
    CHECK(BrSub10075020() == 2000);

    reset_clock();
}

int main(void)
{
    g_pObj29D4 = (BrObj29D4 *)calloc(1, sizeof(BrObj29D4));
    if (g_pObj29D4 == NULL) { return 1; }

    test_fatal();
    test_sprintf();
    test_snd();
    test_hook();
    test_persp();
    test_menu44E20();
    test_sub43BF0();
    test_netflush();
    test_host();
    test_join();
    test_send950();
    test_clock();

    free(g_pObj29D4);

    if (g_cFail != 0) {
        printf("slice4_50: %d failure(s)\n", g_cFail);
        return 1;
    }
    printf("slice4_50: all tests passed\n");
    return 0;
}
