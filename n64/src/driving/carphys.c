/* carphys.c -- the car's physical state
 */
#include "tgr/common.h"
#include "tgr/car.h"

/* -- declarations -- */
void BrMatToQuat(float *param_1,float *param_2);
void BrQuatToMat(float *param_1,float *param_2);
char * memcpy(char *param_1,char *param_2,int param_3);
void guRotateF(float m[4][4], float a, float x, float y, float z);
void guMtxCatF(float m[4][4], float n[4][4], float r[4][4]);
void BrMat3MulVecRows(float out[3], float m[4][4], float v[3]);
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
  BrMatToQuat(mtx, car + 0x1d8);
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


/* WHAT IT DOES: Build the car's draw matrices from its body state: the body
 * rolled by its spin angle; each wheel spun about its axle (the front two
 * also steered, at twice the steering angle), placed at its mount point in
 * the body's frame; the mount points are lowered 0.254 while this runs. */
/* @implements 0x80221E0C tgr BrCarBuildMatrices */
void BrCarBuildMatrices(BrCar *car)
{
  float spin[4][4];
  float m[4][4];
  float steer[4][4];

  guRotateF(m, car->spin, 1.0f, 0.0f, 0.0f);
  guMtxCatF(m, car->stMtx, car->mtx0);
  car->wheels[0].pos[2] += 0.254f;
  car->wheels[1].pos[2] += 0.254f;
  car->wheels[2].pos[2] += 0.254f;
  car->wheels[3].pos[2] += 0.254f;
  guRotateF(m, car->wheels[0].spin, 0.0f, 1.0f, 0.0f);
  guMtxCatF(m, car->stMtx, car->wheelMtx[2]);
  BrMat3MulVecRows(car->wheelMtx[2][3], car->stMtx, car->wheels[0].pos);
  guRotateF(m, car->wheels[2].spin, 0.0f, 1.0f, 0.0f);
  guMtxCatF(m, car->stMtx, car->wheelMtx[3]);
  BrMat3MulVecRows(car->wheelMtx[3][3], car->stMtx, car->wheels[2].pos);
  guRotateF(spin, car->wheels[1].spin, 0.0f, 1.0f, 0.0f);
  guRotateF(steer, car->wheels[1].steer * 114.59155f, 0.0f, 0.0f, 1.0f);
  guMtxCatF(spin, steer, m);
  guMtxCatF(m, car->stMtx, car->wheelMtx[1]);
  BrMat3MulVecRows(car->wheelMtx[1][3], car->stMtx, car->wheels[1].pos);
  guRotateF(spin, car->wheels[3].spin, 0.0f, 1.0f, 0.0f);
  guRotateF(steer, car->wheels[3].steer * 114.59155f, 0.0f, 0.0f, 1.0f);
  guMtxCatF(spin, steer, m);
  guMtxCatF(m, car->stMtx, car->wheelMtx[0]);
  BrMat3MulVecRows(car->wheelMtx[0][3], car->stMtx, car->wheels[3].pos);
  car->wheels[0].pos[2] -= 0.254f;
  car->wheels[1].pos[2] -= 0.254f;
  car->wheels[2].pos[2] -= 0.254f;
  car->wheels[3].pos[2] -= 0.254f;
}
