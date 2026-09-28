/* atan2.c -- angles from two components
 */
#include "tgr/common.h"

/* -- declarations -- */
float sqrtf(float x);
float sinf(float x);
/* -- end declarations -- */

/* WHAT IT DOES: The angle of the vector (x, y), 0 to 2 pi: fold it into the
 * first octant (adding pi, pi/2 and pi/4 as it goes), then find the angle
 * whose sine is y over the length by 16 halving steps of a bisection from
 * pi/8, stopping early within 0.005. */
/* @implements 0x8022576C tgr BrAtan2 */
float BrAtan2(float x, float y)
{
  float ang;
  float t;
  float r;
  float a;
  float step;
  float s;
  int noSwap;
  int i;

  ang = 0.0f;
  noSwap = 1;
  if (y < 0.0f) {
    x = -x;
    y = -y;
    ang = ang + 3.1415927f;
  }
  if (x < 0.0f) {
    t = x;
    x = y;
    y = -t;
    ang = ang + 1.5707964f;
  }
  if (x < y) {
    t = x;
    x = y;
    y = t;
    noSwap = 0;
    ang = ang + 0.7853982f;
  }
  r = sqrtf(y * y + x * x);
  if (r == 0.0f) {
    return 0.0f;
  }
  y = y / r;
  a = 0.3926991f;
  step = a * 0.5f;
  for (i = 0; i < 16; i++) {
    s = sinf(a);
    if (s < y) {
      if (y - s < 0.005f) {
        break;
      }
      a = a + step;
    } else {
      if (!(y < s)) {
        break;
      }
      if (s - y < 0.005f) {
        break;
      }
      a = a - step;
    }
    step = step * 0.5f;
  }
  if (noSwap) {
    ang = ang + a;
  } else {
    ang += 0.7853982f - a;
  }
  return ang;
}
