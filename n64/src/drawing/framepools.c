/* framepools.c -- per-frame pools of matrices, lights and viewports for the display list
 */
#include "tgr/common.h"

/* -- declarations -- */
extern int D_8028DFB0;
extern int D_8028DFB4;
extern int D_8028DFB8;
/* -- end declarations -- */

/* WHAT IT DOES: Empty the three per-frame pools at the start of a frame:
 * matrices (256 per frame), lights (20) and viewports (20), which the
 * display-list builders hand out one at a time. */
/* @implements 0x80255E64 tgr BrFramePoolsReset */
void BrFramePoolsReset(void)
{
  D_8028DFB0 = 0;
  D_8028DFB4 = 0;
  D_8028DFB8 = 0;
}
