/* segment.c -- 2-D segment tests
 */
#include "tgr/common.h"

/* -- declarations -- */

/* -- end declarations -- */

/* WHAT IT DOES: Tell whether two line segments overlap on the ground plane:
 * a bounding-box rejection on x and y, then a side-of-line test of each
 * segment's ends against the other.
 * Each bounding test is the larger of one segment's ends against the
 * smaller of the other's, through two locals, then the four side-of-line
 * products; the second product of each pair names the shared edge last. */
/* @implements 0x80225B64 tgr BrSegmentsOverlapXY */
int BrSegmentsOverlapXY(float *a, float *b, float *c, float *d)
{
  float mx;
  float mn;
  float d1;
  float d2;
  float d3;
  float d4;

  mx = a[0] > b[0] ? a[0] : b[0];
  mn = c[0] < d[0] ? c[0] : d[0];
  if (mx < mn) {
    return 0;
  }
  mx = c[0] > d[0] ? c[0] : d[0];
  mn = a[0] < b[0] ? a[0] : b[0];
  if (mx < mn) {
    return 0;
  }
  mx = a[1] > b[1] ? a[1] : b[1];
  mn = c[1] < d[1] ? c[1] : d[1];
  if (mx < mn) {
    return 0;
  }
  mx = c[1] > d[1] ? c[1] : d[1];
  mn = a[1] < b[1] ? a[1] : b[1];
  if (mx < mn) {
    return 0;
  }
  d1 = (c[0] - a[0]) * (b[1] - a[1]) - (c[1] - a[1]) * (b[0] - a[0]);
  d2 = (d[0] - a[0]) * (b[1] - a[1]) - (d[1] - a[1]) * (b[0] - a[0]);
  if (d1 != 0.0f && d2 != 0.0f && d1 * d2 > 0.0f) {
    return 0;
  }
  d3 = (a[0] - c[0]) * (d[1] - c[1]) - (a[1] - c[1]) * (d[0] - c[0]);
  d4 = (b[0] - c[0]) * (d[1] - c[1]) - (b[1] - c[1]) * (d[0] - c[0]);
  if (d3 != 0.0f && d4 != 0.0f && d3 * d4 > 0.0f) {
    return 0;
  }
  if (d1 == d2) {
    return 2;
  }
  return 1;
}

/* WHAT IT DOES: Which side of the line a->b the points c and d fall on:
 * 0 when both lie strictly on the same side, 2 when the two side values
 * are equal (both on the line, or collinear), 1 otherwise.  The PC twin
 * is BrSeg2SideTest. */
/* @implements 0x80225E1C tgr BrSeg2SideTest */
int BrSeg2SideTest(float *a, float *b, float *c, float *d)
{
  float d1 = (c[0] - a[0]) * (b[1] - a[1]) - (c[1] - a[1]) * (b[0] - a[0]);
  float d2 = (d[0] - a[0]) * (b[1] - a[1]) - (d[1] - a[1]) * (b[0] - a[0]);

  if (d1 != 0.0f && d2 != 0.0f && d1 * d2 > 0.0f) {
    return 0;
  }
  if (d1 - d2 == 0.0f) {
    return 2;
  }
  return 1;
}
