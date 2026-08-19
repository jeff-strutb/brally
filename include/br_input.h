/* br_input.h -- 0x100194C0, THE WINDOW PROCEDURE, and the three handlers it
 * delegates to: 0x10070370 (WM_ACTIVATE), 0x10019350 (WM_ACTIVATEAPP) and
 * 0x10019480 (WM_SYSCOMMAND).
 *
 * ARCHITECTURAL CONCERN: input -- specifically, WINDOW-MESSAGE input. The
 * distinction is not pedantry here; see the next section, which corrects the
 * premise this module was commissioned under.
 *
 * ========================================================================
 * THIS IS NOT WHERE KEYBOARD AND MOUSE INPUT ENTERS THE GAME
 * ========================================================================
 *
 * The brief for this module said 0x100194C0 was "where all keyboard and mouse
 * input enters the game". It is not, and the disassembly is unambiguous: the
 * function's entire message set is
 *
 *   0x0001  WM_CREATE
 *   0x0002  WM_DESTROY
 *   0x0006  WM_ACTIVATE
 *   0x001C  WM_ACTIVATEAPP
 *   0x0020  WM_SETCURSOR
 *   0x0112  WM_SYSCOMMAND
 *   0x0219  WM_DEVICECHANGE            (only when [0x1007B074] == 2)
 *   0x03B9  MM_MCINOTIFY               (only when [0x1007B074] == 1)
 *   [0x104B1620]  a registered message (only when [0x1007B074] == 2)
 *
 * and everything else -- including every one of WM_KEYDOWN 0x100, WM_KEYUP
 * 0x101, WM_CHAR 0x102, WM_SYSKEYDOWN 0x104, WM_MOUSEMOVE 0x200,
 * WM_LBUTTONDOWN 0x201 and their siblings -- reaches DefWindowProcA at
 * 0x100195AC untouched. There is no comparison against any of those values
 * anywhere in the 423 bytes.
 *
 * Corroborated from the import table rather than only from the absence of a
 * test: BRGlide.dll statically imports DINPUT.dll, references
 * "DirectInputCreateA", and contains exactly one GetAsyncKeyState reference
 * and zero of GetKeyState / GetKeyboardState / GetCursorPos. The game reads
 * its controls by POLLING DirectInput, not from the message queue. So the
 * host-side key path in port/src/gfx/metal/br_gfx_metal.m is not competing
 * with a game-side message handler that this file was supposed to supply --
 * the thing it is standing in for is a DirectInput poll that nobody has
 * transcribed yet, and it is somewhere else entirely.
 *
 * What 0x100194C0 IS, then, is the window's LIFECYCLE and FOCUS handler, and
 * that is genuinely valuable: it is what owns the main loop's three gates, and
 * it is the only writer of the HWND the main loop needs.
 *
 * ========================================================================
 * THE ESP TRACE OF THE PROLOGUE -- read this before touching the arguments
 * ========================================================================
 *
 * Let R = esp on entry, so the four arguments sit at R+4, R+8, R+0xC, R+0x10.
 *
 *   100194C5  push ebx                  esp = R-4
 *   100194C6  mov ebx,[esp+0x14]        R-4  + 0x14 = R+0x10  ->  lParam
 *   100194CA  push ebp                  esp = R-8
 *   100194CB  mov ebp,[esp+0x0C]        R-8  + 0x0C = R+0x04  ->  hWnd
 *   100194CF  push esi                  esp = R-0xC
 *   100194D0  mov esi,[esp+0x14]        R-0xC+ 0x14 = R+0x08  ->  uMsg
 *   100194D4  push edi                  esp = R-0x10
 *   100194D5  mov edi,[esp+0x1C]        R-0x10+0x1C= R+0x0C  ->  wParam
 *
 * TWO OF THOSE FOUR READS USE THE SAME DISPLACEMENT, 0x14, AND THEY ARE
 * DIFFERENT ARGUMENTS -- lParam and uMsg. One `push ebp` and one `push esi`
 * separate them. Reading 0x14 as one slot gives a procedure that switches on
 * lParam, which would look plausible and be wholly wrong. This is the failure
 * CONVENTIONS.md records twice, in the wheel ground probe and in the font
 * emitter, and it is present in the very first eight instructions here.
 *
 * So: ebx = lParam, ebp = hWnd, esi = uMsg, edi = wParam.
 *
 * CALLING CONVENTION, confirmed from the epilogue and not assumed: every one
 * of the eight exits is `ret 0x10`. Four dword arguments, callee-cleaned --
 * __stdcall, which is what a WNDPROC must be. The three delegates are
 * different: 0x10019350, 0x10019480 and 0x10070370 all end in a BARE `ret` and
 * their callers all do `add esp,N`, so those three are __cdecl.
 *
 * ========================================================================
 * THE GLOBALS THIS MODULE OWNS, AND THE ONE THE BRIEF GOT WRONG
 * ========================================================================
 *
 * 0x100194C0 writes EXACTLY ONE global, at 0x100195BD:
 *
 *     100195BD  mov dword ptr [0x105BC72C], ebp     ; hWnd, on WM_CREATE
 *
 * It does NOT write 0x105BC740, 0x105BC744 or 0x105BC748. The brief said it
 * did. Those three -- the main loop's gates -- are written by 0x10070370, the
 * WM_ACTIVATE handler, which is a direct callee:
 *
 *     10070376  mov [0x105BC740], eax        ; eax = wParam, raw
 *     10070386  mov [0x105BC744], ecx        ; ecx = wParam & 0xFFFF
 *     1007038C  mov [0x105BC748], eax        ; eax = wParam >> 16
 *
 * and 0x105BC740 is ALSO written by 0x10019350, the WM_ACTIVATEAPP handler:
 *
 *     1001935C  mov [0x105BC740], edi        ; edi = fActive
 *
 * That pins the semantics of all three, which br_mainloop.h could only
 * describe as "ready", "ready" and "suspended":
 *
 *   0x105BC740  ACTIVE. Written from WM_ACTIVATE's whole wParam and from
 *               WM_ACTIVATEAPP's fActive. The two handlers share the slot;
 *               whichever fires last wins. Preserved, not harmonised.
 *   0x105BC744  the LOWORD of WM_ACTIVATE's wParam -- WA_INACTIVE(0),
 *               WA_ACTIVE(1), WA_CLICKACTIVE(2).
 *   0x105BC748  the HIWORD of WM_ACTIVATE's wParam -- fMinimized.
 *
 * So the inverted third gate is literally MINIMISED, and br_mainloop.h's note
 * that getting its polarity backwards "gives a game that runs only while
 * minimised" is not an analogy. It is the mechanism.
 *
 * NO STORAGE FOR THE GATES IS DECLARED HERE. br_mainloop.c already owns those
 * three as statics behind BrMainLoopSetReady / BrMainLoopSetSuspended, and a
 * second definition under a second name is exactly the link-clean aliasing bug
 * CONVENTIONS.md documents. This module drives the existing accessors. One
 * consequence worth stating: the setters normalise to 0/1 while the original
 * stores the raw value. Every read of all three, in the main loop at
 * 0x100197C7/D0/D9 and in 0x10019350 at 0x100193F7 and 0x10019409, is a
 * test-against-zero, so the normalisation is not observable.
 *
 * WM_ACTIVATEAPP sets only 0x105BC740, so BrOnActivateApp reads 0x105BC744
 * back and writes it unchanged -- see the comment at that line.
 *
 * ========================================================================
 * THE THREE MODE-GATED MESSAGES
 * ========================================================================
 *
 * [0x1007B074] is the music backend (br_window.h establishes this from the
 * data). Three of the nine messages exist only for one backend:
 *
 *   mode 2 (EAR)  [0x104B1620], the message
 *                 RegisterWindowMessageA("EAR Interactive Around-Sound")
 *                 returned at 0x10017E1B. lParam == 2 and wParam ==
 *                 [0x1021C788] runs 0x10002CF0, the track advance.
 *   mode 2 (EAR)  WM_DEVICECHANGE 0x219. DBT_DEVICEARRIVAL 0x8000 re-creates
 *                 the EAR channel; 0x8001/0x8003/0x8004 (QUERYREMOVE,
 *                 REMOVEPENDING, REMOVECOMPLETE) tear it down.
 *   mode 1 (MCI)  MM_MCINOTIFY 0x3B9, whose lParam is the MCI device id at
 *                 0x1021C770 and whose wParam 1 is MCI_NOTIFY_SUCCESSFUL --
 *                 i.e. the CD track finished, so play the next one.
 *
 * A DETAIL THAT LOOKS LIKE A BUG AND IS KEPT: in mode 2 the registered-message
 * comparison at 0x1001950F runs BEFORE any other test and against the raw
 * global. If the EAR DLL never loaded, [0x104B1620] is still 0, and a uMsg of
 * 0 (WM_NULL) then takes the EAR arm instead of falling through to
 * DefWindowProc. The original does this; so does this transcription.
 *
 * ANOTHER, at 0x10019517: `cmp ebx, eax` where eax still holds [0x1007B074],
 * which the branch that got here has already established is 2. The compiler
 * reused the register; the source said `lParam == 2`.
 */
#ifndef BR_INPUT_H
#define BR_INPUT_H

#include <stdint.h>

/* ------------------------------------------------------------------ *
 * Portable stand-ins for the Win32 message types.
 *
 * LP64: WPARAM and LPARAM are pointer-sized and an HWND is a pointer. None of
 * the three may be an int32_t here. Where the original's 32-bit registers
 * truncate -- the HIWORD/LOWORD split of WM_ACTIVATE's wParam -- the
 * transcription masks to 32 bits first and says so at the site.
 * ------------------------------------------------------------------ */
typedef uintptr_t BrWParam;
typedef intptr_t  BrLParam;
typedef intptr_t  BrLResult;

/* The nine messages, plus the two this module SENDS. */
enum {
    BR_WM_CREATE       = 0x0001,
    BR_WM_DESTROY      = 0x0002,
    BR_WM_CLOSE        = 0x0010,  /* posted by 0x10019449, not handled here */
    BR_WM_ACTIVATE     = 0x0006,
    BR_WM_ACTIVATEAPP  = 0x001C,
    BR_WM_SETCURSOR    = 0x0020,
    BR_WM_SYSCOMMAND   = 0x0112,
    BR_WM_DEVICECHANGE = 0x0219,
    BR_MM_MCINOTIFY    = 0x03B9
};

/* WM_DEVICECHANGE wParam values 0x10019542..0x1001956F tests, in the order
 * the original tests them. */
enum {
    BR_DBT_DEVICEARRIVAL       = 0x8000,
    BR_DBT_DEVICEQUERYREMOVE   = 0x8001,
    BR_DBT_DEVICEREMOVEPENDING = 0x8003,
    BR_DBT_DEVICEREMOVECOMPLETE= 0x8004
};

/* The three WM_SYSCOMMAND commands 0x10019480 swallows.
 *
 * PRESERVED, and this is a real quirk rather than a tidy-up opportunity: Win32
 * puts the low four bits of an SC_ command to its own use, so correct code
 * masks with 0xFFF0 before comparing. 0x10019480 does not mask -- it is three
 * chained `sub`s against the exact values at 0x10019494, 0x1001949B and
 * 0x100194A0. A keyboard-initiated SC_MOVE arrives as 0xF012 and is therefore
 * NOT blocked. Establish this from the instructions, not from the constants. */
enum {
    BR_SC_SIZE     = 0xF000,
    BR_SC_MOVE     = 0xF010,
    BR_SC_MAXIMIZE = 0xF030
};

/* GetWindowLongA's index at 0x10019485: -21 == GWL_USERDATA. */
#define BR_GWL_USERDATA  (-21)

/* MessageBoxA's uType at 0x10017E6B and 0x10017EBC: MB_ICONHAND. */
#define BR_MB_ICONERROR  0x10

/* ------------------------------------------------------------------ *
 * What a window procedure returns.
 *
 * Four of the nine arms end by calling DefWindowProcA and returning ITS value,
 * and two of those four do real work first (WM_DESTROY posts the quit message,
 * WM_ACTIVATE runs the gate update). So "handled" and "default" are not
 * exclusive and a single BrLResult cannot express the function. This pair can.
 * ------------------------------------------------------------------ */
typedef struct BrWndResult {
    int       fCallDefault;  /* non-zero: caller must call DefWindowProc with
                              * the SAME four arguments and return its result */
    BrLResult lResult;       /* the value to return when fCallDefault == 0 */
} BrWndResult;

/* ------------------------------------------------------------------ *
 * The platform calls the window procedure makes. A host fills these in; a
 * test fills them with recorders. NULL is safe -- the call is skipped and the
 * frontier counter still records that the original would have made it.
 * ------------------------------------------------------------------ */
typedef struct BrWndPlatformOps {
    void (*pfnPostQuit)   (int32_t nExitCode);                 /* 0x100195A6 */
    void (*pfnSetCursorNone)(void);                            /* 0x10019606 */
    void (*pfnInvalidate) (void *hWnd);                        /* 0x100193F1 */
    void (*pfnMessageBox) (void *hWnd, const char *pszText,
                           const char *pszCaption, uint32_t uType);
    void (*pfnPostMessage)(void *hWnd, uint32_t uMsg,
                           BrWParam wParam, BrLParam lParam);  /* 0x1001944A */
    int32_t (*pfnGetWindowLong)(void *hWnd, int32_t iIndex);   /* 0x10019488 */
    void (*pfnExit)       (int32_t nCode);                     /* 0x10017E8E */
    /* 0x1006D280 -- the string table. `if (id < 1 || id >= 0x12F) return 0;
     * return [0x1186C488 + 4*id];` The 0x12F bound is the instruction's own
     * immediate, so the extent comes from the code and not from a guess. */
    const char *(*pfnString)(int32_t id);
} BrWndPlatformOps;

extern BrWndPlatformOps g_brWndPlatform;

/* ------------------------------------------------------------------ *
 * Globals this module READS that belong to modules nobody has written yet.
 *
 * SHADOWED, NOT OWNED, and that is deliberate: 0x10AC5C5C alone has 263
 * references in BRGlide.dll and 0x105CCB88 has 59. Declaring storage for them
 * here would make this module the de-facto owner of half the renderer's and
 * the music module's state, and the second definition would race the real one
 * the moment it lands. Each accessor is the ONE place to re-point.
 *
 * Keyed by ADDRESS, because that is the only key that survives renaming --
 * CONVENTIONS.md, "Aliased storage: a link-clean bug".
 * ------------------------------------------------------------------ */
typedef enum BrWndShadow {
    BR_SH_10AC5C5C = 0, /* the message-hook object                           */
    BR_SH_10AC5C5C_68,  /* its +0x68 field, tested at 0x100194DD. The type is *
                         * unported, so the FIELD is shadowed separately      *
                         * rather than a struct being invented for it.        */
    BR_SH_10AC5C58,     /* hook A's first argument                           */
    BR_SH_10AC408C,     /* non-zero enables hook B                           */
    BR_SH_105CCB5C,     /* "already suspended" latch; also gates MM_MCINOTIFY */
    BR_SH_105CCB88,     /* tested twice in the deactivate block              */
    BR_SH_100A9354,     /* display-mode restore state: 1 pending, 2 done     */
    BR_SH_105BC8DC,     /* cleared at 0x10019393                             */
    BR_SH_10226A48,     /* 0x1001938E                                        */
    BR_SH_10226A44,     /* 0x100193A1                                        */
    BR_SH_10AF21B0,     /* compared < 0x100BCBE8 at 0x100193BE               */
    BR_SH_100BCBE8,
    BR_SH_1021C770,     /* the MCI device id MM_MCINOTIFY's lParam must match */
    BR_SH_1021C788,     /* what 0x100027A0 returns -- the EAR message's wParam */
    BR_SH_104B1620,     /* RegisterWindowMessageA("EAR Interactive Around-Sound") */
    BR_SH_COUNT
} BrWndShadow;

int32_t BrWndShadowGet(BrWndShadow which);
void    BrWndShadowSet(BrWndShadow which, int32_t value);

/* ------------------------------------------------------------------ *
 * The two PRE-DISPATCH HOOKS at 0x100194E4..0x10019504.
 *
 * Before any message is looked at, the procedure offers it to two other
 * subsystems and DISCARDS both results:
 *
 *   if ([0x10AC5C5C] && [0x10AC5C5C]->+0x68)
 *       0x100590D0([0x10AC5C58], hWnd, uMsg, wParam, lParam);   __stdcall
 *   if (the same guard) and ([0x10AC408C])
 *       0x10035A30(hWnd, uMsg, wParam, lParam);                 __stdcall
 *
 * Both are __stdcall: neither call site does any stack cleanup (0x100194F3 and
 * 0x10019505 follow the calls immediately), so the callees pop their own
 * arguments. Note that the SECOND hook is nested inside the first's guard --
 * 0x100194FA's `je` targets 0x10019505, past both -- so [0x10AC408C] alone is
 * not enough to run hook B.
 *
 * Neither callee is transcribed. They are DECLARED as installable pointers,
 * not stood in for.
 * ------------------------------------------------------------------ */
typedef void (*BrWndMsgHookA)(int32_t iArg, void *hWnd, uint32_t uMsg,
                              BrWParam wParam, BrLParam lParam);  /* 0x100590D0 */
typedef void (*BrWndMsgHookB)(void *hWnd, uint32_t uMsg,
                              BrWParam wParam, BrLParam lParam);  /* 0x10035A30 */

extern BrWndMsgHookA g_pfnBrWndMsgHookA;
extern BrWndMsgHookB g_pfnBrWndMsgHookB;

/* ------------------------------------------------------------------ *
 * THE FRONTIER: the eleven callees reached from these four functions that are
 * not transcribed. Same discipline as br_bootfrontier.c -- each entry does
 * NOTHING, RECORDS that it was asked for, and invents no return value. The two
 * that must return something return the value that is unambiguously "nothing
 * happened" for their caller.
 * ------------------------------------------------------------------ */
typedef enum BrWndFrontierId {
    BR_WF_10002580 = 0, /* EAR: (re)create the music channel  -- DEVICEARRIVAL */
    BR_WF_10002AF0,     /* music: play track n                                 */
    BR_WF_10002F70,     /* music: stop                                         */
    BR_WF_10002760,     /* EAR: shut the channel down                          */
    BR_WF_10002CF0,     /* music: advance to the next track                    */
    BR_WF_10002830,     /* MCI: the notified track finished, play the next     */
    BR_WF_100325B0,     /* WM_DESTROY teardown, called with 0                  */
    BR_WF_100609F0,     /* deactivate: 1 of 4                                  */
    BR_WF_10002EB0,     /* deactivate: 2 of 4                                  */
    BR_WF_1006BD70,     /* deactivate: 3 of 4                                  */
    BR_WF_10061440,     /* deactivate: 4 of 4                                  */
    BR_WF_10004F50,     /* deactivate: the 0x10226A48/44 arm, 1 of 2           */
    BR_WF_10005330,     /* deactivate: the 0x10226A48/44 arm, 2 of 2           */
    BR_WF_1007296C,     /* deactivate tail                                     */
    BR_WF_1001DD80,     /* re-set the video mode (w, h). 0 == FAILED.          */
    BR_WF_10019A40,     /* activate tail                                       */
    BR_WF_COUNT
} BrWndFrontierId;

int32_t     BrWndFrontierHits(BrWndFrontierId id);
const char *BrWndFrontierName(BrWndFrontierId id);
void        BrWndFrontierReport(void);

/* 0x1001DD80 is the one frontier entry whose RESULT changes control flow: zero
 * means the mode change failed and the game puts up a message box and closes
 * itself. It cannot be given a fabricated success, so it is settable and
 * defaults to 0 -- FAILURE -- because that is the answer a port with no
 * renderer must honestly give. A host that really did set the mode says so. */
void BrWndSetModeResult(int32_t iResult);

/* ------------------------------------------------------------------ *
 * The four functions.
 * ------------------------------------------------------------------ */

/* 0x100194C0. __stdcall in the original; four arguments, callee-cleaned. */
BrWndResult BrWndProc(void *hWnd, uint32_t uMsg,
                      BrWParam wParam, BrLParam lParam);

/* 0x10070370 -- WM_ACTIVATE. __cdecl, ONE argument (wParam). Sets all three
 * of the main loop's gates and returns nothing. */
void BrOnActivate(BrWParam wParam);

/* 0x10019350 -- WM_ACTIVATEAPP. __cdecl, three arguments. */
BrWndResult BrOnActivateApp(void *hWnd, BrWParam wParam, BrLParam lParam);

/* 0x10019480 -- WM_SYSCOMMAND. __cdecl, three arguments. */
BrWndResult BrOnSysCommand(void *hWnd, BrWParam wParam, BrLParam lParam);

/* Reset everything this module owns to its load-time value. */
void BrInputResetForTest(void);

#endif /* BR_INPUT_H */
