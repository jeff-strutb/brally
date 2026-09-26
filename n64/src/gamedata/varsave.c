/* varsave.c -- saving and restoring the block of game variables kept with a save
 */
#include "tgr/common.h"

/* -- declarations -- */
void func_8022ADCC(int *param_1,int param_2,int param_3);
extern int D_8028BB0C;
void func_8022AE70(int *param_1,int param_2);
/* -- end declarations -- */

/* WHAT IT DOES: Copy every registered game variable into the save buffer,
 * checking it fits the space reserved for it. */
/* @implements 0x8022AED8 tgr BrVarSaveAll */
void BrVarSaveAll(int param_1)
{
  func_8022ADCC(&D_8028BB0C,param_1 + 0x7080,0xdf88);
}

/* WHAT IT DOES: Copy every registered game variable back out of the save
 * buffer. */
/* @implements 0x8022AF08 tgr BrVarLoadAll */
void BrVarLoadAll(int param_1)
{
  func_8022AE70(&D_8028BB0C,param_1 + 0x7080);
}
