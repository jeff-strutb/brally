/* mat4.c -- 4x4 matrix helpers
 */
#include "tgr/common.h"

/* -- declarations -- */
char * memcpy(char *param_1,char *param_2,int param_3);
extern int D_802251C4;
/* -- end declarations -- */

/* WHAT IT DOES: Transpose a 4x4 matrix: out = m transposed, swapping each
 * pair across the diagonal (so out may be m itself; the diagonal is left as
 * it is).  Each swap sits on one line, as a SWAP macro would put it: IDO
 * schedules the loads and stores by line. */
/* @implements 0x802252EC tgr BrMat4Transpose */
void BrMat4Transpose(float out[4][4], float m[4][4])
{
  float t;

  t = m[0][1];   out[0][1] = m[1][0];   out[1][0] = t;
  t = m[0][2];   out[0][2] = m[2][0];   out[2][0] = t;
  t = m[0][3];   out[0][3] = m[3][0];   out[3][0] = t;
  t = m[1][2];   out[1][2] = m[2][1];   out[2][1] = t;
  t = m[1][3];   out[1][3] = m[3][1];   out[3][1] = t;
  t = m[2][3];   out[2][3] = m[3][2];   out[3][2] = t;
}

/* WHAT IT DOES: Copy a 4x4 float matrix (64 bytes). */
/* @implements 0x80225350 tgr BrMat4Copy */
void BrMat4Copy(int param_1,int param_2)
{
  memcpy(param_1,param_2,0x40);
}

/* WHAT IT DOES: Transform a point by a 4x4 matrix and divide by w: the
 * projected position. */
/* @t4-pass 0x80224D00 1 2026-09-26 compiles 17 best 54 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80224D00 2 2026-09-26 compiles 17 best 54 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80224D00 3 2026-09-26 compiles 16 best 54 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x80224D00 tgr BrMat4ProjectPoint */
void BrMat4ProjectPoint(float out[3], float v[3], float m[4][4])
{
  float x = v[0];
  float y = v[1];
  float z = v[2];
  float w = 1.0f / ((x * m[0][3] + y * m[1][3] + z * m[2][3]) + m[3][3]);

  out[0] = ((x * m[0][0] + y * m[1][0] + z * m[2][0]) + m[3][0]) * w;
  out[1] = ((x * m[0][1] + y * m[1][1] + z * m[2][1]) + m[3][1]) * w;
  out[2] = ((x * m[0][2] + y * m[1][2] + z * m[2][2]) + m[3][2]) * w;
}


/* WHAT IT DOES: Transform a point by a 4x4 matrix into all four
 * homogeneous components (x, y, z and w), without the divide. */
/* @implements 0x80224DDC tgr BrMat4TransformPoint4 */
void BrMat4TransformPoint4(float out[4], float v[3], float m[4][4])
{
  float x = v[0];
  float y = v[1];
  float z = v[2];

  out[0] = (x * m[0][0] + y * m[1][0] + z * m[2][0]) + m[3][0];
  out[1] = (x * m[0][1] + y * m[1][1] + z * m[2][1]) + m[3][1];
  out[2] = (x * m[0][2] + y * m[1][2] + z * m[2][2]) + m[3][2];
  out[3] = (x * m[0][3] + y * m[1][3] + z * m[2][3]) + m[3][3];
}

/* WHAT IT DOES: Project a point through a 4x4 matrix (row vectors,
 * translation in row 3): transform and divide by w.  The PC twin is
 * BrVec3Project (br_mat.c). */
/* @implements 0x80225038 tgr BrVec3Project */
void BrVec3Project(float out[3], float v[3], float m[4][4])
{
  float x = v[0];
  float y = v[1];
  float z = v[2];
  float w;

  w = 1.0f / (x * m[0][3] + y * m[1][3] + z * m[2][3] + m[3][3]);
  out[0] = (x * m[0][0] + y * m[1][0] + z * m[2][0]) * w;
  out[1] = (x * m[0][1] + y * m[1][1] + z * m[2][1]) * w;
  out[2] = (x * m[0][2] + y * m[1][2] + z * m[2][2]) * w;
}

/* WHAT IT DOES: Transform a direction by the 3x3 rotation part of a 4x4
 * matrix (no translation). */
/* @implements 0x802250FC tgr BrMat4RotateDir */
void BrMat4RotateDir(float out[3], float v[3], float m[4][4])
{
  float x = v[0];
  float y = v[1];
  float z = v[2];

  out[0] = x * m[0][0] + y * m[1][0] + z * m[2][0];
  out[1] = x * m[0][1] + y * m[1][1] + z * m[2][1];
  out[2] = x * m[0][2] + y * m[1][2] + z * m[2][2];
}


/* WHAT IT DOES: Multiply two 4x4 matrices into a third, through a temporary
 * so the output may be one of the inputs. */
/* @implements 0x80225180 tgr BrMat4Mul */
void BrMat4Mul(float r[4][4], float a[4][4], float b[4][4])
{
  float t[4][4];
  int i;
  int j;
  int k;

  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      t[i][j] = 0.0f;
      for (k = 0; k < 4; k++) {
        t[i][j] += a[i][k] * b[k][j];
      }
    }
  }
  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      r[i][j] = t[i][j];
    }
  }
}


/* WHAT IT DOES: Reset a 4x4 float matrix's w column: m[0..2][3] become 0
 * and m[3][3] becomes 1, leaving the rest alone. */
/* @implements 0x8021EB30 tgr BrMat4ResetW */
void BrMat4ResetW(float m[4][4])
{
  m[2][3] = 0.0f;
  m[1][3] = 0.0f;
  m[0][3] = 0.0f;
  m[3][3] = 1.0f;
}
