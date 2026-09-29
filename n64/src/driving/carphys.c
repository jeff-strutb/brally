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
void BrPerfMark(int bar, int r, int g, int b, int a);
void BrTyreSprings(void *body);
void func_8025AC9C(void *body, void *wheel, float *grip, unsigned char *slip, float dt);
void BrRbForcesClear(void *body);
void BrRbIntegrate(void *state, void *body, float dt);
void func_80259D14(void *body, float dt, float *gripF, float *gripR, unsigned char *slipF, unsigned char *slipR);
void BrRbQuatDerivative(void *state);
void BrTyreSkidCheck(void *body, void *force);
void BrTyreLoads(void *body);
void BrCarPhysAdvance(void *body);
void BrTyreDepthAll(void *body);
typedef struct BrCarWheel {     /* a wheel's rigid body */
  char pad00[0x18];
  char *forces;                 /* 0x18  its force list */
  char pad1c[0x78 - 0x1C];
  char state[0x44];             /* 0x78 */
  float mtx[4][4];              /* 0xBC */
} BrCarWheel;
#define CP_AT(car, off) ((char *)(car) + (off))
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


/* WHAT IT DOES: Advance one car's rigid-body physics by a frame: hang the
 * wheel force lists off the body and zero them, run the force generators
 * (springs, the four tyre passes at most once a frame, drive, skid, loads),
 * integrate the body's velocity and orientation into a scratch copy and the
 * live state with a sign-change damper between them, advance the car, keep
 * the suspension depths, and rebuild each wheel's matrix.  Ported from the
 * PC twin (BrCarPhysStep); tmp and u are declared and unused (the ROM frame
 * keeps their slots). */
/* @implements 0x80221940 tgr BrCarPhysStep */
void BrCarPhysStep(BrCar *car)
{
  BrRbState tmp;
  short i;
  int u[2];

  BrPerfMark(0, 0x80, 0x80, 0, 0xFF);
  *(char **)CP_AT(car, 0x160) = CP_AT(car, 0xB70);
  car->wheel[0]->forces = CP_AT(car, 0xCF0);
  car->wheel[1]->forces = CP_AT(car, 0xD30);
  car->wheel[2]->forces = CP_AT(car, 0xD10);
  car->wheel[3]->forces = CP_AT(car, 0xD50);
  *(float *)(car->wheel[0]->forces + 0x08) = 0.0f;
  *(float *)(car->wheel[0]->forces + 0x0C) = 0.0f;
  *(float *)(car->wheel[0]->forces + 0x10) = 0.0f;
  *(float *)(car->wheel[1]->forces + 0x08) = 0.0f;
  *(float *)(car->wheel[1]->forces + 0x0C) = 0.0f;
  *(float *)(car->wheel[1]->forces + 0x10) = 0.0f;
  *(float *)(car->wheel[2]->forces + 0x08) = 0.0f;
  *(float *)(car->wheel[2]->forces + 0x0C) = 0.0f;
  *(float *)(car->wheel[2]->forces + 0x10) = 0.0f;
  *(float *)(car->wheel[3]->forces + 0x08) = 0.0f;
  *(float *)(car->wheel[3]->forces + 0x0C) = 0.0f;
  *(float *)(car->wheel[3]->forces + 0x10) = 0.0f;
  BrTyreSprings(CP_AT(car, 0x148));
  if (*(int *)CP_AT(car, 0xE54) == 0) {
    *(float *)CP_AT(car, 0xE44) = 0.0f;
    *(float *)CP_AT(car, 0xE4C) = 0.0f;
    *(unsigned char *)CP_AT(car, 0xE50) = 0;
    func_8025AC9C(CP_AT(car, 0x148), car->wheel[0], (float *)CP_AT(car, 0xE4C), (unsigned char *)CP_AT(car, 0xE50), 0.033333335f);
    func_8025AC9C(CP_AT(car, 0x148), car->wheel[1], (float *)CP_AT(car, 0xE4C), (unsigned char *)CP_AT(car, 0xE50), 0.033333335f);
    func_8025AC9C(CP_AT(car, 0x148), car->wheel[2], (float *)CP_AT(car, 0xE44), (unsigned char *)CP_AT(car, 0xE48), 0.033333335f);
    func_8025AC9C(CP_AT(car, 0x148), car->wheel[3], (float *)CP_AT(car, 0xE44), (unsigned char *)CP_AT(car, 0xE48), 0.033333335f);
  }
  *(int *)CP_AT(car, 0xE54) = 0;
  *(float *)CP_AT(car, 0x244) = 0.0f;
  *(float *)CP_AT(car, 0x248) = 0.0f;
  *(float *)CP_AT(car, 0x24C) = 0.0f;
  *(float *)CP_AT(car, 0x250) = 0.0f;
  *(float *)CP_AT(car, 0x254) = 0.0f;
  *(float *)CP_AT(car, 0x258) = 0.0f;
  BrRbForcesClear(CP_AT(car, 0x148));
  BrRbIntegrate(&car->st, CP_AT(car, 0x148), 0.033333335f);
  func_80259D14(CP_AT(car, 0x148), 0.033333335f, (float *)CP_AT(car, 0xE4C), (float *)CP_AT(car, 0xE44),
                (unsigned char *)CP_AT(car, 0xE50), (unsigned char *)CP_AT(car, 0xE48));
  BrRbQuatDerivative(&car->st);
  *(char **)CP_AT(car, 0x160) = CP_AT(car, 0xBF0);
  car->wheel[0]->forces = 0;
  car->wheel[2]->forces = 0;
  car->wheel[1]->forces = 0;
  car->wheel[3]->forces = 0;
  BrTyreSkidCheck(CP_AT(car, 0x148), CP_AT(car, 0xCD0));
  BrTyreLoads(CP_AT(car, 0x148));
  *(float *)CP_AT(car, 0x244) = 0.0f;
  *(float *)CP_AT(car, 0x248) = 0.0f;
  *(float *)CP_AT(car, 0x24C) = 0.0f;
  *(float *)CP_AT(car, 0x250) = 0.0f;
  *(float *)CP_AT(car, 0x254) = 0.0f;
  *(float *)CP_AT(car, 0x258) = 0.0f;
  BrRbForcesClear(CP_AT(car, 0x148));
  memcpy((char *)&car->stB, (char *)&car->st, sizeof(BrRbState));
  BrRbIntegrate(&car->stB, CP_AT(car, 0x148), 0.033333335f);
  for (i = 0; i < 3; i++) {
    if (((&car->st.angVel.x)[i] == 0.0f ? 0.0f : ((&car->st.angVel.x)[i] > 0.0f ? 1.0f : -1.0f)) !=
        ((&car->stB.angVel.x)[i] == 0.0f ? 0.0f : ((&car->stB.angVel.x)[i] > 0.0f ? 1.0f : -1.0f))) {
      (&car->st.angVel.x)[i] = 0.0f;
    } else {
      (&car->st.angVel.x)[i] = (&car->stB.angVel.x)[i];
    }
    if (((&car->st.vel.x)[i] == 0.0f ? 0.0f : ((&car->st.vel.x)[i] > 0.0f ? 1.0f : -1.0f)) !=
        ((&car->stB.vel.x)[i] == 0.0f ? 0.0f : ((&car->stB.vel.x)[i] > 0.0f ? 1.0f : -1.0f))) {
      (&car->st.vel.x)[i] = 0.0f;
    } else {
      (&car->st.vel.x)[i] = (&car->stB.vel.x)[i];
    }
  }
  memcpy((char *)&car->stA, (char *)&car->st, sizeof(BrRbState));
  BrRbQuatDerivative(&car->stA);
  BrRbQuatDerivative(&car->stA);
  BrCarPhysAdvance(CP_AT(car, 0x148));
  memcpy((char *)&car->st, (char *)&car->stB, sizeof(BrRbState));
  BrTyreDepthAll(CP_AT(car, 0x148));
  BrQuatToMat((float *)car->wheel[0]->mtx, (float *)car->wheel[0]->state);
  BrQuatToMat((float *)car->wheel[1]->mtx, (float *)car->wheel[1]->state);
  BrQuatToMat((float *)car->wheel[2]->mtx, (float *)car->wheel[2]->state);
  BrQuatToMat((float *)car->wheel[3]->mtx, (float *)car->wheel[3]->state);
  BrPerfMark(0, 0, 0x80, 0, 0xFF);
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
