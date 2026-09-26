/* texscroll.c -- scrolling textures
 */
#include "tgr/common.h"

/* -- declarations -- */
int func_8022D3D4(int param_1,int param_2);
int BrFloatToInt(float param_1);
/* -- end declarations -- */

/* WHAT IT DOES: Set the texture scroll offsets from two float positions (in
 * texels, 1/32 steps). */
/* @implements 0x8022D43C tgr BrTexScrollSet */
void BrTexScrollSet(float param_1,float param_2)
{
  int uVar1;
  int uVar2;
  
  uVar1 = BrFloatToInt(param_1 * 0.03125);
  uVar2 = BrFloatToInt(param_2 * 0.03125);
  func_8022D3D4(uVar1,uVar2);
}
