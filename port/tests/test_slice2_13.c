/* test_slice2_13.c -- behaviour tests for the agent-13 packet.
 *
 * Everything below the STAND-INS banner is a test-only substitute for a
 * function that lives in another packet. None of it is decompiled code.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "slice2_13.h"
#include "slice1_03.h"

static int g_cFail;
static int g_cRun;

#define CHECK(cond)                                                        \
    do {                                                                   \
        ++g_cRun;                                                          \
        if (!(cond)) {                                                     \
            ++g_cFail;                                                     \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);         \
        }                                                                  \
    } while (0)

/* ==========================================================================
 * STAND-INS
 * ========================================================================== */

static int   g_cErrorf;
static char  g_szLastErr[256];

void BrErrorf(const char *pszFmt, ...)
{
    ++g_cErrorf;
    snprintf(g_szLastErr, sizeof(g_szLastErr), "%s", pszFmt);
}

static uint32_t g_id10071480, g_id10005FE0;
static int      g_c10071480, g_c10005FE0, g_c100360F0, g_c1003CE80;
static uint32_t g_a100360F0[4];

void BrSub10071480(uint32_t id) { ++g_c10071480; g_id10071480 = id; }
void BrSub10005FE0(uint32_t id) { ++g_c10005FE0; g_id10005FE0 = id; }

void BrSub100360F0(void *pv1, uint32_t f0C, uint32_t f10, uint32_t f08,
                   uint32_t idTo)
{
    (void)pv1;
    ++g_c100360F0;
    g_a100360F0[0] = f0C;
    g_a100360F0[1] = f10;
    g_a100360F0[2] = f08;
    g_a100360F0[3] = idTo;
}

void BrSub1003CE80(void) { ++g_c1003CE80; }

static int      g_c1000BAF0;
static uint32_t g_idFrom1000BAF0;

void BrSub1000BAF0(void *pCtx, const void *pvData, uint32_t cbData,
                   uint32_t idFrom, uint32_t idTo)
{
    (void)pCtx; (void)pvData; (void)cbData; (void)idTo;
    ++g_c1000BAF0;
    g_idFrom1000BAF0 = idFrom;
}

/* 0x1003D0B0: hands back a 0x30-byte record with a known dword at +0x2C. */
static int      g_f1003D0B0Fail;
static void    *g_pv1003D0B0;

int32_t BrSub1003D0B0(BrDPlay4Obj *pObj, void **ppvOut)
{
    unsigned char *pb;

    (void)pObj;
    if (g_f1003D0B0Fail)
        return -1;

    /* the real one hands back GlobalAlloc'd storage, which the caller frees */
    pb = (unsigned char *)calloc(1, 0x30);
    pb[0x2C]     = 0x2A;                /* 42, little-endian */
    *ppvOut      = pb;
    g_pv1003D0B0 = pb;
    return 0;
}

/* slice1_03's 0x1000BEA0. */
static int g_cAppMsgDispatch;

void BrAppMsgDispatch(void *pv1, const BrAppMsg *pMsg, void *pv3, void *pv4,
                      void *pv5)
{
    (void)pv1; (void)pMsg; (void)pv3; (void)pv4; (void)pv5;
    ++g_cAppMsgDispatch;
}

/* Agent 14's free list and lerp (0x102E5ECC / 0x10010B00). */
BrLerpNode *g_pBrLerpFree = NULL;

BrLerpNode *BrLerpNodeAlloc(const BrLerpNode *pFrom, const BrLerpNode *pTo,
                            float t)
{
    BrLerpNode *p = g_pBrLerpFree;
    int         i;

    if (p == NULL)
        return NULL;
    g_pBrLerpFree = p->pNext;
    p->pData      = &p->data[0];

    for (i = 0; i < 8; ++i)
        p->pData[i] = (pTo->pData[i] - pFrom->pData[i]) * t + pFrom->pData[i];

    return p;
}

/* Agent 14's 0x10010D10. Made an identity projection so the tests can drive
 * the clip coordinates directly through f00 / f04. */
void BrScrPtProject(BrScrPt *pPt)
{
    pPt->f0C = pPt->f00;
    pPt->f10 = pPt->f04;
}

/* Agent 14's 0x10010BF0, recorded rather than performed. */
#define KEEP_MAX 64
static int   g_cKeep;
static int   g_aKeepIdx[KEEP_MAX];
static float g_aKeepCx[KEEP_MAX];
static float g_aKeepCy[KEEP_MAX];

void BrScrPtKeepNearest(const BrMat4 *pM, BrScrPt *aOut, int *aFlags, int idx,
                        const BrScrPt *pIn, float cx, float cy,
                        const BrDepthRef *pRef)
{
    (void)pM; (void)aOut; (void)aFlags; (void)pIn; (void)pRef;
    if (g_cKeep < KEEP_MAX) {
        g_aKeepIdx[g_cKeep] = idx;
        g_aKeepCx[g_cKeep]  = cx;
        g_aKeepCy[g_cKeep]  = cy;
    }
    ++g_cKeep;
}

/* The two lower scissor planes, 0x10010960 / 0x10010980. */
float BrPolyDistX(const BrScrPt *pPt) { return pPt->f0C; }
float BrPolyDistY(const BrScrPt *pPt) { return pPt->f10; }

/* ==========================================================================
 * 1. BrPathBaseName
 * ========================================================================== */

static void TestPathBaseName(void)
{
    char sz[64];

    BrPathBaseName("c:\\games\\rally\\car.pod", sz);
    CHECK(strcmp(sz, "car.pod") == 0);

    /* no separator at all -- the whole string is the basename */
    BrPathBaseName("car.pod", sz);
    CHECK(strcmp(sz, "car.pod") == 0);

    /* a single character is the je-at-entry case */
    BrPathBaseName("a", sz);
    CHECK(strcmp(sz, "a") == 0);

    /* a TRAILING separator is not a separator: the scan starts at the last
     * character and only ever inspects the byte before the cursor, so the
     * trailing '\\' comes back as part of the name */
    BrPathBaseName("c:\\games\\", sz);
    CHECK(strcmp(sz, "games\\") == 0);

    BrPathBaseName("a\\", sz);
    CHECK(strcmp(sz, "a\\") == 0);

    /* forward slashes are NOT separators in the original */
    BrPathBaseName("/usr/share/car.pod", sz);
    CHECK(strcmp(sz, "/usr/share/car.pod") == 0);

    /* leading separator */
    BrPathBaseName("\\a", sz);
    CHECK(strcmp(sz, "a") == 0);

    /* the guarded empty-string case (DEVIATION) */
    BrPathBaseName("", sz);
    CHECK(strcmp(sz, "") == 0);

    /* idempotence: the basename of a basename is itself */
    {
        char sz2[64];

        BrPathBaseName("c:\\a\\b\\c.txt", sz);
        BrPathBaseName(sz, sz2);
        CHECK(strcmp(sz, sz2) == 0);
    }
}

/* ==========================================================================
 * 2. BrFileOpenWrite / BrFileWriteChecked
 * ========================================================================== */

static void TestFileHelpers(void)
{
    const char *pszPath = "/tmp/br_slice2_13_test.bin";
    FILE       *pF;
    char        ab[8];
    int         cBefore;

    g_cErrorf = 0;

    /* an unopenable path reports and still returns NULL */
    pF = BrFileOpenWrite("/no-such-dir-slice2-13/x.bin");
    CHECK(pF == NULL);
    CHECK(g_cErrorf == 1);
    CHECK(strcmp(g_szLastErr, "Error opening %s: %s") == 0);

    /* a good path opens for WRITING and reports nothing */
    pF = BrFileOpenWrite(pszPath);
    CHECK(pF != NULL);
    CHECK(g_cErrorf == 1);
    if (pF != NULL) {
        cBefore = g_cErrorf;
        BrFileWriteChecked(pF, "RSea", 4);
        BrFileWriteChecked(pF, "", 0);          /* 0 == 0, no report */
        CHECK(g_cErrorf == cBefore);
        fclose(pF);
    }

    /* it really was a write, and it really was binary-exact */
    pF = fopen(pszPath, "rb");
    CHECK(pF != NULL);
    if (pF != NULL) {
        size_t cb = fread(ab, 1, sizeof(ab), pF);

        CHECK(cb == 4);
        CHECK(memcmp(ab, "RSea", 4) == 0);

        /* writing to a read-only stream is a short count -> it reports, and
         * the message is the original's (wrong) "File read failure" */
        cBefore   = g_cErrorf;
        BrFileWriteChecked(pF, "xx", 2);
        CHECK(g_cErrorf == cBefore + 1);
        CHECK(strcmp(g_szLastErr, "File read failure") == 0);
        fclose(pF);
    }

    remove(pszPath);
}

/* ==========================================================================
 * 3. BrCursorAdvance
 * ========================================================================== */

static void CursorSetup(int32_t step, int32_t pos, int32_t limit,
                        const uint16_t *aStops, int32_t cStops)
{
    BrCursorState *pSt = BrCursorGetState();

    pSt->step   = step;
    pSt->pos    = pos;
    pSt->limit  = limit;
    pSt->aStops = aStops;
    pSt->cStops = cStops;
}

static void TestCursor(void)
{
    static const uint16_t aStops[] = { 14, 51 };
    BrCursorState        *pSt      = BrCursorGetState();

    /* step 0 is "disarmed": nothing at all happens */
    CursorSetup(0, 5, 100, aStops, 2);
    BrCursorAdvance();
    CHECK(pSt->pos == 5);
    CHECK(pSt->step == 0);

    /* lands on a stop value */
    CursorSetup(7, 0, 100, aStops, 2);
    BrCursorAdvance();
    CHECK(pSt->pos == 14);
    CHECK(pSt->step == 0);       /* always disarmed on the way out */

    /* with no stop table the only exit is landing exactly on 0, which the
     * wrap guarantees */
    CursorSetup(5, 0, 10, aStops, 0);
    BrCursorAdvance();
    CHECK(pSt->pos == 0);
    CHECK(pSt->step == 0);

    /* the wrap to 0 uses >=, so limit itself is never a legal position */
    CursorSetup(10, 0, 10, aStops, 0);
    BrCursorAdvance();
    CHECK(pSt->pos == 0);

    /* a negative step wraps to limit-1 and still terminates */
    CursorSetup(-3, 5, 10, aStops, 2);
    BrCursorAdvance();
    CHECK(pSt->pos == 0);
    CHECK(pSt->step == 0);

    /* limit <= 0 with a positive step ends on the very first wrap */
    CursorSetup(1, 0, 0, aStops, 2);
    BrCursorAdvance();
    CHECK(pSt->pos == 0);
    CHECK(pSt->step == 0);

    /* invariant: a POSITIVE step always terminates -- the wrap to 0 is the
     * backstop -- and always ends inside [0, limit) */
    {
        int32_t s;

        for (s = 1; s <= 9; ++s) {
            CursorSetup(s, 3, 64, aStops, 2);
            BrCursorAdvance();
            CHECK(pSt->pos >= 0 && pSt->pos < 64);
            CHECK(pSt->step == 0);
        }
    }

    /* A NEGATIVE step has no such backstop: it cycles through
     * limit-1, limit-1-|step|, ... and wraps back to limit-1, so it
     * terminates only if that cycle contains 0 or a stop value. These two
     * do; step -5 with limit 64 and these stops does NOT and hangs in the
     * original as well as here, so it is documented rather than exercised. */
    CursorSetup(-4, 3, 64, aStops, 2);
    BrCursorAdvance();
    CHECK(pSt->pos == 51);
    CHECK(pSt->step == 0);

    CursorSetup(-3, 3, 64, aStops, 2);
    BrCursorAdvance();
    CHECK(pSt->pos == 0);
    CHECK(pSt->step == 0);
}

/* ==========================================================================
 * 4. DirectPlay
 * ========================================================================== */

/* --- a recording OS layer ------------------------------------------------ */

#define TRACE_MAX 64
static const char *g_aTrace[TRACE_MAX];
static int         g_cTrace;

static void Trace(const char *psz)
{
    if (g_cTrace < TRACE_MAX)
        g_aTrace[g_cTrace] = psz;
    ++g_cTrace;
}

static int TraceIndexOf(const char *psz)
{
    int i;

    for (i = 0; i < g_cTrace && i < TRACE_MAX; ++i)
        if (strcmp(g_aTrace[i], psz) == 0)
            return i;
    return -1;
}

static int   g_cAlloc, g_cFree;
static int   g_cEventsMade, g_fEventFail2;
static int   g_fThreadFail;
static char  g_szDebug[256];
static void *g_pPostLParam;
static int   g_cPost;

static void *OsCreateEvent(void)
{
    Trace("CreateEvent");
    ++g_cEventsMade;
    if (g_fEventFail2 && g_cEventsMade == 2)
        return NULL;
    return (void *)(uintptr_t)(0x1000 + g_cEventsMade);
}

static void OsSetEvent(void *h)     { (void)h; Trace("SetEvent"); }
static void OsCloseHandle(void *h)  { (void)h; Trace("CloseHandle"); }
static void OsWaitSingle(void *h)   { (void)h; Trace("WaitSingle"); }
static void OsInitCrit(void)        { Trace("InitCrit"); }
static void OsDeleteCrit(void)      { Trace("DeleteCrit"); }
static void OsExitThread(uint32_t c){ (void)c; Trace("ExitThread"); }

static uint32_t g_aWaitResults[8];
static int      g_cWait;

static uint32_t OsWaitMultiple(uint32_t c, void *const *ah)
{
    (void)c; (void)ah;
    Trace("WaitMultiple");
    return g_aWaitResults[g_cWait < 8 ? g_cWait++ : 7];
}

static void *OsCreateThread(uint32_t (*pfn)(void *), void *pv, uint32_t *pid)
{
    (void)pfn; (void)pv;
    Trace("CreateThread");
    *pid = 4321;
    return g_fThreadFail ? NULL : (void *)(uintptr_t)0x2000;
}

static void *OsAlloc(uint32_t cb)
{
    ++g_cAlloc;
    Trace("Alloc");
    return calloc(1, cb ? cb : 1);
}

static void OsFree(void *pv)
{
    ++g_cFree;
    Trace("Free");
    free(pv);
}

static void OsDebugOut(const char *psz)
{
    Trace("DebugOut");
    snprintf(g_szDebug, sizeof(g_szDebug), "%s", psz);
}

static void OsPost(void *pWnd, uint32_t msg, uintptr_t wp, void *lp)
{
    (void)pWnd;
    Trace("Post");
    ++g_cPost;
    CHECK(msg == BR_DP_WM_LOGLINE);
    CHECK(wp == 0);
    g_pPostLParam = lp;
}

static void DPlayReset(void)
{
    BrDPlayState *pSt = BrDPlayGetState();

    memset(pSt, 0, sizeof(*pSt));
    pSt->os.pfnCreateEvent  = OsCreateEvent;
    pSt->os.pfnSetEvent     = OsSetEvent;
    pSt->os.pfnCloseHandle  = OsCloseHandle;
    pSt->os.pfnWaitMultiple = OsWaitMultiple;
    pSt->os.pfnWaitSingle   = OsWaitSingle;
    pSt->os.pfnCreateThread = OsCreateThread;
    pSt->os.pfnExitThread   = OsExitThread;
    pSt->os.pfnInitCrit     = OsInitCrit;
    pSt->os.pfnDeleteCrit   = OsDeleteCrit;
    pSt->os.pfnAlloc        = OsAlloc;
    pSt->os.pfnFree         = OsFree;
    pSt->os.pfnDebugOut     = OsDebugOut;
    pSt->os.pfnPost         = OsPost;

    g_cTrace = g_cAlloc = g_cFree = g_cPost = 0;
    g_cEventsMade = g_cWait = 0;
    g_fEventFail2 = g_fThreadFail = 0;
    g_pPostLParam = NULL;
    g_szDebug[0]  = '\0';
    g_cAppMsgDispatch = 0;
    g_c10071480 = g_c10005FE0 = g_c100360F0 = g_c1003CE80 = 0;
    g_c1000BAF0 = 0;
}

/* --- a scripted IDirectPlay4A -------------------------------------------- */

typedef struct RecvStep {
    int32_t  hr;
    uint32_t cb;
    uint32_t idFrom;
    uint32_t idTo;
    uint32_t dwType;    /* written into the buffer when hr >= 0 */
} RecvStep;

static RecvStep g_aRecv[8];
static int      g_cRecvSteps, g_iRecv;
static int      g_cDestroyPlayer, g_cClose, g_cRelease;
static uint32_t g_idDestroyed;

static int32_t DpReceive(BrDPlay4Obj *pThis, uint32_t *pidFrom,
                         uint32_t *pidTo, uint32_t dwFlags, void *pvData,
                         uint32_t *pcbData)
{
    const RecvStep *pS;

    (void)pThis;
    CHECK(dwFlags == 1u);
    /* the original re-zeroes both ids before every call */
    CHECK(*pidFrom == 0u);
    CHECK(*pidTo == 0u);

    if (g_iRecv >= g_cRecvSteps)
        return -1;

    pS = &g_aRecv[g_iRecv++];
    *pidFrom = pS->idFrom;
    *pidTo   = pS->idTo;
    *pcbData = pS->cb;
    if (pS->hr >= 0 && pvData != NULL && pS->cb >= 4)
        memcpy(pvData, &pS->dwType, 4);
    return pS->hr;
}

static int32_t DpDestroyPlayer(BrDPlay4Obj *pThis, uint32_t id)
{
    (void)pThis;
    ++g_cDestroyPlayer;
    g_idDestroyed = id;
    Trace("DestroyPlayer");
    return 0;
}

static int32_t DpClose(BrDPlay4Obj *pThis)   { (void)pThis; ++g_cClose;   Trace("Close");   return 0; }
static int32_t DpRelease(BrDPlay4Obj *pThis) { (void)pThis; ++g_cRelease; Trace("Release"); return 0; }

static BrDPlay4Vtbl g_DpVtbl;
static BrDPlay4Obj  g_DpObj;

static void DpVtblInit(void)
{
    memset(&g_DpVtbl, 0, sizeof(g_DpVtbl));
    g_DpVtbl.Release       = DpRelease;
    g_DpVtbl.Close         = DpClose;
    g_DpVtbl.DestroyPlayer = DpDestroyPlayer;
    g_DpVtbl.Receive       = DpReceive;
    g_DpObj.pVtbl          = &g_DpVtbl;
    g_cDestroyPlayer = g_cClose = g_cRelease = 0;
}

static void TestDPlayDispatch(void)
{
    BrDPlaySysMsg msg;

    DPlayReset();
    memset(&msg, 0, sizeof(msg));

    /* dwType 5 is gated on fLog == 0, and the shipped image has fLog == 1 */
    msg.dwType = 5;
    msg.f08    = 77;
    BrDPlayGetState()->fLog = 1;
    BrDPlaySysMsgDispatch(NULL, &msg, 0, 0, 0);
    CHECK(g_c10071480 == 0 && g_c10005FE0 == 0);

    BrDPlayGetState()->fLog = 0;
    BrDPlaySysMsgDispatch(NULL, &msg, 0, 0, 0);
    CHECK(g_c10071480 == 1 && g_c10005FE0 == 1);
    CHECK(g_id10071480 == 77 && g_id10005FE0 == 77);

    /* 3 and 0x21 are matched by the original and then do nothing */
    msg.dwType = 3;
    BrDPlaySysMsgDispatch(NULL, &msg, 0, 0, 0);
    msg.dwType = 0x21;
    BrDPlaySysMsgDispatch(NULL, &msg, 0, 0, 0);
    CHECK(g_c10071480 == 1 && g_c100360F0 == 0);

    /* 0x107 is the only live arm of the whole jump table */
    msg.dwType = 0x107;
    msg.f0C = 11; msg.f10 = 22; msg.f08 = 33;
    BrDPlaySysMsgDispatch(NULL, &msg, 0, 0, 44);
    CHECK(g_c100360F0 == 1);
    CHECK(g_a100360F0[0] == 11 && g_a100360F0[1] == 22);
    CHECK(g_a100360F0[2] == 33 && g_a100360F0[3] == 44);

    /* every other id in the table's range is the default `ret` */
    msg.dwType = 0x106;
    BrDPlaySysMsgDispatch(NULL, &msg, 0, 0, 0);
    msg.dwType = 0x31;
    BrDPlaySysMsgDispatch(NULL, &msg, 0, 0, 0);
    msg.dwType = 0x108;             /* past the table entirely */
    BrDPlaySysMsgDispatch(NULL, &msg, 0, 0, 0);
    CHECK(g_c100360F0 == 1);
}

static void TestDPlayLog(void)
{
    BrDPlaySysMsg msg;
    BrDPlayCtx    ctx;
    BrDPlayState *pSt;

    /* --- f0C picks the dispatcher --- */
    DPlayReset();
    memset(&msg, 0, sizeof(msg));
    memset(&ctx, 0, sizeof(ctx));
    BrDPlayGetState()->fLog = 0;

    ctx.f0C = 0;
    BrDPlaySysMsgLog(&ctx, &msg, 0, 0, 0);
    CHECK(g_cAppMsgDispatch == 1);

    ctx.f0C = 1;
    msg.dwType = 0x107;
    BrDPlaySysMsgLog(&ctx, &msg, 0, 0, 0);
    CHECK(g_cAppMsgDispatch == 1);
    CHECK(g_c100360F0 == 1);

    /* --- dwType 3 builds a line and posts it --- */
    DPlayReset();
    pSt = BrDPlayGetState();
    pSt->fLog = 1;
    pSt->pWnd = (void *)(uintptr_t)0x99;
    memset(&msg, 0, sizeof(msg));
    memset(&ctx, 0, sizeof(ctx));
    msg.dwType    = 3;
    msg.pszName20 = (char *)"Ana";
    BrDPlaySysMsgLog(&ctx, &msg, 0, 0, 0);
    CHECK(g_cPost == 1);
    CHECK(g_pPostLParam != NULL);
    CHECK(strcmp((char *)g_pPostLParam, "Ana joined the game.\r\n") == 0);
    /* the buffer belongs to the receiver -- nothing frees it here */
    CHECK(g_cAlloc == 1 && g_cFree == 0);
    free(g_pPostLParam);

    /* --- a NULL name becomes "unknown" --- */
    DPlayReset();
    pSt = BrDPlayGetState();
    pSt->fLog = 1;
    pSt->pWnd = (void *)(uintptr_t)0x99;
    msg.pszName20 = NULL;
    BrDPlaySysMsgLog(&ctx, &msg, 0, 0, 0);
    CHECK(strcmp((char *)g_pPostLParam, "unknown joined the game.\r\n") == 0);
    free(g_pPostLParam);

    /* --- with no window the line is freed instead of posted --- */
    DPlayReset();
    pSt = BrDPlayGetState();
    pSt->fLog = 1;
    pSt->pWnd = NULL;
    msg.pszName20 = (char *)"Bo";
    BrDPlaySysMsgLog(&ctx, &msg, 0, 0, 0);
    CHECK(g_cPost == 0);
    CHECK(g_cAlloc == 1 && g_cFree == 1);

    /* --- dwType 5 also clears the matching slot and traces it --- */
    DPlayReset();
    pSt = BrDPlayGetState();
    pSt->fLog = 1;
    pSt->pWnd = NULL;
    pSt->aSlots[3][0] = 512;
    pSt->aSlots[3][1] = 7;
    pSt->aSlots[3][2] = 9;
    memset(&msg, 0, sizeof(msg));
    msg.dwType    = 5;
    msg.f08       = 512;
    msg.pszName24 = (char *)"Cy";
    BrDPlaySysMsgLog(&ctx, &msg, 0, 0, 0);
    CHECK(pSt->aSlots[3][0] == -1);
    CHECK(pSt->aSlots[3][1] == 0);
    CHECK(pSt->aSlots[3][2] == 9);      /* only two of the three are touched */
    CHECK(strcmp(g_szDebug,
                 "Destroy Player message received, ID: 512\n") == 0);
    CHECK(g_cAlloc == 1 && g_cFree == 1);

    /* --- a slot miss still posts the line, it just does not trace --- */
    DPlayReset();
    pSt = BrDPlayGetState();
    pSt->fLog = 1;
    pSt->pWnd = NULL;
    msg.f08   = 4096;
    BrDPlaySysMsgLog(&ctx, &msg, 0, 0, 0);
    CHECK(g_szDebug[0] == '\0');
    CHECK(g_cAlloc == 1 && g_cFree == 1);

    /* --- dwType 0x104 calls out and builds no line at all --- */
    DPlayReset();
    pSt = BrDPlayGetState();
    pSt->fLog = 1;
    pSt->pWnd = (void *)(uintptr_t)0x99;
    memset(&msg, 0, sizeof(msg));
    msg.dwType = 0x104;
    BrDPlaySysMsgLog(&ctx, &msg, 0, 0, 0);
    CHECK(g_c1003CE80 == 1);
    CHECK(g_cAlloc == 0 && g_cPost == 0);

    /* --- fLog == 0 suppresses the whole logging half --- */
    DPlayReset();
    pSt = BrDPlayGetState();
    pSt->fLog = 0;
    memset(&msg, 0, sizeof(msg));
    msg.dwType    = 3;
    msg.pszName20 = (char *)"Di";
    BrDPlaySysMsgLog(&ctx, &msg, 0, 0, 0);
    CHECK(g_cAlloc == 0 && g_cPost == 0);
}

static void TestDPlayPump(void)
{
    BrDPlayCtx ctx;

    DPlayReset();
    DpVtblInit();
    memset(&ctx, 0, sizeof(ctx));
    ctx.pDP = &g_DpObj;
    ctx.f0C = 0;                 /* route system messages via 0x1000BEA0 */
    BrDPlayGetState()->fLog = 0;

    /* grow the buffer, deliver one system and one player message, then stop */
    g_aRecv[0].hr = BR_DP_E_BUFFERTOOSMALL; g_aRecv[0].cb = 16;
    g_aRecv[1].hr = 0; g_aRecv[1].cb = 8;  g_aRecv[1].idFrom = 0; g_aRecv[1].idTo = 5;
    g_aRecv[2].hr = 0; g_aRecv[2].cb = 8;  g_aRecv[2].idFrom = 9; g_aRecv[2].idTo = 5;
    g_aRecv[3].hr = -1; g_aRecv[3].cb = 0;
    g_cRecvSteps = 4;
    g_iRecv      = 0;

    CHECK(BrDPlayPump(&ctx) == 0);
    CHECK(g_iRecv == 4);
    CHECK(g_cAppMsgDispatch == 1);      /* idFrom == 0 -> the system route */
    CHECK(g_c1000BAF0 == 1);            /* idFrom != 0 -> the player route */
    CHECK(g_idFrom1000BAF0 == 9);
    CHECK(g_cAlloc == 1 && g_cFree == 1);   /* every buffer is released */

    /* a message shorter than 4 bytes is dropped but does not stop the loop */
    DPlayReset();
    g_iRecv = 0;
    g_aRecv[0].hr = 0; g_aRecv[0].cb = 3; g_aRecv[0].idFrom = 0; g_aRecv[0].idTo = 0;
    g_aRecv[1].hr = -1; g_aRecv[1].cb = 0;
    g_cRecvSteps = 2;
    CHECK(BrDPlayPump(&ctx) == 0);
    CHECK(g_iRecv == 2);
    CHECK(g_cAppMsgDispatch == 0 && g_c1000BAF0 == 0);

    /* a failed grow becomes E_OUTOFMEMORY and stops immediately */
    DPlayReset();
    BrDPlayGetState()->os.pfnAlloc = NULL;   /* not reached: see below */
    BrDPlayGetState()->os.pfnAlloc = OsAlloc;
    g_iRecv = 0;
    g_aRecv[0].hr = BR_DP_E_BUFFERTOOSMALL; g_aRecv[0].cb = 4;
    g_aRecv[1].hr = -1; g_aRecv[1].cb = 0;
    g_cRecvSteps = 2;
    CHECK(BrDPlayPump(&ctx) == 0);
    CHECK(g_cAlloc == g_cFree);              /* no leak on the error path */
}

static void TestDPlayLifecycle(void)
{
    BrDPlayCtx    ctx;
    BrDPlayState *pSt;
    int           iDelete, iSet;

    /* --- the happy path --- */
    DPlayReset();
    DpVtblInit();
    memset(&ctx, 0xEE, sizeof(ctx));
    pSt = BrDPlayGetState();

    CHECK(BrDPlayStartup(&ctx) == 0);
    CHECK(pSt->fCritInit == 1);
    CHECK(ctx.pDP == NULL);          /* startup ZEROES the interface slot */
    CHECK(ctx.idPlayer == 0);
    CHECK(ctx.f0C == 0 && ctx.f10 == 0);
    CHECK(ctx.hRecvEvent != NULL);
    CHECK(pSt->hQuit != NULL);
    CHECK(pSt->hThread != NULL);
    CHECK(pSt->idThread == 4321);

    /* a second startup does not re-create the critical section */
    {
        int cBefore = g_cTrace;

        BrDPlayStartup(&ctx);
        CHECK(TraceIndexOf("InitCrit") < cBefore);
    }

    /* --- shutdown, with the interface installed --- */
    DPlayReset();
    DpVtblInit();
    memset(&ctx, 0, sizeof(ctx));
    pSt = BrDPlayGetState();
    BrDPlayStartup(&ctx);
    ctx.pDP      = &g_DpObj;
    ctx.idPlayer = 1234;

    g_cTrace = 0;
    CHECK(BrDPlayShutdown(&ctx) == 0);
    CHECK(g_cDestroyPlayer == 1 && g_idDestroyed == 1234);
    CHECK(g_cClose == 1 && g_cRelease == 1);
    CHECK(ctx.pDP == NULL);
    CHECK(ctx.idPlayer == 0);
    CHECK(ctx.hRecvEvent == NULL);
    CHECK(pSt->hThread == NULL && pSt->hQuit == NULL);
    CHECK(pSt->fCritInit == 0);

    /* the ordering bug: the critical section is deleted BEFORE the worker is
     * told to quit and joined */
    iDelete = TraceIndexOf("DeleteCrit");
    iSet    = TraceIndexOf("SetEvent");
    CHECK(iDelete >= 0 && iSet >= 0);
    CHECK(iDelete < iSet);

    /* shutdown is idempotent and NULL-safe */
    CHECK(BrDPlayShutdown(&ctx) == 0);
    CHECK(BrDPlayShutdown(NULL) == 0);
    CHECK(g_cDestroyPlayer == 1);

    /* --- a failed second CreateEvent unwinds and reports --- */
    DPlayReset();
    memset(&ctx, 0, sizeof(ctx));
    g_fEventFail2 = 1;
    pSt = BrDPlayGetState();
    CHECK(BrDPlayStartup(&ctx) == BR_DP_E_OUTOFMEMORY);
    CHECK(ctx.hRecvEvent == NULL);      /* the first event was closed again */
    CHECK(pSt->hThread == NULL);
    CHECK(TraceIndexOf("CreateThread") < 0);

    /* --- a failed CreateThread does the same --- */
    DPlayReset();
    memset(&ctx, 0, sizeof(ctx));
    g_fThreadFail = 1;
    pSt = BrDPlayGetState();
    CHECK(BrDPlayStartup(&ctx) == BR_DP_E_OUTOFMEMORY);
    CHECK(pSt->hQuit == NULL);
    CHECK(ctx.hRecvEvent == NULL);
}

static void TestDPlayThreadProc(void)
{
    BrDPlayCtx ctx;

    /* index 0 twice, then something else -> two pumps, then ExitThread */
    DPlayReset();
    DpVtblInit();
    memset(&ctx, 0, sizeof(ctx));
    ctx.pDP = &g_DpObj;
    g_aWaitResults[0] = 0;
    g_aWaitResults[1] = 0;
    g_aWaitResults[2] = 1;
    g_iRecv = 0;
    g_cRecvSteps = 0;                   /* every Receive fails -> pump exits */

    CHECK(BrDPlayThreadProc(&ctx) == 0);
    CHECK(g_cWait == 3);
    CHECK(TraceIndexOf("ExitThread") >= 0);

    /* a first wait that is not index 0 pumps nothing at all */
    DPlayReset();
    g_aWaitResults[0] = 1;
    g_iRecv = 0;
    CHECK(BrDPlayThreadProc(&ctx) == 0);
    CHECK(g_cWait == 1);
    CHECK(g_iRecv == 0);
}

static void TestDPlayPlayerCount(void)
{
    DPlayReset();

    g_f1003D0B0Fail = 0;
    CHECK(BrDPlayGetCurrentPlayers() == 42);
    CHECK(g_cFree == 1);                /* the record is always released */

    DPlayReset();
    g_f1003D0B0Fail = 1;
    CHECK(BrDPlayGetCurrentPlayers() == 0xFFFFu);
    CHECK(g_cFree == 0);                /* nothing was allocated to release */
    g_f1003D0B0Fail = 0;
}

/* ==========================================================================
 * 5. The clip pool
 * ========================================================================== */

static void TestPolyPool(void)
{
    BrLerpNode *aPool = BrPolyPoolBase();
    BrLerpNode *p;
    BrLerpNode  outside;
    int         i;

    BrPolyPoolInit();
    CHECK(BrPolyPoolCount() == BR_POLY_POOL_NODES);
    CHECK(g_pBrLerpFree == &aPool[0]);      /* head is the LOWEST node */

    for (i = 0; i < BR_POLY_POOL_NODES - 1; ++i)
        CHECK(aPool[i].pNext == &aPool[i + 1]);
    CHECK(aPool[BR_POLY_POOL_NODES - 1].pNext == NULL);

    /* alloc / free round-trip */
    p = BrPolyPoolAlloc();
    CHECK(p == &aPool[0]);
    CHECK(BrPolyPoolCount() == BR_POLY_POOL_NODES - 1);
    BrPolyPoolFree(p);
    CHECK(BrPolyPoolCount() == BR_POLY_POOL_NODES);

    /* exhaustion returns NULL rather than faulting (DEVIATION) */
    for (i = 0; i < BR_POLY_POOL_NODES; ++i)
        CHECK(BrPolyPoolAlloc() != NULL);
    CHECK(BrPolyPoolAlloc() == NULL);

    /* a node from outside the pool is DROPPED, not freed */
    BrPolyPoolInit();
    memset(&outside, 0, sizeof(outside));
    BrPolyPoolFree(&outside);
    CHECK(BrPolyPoolCount() == BR_POLY_POOL_NODES);
    CHECK(outside.pNext == NULL);           /* it was not even relinked */
}

/* ==========================================================================
 * 6. BrPolyClipPlane
 * ========================================================================== */

/* Build a circular polygon out of pool nodes from an array of (x, y). */
static void PolyBuild(BrPolyList *pList, const float *aXY, int c)
{
    BrLerpNode *pFirst = NULL;
    BrLerpNode *pPrev  = NULL;
    int         i;

    for (i = 0; i < c; ++i) {
        BrLerpNode *p = BrPolyPoolAlloc();
        BrScrPt    *pt;

        p->pData = &p->data[0];
        pt = (BrScrPt *)(void *)p->pData;
        memset(pt, 0, sizeof(*pt));
        pt->f0C = aXY[i * 2 + 0];
        pt->f10 = aXY[i * 2 + 1];

        if (pFirst == NULL)
            pFirst = p;
        else
            pPrev->pNext = p;
        pPrev = p;
    }
    pPrev->pNext = pFirst;

    pList->pHead  = pFirst;
    pList->cVerts = c;
}

static int PolyRingLen(const BrPolyList *pList)
{
    const BrLerpNode *p = pList->pHead;
    int               n = 0;

    do {
        ++n;
        p = p->pNext;
    } while (p != pList->pHead && n < 256);
    return n;
}

static int PolyHasX(const BrPolyList *pList, float x)
{
    const BrLerpNode *p = pList->pHead;
    int               n = 0;

    do {
        const BrScrPt *pt = (const BrScrPt *)(const void *)p->pData;

        if (fabsf(pt->f0C - x) < 1e-4f)
            return 1;
        p = p->pNext;
        ++n;
    } while (p != pList->pHead && n < 256);
    return 0;
}

static void TestClipPlane(void)
{
    static const float aSquare[] = {
        100.0f, 100.0f,  900.0f, 100.0f,  900.0f, 900.0f,  100.0f, 900.0f
    };
    static const float aCross[] = {
        -100.0f, 100.0f,  900.0f, 100.0f,  900.0f, 900.0f
    };
    BrPolyList  list;
    BrLerpNode *pOldHead;

    /* --- a polygon entirely inside is unchanged, except the head rotates */
    BrPolyPoolInit();
    PolyBuild(&list, aSquare, 4);
    pOldHead = list.pHead;
    BrPolyClipPlane(&list, BrPolyDistX);
    CHECK(list.cVerts == 4);
    CHECK(PolyRingLen(&list) == 4);
    CHECK(list.pHead == pOldHead->pNext);       /* rotated by exactly one */
    CHECK(BrPolyPoolCount() == BR_POLY_POOL_NODES - 4);

    /* the same plane applied again is still the identity */
    BrPolyClipPlane(&list, BrPolyDistX);
    CHECK(list.cVerts == 4);
    CHECK(PolyRingLen(&list) == 4);

    /* --- a polygon entirely OUTSIDE collapses and is fully recycled --- */
    BrPolyPoolInit();
    PolyBuild(&list, aSquare, 4);
    BrPolyClipPlane(&list, BrPolyDistMaxX);     /* 1024 - x, all inside */
    CHECK(list.cVerts == 4);
    {
        /* shift the whole square past 1024 by clipping with a plane that is
         * negative everywhere: reuse MaxX after moving the points */
        BrLerpNode *p = list.pHead;
        int         i;

        for (i = 0; i < 4; ++i) {
            BrScrPt *pt = (BrScrPt *)(void *)p->pData;

            pt->f0C += 2000.0f;
            p = p->pNext;
        }
        BrPolyClipPlane(&list, BrPolyDistMaxX);
        CHECK(list.cVerts < 2);
    }

    /* --- one vertex outside: the count grows by one and the two crossings
     *     land exactly on the plane --- */
    BrPolyPoolInit();
    PolyBuild(&list, aCross, 3);
    BrPolyClipPlane(&list, BrPolyDistX);        /* x >= 0 */
    CHECK(list.cVerts == 4);
    CHECK(PolyRingLen(&list) == 4);
    CHECK(PolyHasX(&list, 0.0f));
    {
        /* nothing survives with a negative x */
        const BrLerpNode *p = list.pHead;
        int               i;
        int               fNeg = 0;

        for (i = 0; i < 4; ++i) {
            const BrScrPt *pt = (const BrScrPt *)(const void *)p->pData;

            if (pt->f0C < -1e-4f)
                fNeg = 1;
            p = p->pNext;
        }
        CHECK(!fNeg);
    }
    /* one node was dropped and one added, so the pool moved by exactly one */
    CHECK(BrPolyPoolCount() == BR_POLY_POOL_NODES - 4);

    /* --- an empty polygon still rotates the head and touches nothing --- */
    BrPolyPoolInit();
    PolyBuild(&list, aSquare, 4);
    pOldHead      = list.pHead;
    list.cVerts   = 0;
    BrPolyClipPlane(&list, BrPolyDistX);
    CHECK(list.cVerts == 0);
    CHECK(list.pHead == pOldHead->pNext);
    CHECK(BrPolyPoolCount() == BR_POLY_POOL_NODES - 4);

    /* --- NaN counts as OUTSIDE --- */
    BrPolyPoolInit();
    PolyBuild(&list, aSquare, 4);
    {
        BrLerpNode *p = list.pHead;
        int         i;

        for (i = 0; i < 4; ++i) {
            BrScrPt *pt = (BrScrPt *)(void *)p->pData;

            pt->f0C = (float)NAN;
            p = p->pNext;
        }
        BrPolyClipPlane(&list, BrPolyDistX);
        CHECK(list.cVerts < 2);
    }
}

/* ==========================================================================
 * 7. BrPolyClipTri
 * ========================================================================== */

static void ScrPtSet(BrScrPt *pt, float x, float y, float z)
{
    memset(pt, 0, sizeof(*pt));
    pt->f00 = x;
    pt->f04 = y;
    pt->f08 = z;
    pt->pad[0] = 1.0f;
    pt->pad[1] = 2.0f;
    pt->pad[2] = 3.0f;
}

static void TestClipTri(void)
{
    BrScrPt v0, v1, v2;
    BrScrPt aOut[4];
    int     aFlags[4];
    BrMat4  m;

    memset(&m, 0, sizeof(m));
    memset(aOut, 0, sizeof(aOut));
    memset(aFlags, 0, sizeof(aFlags));

    /* --- a triangle fully inside the 1024 square survives whole --- */
    BrPolyPoolInit();
    g_cKeep = 0;
    ScrPtSet(&v0, 100.0f, 100.0f, 1.0f);
    ScrPtSet(&v1, 900.0f, 100.0f, 2.0f);
    ScrPtSet(&v2, 500.0f, 900.0f, 3.0f);
    BrPolyClipTri(&m, aOut, aFlags, &v0, &v1, &v2, NULL);

    CHECK(g_cKeep == 12);                       /* 3 vertices x 4 corners */
    CHECK(BrPolyPoolCount() == BR_POLY_POOL_NODES);   /* pool fully restored */

    /* the corner order really is (0,0) (1024,0) (0,1024) (1024,1024) */
    CHECK(g_aKeepIdx[0] == 0 && g_aKeepCx[0] == 0.0f    && g_aKeepCy[0] == 0.0f);
    CHECK(g_aKeepIdx[1] == 1 && g_aKeepCx[1] == 1024.0f && g_aKeepCy[1] == 0.0f);
    CHECK(g_aKeepIdx[2] == 2 && g_aKeepCx[2] == 0.0f    && g_aKeepCy[2] == 1024.0f);
    CHECK(g_aKeepIdx[3] == 3 && g_aKeepCx[3] == 1024.0f && g_aKeepCy[3] == 1024.0f);
    CHECK(g_aKeepIdx[4] == 0);                  /* the pattern repeats */

    /* --- a triangle entirely off-screen emits nothing and leaks nothing --- */
    BrPolyPoolInit();
    g_cKeep = 0;
    ScrPtSet(&v0, -500.0f, -500.0f, 1.0f);
    ScrPtSet(&v1, -400.0f, -500.0f, 2.0f);
    ScrPtSet(&v2, -450.0f, -400.0f, 3.0f);
    BrPolyClipTri(&m, aOut, aFlags, &v0, &v1, &v2, NULL);
    CHECK(g_cKeep == 0);
    CHECK(BrPolyPoolCount() == BR_POLY_POOL_NODES);

    /* --- a triangle straddling one edge gains a vertex --- */
    BrPolyPoolInit();
    g_cKeep = 0;
    ScrPtSet(&v0, -200.0f, 100.0f, 1.0f);
    ScrPtSet(&v1,  900.0f, 100.0f, 2.0f);
    ScrPtSet(&v2,  900.0f, 900.0f, 3.0f);
    BrPolyClipTri(&m, aOut, aFlags, &v0, &v1, &v2, NULL);
    CHECK(g_cKeep == 16);                       /* 4 vertices x 4 corners */
    CHECK(BrPolyPoolCount() == BR_POLY_POOL_NODES);

    /* --- allocation order: the LAST vertex argument gets the first node --- */
    BrPolyPoolInit();
    g_cKeep = 0;
    {
        BrLerpNode *aPool = BrPolyPoolBase();

        ScrPtSet(&v0, 100.0f, 100.0f, 1.0f);
        ScrPtSet(&v1, 200.0f, 100.0f, 2.0f);
        ScrPtSet(&v2, 300.0f, 900.0f, 3.0f);
        BrPolyClipTri(&m, aOut, aFlags, &v0, &v1, &v2, NULL);
        /* aPool[0] was handed to pV2, so its f08 is v2's */
        CHECK(aPool[0].data[2] == 3.0f);
        CHECK(aPool[1].data[2] == 2.0f);
        CHECK(aPool[2].data[2] == 1.0f);
        /* and the trailing three words really are copied, not padding */
        CHECK(aPool[0].data[5] == 1.0f);
        CHECK(aPool[0].data[6] == 2.0f);
        CHECK(aPool[0].data[7] == 3.0f);
    }
}

/* ==========================================================================
 * 8. The two pointer-table setters
 * ========================================================================== */

static void TestGfx(void)
{
    static unsigned char ab0[16], ab1[16], ab2[16];
    BrGfxBanks    *pB = BrGfxGetBanks();
    BrGfxCounters *pC = BrGfxGetCounters();
    int            i;

    pB->pBase0 = ab0;
    pB->pBase1 = ab1;
    pB->pBase2 = ab2;

    pB->iBank = 0;
    BrGfxSetBankPointers();
    CHECK(pB->p363FF0 == ab0 && pB->p2E5EC8 == ab0);
    CHECK(pB->p364304 == ab1 && pB->p3643BC == ab1);
    CHECK(pB->p2E5EC4 == ab2 && pB->p363FF4 == ab2);

    pB->iBank = 3;
    BrGfxSetBankPointers();
    CHECK((char *)pB->p363FF0 - (char *)ab0 == 3 * 80000);
    CHECK((char *)pB->p364304 - (char *)ab1 == 3 * 32000);
    CHECK((char *)pB->p2E5EC4 - (char *)ab2 == 3 * 256000);
    /* the pairs are genuine aliases */
    CHECK(pB->p363FF0 == pB->p2E5EC8);
    CHECK(pB->p364304 == pB->p3643BC);
    CHECK(pB->p2E5EC4 == pB->p363FF4);

    for (i = 0; i < BR_GFX_COUNTERS; ++i) {
        pC->a364308[i] = 0xA5A5A5A5u;
        pC->a363F68[i] = 0x5A5A5A5Au;
    }
    BrGfxClearCounters();
    for (i = 0; i < BR_GFX_COUNTERS; ++i) {
        CHECK(pC->a364308[i] == 0);
        CHECK(pC->a363F68[i] == 0);
    }
}

/* ========================================================================== */

int main(void)
{
    TestPathBaseName();
    TestFileHelpers();
    TestCursor();
    TestDPlayDispatch();
    TestDPlayLog();
    TestDPlayPump();
    TestDPlayLifecycle();
    TestDPlayThreadProc();
    TestDPlayPlayerCount();
    TestPolyPool();
    TestClipPlane();
    TestClipTri();
    TestGfx();

    printf("slice2_13: %d checks, %d failures\n", g_cRun, g_cFail);
    return g_cFail != 0;
}
