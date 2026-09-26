/* framepools.c -- per-frame pools of matrices, lights and viewports for the display list
 */
#include "tgr/common.h"

/* -- declarations -- */
extern int D_8028DFB0;
extern int D_8028DFB4;
extern int D_8028DFB8;
extern int D_8028A85C;
extern int D_8028C75C;
extern int D_8028C760;
extern int D_8028C764;
extern int D_8028C768;
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

/* WHAT IT DOES: Point the vertex pools for this frame buffer back at their
 * start. */
/* @t4-pass 0x80233FDC 1 2026-09-26 compiles 17 best 21 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80233FDC 2 2026-09-26 compiles 16 best 21 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80233FDC 3 2026-09-26 compiles 15 best 21 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x80233FDC tgr BrVtxPoolsReset */
void BrVtxPoolsReset(void)
{
  D_8028C75C = D_8028A85C * 4000 + -0x7fcaca80;
  D_8028C760 = D_8028C75C;
  D_8028C764 = D_8028A85C * 16000 + -0x7fcaab40;
  D_8028C768 = D_8028C764;
}

/* WHAT IT DOES: Hand out the next matrix slot of this frame's pool (256
 * slots); past the end it keeps returning the last one. */
/* @t4-pass 0x80255CD0 1 2026-09-26 compiles 17 best 24 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80255CD0 2 2026-09-26 compiles 16 best 24 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80255CD0 3 2026-09-26 compiles 16 best 24 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x80255CD0 tgr BrMtxAlloc */
int BrMtxAlloc(void)
{
  int iVar1;
  
  if (D_8028DFB0 < 0x100) {
    iVar1 = D_8028DFB0 * 0x40;
    D_8028DFB0 = D_8028DFB0 + 1;
    return D_8028A85C * 0x4040 + iVar1 + -0x7fc951b0;
  }
  D_8028DFB0 = D_8028DFB0 + 1;
  return D_8028A85C * 0x4040 + -0x7fc911b0;
}

/* WHAT IT DOES: Hand out the next light slot of this frame's pool (20
 * slots); past the end it keeps returning the last one. */
/* @t4-pass 0x80255D4C 1 2026-09-26 compiles 17 best 28 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80255D4C 2 2026-09-26 compiles 16 best 28 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80255D4C 3 2026-09-26 compiles 16 best 28 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x80255D4C tgr BrLightAlloc */
int BrLightAlloc(void)
{
  int iVar1;
  
  if (D_8028DFB4 < 0x14) {
    iVar1 = D_8028DFB4 * 0x10;
    D_8028DFB4 = D_8028DFB4 + 1;
    return D_8028A85C * 0x150 + iVar1 + -0x7fc8d130;
  }
  D_8028DFB4 = D_8028DFB4 + 1;
  return D_8028A85C * 0x150 + -0x7fc8cff0;
}

/* WHAT IT DOES: Hand out the next viewport slot of this frame's pool (20
 * slots); past the end it keeps returning the last one. */
/* @t4-pass 0x80255DD8 1 2026-09-26 compiles 17 best 28 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80255DD8 2 2026-09-26 compiles 16 best 28 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80255DD8 3 2026-09-26 compiles 16 best 28 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x80255DD8 tgr BrVpAlloc */
int BrVpAlloc(void)
{
  int iVar1;
  
  if (D_8028DFB8 < 0x14) {
    iVar1 = D_8028DFB8 * 0x20;
    D_8028DFB8 = D_8028DFB8 + 1;
    return D_8028A85C * 0x2a0 + iVar1 + -0x7fc8ce90;
  }
  D_8028DFB8 = D_8028DFB8 + 1;
  return D_8028A85C * 0x2a0 + -0x7fc8cc10;
}
