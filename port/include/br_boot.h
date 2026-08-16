/* br_boot.h -- APPLICATION LIFECYCLE: entry point, window, message loop,
 * and the game's top-level state machine.
 *
 * ARCHITECTURAL CONCERN: app / platform. This is the root of the entire call
 * graph. Everything else in this port hangs below it.
 *
 * WHY THIS FILE APPEARED SO LATE, WHICH IS WORTH RECORDING
 *
 * It should have been the FIRST module written and it was among the last. For
 * most of this project port/host/brally.c hand-wrote a substitute startup --
 * it picked a screen builder, constructed a phase object, and invented the
 * wiring between them -- while the original's actual initialisation sat
 * unread twenty lines from addresses that were already mapped.
 *
 * Every "nothing builds the root menu", "transitions land on an empty phase"
 * and "no boot path exists" note elsewhere in this tree is the same bug seen
 * from a different angle: the entry point had never been decompiled, so there
 * was nothing for the menu system to hang off, so a fake one was built, and
 * then the fake one's shortcomings were investigated as though they were the
 * game's.
 *
 * HOW THE GAME IS ENTERED
 *
 * The disc ships four PE images and only one is the game:
 *
 *   Boot.exe        CD autorun shell -- runs SETUP.EXE / DXSETUP / SetVideo
 *   BossRally.exe   plays brally.avi, then launches brally.exe
 *   BRally.exe      8 KB launcher: reads BossRally.ini [Video]/Driver,
 *                   LoadLibraryA("BRGlide.dll") or ("BRD3D.dll"),
 *                   GetProcAddress("RallyMain"), calls it
 *   BRGlide.dll     the game
 *
 * `RallyMain` is BRGlide.dll's ONLY export. Glide 0x1001CC00, 324 bytes.
 *
 * THE SPINE, all read off the disassembly rather than inferred:
 *
 *   0x1001CC00  RallyMain          CoInitialize, stash 4 args, parse, init,
 *                                  build the config path, create window, loop
 *   0x10019670  create the window  RegisterClassA + CreateWindowExA
 *                                  class "BossRally", title "Boss Rally",
 *                                  wndproc 0x100194C0
 *   0x10019730  the MAIN LOOP      ShowWindow/UpdateWindow/SetFocus, then
 *                                  PeekMessage -> GetMessage -> Translate ->
 *                                  Dispatch, and on idle the frame tick
 *   0x1001CF80  the frame tick     two instructions: jmp [0x100A9900 + s*4]
 *
 * THE TOP-LEVEL STATE MACHINE lives in that jump table. It has exactly FIVE
 * entries -- entry 5 reads 0x73736F42, which is ASCII "Boss", i.e. the string
 * pool begins there, so the table's extent is established by the data rather
 * than assumed:
 *
 *   state 0  0x1001CD70   36 B  cold init; loads the MENU sfx bank
 *                               (0x1006C290(0)); -> state 4
 *   state 1  0x1001CDA0   16 B  -> state 2. Nothing else.
 *   state 2  0x1001CDB0   17 B  RUN. ++frame counter, call 0x1002E324,
 *                               return the continue flag at 0x100A98F8
 *   state 3  0x1001CDD0   67 B  loading screen: "loading.img"; -> state 1
 *   state 4  0x1001CE20  348 B  video mode (640x480) and renderer init;
 *                               -> state 3
 *
 * So the running order is 0 -> 4 -> 3 -> 1 -> 2, and state 2 is where the
 * game actually lives. A handler returns 0 to quit and non-zero to continue,
 * which is exactly what the main loop tests.
 *
 * WHAT IS AND IS NOT TRANSCRIBED HERE
 *
 * The spine above is transcribed. Its callees are the frontier of the port
 * and are DECLARED, not stood in for -- the distinction matters and this
 * project got it wrong before. A declared callee that is not yet transcribed
 * is the edge of the work. A placeholder that returns a plausible value so
 * something visible happens is a lie with a counter attached, and this file
 * contains none.
 */
#ifndef BR_BOOT_H
#define BR_BOOT_H

#include <stdint.h>

/* ------------------------------------------------------------------ *
 * The four values BRally.exe passes in.
 *
 * RallyMain's prologue stores them at 0x105BC730..0x105BC73C. Their order is
 * established by the stack trace, not by assuming a WinMain signature:
 *
 *   entry            esp = R
 *   sub esp,8        esp = R-8
 *   push esi         esp = R-0xC
 *   push edi         esp = R-0x10
 *   [esp+0x14] = R+4   -> 0x105BC730   arg1
 *   [esp+0x18] = R+8   -> 0x105BC734   arg2
 *   [esp+0x1c] = R+0xC -> 0x105BC738   arg3
 *   [esp+0x20] = R+0x10-> 0x105BC73C   arg4
 *
 * arg1 is used as the HINSTANCE (it is handed to LoadIconA and
 * CreateWindowExA), arg3 is handed to 0x10007F40 as a string, and arg4 is
 * handed to ShowWindow as nCmdShow. That is a WinMain signature, confirmed by
 * use rather than by the name.
 * ------------------------------------------------------------------ */
typedef struct BrBootArgs {
    void       *hInstance;     /* 0x105BC730  arg1 */
    void       *hPrevInstance; /* 0x105BC734  arg2 -- stored, never read */
    const char *pszCmdLine;    /* 0x105BC738  arg3 */
    int32_t     nCmdShow;      /* 0x105BC73C  arg4 */
} BrBootArgs;

/* The five states of 0x100A9900, named for what their bodies do. */
enum {
    BR_APP_COLD_INIT = 0,   /* 0x1001CD70 */
    BR_APP_ENTER_RUN = 1,   /* 0x1001CDA0 */
    BR_APP_RUN       = 2,   /* 0x1001CDB0 */
    BR_APP_LOADING   = 3,   /* 0x1001CDD0 */
    BR_APP_SET_MODE  = 4,   /* 0x1001CE20 */
    BR_APP_NSTATES   = 5
};

/* 0x105CCBBC -- the current state, the jump table's index. */
extern int32_t g_brAppState;

/* 0x105CCBB8 -- incremented once per RUN tick. The game's frame counter. */
extern int32_t g_brAppFrame;

/* 0x105CCBC0 -- RallyMain's return value. Zeroed on entry, returned at exit,
 * and never written by anything in the spine, so it is whatever a deeper
 * callee last set. */
extern int32_t g_brAppExitCode;

/* 0x100A98F8 -- state 2 returns this. Zero quits the game. */
extern int32_t g_brAppContinue;

/* The default mode state 4 installs when nothing has chosen one: 640x480.
 * Written as 0x280/0x1E0 into six globals at 0x1001CE36. */
#define BR_APP_DEFAULT_W  640
#define BR_APP_DEFAULT_H  480

/* The mode state 4 installed. The original spreads each value over three
 * globals -- width to 0x105CCBB4 / 0x100A7514 / 0x106E7714, height to
 * 0x105CCBB0 / 0x100A7518 / 0x106E9A2C -- and the 0x100A75xx pair is what
 * CreateWindowExA reads at 0x100196EC, so this is the mode in the operative
 * sense, not a record of it. */
extern int32_t g_brAppModeW;
extern int32_t g_brAppModeH;

/* 0x1001CF80 -- one frame. Dispatches through the state table and returns the
 * handler's value: 0 to quit. */
int32_t BrAppFrame(void);

/* The five handlers, individually testable. */
int32_t BrAppStateColdInit(void);   /* 0x1001CD70 */
int32_t BrAppStateEnterRun(void);   /* 0x1001CDA0 */
int32_t BrAppStateRun(void);        /* 0x1001CDB0 */
int32_t BrAppStateLoading(void);    /* 0x1001CDD0 */
int32_t BrAppStateSetMode(void);    /* 0x1001CE20 */

/* Reset every global this module owns to its load-time value, so a test can
 * run the state machine more than once. Not in the original -- the original
 * gets fresh .data from the loader. */
void BrAppResetForTest(void);

#endif /* BR_BOOT_H */
