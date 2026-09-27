/* fog.c -- the weather fog: the RDP fog range and the fog amount at a point
 */
#include "tgr/common.h"

/* -- declarations -- */
void BrMat4TransformPoint4(float out[4], float v[3], float m[4][4]);
extern int D_8028AA78;
extern int D_8028AB38;
extern int D_8028AB3C;
extern float D_8031AA50[4][4];
/* -- end declarations -- */

/* WHAT IT DOES: How fogged a world point is, 0 to 1: its projected depth
 * through the fog multiplier and offset the RSP uses (0 when the weather has
 * no fog). */
/* @implements 0x80218C8C tgr BrFogAmount */
float BrFogAmount(float v[3])
{
  float f;
  float p[4];

  if (D_8028AA78 == 0) {
    return 0.0f;
  }
  BrMat4TransformPoint4(p, v, D_8031AA50);
  p[2] /= p[3];
  f = (p[2] * D_8028AB3C + D_8028AB38) * 0.00392156862745098;   /* 1/255 */
  if (f < 0.0f) {
    return 0.0f;
  }
  if (f > 1.0f) {
    return 1.0f;
  }
  return f;
}
