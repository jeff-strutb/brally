/* mat3.c -- 3x3 matrix helpers
 */
#include "tgr/common.h"

/* -- declarations -- */
extern int D_8025882C;
extern int D_802588F4;
/* -- end declarations -- */

/* WHAT IT DOES: Transform a point by a 4x4 matrix's rotation (columns) and
 * translation (row 3). */
/* @implements 0x802587E8 tgr BrMat3MulVecRows */
void BrMat3MulVecRows(float out[3], float m[4][4], float v[3])
{
  int i;
  int k;

  for (i = 0; i < 3; i++) {
    out[i] = 0.0f;
    for (k = 0; k < 3; k++) {
      out[i] += m[k][i] * v[k];
    }
  }
  out[0] += m[3][0];
  out[1] += m[3][1];
  out[2] += m[3][2];
}


/* WHAT IT DOES: Multiply a 3x3 matrix by a vector. */
/* @implements 0x802588B8 tgr BrMat3MulVec */
void BrMat3MulVec(float out[3], float m[3][3], float v[3])
{
  int i;
  int k;

  for (i = 0; i < 3; i++) {
    out[i] = 0.0f;
    for (k = 0; k < 3; k++) {
      out[i] += m[i][k] * v[k];
    }
  }
}


/* WHAT IT DOES: Take the 3x3 rotation out of a 4x4 matrix twice: once
 * transposed, once as it is. */
/* @implements 0x80258E04 tgr BrMat3Transpose */
void BrMat3Transpose(float t[3][3], float c[3][3], float m[4][4])
{
  int i;
  int j;

  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++) {
      c[i][j] = t[j][i] = m[i][j];
    }
  }
}


/* WHAT IT DOES: Subtract one 3x3 matrix from another into a third. */
/* @implements 0x80258F70 tgr BrMat3Sub */
void BrMat3Sub(float out[3][3], float a[3][3], float b[3][3])
{
  int i;
  int j;

  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++) {
      out[i][j] = a[i][j] - b[i][j];
    }
  }
}

