/* br_window.c -- see br_window.h.
 *
 * ARCHITECTURAL CONCERN: app / platform. 0x10019670 (create the window) and
 * 0x10017E30 (the mode-2 EAR startup the main loop runs before the pump),
 * transcribed from BRGlide.dll.
 *
 * The two ESP traces that make these functions readable are in the header, not
 * here, because they are the thing a future reader has to check first.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "br_window.h"
#include "br_input.h"      /* g_brWndPlatform: MessageBoxA / exit / the strings */
#include "br_boot.h"       /* g_brAppModeW / g_brAppModeH == 0x100A7514 / 18 */

#include <stddef.h>

/* ------------------------------------------------------------------ *
 * The globals this module owns.
 * ------------------------------------------------------------------ */
void *g_brhWnd;        /* 0x105BC72C -- written by BrWndProc on WM_CREATE */
void *g_brhInstance;   /* 0x105BC730 */
void *g_brhInstance2;  /* 0x118EEF1C */

/* [0x1007B074] and [0x100A74FC] -- shadowed, not owned. See br_window.h. */
static int32_t s_iAudioBackend;
static int32_t s_iEarDllSelect;

/* [0x104B1688] -- 0x10017E30's own counter. Four references in the whole
 * binary and two of them are this function, so it is genuinely local to the
 * EAR startup and is owned here rather than shadowed. */
static int32_t s_cEarStartupCalls;
static int32_t s_cEarStartupBodies;

int32_t BrWindowAudioBackend(void)             { return s_iAudioBackend; }
void    BrWindowSetAudioBackend(int32_t i)     { s_iAudioBackend = i; }
int32_t BrWindowEarDllSelect(void)             { return s_iEarDllSelect; }
void    BrWindowSetEarDllSelect(int32_t i)     { s_iEarDllSelect = i; }
int32_t BrWindowEarStartupBodies(void)         { return s_cEarStartupBodies; }

void BrWindowResetForTest(void)
{
    g_brhWnd            = NULL;
    g_brhInstance       = NULL;
    g_brhInstance2      = NULL;
    s_iAudioBackend     = 0;
    s_iEarDllSelect     = 0;
    s_cEarStartupCalls  = 0;
    s_cEarStartupBodies = 0;
}

/* ================================================================== *
 * 0x10019670 -- create the window. 187 bytes.
 * ================================================================== */

/* The two string literals, at the addresses the pushes name. */
static const char s_szClass[] = "BossRally";    /* 0x1007B378 */
static const char s_szTitle[] = "Boss Rally";   /* 0x1007B384 */

/* 0x10019670's parameters as data -- the WNDCLASSA built on the stack at
 * S+0x00..S+0x24 and CreateWindowExA's twelve arguments. */
/* WHAT IT DOES: describes the game's main window -- its class, its icon and
 * cursor, its background, and its title -- for the window system to create.
 * The original stores the same string as both the menu name and the class
 * name, and there is no menu by that name; that is reproduced because it is
 * what the game asks for. */
/* NOT A CLAIM.  0x10019670's @implements line lives on BrWindowCreate below.
 * This function is the argument setup only: it registers nothing, creates
 * nothing and returns nothing, so it cannot be what a caller of 0x10019670
 * gets.  The claim used to sit here, which made the address read as ported by
 * a function that makes no calls at all -- the shape claimcheck.py looks for
 * and, in this case, correctly found. */
void BrWindowDescribe(BrWindowDesc *pDesc)
{
    if (pDesc == NULL)
        return;

    /* WNDCLASSA, in the order the ESP trace establishes. */
    pDesc->uClassStyle  = 3;                 /* S+0x00, 0x10019680 */
                                             /* S+0x04 is BrWndProc, 0x10019688 */
    pDesc->cbClsExtra   = 0;                 /* S+0x08, 0x10019690 */
    pDesc->cbWndExtra   = 0;                 /* S+0x0C, 0x10019698 */
    pDesc->hInstance    = g_brhInstance;     /* S+0x10, 0x100196A0 */
    pDesc->idIcon       = 0x65;              /* S+0x14, LoadIconA */
    pDesc->idCursor     = 0x7F00;            /* S+0x18, IDC_ARROW */
    pDesc->idStockBrush = 4;                 /* S+0x1C, GetStockObject(4) */
    /* S+0x20 and S+0x24 are the SAME pointer in the original -- 0x1007B378
     * stored twice, at 0x100196D0 and 0x100196D8. The window has no menu
     * resource called "BossRally"; this is kept because it is what the
     * original asks for. */
    pDesc->pszMenuName  = s_szClass;
    pDesc->pszClassName = s_szClass;

    /* CreateWindowExA's twelve arguments, unwound from the pushes at
     * 0x100196F7..0x10019713 (last pushed == first argument). */
    pDesc->dwExStyle      = 0x40000u;        /* WS_EX_APPWINDOW */
    pDesc->pszWindowName  = s_szTitle;
    pDesc->dwStyle        = 0x80C20000u;     /* POPUP|CAPTION|MINIMIZEBOX */
    pDesc->x  = 0;
    pDesc->y  = 0;
    /* 0x100196EC reads [0x100A7518] into edx and 0x100196F2 reads [0x100A7514]
     * into eax, then pushes edx BEFORE eax -- so cx is 0x100A7514 (width) and
     * cy is 0x100A7518 (height), which is br_boot.c's g_brAppModeW/H. Reused,
     * never redefined: one address, one owner. */
    pDesc->cx = g_brAppModeW;
    pDesc->cy = g_brAppModeH;
    /* hWndParent, hMenu and lpParam are all 0 (0x100196F7, 0x100196FA,
     * 0x100196FC) and hInstance is [0x105BC730] again (0x100196E6). */
}

/* 0x10019670 -- RegisterClassA + CreateWindowExA. Returns 1 if a window came
 * back, 0 if not. Does NOT store the handle; the window procedure does, from
 * WM_CREATE.
 *
 * This is 0x10019670: the instance-handle copy at 0x1001967B, both Win32
 * calls and the 0x1001971E return are all here, and the WNDCLASSA/
 * CreateWindowExA argument block is filled by BrWindowDescribe above. */
/* WHAT IT DOES: makes the game's main window. It asks the window system to
 * learn about the game's window class and then to create the window itself,
 * and reports whether one came back. It does not remember the window: the
 * window's own message handler does that when it is told the window exists. */
#ifdef BR_MATCHING_BUILD
#include <windows.h>

/* @implements 0x10019670 glide BrWindowCreate */
int BrWindowCreate(const BrWindowOps *pOps)
{
    WNDCLASSA wc;
    void     *hInst = g_brhInstance;
    HWND      hWnd;

    (void)pOps;

    g_brhInstance2 = hInst;

    wc.style         = 3;
    wc.lpfnWndProc   = (WNDPROC)BrWndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = (HINSTANCE)hInst;
    wc.hIcon         = LoadIconA((HINSTANCE)hInst, (LPCSTR)0x65);
    wc.hCursor       = LoadCursorA(NULL, (LPCSTR)0x7F00);
    wc.hbrBackground = (HBRUSH)GetStockObject(4);
    wc.lpszMenuName  = "BossRally";
    wc.lpszClassName = "BossRally";
    RegisterClassA(&wc);

    hWnd = CreateWindowExA(0x40000, "BossRally", "Boss Rally", 0x80C20000u,
                           0, 0, g_brAppModeW, g_brAppModeH,
                           NULL, NULL, (HINSTANCE)g_brhInstance, NULL);
    return hWnd != NULL;
}
#else
int BrWindowCreate(const BrWindowOps *pOps)
{
    BrWindowDesc desc;
    void        *hWnd;

    /* 0x10019673 / 0x1001967B: the instance handle is read once and copied to
     * the second global before anything else happens. */
    g_brhInstance2 = g_brhInstance;

    if (pOps == NULL || pOps->pfnRegisterClass == NULL ||
        pOps->pfnCreateWindow == NULL)
        return 0;

    BrWindowDescribe(&desc);

    /* 0x100196E0: RegisterClassA. ITS RESULT IS DISCARDED -- nothing tests it
     * and the very next instruction reloads eax from 0x105BC730. A failed
     * registration is followed by a CreateWindowExA attempt anyway. */
    (void)pOps->pfnRegisterClass(&desc, pOps->pUser);

    /* 0x10019718: CreateWindowExA. The handle is NOT stored here -- the
     * window procedure stores it from WM_CREATE, which CreateWindowExA
     * dispatches before returning. A host whose pfnCreateWindow does not do
     * the same leaves g_brhWnd NULL. */
    hWnd = pOps->pfnCreateWindow(&desc, pOps->pUser);

    /* 0x1001971E: xor ecx,ecx / test eax,eax / setne cl / mov eax,ecx. */
    return (hWnd != NULL) ? 1 : 0;
}
#endif /* BR_MATCHING_BUILD */

/* ================================================================== *
 * 0x10017E30 -- the mode-2 (EAR) startup. 211 bytes, __cdecl, one argument.
 *
 * The main loop reaches it at 0x10019773, after SetFocus and before the pump,
 * and only when [0x1007B074] == 2:
 *
 *   10019763  cmp dword ptr [0x1007B074], 2
 *   1001976A  jne 0x1001977B
 *   1001976C  mov ecx, [0x105BC72C]        ; the HWND WM_CREATE stored
 *   10019772  push ecx
 *   10019773  call 0x10017E30
 *   10019778  add esp, 4                   ; cdecl
 * ================================================================== */
/* WHAT IT DOES: brings up the surround-sound add-on before the main loop
 * starts, in the one display mode that uses it. It loads the sound library
 * and resolves its entry points; if that fails the game shows an error box
 * and quits. A counter makes it run once only -- and it is never
 * decremented, so a second call does nothing at all. */
/* @implements 0x10017E30 glide BrWindowEarStartup */
int32_t BrWindowEarStartup(void *hWnd, const BrEarOps *pOps)
{
    int32_t iErr;

    /* 0x10017E30..0x10017E45. The counter is incremented and compared to 1,
     * and it is NEVER decremented, so this is a run-once guard: the second and
     * every later call returns 1 having done nothing. Transcribed as the
     * increment-and-compare it is rather than as a boolean, because the
     * original's counter is observable to anything else that reads
     * 0x104B1688. */
    ++s_cEarStartupCalls;
    if (s_cEarStartupCalls != 1)
        return 1;

    ++s_cEarStartupBodies;

    if (pOps == NULL || pOps->pfnLoad == NULL)
        return 1;    /* frontier: nothing to call, and nothing invented */

    /* 0x10017E46..0x10017E67: 0x10017910([0x100A74FC]). That function loads
     * earpds.dll when the argument is non-zero and earias.dll when it is zero,
     * resolves about thirty _EAR_DLL_* entry points into 0x104B15F8..0x104B1684,
     * and finishes with
     *     RegisterWindowMessageA("EAR Interactive Around-Sound") -> 0x104B1620
     * which is the message br_input.c's mode-2 arm compares uMsg against.
     * It returns 0 on failure. */
    if (pOps->pfnLoad(s_iEarDllSelect, pOps->pUser) == 0) {
        /* 0x10017E6B..0x10017E90: MessageBoxA(hWnd, str(0xFE), str(0xFD),
         * MB_ICONHAND) then exit(1). lpText is the SECOND lookup pushed and
         * lpCaption the first -- 0xFD is pushed with the 0x10 and its result
         * becomes the caption. */
        if (g_brWndPlatform.pfnMessageBox != NULL)
            g_brWndPlatform.pfnMessageBox(
                hWnd,
                (g_brWndPlatform.pfnString != NULL)
                    ? g_brWndPlatform.pfnString(0xFE) : NULL,
                (g_brWndPlatform.pfnString != NULL)
                    ? g_brWndPlatform.pfnString(0xFD) : NULL,
                BR_MB_ICONERROR);
        if (g_brWndPlatform.pfnExit != NULL)
            g_brWndPlatform.pfnExit(1);
        /* The original does not come back from exit(). A test that supplies a
         * recording pfnExit does, and then falls through to 0x10017E93 exactly
         * as the instruction stream would -- which is what the original's
         * fall-through into the validate call literally is. */
    }

    /* 0x10017E93: [0x104B1658](0x9BE9C9) -- _EAR_DLL_AAA_Validate@4. */
    if (pOps->pfnAAAValidate != NULL)
        pOps->pfnAAAValidate(BR_EAR_VALIDATE_COOKIE, pOps->pUser);

    /* 0x10017E9E: [0x104B1634](hWnd) -- _EAR_DLL_AssignHwnd@4. This is the
     * only thing in the whole startup that uses the window handle, and it is
     * why the main loop has to pass it. */
    if (pOps->pfnAssignHwnd != NULL)
        pOps->pfnAssignHwnd(hWnd, pOps->pUser);

    /* 0x10017EA5: [0x104B1668](0) -- _EAR_DLL_InitializeEar@4. NON-ZERO is
     * success: 0x10017EAF `jne 0x10017EFA` returns 1 straight away. */
    if (pOps->pfnInitializeEar != NULL &&
        pOps->pfnInitializeEar(0, pOps->pUser) != 0)
        return 1;

    /* 0x10017EB1: [0x104B166C]() -- _EAR_DLL_GetLastError@0. */
    iErr = (pOps->pfnGetLastError != NULL)
         ? pOps->pfnGetLastError(pOps->pUser) : 0;

    if (iErr == 3) {                          /* 0x10017EB7 */
        /* 0x10017EBC..0x10017EE1: MessageBoxA(hWnd, str(0x12E), str(0xFD),
         * MB_ICONHAND) then exit(1). Same caption as the load failure. */
        if (g_brWndPlatform.pfnMessageBox != NULL)
            g_brWndPlatform.pfnMessageBox(
                hWnd,
                (g_brWndPlatform.pfnString != NULL)
                    ? g_brWndPlatform.pfnString(0x12E) : NULL,
                (g_brWndPlatform.pfnString != NULL)
                    ? g_brWndPlatform.pfnString(0xFD) : NULL,
                BR_MB_ICONERROR);
        if (g_brWndPlatform.pfnExit != NULL)
            g_brWndPlatform.pfnExit(1);
        return 1;                             /* 0x10017EE4 */
    }

    /* 0x10017EED: [0x104B1650]() -- _EAR_DLL_ShowLastError@0, which puts the
     * message up itself, then exit(1). No MessageBoxA on this path. */
    if (pOps->pfnShowLastError != NULL)
        pOps->pfnShowLastError(pOps->pUser);
    if (g_brWndPlatform.pfnExit != NULL)
        g_brWndPlatform.pfnExit(1);
    return 1;                                 /* 0x10017EFC */
}
