/* trackgrid.c -- Top Gear Rally (N64).
 */
#include "tgr/common.h"

/* -- declarations -- */

#include "tgr/track.h"
#define D_80025C6C BEPTR(be16_t *, D_80025C00.x6c)   /* the loaded track's table (big-endian) */
/* -- end declarations -- */

/* WHAT IT DOES: BrTrackGridCell for a world position: the track grid cell
 * under (x, y) -- 32 units a cell, 0 to 2048 on each axis -- as its first
 * entry (low half) and entry count (high half); outside the grid, 0. */
/* @implements 0x8021E7DC tgr BrTrackGridCellAt */
unsigned int BrTrackGridCellAt(float x, float y)
{
  unsigned short i;
  int first;

  if (x < 0.0f || x >= 2048.0f || y < 0.0f || y >= 2048.0f) {
    return 0;
  }
  i = (unsigned char)tgr_f2u(x / 32.0f) + (unsigned char)tgr_f2u(y / 32.0f) * 64;
  first = BE16(D_80025C6C[i]);
  return (BE16(D_80025C6C[(unsigned short)(i + 1)]) - first) << 16 | first;
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
  first = BE16(D_80025C6C[i]);
  return (BE16(D_80025C6C[(unsigned short)(i + 1)]) - first) << 16 | first;
}

