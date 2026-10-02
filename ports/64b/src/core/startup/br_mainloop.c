/* br_mainloop.c -- see br_mainloop.h. 0x10019730's loop. */
#include "br_mainloop.h"
#include "br_boot.h"

#include <stddef.h>

/* 0x105BC740, 0x105BC744, 0x105BC748. Zero at load. */
static int s_fReady1;
static int s_fReady2;
static int s_fSuspended;

void BrMainLoopSetReady(int fReady1, int fReady2)
{
    s_fReady1 = (fReady1 != 0);
    s_fReady2 = (fReady2 != 0);
}

/* @n64 0x8022F5D0 located */
void BrMainLoopSetSuspended(int fSuspended)
{
    s_fSuspended = (fSuspended != 0);
}

int BrMainLoopReady1(void)    { return s_fReady1; }
int BrMainLoopReady2(void)    { return s_fReady2; }
int BrMainLoopSuspended(void) { return s_fSuspended; }

/* The gate, spelled out to match the original's three separate tests rather
 * than folded into one expression -- 0x105BC748 is tested with `jne` where the
 * other two use `je`, and that asymmetry is the whole content of the gate. */
int BrMainLoopFrameAllowed(void)
{
    if (!s_fReady1)   return 0;   /* 0x100197C7 je  */
    if (!s_fReady2)   return 0;   /* 0x100197D0 je  */
    if (s_fSuspended) return 0;   /* 0x100197D9 jne -- INVERTED */
    return 1;
}

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
typedef struct BrPoint { int32_t x, y; } BrPoint;
typedef struct BrMsg {
    void    *hwnd;          /* +0x00 */
    uint32_t message;       /* +0x04 */
    uint32_t wParam;        /* +0x08 */
    int32_t  lParam;        /* +0x0C */
    uint32_t time;          /* +0x10 */
    BrPoint  pt;            /* +0x14 */
} BrMsg;                    /* 0x1C */

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

    ShowWindow((*(void * *)&g_brOwner5BC72C), (*(int32_t *)((char *)&s_args + 0xC)) /* BR_LP64_BYTE_VIEW */);      /* 0x10019744 */
    UpdateWindow((*(void * *)&g_brOwner5BC72C));                  /* 0x10019751 */
    SetFocus((*(void * *)&g_brOwner5BC72C));                      /* 0x1001975D */

    if ((*(int32_t *)&DAT_1007b074) == 2) {                     /* 0x10019763 */
        BrWindowEarStartup((*(void * *)&g_brOwner5BC72C));        /* 0x10019773, cdecl */
    }

    for (;;) {
        /* 0x10019793: PM_NOREMOVE -- the queue is only peeked here. */
        if (PeekMessageA(&msg, NULL, 0, 0, 0) != 0) {
            /* 0x100197B1: zero is WM_QUIT, and the loop ends without
             * running a frame. */
            if (GetMessageA(&msg, NULL, 0, 0) == 0) {
                return;
            }
            TranslateMessage(&msg);              /* 0x100197BC */
            DispatchMessageA(&msg);              /* 0x100197C3 */
            continue;
        }

        /* 0x100197C7 -- three separate tests, and the third is INVERTED. */
        if (DAT_105bc740 != 0 && DAT_105bc744 != 0 && DAT_105bc748 == 0) {
            if (BrAppFrame() == 0) {             /* 0x100197E2 */
                return;
            }
            continue;
        }

        WaitMessage();                           /* 0x100197ED -- BLOCK */
    }
}
