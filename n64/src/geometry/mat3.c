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


/* WHAT IT DOES: Take the 3x3 rotation out of a 4x4 matrix, transposed. */
/* @implements 0x80258E64 tgr BrMat3FromMat4T */
void BrMat3FromMat4T(float t[3][3], float m[4][4])
{
  int i;
  int j;

  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++) {
      t[j][i] = m[i][j];
    }
  }
}


/* WHAT IT DOES: The cross-product matrix of a vector: out * u = v x u. */
/* @implements 0x80258DB0 tgr BrMat3Skew */
void BrMat3Skew(float out[3][3], float v[3])
{
  out[2][2] = 0.0f;
  out[1][1] = 0.0f;
  out[0][0] = 0.0f;
  out[0][1] = -v[2];
  out[0][2] = v[1];
  out[1][0] = v[2];
  out[1][2] = -v[0];
  out[2][0] = -v[1];
  out[2][1] = v[0];
}

/* WHAT IT DOES: Take the 3x3 rotation out of a 4x4 matrix as it is. */
/* @implements 0x80258EAC tgr BrMat3FromMat4 */
void BrMat3FromMat4(float out[3][3], float m[4][4])
{
  int i;
  int j;

  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++) {
      out[i][j] = m[i][j];
    }
  }
}

/* WHAT IT DOES: Multiply two 3x3 matrices: out = a * b.  (The PC twin in
 * br_rbinteg.c needs a float local for VC5; IDO matches without it.) */
/* @implements 0x80258EF8 tgr BrMat3Mul */
void BrMat3Mul(float out[3][3], float a[3][3], float b[3][3])
{
  int i;
  int j;

  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++) {
      out[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j];
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

