/* br_mainloop.c -- see br_mainloop.h. 0x10019730's loop. */
#include "br_mainloop.h"
#include "br_boot.h"

#include <stddef.h>

/* 0x105BC740, 0x105BC744, 0x105BC748. Zero at load. */
static int s_fReady1;
static int s_fReady2;
static int s_fSuspended;

/* (port-only BrMainLoopSetReady removed) */


/* @n64 0x8022F5D0 located */
/* (port-only BrMainLoopSetSuspended removed) */


/* (port-only BrMainLoopReady1 removed) */

/* (port-only BrMainLoopReady2 removed) */

/* (port-only BrMainLoopSuspended removed) */


/* The gate, spelled out to match the original's three separate tests rather
 * than folded into one expression -- 0x105BC748 is tested with `jne` where the
 * other two use `je`, and that asymmetry is the whole content of the gate. */
/* (port-only BrMainLoopFrameAllowed removed) */


/* WHAT IT DOES: the message loop: the outer loop the game sits in from
 * startup to shutdown. Windows messages are drained first and completely,
 * and only when the queue is empty does a frame get to run -- and then only
 * if three separate readiness flags allow it. The loop ends when the window
 * closes or a frame reports that the game should stop. */
/* @implements 0x10019730 glide BrMainLoopRun */
/* The port's ops table turned four direct USER32 calls into `call [ops+N]`,
 * and the whole window bring-up in front of the loop -- ShowWindow,
 * UpdateWindow, SetFocus and the mode-2 gate -- had never been transcribed.
 * The gate helper above is a real call here too; the original tests the three
 * globals inline, so this arm spells them out.
 *
 * MSG is the frame: `sub esp,0x1c` is exactly one of them, and the three
 * `lea` displacements (0x1c / 0x18 / 0x10 at three different esp values) all
 * resolve to it. */
typedef BrOsMsg BrMsg;      /* the original's MSG, 0x1C bytes there */

/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* the game window */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* nCmdShow, stashed by RallyMain */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* renderer mode; 2 takes the extra hook */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* the two ready flags and the suspend flag */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* BrWindowEarStartup: prototype in br_funcs.h */
/* BrAppFrame: prototype in br_funcs.h */

/* 64-bit core: ShowWindow is declared by the platform headers */
/* 64-bit core: UpdateWindow is declared by the platform headers */
/* 64-bit core: SetFocus is declared by the platform headers */
/* 64-bit core: PeekMessageA is declared by the platform headers */
/* 64-bit core: GetMessageA is declared by the platform headers */
/* 64-bit core: TranslateMessage is declared by the platform headers */
/* 64-bit core: DispatchMessageA is declared by the platform headers */
/* 64-bit core: WaitMessage is declared by the platform headers */

void BrMainLoopRun(void)
{
    BrMsg msg;

    br_os_window_show((*(void * *)&g_brOwner5BC72C), (*(int32_t *)&s_args.nCmdShow));      /* 0x10019744 */

    if ((*(int32_t *)&DAT_1007b074) == 2) {                     /* 0x10019763 */
        BrWindowEarStartup((*(void * *)&g_brOwner5BC72C));        /* 0x10019773, cdecl */
    }

    for (;;) {
        /* 0x10019793: the queue is only peeked here. */
        if (br_os_peek(&msg) != 0) {
            /* 0x100197B1: zero is the quit message, and the loop ends
             * without running a frame. */
            if (br_os_get(&msg) == 0) {
                return;
            }
            br_os_dispatch(&msg);              /* 0x100197C3 */
            continue;
        }

        /* 0x100197C7 -- three separate tests, and the third is INVERTED. */
        if (DAT_105bc740 != 0 && DAT_105bc744 != 0 && DAT_105bc748 == 0) {
            if (BrAppFrame() == 0) {             /* 0x100197E2 */
                return;
            }
            continue;
        }

        br_os_wait_message();                           /* 0x100197ED -- BLOCK */
    }
}
