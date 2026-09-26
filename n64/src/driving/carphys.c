/* carphys.c -- the car's physical state
 */
#include "tgr/common.h"

/* -- declarations -- */
void func_8021FF90(float *param_1,float *param_2);
void BrQuatToMat(float *param_1,float *param_2);
char * memcpy(char *param_1,char *param_2,int param_3);
/* -- end declarations -- */

/* WHAT IT DOES: Place a car at a new position: copies the 4x4 placement
 * matrix into the car, sets its position and velocity from it and rebuilds
 * the car's orientation. */
/* @t4-pass 0x80220150 1 2026-09-26 compiles 17 best 12 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80220150 2 2026-09-26 compiles 17 best 12 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80220150 3 2026-09-26 compiles 17 best 12 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x80220150 tgr BrCarPlaceAt */
void BrCarPlaceAt(int param_1,int param_2)
{
  memcpy(param_1,param_2,0x40);
  func_8021FF90(param_2,param_1 + 0x1d8);
  *(int *)(param_1 + 0x2b8) = *(int *)(param_1 + 0x1d8);
  *(int *)(param_1 + 0x274) = *(int *)(param_1 + 0x1d8);
  *(int *)(param_1 + 700) = *(int *)(param_1 + 0x1dc);
  *(int *)(param_1 + 0x278) = *(int *)(param_1 + 0x1dc);
  *(int *)(param_1 + 0x2c0) = *(int *)(param_1 + 0x1e0);
  *(int *)(param_1 + 0x27c) = *(int *)(param_1 + 0x1e0);
  *(int *)(param_1 + 0x2c4) = *(int *)(param_1 + 0x1e4);
  *(int *)(param_1 + 0x280) = *(int *)(param_1 + 0x1e4);
  BrQuatToMat(param_1 + 0x204,param_1 + 0x1c0);
}
