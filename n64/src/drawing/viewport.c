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

/* WHAT IT DOES: Take the next viewport in the ring and set it to a w by h
 * view at (x, y) (320-wide coordinates, doubled on a hi-res screen),
 * optionally scissoring to it.  A negative width mirrors the view (the
 * mirror flag is kept, and a second flag flips the sign again); load it and
 * keep its depth scale and offset.
 * RESIDUE: h gets a callee-saved register across the scissor call; the ROM
 * keeps h in its argument home like x and y (w alone is in s0). */
/* @implements 0x80219BF0 tgr BrViewportSet */
void BrViewportSet(int x, int y, int w, int h, int scissor)
{
  Vp *vp;

  D_8028AB60 = (D_8028AB60 + 1) & 0x1f;
  if (scissor != 0) {
    BrScissorSet(x, y, w < 0 ? -w : w, h);
  }
  if (D_8028A850 != 0) {
    x *= 2;
    y *= 2;
    w *= 2;
    h *= 2;
  }
  if (w < 0) {
    if (D_8028A8AC != 0) {
      (vp = &D_80318CD0[D_8028AB60])->vp.vscale[0] = w * -2;
    } else {
      (vp = &D_80318CD0[D_8028AB60])->vp.vscale[0] = w * 2;
    }
    D_8028A8A8 = 1;
    w = -w;
  } else {
    if (D_8028A8AC != 0) {
      (vp = &D_80318CD0[D_8028AB60])->vp.vscale[0] = w * -2;
    } else {
      (vp = &D_80318CD0[D_8028AB60])->vp.vscale[0] = w * 2;
    }
    D_8028A8A8 = 0;
  }
  vp->vp.vscale[1] = h * 2;
  vp->vp.vscale[2] = 0x1ff;
  vp->vp.vscale[3] = 0;
  vp->vp.vtrans[0] = (x * 2 + w) * 2;
  vp->vp.vtrans[1] = (y * 2 + h) * 2;
  vp->vp.vtrans[2] = 0x1ff;
  vp->vp.vtrans[3] = 0;
  gSPViewport(D_8028A858++, &D_80318CD0[D_8028AB60]);
  D_8028AB64 = D_80318CD0[D_8028AB60].vp.vscale[2];
  D_8028AB68 = D_80318CD0[D_8028AB60].vp.vtrans[2];
}

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
