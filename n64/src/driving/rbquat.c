/* rbquat.c -- rigid-body orientation
 */
#include "tgr/common.h"
#include "tgr/vec.h"

/* -- declarations -- */
void BrStub80258078(void);
typedef struct BrRbQuat { float w, x, y, z; } BrRbQuat;
typedef struct BrRb {           /* a rigid body's integrated state */
  char pad00[0xc];
  BrVec3 pos;                   /* 0x0C */
  BrRbQuat q;                   /* 0x18  orientation */
  BrVec3 omega;                 /* 0x28  angular velocity */
  BrRbQuat qdot;                /* 0x34  orientation rate */
} BrRb;
typedef struct BrRbParams {     /* set together by BrRbSetParams */
  int x0;
  int kind;                     /* 0x04 */
  float x8;
  float xc;
  float x10;
  float x14;
  float x18;
  float x1c;
} BrRbParams;
/* -- end declarations -- */

/* WHAT IT DOES: Compute a rigid body's orientation rate: the quaternion
 * derivative from its angular velocity (half the product of the spin and
 * the orientation). */
/* @implements 0x80258248 tgr BrRbQuatDerivative */
void BrRbQuatDerivative(BrRb *b)
{
  float h[3];

  h[0] = b->omega.x * 0.5f;
  h[1] = b->omega.y * 0.5f;
  h[2] = b->omega.z * 0.5f;
  b->qdot.w = -h[0] * b->q.x - h[1] * b->q.y - h[2] * b->q.z;
  b->qdot.x = b->q.w * h[0] + h[1] * b->q.z - h[2] * b->q.y;
  b->qdot.y = b->q.w * h[1] + h[2] * b->q.x - h[0] * b->q.z;
  b->qdot.z = b->q.w * h[2] + h[0] * b->q.y - h[1] * b->q.x;
}


/* WHAT IT DOES: Advance a rigid body by one time step: position by velocity
 * and orientation by its rate, both scaled by dt. */
/* @t4-pass 0x80258324 1 2026-09-26 compiles 16 best 46 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80258324 2 2026-09-26 compiles 17 best 46 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80258324 3 2026-09-26 compiles 17 best 46 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x80258324 tgr BrRbIntegrate */
void BrRbIntegrate(int param_1,int param_2,float param_3)
{
  float fVar1;
  float fVar2;
  float fVar3;
  float fVar4;
  float fVar5;
  
  fVar2 = *(float *)(param_2 + 0x100);
  fVar4 = *(float *)(param_2 + 0x104);
  fVar1 = *(float *)(param_2 + 0x108);
  fVar3 = *(float *)(param_2 + 0x10c);
  fVar5 = *(float *)(param_2 + 0x110);
  *(float *)(param_1 + 0xc) = *(float *)(param_2 + 0xfc) * param_3 + *(float *)(param_1 + 0xc);
  *(float *)(param_1 + 0x10) = fVar2 * param_3 + *(float *)(param_1 + 0x10);
  *(float *)(param_1 + 0x14) = fVar4 * param_3 + *(float *)(param_1 + 0x14);
  *(float *)(param_1 + 0x28) = fVar1 * param_3 + *(float *)(param_1 + 0x28);
  *(float *)(param_1 + 0x2c) = fVar3 * param_3 + *(float *)(param_1 + 0x2c);
  *(float *)(param_1 + 0x30) = fVar5 * param_3 + *(float *)(param_1 + 0x30);
}

/* WHAT IT DOES: Build the 3x3 rotation matrix of a unit quaternion. */
/* @t4-pass 0x802583DC 1 2026-09-26 compiles 19 best 87 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x802583DC 2 2026-09-26 compiles 20 best 87 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x802583DC tgr BrQuatToMat */
void BrQuatToMat(float *param_1,float *param_2)
{
  float fVar1;
  float fVar2;
  float fVar3;
  float fVar4;
  float fVar5;
  float fVar6;
  float fVar7;
  float fVar8;
  float fVar9;
  float fVar10;
  float fVar11;
  
  fVar10 = param_2[6];
  fVar11 = fVar10 + fVar10;
  fVar3 = param_2[7];
  fVar1 = param_2[8];
  fVar6 = fVar1 * fVar1;
  fVar9 = param_2[9];
  fVar8 = fVar9 * fVar9;
  fVar4 = fVar1 * (fVar3 + fVar3);
  fVar7 = fVar9 * (fVar3 + fVar3);
  fVar5 = fVar9 * (fVar1 + fVar1);
  fVar2 = fVar10 * fVar10 - fVar3 * fVar3;
  *param_1 = ((fVar10 * fVar10 + fVar3 * fVar3) - fVar6) - fVar8;
  param_1[1] = fVar4 + fVar9 * fVar11;
  param_1[3] = 0.0;
  param_1[2] = fVar7 - fVar1 * fVar11;
  param_1[4] = fVar4 - fVar9 * fVar11;
  param_1[5] = (fVar2 + fVar6) - fVar8;
  param_1[7] = 0.0;
  param_1[6] = fVar5 + fVar3 * fVar11;
  param_1[0xb] = 0.0;
  param_1[9] = fVar5 - fVar3 * fVar11;
  param_1[8] = fVar7 + fVar1 * fVar11;
  param_1[10] = (fVar2 - fVar6) + fVar8;
  param_1[0xc] = *param_2;
  param_1[0xd] = param_2[1];
  fVar1 = param_2[2];
  param_1[0xf] = 1.0;
  param_1[0xe] = fVar1;
  BrStub80258078();
}

/* WHAT IT DOES: Fill in a block of seven rigid-body parameters: six floats
 * and an int. */
/* @implements 0x80258680 tgr BrRbSetParams */
void BrRbSetParams(BrRbParams *p, float a, float b, float c, float d, float e, float f, int kind)
{
  p->kind = kind;
  p->x8 = a;
  p->xc = b;
  p->x10 = c;
  p->x14 = d;
  p->x18 = e;
  p->x1c = f;
}

