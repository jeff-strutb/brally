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
extern int D_8028A850;
extern int D_8028A8A8;
extern int D_8028A8AC;
void BrScissorSet(int x, int y, int w, int h);
/* -- end declarations -- */

/* WHAT IT DOES: Turn a depth ratio (a / b) into the z-buffer's fixed-point
 * value through the current viewport's depth scale and offset. Nothing in
 * the ROM calls it. */
/* @implements 0x8023B130 tgr BrViewportDepthZ */
int BrViewportDepthZ(float a, float b)
{
  return ((a / b) * (float)D_8028AB64 + (float)D_8028AB68) * 32.0f;
}
