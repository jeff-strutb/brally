/* gridspan.c -- the 64x64 grid region: per-row leftmost and rightmost column
 * of a scan-converted area, and the rows it covers
 */
#include "tgr/common.h"

/* -- declarations -- */
extern int D_8028BD94;
extern int D_8028BD9C;
extern int D_8034E5B0[64];
extern int D_8034E6B0[64];
int BrFloatToInt(float f);
int BrGridSpanHas(int x, int y);
/* -- end declarations -- */

/* WHAT IT DOES: Widen row y of the 64x64 grid region to take in column x:
 * both are clamped to the grid, then the row's leftmost and rightmost
 * columns are pushed out to x if it lies beyond them. */
/* @implements 0x8022D090 tgr BrGridSpanExtend */
void BrGridSpanExtend(int x, int y)
{
  if (x < 0) {
    x = 0;
  }
  if (x > 63) {
    x = 63;
  }
  if (y < 0) {
    y = 0;
  }
  if (y > 63) {
    y = 63;
  }
  if (x < D_8034E5B0[y]) {
    D_8034E5B0[y] = x;
  }
  if (x > D_8034E6B0[y]) {
    D_8034E6B0[y] = x;
  }
}

/* WHAT IT DOES: Tell whether grid cell (x, y) lies inside the region: its
 * row is within the covered rows and x within that row's columns. */
/* @implements 0x8022D3D4 tgr BrGridSpanHas */
int BrGridSpanHas(int x, int y)
{
  return y >= D_8028BD94 && y <= D_8028BD9C && x >= D_8034E5B0[y] && x <= D_8034E6B0[y];
}

/* WHAT IT DOES: Tell whether a world point (x, z) falls in the grid region:
 * the coordinates are turned into cells (32 units each) and tested. */
/* @implements 0x8022D43C tgr BrGridSpanHasPoint */
int BrGridSpanHasPoint(float x, float z)
{
  return BrGridSpanHas(BrFloatToInt(x * 0.03125f), BrFloatToInt(z * 0.03125f));
}
