/* gamemode.c -- the main loop's current game mode, and the one-shot hook the frame builder runs
 */
#include "tgr/common.h"

/* -- declarations -- */
extern int D_8031B31C;
extern int D_8031B320;
extern int D_8031B318;
/* -- end declarations -- */

/* WHAT IT DOES: Does nothing. An empty function the retail build kept
 * beside the game-mode switch. */
/* @implements 0x8021C6A8 tgr BrStub8021C6A8 */
void BrStub8021C6A8(void)

{
}

/* WHAT IT DOES: Does nothing. A second empty function beside the game-mode
 * switch. */
/* @implements 0x8021C6B0 tgr BrStub8021C6B0 */
void BrStub8021C6B0(void)

{
}

/* WHAT IT DOES: Register a function for the frame builder to call once
 * while it composes the next frame; the builder clears it after the call. */
/* @implements 0x8021C6B8 tgr BrFrameSetHook */
void BrFrameSetHook(int param_1)

{
  D_8031B31C = param_1;
}

/* WHAT IT DOES: Ask for the registered frame hook to be called again at the
 * very end of the frame, after the overlays are drawn. */
/* @implements 0x8021C6C4 tgr BrFrameSetHookLate */
void BrFrameSetHookLate(int param_1)

{
  D_8031B320 = param_1;
}

/* WHAT IT DOES: Tell whether the given function is the main loop's current
 * game mode. */
/* @implements 0x8021C6D0 tgr BrModeIs */
int BrModeIs(int param_1)

{
  return param_1 == D_8031B318;
}

/* WHAT IT DOES: Switch the main loop to a new game mode: the function it
 * calls once per frame (front end, race, replay and so on). */
/* @implements 0x8021C6E4 tgr BrModeSet */
void BrModeSet(int param_1)

{
  D_8031B318 = param_1;
}

/* WHAT IT DOES: Does nothing. An empty function the retail build kept after
 * the game-mode code. */
/* @implements 0x8021C740 tgr BrStub8021C740 */
void BrStub8021C740(void)

{
}

/* WHAT IT DOES: Always returns 0. A placeholder the retail build kept;
 * nothing in the ROM calls it directly. */
/* @implements 0x802173B8 tgr BrStub802173B8 */
int BrStub802173B8(int arg0,int arg1)
{
  return 0;
}
