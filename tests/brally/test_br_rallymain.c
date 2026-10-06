/* test_br_rallymain.c -- 0x1001CC00, the game's entry point.
 *
 * Almost all of RallyMain is ABORT PATHS, and each one aborts at a different
 * depth and cleans up differently. That is the whole content of the function
 * and it is what these checks pin:
 *
 *   CoInitialize fails  -> CoUninitialize runs, nothing else does
 *   DirectX < 6         -> a message box, NO CoUninitialize, return 0
 *   the start gate fails-> NO message box, NO CoUninitialize, return 0
 *   window create fails -> CoUninitialize runs, the loop does not
 *   pre-loop gate fails -> CoUninitialize runs, the loop does not
 *
 * Two of those skip CoUninitialize and that is the original's behaviour, not
 * an oversight in the port: 0x1001CC89 and 0x1001CC9A both `ret` without
 * reaching 0x1001CD33. It is observable to anything else in the process, so it
 * is preserved and asserted rather than tidied.
 */
#include "br_boot.h"
#include "br_bootfrontier.h"

#include <stdio.h>
#include <string.h>

static int g_fails;
#define CHECK(c) do { if (!(c)) { \
    printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); g_fails++; } } while (0)

typedef struct H {
    int32_t hrCo, dxVersion, fStartGate, fWindow, fPreLoop;
    int cCoInit, cCoUninit, cBox, cDx, cGate, cWindow, cPreLoop, cLoop;
    int32_t idText, idCaption;
} H;

static int32_t h_co   (void *p) { H *h=p; h->cCoInit++;   return h->hrCo; }
static void    h_unco (void *p) { H *h=p; h->cCoUninit++; }
static void    h_box  (void *p, int32_t t, int32_t c)
{ H *h=p; h->cBox++; h->idText=t; h->idCaption=c; }
static int32_t h_dx   (void *p) { H *h=p; h->cDx++;      return h->dxVersion; }
static int32_t h_gate (void *p) { H *h=p; h->cGate++;    return h->fStartGate; }
static int32_t h_win  (void *p) { H *h=p; h->cWindow++;  return h->fWindow; }
static int32_t h_pre  (void *p) { H *h=p; h->cPreLoop++; return h->fPreLoop; }
static void    h_loop (void *p) { H *h=p; h->cLoop++; }

static BrRallyMainOps ops_for(H *h)
{
    BrRallyMainOps o;
    o.pfnCoInitialize = h_co;   o.pfnCoUninitialize = h_unco;
    o.pfnMessageBox   = h_box;  o.pfnDxVersion      = h_dx;
    o.pfnStartGate    = h_gate; o.pfnCreateWindow   = h_win;
    o.pfnPreLoopGate  = h_pre;  o.pfnRunLoop        = h_loop;
    o.pUser = h;
    return o;
}

static void reset(H *h)
{
    memset(h, 0, sizeof *h);
    h->hrCo = 0; h->dxVersion = 0x600;
    h->fStartGate = 1; h->fWindow = 1; h->fPreLoop = 1;
    BrAppResetForTest();
}

static const BrBootArgs kArgs = { (void *)0x1234, (void *)0x5678, "-cmd", 5 };

/* ---- the happy path reaches the loop, once -------------------------- */
static void test_full_boot(void)
{
    H h; BrRallyMainOps o; reset(&h); o = ops_for(&h);
    CHECK(BrRallyMain(&kArgs, &o) == 0);      /* exit code global, still 0 */
    CHECK(h.cCoInit == 1 && h.cCoUninit == 1);
    CHECK(h.cDx == 1 && h.cGate == 1);
    CHECK(h.cWindow == 1 && h.cPreLoop == 1 && h.cLoop == 1);
    CHECK(h.cBox == 0);
    /* the four arguments were stashed for the window creation to read */
    CHECK(BrAppArgs()->hInstance == (void *)0x1234);
    CHECK(BrAppArgs()->nCmdShow == 5);
    CHECK(strcmp(BrAppArgs()->pszCmdLine, "-cmd") == 0);
}

/* ---- CoInitialize failing skips EVERYTHING -------------------------- */
static void test_co_fails(void)
{
    H h; BrRallyMainOps o; reset(&h); h.hrCo = -1; o = ops_for(&h);
    CHECK(BrRallyMain(&kArgs, &o) == 0);
    CHECK(h.cCoUninit == 1);         /* the jump lands on it */
    CHECK(h.cDx == 0);               /* and nothing before it ran */
    CHECK(h.cGate == 0 && h.cWindow == 0 && h.cLoop == 0);
    /* the arguments were NOT stashed: the stores are below the branch */
    CHECK(BrAppArgs()->hInstance == NULL);
}

/* ---- DirectX below 6 shows the box and does NOT uninitialise -------- */
static void test_dx_too_old(void)
{
    H h; BrRallyMainOps o; reset(&h); h.dxVersion = 0x500; o = ops_for(&h);
    CHECK(BrRallyMain(&kArgs, &o) == 0);
    CHECK(h.cBox == 1);
    /* ids are not interchangeable: TEXT is 0x128 and CAPTION is 0x126, the
     * reverse of the order they appear in the listing */
    CHECK(h.idText == 0x128);
    CHECK(h.idCaption == 0x126);
    CHECK(h.cCoUninit == 0);         /* 0x1001CC89 returns without it */
    CHECK(h.cGate == 0 && h.cWindow == 0 && h.cLoop == 0);
}

/* ---- exactly 0x600 is ACCEPTED: the compare is `jae`, not `ja` ------- */
static void test_dx_exactly_six(void)
{
    H h; BrRallyMainOps o; reset(&h); h.dxVersion = 0x600; o = ops_for(&h);
    (void)BrRallyMain(&kArgs, &o);
    CHECK(h.cBox == 0);
    CHECK(h.cLoop == 1);
}

/* ---- the compare is UNSIGNED, and it decides a real case ------------ *
 * BrDxDetect's NT 3.x arm returns WITHOUT writing the version, and RallyMain
 * never initialises the slot, so it can hold a value with bit 31 set. Under
 * `jae` that starts the game; transcribed signed it would refuse. */
static void test_dx_unsigned(void)
{
    H h; BrRallyMainOps o; reset(&h);
    h.dxVersion = (int32_t)0x80000000u; o = ops_for(&h);
    (void)BrRallyMain(&kArgs, &o);
    CHECK(h.cBox == 0);              /* unsigned: 0x80000000 >= 0x600 */
    CHECK(h.cLoop == 1);
}

/* ---- the start gate aborts without a box AND without uninit --------- */
static void test_start_gate(void)
{
    H h; BrRallyMainOps o; reset(&h); h.fStartGate = 0; o = ops_for(&h);
    CHECK(BrRallyMain(&kArgs, &o) == 0);
    CHECK(h.cGate == 1);
    CHECK(h.cBox == 0);
    CHECK(h.cCoUninit == 0);         /* 0x1001CC9A returns without it */
    CHECK(h.cWindow == 0 && h.cLoop == 0);
}

/* ---- window creation failing still uninitialises -------------------- */
static void test_window_fails(void)
{
    H h; BrRallyMainOps o; reset(&h); h.fWindow = 0; o = ops_for(&h);
    (void)BrRallyMain(&kArgs, &o);
    CHECK(h.cWindow == 1);
    CHECK(h.cPreLoop == 0 && h.cLoop == 0);
    CHECK(h.cCoUninit == 1);         /* this one DOES reach 0x1001CD33 */
}

/* ---- the pre-loop gate stops the loop but not the cleanup ----------- */
static void test_preloop_gate(void)
{
    H h; BrRallyMainOps o; reset(&h); h.fPreLoop = 0; o = ops_for(&h);
    (void)BrRallyMain(&kArgs, &o);
    CHECK(h.cPreLoop == 1);
    CHECK(h.cLoop == 0);
    CHECK(h.cCoUninit == 1);
}

/* ---- the config path is built before the config load ---------------- */
static void test_config_path(void)
{
    H h; BrRallyMainOps o; reset(&h); o = ops_for(&h);
    (void)BrRallyMain(&kArgs, &o);
    CHECK(strstr(BrBootConfigPath(), "BossRally.cfg") != NULL);
}

/* ---- a caller supplying no ops gets nothing ------------------------- */
static void test_null_ops(void)
{
    H h; BrRallyMainOps o; reset(&h); o = ops_for(&h);
    CHECK(BrRallyMain(NULL, &o) == 0);
    CHECK(BrRallyMain(&kArgs, NULL) == 0);
    o.pfnRunLoop = NULL;
    CHECK(BrRallyMain(&kArgs, &o) == 0);
    CHECK(h.cCoInit == 0);           /* refused before touching anything */
}

/* ---- the frontier hooks: NULL counts, installed runs ---------------- *
 * The three entries that now have real transcriptions are reached through
 * optional hooks rather than a direct call, so the boot chain takes no link
 * dependency on the config graph. This asserts BOTH halves: with no hook the
 * reach is still counted, and with one it actually runs. */
static int g_cHookRuns;
static void hook_f10(void)              { g_cHookRuns++; }
static void hook_f40(const char *p)     { (void)p; g_cHookRuns++; }
static void hook_63060(void)            { g_cHookRuns++; }

static int32_t frontier_hits(const char *pszNeedle)
{
    int i, n = BrBootFrontierCount();
    for (i = 0; i < n; i++)
        if (strstr(BrBootFrontierName(i), pszNeedle) != NULL)
            return BrBootFrontierHits(i);
    return -1;
}

static void test_frontier_hooks(void)
{
    H h; BrRallyMainOps o;

    /* no hooks: reached, counted, nothing run */
    reset(&h); o = ops_for(&h); g_cHookRuns = 0;
    (void)BrRallyMain(&kArgs, &o);
    CHECK(frontier_hits("0x10007F10") == 1);
    CHECK(frontier_hits("0x10007F40") == 1);
    CHECK(frontier_hits("0x10063060") == 1);
    CHECK(g_cHookRuns == 0);

    /* hooks installed: same counts, and now they run */
    reset(&h); o = ops_for(&h); g_cHookRuns = 0;
    BrBootFrontierInstall(hook_f10, hook_f40, hook_63060);
    (void)BrRallyMain(&kArgs, &o);
    CHECK(frontier_hits("0x10007F40") == 1);
    CHECK(g_cHookRuns == 3);

    /* and a reset clears them again, so one test cannot leak into the next */
    BrAppResetForTest();
    reset(&h); o = ops_for(&h); g_cHookRuns = 0;
    (void)BrRallyMain(&kArgs, &o);
    CHECK(g_cHookRuns == 0);
}

int main(void)
{
    test_frontier_hooks();
    test_full_boot();
    test_co_fails();
    test_dx_too_old();
    test_dx_exactly_six();
    test_dx_unsigned();
    test_start_gate();
    test_window_fails();
    test_preloop_gate();
    test_config_path();
    test_null_ops();

    if (g_fails != 0) { printf("%d FAILURE(S)\n", g_fails); return 1; }
    printf("br_rallymain: all checks passed\n");
    return 0;
}
