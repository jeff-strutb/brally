/* cheats.c -- the debug-menu toggles and unlock cheats (reached only through
 * the debug menu's table)
 */
#include "tgr/common.h"
#include "tgr/car.h"

/* -- declarations -- */
extern int D_8028AA94;
extern int D_8026FF20;
extern int D_8028AA68;
/* -- end declarations -- */

/* WHAT IT DOES: Flip the debug flag at 0x8028AA94 on or off. */
/* @implements 0x80254F40 tgr BrCheatToggleAA94 */
void BrCheatToggleAA94(void)
{
  D_8028AA94 = !D_8028AA94;
}

/* WHAT IT DOES: Cheat: set bits 0x1F of player 1's season record's second unlock mask. */
/* @implements 0x80254F58 tgr BrCheatUnlockCE1F */
void BrCheatUnlockCE1F(void)
{
  D_8031B760[0].season->xce |= 0x1f;
}

/* WHAT IT DOES: Cheat: set bits 0x1FF of player 1's season record's unlock mask. */
/* @implements 0x80254F70 tgr BrCheatUnlockCars */
void BrCheatUnlockCars(void)
{
  D_8031B760[0].season->unlocked |= 0x1ff;
}

/* WHAT IT DOES: Cheat: set bits 0x200 of player 1's season record's unlock mask. */
/* @implements 0x80254F88 tgr BrCheatUnlock0200 */
void BrCheatUnlock0200(void)
{
  D_8031B760[0].season->unlocked |= 0x200;
}

/* WHAT IT DOES: Cheat: set bits 0x1000 of player 1's season record's unlock mask. */
/* @implements 0x80254FA0 tgr BrCheatUnlock1000 */
void BrCheatUnlock1000(void)
{
  D_8031B760[0].season->unlocked |= 0x1000;
}

/* WHAT IT DOES: Cheat: set bits 0x800 of player 1's season record's unlock mask. */
/* @implements 0x80254FB8 tgr BrCheatUnlock0800 */
void BrCheatUnlock0800(void)
{
  D_8031B760[0].season->unlocked |= 0x800;
}

/* WHAT IT DOES: Cheat: set bits 0x400 of player 1's season record's unlock mask. */
/* @implements 0x80254FD0 tgr BrCheatUnlock0400 */
void BrCheatUnlock0400(void)
{
  D_8031B760[0].season->unlocked |= 0x400;
}

/* WHAT IT DOES: Cheat: set bits 0x3E0 of player 1's season record's second unlock mask. */
/* @implements 0x80254FE8 tgr BrCheatUnlockCE3E0 */
void BrCheatUnlockCE3E0(void)
{
  D_8031B760[0].season->xce |= 0x3e0;
}

/* WHAT IT DOES: Cheat: set bits 0x8000 of player 1's season record's unlock mask. */
/* @implements 0x80255000 tgr BrCheatUnlock8000 */
void BrCheatUnlock8000(void)
{
  D_8031B760[0].season->unlocked |= 0x8000;
}

/* WHAT IT DOES: Flip the debug flag at 0x8026FF20 on or off. */
/* @implements 0x80255018 tgr BrCheatToggleFF20 */
void BrCheatToggleFF20(void)
{
  D_8026FF20 = !D_8026FF20;
}

/* WHAT IT DOES: Flip the debug flag at 0x8028AA68 on or off. */
/* @implements 0x80255030 tgr BrCheatToggleAA68 */
void BrCheatToggleAA68(void)
{
  D_8028AA68 = !D_8028AA68;
}
