/* mat3.c -- 3x3 matrix helpers
 */
#include "tgr/common.h"

/* -- declarations -- */
extern int D_8025882C;
extern int D_802588F4;
/* -- end declarations -- */

/* WHAT IT DOES: Rotate a vector by a 4x4 matrix's 3x3 part: out = R v. */
/* @implements 0x802586C0 tgr BrMat4RotateVec */
void BrMat4RotateVec(float out[3], float m[4][4], float v[3])
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

/* WHAT IT DOES: Rotate a vector by the transpose of a 4x4 matrix's 3x3
 * part: out = R^T v. */
/* @implements 0x80258758 tgr BrMat4RotateVecT */
void BrMat4RotateVecT(float out[3], float m[4][4], float v[3])
{
  int i;
  int k;

  for (i = 0; i < 3; i++) {
    out[i] = 0.0f;
    for (k = 0; k < 3; k++) {
      out[i] += m[k][i] * v[k];
    }
  }
}

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


/* WHAT IT DOES: Invert a rotation-and-translation matrix whose rows carry
 * the scales s: the rotation is transposed with each column j scaled by
 * s[j], and the translation becomes -t taken through the new rotation's
 * transpose. */
/* @implements 0x80258FD4 tgr BrMat4InvertScaled */
void BrMat4InvertScaled(float m[4][4], float out[4][4], float s[3])
{
  int i;
  int j;
  float t[3];

  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++) {
      out[i][j] = m[j][i];
      out[i][j] *= s[j];
    }
    out[i][3] = 0.0f;
  }
  out[3][3] = 1.0f;
  t[0] = -m[3][0];
  t[1] = -m[3][1];
  t[2] = -m[3][2];
  BrMat4RotateVecT(out[3], out, t);
}


/* WHAT IT DOES: Solve the 3x3 system m x = v for x by Cramer's rule (no
 * guard against a singular matrix): six 2x2 products per column, the
 * determinant's reciprocal kept in d.  The PC twin is BrMat3Solve. */
/* @implements 0x802590E8 tgr BrMat3Solve */
void BrMat3Solve(float out[3], float m[9], float v[3])
{
  float p0;
  float p1;
  float p2;
  float p3;
  float p4;
  float p5;
  float d;

  p0 = m[4] * m[8];
  p1 = m[5] * m[7];
  p2 = m[1] * m[8];
  p3 = m[2] * m[7];
  p4 = m[1] * m[5];
  p5 = m[2] * m[4];
  d = -m[0] * p0 + m[0] * p1 + m[3] * p2 - m[3] * p3 - m[6] * p4 + m[6] * p5;
  d = 1.0f / d;
  out[0] = (-v[0] * p0 + v[0] * p1 + v[1] * p2 - v[1] * p3 - v[2] * p4 + v[2] * p5) * d;
  p0 = m[3] * m[8];
  p1 = m[5] * m[6];
  p2 = m[0] * m[8];
  p3 = m[2] * m[6];
  p4 = m[0] * m[5];
  p5 = m[2] * m[3];
  out[1] = -(-v[0] * p0 + v[0] * p1 + v[1] * p2 - v[1] * p3 - v[2] * p4 + v[2] * p5) * d;
  p0 = m[3] * m[7];
  p1 = m[4] * m[6];
  p2 = m[0] * m[7];
  p3 = m[1] * m[6];
  p4 = m[0] * m[4];
  p5 = m[1] * m[3];
  out[2] = (-v[0] * p0 + v[0] * p1 + v[1] * p2 - v[1] * p3 - v[2] * p4 + v[2] * p5) * d;
}
