/* test_br_input.c -- 0x100194C0 and its three handlers.
 *
 * WHAT IS WORTH ASSERTING HERE, AND WHY EACH ONE CAN FAIL
 *
 * Every check below guards a specific misreading that this decompilation could
 * plausibly have made, and each was verified by REINSTATING that misreading in
 * br_input.c and confirming the suite goes red. The list, and the mutation
 * that kills it, is in the block comment above each group. A test that cannot
 * fail is worse than none -- CONVENTIONS.md, and two were found in this tree
 * in two days.
 *
 * The headline is the gate polarity. 0x105BC748 is WM_ACTIVATE's fMinimized,
 * and the main loop tests it INVERTED, so swapping the LOWORD and HIWORD in
 * BrOnActivate produces a game that runs only while minimised -- a defect that
 * presents as a hang, not as a wrong number. test_activate_gates fails on that
 * mutation in both directions.
 */
#include "br_input.h"
#include "br_window.h"
#include "br_mainloop.h"
#include "br_boot.h"

#include <stdio.h>
#include <stddef.h>

static int g_fails;
#define CHECK(c) do { if (!(c)) { \
    printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); g_fails++; } } while (0)

/* WM_ACTIVATE's wParam: LOWORD = WA_*, HIWORD = fMinimized. */
#define MKW(lo, hi) ((BrWParam)(((uint32_t)(hi) << 16) | ((uint32_t)(lo) & 0xFFFFu)))

/* ---- recorders for the platform ops --------------------------------- */
static int      g_cPostQuit, g_cSetCursor, g_cInvalidate, g_cMsgBox;
static int      g_cPostMsg,  g_cGetWindowLong, g_cExit;
static uint32_t g_uLastPostMsg;
static int32_t  g_iLastGwl;

static void  r_postquit(int32_t n)      { (void)n; g_cPostQuit++; }
static void  r_setcursor(void)          { g_cSetCursor++; }
static void  r_invalidate(void *h)      { (void)h; g_cInvalidate++; }
static void  r_msgbox(void *h, const char *t, const char *c, uint32_t u)
{ (void)h; (void)t; (void)c; (void)u; g_cMsgBox++; }
static void  r_postmsg(void *h, uint32_t m, BrWParam w, BrLParam l)
{ (void)h; (void)w; (void)l; g_cPostMsg++; g_uLastPostMsg = m; }
static int32_t r_gwl(void *h, int32_t i) { (void)h; g_cGetWindowLong++; g_iLastGwl = i; return 0x1234; }
static void  r_exit(int32_t n)          { (void)n; g_cExit++; }
static const char *r_str(int32_t id)    { (void)id; return "s"; }

/* ---- recorders for the two pre-dispatch hooks ------------------------ */
static int g_cHookA, g_cHookB;
static void h_a(int32_t a, void *h, uint32_t m, BrWParam w, BrLParam l)
{ (void)a; (void)h; (void)m; (void)w; (void)l; g_cHookA++; }
static void h_b(void *h, uint32_t m, BrWParam w, BrLParam l)
{ (void)h; (void)m; (void)w; (void)l; g_cHookB++; }

static char       g_wnd;               /* a distinguishable fake HWND */
static void *const HWND_A = &g_wnd;

static void arm(void)
{
    BrInputResetForTest();
    BrWindowResetForTest();
    g_cPostQuit = g_cSetCursor = g_cInvalidate = g_cMsgBox = 0;
    g_cPostMsg  = g_cGetWindowLong = g_cExit = 0;
    g_uLastPostMsg = 0; g_iLastGwl = 0;
    g_cHookA = g_cHookB = 0;

    g_brWndPlatform.pfnPostQuit      = r_postquit;
    g_brWndPlatform.pfnSetCursorNone = r_setcursor;
    g_brWndPlatform.pfnInvalidate    = r_invalidate;
    g_brWndPlatform.pfnMessageBox    = r_msgbox;
    g_brWndPlatform.pfnPostMessage   = r_postmsg;
    g_brWndPlatform.pfnGetWindowLong = r_gwl;
    g_brWndPlatform.pfnExit          = r_exit;
    g_brWndPlatform.pfnString        = r_str;

    BrMainLoopSetReady(0, 0);
    BrMainLoopSetSuspended(0);
}

/* ==================================================================== *
 * 1. WM_ACTIVATE owns all three gates, and the third one is fMinimized.
 *
 * MUTATIONS KILLED:
 *   a) swap lo/hi in BrOnActivate            -> both halves of this fail
 *   b) drop the >>16, use wParam directly    -> the minimised case fails
 *   c) call BrMainLoopSetSuspended(!hi)      -> every case fails
 * ==================================================================== */
static void test_activate_gates(void)
{
    arm();

    /* WA_ACTIVE, not minimised: the frame runs. */
    BrOnActivate(MKW(1, 0));
    CHECK(BrMainLoopReady1()    == 1);
    CHECK(BrMainLoopReady2()    == 1);
    CHECK(BrMainLoopSuspended() == 0);
    CHECK(BrMainLoopFrameAllowed() == 1);

    /* WA_ACTIVE and MINIMISED: the frame must NOT run. This is the case that
     * distinguishes the correct reading from the swapped one -- under the swap
     * the gate would be OPEN here. */
    BrOnActivate(MKW(1, 1));
    CHECK(BrMainLoopReady2()    == 1);
    CHECK(BrMainLoopSuspended() == 1);
    CHECK(BrMainLoopFrameAllowed() == 0);

    /* WA_INACTIVE, not minimised: gate 2 shuts. Under the swap this would
     * report suspended instead, so the pair of assertions pins the direction
     * rather than just the answer. */
    BrOnActivate(MKW(0, 0));
    CHECK(BrMainLoopReady2()    == 0);
    CHECK(BrMainLoopSuspended() == 0);
    CHECK(BrMainLoopFrameAllowed() == 0);

    /* WA_CLICKACTIVE is 2, and it is a NON-ZERO state -- the gate is a
     * test-against-zero, not a comparison with 1. */
    BrOnActivate(MKW(2, 0));
    CHECK(BrMainLoopFrameAllowed() == 1);

    /* Bits above 31 must not reach the HIWORD on LP64. The original's
     * `shr eax,0x10` works on a 32-bit register; without the (uint32_t) mask
     * this would come back suspended. */
    BrOnActivate((BrWParam)MKW(1, 0) | (BrWParam)0x100000000ULL);
    CHECK(BrMainLoopSuspended() == 0);
    CHECK(BrMainLoopFrameAllowed() == 1);
}

/* ==================================================================== *
 * 2. WM_CREATE is the ONLY writer of the window handle.
 *
 * MUTATIONS KILLED:
 *   a) move the store to any other arm       -> the first two CHECKs fail
 *   b) store lParam or wParam instead of hWnd-> the identity CHECK fails
 *   c) return fCallDefault = 1 from WM_CREATE-> the third CHECK fails
 * ==================================================================== */
static void test_wm_create_stores_hwnd(void)
{
    BrWndResult r;

    arm();
    CHECK(g_brhWnd == NULL);

    r = BrWndProc(HWND_A, BR_WM_DESTROY, 0, 0);
    CHECK(g_brhWnd == NULL);          /* not this one */

    arm();
    r = BrWndProc(HWND_A, BR_WM_CREATE, 0, 0);
    CHECK(g_brhWnd == HWND_A);
    CHECK(r.fCallDefault == 0);       /* 0 lets creation proceed */
    CHECK(r.lResult == 0);
}

/* ==================================================================== *
 * 3. THE PROLOGUE'S ESP TRACE: uMsg, wParam and lParam are three different
 *    things, and two of them are read through the same displacement 0x14.
 *
 * The mutation this guards is the one the brief warned about and that this
 * project has shipped twice: reading [esp+0x14] as one slot, which makes the
 * procedure switch on lParam.
 *
 * MUTATIONS KILLED:
 *   a) switch (lParam) instead of uMsg       -> both cross-checks fail
 *   b) BrOnSysCommand tests lParam not wParam-> the last pair fails
 * ==================================================================== */
static void test_argument_identity(void)
{
    BrWndResult r;

    arm();
    /* uMsg = WM_SETCURSOR, lParam = WM_SYSCOMMAND. If the switch ran on
     * lParam this would take the SYSCOMMAND arm and the cursor would not be
     * hidden. */
    r = BrWndProc(HWND_A, BR_WM_SETCURSOR, 0, (BrLParam)BR_WM_SYSCOMMAND);
    CHECK(g_cSetCursor == 1);
    CHECK(r.fCallDefault == 0);
    CHECK(r.lResult == 1);

    /* And the other way round. */
    arm();
    r = BrWndProc(HWND_A, BR_WM_SYSCOMMAND, BR_SC_MOVE, (BrLParam)BR_WM_SETCURSOR);
    CHECK(g_cSetCursor == 0);
    CHECK(r.fCallDefault == 0);       /* SC_MOVE is swallowed */

    /* wParam is not lParam: the command must come from wParam. */
    arm();
    r = BrWndProc(HWND_A, BR_WM_SYSCOMMAND, 0, (BrLParam)BR_SC_MOVE);
    CHECK(r.fCallDefault == 1);       /* wParam 0 is not a blocked command */
}

/* ==================================================================== *
 * 4. WM_SYSCOMMAND: three exact values, NO 0xFFF0 mask, and a dead
 *    GetWindowLongA that is still called.
 *
 * MUTATIONS KILLED:
 *   a) `sc &= 0xFFF0` before comparing        -> the 0xF012 CHECK fails
 *   b) add SC_MINIMIZE 0xF020 to the set      -> the 0xF020 CHECK fails
 *   c) drop the GetWindowLongA call           -> the count/index CHECKs fail
 * ==================================================================== */
static void test_syscommand(void)
{
    BrWndResult r;
    arm();

    r = BrWndProc(HWND_A, BR_WM_SYSCOMMAND, BR_SC_SIZE, 0);
    CHECK(r.fCallDefault == 0 && r.lResult == 0);
    r = BrWndProc(HWND_A, BR_WM_SYSCOMMAND, BR_SC_MOVE, 0);
    CHECK(r.fCallDefault == 0);
    r = BrWndProc(HWND_A, BR_WM_SYSCOMMAND, BR_SC_MAXIMIZE, 0);
    CHECK(r.fCallDefault == 0);

    /* SC_MINIMIZE is 0xF020 and sits BETWEEN two blocked values. The original
     * subtracts 0x10 and then 0x20, i.e. it steps 0xF000 -> 0xF010 -> 0xF030
     * and never lands on 0xF020. Minimising is allowed. */
    r = BrWndProc(HWND_A, BR_WM_SYSCOMMAND, 0xF020, 0);
    CHECK(r.fCallDefault == 1);

    /* The unmasked comparison: a keyboard-initiated SC_MOVE is 0xF012 and is
     * NOT blocked. Preserved from the instruction stream. */
    r = BrWndProc(HWND_A, BR_WM_SYSCOMMAND, 0xF012, 0);
    CHECK(r.fCallDefault == 1);

    /* SC_CLOSE 0xF060 reaches DefWindowProc, which is how the window closes. */
    r = BrWndProc(HWND_A, BR_WM_SYSCOMMAND, 0xF060, 0);
    CHECK(r.fCallDefault == 1);

    /* GetWindowLongA(hWnd, GWL_USERDATA) runs on every one of those, and its
     * result is thrown away. Six calls, six lookups. */
    CHECK(g_cGetWindowLong == 6);
    CHECK(g_iLastGwl == BR_GWL_USERDATA);
}

/* ==================================================================== *
 * 5. The two pre-dispatch hooks, and the NESTING that is easy to miss.
 *
 * MUTATIONS KILLED:
 *   a) un-nest hook B (test 0x10AC408C on its own) -> the nesting CHECK fails
 *   b) drop the +0x68 condition                    -> the +0x68 CHECK fails
 *   c) run the hooks after the dispatch            -> not observable here, so
 *      it is NOT claimed; the order is documented, not asserted.
 * ==================================================================== */
static void test_prehooks(void)
{
    arm();
    g_pfnBrWndMsgHookA = h_a;
    g_pfnBrWndMsgHookB = h_b;

    /* object NULL: neither hook. */
    (void)BrWndProc(HWND_A, BR_WM_SETCURSOR, 0, 0);
    CHECK(g_cHookA == 0 && g_cHookB == 0);

    /* object set but its +0x68 field zero: still neither. */
    BrWndShadowSet(BR_SH_10AC5C5C, 1);
    (void)BrWndProc(HWND_A, BR_WM_SETCURSOR, 0, 0);
    CHECK(g_cHookA == 0 && g_cHookB == 0);

    /* both parts of the guard: hook A only, because 0x10AC408C is clear. */
    BrWndShadowSet(BR_SH_10AC5C5C_68, 1);
    (void)BrWndProc(HWND_A, BR_WM_SETCURSOR, 0, 0);
    CHECK(g_cHookA == 1 && g_cHookB == 0);

    /* 0x10AC408C alone is NOT enough -- it is nested inside hook A's guard. */
    BrWndShadowSet(BR_SH_10AC5C5C,    0);
    BrWndShadowSet(BR_SH_10AC5C5C_68, 0);
    BrWndShadowSet(BR_SH_10AC408C,    1);
    (void)BrWndProc(HWND_A, BR_WM_SETCURSOR, 0, 0);
    CHECK(g_cHookA == 1 && g_cHookB == 0);   /* still 1 and 0 */

    /* all three: both hooks. */
    BrWndShadowSet(BR_SH_10AC5C5C,    1);
    BrWndShadowSet(BR_SH_10AC5C5C_68, 1);
    (void)BrWndProc(HWND_A, BR_WM_SETCURSOR, 0, 0);
    CHECK(g_cHookA == 2 && g_cHookB == 1);

    /* The hooks run for EVERY message, including ones that go straight to
     * DefWindowProc -- they are before the dispatch, not inside it. */
    (void)BrWndProc(HWND_A, 0x0100 /* WM_KEYDOWN */, 0, 0);
    CHECK(g_cHookA == 3 && g_cHookB == 2);
}

/* ==================================================================== *
 * 6. WM_DEVICECHANGE exists only in mode 2, and DEVICEARRIVAL does not also
 *    run the teardown.
 *
 * MUTATIONS KILLED:
 *   a) drop the mode == 2 guard              -> the mode-0 and mode-1 CHECKs fail
 *   b) make the 0x8001/3/4 test an `else`    -> not caught here; the fall-
 *      through only matters for 0x8000, which fails all three anyway. The
 *      CHECK that 0x10002F70 stays at 0 after 0x8000 is what pins it, and it
 *      fails if the teardown is hoisted out of the test.
 *   c) include 0x8002 in the teardown set    -> the 0x8002 CHECK fails
 * ==================================================================== */
static void test_devicechange(void)
{
    BrWndResult r;

    /* mode 0 and mode 1: the message is not ours. */
    arm();
    BrWindowSetAudioBackend(BR_AUDIO_NONE);
    r = BrWndProc(HWND_A, BR_WM_DEVICECHANGE, BR_DBT_DEVICEARRIVAL, 0);
    CHECK(r.fCallDefault == 1);
    CHECK(BrWndFrontierHits(BR_WF_10002580) == 0);

    BrWindowSetAudioBackend(BR_AUDIO_MCI);
    r = BrWndProc(HWND_A, BR_WM_DEVICECHANGE, BR_DBT_DEVICEARRIVAL, 0);
    CHECK(r.fCallDefault == 1);
    CHECK(BrWndFrontierHits(BR_WF_10002580) == 0);

    /* mode 2, DEVICEARRIVAL: create the channel and start a track. */
    arm();
    BrWindowSetAudioBackend(BR_AUDIO_EAR);
    r = BrWndProc(HWND_A, BR_WM_DEVICECHANGE, BR_DBT_DEVICEARRIVAL, 0);
    CHECK(r.fCallDefault == 0);
    CHECK(r.lResult == 1);
    CHECK(BrWndFrontierHits(BR_WF_10002580) == 1);
    CHECK(BrWndFrontierHits(BR_WF_10002AF0) == 1);
    /* and NOT the teardown -- 0x8000 falls through the three removal tests
     * and fails every one of them. */
    CHECK(BrWndFrontierHits(BR_WF_10002F70) == 0);
    CHECK(BrWndFrontierHits(BR_WF_10002760) == 0);

    /* the three removal values, and one that is NOT in the set */
    arm();
    BrWindowSetAudioBackend(BR_AUDIO_EAR);
    r = BrWndProc(HWND_A, BR_WM_DEVICECHANGE, BR_DBT_DEVICEQUERYREMOVE, 0);
    CHECK(r.lResult == 1 && r.fCallDefault == 0);
    (void)BrWndProc(HWND_A, BR_WM_DEVICECHANGE, BR_DBT_DEVICEREMOVEPENDING, 0);
    (void)BrWndProc(HWND_A, BR_WM_DEVICECHANGE, BR_DBT_DEVICEREMOVECOMPLETE, 0);
    CHECK(BrWndFrontierHits(BR_WF_10002F70) == 3);
    CHECK(BrWndFrontierHits(BR_WF_10002760) == 3);
    CHECK(BrWndFrontierHits(BR_WF_10002580) == 0);

    (void)BrWndProc(HWND_A, BR_WM_DEVICECHANGE, 0x8002, 0);
    CHECK(BrWndFrontierHits(BR_WF_10002F70) == 3);   /* 0x8002 is not in the set */
    /* an unknown wParam still returns 1 -- 0x1001956F jumps to 0x1001960C */
    r = BrWndProc(HWND_A, BR_WM_DEVICECHANGE, 0x1234, 0);
    CHECK(r.fCallDefault == 0 && r.lResult == 1);
}

/* ==================================================================== *
 * 7. MM_MCINOTIFY exists only in mode 1, and needs all three conditions.
 *
 * MUTATIONS KILLED:
 *   a) drop the mode == 1 guard              -> the mode-2 CHECK fails
 *   b) drop any one of the three conditions  -> its CHECK fails
 *   c) return fCallDefault = 1 in mode 1     -> the mode-1 return CHECK fails
 * ==================================================================== */
static void test_mcinotify(void)
{
    BrWndResult r;
    const BrLParam idDev = 0x4321;

    arm();
    BrWindowSetAudioBackend(BR_AUDIO_EAR);       /* wrong mode */
    BrWndShadowSet(BR_SH_1021C770, (int32_t)idDev);
    r = BrWndProc(HWND_A, BR_MM_MCINOTIFY, 1, idDev);
    CHECK(r.fCallDefault == 1);
    CHECK(BrWndFrontierHits(BR_WF_10002830) == 0);

    arm();
    BrWindowSetAudioBackend(BR_AUDIO_MCI);
    BrWndShadowSet(BR_SH_1021C770, (int32_t)idDev);

    /* all three conditions -> advance */
    r = BrWndProc(HWND_A, BR_MM_MCINOTIFY, 1, idDev);
    CHECK(r.fCallDefault == 0 && r.lResult == 0);
    CHECK(BrWndFrontierHits(BR_WF_10002830) == 1);

    /* wrong device id */
    r = BrWndProc(HWND_A, BR_MM_MCINOTIFY, 1, idDev + 1);
    CHECK(BrWndFrontierHits(BR_WF_10002830) == 1);
    CHECK(r.fCallDefault == 0);          /* still swallowed, still 0 */

    /* wParam must be MCI_NOTIFY_SUCCESSFUL == 1 */
    r = BrWndProc(HWND_A, BR_MM_MCINOTIFY, 2, idDev);
    CHECK(BrWndFrontierHits(BR_WF_10002830) == 1);

    /* 0x105CCB5C must be clear */
    BrWndShadowSet(BR_SH_105CCB5C, 1);
    r = BrWndProc(HWND_A, BR_MM_MCINOTIFY, 1, idDev);
    CHECK(BrWndFrontierHits(BR_WF_10002830) == 1);
    BrWndShadowSet(BR_SH_105CCB5C, 0);
    r = BrWndProc(HWND_A, BR_MM_MCINOTIFY, 1, idDev);
    CHECK(BrWndFrontierHits(BR_WF_10002830) == 2);
}

/* ==================================================================== *
 * 8. The EAR registered message: mode 2, lParam == 2, wParam == [0x1021C788],
 *    and EVERY path returns 0.
 *
 * MUTATIONS KILLED:
 *   a) compare lParam against 1 or against the mode variable's later value
 *                                            -> the lParam CHECK fails
 *   b) swap the wParam / lParam roles         -> the match CHECK fails
 *   c) return fCallDefault = 1                -> the return CHECKs fail
 * ==================================================================== */
static void test_ear_message(void)
{
    BrWndResult r;
    const uint32_t uEar = 0xC123;

    arm();
    BrWindowSetAudioBackend(BR_AUDIO_EAR);
    BrWndShadowSet(BR_SH_104B1620, (int32_t)uEar);
    BrWndShadowSet(BR_SH_1021C788, 7);

    r = BrWndProc(HWND_A, uEar, 7, 2);
    CHECK(r.fCallDefault == 0 && r.lResult == 0);
    CHECK(BrWndFrontierHits(BR_WF_10002CF0) == 1);

    r = BrWndProc(HWND_A, uEar, 7, 1);          /* lParam must be 2 */
    CHECK(r.fCallDefault == 0 && r.lResult == 0);
    CHECK(BrWndFrontierHits(BR_WF_10002CF0) == 1);

    r = BrWndProc(HWND_A, uEar, 8, 2);          /* wParam must match 0x1021C788 */
    CHECK(r.fCallDefault == 0 && r.lResult == 0);
    CHECK(BrWndFrontierHits(BR_WF_10002CF0) == 1);

    /* mode 1 does not look at the registered message at all. */
    arm();
    BrWindowSetAudioBackend(BR_AUDIO_MCI);
    BrWndShadowSet(BR_SH_104B1620, (int32_t)uEar);
    r = BrWndProc(HWND_A, uEar, 7, 2);
    CHECK(r.fCallDefault == 1);

    /* THE PRESERVED ODDITY: with the DLL never loaded, 0x104B1620 is 0, and in
     * mode 2 a WM_NULL therefore takes the EAR arm instead of reaching
     * DefWindowProc. The original does this. */
    arm();
    BrWindowSetAudioBackend(BR_AUDIO_EAR);
    CHECK(BrWndShadowGet(BR_SH_104B1620) == 0);
    r = BrWndProc(HWND_A, 0x0000, 0, 0);
    CHECK(r.fCallDefault == 0 && r.lResult == 0);
}

/* ==================================================================== *
 * 9. WM_DESTROY posts the quit message and STILL calls DefWindowProc.
 *
 * MUTATIONS KILLED:
 *   a) return fCallDefault = 0 after PostQuitMessage -> the CHECK fails
 *   b) drop the 0x100325B0 teardown call             -> its hit CHECK fails
 * ==================================================================== */
static void test_wm_destroy(void)
{
    BrWndResult r;
    arm();
    r = BrWndProc(HWND_A, BR_WM_DESTROY, 0, 0);
    CHECK(g_cPostQuit == 1);
    CHECK(BrWndFrontierHits(BR_WF_100325B0) == 1);
    CHECK(r.fCallDefault == 1);      /* 0x100195AC is reached by fall-through */
}

/* ==================================================================== *
 * 10. WM_ACTIVATEAPP writes ONLY gate 1, and its two halves are not an
 *     if/else.
 *
 * MUTATIONS KILLED:
 *   a) BrMainLoopSetReady(fActive, fActive)  -> the "gate 2 survives" CHECK fails
 *   b) make the activate half an `else`      -> the deactivate-then-tail CHECK fails
 *   c) hoist the four calls out of the latch guard -> the latch CHECK fails
 * ==================================================================== */
static void test_activateapp(void)
{
    BrWndResult r;

    /* gate 2 must survive a deactivation, because 0x10019350 never touches it */
    arm();
    BrOnActivate(MKW(1, 0));                 /* gate1 = gate2 = 1 */
    CHECK(BrMainLoopReady2() == 1);
    r = BrOnActivateApp(HWND_A, 0, 0);       /* fActive = 0 */
    CHECK(BrMainLoopReady1() == 0);
    CHECK(BrMainLoopReady2() == 1);          /* untouched */
    CHECK(r.fCallDefault == 1);              /* always DefWindowProc */

    /* the deactivate block ran in full */
    CHECK(BrWndFrontierHits(BR_WF_100609F0) == 1);
    CHECK(BrWndFrontierHits(BR_WF_10061440) == 1);
    CHECK(BrWndFrontierHits(BR_WF_1007296C) == 1);
    CHECK(BrWndShadowGet(BR_SH_100A9354) == 1);
    CHECK(g_cInvalidate == 1);
    /* 0x10226A48 and 0x10226A44 are both zero, so the arm was not taken and
     * the latch was set instead. */
    CHECK(BrWndFrontierHits(BR_WF_10004F50) == 0);
    CHECK(BrWndShadowGet(BR_SH_105CCB5C) == 1);

    /* the latch now SKIPS the four calls but NOT the tail */
    arm();
    BrWndShadowSet(BR_SH_105CCB5C, 1);
    (void)BrOnActivateApp(HWND_A, 0, 0);
    CHECK(BrWndFrontierHits(BR_WF_100609F0) == 0);
    CHECK(BrWndFrontierHits(BR_WF_1007296C) == 1);   /* tail still runs */
    CHECK(BrWndShadowGet(BR_SH_100A9354) == 1);
    CHECK(g_cInvalidate == 1);

    /* the 0x10226A48 arm, taken: no latch is set */
    arm();
    BrWndShadowSet(BR_SH_10226A48, 1);
    BrWndShadowSet(BR_SH_10226A44, 1);
    BrWndShadowSet(BR_SH_10AF21B0, 1);
    BrWndShadowSet(BR_SH_100BCBE8, 2);       /* 1 < 2, so the arm runs */
    (void)BrOnActivateApp(HWND_A, 0, 0);
    CHECK(BrWndFrontierHits(BR_WF_10004F50) == 1);
    CHECK(BrWndFrontierHits(BR_WF_10005330) == 1);
    CHECK(BrWndShadowGet(BR_SH_105CCB5C) == 0);   /* latch NOT set */

    /* `jge` skips it: equal is not less. */
    arm();
    BrWndShadowSet(BR_SH_10226A48, 1);
    BrWndShadowSet(BR_SH_10226A44, 1);
    BrWndShadowSet(BR_SH_10AF21B0, 2);
    BrWndShadowSet(BR_SH_100BCBE8, 2);
    (void)BrOnActivateApp(HWND_A, 0, 0);
    CHECK(BrWndFrontierHits(BR_WF_10004F50) == 0);
    CHECK(BrWndShadowGet(BR_SH_105CCB5C) == 1);
}

/* ==================================================================== *
 * 11. The activate half: the mode restore, and what a FAILED restore does.
 *
 * MUTATIONS KILLED:
 *   a) drop the !suspended condition          -> the minimised CHECK fails
 *   b) treat 0x1001DD80 != 0 as failure       -> the success CHECK fails
 *   c) drop the 0x100A9354 == 1 test          -> the "runs once" CHECK fails
 *   d) post something other than WM_CLOSE     -> the message-id CHECK fails
 * ==================================================================== */
static void test_activateapp_moderestore(void)
{
    arm();
    BrOnActivate(MKW(1, 0));                    /* active, not minimised */
    BrWndShadowSet(BR_SH_100A9354, 1);          /* a restore is pending */
    BrWndSetModeResult(0);                      /* 0x1001DD80 FAILS */

    (void)BrOnActivateApp(HWND_A, 1, 0);
    CHECK(BrWndFrontierHits(BR_WF_1001DD80) == 1);
    CHECK(g_cMsgBox  == 1);
    CHECK(g_cPostMsg == 1);
    CHECK(g_uLastPostMsg == BR_WM_CLOSE);
    CHECK(BrWndShadowGet(BR_SH_100A9354) == 2); /* marked done regardless */
    CHECK(BrWndFrontierHits(BR_WF_10019A40) == 1);

    /* and it does not run a second time, because 0x100A9354 is now 2 */
    (void)BrOnActivateApp(HWND_A, 1, 0);
    CHECK(BrWndFrontierHits(BR_WF_1001DD80) == 1);
    CHECK(g_cMsgBox == 1);
    CHECK(BrWndFrontierHits(BR_WF_10019A40) == 2);   /* the tail still runs */

    /* a SUCCESSFUL restore is silent */
    arm();
    BrOnActivate(MKW(1, 0));
    BrWndShadowSet(BR_SH_100A9354, 1);
    BrWndSetModeResult(1);
    (void)BrOnActivateApp(HWND_A, 1, 0);
    CHECK(BrWndFrontierHits(BR_WF_1001DD80) == 1);
    CHECK(g_cMsgBox  == 0);
    CHECK(g_cPostMsg == 0);
    CHECK(BrWndShadowGet(BR_SH_100A9354) == 2);

    /* MINIMISED: 0x10019409 tests 0x105BC748 and skips the whole restore. */
    arm();
    BrOnActivate(MKW(1, 1));                    /* active AND minimised */
    BrWndShadowSet(BR_SH_100A9354, 1);
    BrWndSetModeResult(0);
    (void)BrOnActivateApp(HWND_A, 1, 0);
    CHECK(BrWndFrontierHits(BR_WF_1001DD80) == 0);
    CHECK(g_cMsgBox == 0);
    CHECK(BrWndShadowGet(BR_SH_100A9354) == 1); /* NOT marked done */
    CHECK(BrWndFrontierHits(BR_WF_10019A40) == 1);  /* tail still runs */

    /* gate 1 clear: none of the activate half runs at all. */
    arm();
    BrMainLoopSetReady(0, 0);
    BrWndShadowSet(BR_SH_100A9354, 1);
    (void)BrOnActivateApp(HWND_A, 0, 0);
    CHECK(BrWndFrontierHits(BR_WF_10019A40) == 0);
}

/* ==================================================================== *
 * 12. THE ONE THAT PINS WHAT THIS FUNCTION IS NOT.
 *
 * 0x100194C0 handles no keyboard and no mouse message. Every one of these
 * reaches DefWindowProcA with no side effect. If a later pass "helpfully" adds
 * a key handler here -- which is exactly the mistake the commissioning brief
 * would have led to -- this test goes red and points at br_input.h's banner.
 * ==================================================================== */
static void test_no_keyboard_or_mouse(void)
{
    static const uint32_t auMsg[] = {
        0x0100, /* WM_KEYDOWN    */  0x0101, /* WM_KEYUP      */
        0x0102, /* WM_CHAR       */  0x0104, /* WM_SYSKEYDOWN */
        0x0105, /* WM_SYSKEYUP   */  0x0106, /* WM_SYSCHAR    */
        0x0200, /* WM_MOUSEMOVE  */  0x0201, /* WM_LBUTTONDOWN*/
        0x0202, /* WM_LBUTTONUP  */  0x0204, /* WM_RBUTTONDOWN*/
        0x020A, /* WM_MOUSEWHEEL */  0x0007, /* WM_SETFOCUS   */
        0x0008, /* WM_KILLFOCUS  */  0x000F, /* WM_PAINT      */
        0x0005, /* WM_SIZE       */  0x0003, /* WM_MOVE       */
        0x0010, /* WM_CLOSE      */  0x0021  /* WM_MOUSEACTIVATE */
    };
    size_t i;
    arm();
    for (i = 0; i < sizeof auMsg / sizeof auMsg[0]; i++) {
        BrWndResult r = BrWndProc(HWND_A, auMsg[i], 0x41, 0x1234);
        CHECK(r.fCallDefault == 1);
        CHECK(r.lResult == 0);
    }
    /* no gate moved, no handle stored, no platform call made */
    CHECK(g_brhWnd == NULL);
    CHECK(BrMainLoopReady1() == 0 && BrMainLoopReady2() == 0);
    CHECK(BrMainLoopSuspended() == 0);
    CHECK(g_cSetCursor == 0 && g_cPostQuit == 0 && g_cInvalidate == 0);
    for (i = 0; i < BR_WF_COUNT; i++)
        CHECK(BrWndFrontierHits((BrWndFrontierId)i) == 0);
}

/* ==================================================================== *
 * 13. WM_ACTIVATE reaches the gates THROUGH the window procedure, and still
 *     hands the message on to DefWindowProc.
 * ==================================================================== */
static void test_wm_activate_through_wndproc(void)
{
    BrWndResult r;
    arm();
    r = BrWndProc(HWND_A, BR_WM_ACTIVATE, MKW(1, 0), 0);
    CHECK(BrMainLoopFrameAllowed() == 1);
    CHECK(r.fCallDefault == 1);      /* 0x100195D9 */

    r = BrWndProc(HWND_A, BR_WM_ACTIVATE, MKW(1, 1), 0);
    CHECK(BrMainLoopFrameAllowed() == 0);
    CHECK(r.fCallDefault == 1);
}

int main(void)
{
    test_activate_gates();
    test_wm_create_stores_hwnd();
    test_argument_identity();
    test_syscommand();
    test_prehooks();
    test_devicechange();
    test_mcinotify();
    test_ear_message();
    test_wm_destroy();
    test_activateapp();
    test_activateapp_moderestore();
    test_no_keyboard_or_mouse();
    test_wm_activate_through_wndproc();

    if (g_fails != 0) { printf("%d FAILURE(S)\n", g_fails); return 1; }
    printf("br_input: all checks passed, 0 failures\n");
    return 0;
}
