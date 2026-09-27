/* viewport.c -- the viewport ring (32 Vp at 0x80318CD0, one taken per view)
 * and the depth scale it implies
 */
#include "tgr/common.h"
#include "tgr/gbi.h"

/* -- declarations -- */
extern Gfx *D_8028A858;
extern int D_8028AB60;
extern int D_8028AB64;
extern int D_8028AB68;
extern Vp D_80318CD0[32];
/* -- end declarations -- */

/* WHAT IT DOES: Load the current viewport into the display list and keep
 * its depth scale and offset for turning depths into z-buffer values. */
/* @implements 0x80219F04 tgr BrViewportApply */
void BrViewportApply(void)
{
  gSPViewport(D_8028A858++, &D_80318CD0[D_8028AB60]);
  D_8028AB64 = D_80318CD0[D_8028AB60].vp.vscale[2];
  D_8028AB68 = D_80318CD0[D_8028AB60].vp.vtrans[2];
}

/* WHAT IT DOES: Turn a depth ratio (a / b) into the z-buffer's fixed-point
 * value through the current viewport's depth scale and offset. Nothing in
 * the ROM calls it. */
/* @implements 0x8023B130 tgr BrViewportDepthZ */
int BrViewportDepthZ(float a, float b)
{
  return ((a / b) * (float)D_8028AB64 + (float)D_8028AB68) * 32.0f;
}
