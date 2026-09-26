/* trackgrid.c -- Top Gear Rally (N64).
 */
#include "tgr/common.h"

/* -- declarations -- */

/* -- end declarations -- */

/* WHAT IT DOES: Reset a track-cell search record: no position yet and a
 * unit scale. */
/* @implements 0x8021EB30 tgr BrTrackCellReset */
void BrTrackCellReset(int param_1)
{
  *(int *)(param_1 + 0x2c) = 0;
  *(int *)(param_1 + 0x1c) = 0;
  *(int *)(param_1 + 0xc) = 0;
  *(int *)(param_1 + 0x3c) = 0x3f800000;
}

/* WHAT IT DOES: Look up the track's 64x64 grid cell (x, y): returns the
 * cell's first entry index in the low half and its entry count in the high
 * half, or 0 outside the grid. */
/* @implements 0x8021EA0C tgr BrTrackGridCell */
unsigned int BrTrackGridCell(unsigned int param_1,unsigned int param_2)
{
  unsigned int uVar1;
  int iVar2;
  
  if ((((-1 < (int)param_1) && ((int)param_1 < 0x40)) && (-1 < (int)param_2)) &&
     ((int)param_2 < 0x40)) {
    iVar2 = (param_1 & 0xff) + (param_2 & 0xff) * 0x40;
    uVar1 = (unsigned int)*(unsigned short *)((*(int *)0x80025C6C) + iVar2 * 2);
    return (*(unsigned short *)((*(int *)0x80025C6C) + (iVar2 + 1) * 2) - uVar1) * 0x10000 | uVar1;
  }
  return 0;
}
