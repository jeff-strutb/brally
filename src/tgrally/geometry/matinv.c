/* matinv.c -- inverting an affine 4x4 matrix (Graphics Gems II)
 */
#include "tgr/common.h"

/* -- declarations -- */
void BrFatal(char *msg);
/* -- end declarations -- */


#define ACCUMULATE    \
    if (temp >= 0.0f) \
        pos += temp;  \
    else              \
        neg += temp;
#define PRECISION_LIMIT (1.0e-15f)

/* WHAT IT DOES: Invert an affine 4x4 matrix (Graphics Gems II, "Fast
 * Matrix Inversion"): the 3x3 part by its adjoint over the determinant
 * (fatal and 0 when the determinant is 0 or negligible against its terms),
 * the translation as minus the old one through that inverse, the w column
 * 0, 0, 0, 1.  Returns 1.  The ratio test is written out rather than
 * through the gem's ABS macro: the ROM keeps it in a named local. */
/* @implements 0x80225370 tgr BrMat4Inverse */
int BrMat4Inverse(float out[4][4], float in[4][4])
{
  float det_1;
  float pos, neg, temp;
  float ratio;

  pos = neg = 0.0f;
  temp = in[0][0] * in[1][1] * in[2][2];
  ACCUMULATE
  temp = in[0][1] * in[1][2] * in[2][0];
  ACCUMULATE
  temp = in[0][2] * in[1][0] * in[2][1];
  ACCUMULATE
  temp = -in[0][2] * in[1][1] * in[2][0];
  ACCUMULATE
  temp = -in[0][1] * in[1][0] * in[2][2];
  ACCUMULATE
  temp = -in[0][0] * in[1][2] * in[2][1];
  ACCUMULATE
  det_1 = pos + neg;

  ratio = det_1 / (pos - neg);
  if (ratio < 0.0f) {
    ratio = -ratio;
  }
  if ((det_1 == 0.0f) || (ratio < PRECISION_LIMIT)) {
    BrFatal("Matrix4Inverse: singular matrix");
    return 0;
  } else {
    det_1 = 1.0f / det_1;
    out[0][0] =   (in[1][1] * in[2][2] - in[1][2] * in[2][1]) * det_1;
    out[1][0] = - (in[1][0] * in[2][2] - in[1][2] * in[2][0]) * det_1;
    out[2][0] =   (in[1][0] * in[2][1] - in[1][1] * in[2][0]) * det_1;
    out[0][1] = - (in[0][1] * in[2][2] - in[0][2] * in[2][1]) * det_1;
    out[1][1] =   (in[0][0] * in[2][2] - in[0][2] * in[2][0]) * det_1;
    out[2][1] = - (in[0][0] * in[2][1] - in[0][1] * in[2][0]) * det_1;
    out[0][2] =   (in[0][1] * in[1][2] - in[0][2] * in[1][1]) * det_1;
    out[1][2] = - (in[0][0] * in[1][2] - in[0][2] * in[1][0]) * det_1;
    out[2][2] =   (in[0][0] * in[1][1] - in[0][1] * in[1][0]) * det_1;

    out[3][0] = - (in[3][0] * out[0][0] + in[3][1] * out[1][0] + in[3][2] * out[2][0]);
    out[3][1] = - (in[3][0] * out[0][1] + in[3][1] * out[1][1] + in[3][2] * out[2][1]);
    out[3][2] = - (in[3][0] * out[0][2] + in[3][1] * out[1][2] + in[3][2] * out[2][2]);

    out[0][3] = out[1][3] = out[2][3] = 0.0f;
    out[3][3] = 1.0f;

    return 1;
  }
}
