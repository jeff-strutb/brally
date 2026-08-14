/* test_slice6_70.c -- behaviour and invariants for slice6_70.c.
 *
 * The assertions here are properties of the code, not transcriptions of it:
 * the alias pairs must be indistinguishable, the mode gates must be the ONLY
 * thing that moves the guarded stores, the mask writes must leave everything
 * outside the mask alone, the counters must be monotone in the exact place the
 * original bumps them, and the ignored argument must really be ignored.
 *
 * Every cross-slice callee slice6_70.c reaches lives here as a stand-in, and
 * NOWHERE else -- the same arrangement slice2_26.h describes for its packet.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "slice6_70.h"

/* ==========================================================================
 * Harness
 * ========================================================================== */

static int g_cFail = 0;
static int g_cRun  = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        ++g_cRun;                                                          \
        if (!(cond)) {                                                     \
            ++g_cFail;                                                     \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);         \
        }                                                                  \
    } while (0)

#define CHECK_EQ(a, b)                                                     \
    do {                                                                   \
        long va_ = (long)(a), vb_ = (long)(b);                             \
        ++g_cRun;                                                          \
        if (va_ != vb_) {                                                  \
            ++g_cFail;                                                     \
            printf("FAIL %s:%d  %s (%ld) != %s (%ld)\n",                   \
                   __FILE__, __LINE__, #a, va_, #b, vb_);                  \
        }                                                                  \
    } while (0)

/* --- an ordered log of everything the module reaches out to -------------- */

#define LOG_MAX 128
#define LOG_LEN 160

static char g_aLog[LOG_MAX][LOG_LEN];
static int  g_cLog = 0;

static void LogAdd(const char *pszFmt, ...)
{
    va_list ap;
    if (g_cLog >= LOG_MAX) {
        return;
    }
    va_start(ap, pszFmt);
    vsnprintf(g_aLog[g_cLog], LOG_LEN, pszFmt, ap);
    va_end(ap);
    ++g_cLog;
}

static void LogReset(void) { g_cLog = 0; }

static int LogFind(const char *psz)
{
    int i;
    for (i = 0; i < g_cLog; ++i) {
        if (strcmp(g_aLog[i], psz) == 0) {
            return i;
        }
    }
    return -1;
}

static int LogCount(const char *psz)
{
    int i, n = 0;
    for (i = 0; i < g_cLog; ++i) {
        if (strcmp(g_aLog[i], psz) == 0) {
            ++n;
        }
    }
    return n;
}

/* Snapshot of the whole log, for the "the two names are one function" tests. */
static char g_aSnap[LOG_MAX][LOG_LEN];
static int  g_cSnap = 0;

static void LogSnapshot(void)
{
    memcpy(g_aSnap, g_aLog, sizeof g_aLog);
    g_cSnap = g_cLog;
}

static int LogEqualsSnapshot(void)
{
    int i;
    if (g_cSnap != g_cLog) {
        return 0;
    }
    for (i = 0; i < g_cLog; ++i) {
        if (strcmp(g_aSnap[i], g_aLog[i]) != 0) {
            return 0;
        }
    }
    return 1;
}

/* ==========================================================================
 * Stand-ins for the objects slice6_70.h keeps opaque
 * ========================================================================== */

/* CONFLICT 4 made concrete: 0x10AA29D8's object really is an entity record. */
struct BrOptFlagObj { unsigned char raw[0x2B68]; };
struct BrDPlay      { int nDummy; };
struct BrObj29D4    { int nDummy; };
struct BrObjA9D008  { void *f00; void *f04; void *f08; };

static struct BrOptFlagObj g_flagObj;
static struct BrDPlay      g_dplay;
static struct BrObj29D4    g_obj29D4;

/* ==========================================================================
 * Stand-ins for the globals other slices own
 * ========================================================================== */

void   *g_brP680584 = (void *)0x1234;
uint32_t g_brA9BFDC = 0;
uint32_t (*g_pfnBrPlatSetTimer)(void *hWnd, uint32_t idEvent,
                                uint32_t uElapseMs, void *pfnProc) = NULL;

struct BrDPlay      *g_brP277B40  = NULL;
struct BrOptFlagObj *g_brPAA29D8  = NULL;
struct BrObj29D4    *g_brPAA29D4  = NULL;

int32_t g_br0AA010 = 0;
int32_t g_br0AC648 = 0;
int32_t g_br0AC64C = 0;
int32_t g_br0AC650 = 0;
int32_t g_br0AC654 = 0;
int32_t g_br0AC658 = 0;
int32_t g_br0BD3E0 = 0;
int32_t g_br22AF18 = 0;
int32_t g_brA9CFFC = 0;
int32_t g_brAA287C = 0;
int32_t g_brAA2884 = 0;
int32_t g_brAA2888 = 0;
int32_t g_brAA289C = 0;
int32_t g_brAA2A00 = 0;
int32_t g_brAA2A08 = 0;
int32_t g_aBrAA26F0[BR70_AA26F0_COUNT];

int32_t g_brAA28C8 = 0;

int32_t  g_brAA2A10 = 0;
int32_t  g_brAA2A14 = 0;
int32_t  g_brAA28A0 = 0;
int32_t  g_brAA28A4 = 0;
int32_t  g_brAA28AC = 0;
int8_t   g_brAA28B8 = 0;
int32_t  g_brAA28C4 = 0;
uint32_t g_brAA27E0 = 0;
char     g_aBrAA2518[BR63_TEXT_MAX];
char     g_aBrA9D618[BR63_TEXT_MAX];
const char *g_pszBr0A73C4 = "%d";

/* ==========================================================================
 * Stand-ins for the callees
 * ========================================================================== */

int BrSprintf(char *pszDest, const char *pszFmt, ...)
{
    va_list ap;
    int     n;
    va_start(ap, pszFmt);
    n = vsprintf(pszDest, pszFmt, ap);
    va_end(ap);
    LogAdd("sprintf:%s", pszDest);
    return n;
}

/* A tiny string table: ids 0xB3..0xB6 and 0xE5/0xE6 are the only ones used. */
const char *BrStrGet(int id)
{
    switch (id) {
    case 0xB3: return "st";
    case 0xB4: return "nd";
    case 0xB5: return "rd";
    case 0xB6: return "th";
    case 0xE5: return "LAP ";
    case 0xE6: return "FINISHED";
    default:   return NULL;
    }
}

void BrSub100586A0(void) { LogAdd("slotsReset"); }
void BrSub1003E510(void) { LogAdd("1003E510"); }
void BrSub1003C150(void) { LogAdd("1003C150"); }
void BrSub1003C550(void) { LogAdd("1003C550"); }
void BrSub10072270(void) { LogAdd("10072270"); }
void BrSub1003E1D0(void) { LogAdd("1003E1D0"); }
void BrSub_10019240(void) { LogAdd("textOpen"); }
void BrSub_10019250(void) { LogAdd("textClose"); }
void BrSub_10019260(void) { LogAdd("19260"); }
void BrSub_10019280(void) { LogAdd("alignLeft"); }
void BrSub_100192F0(int size) { LogAdd("scale:%d", size); }

int BrSub_100193C0(const char *psz, int scale)
{
    LogAdd("measure:%s:%d", psz, scale);
    return (int)strlen(psz) * scale;    /* deterministic, not a real metric */
}

void BrTextDraw(const char *psz, int x, int y)
{
    LogAdd("draw:%s:%d:%d", (psz != NULL) ? psz : "(null)", x, y);
}

void BrTextSetColors(int a1, int a2, int a3, int a4, int a5, int a6)
{
    LogAdd("colors:%d:%d:%d:%d:%d:%d", a1, a2, a3, a4, a5, a6);
}

/* --- the DirectPlay side, scriptable --------------------------------------*/

static void    *g_pConnOut;        /* what BrSub1003D480 hands back */
static int32_t  g_hr1003D480;
static int32_t  g_hr1003C520;
static int32_t  g_hr1003CC70;
static struct BrDPlay *g_pDPlayAfterCreate;

int32_t BrSub1003D480(void **ppConn, void **ppOut2)
{
    LogAdd("1003D480");
    *ppConn = g_pConnOut;
    *ppOut2 = NULL;
    return g_hr1003D480;
}

int32_t BrSub1003C520(struct BrDPlay **ppDPlay)
{
    LogAdd("1003C520");
    *ppDPlay = g_pDPlayAfterCreate;
    return g_hr1003C520;
}

int32_t BrSub1003CC70(struct BrDPlay *pDPlay)
{
    LogAdd("1003CC70:%d", pDPlay != NULL);
    return g_hr1003CC70;
}

static const struct BrDPlayLink *g_pLinkSent;
static uint32_t                  g_valueSent;

int BrDPlaySendTag7(const struct BrDPlayLink *pLink, uint32_t value)
{
    g_pLinkSent = pLink;
    g_valueSent = value;
    LogAdd("sendTag7:%lu", (unsigned long)value);
    return 0;
}

/* --- the slice2_15 accessors --------------------------------------------- */

static BrScreenInfo g_screen;
static BrHudEnv     g_hud;
static BrRace       g_race;

BrScreenInfo *BrScreenGet(void) { return &g_screen; }
BrHudEnv     *BrHudGetEnv(void) { return &g_hud; }

/* --- the platform hooks --------------------------------------------------- */

static uint32_t g_idTimer = 0x5A5A;
static void    *g_hEvent  = NULL;

static int32_t StubKillTimer(void *hWnd, uint32_t idEvent)
{
    LogAdd("killTimer:%d:%lu", hWnd != NULL, (unsigned long)idEvent);
    return 1;
}

static uint32_t StubSetTimer(void *hWnd, uint32_t idEvent, uint32_t ms,
                             void *pfnProc)
{
    LogAdd("setTimer:%d:%lu:%lu:%d", hWnd != NULL, (unsigned long)idEvent,
           (unsigned long)ms, pfnProc != NULL);
    return g_idTimer;
}

static void *StubCreateEvent(void)
{
    LogAdd("createEvent");
    return g_hEvent;
}

static int32_t g_hrInitConn;

static int32_t StubInitConn(struct BrDPlay *pThis, void *pConnection,
                            uint32_t dwFlags)
{
    LogAdd("initConn:%d:%d:%lu", pThis != NULL, pConnection != NULL,
           (unsigned long)dwFlags);
    return g_hrInitConn;
}

/* ==========================================================================
 * Shared reset
 * ========================================================================== */

static char g_szConn[8];

static void ResetAll(void)
{
    LogReset();

    g_pfnBrPlatKillTimer   = StubKillTimer;
    g_pfnBrPlatSetTimer    = StubSetTimer;
    g_pfnBrPlatCreateEvent = StubCreateEvent;
    g_pfnBrDPlayInitConn   = StubInitConn;

    g_pConnOut          = g_szConn;
    g_hr1003D480        = 0;
    g_hr1003C520        = 0;
    g_hr1003CC70        = 0;
    g_hrInitConn        = 0;
    g_pDPlayAfterCreate = &g_dplay;
    g_hEvent            = (void *)0xE0E0;

    g_br277B44 = NULL;
    g_brA9D004 = 0;
    g_brA9BFDC = 0;
    g_brA9CFFC = 0;
    g_brAA287C = 0;
    g_brAA2884 = 0;
    g_brAA2888 = 0;
    g_br22AF18 = 0;
    g_brP277B40 = NULL;
    g_brPAA29D4 = NULL;
    g_brPAA29D8 = NULL;

    memset(&g_flagObj, 0, sizeof g_flagObj);
}

/* ==========================================================================
 * 0x1003DB00
 * ========================================================================== */

static void Test1003DB00(void)
{
    struct BrObjA9D008 obj;

    ResetAll();
    obj.f00 = &g_dplay;
    obj.f04 = NULL;
    obj.f08 = (void *)(uintptr_t)0x11u;

    g_pLinkSent = NULL;
    g_valueSent = 0;
    BrExt_1003DB00(&obj, (void *)(uintptr_t)0xABCDu);

    /* The object is handed through UNCHANGED: this address is slice2_22's
     * body under a second name, and the adapter must not re-gate it (tags 6,
     * 7 and 8 deliberately ignore the 0x10AA288C gate the others honour). */
    CHECK((const void *)g_pLinkSent == (const void *)&obj);
    CHECK_EQ(g_valueSent, 0xABCDu);
    CHECK_EQ(LogCount("sendTag7:43981"), 1);

    /* The payload word is the low 32 bits of the argument -- the documented
     * narrowing. On a 32-bit host this is the identity. */
    LogReset();
    BrExt_1003DB00(&obj, (void *)(uintptr_t)0xFFFFFFFFu);
    CHECK_EQ(g_valueSent, 0xFFFFFFFFu);
}

/* ==========================================================================
 * 0x1003C150
 * ========================================================================== */

static void Test1003C150(void)
{
    ResetAll();
    BrExt_1003C150();

    /* Exactly one delegation and no re-implementation: nothing else in the
     * module's reach may be touched. */
    CHECK_EQ(g_cLog, 1);
    CHECK_EQ(LogCount("1003C150"), 1);
}

/* ==========================================================================
 * 0x1003C020
 * ========================================================================== */

static void Test1003C020Counter(void)
{
    /* The counter is bumped AFTER 0x1003C520 and BEFORE its result is tested,
     * so a failed create still counts -- but a NULL connection short-circuits
     * before the call and must NOT count. */
    ResetAll();
    g_pConnOut = NULL;
    BrSub1003C020();
    CHECK_EQ(g_brA9D004, 0);
    CHECK_EQ(LogCount("1003C520"), 0);

    ResetAll();
    g_hr1003C520 = -1;
    BrSub1003C020();
    CHECK_EQ(g_brA9D004, 1);

    /* Monotone across calls. */
    g_hr1003C520 = -1;
    BrSub1003C020();
    CHECK_EQ(g_brA9D004, 2);
}

static void Test1003C020Quiet(void)
{
    /* 0x88770118 is the one HRESULT that is NOT formatted. Everything else on
     * the same path is. */
    ResetAll();
    g_pConnOut   = NULL;
    g_hr1003D480 = (int32_t)0x88770118u;
    BrSub1003C020();
    CHECK_EQ(LogCount("1003D480"), 1);
    CHECK(LogFind("sprintf:Could not select service provider because of "
                  "error 0x88770118") < 0);

    ResetAll();
    g_pConnOut   = NULL;
    g_hr1003D480 = (int32_t)0x88770119u;
    BrSub1003C020();
    CHECK(LogFind("sprintf:Could not select service provider because of "
                  "error 0x88770119") >= 0);
}

static void Test1003C020ModeGate(void)
{
    int32_t mode;

    /* Modes 2 and 3 skip the timer restart entirely; 0 and 1 perform it. The
     * mode is the ONLY thing that moves those two stores. */
    for (mode = 0; mode <= 3; ++mode) {
        int fRestart = (mode != 2 && mode != 3);

        ResetAll();
        g_brAA287C = mode;
        BrSub1003C020();

        CHECK_EQ(LogCount("setTimer:1:1:1000:0"), fRestart ? 1 : 0);
        CHECK_EQ(g_brA9CFFC, fRestart ? 1 : 0);
        CHECK_EQ(g_brA9BFDC, fRestart ? g_idTimer : 0u);

        /* The event is created on every path that gets this far. */
        CHECK_EQ(LogCount("createEvent"), 1);
        CHECK(g_br277B44 != NULL);
    }
}

static void Test1003C020CC70Gate(void)
{
    /* 0x1003CC70 runs only when 0x10AA29D4 is set, and a negative result from
     * it aborts BEFORE the timer restart. */
    ResetAll();
    BrSub1003C020();
    CHECK_EQ(LogCount("1003CC70:1"), 0);

    ResetAll();
    g_brPAA29D4 = &g_obj29D4;
    BrSub1003C020();
    CHECK_EQ(LogCount("1003CC70:1"), 1);
    CHECK_EQ(LogCount("setTimer:1:1:1000:0"), 1);

    ResetAll();
    g_brPAA29D4  = &g_obj29D4;
    g_hr1003CC70 = -5;
    BrSub1003C020();
    CHECK_EQ(LogCount("1003CC70:1"), 1);
    CHECK_EQ(LogCount("setTimer:1:1:1000:0"), 0);
    CHECK_EQ(g_brA9CFFC, 0);
    CHECK(LogFind("sprintf:Could not select service provider because of "
                  "error 0xFFFFFFFB") >= 0);
}

static void Test1003C020EventOnce(void)
{
    /* Idempotence: once the handle exists it is never recreated, and a
     * CreateEventA that fails reports E_OUTOFMEMORY. */
    ResetAll();
    BrSub1003C020();
    CHECK_EQ(LogCount("createEvent"), 1);

    LogReset();
    BrSub1003C020();
    CHECK_EQ(LogCount("createEvent"), 0);

    ResetAll();
    g_hEvent = NULL;
    BrSub1003C020();
    CHECK_EQ(LogCount("createEvent"), 1);
    CHECK(g_br277B44 == NULL);
    CHECK(LogFind("sprintf:Could not select service provider because of "
                  "error 0x8007000E") >= 0);
}

static void Test1003C020NullInterface(void)
{
    /* The documented oddity: a NULL interface after a SUCCESSFUL create makes
     * the original format a success code into the error string. */
    ResetAll();
    g_pDPlayAfterCreate = NULL;
    g_hr1003C520        = 0x10;
    BrSub1003C020();
    CHECK_EQ(LogCount("initConn:1:1:0"), 0);
    CHECK(LogFind("sprintf:Could not select service provider because of "
                  "error 0x00000010") >= 0);
}

static void Test1003C020Alias(void)
{
    /* BrExt_1003C020 and BrSub1003C020 are one address: indistinguishable. */
    ResetAll();
    g_brPAA29D4 = &g_obj29D4;
    BrSub1003C020();
    LogSnapshot();

    ResetAll();
    g_brPAA29D4 = &g_obj29D4;
    BrExt_1003C020();
    CHECK(LogEqualsSnapshot());
}

/* ==========================================================================
 * 0x1003BF60
 * ========================================================================== */

static int32_t FlagWord(void)
{
    int32_t v;
    memcpy(&v, g_flagObj.raw + 0x1C, sizeof v);
    return v;
}

static void SetFlagWord(int32_t v)
{
    memcpy(g_flagObj.raw + 0x1C, &v, sizeof v);
}

static void Test1003BF60(void)
{
    int32_t mode;

    for (mode = 0; mode <= 3; ++mode) {
        int fTouch = (mode != 2 && mode != 3);

        ResetAll();
        g_brAA287C  = mode;
        g_brPAA29D8 = &g_flagObj;
        g_brA9CFFC  = 7;
        g_brAA2884  = 9;
        g_br22AF18  = 11;
        g_brAA2888  = 13;
        g_flagObj.raw[0x2B64] = 0xFF;
        SetFlagWord((int32_t)0xDEADBEEF);

        BrExt_1003BF60();

        /* The four clears are unconditional. */
        CHECK_EQ(g_brA9CFFC, 0);
        CHECK_EQ(g_brAA2884, 0);
        CHECK_EQ(g_br22AF18, 0);
        CHECK_EQ(g_brAA2888, 0);

        /* The object writes are the only mode-dependent part. */
        CHECK_EQ(g_flagObj.raw[0x2B64], fTouch ? 0x00 : 0xFF);
        /* Mask property: exactly bit 0x10 goes, nothing else moves. */
        CHECK_EQ(FlagWord(),
                 fTouch ? (int32_t)(0xDEADBEEFu & ~0x10u)
                        : (int32_t)0xDEADBEEFu);

        /* The slot table reset and the teardown always happen, in order. */
        CHECK(LogFind("slotsReset") == 0);
        CHECK(LogFind("1003C550") > LogFind("killTimer:1:0"));
    }
}

static void Test1003BF60Gate(void)
{
    /* 0x10072270 is gated on 0x10AA2884, which this same function then
     * clears -- so the gate reads the PRE-call value. */
    ResetAll();
    g_brAA2884 = 0;
    BrExt_1003BF60();
    CHECK_EQ(LogCount("10072270"), 0);

    ResetAll();
    g_brAA2884 = 1;
    BrExt_1003BF60();
    CHECK_EQ(LogCount("10072270"), 1);
    CHECK_EQ(g_brAA2884, 0);

    /* A NULL object must not be dereferenced even in the touching modes. */
    ResetAll();
    g_brAA287C  = 0;
    g_brPAA29D8 = NULL;
    BrExt_1003BF60();
    CHECK_EQ(g_brA9CFFC, 0);

    /* Alias equality. */
    ResetAll();
    g_brAA2884 = 1;
    BrExt_1003BF60();
    LogSnapshot();
    ResetAll();
    g_brAA2884 = 1;
    BrSub1003BF60();
    CHECK(LogEqualsSnapshot());
}

/* ==========================================================================
 * 0x1003E680
 * ========================================================================== */

static int AllZero(const int32_t *a, size_t c)
{
    size_t i;
    for (i = 0; i < c; ++i) {
        if (a[i] != 0) {
            return 0;
        }
    }
    return 1;
}

static void Test1003E680(void)
{
    size_t i;

    ResetAll();
    for (i = 0; i < BR70_AA26F0_COUNT; ++i) {
        g_aBrAA26F0[i] = (int32_t)i + 1;
        g_aBrA9DBD8[i] = (int32_t)i + 1;
    }
    for (i = 0; i < BR70_220B20_COUNT; ++i) {
        g_a220B20[i] = 0xAAAAAAAAu;
    }
    g_brAA27E0 = 0xF00DBEEFu;
    g_brAA28A4 = 41;
    g_brAA289C = 5;

    BrExt_1003E680();

    /* Both scratch arrays go fully to zero. */
    CHECK(AllZero(g_aBrAA26F0, BR70_AA26F0_COUNT));
    CHECK(AllZero(g_aBrA9DBD8, BR70_AA26F0_COUNT));

    /* 0x10220B20 is zeroed and then dword 0 is re-set -- so exactly one
     * element survives, and it is 0xFFFFFFFF (NOT BrInit220B20's 8). */
    CHECK_EQ(g_a220B20[0], 0xFFFFFFFFu);
    for (i = 1; i < BR70_220B20_COUNT; ++i) {
        CHECK_EQ(g_a220B20[i], 0u);
    }

    /* A WORD store: the high half of the dword is untouched. */
    CHECK_EQ(g_brAA27E0 & 0xFFFFu, 0x0102u);
    CHECK_EQ(g_brAA27E0 >> 16, 0xF00Du);

    /* The constants the block installs. */
    CHECK_EQ(g_br0AC648, 2);
    CHECK_EQ(g_br0AC64C, 1);
    CHECK_EQ(g_br0AC650, 1);
    CHECK_EQ(g_br0AC654, 1);
    CHECK_EQ(g_br0AC658, 3);
    CHECK_EQ(g_brAA289C, 0);

    /* 0x10AA28A4 is cleared before the second BrSprintf reads it, so the
     * second string is "1" too, whatever the field held on entry. */
    CHECK_EQ(g_brAA28A4, 0);
    CHECK(strcmp(g_aBrAA2518, "1") == 0);
    CHECK(strcmp(g_aBrA9D618, "1") == 0);

    /* Order: 0x1003E1D0 runs before the buffers are cleared, 0x1003E510 last. */
    CHECK(LogFind("1003E1D0") >= 0);
    CHECK(LogFind("1003E510") == g_cLog - 1);
    CHECK(LogFind("1003E1D0") < LogFind("1003E510"));

    /* Idempotent: a second reset lands on the same state. */
    {
        uint32_t w = g_brAA27E0;
        LogReset();
        BrExt_1003E680();
        CHECK_EQ(g_a220B20[0], 0xFFFFFFFFu);
        CHECK(AllZero(g_aBrA9DBD8, BR70_AA26F0_COUNT));
        CHECK_EQ(g_brAA27E0, w);
    }

    /* Alias equality. */
    LogReset();
    BrExt_1003E680();
    LogSnapshot();
    LogReset();
    BrSub1003E680();
    CHECK(LogEqualsSnapshot());
}

/* ==========================================================================
 * 0x100173F0
 * ========================================================================== */

static BrHudView g_aViews[4];
static int32_t   g_racePos;

static void SetupHud(int cViews, int iView)
{
    int i;

    LogReset();
    memset(g_aViews, 0, sizeof g_aViews);
    for (i = 0; i < 4; ++i) {
        g_aViews[i].x = 100 + 10 * i;
        g_aViews[i].y = 200 + 20 * i;
        g_aViews[i].w = 320;
        g_aViews[i].h = 240;
    }

    memset(&g_hud,  0, sizeof g_hud);
    memset(&g_race, 0, sizeof g_race);
    g_hud.pRace   = &g_race;
    g_hud.f22AF1C = 0;

    g_screen.cViews = cViews;
    g_screen.iView  = iView;

    g_br0AA010 = 0;
    g_br0BD3E8 = 1;
    g_br0BD3F8 = 1;
    g_br0BD3E0 = 5;
    g_race.cSplits = 1;

    g_racePos      = 0;
    g_pBrRace0FF8  = &g_racePos;
}

static void Test173F0Phase3(void)
{
    /* 0x100AA010 == 3 short-circuits before anything is read, including the
     * view array -- so a NULL array must be safe there. */
    SetupHud(1, 0);
    g_br0AA010 = 3;
    BrSub_100173F0(NULL, 0);
    CHECK_EQ(g_cLog, 0);
}

static void Test173F0IgnoresA2(void)
{
    SetupHud(2, 1);
    BrSub_100173F0(g_aViews, 0);
    LogSnapshot();

    SetupHud(2, 1);
    BrSub_100173F0(g_aViews, 0x7FFFFFFF);
    CHECK(LogEqualsSnapshot());
}

static void Test173F0LapGate(void)
{
    /* 0x100BD3E8 clear removes the lap half and nothing else. */
    SetupHud(2, 1);
    g_br0BD3E8 = 0;
    g_br0BD3F8 = 0;
    BrSub_100173F0(g_aViews, 0);
    CHECK_EQ(g_cLog, 0);

    /* Split screen, race unfinished: the bare "L" replaces string 0xE5. */
    SetupHud(2, 1);
    g_br0BD3F8 = 0;
    BrSub_100173F0(g_aViews, 0);
    CHECK(LogFind("sprintf:%y1L2/5") >= 0);

    /* Three views take the string-table branch instead. */
    SetupHud(3, 1);
    g_br0BD3F8 = 0;
    BrSub_100173F0(g_aViews, 0);
    CHECK(LogFind("sprintf:%y1LAP 2/5") >= 0);

    /* Finished, split screen: no lap text at all. */
    SetupHud(2, 1);
    g_br0BD3F8     = 0;
    g_race.cSplits = 5;
    BrSub_100173F0(g_aViews, 0);
    CHECK_EQ(g_cLog, 0);

    /* Finished, FULL screen: drawn, but with the other string. */
    SetupHud(1, 0);
    g_br0BD3F8     = 0;
    g_race.cSplits = 5;
    BrSub_100173F0(g_aViews, 0);
    CHECK(LogFind("sprintf:FINISHED") >= 0);
}

static void Test173F0ViewAsymmetry(void)
{
    /* x comes from view 0, y from the CURRENT view. Moving view 0's x moves
     * the text; moving view 1's x does not. */
    int i0, i1;

    SetupHud(2, 1);
    g_br0BD3F8 = 0;
    BrSub_100173F0(g_aViews, 0);
    i0 = LogFind("draw:%y1L2/5:116:240");   /* x = views[0].x + 0x10   */
    CHECK(i0 >= 0);                        /* y = views[1].y + 5 + 15 */

    SetupHud(2, 1);
    g_br0BD3F8   = 0;
    g_aViews[1].x = 9999;                    /* the current view's x */
    BrSub_100173F0(g_aViews, 0);
    i1 = LogFind("draw:%y1L2/5:116:240");
    CHECK(i1 >= 0);
}

static void Test173F0Suppress(void)
{
    /* 0x1022AF1C suppresses only the position half. */
    SetupHud(2, 1);
    g_hud.f22AF1C = 1;
    BrSub_100173F0(g_aViews, 0);
    CHECK(LogFind("colors:255:240:125:255:120:0") < 0);
    CHECK(LogFind("sprintf:%y1L2/5") >= 0);

    SetupHud(2, 1);
    g_br0BD3F8 = 0;
    BrSub_100173F0(g_aViews, 0);
    CHECK(LogFind("colors:255:240:125:255:120:0") < 0);
}

static void Test173F0Nudge(void)
{
    /* The suffix nudge: raw on full screen, (2*n)/3 truncated toward zero on
     * split screen. Position 0 is the interesting one: -3 vs -2.
     *
     * Full screen: x = width + n + (views[0].x + 0x10 - 2) + 3
     *              width = strlen("1") * 0x28 = 40
     *              base  = 100 + 16 - 2 = 114 ; so x = 40 + n + 114 + 3 */
    SetupHud(1, 0);
    g_br0BD3E8 = 0;
    g_racePos  = 0;
    BrSub_100173F0(g_aViews, 0);
    CHECK(LogFind("draw:st:154:413") >= 0);     /* 40 - 3 + 114 + 3 = 154 */

    SetupHud(1, 0);
    g_br0BD3E8 = 0;
    g_racePos  = 1;
    BrSub_100173F0(g_aViews, 0);
    CHECK(LogFind("draw:nd:158:413") >= 0);     /* 40 + 1 + 114 + 3 = 158 */

    SetupHud(1, 0);
    g_br0BD3E8 = 0;
    g_racePos  = 2;
    BrSub_100173F0(g_aViews, 0);
    CHECK(LogFind("draw:rd:157:413") >= 0);     /* 40 + 0 + 114 + 3 = 157 */

    /* Split screen: width = 1 * 0x1A = 26; base = 114; nudge folded through
     * (2*n)/3 -- -3 -> -2, 1 -> 0, 0 -> 0. */
    SetupHud(2, 0);
    g_br0BD3E8 = 0;
    g_racePos  = 0;
    BrSub_100173F0(g_aViews, 0);
    CHECK(LogFind("draw:st:141:418") >= 0);     /* 26 - 2 + 114 + 3 = 141 */

    SetupHud(2, 0);
    g_br0BD3E8 = 0;
    g_racePos  = 1;
    BrSub_100173F0(g_aViews, 0);
    CHECK(LogFind("draw:nd:143:418") >= 0);     /* 26 + 0 + 114 + 3 = 143 */

    /* Anything above 2 shares position 1's nudge but not its string. */
    SetupHud(2, 0);
    g_br0BD3E8 = 0;
    g_racePos  = 7;
    BrSub_100173F0(g_aViews, 0);
    CHECK(LogFind("draw:th:143:418") >= 0);
    CHECK(LogFind("sprintf:8") >= 0);           /* the readout is pos + 1 */
}

static void Test173F0Bracket(void)
{
    /* The position half opens with 0x10019240 and closes with 0x10019250, and
     * the close is the LAST thing the function does. */
    SetupHud(2, 0);
    g_br0BD3E8 = 0;
    BrSub_100173F0(g_aViews, 0);
    CHECK(LogFind("textOpen") == 0);
    CHECK(LogFind("textClose") == g_cLog - 1);

    /* Both scale changes happen, in the split-screen sizes. */
    CHECK(LogFind("scale:26") >= 0);
    CHECK(LogFind("scale:13") > LogFind("scale:26"));

    /* NULL race-position pointer degrades to position 0 rather than faulting. */
    SetupHud(2, 0);
    g_br0BD3E8    = 0;
    g_pBrRace0FF8 = NULL;
    BrSub_100173F0(g_aViews, 0);
    CHECK(LogFind("sprintf:1") >= 0);
    CHECK(LogFind("draw:st:141:418") >= 0);
}

/* ==========================================================================
 * main
 * ========================================================================== */

int main(void)
{
    Test1003DB00();
    Test1003C150();

    Test1003C020Counter();
    Test1003C020Quiet();
    Test1003C020ModeGate();
    Test1003C020CC70Gate();
    Test1003C020EventOnce();
    Test1003C020NullInterface();
    Test1003C020Alias();

    Test1003BF60();
    Test1003BF60Gate();

    Test1003E680();

    Test173F0Phase3();
    Test173F0IgnoresA2();
    Test173F0LapGate();
    Test173F0ViewAsymmetry();
    Test173F0Suppress();
    Test173F0Nudge();
    Test173F0Bracket();

    printf("%s: %d checks, %d failures\n",
           (g_cFail == 0) ? "PASS" : "FAIL", g_cRun, g_cFail);
    return (g_cFail == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
