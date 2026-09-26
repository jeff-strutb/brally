/* rbforce.c -- forces acting on the rigid-body car model
 */
#include "tgr/common.h"

/* -- declarations -- */
void func_802589F4(int param_1,int param_2);
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
