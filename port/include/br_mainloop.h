/* br_mainloop.h -- 0x10019730, the game's message loop.
 *
 * ARCHITECTURAL CONCERN: app / platform.
 *
 * WHAT IS GAME LOGIC HERE AND WHAT IS WIN32
 *
 * Most of this function is boilerplate -- ShowWindow, a PeekMessage /
 * GetMessage / TranslateMessage / DispatchMessage pump. Transcribing that
 * verbatim would port nothing worth having and would tie the result to Win32,
 * which the whole project exists to avoid.
 *
 * What IS worth transcribing, exactly, is the game's own decision about WHEN A
 * FRAME RUNS. That is four lines of the original and it is not obvious:
 *
 *     100197C7  mov  eax, [0x105BC740]
 *              test eax, eax
 *              je   WaitMessage
 *     100197D0  mov  eax, [0x105BC744]
 *              test eax, eax
 *              je   WaitMessage
 *     100197D9  mov  eax, [0x105BC748]
 *              test eax, eax
 *              jne  WaitMessage
 *     100197E2  call 0x1001CF80          ; the frame
 *              test eax, eax
 *              je   done                 ; 0 quits
 *              jmp  loop
 *
 * So: a frame runs only when the message queue is EMPTY and two flags are set
 * and a third is clear. Note the polarity -- 0x105BC748 is inverted relative
 * to the other two, so it is a "suspended" flag while the first two are
 * "ready" flags. Getting that backwards gives a game that runs only while
 * minimised, which is the kind of defect that looks like a hang.
 *
 * When the gate is closed the original calls WaitMessage and BLOCKS. It does
 * not spin. That matters for a port: a busy-wait here would burn a core and
 * would not be what the original does.
 *
 * The pump is therefore expressed against a small platform vtable, so the loop
 * itself -- the part that is the game's -- is testable with no window at all,
 * and a host supplies the four calls. The Win32 sequence is recorded above so
 * nothing is lost by not spelling it out in code.
 *
 * WHAT SETS THE THREE GATES
 *
 * The window procedure at 0x100194C0 (423 bytes, NOT PORTED). Until it is
 * transcribed the gates are whatever the host says they are, and the accessors
 * below are the single place that changes when it lands.
 */
#ifndef BR_MAINLOOP_H
#define BR_MAINLOOP_H

#include <stdint.h>

/* The four platform operations the loop needs. A host fills these in; a test
 * fills them with a script. Return values follow Win32's, because that is what
 * the original's control flow was written against:
 *
 *   pfnPeek   non-zero if a message is waiting (PM_NOREMOVE -- it does not
 *             consume; the original passes 0 for wRemoveMsg at 0x10019793)
 *   pfnGet    0 means WM_QUIT was dequeued -- the loop ends
 *   pfnPump   TranslateMessage + DispatchMessage, which the original always
 *             does as a pair and never separately
 *   pfnWait   block until something arrives (WaitMessage)
 */
typedef struct BrMainLoopOps {
    int  (*pfnPeek)(void *pUser);
    int  (*pfnGet)(void *pUser);
    void (*pfnPump)(void *pUser);
    void (*pfnWait)(void *pUser);
    void  *pUser;
} BrMainLoopOps;

/* The three gates at 0x105BC740 / 0x105BC744 / 0x105BC748. Set by the window
 * procedure in the original; settable here until it is ported. */
void BrMainLoopSetReady(int fReady1, int fReady2);   /* 0x105BC740, 0x105BC744 */
void BrMainLoopSetSuspended(int fSuspended);         /* 0x105BC748 -- INVERTED */

int  BrMainLoopReady1(void);
int  BrMainLoopReady2(void);
int  BrMainLoopSuspended(void);

/* True when a frame is allowed to run: both ready flags set AND not suspended.
 * Exposed separately from the loop so the polarity can be asserted directly. */
int  BrMainLoopFrameAllowed(void);

/* 0x10019730's loop body, from 0x10019793 to 0x100197F3.
 *
 * Runs until WM_QUIT is dequeued or the frame returns 0. Returns the number of
 * frames run, which the original does not -- it returns void -- but which
 * makes the loop observable without a window. */
int32_t BrMainLoopRun(const BrMainLoopOps *pOps);

#endif /* BR_MAINLOOP_H */
