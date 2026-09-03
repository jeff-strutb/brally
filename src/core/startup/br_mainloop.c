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

/* @n64 0x80225ED8 located */
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
#ifdef BR_MATCHING_BUILD
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

extern void   *DAT_105bc72c;    /* the game window */
extern int32_t DAT_105bc73c;    /* nCmdShow, stashed by RallyMain */
extern int32_t DAT_1007b074;    /* renderer mode; 2 takes the extra hook */
extern int32_t DAT_105bc740;    /* the two ready flags and the suspend flag */
extern int32_t DAT_105bc744;
extern int32_t DAT_105bc748;

extern void BrWindowEarStartup(void *hWnd);   /* 0x10017E30 */
extern int32_t BrAppFrame(void);              /* 0x1001CF80 */

__declspec(dllimport) int   __stdcall ShowWindow(void *hWnd, int nCmdShow);
__declspec(dllimport) int   __stdcall UpdateWindow(void *hWnd);
__declspec(dllimport) void *__stdcall SetFocus(void *hWnd);
__declspec(dllimport) int   __stdcall PeekMessageA(BrMsg *pMsg, void *hWnd,
                                                   unsigned int nMin,
                                                   unsigned int nMax,
                                                   unsigned int nRemove);
__declspec(dllimport) int   __stdcall GetMessageA(BrMsg *pMsg, void *hWnd,
                                                  unsigned int nMin,
                                                  unsigned int nMax);
__declspec(dllimport) int   __stdcall TranslateMessage(const BrMsg *pMsg);
__declspec(dllimport) int32_t __stdcall DispatchMessageA(const BrMsg *pMsg);
__declspec(dllimport) int   __stdcall WaitMessage(void);

void BrMainLoopRun(void)
{
    BrMsg msg;

    ShowWindow(DAT_105bc72c, DAT_105bc73c);      /* 0x10019744 */
    UpdateWindow(DAT_105bc72c);                  /* 0x10019751 */
    SetFocus(DAT_105bc72c);                      /* 0x1001975D */

    if (DAT_1007b074 == 2) {                     /* 0x10019763 */
        BrWindowEarStartup(DAT_105bc72c);        /* 0x10019773, cdecl */
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
#else
int32_t BrMainLoopRun(const BrMainLoopOps *pOps)
{
    int32_t cFrames = 0;

    if (pOps == NULL ||
        pOps->pfnPeek == NULL || pOps->pfnGet == NULL ||
        pOps->pfnPump == NULL || pOps->pfnWait == NULL) {
        return 0;
    }

    for (;;) {
        /* 0x10019793: PeekMessage(&msg, 0, 0, 0, PM_NOREMOVE). */
        if (pOps->pfnPeek(pOps->pUser)) {
            /* 0x100197A6: GetMessage; 0 means WM_QUIT, and the loop ends
             * WITHOUT running a frame. */
            if (!pOps->pfnGet(pOps->pUser)) {
                break;
            }
            /* 0x100197B7 / 0x100197C2: Translate then Dispatch, always as a
             * pair, then straight back to the top -- messages are drained
             * before any frame runs. */
            pOps->pfnPump(pOps->pUser);
            continue;
        }

        /* Queue empty. 0x100197C7: run a frame only if the gate is open. */
        if (BrMainLoopFrameAllowed()) {
            if (BrAppFrame() == 0) {     /* 0x100197E2 -> 0x1001CF80 */
                break;                   /* 0 quits */
            }
            ++cFrames;
            continue;
        }

        /* 0x100197ED: BLOCK. The original does not spin here, and a port that
         * did would burn a core doing nothing the original does. */
        pOps->pfnWait(pOps->pUser);
    }

    return cFrames;
}
#endif
