/* nodepass.c -- walking the scene tree
 */
#include "tgr/common.h"

/* -- declarations -- */
void func_80255BA0(int *param_1);
void func_80255C50(int *param_1);
extern int D_80025C70;
/* -- end declarations -- */

/* WHAT IT DOES: Walk the scene tree once, stamping each node and applying
 * the per-mode fix-up, then walk it again taking the stamps off so the tree
 * is ready for the next pass. */
/* @implements 0x80255CA0 tgr BrScenePassRun */
void BrScenePassRun(void)
{
  func_80255BA0(D_80025C70);
  func_80255C50(D_80025C70);
}
