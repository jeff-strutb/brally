/* rbforce.c -- forces acting on the rigid-body car model
 */
#include "tgr/common.h"

/* -- declarations -- */
typedef struct BrRbBody {       /* a rigid body with up to four attached */
  int x0;
  struct BrRbBody *sub[4];      /* 0x04 */
  char pad14[0xfc - 0x14];
  float force[3];               /* 0xFC  accumulated this step */
  float torque[3];              /* 0x108 */
} BrRbBody;
void func_802589F4(int param_1,int param_2);
void func_802594BC(void);
void func_80259634(BrRbBody *b, BrRbBody *sub);
void func_8025980C(BrRbBody *b);
/* -- end declarations -- */

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
  func_8025980C(b);
}

