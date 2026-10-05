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
extern int D_8034E7B0[64];
extern int D_8034E8B0[64];
typedef struct BrFrustum {
  float eye[3];                 /* 0x00 */
  float corner[4][3];           /* 0x0C */
  float centre[3];              /* 0x3C  the far plane's centre */
} BrFrustum;
extern BrFrustum D_8031B1F0;
int BrFloatToInt(float f);
int BrGridSpanHas(int x, int y);
void BrGridSpanEdge(float x0, float z0, float x1, float z1);
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

/* WHAT IT DOES: Work out the grid region the race camera sees: clear it,
 * add the frustum's edges (eye to each far corner, far centre to each
 * corner, and round the far rectangle), clamp the covered ranges to the
 * grid, then for each covered column find the first and last rows that
 * reach it. */
/* @implements 0x8022D49C tgr BrGridSpanFrustum */
void BrGridSpanFrustum(void)
{
  int i;
  int c;
  int r0;
  int r1;

  D_8028BD98 = 0;
  D_8028BD9C = 0;
  D_8028BD90 = 63;
  D_8028BD94 = 63;
  for (i = 0; i < 64; i++) {
    D_8034E5B0[i] = 64;
    D_8034E6B0[i] = 0;
    D_8034E7B0[i] = 64;
    D_8034E8B0[i] = 0;
  }
  BrGridSpanEdge(D_8031B1F0.eye[0], D_8031B1F0.eye[1], D_8031B1F0.corner[0][0], D_8031B1F0.corner[0][1]);
  BrGridSpanEdge(D_8031B1F0.eye[0], D_8031B1F0.eye[1], D_8031B1F0.corner[1][0], D_8031B1F0.corner[1][1]);
  BrGridSpanEdge(D_8031B1F0.eye[0], D_8031B1F0.eye[1], D_8031B1F0.corner[2][0], D_8031B1F0.corner[2][1]);
  BrGridSpanEdge(D_8031B1F0.eye[0], D_8031B1F0.eye[1], D_8031B1F0.corner[3][0], D_8031B1F0.corner[3][1]);
  BrGridSpanEdge(D_8031B1F0.centre[0], D_8031B1F0.centre[1], D_8031B1F0.corner[0][0], D_8031B1F0.corner[0][1]);
  BrGridSpanEdge(D_8031B1F0.centre[0], D_8031B1F0.centre[1], D_8031B1F0.corner[1][0], D_8031B1F0.corner[1][1]);
  BrGridSpanEdge(D_8031B1F0.centre[0], D_8031B1F0.centre[1], D_8031B1F0.corner[2][0], D_8031B1F0.corner[2][1]);
  BrGridSpanEdge(D_8031B1F0.centre[0], D_8031B1F0.centre[1], D_8031B1F0.corner[3][0], D_8031B1F0.corner[3][1]);
  BrGridSpanEdge(D_8031B1F0.corner[0][0], D_8031B1F0.corner[0][1], D_8031B1F0.corner[1][0], D_8031B1F0.corner[1][1]);
  BrGridSpanEdge(D_8031B1F0.corner[1][0], D_8031B1F0.corner[1][1], D_8031B1F0.corner[2][0], D_8031B1F0.corner[2][1]);
  BrGridSpanEdge(D_8031B1F0.corner[2][0], D_8031B1F0.corner[2][1], D_8031B1F0.corner[3][0], D_8031B1F0.corner[3][1]);
  BrGridSpanEdge(D_8031B1F0.corner[3][0], D_8031B1F0.corner[3][1], D_8031B1F0.corner[0][0], D_8031B1F0.corner[0][1]);
  if (D_8028BD90 < 0) {
    D_8028BD90 = 0;
  }
  if (D_8028BD94 < 0) {
    D_8028BD94 = 0;
  }
  if (D_8028BD98 > 63) {
    D_8028BD98 = 63;
  }
  if (D_8028BD9C > 63) {
    D_8028BD9C = 63;
  }
  for (c = D_8028BD90; c <= D_8028BD98; c++) {
    for (r0 = D_8028BD94, r1 = D_8028BD9C; c < D_8034E5B0[r0] || c > D_8034E6B0[r0]; r0++) {
    }
    for (; c < D_8034E5B0[r1] || c > D_8034E6B0[r1]; r1--) {
    }
    D_8034E7B0[c] = r0;
    D_8034E8B0[c] = r1;
  }
}
