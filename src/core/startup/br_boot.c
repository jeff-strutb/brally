/* br_boot.c -- see br_boot.h. The game's entry point and top-level state
 * machine, transcribed from BRGlide.dll.
 *
 * Every address in this file was read out of the disassembly. Where a callee
 * is not yet transcribed it is DECLARED and its reach is COUNTED (see
 * br_bootfrontier.c) -- it is not stood in for. The difference is the whole
 * subject of the banner in br_boot.h.
 */
#include "br_boot.h"
#include "br_bootfrontier.h"
#include "br_gamestep.h"   /* 0x1002E324 was ALREADY ported -- see below */

#include <stddef.h>
#include <string.h>   /* the inlined strcpy/strcat at 0x1001CCB5 */

/* ------------------------------------------------------------------ *
 * The globals this module owns.
 * ------------------------------------------------------------------ */
int32_t g_brAppState    = BR_APP_COLD_INIT;  /* 0x105CCBBC */
int32_t g_brAppFrame    = 0;                 /* 0x105CCBB8 */
int32_t g_brAppExitCode = 0;                 /* 0x105CCBC0 */
int32_t g_brAppContinue = 1;                 /* 0x100A98F8 */

/* Video mode, written by state 4. The original spreads each value over three
 * globals; they are listed here so the aliasing is visible rather than
 * hidden behind one name.
 *
 *   width  -> 0x105CCBB4, 0x100A7514, 0x106E7714
 *   height -> 0x105CCBB0, 0x100A7518, 0x106E9A2C
 *
 * 0x100A7514/0x100A7518 are the pair CreateWindowExA reads at 0x100196EC,
 * which is how the window gets its size -- so these are not merely a record
 * of the mode, they are the mode. */
/* 0x105BC730..0x105BC73C, written by RallyMain's prologue and read later by
 * the window creation (hInstance) and ShowWindow (nCmdShow). */
static BrBootArgs s_args;

int32_t g_brAppModeW    = 0;
int32_t g_brAppModeH    = 0;

/* ------------------------------------------------------------------ *
 * 0x1001CD70 -- state 0, COLD INIT
 *
 *     call 0x10032530
 *     push 0 ; call 0x1006C290 ; add esp,4
 *     call 0x10058AF0
 *     mov [0x105CCBBC], 4
 *     mov eax, 1
 *     ret
 *
 * The 0 handed to 0x1006C290 is the MENU sound bank. The same function is
 * called with 1 from the race sound init, which is how port/src/br_sfxsrc.c
 * came to know there are two banks -- this is the other call site, and it is
 * the one that runs first.
 * ------------------------------------------------------------------ */
/* WHAT IT DOES: the very first thing the game does after the window is up --
 * one-off setup, including loading the front-end sound bank so the menus have
 * something to play, and then it hands over to the video-mode stage. It runs
 * once and is never returned to. */
/* @implements 0x1001CD70 glide BrAppStateColdInit */
int32_t BrAppStateColdInit(void)
{
    BrBootFrontier_10032530();
    BrBootFrontier_1006C290(0);      /* set 0 == the front-end bank */
    BrBootFrontier_10058AF0();

    g_brAppState = BR_APP_SET_MODE;  /* 4 */
    return 1;
}

/* ------------------------------------------------------------------ *
 * 0x1001CDA0 -- state 1
 *
 * Sixteen bytes, and all of them are the transition. There is no work here.
 * It exists so that state 3 has somewhere to land that is not state 2
 * directly; the original could have written 2 into 0x105CCBBC from state 3
 * and did not.
 * ------------------------------------------------------------------ */
/* WHAT IT DOES: a one-frame hop from the loading screen into the running
 * game. It does no work of its own; it exists only as a place for the loading
 * stage to land before play begins. */
/* @implements 0x1001CDA0 glide BrAppStateEnterRun */
int32_t BrAppStateEnterRun(void)
{
    g_brAppState = BR_APP_RUN;       /* 2 */
    return 1;
}

/* ------------------------------------------------------------------ *
 * 0x1001CDB0 -- state 2, RUN. This is where the game lives.
 *
 *     inc dword ptr [0x105CCBB8]     ; the frame counter
 *     call 0x1002E324                ; one frame of the game
 *     mov eax, [0x100A98F8]
 *     ret                            ; 0 quits
 *
 * Note what is NOT here: no state assignment. State 2 is terminal in the
 * state machine's own terms -- the game leaves it only by 0x1002E324 or a
 * deeper callee writing 0x105CCBBC, or by 0x100A98F8 going to zero and the
 * main loop exiting.
 * ------------------------------------------------------------------ */
/* WHAT IT DOES: one tick of the running game. Counts the frame, does the
 * frame's work, and reports whether the game should keep going. This is the
 * state the game sits in for as long as it is being played. */
/* @implements 0x1001CDB0 glide BrAppStateRun */
int32_t BrAppStateRun(void)
{
    ++g_brAppFrame;
    /* 0x1002E324 IS BrGameStepInvoke, and it was ported before this module
     * existed. My first draft of br_boot.c gave it a frontier entry -- i.e.
     * declared the game's frame dispatcher missing while a correct
     * transcription of it sat in br_gamestep.c. That is the same failure this
     * project has now made repeatedly: writing the brief before checking
     * whether the tree already had the function.
     *
     * Worth knowing what it is, because it is the seam everything hangs off:
     * eleven bytes, `call dword ptr [0x106E79F4]`. The frame does not decide
     * anything -- it calls whichever step is INSTALLED in that slot. Menu and
     * race are two different installs, so moving between them is a write to
     * 0x106E79F4 and nothing more. */
    (void)BrGameStepInvoke();
    return g_brAppContinue;
}

/* ------------------------------------------------------------------ *
 * 0x1001CDD0 -- state 3, LOADING SCREEN
 *
 *     push [0x10B71A54] ; push [0x10B71A50] ; push [0x10B71A4C]
 *     push [0x10B71A48] ; push 3
 *     call 0x10063970 ; add esp,0x14          (cdecl, 5 args)
 *     push 0 ; push "loading.img"
 *     call 0x1006C990 ; add esp,8             (cdecl, 2 args)
 *     call 0x100628B0
 *     mov eax, 1
 *     mov [0x105CCBBC], eax                   -> state 1
 *     ret 1
 *
 * The argument order is right-to-left in the listing, so the call reads
 * 0x10063970(3, [0x10B71A48], [0x10B71A4C], [0x10B71A50], [0x10B71A54]).
 *
 * "loading.img" at 0x100A9924 is the loading graphic.
 * ------------------------------------------------------------------ */
/* WHAT IT DOES: puts the loading screen on the display and moves on. The
 * picture is "loading.img"; the game shows it while the next stage sets
 * itself up. */
/* @implements 0x1001CDD0 glide BrAppStateLoading */
#ifdef BR_MATCHING_BUILD
/* Orig loads these four globals directly (mov eax,[0x10B71A54] ...), not
 * through the port's accessor thunks. */
extern int32_t DAT_10b71a48;
extern int32_t DAT_10b71a4c;
extern int32_t DAT_10b71a50;
extern int32_t DAT_10b71a54;
#endif

int32_t BrAppStateLoading(void)
{
#ifdef BR_MATCHING_BUILD
    BrBootFrontier_10063970(3,
                            DAT_10b71a48,
                            DAT_10b71a4c,
                            DAT_10b71a50,
                            DAT_10b71a54);
#else
    BrBootFrontier_10063970(3,
                            BrBootGlobal_B71A48(),
                            BrBootGlobal_B71A4C(),
                            BrBootGlobal_B71A50(),
                            BrBootGlobal_B71A54());
#endif
    BrBootFrontier_1006C990("loading.img", 0);   /* 0x100A9924 */
    BrBootFrontier_100628B0();

    g_brAppState = BR_APP_ENTER_RUN;             /* 1 */
    return 1;
}

/* ------------------------------------------------------------------ *
 * 0x1001CE20 -- state 4, VIDEO MODE / RENDERER INIT
 *
 * Two arms, selected by a pair of globals:
 *
 *   0x10AC5C5C  a renderer/device handle -- zero means "not yet created"
 *   0x100ABAA0  a "mode change requested" flag, cleared at the end
 *
 *   both zero      -> first run. Install 640x480 into the six mode globals,
 *                     call 0x1006C460, go to state 3 (the loading screen).
 *
 * CORRECTION: this file first described 0x1006C460 as "renderer create", which
 * was a guess from its position in a video-mode state and was wrong. Its first
 * instruction reads 0x100B55F0 -- BrSndG0B5DE8, the SOUND enable flag in
 * slice1_08.c -- and it is gated on that plus two more sound globals before
 * calling into the 0x1006BBxx family. It is a SOUND reset. The mode state
 * resets audio because a mode change tears the device down, which is a
 * different fact from the one the old label asserted.
 *                     Returns 1 immediately -- the tail below does not run.
 *   handle zero,
 *   flag non-zero  -> 0x1006C290(0), 0x10056260(), 0x1006E280() -> 0x10AC6748
 *   otherwise      -> straight to the tail
 *
 * The literals are 0x280 and 0x1E0 -- 640 and 480 -- and the same two values
 * go to three globals each. See g_brAppModeW/H above for why that matters.
 *
 * The tail (from 0x1001CE9D) is a block of configuration stores gated on
 * 0x118EEEDC. Those globals belong to modules this file does not own, so the
 * tail is NOT transcribed here; it is reached through a counted frontier
 * entry so that a run reports having hit it rather than silently skipping it.
 * ------------------------------------------------------------------ */
/* WHAT IT DOES: decides what resolution the game runs at and gets the display
 * ready. On a first run there is no choice recorded, so it picks 640x480. If
 * the player has changed the mode it tears the old display down and rebuilds
 * -- which also resets the sound, because that goes with the device. */
/* @implements 0x1001CE20 glide BrAppStateSetMode */
int32_t BrAppStateSetMode(void)
{
    const int32_t hDevice = BrBootGlobal_AC5C5C();
    const int32_t fModeChange = BrBootGlobal_ABAA0();

    if (hDevice == 0 && fModeChange == 0) {
        g_brAppModeW = BR_APP_DEFAULT_W;   /* 0x280 */
        g_brAppModeH = BR_APP_DEFAULT_H;   /* 0x1E0 */
        BrBootSetModeGlobals(g_brAppModeW, g_brAppModeH);

        BrBootFrontier_1006C460();
        g_brAppState = BR_APP_LOADING;     /* 3 */
        return 1;
    }

    if (hDevice == 0 && fModeChange != 0) {
        BrBootFrontier_1006C290(0);
        BrBootFrontier_10056260();
        BrBootSetAC6748(BrBootFrontier_1006E280());
    }

    /* 0x1001CE9D onward -- the gated configuration tail. */
    BrBootFrontier_SetModeTail();
    return 1;
}

/* ------------------------------------------------------------------ *
 * 0x1001CF80 -- the frame tick.
 *
 *     mov eax, [0x105CCBBC]
 *     jmp [eax*4 + 0x100A9900]
 *
 * Two instructions and no bounds check: the original jumps through the table
 * with whatever 0x105CCBBC holds. This port RANGE-CHECKS, and that is a
 * deliberate deviation rather than an oversight -- an out-of-range state in
 * the original transfers control to whatever follows the table, which is the
 * string "BossRally", i.e. it executes ASCII. Reproducing that faithfully
 * would mean reproducing undefined behaviour, and there is nothing to gain:
 * no reachable path writes an out-of-range value, so the check is unobservable
 * on every input the game can actually produce.
 * ------------------------------------------------------------------ */
/* WHAT IT DOES: runs whichever stage the game is currently in -- cold start,
 * video setup, loading screen, or play -- once per turn of the main loop, and
 * passes back that stage's "keep going" answer. The original jumps blindly
 * through a table; this port refuses an unrecognised stage instead, which is a
 * deliberate difference described below. */
/* @implements 0x1001CF80 glide BrAppFrame */
#ifdef BR_MATCHING_BUILD
/* The original is twelve bytes: load the state and jump straight through a
 * five-entry table -- `mov eax,[g_brAppState]; jmp dword ptr [tbl + eax*4]`,
 * with no bounds check at all.  A C `switch` cannot produce that: MSVC always
 * guards a switch's jump table with a `cmp`/`ja` pair, which is the entire
 * divergence here.  Tail-dispatching through an explicit function-pointer
 * table does produce the bare indirect jump.
 *
 * The states are contiguous 0..4, so the table is dense and index-exact.
 * This arm deliberately inherits the original's blind jump; the port arm
 * below keeps its guard, because refusing an unrecognised stage is a real
 * safety property and it is not worth trading for a match. */
static int32_t (* const s_apfnAppState[])(void) = {
    BrAppStateColdInit,     /* BR_APP_COLD_INIT == 0 */
    BrAppStateEnterRun,     /* BR_APP_ENTER_RUN == 1 */
    BrAppStateRun,          /* BR_APP_RUN       == 2 */
    BrAppStateLoading,      /* BR_APP_LOADING   == 3 */
    BrAppStateSetMode       /* BR_APP_SET_MODE  == 4 */
};

int32_t BrAppFrame(void)
{
    return s_apfnAppState[g_brAppState]();
}
#else
int32_t BrAppFrame(void)
{
    switch (g_brAppState) {
    case BR_APP_COLD_INIT: return BrAppStateColdInit();
    case BR_APP_ENTER_RUN: return BrAppStateEnterRun();
    case BR_APP_RUN:       return BrAppStateRun();
    case BR_APP_LOADING:   return BrAppStateLoading();
    case BR_APP_SET_MODE:  return BrAppStateSetMode();
    default:               return 0;   /* see the banner: not the original's */
    }
}
#endif

void BrAppResetForTest(void)
{
    g_brAppState    = BR_APP_COLD_INIT;
    g_brAppFrame    = 0;
    g_brAppExitCode = 0;
    g_brAppContinue = 1;
    g_brAppModeW    = 0;
    g_brAppModeH    = 0;
    /* 0x105BC730..73C too. They are module globals and this function's job is
     * load-time state; without it, "CoInitialize failed so the argument stores
     * never ran" is indistinguishable from "a previous test left them set",
     * and the abort-path test silently passes on stale data. */
    s_args.hInstance = NULL; s_args.hPrevInstance = NULL;
    s_args.pszCmdLine = NULL; s_args.nCmdShow = 0;
    BrBootFrontierReset();
}

/* ==================================================================== *
 * RallyMain and the argument block. See br_boot.h for the full listing.
 * ==================================================================== */

const BrBootArgs *BrAppArgs(void) { return &s_args; }

/* WHAT IT DOES: this is where Boss Rally starts. The launcher hands control
 * here, and it brings the game up in order -- check the machine has DirectX 6
 * (refusing with a message box if not), make sure another copy is not already
 * running, read the settings file next to the game, create the window, and
 * hand over to the message loop. When the loop ends it tidies up and returns
 * the game's exit code. Most of the function is failure paths: there are five
 * different ways to give up and they clean up differently.
 *
 * 0x1001CC00 -- see br_boot.h for the instruction-level listing. */
/* @implements 0x1001CC00 glide BrRallyMain */
#ifdef BR_MATCHING_BUILD
/* The original calls its platform DIRECTLY -- six `call rel32`, three
 * `call dword ptr [IAT]` and two inlined string intrinsics. The port's ops
 * table turns every one of those into `call dword ptr [ops+N]`, which is the
 * whole 116-instruction shape gap this arm removes. Nothing else differs:
 * the control flow below is the same one the port arm expresses.
 *
 * The callees are declared here rather than pulled in from their own headers
 * because those headers give them the PORT's ops-table signatures. What the
 * original's bytes need is the original's: no arguments, direct calls. */
extern const char *BrStrGet(int32_t id);                 /* 0x1006D280 */
extern void        BrDxDetect(int32_t *pVer, int32_t *pPlat);  /* 0x1001D8A0 */
extern int32_t     BrAppCheckPreviousApp(void);          /* 0x10007E80 */
extern void        BrMemoryQuery(void);                  /* 0x10007F10 */
extern void        BrBaseDirInit(void);                  /* 0x10063860 */
extern void        BrStrResLoad(void);                   /* 0x1006D1A0 */
extern void        BrCmdLineParse(const char *psz);      /* 0x10007F40 */
extern int32_t     BrWindowCreate(void);                 /* 0x10019670 */
extern void        BrDesktopSetup(void);                 /* 0x10009C00 */
extern int32_t     BrUiBootPreLoopGate(void);            /* 0x10056260 */
extern void        BrMainLoopRun(void);                  /* 0x10019730 */

/* 0x10063060 is __thiscall with one stack argument, which MSVC 5.0 cannot
 * spell in C (`error C4234`). The reachable form is __fastcall with every
 * stack argument wrapped in a one-member struct: structs are never
 * register-eligible, so ecx takes `this`, edx is left alone and the callee
 * pops its own argument. See docs/VC5-IDIOMS.md, "CALLING one is ALSO
 * reachable". */
typedef struct BrCfgPathArg { const char *psz; } BrCfgPathArg;
extern void __fastcall BrCfgReadFileT(void *pThis, BrCfgPathArg path);

/* 0x10B71290 -- g_BrCtrlCfg, index 0 of the four config objects.
 * 0x10B72F48 -- the config path this function builds.
 * 0x10B73540 -- the base directory 0x10063860 read out of the registry. */
extern uint8_t DAT_10b71290[];
extern char    DAT_10b72f48[];
extern char    DAT_10b73540[];

/* The three imports the original calls through the IAT. dllimport is what
 * produces `call dword ptr [__imp__...]` rather than a linker thunk. */
__declspec(dllimport) int32_t __stdcall CoInitialize(void *pvReserved);
__declspec(dllimport) void    __stdcall CoUninitialize(void);
__declspec(dllimport) int     __stdcall MessageBoxA(void *hWnd,
                                                    const char *pszText,
                                                    const char *pszCaption,
                                                    unsigned int uType);

int32_t BrRallyMain(void *hInstance, void *hPrevInstance,
                    const char *pszCmdLine, int32_t nCmdShow)
{
    int32_t dxVersion;
    int32_t dxPlatform;
    BrCfgPathArg path;

    g_brAppExitCode = 0;                       /* 0x1001CC03 */

    /* 0x1001CC11..0x1001CC19. Failure jumps STRAIGHT to the CoUninitialize
     * tail, skipping even the argument stores -- hence the wrapped body
     * rather than an early return. */
    if (CoInitialize(NULL) >= 0) {
        s_args.hInstance     = hInstance;      /* 0x1001CC2F */
        s_args.hPrevInstance = hPrevInstance;  /* 0x1001CC34 */
        s_args.pszCmdLine    = pszCmdLine;     /* 0x1001CC44 */
        s_args.nCmdShow      = nCmdShow;       /* 0x1001CC4A */

        BrDxDetect(&dxVersion, &dxPlatform);   /* 0x1001CC50 */

        /* 0x1001CC5C is `jae`, i.e. UNSIGNED -- load-bearing, see br_boot.h. */
        if ((uint32_t)dxVersion < (uint32_t)BR_APP_MIN_DXVERSION) {
            /* 0x1001CC63. Pushes are uType, caption, text, hWnd, so the TEXT
             * is 0x128 and the CAPTION is 0x126. */
            MessageBoxA(NULL, BrStrGet(BR_APP_STR_DX_TEXT),
                        BrStrGet(BR_APP_STR_DX_CAPTION), 0x10);
            return 0;                          /* 0x1001CC89 xor eax,eax */
        }

        /* 0x1001CC91. Returns WITHOUT CoUninitialize -- the original really
         * does leak the apartment on this one path, and eax is already zero
         * from the test, so there is no `xor` here. */
        if (BrAppCheckPreviousApp() == 0) {
            return 0;
        }

        BrMemoryQuery();                       /* 0x1001CCA0 */
        BrBaseDirInit();                       /* 0x1001CCA5 */
        BrStrResLoad();                        /* 0x1001CCAA */
        BrCmdLineParse(pszCmdLine);            /* 0x1001CCAF, arg3 in esi */

        /* 0x1001CCB5..0x1001CD0D -- MSVC's inlined strcpy then strcat:
         * `or ecx,-1; repne scasb; not ecx; sub edi,ecx; shr ecx,2;
         * rep movsd; and ecx,3; rep movsb` twice over. */
        strcpy(DAT_10b72f48, DAT_10b73540);
        strcat(DAT_10b72f48, "BossRally.cfg");

        path.psz = DAT_10b72f48;
        BrCfgReadFileT(DAT_10b71290, path);    /* 0x1001CD12 */

        if (BrWindowCreate() != 0) {           /* 0x1001CD17 */
            BrDesktopSetup();                  /* 0x1001CD20 */
            if (BrUiBootPreLoopGate() != 0) {  /* 0x1001CD25 */
                BrMainLoopRun();               /* 0x1001CD2E */
            }
        }
    }

    CoUninitialize();                          /* 0x1001CD33 */
    return g_brAppExitCode;                    /* 0x1001CD39 */
}
#else
int32_t BrRallyMain(const BrBootArgs *pArgs, const BrRallyMainOps *pOps)
{
    int32_t dxVersion;

    if (pArgs == NULL || pOps == NULL ||
        pOps->pfnCoInitialize == NULL || pOps->pfnCoUninitialize == NULL ||
        pOps->pfnMessageBox   == NULL || pOps->pfnDxVersion      == NULL ||
        pOps->pfnStartGate    == NULL || pOps->pfnCreateWindow   == NULL ||
        pOps->pfnPreLoopGate  == NULL || pOps->pfnRunLoop        == NULL) {
        return 0;
    }

    g_brAppExitCode = 0;                       /* 0x1001CC03 */

    /* 0x1001CC11. A failure here skips EVERYTHING, including the argument
     * stores below -- the jump goes straight to CoUninitialize. */
    if (pOps->pfnCoInitialize(pOps->pUser) < 0) {
        pOps->pfnCoUninitialize(pOps->pUser);
        return g_brAppExitCode;
    }

    s_args = *pArgs;                           /* 0x1001CC2F..0x1001CC4A */

    /* 0x1001CC50 -> 0x1001D8A0, then the UNSIGNED compare at 0x1001CC5C. */
    dxVersion = pOps->pfnDxVersion(pOps->pUser);
    if ((uint32_t)dxVersion < (uint32_t)BR_APP_MIN_DXVERSION) {
        /* 0x1001CC63: MessageBoxA(NULL, str(0x128), str(0x126), 0x10).
         * The push order in the listing is uType, caption, text, hWnd, so the
         * TEXT is 0x128 and the CAPTION is 0x126 -- the reverse of the order
         * the ids appear in. */
        pOps->pfnMessageBox(pOps->pUser, BR_APP_STR_DX_TEXT,
                            BR_APP_STR_DX_CAPTION);
        return 0;                              /* 0x1001CC89 xor eax,eax */
    }

    /* 0x1001CC91. Zero aborts, and note it returns WITHOUT CoUninitialize --
     * the original really does leak the apartment on this one path. Preserved:
     * it is observable to anything else in the process. */
    if (pOps->pfnStartGate(pOps->pUser) == 0) {
        return 0;
    }

    BrBootFrontier_10007F10();
    BrBootFrontier_10063860();
    BrBootFrontier_1006D1A0();
    BrBootFrontier_10007F40(s_args.pszCmdLine);   /* 0x1001CCB0, arg3 */

    /* 0x1001CCB5..0x1001CD0D -- inlined strcpy then strcat, building
     * <dir> + "BossRally.cfg" at 0x10B72F48 from 0x10B73540. */
    BrBootBuildConfigPath();

    /* 0x1001CD12, __thiscall on 0x10B71290 with the path just built. */
    BrBootFrontier_10063060();

    /* 0x1001CD17. Window creation; zero goes to the CoUninitialize tail. */
    if (pOps->pfnCreateWindow(pOps->pUser) != 0) {
        BrBootFrontier_10009C00();             /* 0x1001CD20 */
        if (pOps->pfnPreLoopGate(pOps->pUser) != 0) {   /* 0x1001CD25 */
            pOps->pfnRunLoop(pOps->pUser);     /* 0x1001CD2E -- the main loop */
        }
    }

    pOps->pfnCoUninitialize(pOps->pUser);      /* 0x1001CD33 */
    return g_brAppExitCode;                    /* 0x1001CD39 */
}
#endif
