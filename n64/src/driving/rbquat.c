/* rbquat.c -- rigid-body orientation
 */
#include "tgr/common.h"

/* -- declarations -- */
void BrStub80258078(void);
/* -- end declarations -- */

/* WHAT IT DOES: Compute a rigid body's orientation rate: the quaternion
 * derivative from its angular velocity (half the product of the spin and
 * the orientation). */
/* @implements 0x80258248 tgr BrRbQuatDerivative */
void BrRbQuatDerivative(int param_1)
{
  float fVar1;
  float fVar2;
  float fVar3;
  float fVar4;
  float fVar5;
  float fVar6;
  float fVar7;
  
  fVar4 = *(float *)(param_1 + 0x28) * 0.5;
  fVar5 = *(float *)(param_1 + 0x2c) * 0.5;
  fVar3 = *(float *)(param_1 + 0x30) * 0.5;
  fVar1 = *(float *)(param_1 + 0x1c);
  fVar2 = *(float *)(param_1 + 0x20);
  fVar6 = *(float *)(param_1 + 0x24);
  fVar7 = *(float *)(param_1 + 0x18);
  *(float *)(param_1 + 0x34) = (-fVar4 * fVar1 - fVar5 * fVar2) - fVar6 * fVar3;
  *(float *)(param_1 + 0x38) = (fVar7 * fVar4 + fVar5 * fVar6) - fVar2 * fVar3;
  *(float *)(param_1 + 0x3c) = (fVar7 * fVar5 + fVar3 * fVar1) - fVar6 * fVar4;
  *(float *)(param_1 + 0x40) = (fVar7 * fVar3 + fVar4 * fVar2) - fVar1 * fVar5;
}

/* WHAT IT DOES: Advance a rigid body by one time step: position by velocity
 * and orientation by its rate, both scaled by dt. */
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
