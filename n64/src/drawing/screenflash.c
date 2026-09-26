/* screenflash.c -- the white screen flash drawn over the 3-D view
 */
#include "tgr/common.h"

/* -- declarations -- */
extern float D_8028B750;
/* -- end declarations -- */

/* WHAT IT DOES: Cancel the white screen flash: its strength goes back to
 * zero, so nothing is drawn over the view. */
/* @implements 0x80223470 tgr BrScreenFlashClear */
void BrScreenFlashClear(void)
{
  D_8028B750 = 0;
}

/* WHAT IT DOES: Does nothing. An empty function the retail build kept after
 * the screen-flash code. */
/* @implements 0x80223A68 tgr BrStub80223A68 */
void BrStub80223A68(void)
{
}
