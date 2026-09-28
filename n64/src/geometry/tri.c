/* tri.c -- point-in-triangle tests
 */
#include "tgr/common.h"
#include "tgr/vec.h"

/* -- declarations -- */
void BrVec3Sub(BrVec3 *out, BrVec3 *a, BrVec3 *b);
void BrVec3Cross(BrVec3 *pOut, BrVec3 *pA, BrVec3 *pB);
float BrVec3Dot(BrVec3 *pA, BrVec3 *pB);
/* -- end declarations -- */

/* WHAT IT DOES: Tell whether a point lies inside triangle a, b, c as seen
 * along a reference normal: for each edge the cross product of the edge
 * with the point's offset must not face away from the normal.  The PC twin
 * is BrTriContainsPoint. */
/* @implements 0x8022591C tgr BrTriContainsPoint */
int BrTriContainsPoint(BrVec3 *pPt, BrVec3 *pA, BrVec3 *pB, BrVec3 *pC, BrVec3 *pRef)
{
  BrVec3 toPtA, toPtB, edge1, edge2, edge3, n;

  BrVec3Sub(&edge1, pB, pA);
  BrVec3Sub(&toPtB, pPt, pB);
  BrVec3Cross(&n, &edge1, &toPtB);
  if (BrVec3Dot(&n, pRef) < 0.0f) {
    return 0;
  }
  BrVec3Sub(&edge2, pC, pB);
  BrVec3Cross(&n, &edge2, &toPtB);
  if (BrVec3Dot(&n, pRef) < 0.0f) {
    return 0;
  }
  BrVec3Sub(&edge3, pA, pC);
  BrVec3Sub(&toPtA, pPt, pA);
  BrVec3Cross(&n, &edge3, &toPtA);
  if (BrVec3Dot(&n, pRef) < 0.0f) {
    return 0;
  }
  return 1;
}
