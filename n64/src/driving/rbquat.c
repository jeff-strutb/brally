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
typedef struct BrRbBody {       /* a rigid body's shape and mass properties */
  int x0[6];
  int x18;
  int shape;                    /* 0x1C  0, 1: box; 2: fixed (no inverse inertia) */
  float w, h, d;                /* 0x20  box size */
  float mass;                   /* 0x2C */
  float I[3][3];                /* 0x30  inertia tensor */
  float Iinv[3][3];             /* 0x54  its inverse (diagonal) */
  char pad78[0x19C - 0x78];
  int x19c;                     /* 0x19C */
  char pad1a0[0x1B4 - 0x1A0];
  int x1b4;                     /* 0x1B4 */
  char pad1b8[0x1C0 - 0x1B8];
  float x1c0;                   /* 0x1C0 */
  float x1c4;                   /* 0x1C4 */
  float x1c8;                   /* 0x1C8 */
  float x1cc;
  float x1d0;
  float x1d4;
  float x1d8;
} BrRbBody;
void BrStub80258070(float m[3][3]);
typedef struct BrRbState {      /* a full rigid-body state (0x44 bytes) */
  float pos[3];                 /* 0x00 */
  float vel[3];                 /* 0x0C */
  float q[4];                   /* 0x18  orientation */
  float omega[3];               /* 0x28  angular velocity */
  float qdot[4];                /* 0x34  orientation rate */
} BrRbState;
float sqrtf(float x);
/* -- end declarations -- */

/* WHAT IT DOES: Multiply two quaternions (w, x, y, z): out = a * b.  The
 * PC twin's source (br_vecnorm.c) compiles to the same bytes. */
/* @implements 0x80258080 tgr BrQuatMul */
void BrQuatMul(float out[4], float a[4], float b[4])
{
  float b0 = b[0];
  float b1 = b[1];
  float b2 = b[2];
  float b3 = b[3];
  float a0 = a[0];
  float a1 = a[1];
  float a2 = a[2];
  float a3 = a[3];
  float r0, r1, r2;

  r0 = a0 * b0 - a1 * b1 - a2 * b2 - a3 * b3;
  r1 = a1 * b0 + a0 * b1 - a3 * b2 + a2 * b3;
  r2 = a2 * b0 + a3 * b1 + a0 * b2 - a1 * b3;
  out[3] = a3 * b0 - a2 * b1 + a1 * b2 + a0 * b3;
  out[2] = r2;
  out[1] = r1;
  out[0] = r0;
}

/* WHAT IT DOES: Scale a 4-vector (a quaternion) to unit length. */
/* @implements 0x8025813C tgr BrVec4Normalise */
void BrVec4Normalise(float v[4])
{
  float k;

  k = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2] + v[3] * v[3]);
  k = 1.0f / k;
  v[0] *= k;
  v[1] *= k;
  v[2] *= k;
  v[3] *= k;
}

/* WHAT IT DOES: Scale a 3-vector to unit length. */
/* @implements 0x802581CC tgr BrVec3NormaliseF */
void BrVec3NormaliseF(float v[3])
{
  float k;

  k = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  k = 1.0f / k;
  v[0] *= k;
  v[1] *= k;
  v[2] *= k;
}


/* WHAT IT DOES: Compute a rigid body's orientation rate: the quaternion
 * derivative from its angular velocity (half the product of the spin and
 * the orientation). */
/* @implements 0x80258248 tgr BrRbQuatDerivative */
void BrRbQuatDerivative(BrRbState *b)
{
  float h[3];

  h[0] = b->omega[0] * 0.5f;
  h[1] = b->omega[1] * 0.5f;
  h[2] = b->omega[2] * 0.5f;
  b->qdot[0] = -h[0] * b->q[1] - h[1] * b->q[2] - h[2] * b->q[3];
  b->qdot[1] = b->q[0] * h[0] + h[1] * b->q[3] - h[2] * b->q[2];
  b->qdot[2] = b->q[0] * h[1] + h[2] * b->q[1] - h[0] * b->q[3];
  b->qdot[3] = b->q[0] * h[2] + h[0] * b->q[2] - h[1] * b->q[1];
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


/* WHAT IT DOES: Step a rigid-body state by dt (Euler): position by
 * velocity, orientation by its rate (renormalised); velocity, angular
 * velocity and orientation rate are carried over. */
/* @implements 0x80258530 tgr BrRbStateStep */
void BrRbStateStep(BrRbState *out, BrRbState *in, float dt)
{
  float dp[3];
  float dq[4];

  dp[0] = in->vel[0] * dt;
  dp[1] = in->vel[1] * dt;
  dp[2] = in->vel[2] * dt;
  out->pos[0] = in->pos[0] + dp[0];
  out->pos[1] = in->pos[1] + dp[1];
  out->pos[2] = in->pos[2] + dp[2];
  out->vel[0] = in->vel[0];
  out->vel[1] = in->vel[1];
  out->vel[2] = in->vel[2];
  dq[0] = in->qdot[0] * dt;
  dq[1] = in->qdot[1] * dt;
  dq[2] = in->qdot[2] * dt;
  dq[3] = in->qdot[3] * dt;
  out->q[0] = in->q[0] + dq[0];
  out->q[1] = in->q[1] + dq[1];
  out->q[2] = in->q[2] + dq[2];
  out->q[3] = in->q[3] + dq[3];
  BrVec4Normalise(out->q);
  out->omega[0] = in->omega[0];
  out->omega[1] = in->omega[1];
  out->omega[2] = in->omega[2];
  out->qdot[0] = in->qdot[0];
  out->qdot[1] = in->qdot[1];
  out->qdot[2] = in->qdot[2];
  out->qdot[3] = in->qdot[3];
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


/* WHAT IT DOES: Set up a rigid body at rest: state cleared, damping-like
 * constants 0.174 and 0.5, and for a box the inertia tensor of a solid
 * cuboid (mass / 12 * the sum of the other two sides squared) and, unless
 * the body is fixed, its diagonal inverse.
 * The squared sides are a vector whose address is taken, so they live in
 * stack homes (h*h at sp+0x2C, w then w*w at sp+0x24) and each sum keeps
 * its written operand order (d first), as the ROM has it; an array element
 * would be loaded first.  k is first set to the side d, which numbers the
 * side's load before d*d and gives the ROM's f14/f16; k = 1/12 then
 * k *= mass orders the constant and mass loads as the ROM does. */
/* @implements 0x80258C24 tgr BrRbBodyInit */
void BrRbBodyInit(BrRbBody *b)
{
  int i;
  int j;
  float d;
  float k;
  BrVec3 s;
  BrVec3 *p;

  b->x0[0] = 0;
  b->x0[1] = 0;
  b->x0[2] = 0;
  b->x0[3] = 0;
  b->x0[4] = 0;
  b->x0[5] = 0;
  b->x1b4 = 0;
  b->x19c = 0;
  b->x1c4 = 0.0f;
  b->x1c0 = 0.174f;
  b->x1cc = 0.0f;
  b->x1d0 = 0.0f;
  b->x1c8 = 0.5f;
  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++) {
      if (i == j) {
        b->I[i][j] = 1.0f;
      } else {
        b->I[i][j] = 0.0f;
      }
    }
  }
  k = b->d;
  switch (b->shape) {
  case 0:
  case 1:
    d = b->d * b->d;
    s.z = b->h * b->h;
    k = 0.083333336f;
    k *= b->mass;
    b->I[0][0] = (d + s.z) * k;
    s.x = b->w;
    s.x = s.x * s.x;
    b->I[1][1] = (d + s.x) * k;
    b->I[2][2] = (s.z + s.x) * k;
    p = &s;
    BrStub80258070(b->I);
    break;
  }
  if (b->shape != 2) {
    b->Iinv[0][0] = 1.0f / b->I[0][0];
    b->Iinv[1][1] = 1.0f / b->I[1][1];
    b->Iinv[2][2] = 1.0f / b->I[2][2];
  }
  b->x1d4 = 0.0f;
  b->x1d8 = 0.0f;
}
