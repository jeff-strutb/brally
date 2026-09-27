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
/* @implements 0x80220150 tgr BrCarPlaceAt */
void BrCarPlaceAt(char *car, float *mtx)
{
  float *q;
  float x;
  float y;
  float z;
  float w;

  memcpy(car, mtx, 0x40);
  func_8021FF90(mtx, car + 0x1d8);
  q = (float *)(car + 0x1d8);
  x = q[0];
  y = q[1];
  z = q[2];
  w = q[3];
  ((float *)(car + 0x274))[0] = ((float *)(car + 0x2b8))[0] = x;
  ((float *)(car + 0x274))[1] = ((float *)(car + 0x2b8))[1] = y;
  ((float *)(car + 0x274))[2] = ((float *)(car + 0x2b8))[2] = z;
  ((float *)(car + 0x274))[3] = ((float *)(car + 0x2b8))[3] = w;
  BrQuatToMat(car + 0x204, car + 0x1c0);
}

