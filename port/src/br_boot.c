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

#include <stddef.h>

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
int32_t BrAppStateRun(void)
{
    ++g_brAppFrame;
    BrBootFrontier_1002E324();
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
int32_t BrAppStateLoading(void)
{
    BrBootFrontier_10063970(3,
                            BrBootGlobal_B71A48(),
                            BrBootGlobal_B71A4C(),
                            BrBootGlobal_B71A50(),
                            BrBootGlobal_B71A54());
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

void BrAppResetForTest(void)
{
    g_brAppState    = BR_APP_COLD_INIT;
    g_brAppFrame    = 0;
    g_brAppExitCode = 0;
    g_brAppContinue = 1;
    g_brAppModeW    = 0;
    g_brAppModeH    = 0;
    BrBootFrontierReset();
}
