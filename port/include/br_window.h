/* br_window.h -- 0x10019670 (create the window) and 0x10017E30 (the mode-2
 * audio-middleware startup the main loop runs before the pump).
 *
 * ARCHITECTURAL CONCERN: app / platform. This sits beside br_boot.c and
 * br_mainloop.c; the window procedure that the class registered here names is
 * a DIFFERENT concern and lives in br_input.h.
 *
 * ------------------------------------------------------------------------
 * 0x10019670 -- THE ESP TRACE, written out because the brief that commissioned
 * this file warned about exactly this hazard and it is real here.
 *
 * The WNDCLASSA is built on the stack with pushes interleaved between the
 * field stores, so the SAME `[esp+N]` names different fields at different
 * points. Let R = esp on entry and S = R - 0x28 (i.e. esp just after
 * `sub esp,0x28`). S is the address of the WNDCLASSA.
 *
 *   10019670  sub esp,0x28              esp = S
 *   10019678  push 0x65                 esp = S-4
 *   1001967A  push eax                  esp = S-8
 *   10019680  [esp+0x08] = 3            S+0x00  style        = 3
 *   10019688  [esp+0x0C] = 0x100194C0   S+0x04  lpfnWndProc  = the wndproc
 *   10019690  [esp+0x10] = 0            S+0x08  cbClsExtra   = 0
 *   10019698  [esp+0x14] = 0            S+0x0C  cbWndExtra   = 0
 *   100196A0  [esp+0x18] = eax          S+0x10  hInstance    = [0x105BC730]
 *   100196A4  call LoadIconA            stdcall, 2 args      esp = S
 *   100196AA  push 0x7F00               esp = S-4
 *   100196AF  push 0                    esp = S-8
 *   100196B1  [esp+0x1C] = eax          S+0x14  hIcon        = icon 0x65
 *   100196B5  call LoadCursorA          stdcall, 2 args      esp = S
 *   100196BB  push 4                    esp = S-4
 *   100196BD  [esp+0x1C] = eax          S+0x18  hCursor      = IDC_ARROW
 *   100196C1  call GetStockObject       stdcall, 1 arg       esp = S
 *   100196C7  [esp+0x1C] = eax          S+0x1C  hbrBackground= stock 4
 *   100196CB  lea eax,[esp]             eax = S, the WNDCLASSA
 *   100196CF  push eax                  esp = S-4
 *   100196D0  [esp+0x24] = 0x1007B378   S+0x20  lpszMenuName = "BossRally"
 *   100196D8  [esp+0x28] = 0x1007B378   S+0x24  lpszClassName= "BossRally"
 *   100196E0  call RegisterClassA       stdcall, 1 arg       esp = S
 *
 * Three of the ten fields are written through a displacement that names a
 * DIFFERENT field elsewhere in the same function: 0x1C is hIcon at 0x100196B1,
 * hCursor at 0x100196BD and hbrBackground at 0x100196C7. Reading them as one
 * slot -- the failure this project has shipped twice -- would give a WNDCLASS
 * with one field set three times and two left zero.
 *
 * The trace also PINS the layout: `sub esp,0x28` is exactly sizeof(WNDCLASSA)
 * on Win32 and the last field written lands at S+0x24, its last dword. There
 * is no slack, so the stores cannot be anything but the ten WNDCLASS fields.
 *
 * TWO THINGS WORTH NOTICING, both preserved:
 *
 *   - RegisterClassA's RETURN VALUE IS DISCARDED. If registration fails the
 *     function goes on to call CreateWindowExA anyway.
 *   - lpszMenuName is the SAME pointer as lpszClassName -- 0x1007B378,
 *     "BossRally", stored twice. The window has no menu resource by that name,
 *     so this is very likely a copy-paste in the original; it is harmless and
 *     it is kept.
 *
 * AND THE THING THIS FUNCTION DOES NOT DO: it does not store the HWND. The
 * `xor ecx,ecx / test eax,eax / setne cl` tail turns CreateWindowExA's result
 * into a 0/1 and throws the handle away. 0x105BC72C -- the handle the main
 * loop hands to ShowWindow, UpdateWindow and SetFocus at 0x10019735,
 * 0x1001974A and 0x10019757 -- is written by the WINDOW PROCEDURE, on
 * WM_CREATE, at 0x100195BD. CreateWindowExA dispatches WM_CREATE before it
 * returns, so by the time this function tests its result the handle is already
 * in place. A host that does not deliver WM_CREATE to BrWndProc from inside
 * its create callback will leave g_brhWnd NULL and the main loop with nothing
 * to show. See BrWindowOps below.
 *
 * ------------------------------------------------------------------------
 * 0x10017E30 -- WHAT MODE 2 IS. The main loop calls this at 0x10019773, with
 * the HWND, only when [0x1007B074] == 2.
 *
 * [0x1007B074] is the MUSIC BACKEND SELECTOR, and mode 2 is the EAR audio
 * middleware. Established from the data rather than guessed, four ways:
 *
 *   1. 0x10017E30 -> 0x10017910 LoadLibraryA("earias.dll") or ("earpds.dll")
 *      (0x100A74E4 / 0x100A74F0) and GetProcAddress's about thirty
 *      `_EAR_DLL_*@n` entry points into the table at 0x104B15F8..0x104B1684.
 *   2. 0x10017E10 RegisterWindowMessageA("EAR Interactive Around-Sound")
 *      -> 0x104B1620, which is the message id the window procedure compares
 *      uMsg against at 0x1001950F, in its mode-2 arm.
 *   3. 0x10002F70 branches on [0x1007B074] == 1 to a path built out of
 *      WINMM mciSendCommandA against the device id at 0x1021C770, and
 *      otherwise to one built out of [0x104B1628] == _EAR_DLL_ClearChannel@8.
 *      So mode 1 is MCI CD audio and mode 2 is EAR.
 *   4. The window procedure's mode-1 message is 0x3B9 == MM_MCINOTIFY and its
 *      lParam is compared against that same MCI device id 0x1021C770.
 *
 * So: 0 = no music, 1 = MCI redbook CD audio, 2 = EAR. See br_input.h for the
 * three window messages that are gated on this global.
 *
 * WHAT 0x10017E30 ITSELF DOES, in order:
 *
 *   ++[0x104B1688]; run the body only when the result is 1. It is never
 *   decremented, so this is a RUN-ONCE guard and not a recursion counter --
 *   every later call returns 1 having done nothing at all.
 *
 *   0x10017910([0x100A74FC])  load the DLL and resolve the entry points.
 *                             The argument picks earpds.dll (non-zero) over
 *                             earias.dll (zero). 0 means failure:
 *                             MessageBoxA(hWnd, str(0xFE), str(0xFD),
 *                             MB_ICONERROR) then exit(1).
 *   [0x104B1658](0x9BE9C9)    _EAR_DLL_AAA_Validate@4
 *   [0x104B1634](hWnd)        _EAR_DLL_AssignHwnd@4
 *   [0x104B1668](0)           _EAR_DLL_InitializeEar@4. Non-zero == success,
 *                             and the function returns 1 there.
 *   [0x104B166C]()            _EAR_DLL_GetLastError@0. 3 gets its own message,
 *                             str(0x12E), then exit(1); anything else calls
 *                             [0x104B1650] _EAR_DLL_ShowLastError@0 and then
 *                             exit(1) with no message of its own.
 *
 * ITS ESP TRACE, the same hazard again and this one is nastier because the
 * cleanup is late:
 *
 *   entry                    esp = R,   hWnd at R+4
 *   10017E4B push edi        esp = R-4
 *   10017E4C push esi        esp = R-8
 *   10017E4D push ebx        esp = R-0xC
 *   10017E4E push eax        esp = R-0x10        (the argument to 0x10017910)
 *   10017E4F call 0x10017910                     cdecl -- esp still R-0x10
 *   10017E54 mov esi,[esp+0x14]                  R-0x10+0x14 = R+4 == hWnd
 *   10017E64 add esp,4       esp = R-0xC         THE CLEANUP IS HERE, AFTER
 *
 * Assuming the `add esp,4` had already run would read R+8, which is not an
 * argument of this function at all.
 *
 * The two calls that follow the resolve are made through IMPORT-STYLE
 * function-pointer slots in .data, not direct calls, so `call [0x104B1658]`
 * with the slot still NULL is what happens when the DLL is absent -- except
 * that the failure path above exits first. That ordering is the whole point of
 * the function and is preserved.
 */
#ifndef BR_WINDOW_H
#define BR_WINDOW_H

#include <stdint.h>

/* ------------------------------------------------------------------ *
 * The window handle and the instance handle.
 *
 * LP64: an HWND and an HINSTANCE are POINTERS. They must not live in an
 * int32_t here even though the original's are dwords -- CONVENTIONS.md,
 * "Portability".
 * ------------------------------------------------------------------ */

/* 0x105BC72C -- the game's one window. Written by the window procedure on
 * WM_CREATE (0x100195BD, br_input.c) and read by the main loop at 0x10019735.
 * br_window.c owns the storage; grep the ADDRESS before adding another name
 * for it. */
extern void *g_brhWnd;

/* 0x105BC730 -- the HINSTANCE BRally.exe passed in. This is BrBootArgs.hInstance
 * in br_boot.h; that header declares the TYPE and no storage, so the address
 * has no other owner and this is it. If a later pass gives BrBootArgs storage,
 * these two must become one object, not two. */
extern void *g_brhInstance;

/* 0x118EEF1C -- a SECOND copy of the instance handle, written by 0x1001967B.
 * The original really does keep two objects, so the port does too rather than
 * folding them and losing the fact. Nothing in the transcribed spine reads it. */
extern void *g_brhInstance2;

/* ------------------------------------------------------------------ *
 * [0x1007B074] -- the music backend. See the banner.
 *
 * SHADOWED, NOT OWNED. Thirty-five sites in BRGlide.dll read this global and
 * almost all of them are in the music module at 0x10002xxx, which is not
 * ported. These two functions are the ONE place to re-point when that module
 * lands; a second definition under a second name is the aliasing bug
 * CONVENTIONS.md documents.
 * ------------------------------------------------------------------ */
enum {
    BR_AUDIO_NONE = 0,   /* every 0x10002xxx entry point early-outs */
    BR_AUDIO_MCI  = 1,   /* WINMM mciSendCommandA, redbook CD audio */
    BR_AUDIO_EAR  = 2    /* "EAR Interactive Around-Sound", earias/earpds.dll */
};
int32_t BrWindowAudioBackend(void);
void    BrWindowSetAudioBackend(int32_t iBackend);

/* ------------------------------------------------------------------ *
 * 0x10019670 -- the window description, as DATA.
 *
 * Every field is an immediate read out of the disassembly. Nothing here is
 * chosen; if a value looks odd (pszMenuName, below) it is odd in the original.
 * ------------------------------------------------------------------ */
typedef struct BrWindowDesc {
    /* the WNDCLASSA at S+0x00 .. S+0x24 */
    uint32_t    uClassStyle;      /* S+0x00  3 == CS_VREDRAW|CS_HREDRAW      */
    int32_t     cbClsExtra;       /* S+0x08  0                               */
    int32_t     cbWndExtra;       /* S+0x0C  0                               */
    void       *hInstance;        /* S+0x10  [0x105BC730]                    */
    int32_t     idIcon;           /* S+0x14  LoadIconA(hInstance, 0x65)      */
    int32_t     idCursor;         /* S+0x18  LoadCursorA(NULL, 0x7F00)       */
    int32_t     idStockBrush;     /* S+0x1C  GetStockObject(4)               */
    const char *pszMenuName;      /* S+0x20  "BossRally" -- yes, the class   */
    const char *pszClassName;     /* S+0x24  "BossRally"  (0x1007B378)       */

    /* the twelve arguments of CreateWindowExA at 0x10019718 */
    uint32_t    dwExStyle;        /* 0x40000    WS_EX_APPWINDOW              */
    const char *pszWindowName;    /* "Boss Rally"  (0x1007B384)              */
    uint32_t    dwStyle;          /* 0x80C20000 WS_POPUP|WS_CAPTION|WS_MINIMIZEBOX */
    int32_t     x, y;             /* 0, 0                                    */
    int32_t     cx, cy;           /* [0x100A7514], [0x100A7518] -- and those  *
                                   * two ARE g_brAppModeW / g_brAppModeH,     *
                                   * br_boot.c's. Reused, never redefined.    */
} BrWindowDesc;

/* Fill pDesc with what 0x10019670 asks for. Reads g_brhInstance and
 * g_brAppModeW/H at the moment of the call, exactly as the original reads
 * 0x105BC730 and 0x100A7514/18 at 0x10019673 and 0x100196EC. */
void BrWindowDescribe(BrWindowDesc *pDesc);

/* The two platform calls 0x10019670 makes.
 *
 * pfnCreateWindow MUST deliver WM_CREATE to BrWndProc before it returns, the
 * way CreateWindowExA does. That is what puts the handle in g_brhWnd -- this
 * function does not store it. */
typedef struct BrWindowOps {
    int   (*pfnRegisterClass)(const BrWindowDesc *pDesc, void *pUser);
    void *(*pfnCreateWindow) (const BrWindowDesc *pDesc, void *pUser);
    void   *pUser;
} BrWindowOps;

/* 0x10019670. Returns 1 if CreateWindowExA produced a handle, 0 if not.
 * pfnRegisterClass's result is IGNORED, as in the original. */
int BrWindowCreate(const BrWindowOps *pOps);

/* ------------------------------------------------------------------ *
 * 0x10017E30 -- the mode-2 (EAR) startup.
 * ------------------------------------------------------------------ */

/* The five middleware entry points 0x10017E30 reaches, by the .data slot the
 * loader at 0x10017910 puts each one in. A host that has no EAR middleware
 * leaves the whole struct NULL; the run-once guard and the decision tree are
 * still exercised, and BrWindowEarFrontierHits() counts what was asked for. */
typedef struct BrEarOps {
    /* 0x10017910 itself: load the DLL and resolve the entry points.
     * Returns 0 on failure. iWhich is [0x100A74FC]: non-zero picks
     * earpds.dll, zero picks earias.dll. */
    int32_t (*pfnLoad)        (int32_t iWhich, void *pUser);
    void    (*pfnAAAValidate) (int32_t iCookie, void *pUser);  /* 0x104B1658 */
    void    (*pfnAssignHwnd)  (void *hWnd, void *pUser);       /* 0x104B1634 */
    int32_t (*pfnInitializeEar)(int32_t a, void *pUser);       /* 0x104B1668 */
    int32_t (*pfnGetLastError)(void *pUser);                   /* 0x104B166C */
    void    (*pfnShowLastError)(void *pUser);                  /* 0x104B1650 */
    void     *pUser;
} BrEarOps;

/* The cookie 0x10017E93 hands to _EAR_DLL_AAA_Validate@4. Read out of the
 * instruction, not invented. */
#define BR_EAR_VALIDATE_COOKIE  0x009BE9C9

/* [0x100A74FC] -- which DLL 0x10017910 is asked for. One reference in the
 * whole binary, this one, so it is shadowed here with the same re-point note
 * as the audio backend. */
int32_t BrWindowEarDllSelect(void);
void    BrWindowSetEarDllSelect(int32_t iWhich);

/* 0x10017E30. Always returns 1 -- every path does, including the run-once
 * early-out and both fatal paths (which the original does not return from at
 * all, because they call exit()). See BrWndPlatformOps::pfnExit in br_input.h:
 * a host that supplies a real exit() never comes back, and a test that
 * supplies a recording one sees the 1. */
int32_t BrWindowEarStartup(void *hWnd, const BrEarOps *pOps);

/* How many times 0x10017E30's body has run past the guard. The original has
 * no such counter; this makes the run-once rule observable. */
int32_t BrWindowEarStartupBodies(void);

/* Reset every global this module owns to its load-time value. Not in the
 * original -- the original gets fresh .data from the loader. */
void BrWindowResetForTest(void);

#endif /* BR_WINDOW_H */
