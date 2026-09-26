/* nodepass.c -- walking the scene tree
 */
#include "tgr/common.h"

/* -- declarations -- */
void BrNodeMarkPass(int *param_1);
void BrNodeClearMarkPass(int *param_1);
extern int D_80025C70;
extern int D_8028B940;
/* -- end declarations -- */

/* WHAT IT DOES: Walk the scene tree once, stamping each node and applying
 * the per-mode fix-up, then walk it again taking the stamps off so the tree
 * is ready for the next pass. */
/* @implements 0x80255CA0 tgr BrScenePassRun */
void BrScenePassRun(void)
{
  BrNodeMarkPass(D_80025C70);
  BrNodeClearMarkPass(D_80025C70);
}

/* WHAT IT DOES: Walk the scene tree, stamping each node not yet visited
 * before descending into it; in the modes that need it the pass also clears
 * one per-node byte. */
/* @implements 0x80255BA0 tgr BrNodeMarkPass */
void BrNodeMarkPass(int *param_1)
{
  short uVar1;
  
  if (param_1 == (int *)0x0) {
    return;
  }
  uVar1 = *(short *)((int)param_1 + 0x16);
  do {
    if ((uVar1 & 0x8000) == 0) {
      if ((uVar1 & 1) == 0) {
        *(short *)((int)param_1 + 0x16) = uVar1 | 0x8000;
        if (*(char *)((int)param_1 + 0x11) == '\x02') {
          if (D_8028B940 == 3) {
            *(char *)((int)param_1 + 0x11) = 0;
          }
          else if (D_8028B940 == 8) {
            *(char *)((int)param_1 + 0x11) = 0;
          }
        }
        BrNodeMarkPass(*param_1);
        goto LAB_80255c24;
      }
      param_1 = (int *)param_1[1];
    }
    else {
LAB_80255c24:
      param_1 = (int *)param_1[1];
    }
    if (param_1 == (int *)0x0) {
      return;
    }
    uVar1 = *(short *)((int)param_1 + 0x16);
  } while( 1 );
}

/* WHAT IT DOES: Walk the scene tree taking off every visit stamp the
 * marking pass left, ready for the next walk. */
/* @implements 0x80255C50 tgr BrNodeClearMarkPass */
void BrNodeClearMarkPass(int *param_1)
{
  short uVar1;
  
  if (param_1 != (int *)0x0) {
    uVar1 = *(short *)((int)param_1 + 0x16);
    while( 1 ) {
      if ((uVar1 & 0x8000) != 0) {
        *(short *)((int)param_1 + 0x16) = uVar1 & 0x7fff;
        BrNodeClearMarkPass(*param_1);
      }
      param_1 = (int *)param_1[1];
      if (param_1 == (int *)0x0) break;
      uVar1 = *(short *)((int)param_1 + 0x16);
    }
  }
}
