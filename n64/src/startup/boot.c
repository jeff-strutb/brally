/* boot.c -- power-on entry and fatal errors
 */
#include "tgr/common.h"

/* -- declarations -- */
void func_8021E2C8(int param_1);
int func_80265670();
void func_802657C0(int param_1);
void func_80265D90(void);
extern int D_80272680;
extern int D_80316CD0;
void func_8021E1EC();
extern int D_8028A88C;
/* -- end declarations -- */

/* WHAT IT DOES: The game's entry point after the boot stub clears memory:
 * initialise the N64 operating system, then create and start the idle
 * thread that brings everything else up. */
/* @implements 0x8021E5C4 tgr BrBoot */
void BrBoot(void)
{
  func_80265D90();
  func_80265670(&D_80272680,1,func_8021E2C8,0,&D_80316CD0,10);
  func_802657C0(&D_80272680);
}

/* WHAT IT DOES: Stop the game with an error message: store the message for
 * the error screen and hand over to the halt loop. Used for out-of-memory
 * and overflow checks. */
/* @implements 0x8021E1F4 tgr BrFatal */
void BrFatal(int param_1)
{
  D_8028A88C = param_1;
  func_8021E1EC(0);
}
