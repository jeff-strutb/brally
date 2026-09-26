/* boot.c -- power-on entry and fatal errors
 */
#include "tgr/common.h"

/* -- declarations -- */
void func_8021E2C8(int param_1);
int osCreateThread();
void osStartThread(int param_1);
void osInitialize(void);
extern int D_80272680;
extern int D_80316CD0;
void BrHaltLoop();
extern int D_8028A88C;
int osRecvMesg(int param_1,int *param_2,int param_3);
extern int D_8031A390;
/* -- end declarations -- */

/* WHAT IT DOES: The game's entry point after the boot stub clears memory:
 * initialise the N64 operating system, then create and start the idle
 * thread that brings everything else up. */
/* @implements 0x8021E5C4 tgr BrBoot */
void BrBoot(void)
{
  osInitialize();
  osCreateThread(&D_80272680,1,func_8021E2C8,0,&D_80316CD0,10);
  osStartThread(&D_80272680);
}

/* WHAT IT DOES: Stop the game with an error message: store the message for
 * the error screen and hand over to the halt loop. Used for out-of-memory
 * and overflow checks. */
/* @implements 0x8021E1F4 tgr BrFatal */
void BrFatal(int param_1)
{
  D_8028A88C = param_1;
  BrHaltLoop(0);
}

/* WHAT IT DOES: Wait for the next vertical retrace: block until the video
 * interrupt posts its message. */
/* @implements 0x8021E1C0 tgr BrWaitRetrace */
void BrWaitRetrace(void)
{
  osRecvMesg((&D_8031A390),0,1);
}

/* WHAT IT DOES: The end of the line after a fatal error: takes the error
 * code and returns straight away in the retail build (its body was compiled
 * out). */
/* @implements 0x8021E1EC tgr BrHaltLoop */
void BrHaltLoop(int arg0)
{
}
