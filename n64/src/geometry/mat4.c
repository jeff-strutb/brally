/* mat4.c -- 4x4 matrix helpers
 */
#include "tgr/common.h"

/* -- declarations -- */
char * memcpy(char *param_1,char *param_2,int param_3);
extern int D_802251C4;
/* -- end declarations -- */

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
void BrMat4ProjectPoint(float *param_1,float *param_2,float *param_3)
{
  float fVar1;
  float fVar2;
  float fVar3;
  float fVar4;
  
  fVar1 = *param_2;
  fVar2 = param_2[1];
  fVar3 = param_2[2];
  fVar4 = 1.0f / (param_3[0xf] + fVar1 * param_3[3] + fVar2 * param_3[7] + fVar3 * param_3[0xb]);
  *param_1 = (param_3[0xc] + fVar1 * *param_3 + fVar2 * param_3[4] + fVar3 * param_3[8]) * fVar4;
  param_1[1] = (param_3[0xd] + fVar1 * param_3[1] + fVar2 * param_3[5] + fVar3 * param_3[9]) * fVar4
  ;
  param_1[2] = (param_3[0xe] + fVar1 * param_3[2] + fVar2 * param_3[6] + fVar3 * param_3[10]) *
               fVar4;
}

/* WHAT IT DOES: Transform a direction by the 3x3 rotation part of a 4x4
 * matrix (no translation). */
/* @t4-pass 0x802250FC 1 2026-09-26 compiles 16 best 21 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x802250FC 2 2026-09-26 compiles 17 best 18 moved 3  (n64/tools/n64permute.py) */
/* @t4-pass 0x802250FC 3 2026-09-26 compiles 16 best 18 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x802250FC tgr BrMat4RotateDir */
void BrMat4RotateDir(float *param_1,float *param_2,float *param_3)
{
  float fVar1;
  float fVar2;
  float fVar3;
  
  fVar2 = param_2[1];
  fVar1 = *param_2;
  fVar3 = param_2[2];
  *param_1 = param_3[8] * fVar3 + fVar1 * *param_3 + fVar2 * param_3[4];
  param_1[1] = param_3[9] * fVar3 + fVar1 * param_3[1] + fVar2 * param_3[5];
  param_1[2] = fVar3 * param_3[10] + fVar1 * param_3[2] + fVar2 * param_3[6];
}

/* WHAT IT DOES: Multiply two 4x4 matrices into a third, through a temporary
 * so the output may be one of the inputs. */
/* @t4-pass 0x80225180 1 2026-09-26 compiles 17 best 91 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80225180 2 2026-09-26 compiles 17 best 91 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80225180 3 2026-09-26 compiles 17 best 91 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x80225180 tgr BrMat4Mul */
void BrMat4Mul(float *param_1,float *param_2,float *param_3)
{
  float *pfVar1;
  int iVar2;
  float *pfVar3;
  float *pfVar4;
  float *pfVar5;
  float fVar6;
  float fVar7;
  float afStackX_0 [4];
  float local_40 [16];
  
  pfVar1 = local_40;
  do {
    *pfVar1 = 0.0;
    iVar2 = 4;
    fVar6 = *pfVar1 + *param_2 * *param_3;
    pfVar3 = pfVar1;
    pfVar5 = param_3;
    while( 1 ) {
      iVar2 = iVar2 + 4;
      pfVar4 = pfVar3 + 1;
      *pfVar3 = fVar6;
      *pfVar3 = *pfVar3 + param_2[1] * pfVar5[4];
      *pfVar3 = *pfVar3 + param_2[2] * pfVar5[8];
      fVar6 = param_2[3];
      fVar7 = pfVar5[0xc];
      *pfVar4 = 0.0;
      *pfVar3 = *pfVar3 + fVar6 * fVar7;
      fVar6 = *param_2 * pfVar5[1];
      if (iVar2 == 0x10) break;
      fVar6 = *pfVar4 + fVar6;
      pfVar3 = pfVar4;
      pfVar5 = pfVar5 + 1;
    }
    *pfVar4 = *pfVar4 + fVar6;
    *pfVar4 = *pfVar4 + param_2[1] * pfVar5[5];
    *pfVar4 = *pfVar4 + param_2[2] * pfVar5[9];
    *pfVar4 = *pfVar4 + param_2[3] * pfVar5[0xd];
    pfVar1 = pfVar1 + 4;
    param_2 = param_2 + 4;
  } while (pfVar1 < afStackX_0);
  pfVar1 = local_40;
  do {
    pfVar3 = pfVar1 + 4;
    *param_1 = *pfVar1;
    param_1[1] = pfVar1[1];
    param_1[2] = pfVar1[2];
    param_1[3] = pfVar1[3];
    param_1 = param_1 + 4;
    pfVar1 = pfVar3;
  } while (pfVar3 != afStackX_0);
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
