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
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "br_input.h"
#include "br_window.h"
#include "br_mainloop.h"
#include "br_boot.h"        /* g_brAppModeW / g_brAppModeH == 0x100A7514/18 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

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

/* @n64 0x8026CE20 located */
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

extern int DAT_10073ae0;
extern int DAT_10078718;
extern int DAT_118ee9cc;
extern int DAT_118ee9d0;
extern int DAT_118eebd0;
extern int DAT_118eebf0;
extern int DAT_118eebf8;
extern int *DAT_118eee88;
extern int DAT_118eee94;
extern int DAT_118eeef0;
extern int g_brP680584;
extern int *g_pBrDik18ABDD0;
int BrSub100770C0();
typedef int (__stdcall *CC_std_4)();   /* COM method: this + arguments */
typedef int (__stdcall *CC_std_2)();   /* COM method: this + arguments */
typedef int (__stdcall *CC_std_3)();   /* COM method: this + arguments */
typedef int (__stdcall *CC_std_1)();   /* COM method: this + arguments */

/* WHAT IT DOES: bring the keyboard input system up, but only on the FIRST
 * caller -- later callers just increment the count. Clears the key-state and
 * key-mapping tables on that first call. BrDiKeyboardShutdown is the
 * matching release. */
/* @implements 0x100703D0 glide FUN_100703d0 */
int FUN_100703d0(void)

{
  int iVar1;
  int *puVar2;
  
  DAT_118eeef0 = DAT_118eeef0 + 1;
  if (DAT_118eeef0 == 1) {
    DAT_118eebf0 = 1;
    DAT_118ee9cc = 0;
    puVar2 = &DAT_118ee9d0;
    for (iVar1 = 0x80; iVar1 != 0; iVar1 = iVar1 + -1) {
      *puVar2 = 0;
      puVar2 = puVar2 + 1;
    }
    puVar2 = &DAT_118eebf8;
    for (iVar1 = 0x88; iVar1 != 0; iVar1 = iVar1 + -1) {
      *puVar2 = 0;
      puVar2 = puVar2 + 1;
    }
    DAT_118eee94 = 0;
    DAT_118eebd0 = 1;
    BrSub100770C0();
    iVar1 = (*(CC_std_4 *)(*(int *)(DAT_118eee88) + 12))(DAT_118eee88,&DAT_10078718,&g_pBrDik18ABDD0,0);
    if (iVar1 < 0) {
      return 0;
    }
    iVar1 = (*(CC_std_2 *)(*(int *)(g_pBrDik18ABDD0) + 44))(g_pBrDik18ABDD0,&DAT_10073ae0);
    if (iVar1 < 0) {
      return 0;
    }
    iVar1 = (*(CC_std_3 *)(*(int *)(g_pBrDik18ABDD0) + 52))(g_pBrDik18ABDD0,g_brP680584,6);
    if (iVar1 < 0) {
      return 0;
    }
    if (g_pBrDik18ABDD0 != (int *)0x0) {
      (*(CC_std_1 *)(*(int *)(g_pBrDik18ABDD0) + 28))(g_pBrDik18ABDD0);
    }
  }
  return 1;
}

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
extern int32_t g_br10AC5C5C;
extern int32_t g_br10AC5C58;
extern int32_t g_br10AC408C;
extern int32_t g_brAudioMode;      /* 0x1007B074 */
extern int32_t g_brEarMsg;         /* 0x104B1620 */
extern int32_t g_br1021C770;
extern void   *g_brhWnd;
extern int32_t BrGet1021C788(void);
extern int32_t __stdcall BrSub100590D0(int32_t, void *, uint32_t, uint32_t, int32_t);
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
/* WHAT IT DOES: the game window's message handler -- everything Windows
 * sends the game arrives here and is routed: input, focus changes, painting,
 * close. Gives a registered hook first refusal on each message before
 * handling it. */
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

    /* Case order and exits as the original lays them out.  WM_ACTIVATE
     * BREAKS to the shared DefWindowProcA call like WM_DESTROY and a
     * non-EAR 0x3B9: VC5 tail-duplicates that call for it AFTER constant
     * propagation, so the copy pushes uMsg's register, as the original's
     * does.  Returning DefWindowProcA from the case pushes the constant 6. */
    switch (uMsg) {
    case 1:
        g_brhWnd = hWnd;
        return 0;
    case 2:
        BrSub100325B0(0);
        PostQuitMessage(0);
        break;
    case 0x1C:
        return BrOnActivateApp(hWnd, wParam, lParam);
    case 0x20:
        SetCursor(0);
        return 1;
    case 0x112:
        return BrOnSysCommand(hWnd, wParam, lParam);
    case 0x3B9:
        if (iMode == 1) {
            if (lParam == (BrLParam)g_br1021C770 &&
                wParam == 1 &&
                g_br105CCB5C == 0)
                BrSub10002830();
            return 0;
        }
        break;
    case 6:
        BrOnActivate(wParam);
        break;
    }
defwnd:
    return DefWindowProcA(hWnd, uMsg, (uint32_t)wParam, (int32_t)lParam);
}

/* ââ Ghidra-matched functions âââââââââââââââââââââââââââ */
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

extern int * DAT_10ac66e8;
extern int * DAT_10ac6720;
extern int * DAT_10ac6730;

/* WHAT IT DOES: update the button-latch state: detect new presses by comparing current vs previous frame. */
/* @implements 0x10059060 glide BrInputLatchUpdate */

int BrInputLatchUpdate(void)

{
  int iVar1;
  
  iVar1 = 0;
  do {
    *(unsigned int *)((int)&DAT_10ac6730 + iVar1) = (unsigned int)(*(int *)((int)&DAT_10ac66e8 + iVar1) == 0);
    *(unsigned int *)((int)&DAT_10ac66e8 + iVar1) = *(unsigned int *)((int)&DAT_10ac6720 + iVar1);
    *(unsigned int *)((int)&DAT_10ac6730 + iVar1) =
         *(unsigned int *)((int)&DAT_10ac6730 + iVar1) & *(unsigned int *)((int)&DAT_10ac6720 + iVar1);
    iVar1 = iVar1 + 4;
  } while (iVar1 < 0x10);
  return;
}


/* 0x10AC61E0 -- the DirectInput record 0x10059350.cpp builds; its device
 * pointer sits at +0x50.  Only the two vtable slots used here are named. */
struct BrDIPollDev;

typedef struct BrDIPollVtbl {
    void *pad00[7];                                     /* +0x00..+0x18 */
    int(__stdcall *Acquire)(struct BrDIPollDev *);      /* +0x1C */
    void *pad20;                                        /* +0x20 */
    int(__stdcall *GetDeviceState)(struct BrDIPollDev *,
                                   unsigned int, void *);   /* +0x24 */
} BrDIPollVtbl;

typedef struct BrDIPollDev {
    BrDIPollVtbl *lpVtbl;
} BrDIPollDev;

typedef struct BrDIPollRec {
    char          pad00[0x50];
    BrDIPollDev  *pDev;                                 /* +0x50 */
} BrDIPollRec;

extern BrDIPollRec *DAT_10ac61e0;

/* WHAT IT DOES: read the four action buttons straight off the DirectInput
 * device and report which one has just been pressed.  Clears the four
 * current-state words, asks the device for its 16-byte state block,
 * re-acquires it if it has been lost, sets a word for each button whose
 * high bit is down, lets 0x10059060 turn those into new-press flags, and
 * returns the index of the first button that flagged.  -1 when none did,
 * and also when there is no device at all. */
/* @implements 0x100705F0 glide BrInputPollPressed */
int BrInputPollPressed(void)
{
    /* Hand-transcribed from the asm.  The state block is a DIMOUSESTATE:
     * the buttons are a byte array at +12, which VC5 reads as one dword and
     * tests through al/ah.  The four-word clear is a memset (its zero stays
     * local; four `= 0` stores make VC5 keep 0 in esi for the whole body),
     * and the first-press scan walks an address compared SIGNED (`jl`). */
    struct {
        long          lX, lY, lZ;
        unsigned char rgbButtons[4];      /* +0x0C */
    } st;
    BrDIPollDev *pDev;
    int hr;
    int i;
    int *p;

    if (DAT_10ac61e0 == 0 || DAT_10ac61e0->pDev == 0)
        return -1;

    memset(&DAT_10ac6720, 0, 16);

    /* Re-read the device: the clear may alias it as far as the compiler
     * knows, so this is a second load, not the tested one. */
    pDev = DAT_10ac61e0->pDev;
    hr = pDev->lpVtbl->GetDeviceState(pDev, 0x10, &st);
    /* 0x8007001E only -- any other failure is left alone. */
    if (hr != 0 && hr == 0x8007001E) {
        pDev = DAT_10ac61e0->pDev;
        pDev->lpVtbl->Acquire(pDev);
    }

    if (st.rgbButtons[0] & 0x80)
        ((int *)&DAT_10ac6720)[0] = 1;
    if (st.rgbButtons[1] & 0x80)
        ((int *)&DAT_10ac6720)[1] = 1;
    if (st.rgbButtons[2] & 0x80)
        ((int *)&DAT_10ac6720)[2] = 1;
    if (st.rgbButtons[3] & 0x80)
        ((int *)&DAT_10ac6720)[3] = 1;

    BrInputLatchUpdate();

    i = 0;
    for (p = (int *)&DAT_10ac6730;
         (ptrdiff_t)p < (ptrdiff_t)((int *)&DAT_10ac6730 + 4);
         p++, i++) {
        if (*p != 0)
            return i;
    }
    return -1;
}

/* ================================================================== *
 * 0x100590D0 -- hook A, the first refusal BrWndProc offers every message.
 * __stdcall, five arguments (`ret 0x14`); the first is never read.
 * ================================================================== */
extern int32_t       DAT_10ac5b9c;
extern unsigned char DAT_10ac66b0;
extern unsigned char DAT_10ac66b3;
extern unsigned char DAT_10ac66b5;
extern unsigned char DAT_10ac66b8;
extern int32_t g_BrAA33E0;    /* 0x10AC6740 */
extern int32_t g_brAA33E4;    /* 0x10AC6744 */
extern int32_t g_pBrAC61E0;   /* 0x10AC61E0 */
/* 0x100597C0 is a 2-arg thiscall: `this` in ecx is the device pointer held
 * at 0x10AC61E0, plus one unread stack dword (`ret 4`).  BR_THISCALL1
 * (= __fastcall) would pass that dword in edx; a struct is never
 * register-eligible, so it is forced back onto the stack -- same arm as the
 * definition in br_dik.c. */
#include "br_match.h"
typedef struct { void *v; } BrSub10060750Arg;
extern void BR_THISCALL1 BrSub10060750(int32_t pDev, BrSub10060750Arg unused);

__declspec(dllimport) int32_t __stdcall IsWindow(void *hWnd);
__declspec(dllimport) void   *__stdcall GetActiveWindow(void);
__declspec(dllimport) int32_t __stdcall IsIconic(void *hWnd);

/* WHAT IT DOES: the menu system's look at each window message before the
 * game's own handler. Keystrokes (WM_CHAR) are recorded as the pending menu
 * key unless a modifier byte is held or the keyboard is locked out; the one
 * menu command it knows (0x9C41) becomes key 0x1B; entering a menu or a
 * size/move loop marks the game as paused; leaving one asks whether the
 * window is still active and un-minimised and posts WM_USER to itself, and
 * WM_USER / WM_ACTIVATE / WM_SYSCOMMAND re-run the layout refresh when there
 * is a live screen. Returns 1 except after WM_SYSCOMMAND, where it returns
 * what DefWindowProcA said. */
/* PARKED T2, 123/123 insns, REGNORM 0+0, RAW 8+8 (2026-09-09): scratch
 * registers only. The original loads 0x10AC61E0 into ecx at all three test
 * sites (ours: eax, whose short `a1` encoding is the whole 2-byte size gap)
 * and pushes WM_SYSCOMMAND's lParam/wParam from ecx/edx (ours: edx/eax); in
 * the WM_CHAR arm it stores wParam through eax before the `mov eax,edi`
 * return move (ours: ecx, with the move hoisted). Facts that DID land: the
 * result is one local, 1 at the top, assigned by DefWindowProcA in the
 * WM_SYSCOMMAND arm and returned ONCE after the switch (per-arm `return r`
 * lets the front end fold it to `mov eax,1`); the refresh callee 0x100597C0
 * is a 2-arg thiscall (`this` in ecx = the 0x10AC61E0 pointer, one unread
 * stack dword, `ret 4` so no `add esp,4` at the call sites).  The original's
 * ecx load at all three test sites is the argument, not colouring: an eax
 * load there leaves ecx dead at the call and the callee faults on a null
 * `this` (2026-09-21, invalid page fault at 0x100597C0 on Win98 hardware).
 * DEAD (12 compiles): `register` on r; `g = r` for the two 1-stores; uMsg
 * instead of the 0x112 literal in the DefWindowProcA call; positive
 * `if (g != 0) call` arms; a named `b = IsWindow()` local; a named local
 * for the 0x10AC61E0 load at all three sites; the global typed as a pointer;
 * a hWnd local; per-arm returns with `register` (worse, +29 B); placement at
 * the top of the TU and at its end. */
/* @t4-pass 0x100590D0 1 2026-09-09 probes 12 bytes 417 insns 123 regions 0 rows 8 census no  (hand) */
/* @t4-pass 0x100590D0 2 2026-09-09 probes 10 bytes 417 insns 123 regions 5 rows 0 census no  (hand, fn.py variants: literal/comparison/cast spellings across the case arms, all inert or worse) */
/* @t4-pass 0x100590D0 3 2026-09-09 probes 10 bytes 417 insns 123 regions 5 rows 0 census yes  (hand, fn.py variants: switch/decl/guard forms, all inert; corpus MISS at +0x8 len 10) */
/* @t3 0x100590D0 2026-09-09 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 420/419 insns 123/123 rows 0+0 regions 5 oracle UNCLASSIFIED
 * @t3-effort passes 3 zero-movement 2 3
 * RE-OPENED 2026-09-21: the 2026-09-09 cert mis-read the 0x100597C0 callee as
 * __stdcall and its ecx call-site loads as colouring; that "residue" was a
 * live ABI argument and crashed on hardware.  Fixed (thiscall via
 * BR_THISCALL1, `this` = g_pBrAC61E0).  Residue now: two swapped scratch
 * registers at the [esp+0x10] reloads (rows 0+0 register-blind), +1 B.
 * Do not reopen before the end-grind (project rule 12). */
/* @implements 0x100590D0 glide BrSub100590D0 */
int32_t __stdcall BrSub100590D0(int32_t iArg, void *hWnd, uint32_t uMsg,
                                uint32_t wParam, int32_t lParam)
{
    int32_t r;
    int32_t b;

    r = 1;
    switch (uMsg) {
    case 6:
        g_BrAA33E0 = (wParam == 0);
        goto user;
    case 0x102:
        if (DAT_10ac5b9c != 0)
            break;
        if (DAT_10ac66b0 & 0x80)
            break;
        if (DAT_10ac66b8 & 0x80)
            break;
        if (DAT_10ac66b3 & 0x80)
            break;
        if (DAT_10ac66b5 & 0x80)
            break;
        g_brAA33E4 = wParam;
        break;
    case 0x111:
        if ((uint16_t)wParam != 0x9C41u)
            break;
        g_brAA33E4 = 0x1B;
        break;
    case 0x112:
        r = DefWindowProcA(hWnd, 0x112, wParam, lParam);
        if (IsWindow(hWnd) == 0)
            break;
        if (g_pBrAC61E0 == 0)
            break;
        BrSub10060750(g_pBrAC61E0, *(BrSub10060750Arg *)&hWnd);
        break;
    case 0x211:
    case 0x231:
        g_BrAA33E0 = 1;
        if (g_pBrAC61E0 == 0)
            break;
        BrSub10060750(g_pBrAC61E0, *(BrSub10060750Arg *)&hWnd);
        break;
    case 0x212:
    case 0x232:
        if (GetActiveWindow() == hWnd) {
            b = IsIconic(hWnd);
            g_BrAA33E0 = 0;
            if (b == 0)
                goto post;
        }
        g_BrAA33E0 = 1;
    post:
        PostMessageA(hWnd, 0x400, 0, 0);
        break;
    case 0x400:
    user:
        if (g_pBrAC61E0 == 0)
            break;
        BrSub10060750(g_pBrAC61E0, *(BrSub10060750Arg *)&hWnd);
        break;
    default:
        break;
    }
    return r;
}


extern int *DAT_118eeeec;

/* WHAT IT DOES: take exclusive control of the input device back from Windows
 * -- what has to happen after the game regains focus before it can read the
 * device again. Reports whether it succeeded, and false if there is no
 * device. */
/* @implements 0x100706B0 glide BrDiAcquire */
int BrDiAcquire(void)

{
  int iVar1;
  
  if (DAT_118eeeec != (int *)0x0) {
    iVar1 = (*(CC_std_1 *)(*(int *)(DAT_118eeeec) + 28))(DAT_118eeeec);
    return (iVar1 >= 0);
  }
  return 0;
}

/* Hand-matched from disassembly â 0x100592F0
 * fastcall: pointer in ecx, ten fields zeroed in source order, returns this. */

/* WHAT IT DOES: the constructor of the 0x54-byte DirectInput object that
 * Ctl71FC0::Activate (0x10071FC0) news on first use: zeroes +0x2C..+0x50
 * (the +0x50 store first) and leaves the first 0x2C bytes as they are.  A
 * C++ constructor, written as __fastcall (this in ecx). */
/* @implements 0x100592F0 glide FUN_100592f0 */
int *__fastcall FUN_100592f0(int *p)
{
  p[20] = 0;   /* 0x50 */
  p[11] = 0;   /* 0x2c */
  p[12] = 0;   /* 0x30 */
  p[13] = 0;   /* 0x34 */
  p[14] = 0;   /* 0x38 */
  p[15] = 0;   /* 0x3c */
  p[16] = 0;   /* 0x40 */
  p[17] = 0;   /* 0x44 */
  p[18] = 0;   /* 0x48 */
  p[19] = 0;   /* 0x4c */
  return p;
}
