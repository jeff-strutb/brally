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
typedef struct BrPhysBody {     /* a rigid body as the car embeds it (0x208 bytes) */
  char pad00[0x1C];
  int shape;                    /* 0x1C  1: box; 2: wheel */
  float w, h, d;                /* 0x20  box size */
  float mass;                   /* 0x2C */
  char pad30[0x78 - 0x30];
  float pos[3];                 /* 0x78  state: position */
  float vel[3];                 /* 0x84 */
  float q[4];                   /* 0x90  orientation */
  float omega[3];               /* 0xA0 */
  float qdot[4];                /* 0xAC */
  float mtx[4][4];              /* 0xBC */
  char padfc[0x19C - 0xFC];
  int x19c;                     /* 0x19C */
  char pad1a0[0x1B8 - 0x1A0];
  float spring;                 /* 0x1B8 */
  float damper;                 /* 0x1BC */
  char pad1c0[0x1C4 - 0x1C0];
  float x1c4;                   /* 0x1C4 */
  char pad1c8[0x1D8 - 0x1C8];
  float x1d8;                   /* 0x1D8 */
  char pad1dc[0x208 - 0x1DC];
} BrPhysBody;
typedef struct BrCarMounts {    /* the wheel mounts in a car's model buffer */
  char pad00[0xEC];
  float rearX;                  /* 0xEC */
  float rearY;                  /* 0xF0 */
  char padf4[0xF8 - 0xF4];
  float frontX;                 /* 0xF8 */
  float frontY;                 /* 0xFC */
} BrCarMounts;
void BrRbBodyInit(void *body);
void BrRbSetParams(void *force, float fx, float fy, float fz, float rx, float ry, float rz, int frame);
#define CP_BODY(car, off) ((BrPhysBody *)CP_AT(car, off))
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

/* WHAT IT DOES: Set up a car's rigid bodies for a race: the chassis (a
 * 3.5 by 2 by 1.5 box of 1000 at 2 up, level, with its spring set from the
 * car's class and its damper), each wheel (a fixed body at its mount from
 * the model, 0.1 down, the right side mirrored) and the force lists (the
 * four corners then weight, a drag list, the wheels' tyre nodes on shared
 * weight nodes); the tyre pass is skipped on the first frame.  The PC
 * twin is BrCarPhysInit (a port-side reconstruction).
 * RESIDUE (243): the ROM keeps 2.0 in f22 across the chassis init call (for
 * the start height) and 1.0 in f24, one frame slot more; ours reloads 2.0,
 * which shifts the register numbering and scheduling of the store runs. */
/* @t3 0x80222D54 */
/* @t4-pass 0x80222D54 1 2026-09-29 compiles 231 best 243 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80222D54 2 2026-09-29 compiles 231 best 243 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x80222D54 tgr BrCarPhysInit */
void BrCarPhysInit(BrCar *car)
{
  *(int *)CP_AT(car, 0xE54) = 1;
  *(int *)CP_AT(car, 0xE40) = 0;
  *(float *)CP_AT(car, 0xDF0) = 0.0f;
  *(float *)CP_AT(car, 0xDF4) = 0.0f;
  if (car->model != 0) {
    CP_BODY(car, 0x148)->shape = 1;
    CP_BODY(car, 0x148)->h = 2.0f;
    CP_BODY(car, 0x148)->mass = 1000.0f;
    CP_BODY(car, 0x148)->w = 3.5f;
    CP_BODY(car, 0x148)->d = 1.5f;
    BrRbBodyInit(CP_BODY(car, 0x148));
    CP_BODY(car, 0x148)->pos[0] = 0.0f;
    CP_BODY(car, 0x148)->pos[1] = 0.0f;
    CP_BODY(car, 0x148)->pos[2] = 2.0f;
    CP_BODY(car, 0x148)->vel[0] = 0.0f;
    CP_BODY(car, 0x148)->vel[1] = 0.0f;
    CP_BODY(car, 0x148)->vel[2] = 0.0f;
    CP_BODY(car, 0x148)->q[1] = 0.0f;
    CP_BODY(car, 0x148)->q[2] = 0.0f;
    CP_BODY(car, 0x148)->q[3] = 0.0f;
    CP_BODY(car, 0x148)->omega[0] = 0.0f;
    CP_BODY(car, 0x148)->omega[1] = 0.0f;
    CP_BODY(car, 0x148)->omega[2] = 0.0f;
    CP_BODY(car, 0x148)->q[0] = 1.0f;
    BrQuatToMat((float *)CP_BODY(car, 0x148)->mtx, CP_BODY(car, 0x148)->pos);
    CP_BODY(car, 0x148)->spring = (car->xe64 * 4.0f + 20.0f) * 16000.0f;
    CP_BODY(car, 0x148)->damper = -3000.0f;
    CP_BODY(car, 0x350)->x19c = 0;
    CP_BODY(car, 0x350)->shape = 2;
    CP_BODY(car, 0x350)->mass = 0.0f;
    CP_BODY(car, 0x350)->w = 0.0f;
    CP_BODY(car, 0x350)->h = 0.0f;
    CP_BODY(car, 0x350)->d = 0.0f;
    CP_BODY(car, 0x350)->x1d8 = 0.0f;
    CP_BODY(car, 0x350)->x1c4 = 0.0f;
    BrRbBodyInit(CP_BODY(car, 0x350));
    CP_BODY(car, 0x350)->pos[0] = ((BrCarMounts *)car->model)->frontX;
    CP_BODY(car, 0x350)->pos[1] = ((BrCarMounts *)car->model)->frontY;
    CP_BODY(car, 0x350)->vel[2] = 0.0f;
    CP_BODY(car, 0x350)->vel[1] = 0.0f;
    CP_BODY(car, 0x350)->vel[0] = 0.0f;
    CP_BODY(car, 0x350)->q[3] = 0.0f;
    CP_BODY(car, 0x350)->q[2] = 0.0f;
    CP_BODY(car, 0x350)->q[1] = 0.0f;
    CP_BODY(car, 0x350)->q[0] = 1.0f;
    CP_BODY(car, 0x350)->omega[2] = 0.0f;
    CP_BODY(car, 0x350)->omega[1] = 0.0f;
    CP_BODY(car, 0x350)->omega[0] = 0.0f;
    CP_BODY(car, 0x350)->pos[2] = -0.1f;
    BrQuatToMat((float *)CP_BODY(car, 0x350)->mtx, CP_BODY(car, 0x350)->pos);
    CP_BODY(car, 0x760)->x19c = 0;
    CP_BODY(car, 0x760)->shape = 2;
    CP_BODY(car, 0x760)->mass = 0.0f;
    CP_BODY(car, 0x760)->w = 0.0f;
    CP_BODY(car, 0x760)->h = 0.0f;
    CP_BODY(car, 0x760)->d = 0.0f;
    CP_BODY(car, 0x760)->x1d8 = 0.0f;
    CP_BODY(car, 0x760)->x1c4 = 0.0f;
    BrRbBodyInit(CP_BODY(car, 0x760));
    CP_BODY(car, 0x760)->pos[0] = ((BrCarMounts *)car->model)->frontX;
    CP_BODY(car, 0x760)->pos[1] = -((BrCarMounts *)car->model)->frontY;
    CP_BODY(car, 0x760)->vel[2] = 0.0f;
    CP_BODY(car, 0x760)->vel[1] = 0.0f;
    CP_BODY(car, 0x760)->vel[0] = 0.0f;
    CP_BODY(car, 0x760)->q[3] = 0.0f;
    CP_BODY(car, 0x760)->q[2] = 0.0f;
    CP_BODY(car, 0x760)->q[1] = 0.0f;
    CP_BODY(car, 0x760)->q[0] = 1.0f;
    CP_BODY(car, 0x760)->omega[2] = 0.0f;
    CP_BODY(car, 0x760)->omega[1] = 0.0f;
    CP_BODY(car, 0x760)->omega[0] = 0.0f;
    CP_BODY(car, 0x760)->pos[2] = -0.1f;
    BrQuatToMat((float *)CP_BODY(car, 0x760)->mtx, CP_BODY(car, 0x760)->pos);
    CP_BODY(car, 0x558)->x19c = 0;
    CP_BODY(car, 0x558)->shape = 2;
    CP_BODY(car, 0x558)->mass = 0.0f;
    CP_BODY(car, 0x558)->w = 0.0f;
    CP_BODY(car, 0x558)->h = 0.0f;
    CP_BODY(car, 0x558)->d = 0.0f;
    CP_BODY(car, 0x558)->x1d8 = 0.0f;
    CP_BODY(car, 0x558)->x1c4 = 0.0f;
    BrRbBodyInit(CP_BODY(car, 0x558));
    CP_BODY(car, 0x558)->pos[0] = ((BrCarMounts *)car->model)->rearX;
    CP_BODY(car, 0x558)->pos[1] = ((BrCarMounts *)car->model)->rearY;
    CP_BODY(car, 0x558)->vel[2] = 0.0f;
    CP_BODY(car, 0x558)->vel[1] = 0.0f;
    CP_BODY(car, 0x558)->vel[0] = 0.0f;
    CP_BODY(car, 0x558)->q[3] = 0.0f;
    CP_BODY(car, 0x558)->q[2] = 0.0f;
    CP_BODY(car, 0x558)->q[1] = 0.0f;
    CP_BODY(car, 0x558)->q[0] = 1.0f;
    CP_BODY(car, 0x558)->omega[2] = 0.0f;
    CP_BODY(car, 0x558)->omega[1] = 0.0f;
    CP_BODY(car, 0x558)->omega[0] = 0.0f;
    CP_BODY(car, 0x558)->pos[2] = -0.1f;
    BrQuatToMat((float *)CP_BODY(car, 0x558)->mtx, CP_BODY(car, 0x558)->pos);
    CP_BODY(car, 0x968)->x19c = 0;
    CP_BODY(car, 0x968)->shape = 2;
    CP_BODY(car, 0x968)->mass = 0.0f;
    CP_BODY(car, 0x968)->w = 0.0f;
    CP_BODY(car, 0x968)->h = 0.0f;
    CP_BODY(car, 0x968)->d = 0.0f;
    CP_BODY(car, 0x968)->x1d8 = 0.0f;
    CP_BODY(car, 0x968)->x1c4 = 0.0f;
    BrRbBodyInit(CP_BODY(car, 0x968));
    CP_BODY(car, 0x968)->pos[0] = ((BrCarMounts *)car->model)->rearX;
    CP_BODY(car, 0x968)->pos[1] = -((BrCarMounts *)car->model)->rearY;
    CP_BODY(car, 0x968)->vel[2] = 0.0f;
    CP_BODY(car, 0x968)->vel[1] = 0.0f;
    CP_BODY(car, 0x968)->vel[0] = 0.0f;
    CP_BODY(car, 0x968)->q[3] = 0.0f;
    CP_BODY(car, 0x968)->q[2] = 0.0f;
    CP_BODY(car, 0x968)->q[1] = 0.0f;
    CP_BODY(car, 0x968)->q[0] = 1.0f;
    CP_BODY(car, 0x968)->omega[2] = 0.0f;
    CP_BODY(car, 0x968)->omega[1] = 0.0f;
    CP_BODY(car, 0x968)->omega[0] = 0.0f;
    CP_BODY(car, 0x968)->pos[2] = -0.1f;
    BrQuatToMat((float *)CP_BODY(car, 0x968)->mtx, CP_BODY(car, 0x968)->pos);
    car->wheel[0] = (struct BrCarWheel *)CP_AT(car, 0x350);
    car->wheel[1] = (struct BrCarWheel *)CP_AT(car, 0x760);
    car->wheel[2] = (struct BrCarWheel *)CP_AT(car, 0x558);
    car->wheel[3] = (struct BrCarWheel *)CP_AT(car, 0x968);
    BrRbSetParams(CP_AT(car, 0xB70), 0.0f, 0.0f, 0.0f, -1.5f, -1.0f, 0.0f, 1);
    BrRbSetParams(CP_AT(car, 0xBB0), 0.0f, 0.0f, 0.0f, -1.5f, 1.0f, 0.0f, 1);
    BrRbSetParams(CP_AT(car, 0xB90), 0.0f, 0.0f, 0.0f, 1.5f, -1.0f, 0.0f, 1);
    BrRbSetParams(CP_AT(car, 0xBD0), 0.0f, 0.0f, 0.0f, 1.5f, 1.0f, 0.0f, 1);
    BrRbSetParams(CP_AT(car, 0xC70), 0.0f, 0.0f, -15450.75f, 0.0f, 0.0f, 0.0f, 0);
    BrRbSetParams(CP_AT(car, 0xC90), 0.0f, 0.0f, -103.005005f, 0.0f, 0.0f, 0.0f, 0);
    BrRbSetParams(CP_AT(car, 0xCB0), 0.0f, 0.0f, -103.005005f, 0.0f, 0.0f, 0.0f, 0);
    *(char **)CP_AT(car, 0xB70) = CP_AT(car, 0xBB0);
    *(char **)CP_AT(car, 0xBB0) = CP_AT(car, 0xB90);
    *(char **)CP_AT(car, 0xB90) = CP_AT(car, 0xBD0);
    *(char **)CP_AT(car, 0xC70) = 0;
    *(char **)CP_AT(car, 0xBD0) = CP_AT(car, 0xC70);
    *(char **)CP_AT(car, 0x160) = CP_AT(car, 0xB70);
    BrRbSetParams(CP_AT(car, 0xCF0), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1);
    BrRbSetParams(CP_AT(car, 0xD30), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1);
    BrRbSetParams(CP_AT(car, 0xD10), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1);
    BrRbSetParams(CP_AT(car, 0xD50), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1);
    *(char **)CP_AT(car, 0x368) = CP_AT(car, 0xCF0);
    *(char **)CP_AT(car, 0x778) = CP_AT(car, 0xD30);
    *(char **)CP_AT(car, 0x570) = CP_AT(car, 0xD10);
    *(char **)CP_AT(car, 0xCB0) = 0;
    *(char **)CP_AT(car, 0xC90) = 0;
    *(char **)CP_AT(car, 0xCF0) = CP_AT(car, 0xCB0);
    *(char **)CP_AT(car, 0xD30) = CP_AT(car, 0xCB0);
    *(char **)CP_AT(car, 0xD10) = CP_AT(car, 0xC90);
    *(char **)CP_AT(car, 0xD50) = CP_AT(car, 0xC90);
    *(char **)CP_AT(car, 0x980) = CP_AT(car, 0xD50);
    BrRbSetParams(CP_AT(car, 0xBF0), 0.0f, 0.0f, 0.0f, -1.5f, -1.0f, 0.0f, 1);
    BrRbSetParams(CP_AT(car, 0xC30), 0.0f, 0.0f, 0.0f, -1.5f, 1.0f, 0.0f, 1);
    BrRbSetParams(CP_AT(car, 0xC10), 0.0f, 0.0f, 0.0f, 1.5f, -1.0f, 0.0f, 1);
    BrRbSetParams(CP_AT(car, 0xC50), 0.0f, 0.0f, 0.0f, 1.5f, 1.0f, 0.0f, 1);
    BrRbSetParams(CP_AT(car, 0xCD0), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0);
    *(char **)CP_AT(car, 0xBF0) = CP_AT(car, 0xC30);
    *(char **)CP_AT(car, 0xC30) = CP_AT(car, 0xC10);
    *(char **)CP_AT(car, 0xCD0) = 0;
    *(unsigned char *)CP_AT(car, 0xE51) = 0;
    *(float *)CP_AT(car, 0xE44) = 0.0f;
    *(float *)CP_AT(car, 0xE4C) = 0.0f;
    *(char **)CP_AT(car, 0xC50) = CP_AT(car, 0xCD0);
    *(unsigned char *)CP_AT(car, 0x345) = car->xe60;
    *(char **)CP_AT(car, 0xC10) = CP_AT(car, 0xC50);
  }
  *(unsigned char *)CP_AT(car, 0xE50) = 0;
  *(unsigned char *)CP_AT(car, 0xE48) = 0;
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
