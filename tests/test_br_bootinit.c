/* test_br_bootinit.c -- 0x10032530.
 *
 * The function has no arithmetic; its whole content is WHICH calls happen, in
 * WHAT ORDER, with WHAT arguments, and under WHICH of the two guards.  So the
 * test records the call order as a string and compares the whole thing --
 * moving any call, or moving the guard boundary by one call, changes it.
 *
 * The three facts that are easiest to get wrong and are each asserted:
 *   - the music guard covers THREE calls, so PlayMusic=0 also skips the
 *     volume tables at 0x10059E00 and the CD track;
 *   - the last call is a TAIL JUMP, so its return value is the function's;
 *   - the POD name and the splash argument are literals, and the splash
 *     argument is 0x2AC7E58B and not the 0 state 3 passes.
 */
#include "br_bootinit.h"

#include <stdio.h>
#include <string.h>

static int g_fails;
#define CHECK(c) do { if (!(c)) { \
    printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); g_fails++; } } while (0)

static char     g_szLog[512];
static char     g_szPodName[64];
static char     g_szImage[64];
static uint32_t g_uImageArg;
static int32_t  g_n639D0;
static int32_t  g_nTrack;
static void    *g_pPodSeen;
static void    *g_hWndSeen;
static int32_t  g_rcSfx;

static void logs(const char *psz)
{
    size_t n = strlen(g_szLog);
    snprintf(g_szLog + n, sizeof g_szLog - n, "%s%s",
             (n != 0) ? "," : "", psz);
}

static void f_setname(void *pPod, const char *pszName)
{
    g_pPodSeen = pPod;
    snprintf(g_szPodName, sizeof g_szPodName, "%s", pszName);
    logs("setname");
}
static void f_open(void *pPod)   { g_pPodSeen = pPod; logs("open"); }
static void f_639D0(int32_t a)   { g_n639D0 = a;      logs("639D0"); }
static void f_6C990(const char *pszImage, uint32_t arg)
{
    snprintf(g_szImage, sizeof g_szImage, "%s", pszImage);
    g_uImageArg = arg;
    logs("6C990");
}
static void f_5A480(void)        { logs("5A480"); }
static void f_71FC0(void)        { logs("71FC0"); }
static void f_703D0(void)        { logs("703D0"); }
static void f_28E0(void *hWnd)   { g_hWndSeen = hWnd; logs("28E0"); }
static void f_59E00(void)        { logs("59E00"); }
static void f_2AF0(int32_t t)    { g_nTrack = t;      logs("2AF0"); }
static int32_t f_6C4D0(void)     { logs("6C4D0"); return g_rcSfx; }

static char g_PodObject[4];
static char g_WndObject[4];

static BrBootColdInitOps g_ops;

static void arm(void)
{
    memset(&g_ops, 0, sizeof g_ops);
    g_ops.pPod          = g_PodObject;
    g_ops.pfnPodSetName = f_setname;
    g_ops.pfnPodOpen    = f_open;
    g_ops.pfn100639D0   = f_639D0;
    g_ops.pfn1006C990   = f_6C990;
    g_ops.pfn1005A480   = f_5A480;
    g_ops.pfn10071FC0   = f_71FC0;
    g_ops.pfn100703D0   = f_703D0;
    g_ops.hWnd          = g_WndObject;
    g_ops.pfn100028E0   = f_28E0;
    g_ops.pfn10059E00   = f_59E00;
    g_ops.pfn10002AF0   = f_2AF0;
    g_ops.pfn1006C4D0   = f_6C4D0;

    g_szLog[0] = 0;
    g_szPodName[0] = 0;
    g_szImage[0] = 0;
    g_uImageArg = 0;
    g_n639D0 = -1;
    g_nTrack = -1;
    g_pPodSeen = NULL;
    g_hWndSeen = NULL;
    g_rcSfx = 0;
    BrBootColdInitResetForTest();
}

/* ---- both flags off: the seven unconditional calls, in order --------- */
static void test_order_no_flags(void)
{
    int32_t rc;

    arm();
    rc = BrBootColdInit(&g_ops, 0, 0);
    CHECK(strcmp(g_szLog,
        "setname,open,639D0,6C990,5A480,71FC0,703D0") == 0);
    CHECK(rc == 0);
}

/* ---- the arguments are the image's literals -------------------------- */
static void test_arguments(void)
{
    arm();
    (void)BrBootColdInit(&g_ops, 1, 1);
    CHECK(strcmp(g_szPodName, "BossRally.pod") == 0);
    CHECK(strcmp(g_szImage,   "splash.img") == 0);
    CHECK(g_uImageArg == 0x2AC7E58Bu);
    CHECK(g_n639D0 == 3);
    CHECK(g_nTrack == 2);
    CHECK(g_pPodSeen == g_PodObject);
    CHECK(g_hWndSeen == g_WndObject);
}

/* ---- ONE guard over THREE calls -------------------------------------- */
static void test_music_guard_covers_three(void)
{
    arm();
    (void)BrBootColdInit(&g_ops, 1, 0);
    CHECK(strcmp(g_szLog,
        "setname,open,639D0,6C990,5A480,71FC0,703D0,28E0,59E00,2AF0") == 0);

    arm();
    (void)BrBootColdInit(&g_ops, 0, 0);
    CHECK(strstr(g_szLog, "59E00") == NULL);   /* volume tables skipped too */
    CHECK(strstr(g_szLog, "2AF0")  == NULL);
    CHECK(strstr(g_szLog, "28E0")  == NULL);
}

/* ---- any non-zero passes the guard, not just 1 ----------------------- */
static void test_guard_is_nonzero_not_one(void)
{
    arm();
    (void)BrBootColdInit(&g_ops, 2, 0);        /* the shipped PlayMusic=2 */
    CHECK(strstr(g_szLog, "28E0") != NULL);

    arm();
    (void)BrBootColdInit(&g_ops, -1, 0);
    CHECK(strstr(g_szLog, "28E0") != NULL);
}

/* ---- the sfx call is a TAIL JUMP: its value is the return value ------ */
static void test_tail_jump_returns_callee(void)
{
    int32_t rc;

    arm();
    g_rcSfx = 0x1234;
    rc = BrBootColdInit(&g_ops, 0, 1);
    CHECK(rc == 0x1234);
    CHECK(strcmp(g_szLog,
        "setname,open,639D0,6C990,5A480,71FC0,703D0,6C4D0") == 0);

    /* guard off: 0, and the callee is not reached */
    arm();
    g_rcSfx = 0x1234;
    rc = BrBootColdInit(&g_ops, 0, 0);
    CHECK(rc == 0);
    CHECK(strstr(g_szLog, "6C4D0") == NULL);
}

/* ---- everything on, in one order ------------------------------------- */
static void test_full_order(void)
{
    arm();
    (void)BrBootColdInit(&g_ops, 1, 1);
    CHECK(strcmp(g_szLog,
        "setname,open,639D0,6C990,5A480,71FC0,703D0,"
        "28E0,59E00,2AF0,6C4D0") == 0);
}

/* ---- a missing hook is counted, never faked -------------------------- */
static void test_unhooked_is_counted(void)
{
    int32_t rc;

    arm();
    g_ops.pfn1005A480 = NULL;
    g_ops.pfn1006C4D0 = NULL;
    g_rcSfx = 0x1234;
    rc = BrBootColdInit(&g_ops, 0, 1);

    CHECK(strstr(g_szLog, "5A480") == NULL);
    CHECK(BrBootColdInitSkipped(BR_COLDINIT_1005A480) == 1);
    CHECK(BrBootColdInitSkipped(BR_COLDINIT_1006C4D0) == 1);
    CHECK(BrBootColdInitSkipped(BR_COLDINIT_100703D0) == 0);
    /* the tail call did not happen, so no value was produced */
    CHECK(rc == 0);

    /* a step under a guard that did not run is not counted as skipped */
    CHECK(BrBootColdInitSkipped(BR_COLDINIT_10002AF0) == 0);
}

/* ---- no ops at all: nothing happens ---------------------------------- */
static void test_no_ops(void)
{
    arm();
    CHECK(BrBootColdInit(NULL, 1, 1) == 0);
    CHECK(g_szLog[0] == 0);
}

int main(void)
{
    test_order_no_flags();
    test_arguments();
    test_music_guard_covers_three();
    test_guard_is_nonzero_not_one();
    test_tail_jump_returns_callee();
    test_full_order();
    test_unhooked_is_counted();
    test_no_ops();

    if (g_fails != 0) { printf("%d FAILURE(S)\n", g_fails); return 1; }
    printf("br_bootinit: all checks passed\n");
    return 0;
}
