/* trackgrid.c -- Top Gear Rally (N64).
 */
#include "tgr/common.h"

/* -- declarations -- */

extern unsigned short *D_80025C6C;
/* -- end declarations -- */

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

