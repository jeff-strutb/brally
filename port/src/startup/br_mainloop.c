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

/* @implements 0x10019730 glide BrMainLoopRun */
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
