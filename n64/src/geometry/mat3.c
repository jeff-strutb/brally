/* mat3.c -- 3x3 matrix helpers
 */
#include "tgr/common.h"

/* -- declarations -- */
extern int D_8025882C;
extern int D_802588F4;
/* -- end declarations -- */

/* WHAT IT DOES: Multiply a 3x3 matrix by a vector, each output the dot
 * product of a matrix row with the vector. */
/* @t4-pass 0x802587E8 1 2026-09-26 compiles 16 best 49 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x802587E8 2 2026-09-26 compiles 17 best 49 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x802587E8 3 2026-09-26 compiles 17 best 49 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x802587E8 tgr BrMat3MulVecRows */
void BrMat3MulVecRows(float *param_1,int param_2,float *param_3)
{
  int iVar1;
  int iVar2;
  float *pfVar3;
  float *pfVar4;
  float *pfVar5;
  float fVar6;
  
  iVar2 = 0;
  pfVar4 = param_1;
  do {
    *pfVar4 = 0.0;
    pfVar3 = (float *)(param_2 + iVar2);
    iVar1 = 1;
    fVar6 = *pfVar4 + *pfVar3 * *param_3;
    pfVar5 = param_3;
    while( 1 ) {
      iVar1 = iVar1 + 1;
      pfVar3 = pfVar3 + 4;
      pfVar5 = pfVar5 + 1;
      *pfVar4 = fVar6;
      if (iVar1 == 3) break;
      fVar6 = *pfVar4 + *pfVar3 * *pfVar5;
    }
    *pfVar4 = *pfVar4 + *pfVar3 * *pfVar5;
    iVar2 = iVar2 + 4;
    pfVar4 = pfVar4 + 1;
  } while (iVar2 != 0xc);
  *param_1 = *param_1 + *(float *)(param_2 + 0x30);
  param_1[1] = param_1[1] + *(float *)(param_2 + 0x34);
  param_1[2] = param_1[2] + *(float *)(param_2 + 0x38);
}

/* WHAT IT DOES: Multiply a 3x3 matrix by a vector. */
/* @t4-pass 0x802588B8 1 2026-09-26 compiles 17 best 37 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x802588B8 2 2026-09-26 compiles 17 best 37 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x802588B8 3 2026-09-26 compiles 16 best 37 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x802588B8 tgr BrMat3MulVec */
void BrMat3MulVec(float *param_1,float *param_2,float *param_3)
{
  int iVar1;
  int iVar2;
  float *pfVar3;
  float *pfVar4;
  float fVar5;
  
  iVar2 = 0;
  do {
    *param_1 = 0.0;
    iVar1 = 1;
    fVar5 = *param_1 + *param_2 * *param_3;
    pfVar3 = param_2;
    pfVar4 = param_3;
    while( 1 ) {
      pfVar3 = pfVar3 + 1;
      iVar1 = iVar1 + 1;
      pfVar4 = pfVar4 + 1;
      *param_1 = fVar5;
      if (iVar1 == 3) break;
      fVar5 = *param_1 + *pfVar3 * *pfVar4;
    }
    *param_1 = *param_1 + *pfVar3 * *pfVar4;
    iVar2 = iVar2 + 4;
    param_1 = param_1 + 1;
    param_2 = param_2 + 3;
  } while (iVar2 != 0xc);
}

/* WHAT IT DOES: Write the transpose of a 3x3 matrix. */
/* @t4-pass 0x80258E04 1 2026-09-26 compiles 16 best 27 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80258E04 2 2026-09-26 compiles 17 best 27 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80258E04 3 2026-09-26 compiles 16 best 27 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x80258E04 tgr BrMat3Transpose */
void BrMat3Transpose(int param_1,int *param_2,int *param_3)
{
  int iVar1;
  int *puVar2;
  int *puVar3;
  int *puVar4;
  int iVar5;
  int uVar6;
  
  iVar5 = 0;
  do {
    iVar1 = 0;
    puVar3 = (int *)(param_1 + iVar5);
    puVar2 = param_3;
    puVar4 = param_2;
    do {
      uVar6 = *puVar2;
      iVar1 = iVar1 + 1;
      puVar2 = puVar2 + 1;
      *puVar3 = uVar6;
      puVar3 = puVar3 + 3;
      *puVar4 = uVar6;
      puVar4 = puVar4 + 1;
    } while (iVar1 != 3);
    iVar5 = iVar5 + 4;
    param_3 = param_3 + 4;
    param_2 = param_2 + 3;
  } while (iVar5 != 0xc);
}

/* WHAT IT DOES: Multiply two 3x3 matrices into a third. */
/* @t4-pass 0x80258F70 1 2026-09-26 compiles 16 best 30 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80258F70 2 2026-09-26 compiles 17 best 27 moved 3  (n64/tools/n64permute.py) */
/* @t4-pass 0x80258F70 3 2026-09-26 compiles 17 best 26 moved 1  (n64/tools/n64permute.py) */
/* @t4-pass 0x80258F70 4 2026-09-26 compiles 41 best 26 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x80258F70 tgr BrMat3Mul */
void BrMat3Mul(float *param_1,float *param_2,float *param_3)
{
  int iVar1;
  float *pfVar3;
  int iVar2;
  float *pfVar4;
  float *pfVar5;
  float fVar6;
  float fVar7;
  
  iVar1 = 0;
  do {
    pfVar3 = param_1;
    iVar2 = 0;
    pfVar5 = param_3;
    pfVar4 = param_2;
    do {
      fVar6 = *pfVar4;
      fVar7 = *pfVar5;
      iVar2 = iVar2 + 1;
      pfVar4 = pfVar4 + 1;
      pfVar5 = pfVar5 + 1;
      *pfVar3 = fVar6 - fVar7;
      pfVar3 = pfVar3 + 1;
    } while (iVar2 != 3);
    param_1 = param_1 + 3;
    iVar1 = iVar1 + 1;
    param_2 = param_2 + 3;
    param_3 = param_3 + 3;
  } while (iVar1 != 3);
}
