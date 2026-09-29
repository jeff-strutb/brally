/* gridspan.c -- the 64x64 grid region: per-row leftmost and rightmost column
 * of a scan-converted area, and the rows it covers
 */
#include "tgr/common.h"

/* -- declarations -- */
extern int D_8028BD90;
extern int D_8028BD94;
extern int D_8028BD98;
extern int D_8028BD9C;
extern int D_8034E5B0[64];
extern int D_8034E6B0[64];
int BrFloatToInt(float f);
int BrGridSpanHas(int x, int y);
void BrGridSpanExtend(int x, int y);
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

/* WHAT IT DOES: Add the edge from world point (x0, z0) to (x1, z1) to the
 * grid region: both end cells go in, the covered columns and rows widen to
 * take them, and for each 32-unit row line the edge crosses, the cell it
 * crosses in (and the neighbour when it lies on a column line) is added to
 * the rows either side. */
/* @implements 0x8022D104 tgr BrGridSpanEdge */
void BrGridSpanEdge(float x0, float z0, float x1, float z1)
{
  int a;
  int b;
  int r;
  int c;
  float t;
  float x;

  BrGridSpanExtend(BrFloatToInt(x0 * 0.03125f), BrFloatToInt(z0 * 0.03125f));
  BrGridSpanExtend(BrFloatToInt(x1 * 0.03125f), BrFloatToInt(z1 * 0.03125f));
  a = BrFloatToInt(x0 * 0.03125f);
  b = BrFloatToInt(x1 * 0.03125f);
  if (b < a) {
    b ^= a ^= b ^= a;
  }
  if (a < D_8028BD90) {
    D_8028BD90 = a;
  }
  if (b > D_8028BD98) {
    D_8028BD98 = b;
  }
  if (z1 < z0) {
    t = x0;
    x0 = x1;
    x1 = t;
    t = z0;
    z0 = z1;
    z1 = t;
  }
  if (z1 == z0) {
    return;
  }
  a = BrFloatToInt(z0 * 0.03125f);
  b = BrFloatToInt(z1 * 0.03125f);
  if (a < D_8028BD94) {
    D_8028BD94 = a;
  }
  if (b > D_8028BD9C) {
    D_8028BD9C = b;
  }
  for (r = a; r <= b; r++) {
    t = r * 32.0f;
    if (z0 <= t && t <= z1) {
      x = (x1 - x0) * (t - z0) / (z1 - z0) + x0;
      c = BrFloatToInt(x * 0.03125f);
      BrGridSpanExtend(c, r - 1);
      BrGridSpanExtend(c, r);
      if (x <= c * 32.0f) {
        BrGridSpanExtend(c - 1, r - 1);
        BrGridSpanExtend(c - 1, r);
      }
      if ((c + 1) * 32.0f <= x) {
        BrGridSpanExtend(c + 1, r - 1);
        BrGridSpanExtend(c + 1, r);
      }
    }
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
