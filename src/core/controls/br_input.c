/* br_input.c -- see br_input.h.
 *
 * ARCHITECTURAL CONCERN: input (window messages). 0x100194C0 and its three
 * message handlers, transcribed from BRGlide.dll.
 *
 * Every address in this file was read out of the disassembly. Nothing is
 * stood in for: the sixteen callees that are not transcribed are frontier
 * entries that do nothing and count, and the fifteen globals owned by modules
 * that do not exist yet are shadows with one re-point site each.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "br_input.h"
#include "br_window.h"
#include "br_mainloop.h"
#include "br_boot.h"        /* g_brAppModeW / g_brAppModeH == 0x100A7514/18 */

#include <stddef.h>
#include <stdio.h>

/* ------------------------------------------------------------------ *
 * Storage.
 * ------------------------------------------------------------------ */
BrWndPlatformOps g_brWndPlatform;
BrWndMsgHookA    g_pfnBrWndMsgHookA;
BrWndMsgHookB    g_pfnBrWndMsgHookB;

static int32_t s_aShadow[BR_SH_COUNT];
static int32_t s_aFrontierHits[BR_WF_COUNT];
static int32_t s_iModeResult;   /* 0x1001DD80's answer. 0 == failed. */

static const char *const s_apszFrontier[BR_WF_COUNT] = {
    "0x10002580 EAR create music channel",
    "0x10002AF0 music play track",
    "0x10002F70 music stop",
    "0x10002760 EAR shut channel down",
    "0x10002CF0 music next track",
    "0x10002830 MCI notify -> next track",
    "0x100325B0 WM_DESTROY teardown",
    "0x100609F0 deactivate 1/4",
    "0x10002EB0 deactivate 2/4",
    "0x1006BD70 deactivate 3/4",
    "0x10061440 deactivate 4/4",
    "0x10004F50 deactivate arm 1/2",
    "0x10005330 deactivate arm 2/2",
    "0x1007296C deactivate tail",
    "0x1001DD80 re-set video mode",
    "0x10019A40 activate tail"
};

int32_t BrWndShadowGet(BrWndShadow which)
{
    return (which >= 0 && which < BR_SH_COUNT) ? s_aShadow[which] : 0;
}

/* @n64 0x8021E998 located */
void BrWndShadowSet(BrWndShadow which, int32_t value)
{
    if (which >= 0 && which < BR_SH_COUNT)
        s_aShadow[which] = value;
}

int32_t BrWndFrontierHits(BrWndFrontierId id)
{
    return (id >= 0 && id < BR_WF_COUNT) ? s_aFrontierHits[id] : 0;
}

const char *BrWndFrontierName(BrWndFrontierId id)
{
    return (id >= 0 && id < BR_WF_COUNT) ? s_apszFrontier[id] : "";
}

void BrWndFrontierReport(void)
{
    int i, n = 0;
    for (i = 0; i < BR_WF_COUNT; i++)
        if (s_aFrontierHits[i] != 0) n++;
    if (n == 0) {
        printf("window-message frontier: nothing reached\n");
        return;
    }
    printf("window-message frontier -- reached but NOT transcribed:\n");
    for (i = 0; i < BR_WF_COUNT; i++)
        if (s_aFrontierHits[i] != 0)
            printf("    %-38s %6d\n", s_apszFrontier[i], (int)s_aFrontierHits[i]);
}

void BrWndSetModeResult(int32_t iResult) { s_iModeResult = iResult; }

static void frontier(BrWndFrontierId id) { ++s_aFrontierHits[id]; }

void BrInputResetForTest(void)
{
    int i;
    for (i = 0; i < BR_SH_COUNT;    i++) s_aShadow[i]       = 0;
    for (i = 0; i < BR_WF_COUNT;    i++) s_aFrontierHits[i] = 0;
    s_iModeResult      = 0;
    g_pfnBrWndMsgHookA = NULL;
    g_pfnBrWndMsgHookB = NULL;
}

/* A NULL-safe string fetch. 0x1006D280 returns NULL for an out-of-range id and
 * MessageBoxA accepts a NULL lpText, so a NULL here is faithful, not a hole. */
static const char *brstr(int32_t id)
{
    return (g_brWndPlatform.pfnString != NULL)
         ? g_brWndPlatform.pfnString(id) : NULL;
}

/* ================================================================== *
 * 0x10070370 -- WM_ACTIVATE. 45 bytes, __cdecl, one argument.
 *
 *   10070370  mov eax,[esp+4]              eax = wParam
 *   10070376  mov [0x105BC740], eax        gate 1 = the WHOLE wParam
 *   1007037B  and ecx, 0xFFFF              ecx = LOWORD  (WA_* state)
 *   10070381  shr eax, 0x10                eax = HIWORD  (fMinimized)
 *   10070386  mov [0x105BC744], ecx        gate 2 = LOWORD
 *   1007038C  mov [0x105BC748], eax        gate 3 = HIWORD -- the INVERTED one
 *   10070391  je  0x10070397               (flags from `test ecx,ecx`)
 *   10070393  test eax,eax
 *   10070395  je  0x1007039C               -> bare ret
 *   10070397  jmp 0x10008D60               -> also a bare ret, see below
 *
 * So gate 3, the one br_mainloop.c tests with the opposite polarity, is
 * WM_ACTIVATE's fMinimized. That is what makes "invert it and the game runs
 * only while minimised" a literal description rather than a figure of speech.
 *
 * The tail is dead weight and is transcribed as a comment rather than as code:
 * 0x10008D60 is ONE BYTE, `c3` -- a bare `ret` (CONVENTIONS.md, "Facts not to
 * re-derive"; re-checked against BRGlide.dll for this file). Both arms of the
 * two-way branch therefore do nothing whatsoever, and there is no side effect
 * to preserve. Note the shape though: CONVENTIONS.md's warning is that a call
 * REACHING 0x10008D60 says nothing about the CALLER's side effects. Here the
 * callee itself is the stub and the branch has no other body, which is a
 * different and much weaker claim -- it is safe.
 * ================================================================== */
/* WHAT IT DOES: notes whether the game window has just been given or lost the
 * keyboard focus, and whether it was minimised, so the main loop knows when to
 * keep running and when to sit still. It only records the state; the actual
 * pausing is done elsewhere. */
/* @implements 0x10070370 glide BrOnActivate */
#ifdef BR_MATCHING_BUILD
/* Matching build names the three gates and the 0x10008B80 stub directly so
 * MSVC emits the original's `mov [imm32]` stores and tail `jmp`. */
extern uint32_t g_brActivateGate1;   /* 0x10680598 / 0x105BC740 */
extern uint32_t g_brActivateGate2;   /* 0x1068059C / 0x105BC744 */
extern uint32_t g_brActivateGate3;   /* 0x106805A0 / 0x105BC748 */
extern void BrOnActivateTail(void);  /* 0x10008B80 / 0x10008D60, bare ret */

__declspec(dllimport) int32_t __stdcall GetWindowLongA(void *hWnd, int nIndex);
__declspec(dllimport) int32_t __stdcall DefWindowProcA(void *hWnd, uint32_t uMsg,
                                                       uint32_t wParam, int32_t lParam);
__declspec(dllimport) int32_t __stdcall InvalidateRect(void *hWnd, void *pRc, int fErase);
__declspec(dllimport) int32_t __stdcall MessageBoxA(void *hWnd, const char *pszText,
                                                    const char *pszCap, uint32_t uType);
__declspec(dllimport) int32_t __stdcall PostMessageA(void *hWnd, uint32_t uMsg,
                                                     uint32_t wParam, int32_t lParam);
__declspec(dllimport) void    __stdcall PostQuitMessage(int nExit);
__declspec(dllimport) void   *__stdcall SetCursor(void *hCursor);

void BrOnActivate(BrWParam wParam)
{
    uint32_t lo;
    uint32_t hi;

    lo = (uint32_t)wParam;
    g_brActivateGate1 = lo;
    lo &= 0xFFFFu;
    hi = (uint32_t)wParam >> 16;
    g_brActivateGate2 = lo;
    g_brActivateGate3 = hi;
    if (lo == 0u || hi != 0u)
        BrOnActivateTail();
}
#else
void BrOnActivate(BrWParam wParam)
{
    /* The original's registers are 32 bits wide. Masking reproduces
     * `shr eax,0x10` exactly on LP64, where a bare >>16 of a 64-bit WPARAM
     * would carry bits 32..47 down into the HIWORD. */
    uint32_t w  = (uint32_t)wParam;
    uint32_t lo = w & 0xFFFFu;
    uint32_t hi = w >> 16;

    /* 0x105BC740 and 0x105BC744, in the original's order. br_mainloop.c owns
     * this storage; see the aliasing note in br_input.h. */
    BrMainLoopSetReady((int)w, (int)lo);

    /* 0x105BC748 -- MINIMISED. */
    BrMainLoopSetSuspended((int)hi);

    /* 0x10070391..0x1007039C: both arms reach a bare `ret`. Nothing to do. */
}
#endif

/* ================================================================== *
 * 0x10019350 -- WM_ACTIVATEAPP. 294 bytes, __cdecl, three arguments.
 *
 * ESP TRACE, because the lParam read is late and unbracketed:
 *   entry                      esp = R;  hWnd R+4, wParam R+8, lParam R+0xC
 *   10019350 push esi          esp = R-4
 *   10019351 mov esi,[esp+8]   R-4 + 8    = R+4    -> hWnd
 *   10019355 push edi          esp = R-8
 *   10019356 mov edi,[esp+0x10]R-8 + 0x10 = R+8    -> wParam  (fActive)
 *   10019464 mov eax,[esp+0x14]R-8 + 0x14 = R+0xC  -> lParam
 * Every call in between is balanced, so esp is still R-8 at 0x10019464. Ends
 * in a bare `ret` and the call site does `add esp,0xC`: __cdecl.
 *
 * The shape: a DEACTIVATE block that runs only when fActive is 0, then a
 * RE-READ of the gate that runs the activate work. Both halves are in one
 * function and neither is an else of the other.
 * ================================================================== */
/* WHAT IT DOES: handles the player switching away from the game and back
 * again. On the way out it shuts the running subsystems down and marks the
 * display as needing to be rebuilt; on the way back in it re-applies the video
 * mode, and if that fails it apologises with a message box and asks the window
 * to close. Both halves live in one function and the second is not an else of
 * the first, so a single call can do both. */
#ifdef BR_MATCHING_BUILD
extern int32_t g_br105CCB5C;
extern int32_t g_br105CCB88;
extern int32_t g_br105BC8DC;
extern int32_t g_br10226A48;
extern int32_t g_br10226A44;
extern int32_t g_br10AF21B0;
extern int32_t g_br100BCBE8;
extern int32_t g_br100A9354;
extern int32_t g_brAppModeW;
extern int32_t g_brAppModeH;
extern void BrSub100609F0(void);
extern void BrSub10002EB0(void);
extern void BrSub1006BD70(void);
extern void BrSub10061440(void);
extern void BrSub10004F50(void);
extern void BrSub10005330(void);
extern void BrSub1007296C(void);
extern int32_t BrSetVideoMode(int32_t w, int32_t h);
extern char *BrStrId(int32_t id);
extern void BrSub10019A40(void);

/* WHAT IT DOES: handle the window losing or gaining focus -- alt-tab, or the
 * player clicking away. On losing focus it releases the things another
 * application must be allowed to take (input devices, sound, the 3dfx
 * hardware) and remembers that it did; on regaining it, it takes them back.
 * Getting this wrong is what leaves a full-screen game holding the display
 * after the player has switched away. */
/* @implements 0x10019350 glide BrOnActivateApp */
BrWndResult BrOnActivateApp(void *hWnd, BrWParam wParam, BrLParam lParam)
{
    g_brActivateGate1 = (uint32_t)wParam;
    if (wParam == 0) {
        if (g_br105CCB5C == 0 && g_br105CCB88 == 0) {
            BrSub100609F0();
            BrSub10002EB0();
            BrSub1006BD70();
            BrSub10061440();
            g_br105BC8DC = 0;
            if (g_br10226A48 != 0 && g_br10226A44 != 0 &&
                g_br105CCB88 == 0 && g_br10AF21B0 < g_br100BCBE8) {
                BrSub10004F50();
                BrSub10005330();
            } else {
                g_br105CCB5C = 1;
            }
        }
        BrOnActivateTail();
        BrSub1007296C();
        g_br100A9354 = 1;
        InvalidateRect(hWnd, 0, 0);
    }
    if (g_brActivateGate1 != 0) {
        if (g_br100A9354 == 1 && g_brActivateGate3 == 0) {
            if (BrSetVideoMode(g_brAppModeW, g_brAppModeH) == 0) {
                MessageBoxA(hWnd, BrStrId(0x129), 0, 0);
                PostMessageA(hWnd, 0x10, 0, 0);
            }
            g_br100A9354 = 2;
        }
        BrOnActivateTail();
        BrSub10019A40();
    }
    return DefWindowProcA(hWnd, 0x1C, (uint32_t)wParam, (int32_t)lParam);
}
#else
/* @implements 0x10019350 glide BrOnActivateApp */
BrWndResult BrOnActivateApp(void *hWnd, BrWParam wParam, BrLParam lParam)
{
    BrWndResult r;
    int fActive = (int)(uint32_t)wParam;

    (void)lParam;   /* only reaches DefWindowProcA, at 0x10019464 */

    /* 0x1001935C: [0x105BC740] = fActive. ONLY gate 1 -- 0x105BC744 is left
     * alone, so gate 2 is read back and written unchanged. Without the
     * read-modify-write, br_mainloop.c's two-in-one setter would silently
     * clear a gate this function never touches. */
    BrMainLoopSetReady(fActive, BrMainLoopReady2());

    if (fActive == 0) {                       /* 0x10019362 jne skips it all */
        /* 0x10019368 / 0x10019371: either latch set skips straight to the
         * tail at 0x100193D8 -- the four calls AND the 0x10226A48 arm are
         * both inside this guard. */
        if (BrWndShadowGet(BR_SH_105CCB5C) == 0 &&
            BrWndShadowGet(BR_SH_105CCB88) == 0) {
            frontier(BR_WF_100609F0);          /* 0x1001937A */
            frontier(BR_WF_10002EB0);          /* 0x1001937F */
            frontier(BR_WF_1006BD70);          /* 0x10019384 */
            frontier(BR_WF_10061440);          /* 0x10019389 */

            /* 0x1001938E loads 0x10226A48 BEFORE 0x10019393 clears
             * 0x105BC8DC. Different addresses, so the order is not
             * observable -- kept anyway, because "not observable" is a claim
             * about callees this port has not read. */
            BrWndShadowSet(BR_SH_105BC8DC, 0); /* 0x10019393 */

            if (BrWndShadowGet(BR_SH_10226A48) != 0 &&
                BrWndShadowGet(BR_SH_10226A44) != 0 &&
                BrWndShadowGet(BR_SH_105CCB88) == 0 &&
                BrWndShadowGet(BR_SH_10AF21B0) <
                BrWndShadowGet(BR_SH_100BCBE8)) {   /* 0x100193BE jge skips */
                frontier(BR_WF_10004F50);      /* 0x100193C2 */
                frontier(BR_WF_10005330);      /* 0x100193C7 */
            } else {
                BrWndShadowSet(BR_SH_105CCB5C, 1);  /* 0x100193CE */
            }
        }
        /* 0x100193D8: `call 0x10008D60` is the one-byte stub -- omitted. */
        frontier(BR_WF_1007296C);              /* 0x100193DD */
        BrWndShadowSet(BR_SH_100A9354, 1);     /* 0x100193E7 */
        if (g_brWndPlatform.pfnInvalidate != NULL)
            g_brWndPlatform.pfnInvalidate(hWnd);   /* 0x100193F1, (hWnd,0,0) */
    }

    /* 0x100193F7 -- the ORIGINAL RE-READS [0x105BC740] rather than reusing
     * edi. It cannot differ from fActive here (nothing between writes it), but
     * the re-read is transcribed as a re-read so that a future writer inserted
     * into the block above changes this test, exactly as it would in the
     * original. */
    if (BrMainLoopReady1()) {
        /* 0x10019400 and 0x10019410: mode-restore pending, and not minimised.
         * Both `jne` to the same tail, so they collapse to one condition. */
        if (BrWndShadowGet(BR_SH_100A9354) == 1 && !BrMainLoopSuspended()) {
            /* 0x10019412..0x10019425: push [0x100A7518] then [0x100A7514],
             * cdecl, so the arguments are (width, height) -- the LAST push is
             * the FIRST argument. Those two globals are g_brAppModeW/H, owned
             * by br_boot.c and reused here rather than redeclared. */
            frontier(BR_WF_1001DD80);
            if (s_iModeResult == 0) {          /* 0x10019428 test/jne */
                /* 0x1001942C..0x1001943D: MessageBoxA(hWnd, str(0x129),
                 * NULL, 0). The two `push eax` reuse the zero the test left
                 * in eax, which is why the caption and type are both NULL/0. */
                if (g_brWndPlatform.pfnMessageBox != NULL)
                    g_brWndPlatform.pfnMessageBox(hWnd, brstr(0x129), NULL, 0);
                /* 0x10019443: PostMessageA(hWnd, WM_CLOSE, 0, 0). */
                if (g_brWndPlatform.pfnPostMessage != NULL)
                    g_brWndPlatform.pfnPostMessage(hWnd, BR_WM_CLOSE, 0, 0);
            }
            BrWndShadowSet(BR_SH_100A9354, 2); /* 0x10019450 */
        }
        /* 0x1001945A: `call 0x10008D60` -- the stub again, omitted. */
        frontier(BR_WF_10019A40);              /* 0x1001945F */
    }

    /* 0x10019464: DefWindowProcA(hWnd, WM_ACTIVATEAPP, wParam, lParam) and
     * return ITS value -- on every path, including the one that just asked the
     * window to close. */
    r.fCallDefault = 1;
    r.lResult      = 0;
    return r;
}
#endif

/* ================================================================== *
 * 0x10019480 -- WM_SYSCOMMAND. 61 bytes, __cdecl, three arguments.
 *
 * ESP TRACE:
 *   entry                      esp = R;  hWnd R+4, wParam R+8, lParam R+0xC
 *   10019480 push esi          esp = R-4
 *   10019481 mov esi,[esp+8]   R-4 + 8    = R+4    -> hWnd
 *   10019485 push -0x15 / push esi / call GetWindowLongA   stdcall, pops both
 *   1001948E mov ecx,[esp+0xC] R-4 + 0xC  = R+8    -> wParam
 *   100194A5 mov eax,[esp+0x10]R-4 + 0x10 = R+0xC  -> lParam
 * Bare `ret`, caller does `add esp,0xC`: __cdecl.
 * ================================================================== */
/* WHAT IT DOES: refuses to let the player resize, move or maximise the game
 * window from the system menu, so the display stays the size the game set it
 * to; anything else on that menu is passed through to Windows untouched. It
 * also asks Windows for the window's user data and throws the answer away. */
#ifdef BR_MATCHING_BUILD
/* @implements 0x10019480 glide BrOnSysCommand */
BrWndResult BrOnSysCommand(void *hWnd, BrWParam wParam, BrLParam lParam)
{
    uint32_t sc;

    GetWindowLongA(hWnd, -21);
    sc = (uint32_t)wParam;
    switch (sc) {
    case 0xF000u:
    case 0xF010u:
    case 0xF030u:
        return 0;
    default:
        return DefWindowProcA(hWnd, 0x112, sc, (int32_t)lParam);
    }
}
#else
/* @implements 0x10019480 glide BrOnSysCommand */
BrWndResult BrOnSysCommand(void *hWnd, BrWParam wParam, BrLParam lParam)
{
    BrWndResult r;
    uint32_t sc = (uint32_t)wParam;

    (void)lParam;   /* only reaches DefWindowProcA, at 0x100194A5 */

    /* 0x10019488: GetWindowLongA(hWnd, GWL_USERDATA). ITS RESULT IS DISCARDED
     * -- eax is overwritten by `mov eax,ecx` at 0x10019492 before anything
     * reads it. Kept because a call is not nothing: CONVENTIONS.md's
     * 0x10066D70 note is that a dead return value says nothing about side
     * effects, and this port cannot see inside USER32. */
    if (g_brWndPlatform.pfnGetWindowLong != NULL)
        (void)g_brWndPlatform.pfnGetWindowLong(hWnd, BR_GWL_USERDATA);

    /* 0x10019494 / 0x1001949B / 0x100194A0 -- three chained subtractions,
     * against the EXACT values and with NO 0xFFF0 mask. See the enum's comment
     * in br_input.h: keyboard-initiated variants such as 0xF012 pass straight
     * through to DefWindowProcA. Preserved. */
    if (sc == BR_SC_SIZE || sc == BR_SC_MOVE || sc == BR_SC_MAXIMIZE) {
        r.fCallDefault = 0;      /* 0x100194B9 xor eax,eax / ret */
        r.lResult      = 0;
        return r;
    }

    r.fCallDefault = 1;          /* 0x100194B1 DefWindowProcA(hWnd,0x112,..) */
    r.lResult      = 0;
    return r;
}
#endif

/* ================================================================== *
 * 0x100194C0 -- THE WINDOW PROCEDURE. 423 bytes, __stdcall, four arguments.
 * See br_input.h for the prologue's ESP trace and for what this function is
 * and is not.
 * ================================================================== */
/* WHAT IT DOES: the game's window procedure -- everything Windows wants to
 * tell the game arrives here first. It offers each message to two optional
 * hooks, deals with music-player notifications and CD/device arrival and
 * removal, remembers the window handle when the window is created, quits when
 * it is destroyed, keeps the mouse pointer hidden, and hands focus changes to
 * the handlers above. Anything it does not recognise goes back to Windows for
 * the default treatment. */
#ifdef BR_MATCHING_BUILD
extern int32_t g_br10AC5C5C;
extern int32_t g_br10AC5C58;
extern int32_t g_br10AC408C;
extern int32_t g_brAudioMode;      /* 0x1007B074 */
extern int32_t g_brEarMsg;         /* 0x104B1620 */
extern int32_t g_br1021C770;
extern void   *g_brhWnd;
extern int32_t BrGet1021C788(void);
extern void __stdcall BrSub100590D0(int32_t, void *, uint32_t, uint32_t, int32_t);
extern void __stdcall BrSub10035A30(void *, uint32_t, uint32_t, int32_t);
extern void BrSub10002CF0(void);
extern void BrSub10002580(void);
extern void BrSub10002AF0(int32_t n);
extern void BrSub10002F70(void);
extern void BrSub10002760(void);
extern void BrSub100325B0(int32_t a);
extern void BrSub10002830(void);

/* RESIDUE (+17 bytes, +10 insns, 11+1 register-blind): ours emits the
 * DefWindowProcA tail THREE times where the original emits it twice. The
 * original places the shared copy immediately after case 2, which case 2
 * falls into and which the dispatch default and the 0x3B9 arm both jump
 * BACK to (`jne 0x100195ac`); ours puts default's own copy at the end and
 * reaches it with a `je` around it. DEAD PROBES, none of which moved the
 * placement: putting `default:` (with the label and the return on it)
 * textually between case 2 and case 6 so case 2 falls through, and leaving
 * `default: goto defwnd;` with the label after the switch. VC5 orders the
 * arms itself and places default last regardless of source position; this
 * is block layout, not source shape. */
/* @implements 0x100194C0 glide BrWndProc */
BrWndResult __stdcall BrWndProc(void *hWnd, uint32_t uMsg, BrWParam wParam, BrLParam lParam)
{
    int32_t iMode;
    int32_t *pHook = (int32_t *)g_br10AC5C5C;

    if (pHook != 0 && pHook[0x68 / 4] != 0) {
        BrSub100590D0(g_br10AC5C58, hWnd, uMsg, (uint32_t)wParam, (int32_t)lParam);
        if (g_br10AC408C != 0)
            BrSub10035A30(hWnd, uMsg, (uint32_t)wParam, (int32_t)lParam);
    }

    iMode = g_brAudioMode;
    if (iMode == 2) {
        if ((int32_t)uMsg == g_brEarMsg) {
            if (lParam == 2) {
                if ((int32_t)wParam == BrGet1021C788())
                    BrSub10002CF0();
            }
            return 0;
        }
        if (uMsg == 0x219u) {
            if (wParam == 0x8000u) {
                BrSub10002580();
                BrSub10002AF0(1);
            }
            if (wParam == 0x8001u || wParam == 0x8003u || wParam == 0x8004u) {
                BrSub10002F70();
                BrSub10002760();
            }
            return 1;
        }
    }

    switch (uMsg) {
    case 1:
        g_brhWnd = hWnd;
        return 0;
    case 2:
        BrSub100325B0(0);
        PostQuitMessage(0);
        goto defwnd;
    case 6:
        /* `goto defwnd`, NOT its own `return DefWindowProcA(...)`. VC5
         * tail-duplicates the shared block here either way, but written
         * inline the front end has already folded uMsg to the constant and
         * emits `push 6`; reached by the goto it copies the generic block
         * and emits `push esi`, which is what the original has. */
        BrOnActivate(wParam);
        goto defwnd;
    case 0x1C:
        return BrOnActivateApp(hWnd, wParam, lParam);
    case 0x20:
        SetCursor(0);
        return 1;
    case 0x112:
        return BrOnSysCommand(hWnd, wParam, lParam);
    case 0x3B9:
        if (iMode != 1)
            goto defwnd;
        if (lParam == (BrLParam)g_br1021C770 &&
            wParam == 1 &&
            g_br105CCB5C == 0)
            BrSub10002830();
        return 0;
    default:
        goto defwnd;
    }
defwnd:
    return DefWindowProcA(hWnd, uMsg, (uint32_t)wParam, (int32_t)lParam);
}
#else
/* @implements 0x100194C0 glide BrWndProc */
BrWndResult BrWndProc(void *hWnd, uint32_t uMsg, BrWParam wParam, BrLParam lParam)
{
    BrWndResult r;
    int32_t iMode;

    r.fCallDefault = 0;
    r.lResult      = 0;

    /* ---- 0x100194C0..0x10019504: the two pre-dispatch hooks ---------- *
     * Hook B is nested inside hook A's guard -- 0x100194FA's `je` lands at
     * 0x10019505, past both -- so [0x10AC408C] alone does not run it. */
    if (BrWndShadowGet(BR_SH_10AC5C5C) != 0 &&
        BrWndShadowGet(BR_SH_10AC5C5C_68) != 0) {
        if (g_pfnBrWndMsgHookA != NULL)
            g_pfnBrWndMsgHookA(BrWndShadowGet(BR_SH_10AC5C58),
                               hWnd, uMsg, wParam, lParam);   /* 0x100194EE */
        if (BrWndShadowGet(BR_SH_10AC408C) != 0) {
            if (g_pfnBrWndMsgHookB != NULL)
                g_pfnBrWndMsgHookB(hWnd, uMsg, wParam, lParam); /* 0x10019500 */
        }
    }

    /* 0x10019505: eax = [0x1007B074], and it STAYS in eax all the way to
     * 0x10019628, which is why the MM_MCINOTIFY arm can test it without
     * reloading. */
    iMode = BrWindowAudioBackend();

    /* ---- 0x1001950A: the mode-2 (EAR) arm --------------------------- */
    if (iMode == BR_AUDIO_EAR) {
        /* 0x1001950F. Compared against the RAW global, which is 0 until
         * 0x10017E1B registers the message -- so with no EAR DLL, uMsg 0
         * lands here. Preserved; see br_input.h. */
        if ((int32_t)uMsg == BrWndShadowGet(BR_SH_104B1620)) {
            /* 0x10019517 `cmp ebx,eax` with eax still 2: lParam == 2. */
            if (lParam == 2) {
                /* 0x1001951F: 0x100027A0 is six bytes,
                 * `mov eax,[0x1021C788]; ret` -- transcribed inline as the
                 * shadow read it is, not made a frontier entry. */
                if ((int32_t)(uint32_t)wParam == BrWndShadowGet(BR_SH_1021C788))
                    frontier(BR_WF_10002CF0);      /* 0x1001952C */
            }
            /* Every arm here returns 0: 0x10019519, 0x10019526 and
             * 0x10019531 all reach `xor eax,eax / ret 0x10`. */
            return r;
        }

        if (uMsg == BR_WM_DEVICECHANGE) {          /* 0x1001953A, 0x219 */
            if (wParam == BR_DBT_DEVICEARRIVAL) {  /* 0x10019542, 0x8000 */
                frontier(BR_WF_10002580);          /* 0x1001954A */
                frontier(BR_WF_10002AF0);          /* 0x10019551, arg 1 */
            }
            /* NOT an else: 0x10019559 is reached by fall-through as well as by
             * the `jne` at 0x10019548. 0x8000 therefore runs the arrival work
             * and then fails all three of these. */
            if (wParam == BR_DBT_DEVICEQUERYREMOVE ||
                wParam == BR_DBT_DEVICEREMOVEPENDING ||
                wParam == BR_DBT_DEVICEREMOVECOMPLETE) {
                frontier(BR_WF_10002F70);          /* 0x10019575 */
                frontier(BR_WF_10002760);          /* 0x1001957A */
            }
            /* Both tails -- 0x1001957F and 0x1001960C -- return 1. */
            r.lResult = 1;
            return r;
        }
        /* anything else in mode 2 falls through to the main chain */
    }

    /* ---- 0x1001958B: the main comparison chain ---------------------- */
    switch (uMsg) {
    case BR_WM_CREATE:              /* 1 -- 0x100195BD */
        /* THE ONLY GLOBAL THIS FUNCTION WRITES. 0x105BC72C is the handle the
         * main loop hands to ShowWindow/UpdateWindow/SetFocus; 0x10019670
         * throws CreateWindowExA's result away, so this is where it comes
         * from. Returning 0 lets creation proceed. */
        g_brhWnd = hWnd;
        return r;                   /* lResult 0, fCallDefault 0 */

    case BR_WM_DESTROY:             /* 2 -- 0x1001959A */
        frontier(BR_WF_100325B0);                       /* called with 0 */
        if (g_brWndPlatform.pfnPostQuit != NULL)
            g_brWndPlatform.pfnPostQuit(0);             /* 0x100195A6 */
        break;                      /* falls into DefWindowProcA at 0x100195AC */

    case BR_WM_ACTIVATE:            /* 6 -- 0x100195CC */
        BrOnActivate(wParam);       /* 0x10070370, cdecl, wParam only */
        break;                      /* then DefWindowProcA at 0x100195D9 */

    case BR_WM_ACTIVATEAPP:         /* 0x1C -- 0x100195F2 */
        /* 0x100195FD returns 0x10019350's eax UNCHANGED -- there is no
         * `mov eax,..` between the call and the epilogue. */
        return BrOnActivateApp(hWnd, wParam, lParam);

    case BR_WM_SETCURSOR:           /* 0x20 -- 0x10019604 */
        if (g_brWndPlatform.pfnSetCursorNone != NULL)
            g_brWndPlatform.pfnSetCursorNone();         /* SetCursor(NULL) */
        r.lResult = 1;              /* 0x1001960C -- TRUE stops further work */
        return r;

    case BR_WM_SYSCOMMAND:          /* 0x112 -- 0x10019655 */
        return BrOnSysCommand(hWnd, wParam, lParam);    /* 0x10019480 */

    case BR_MM_MCINOTIFY:           /* 0x3B9 -- 0x10019628 */
        /* 0x10019628 `cmp eax,1` on the mode still in eax. NOT mode 1 means
         * DefWindowProcA; mode 1 means return 0 whatever else happens. */
        if (iMode != BR_AUDIO_MCI)
            break;
        if (lParam == (BrLParam)BrWndShadowGet(BR_SH_1021C770) &&  /* 0x10019631 */
            wParam == 1 &&                                /* MCI_NOTIFY_SUCCESSFUL */
            BrWndShadowGet(BR_SH_105CCB5C) == 0)          /* 0x1001963E */
            frontier(BR_WF_10002830);                     /* 0x10019647 */
        return r;                   /* 0x1001964C xor eax,eax */

    default:
        break;                      /* 0x100195AC */
    }

    r.fCallDefault = 1;
    r.lResult      = 0;
    return r;
}
#endif

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
#include <windows.h>
#include <mmsystem.h>
/* WHAT IT DOES: seek a RIFF WAVE file to the start of its "data" chunk via mmioDescend. */
/* @implements 0x10070170 glide BrWaveSeekData */

int BrWaveSeekData(int *param_1,LPMMCKINFO param_2,MMCKINFO *param_3)

{
  mmioSeek((HMMIO)*param_1,param_3->dwDataOffset + 4,0);
  param_2->ckid = 0x61746164;
  mmioDescend((HMMIO)*param_1,param_2,param_3,0x10);
  return;
}

/* ------------------------------------------------------------------ */
/* 0x10059E70                                                         */
/* ------------------------------------------------------------------ */

unsigned int FUN_100706d0(unsigned char *, unsigned char *);

/* WHAT IT DOES: reads the live joystick/pad buttons into two analog
 * bytes plus a packed two-byte mask the rest of the input layer uses. */
/* @implements 0x10059E70 glide BrPadPackButtons */
void BrPadPackButtons(unsigned char *out)
{
    unsigned int flags;
    unsigned char a[4];
    unsigned char b[4];

    flags = FUN_100706d0(a, b);
    out[2] = a[0];
    out[3] = b[0];
    *(unsigned short *)out = 0;
    if ((flags & 0x10) != 0) {
        *(unsigned short *)out = 0x8400;
    }
    if ((flags & 4) != 0) {
        out[1] |= 0x88;
    }
    if ((flags & 1) != 0) {
        out[1] |= 2;
    }
    if ((flags & 2) != 0) {
        out[1] |= 1;
    }
    if ((flags & 8) != 0) {
        out[1] |= 0x40;
    }
    if ((flags & 0x100) != 0) {
        out[0] |= 8;
    }
    if ((flags & 0x200) != 0) {
        out[0] |= 2;
    }
    if ((flags & 0x400) != 0) {
        out[0] |= 4;
    }
    if ((flags & 0x8000) != 0) {
        out[1] |= 0x10;
    }
    if ((flags & 0x20) != 0) {
        out[0] |= 0x10;
    }
    if ((flags & 0x40) != 0) {
        out[0] |= 0x20;
    }
}

#endif /* BR_MATCHING_BUILD */
