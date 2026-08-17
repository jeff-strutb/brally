/* test_br_window.c -- 0x10019670 (create the window) and 0x10017E30 (the
 * mode-2 EAR startup).
 *
 * The two things worth asserting about 0x10019670 are both consequences of its
 * ESP trace, and both are invisible from the C:
 *
 *   - the ten WNDCLASS fields are TEN fields. Three of them are written
 *     through the same displacement 0x1C at three different points, so a
 *     transcription that read 0x1C as one slot would set one field three
 *     times and leave two zero. The distinctness check below fails on exactly
 *     that mutation.
 *   - the function does NOT store the HWND. It converts CreateWindowExA's
 *     result to a boolean and drops it; 0x105BC72C comes from WM_CREATE.
 *
 * For 0x10017E30 the point is the run-once guard and the decision tree over
 * _EAR_DLL_InitializeEar@4 / _EAR_DLL_GetLastError@0.
 *
 * Every assertion below was mutation-tested: the guarded bug was reinstated in
 * br_window.c, the suite confirmed red, and the source restored.
 */
#include "br_window.h"
#include "br_input.h"
#include "br_boot.h"

#include <stdio.h>
#include <string.h>
#include <stddef.h>

static int g_fails;
#define CHECK(c) do { if (!(c)) { \
    printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); g_fails++; } } while (0)

static char g_inst, g_wnd;
static void *const HINST = &g_inst;
static void *const HWND_A = &g_wnd;

/* ---- recorders ------------------------------------------------------- */
static int  g_cRegister, g_cCreate;
static int  g_fCreateDispatchesWmCreate;
static int  g_fCreateFails;
static BrWindowDesc g_descSeen;

static int rc_register(const BrWindowDesc *d, void *u)
{
    (void)u;
    g_cRegister++;
    g_descSeen = *d;
    return 0;            /* FAILS -- the original ignores this */
}

static void *rc_create(const BrWindowDesc *d, void *u)
{
    (void)d; (void)u;
    g_cCreate++;
    if (g_fCreateFails)
        return NULL;
    /* A real CreateWindowExA dispatches WM_CREATE before it returns. That is
     * how the handle reaches 0x105BC72C, so a host callback must do the same
     * -- and this test drives both ways to prove the dependency is real. */
    if (g_fCreateDispatchesWmCreate)
        (void)BrWndProc(HWND_A, BR_WM_CREATE, 0, 0);
    return HWND_A;
}

/* ---- EAR recorders --------------------------------------------------- */
static int     g_cLoad, g_cValidate, g_cAssign, g_cInit, g_cGetErr, g_cShowErr;
static int32_t g_iLoadArgSeen, g_iCookieSeen, g_iInitArgSeen;
static void   *g_hwndAssigned;
static int32_t g_iLoadResult, g_iInitResult, g_iErrResult;

static int32_t e_load(int32_t w, void *u)
{ (void)u; g_cLoad++; g_iLoadArgSeen = w; return g_iLoadResult; }
static void e_validate(int32_t c, void *u)
{ (void)u; g_cValidate++; g_iCookieSeen = c; }
static void e_assign(void *h, void *u)
{ (void)u; g_cAssign++; g_hwndAssigned = h; }
static int32_t e_init(int32_t a, void *u)
{ (void)u; g_cInit++; g_iInitArgSeen = a; return g_iInitResult; }
static int32_t e_geterr(void *u)      { (void)u; g_cGetErr++; return g_iErrResult; }
static void    e_showerr(void *u)     { (void)u; g_cShowErr++; }

static BrEarOps ear_ops(void)
{
    BrEarOps o;
    o.pfnLoad = e_load; o.pfnAAAValidate = e_validate;
    o.pfnAssignHwnd = e_assign; o.pfnInitializeEar = e_init;
    o.pfnGetLastError = e_geterr; o.pfnShowLastError = e_showerr;
    o.pUser = NULL;
    return o;
}

/* ---- platform recorders (shared with br_input's table) --------------- */
static int g_cMsgBox, g_cExit;
static const char *g_pszLastText;
static const char *g_pszLastCaption;
static uint32_t    g_uLastType;
static char g_aszStr[0x140][8];

static void p_msgbox(void *h, const char *t, const char *c, uint32_t u)
{ (void)h; g_cMsgBox++; g_pszLastText = t; g_pszLastCaption = c; g_uLastType = u; }
static void p_exit(int32_t n) { (void)n; g_cExit++; }
static const char *p_str(int32_t id)
{
    if (id < 1 || id >= 0x12F) return NULL;   /* 0x1006D280's own bounds */
    sprintf(g_aszStr[id], "%d", (int)id);
    return g_aszStr[id];
}

static void arm(void)
{
    BrWindowResetForTest();
    BrInputResetForTest();
    memset(&g_brWndPlatform, 0, sizeof g_brWndPlatform);
    g_brWndPlatform.pfnMessageBox = p_msgbox;
    g_brWndPlatform.pfnExit       = p_exit;
    g_brWndPlatform.pfnString     = p_str;

    g_cRegister = g_cCreate = 0;
    g_fCreateDispatchesWmCreate = 1;
    g_fCreateFails = 0;
    memset(&g_descSeen, 0, sizeof g_descSeen);

    g_cLoad = g_cValidate = g_cAssign = g_cInit = g_cGetErr = g_cShowErr = 0;
    g_iLoadArgSeen = g_iCookieSeen = g_iInitArgSeen = 0;
    g_hwndAssigned = NULL;
    g_iLoadResult = 1; g_iInitResult = 1; g_iErrResult = 0;

    g_cMsgBox = g_cExit = 0;
    g_pszLastText = g_pszLastCaption = NULL;
    g_uLastType = 0;
}

/* ==================================================================== *
 * 1. The WNDCLASS is TEN DISTINCT FIELDS.
 *
 * MUTATIONS KILLED:
 *   a) read [esp+0x1C] as one slot, i.e. set idIcon = idCursor =
 *      idStockBrush from one value            -> the distinctness CHECKs fail
 *   b) uClassStyle = 0 (drop the `mov [esp+8],3`) -> its CHECK fails
 *   c) swap idIcon and idCursor               -> both value CHECKs fail
 * ==================================================================== */
static void test_wndclass_fields(void)
{
    BrWindowDesc d;
    arm();
    g_brhInstance = HINST;
    BrWindowDescribe(&d);

    CHECK(d.uClassStyle  == 3);          /* CS_VREDRAW|CS_HREDRAW */
    CHECK(d.cbClsExtra   == 0);
    CHECK(d.cbWndExtra   == 0);
    CHECK(d.hInstance    == HINST);
    CHECK(d.idIcon       == 0x65);
    CHECK(d.idCursor     == 0x7F00);     /* IDC_ARROW */
    CHECK(d.idStockBrush == 4);

    /* The three that share displacement 0x1C are three different numbers.
     * Stated as a relation, not just as three values, so a mutation that
     * collapses them fails here even if it happens to pick one of the three. */
    CHECK(d.idIcon != d.idCursor);
    CHECK(d.idCursor != d.idStockBrush);
    CHECK(d.idIcon != d.idStockBrush);

    /* The class name string, and the menu name that is the SAME pointer. */
    CHECK(strcmp(d.pszClassName, "BossRally") == 0);
    CHECK(d.pszMenuName == d.pszClassName);      /* 0x1007B378 stored twice */
    CHECK(strcmp(d.pszWindowName, "Boss Rally") == 0);
    /* and they are NOT the same string -- 0x1007B378 vs 0x1007B384 */
    CHECK(strcmp(d.pszClassName, d.pszWindowName) != 0);
}

/* ==================================================================== *
 * 2. CreateWindowExA's arguments, and which of cx/cy is which.
 *
 * MUTATIONS KILLED:
 *   a) swap cx and cy                          -> the size CHECKs fail
 *   b) dwStyle 0x80C20000 -> WS_OVERLAPPEDWINDOW etc -> its CHECK fails
 *   c) read the size once at load rather than per call -> the second pair fails
 * ==================================================================== */
static void test_createwindow_args(void)
{
    BrWindowDesc d;
    arm();

    g_brAppModeW = 640;
    g_brAppModeH = 480;
    BrWindowDescribe(&d);
    CHECK(d.dwExStyle == 0x40000u);          /* WS_EX_APPWINDOW */
    CHECK(d.dwStyle   == 0x80C20000u);
    CHECK(d.x == 0 && d.y == 0);
    CHECK(d.cx == 640);                      /* [0x100A7514] -- WIDTH */
    CHECK(d.cy == 480);                      /* [0x100A7518] -- HEIGHT */

    /* The original reads the two globals at the moment of the call
     * (0x100196EC / 0x100196F2), so a mode change before the window is made
     * is picked up. Asymmetric values so a swap cannot pass. */
    g_brAppModeW = 800;
    g_brAppModeH = 600;
    BrWindowDescribe(&d);
    CHECK(d.cx == 800);
    CHECK(d.cy == 600);
}

/* ==================================================================== *
 * 3. 0x10019670's control flow: RegisterClass's result is ignored, the
 *    handle is not stored here, and the return is a 0/1.
 *
 * MUTATIONS KILLED:
 *   a) `if (!pfnRegisterClass(...)) return 0;` -> the first CHECK fails
 *      (the recorder returns 0 deliberately)
 *   b) `g_brhWnd = hWnd;` inside BrWindowCreate -> the no-WM_CREATE CHECK fails
 *   c) `return (int)(intptr_t)hWnd;`            -> the ==1 CHECK fails
 * ==================================================================== */
static void test_window_create(void)
{
    BrWindowOps ops;
    int rc;

    arm();
    g_brhInstance = HINST;
    ops.pfnRegisterClass = rc_register;
    ops.pfnCreateWindow  = rc_create;
    ops.pUser            = NULL;

    rc = BrWindowCreate(&ops);
    CHECK(rc == 1);                       /* exactly 1, not "the handle" */
    CHECK(g_cRegister == 1);
    CHECK(g_cCreate   == 1);              /* ran despite RegisterClass == 0 */
    CHECK(g_brhWnd == HWND_A);            /* stored by WM_CREATE, not by us */
    CHECK(g_brhInstance2 == HINST);       /* 0x118EEF1C, copied at 0x1001967B */
    CHECK(g_descSeen.uClassStyle == 3);   /* the desc really reached the host */

    /* A host whose create callback does NOT deliver WM_CREATE leaves the
     * handle NULL even though creation "succeeded". That is the whole content
     * of the note in br_window.h and it is asserted rather than only written
     * down. */
    arm();
    g_fCreateDispatchesWmCreate = 0;
    rc = BrWindowCreate(&ops);
    CHECK(rc == 1);
    CHECK(g_brhWnd == NULL);

    /* CreateWindowExA returning NULL is the 0 case. */
    arm();
    g_fCreateFails = 1;
    rc = BrWindowCreate(&ops);
    CHECK(rc == 0);
    CHECK(g_cCreate == 1);
}

/* ==================================================================== *
 * 4. 0x10017E30's RUN-ONCE guard.
 *
 * MUTATIONS KILLED:
 *   a) `if (s_cEarStartupCalls > 1)` / `>= 1`   -> the body-count CHECK fails
 *   b) decrement the counter on the way out     -> the second-call CHECK fails
 *   c) drop the guard entirely                  -> the body-count CHECK fails
 * ==================================================================== */
static void test_ear_run_once(void)
{
    BrEarOps o = ear_ops();
    arm();

    CHECK(BrWindowEarStartup(HWND_A, &o) == 1);
    CHECK(BrWindowEarStartupBodies() == 1);
    CHECK(g_cLoad == 1);

    /* every later call returns 1 having done NOTHING */
    CHECK(BrWindowEarStartup(HWND_A, &o) == 1);
    CHECK(BrWindowEarStartup(HWND_A, &o) == 1);
    CHECK(BrWindowEarStartupBodies() == 1);
    CHECK(g_cLoad == 1);
    CHECK(g_cValidate == 1);
}

/* ==================================================================== *
 * 5. 0x10017E30's sequence and its decision tree.
 *
 * MUTATIONS KILLED:
 *   a) treat pfnLoad != 0 as failure            -> the load-failure CHECK fails
 *   b) treat pfnInitializeEar == 0 as success   -> the success CHECK fails
 *   c) compare GetLastError against something other than 3 -> both err CHECKs fail
 *   d) change BR_EAR_VALIDATE_COOKIE            -> the cookie CHECK fails
 *   e) pass something other than hWnd to AssignHwnd -> its CHECK fails
 * ==================================================================== */
static void test_ear_sequence(void)
{
    BrEarOps o = ear_ops();

    /* --- the happy path: InitializeEar non-zero is SUCCESS --------- */
    arm();
    g_iLoadResult = 1; g_iInitResult = 1;
    BrWindowSetEarDllSelect(0);                 /* earias.dll */
    CHECK(BrWindowEarStartup(HWND_A, &o) == 1);
    CHECK(g_cLoad == 1 && g_iLoadArgSeen == 0);
    CHECK(g_cValidate == 1);
    CHECK(g_iCookieSeen == BR_EAR_VALIDATE_COOKIE);
    CHECK(g_iCookieSeen == 0x009BE9C9);         /* read off 0x10017E93 */
    CHECK(g_cAssign == 1 && g_hwndAssigned == HWND_A);
    CHECK(g_cInit == 1 && g_iInitArgSeen == 0);
    CHECK(g_cGetErr  == 0);                     /* never consulted on success */
    CHECK(g_cShowErr == 0);
    CHECK(g_cMsgBox  == 0 && g_cExit == 0);

    /* the DLL selector is passed through, and it is what picks earpds.dll */
    arm();
    BrWindowSetEarDllSelect(1);
    (void)BrWindowEarStartup(HWND_A, &o);
    CHECK(g_iLoadArgSeen == 1);

    /* --- load failure: message box (0xFE over 0xFD) then exit(1) --- */
    arm();
    g_iLoadResult = 0;
    (void)BrWindowEarStartup(HWND_A, &o);
    CHECK(g_cLoad == 1);
    CHECK(g_cMsgBox == 1);
    CHECK(g_pszLastText    != NULL && strcmp(g_pszLastText,    "254") == 0); /* 0xFE */
    CHECK(g_pszLastCaption != NULL && strcmp(g_pszLastCaption, "253") == 0); /* 0xFD */
    CHECK(g_uLastType == BR_MB_ICONERROR);
    CHECK(g_cExit == 1);

    /* --- InitializeEar 0 and GetLastError 3: its own message ------- */
    arm();
    g_iInitResult = 0; g_iErrResult = 3;
    CHECK(BrWindowEarStartup(HWND_A, &o) == 1);
    CHECK(g_cGetErr  == 1);
    CHECK(g_cShowErr == 0);                     /* the 3 arm does NOT show */
    CHECK(g_cMsgBox  == 1);
    CHECK(g_pszLastText    != NULL && strcmp(g_pszLastText,    "302") == 0); /* 0x12E */
    CHECK(g_pszLastCaption != NULL && strcmp(g_pszLastCaption, "253") == 0); /* 0xFD */
    CHECK(g_cExit == 1);

    /* --- InitializeEar 0 and GetLastError anything else ------------ */
    arm();
    g_iInitResult = 0; g_iErrResult = 5;
    CHECK(BrWindowEarStartup(HWND_A, &o) == 1);
    CHECK(g_cGetErr  == 1);
    CHECK(g_cShowErr == 1);                     /* the middleware shows it */
    CHECK(g_cMsgBox  == 0);                     /* and the game does not */
    CHECK(g_cExit == 1);

    /* 0 is "anything else" too -- the test is == 3, not != 0. */
    arm();
    g_iInitResult = 0; g_iErrResult = 0;
    (void)BrWindowEarStartup(HWND_A, &o);
    CHECK(g_cShowErr == 1);
    CHECK(g_cMsgBox  == 0);
}

/* ==================================================================== *
 * 6. The audio-backend enum, which is what "mode 2" means.
 *
 * Not a behaviour test -- a pin. If a later pass decides mode 2 is something
 * else, this and br_window.h's four-way derivation have to be argued with.
 * ==================================================================== */
static void test_audio_backend_shadow(void)
{
    arm();
    CHECK(BrWindowAudioBackend() == BR_AUDIO_NONE);
    BrWindowSetAudioBackend(BR_AUDIO_EAR);
    CHECK(BrWindowAudioBackend() == 2);
    BrWindowSetAudioBackend(BR_AUDIO_MCI);
    CHECK(BrWindowAudioBackend() == 1);
    BrWindowResetForTest();
    CHECK(BrWindowAudioBackend() == 0);
}

int main(void)
{
    test_wndclass_fields();
    test_createwindow_args();
    test_window_create();
    test_ear_run_once();
    test_ear_sequence();
    test_audio_backend_shadow();

    if (g_fails != 0) { printf("%d FAILURE(S)\n", g_fails); return 1; }
    printf("br_window: all checks passed, 0 failures\n");
    return 0;
}
