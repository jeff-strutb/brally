/* obb.c -- triangle against the unit cube: Voorhies' "Triangle-Cube
 * Intersection" (Graphics Gems III), in Don Hatch's revision
 */
#include "tgr/common.h"

/* -- declarations -- */
int BrTriCubeTrivial(float verts[3][3]);
#define PAIR(cum, out, expr, bp, bn, lim) \
  if ((cum) & ((bp) | (bn))) { \
    s = (expr); \
    if (((cum) & (bp)) && s > (lim)) (out) |= (bp); \
    else if (((cum) & (bn)) && s < -(lim)) (out) |= (bn); \
  }
int BrPolyIntersectsCube(float verts[3][3], float polynormal[3]);
#define DOT3(a, b) ((a)[0] * (b)[0] + (a)[1] * (b)[1] + (a)[2] * (b)[2])
#define SIGN_NONZERO(x) ((x) < 0 ? -1 : 1)
#define IN_CLOSED_INTERVAL(a, x, b) (((x) - (a)) * ((x) - (b)) <= 0)
#define ABS(x) ((x) < 0 ? -(x) : (x))
#define MAXINDEX2(a) ((a)[0] > (a)[1] ? 0 : 1)
#define MAXINDEX3(a) ((a)[0] > (a)[2] ? MAXINDEX2(a) : 1 + MAXINDEX2((a) + 1))
#define seg_contains_point(a, b, x) (((b) > (x)) - ((a) > (x)))
#define VMV3(r, a, b) ((r)[0] = (a)[0] - (b)[0], (r)[1] = (a)[1] - (b)[1], (r)[2] = (a)[2] - (b)[2])
#define SQR(x) ((x) * (x))
#define SXV3(result, s, v) ((result)[0] = (s) * (v)[0], (result)[1] = (s) * (v)[1], (result)[2] = (s) * (v)[2])
/* -- end declarations -- */

/* WHAT IT DOES: Graphics Gems III (Voorhies, Hatch's revision)
 * trivial_vertex_tests for a triangle against the unit cube: 1 when a vertex
 * lies inside the cube, 0 when all three lie outside one face plane, one of
 * the twelve edge planes or one of the eight corner planes (the edge and
 * corner passes test only the planes every vertex so far is outside of), and
 * -1 when neither can be decided this way. */
/* @implements 0x8025C2C8 tgr BrTriCubeTrivial */
int BrTriCubeTrivial(float verts[3][3])
{
  int cum_and;
  int i;
  int bits;
  float s;

  cum_and = ~0;
  for (i = 0; i < 3; i++) {
    int face_bits;
    float *p = verts[i];

    face_bits = 0;
    PAIR(~0, face_bits, p[0], 0x01, 0x02, .5)
    PAIR(~0, face_bits, p[1], 0x04, 0x08, .5)
    PAIR(~0, face_bits, p[2], 0x10, 0x20, .5)
    if (face_bits == 0)
      return 1;
    cum_and &= face_bits;
  }
  if (cum_and != 0)
    return 0;

  cum_and = ~0;
  for (i = 0; i < 3; i++) {
    bits = 0;
    PAIR(cum_and, bits, verts[i][0] + verts[i][1], 0x001, 0x002, 1.0)
    PAIR(cum_and, bits, verts[i][0] - verts[i][1], 0x004, 0x008, 1.0)
    PAIR(cum_and, bits, verts[i][0] + verts[i][2], 0x010, 0x020, 1.0)
    PAIR(cum_and, bits, verts[i][0] - verts[i][2], 0x040, 0x080, 1.0)
    PAIR(cum_and, bits, verts[i][1] + verts[i][2], 0x100, 0x200, 1.0)
    PAIR(cum_and, bits, verts[i][1] - verts[i][2], 0x400, 0x800, 1.0)
    cum_and = bits;
    if (cum_and == 0)
      break;
  }
  if (cum_and != 0)
    return 0;

  cum_and = ~0;
  for (i = 0; i < 3; i++) {
    bits = 0;
    PAIR(cum_and, bits, verts[i][0] + verts[i][1] + verts[i][2], 0x01, 0x02, 1.5)
    PAIR(cum_and, bits, verts[i][0] + verts[i][1] - verts[i][2], 0x04, 0x08, 1.5)
    PAIR(cum_and, bits, verts[i][0] - verts[i][1] + verts[i][2], 0x10, 0x20, 1.5)
    PAIR(cum_and, bits, verts[i][0] - verts[i][1] - verts[i][2], 0x40, 0x80, 1.5)
    cum_and = bits;
    if (cum_and == 0)
      break;
  }
  if (cum_and != 0)
    return 0;
  return -1;
}

/* WHAT IT DOES: Graphics Gems III (Voorhies) polygon_contains_point_3d
 * for a triangle: drop the axis the normal is largest along, then count the
 * signed crossings of the triangle's edges by a ray from the point in the
 * remaining plane (non-zero = inside).  BrPolyIntersectsCube's last step. */
/* @implements 0x8025C8DC tgr BrPolyContainsPoint3d */
int BrPolyContainsPoint3d(float verts[][3], float polynormal[3], float point[3])
{
  float abspolynormal[3];
  int zaxis, xaxis, yaxis, i, count;
  int xdirection;
  float *v, *w;

  for (i = 0; i < 3; i++)
    abspolynormal[i] = ABS(polynormal[i]);
  zaxis = MAXINDEX3(abspolynormal);

  if (polynormal[zaxis] < 0) {
    xaxis = (zaxis + 2) % 3;
    yaxis = (zaxis + 1) % 3;
  } else {
    xaxis = (zaxis + 1) % 3;
    yaxis = (zaxis + 2) % 3;
  }

  count = 0;
  for (i = 0; i < 3; i++) {
    v = verts[i];
    w = verts[(i + 1) % 3];
    if (xdirection = seg_contains_point(v[xaxis], w[xaxis], point[xaxis])) {
      if (seg_contains_point(v[yaxis], w[yaxis], point[yaxis])) {
        if (xdirection * (point[xaxis] - v[xaxis]) * (w[yaxis] - v[yaxis]) <=
            xdirection * (point[yaxis] - v[yaxis]) * (w[xaxis] - v[xaxis]))
          count += xdirection;
      } else {
        if (v[yaxis] <= point[yaxis])
          count += xdirection;
      }
    }
  }
  return count;
}

/* WHAT IT DOES: Graphics Gems III (Voorhies) segment_intersects_cube: does
 * the segment v0-v1 pass through the unit cube at the origin?  Reject it
 * when it lies wholly beyond a face pair along any axis, then require it to
 * cross each of the three rhombus-shaped shadows the cube casts along the
 * segment's direction. */
/* @implements 0x8025CBF8 tgr BrSegIntersectsCube */
int BrSegIntersectsCube(float v0[3], float v1[3])
{
  int i, iplus1, iplus2, edgevec_signs[3];
  float edgevec[3];

  VMV3(edgevec, v1, v0);

  for (i = 0; i < 3; i++)
    edgevec_signs[i] = SIGN_NONZERO(edgevec[i]);

  for (i = 0; i < 3; i++) {
    if (v0[i] * edgevec_signs[i] > .5) return 0;
    if (v1[i] * edgevec_signs[i] < -.5) return 0;
  }

  for (i = 0; i < 3; i++) {
    float rhomb_normal_dot_v0, rhomb_normal_dot_cubedge;

    iplus1 = (i + 1) % 3;
    iplus2 = (i + 2) % 3;

    rhomb_normal_dot_v0 = edgevec[iplus2] * v0[iplus1]
                        - edgevec[iplus1] * v0[iplus2];

    rhomb_normal_dot_cubedge = .5 *
                            (edgevec[iplus2] * edgevec_signs[iplus1] +
                             edgevec[iplus1] * edgevec_signs[iplus2]);

    if (SQR(rhomb_normal_dot_v0) > SQR(rhomb_normal_dot_cubedge))
      return 0;
  }
  return 1;
}

/* WHAT IT DOES: Graphics Gems III (Voorhies) polygon_intersects_cube for a
 * triangle against the unit cube at the origin: any edge through the cube
 * (0x8025CBF8) is a hit; otherwise pick the cube diagonal nearest the
 * normal, find where it meets the triangle's plane, and if that is inside
 * the cube ask 0x8025C8DC whether the point is inside the triangle. */
/* @implements 0x8025CE28 tgr BrPolyIntersectsCube */
int BrPolyIntersectsCube(float verts[3][3], float polynormal[3])
{
  int i, best_diagonal[3];
  float p[3], t;

  for (i = 0; i < 3; ++i)
    if (BrSegIntersectsCube(verts[i], verts[(i + 1) % 3]))
      return 1;

  for (i = 0; i < 3; i++) best_diagonal[i] = SIGN_NONZERO(polynormal[i]);

  t = DOT3(polynormal, verts[0]) / DOT3(polynormal, best_diagonal);
  if (!IN_CLOSED_INTERVAL(-.5, t, .5))
    return 0;
  SXV3(p, t, best_diagonal);
  return BrPolyContainsPoint3d(verts, polynormal, p);
}

/* WHAT IT DOES: Triangle-against-unit-cube test: the vertex outcode pass
 * decides most cases; when it cannot (-1), fall back to the edge and plane
 * test with the triangle's normal. */
/* @implements 0x8025D018 tgr BrTriCubeTest */
int BrTriCubeTest(float *tri, float *norm)
{
  int r;

  r = BrTriCubeTrivial((float (*)[3])tri);
  if (r == -1) {
    return BrPolyIntersectsCube((float (*)[3])tri, norm);
  }
  return r;
}
