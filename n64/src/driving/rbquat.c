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
typedef struct BrPose {         /* position and orientation */
  BrVec3 pos;                   /* 0x00 */
  char pad0c[0xc];
  BrRbQuat q;                   /* 0x18 */
} BrPose;
typedef struct BrRbRates {      /* what a rigid body integrates from */
  char pad00[0xfc];
  float vel[3];                 /* 0xFC  linear */
  float spin[3];                /* 0x108 angular */
} BrRbRates;
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
/* @implements 0x80258324 tgr BrRbIntegrate */
void BrRbIntegrate(BrRb *b, BrRbRates *r, float dt)
{
  float dv[3];
  float dw[3];

  dv[0] = r->vel[0] * dt;
  dv[1] = r->vel[1] * dt;
  dv[2] = r->vel[2] * dt;
  dw[0] = r->spin[0] * dt;
  dw[1] = r->spin[1] * dt;
  dw[2] = r->spin[2] * dt;
  b->pos.x = dv[0] + b->pos.x;
  b->pos.y = dv[1] + b->pos.y;
  b->pos.z = dv[2] + b->pos.z;
  b->omega.x = dw[0] + b->omega.x;
  b->omega.y = dw[1] + b->omega.y;
  b->omega.z = dw[2] + b->omega.z;
}


/* WHAT IT DOES: Build the 3x3 rotation matrix of a unit quaternion. */
/* @implements 0x802583DC tgr BrQuatToMat */
void BrQuatToMat(float m[4][4], BrPose *p)
{
  float ww;
  float xx;
  float yy;
  float zz;
  float xy2;
  float wz2;
  float xz2;
  float wy2;
  float yz2;
  float wx2;

  ww = p->q.w * p->q.w;
  xx = p->q.x * p->q.x;
  yy = p->q.y * p->q.y;
  zz = p->q.z * p->q.z;
  xy2 = p->q.y * (p->q.x * 2.0f);
  wz2 = p->q.z * (p->q.w * 2.0f);
  xz2 = p->q.z * (p->q.x * 2.0f);
  wy2 = p->q.y * (p->q.w * 2.0f);
  yz2 = p->q.z * (p->q.y * 2.0f);
  wx2 = p->q.x * (p->q.w * 2.0f);
  m[0][0] = ww + xx - yy - zz;
  m[0][1] = xy2 + wz2;
  m[0][2] = xz2 - wy2;
  m[0][3] = 0.0f;
  m[1][0] = xy2 - wz2;
  m[1][1] = ww - xx + yy - zz;
  m[1][2] = yz2 + wx2;
  m[1][3] = 0.0f;
  m[2][0] = xz2 + wy2;
  m[2][1] = yz2 - wx2;
  m[2][2] = ww - xx - yy + zz;
  m[2][3] = 0.0f;
  m[3][0] = p->pos.x;
  m[3][1] = p->pos.y;
  m[3][2] = p->pos.z;
  m[3][3] = 1.0f;
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

