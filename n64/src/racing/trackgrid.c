/* trackgrid.c -- Top Gear Rally (N64).
 */
#include "tgr/common.h"

/* -- declarations -- */

extern unsigned short *D_80025C6C;
/* -- end declarations -- */

/* WHAT IT DOES: Reset a track-cell search record: no position yet and a
 * unit scale. */
/* @t4-pass 0x8021EB30 1 2026-09-26 compiles 17 best 8 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8021EB30 2 2026-09-26 compiles 16 best 8 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8021EB30 3 2026-09-26 compiles 13 best 8 moved 0  (n64/tools/n64permute.py) */
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
unsigned int BrTrackGridCell(int x, int y)
{
  unsigned short i;
  unsigned int first;

  if (x < 0 || x >= 64 || y < 0 || y >= 64) {
    return 0;
  }
  i = (x & 0xff) + (y & 0xff) * 64;
  first = D_80025C6C[i];
  return (D_80025C6C[(unsigned short)(i + 1)] - first) << 16 | first;
}

