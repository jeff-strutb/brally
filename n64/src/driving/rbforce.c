/* rbforce.c -- forces acting on the rigid-body car model
 */
#include "tgr/common.h"

/* -- declarations -- */
typedef struct BrRbBody {       /* a rigid body with up to four attached */
  int x0;
  struct BrRbBody *sub[4];      /* 0x04 */
  char pad14[0x1c - 0x14];
  int kind;                     /* 0x1C  2: does not rotate */
  char pad20[0x2c - 0x20];
  float mass;                   /* 0x2C */
  char pad30[0x54 - 0x30];
  float Iinv[3][3];             /* 0x54  inverse inertia */
  char pad78[0xbc - 0x78];
  float m[4][4];                /* 0xBC  orientation */
  float force[3];               /* 0xFC  accumulated this step */
  float torque[3];              /* 0x108 */
} BrRbBody;
void func_802589F4(int param_1,int param_2);
void func_802594BC(void);
void func_80259634(BrRbBody *b, BrRbBody *sub);
void BrRbSolveAccel(BrRbBody *b);
void func_802586C0(float out[3], float m[4][4], float v[3]);   /* v into the body frame */
void func_80258758(float out[3], float m[4][4], float v[3]);   /* and back out */
void BrMat3MulVec(float out[3], float m[3][3], float v[3]);
/* -- end declarations -- */

/* WHAT IT DOES: Turn a body's accumulated force and torque into
 * accelerations: force over mass, and (unless the body does not rotate)
 * torque taken into the body frame, through the inverse inertia and back
 * out. */
/* @implements 0x80258950 tgr BrRbAccel */
void BrRbAccel(BrRbBody *b)
{
  float bodyT[3];
  float acc[3];

  b->force[0] *= 1.0f / b->mass;
  b->force[1] *= 1.0f / b->mass;
  b->force[2] *= 1.0f / b->mass;
  if (b->kind != 2) {
    func_802586C0(bodyT, b->m, b->torque);
    BrMat3MulVec(acc, (float (*)[3])b->Iinv, bodyT);
    func_80258758(b->torque, b->m, acc);
  }
}

/* WHAT IT DOES: Apply every force attached to a rigid body for this step:
 * walks the body's list of force records and adds each one in turn. */
/* @implements 0x80258BDC tgr BrRbApplyForces */
void BrRbApplyForces(int param_1)
{
  int *piVar1;
  
  for (piVar1 = *(int **)(param_1 + 0x18); piVar1 != (int *)0x0; piVar1 = (int *)*piVar1) {
    func_802589F4(param_1,piVar1);
  }
}

/* WHAT IT DOES: Does nothing with its argument. An empty rigid-body hook
 * the physics still calls. */
/* @implements 0x80258070 tgr BrStub80258070 */
void BrStub80258070(int arg0)
{
}

/* WHAT IT DOES: Does nothing with its argument. A second empty rigid-body
 * hook the physics still calls. */
/* @implements 0x80258078 tgr BrStub80258078 */
void BrStub80258078(int arg0)
{
}

/* WHAT IT DOES: Always returns 1 and ignores its three arguments. A
 * placeholder physics callback nothing calls directly. */
/* @implements 0x8025BBA4 tgr BrStub8025BBA4 */
int BrStub8025BBA4(int arg0,int arg1,int arg2)
{
  return 1;
}

/* WHAT IT DOES: Always returns 0 and ignores its argument. A placeholder
 * physics callback nothing calls directly. */
/* @implements 0x8025D35C tgr BrStub8025D35C */
int BrStub8025D35C(int arg0)
{
  return 0;
}

/* WHAT IT DOES: Turn a car body's summed force and torque into
 * accelerations: in the body frame the force (with its four wheels' forces
 * added on the two ground axes) is divided by the mass, the torque goes
 * through the inverse inertia, and both come back to the world frame.  The
 * PC twin is BrRbSolveAccel (br_rbaccum.c). */
/* @implements 0x8025980C tgr BrRbSolveAccel */
void BrRbSolveAccel(BrRbBody *b)
{
  float t[3];
  float spare[3];                 /* declared and unused: it only holds a stack slot */
  float u[3];
  float w[3];

  func_802586C0(t, b->m, b->force);
  b->force[0] = t[0];
  b->force[1] = t[1];
  b->force[2] = t[2];
  t[0] = (b->force[0] + b->sub[0]->force[0] + b->sub[1]->force[0] + b->sub[2]->force[0] +
          b->sub[3]->force[0]) / b->mass;
  t[1] = (b->force[1] + b->sub[0]->force[1] + b->sub[1]->force[1] + b->sub[2]->force[1] +
          b->sub[3]->force[1]) / b->mass;
  t[2] = b->force[2] / b->mass;
  func_80258758(b->force, b->m, t);
  func_802586C0(u, b->m, b->torque);
  BrMat3MulVec(w, b->Iinv, u);
  func_80258758(b->torque, b->m, w);
}

/* WHAT IT DOES: Clear the force and torque accumulators of a car body and
 * of each of its four wheel bodies before the forces are summed again. */
/* @implements 0x8025993C tgr BrRbForcesClear */
void BrRbForcesClear(BrRbBody *b)
{
  b->force[2] = b->force[1] = b->force[0] = 0.0f;
  b->torque[2] = b->torque[1] = b->torque[0] = 0.0f;
  b->sub[0]->force[0] = 0.0f;
  b->sub[0]->force[1] = 0.0f;
  b->sub[0]->force[2] = 0.0f;
  b->sub[1]->force[0] = 0.0f;
  b->sub[1]->force[1] = 0.0f;
  b->sub[1]->force[2] = 0.0f;
  b->sub[2]->force[0] = 0.0f;
  b->sub[2]->force[1] = 0.0f;
  b->sub[2]->force[2] = 0.0f;
  b->sub[3]->force[0] = 0.0f;
  b->sub[3]->force[1] = 0.0f;
  b->sub[3]->force[2] = 0.0f;
  func_802594BC();
  func_80259634(b, b->sub[0]);
  func_80259634(b, b->sub[1]);
  func_80259634(b, b->sub[2]);
  func_80259634(b, b->sub[3]);
  BrRbSolveAccel(b);
}

