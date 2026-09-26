/* rbforce.c -- forces acting on the rigid-body car model
 */
#include "tgr/common.h"

/* -- declarations -- */
void func_802589F4(int param_1,int param_2);
void func_802594BC();
void func_80259634(int param_1,int param_2);
void func_8025980C(int param_1);
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
/* @t4-pass 0x8025993C 1 2026-09-26 compiles 17 best 53 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8025993C 2 2026-09-26 compiles 17 best 53 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8025993C 3 2026-09-26 compiles 17 best 53 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x8025993C tgr BrRbForcesClear */
void BrRbForcesClear(int param_1)
{
  *(int *)(param_1 + 0x104) = 0;
  *(int *)(param_1 + 0x100) = 0;
  *(int *)(param_1 + 0xfc) = 0;
  *(int *)(param_1 + 0x110) = 0;
  *(int *)(param_1 + 0x10c) = 0;
  *(int *)(param_1 + 0x108) = 0;
  *(int *)(*(int *)(param_1 + 4) + 0xfc) = 0;
  *(int *)(*(int *)(param_1 + 4) + 0x100) = 0;
  *(int *)(*(int *)(param_1 + 4) + 0x104) = 0;
  *(int *)(*(int *)(param_1 + 8) + 0xfc) = 0;
  *(int *)(*(int *)(param_1 + 8) + 0x100) = 0;
  *(int *)(*(int *)(param_1 + 8) + 0x104) = 0;
  *(int *)(*(int *)(param_1 + 0xc) + 0xfc) = 0;
  *(int *)(*(int *)(param_1 + 0xc) + 0x100) = 0;
  *(int *)(*(int *)(param_1 + 0xc) + 0x104) = 0;
  *(int *)(*(int *)(param_1 + 0x10) + 0xfc) = 0;
  *(int *)(*(int *)(param_1 + 0x10) + 0x100) = 0;
  *(int *)(*(int *)(param_1 + 0x10) + 0x104) = 0;
  func_802594BC();
  func_80259634(param_1,*(int *)(param_1 + 4));
  func_80259634(param_1,*(int *)(param_1 + 8));
  func_80259634(param_1,*(int *)(param_1 + 0xc));
  func_80259634(param_1,*(int *)(param_1 + 0x10));
  func_8025980C(param_1);
}
