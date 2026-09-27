/* texscroll.c -- scrolling textures
 */
#include "tgr/common.h"

/* -- declarations -- */
int func_8022D3D4(int param_1,int param_2);
int BrFloatToInt(float param_1);
/* -- end declarations -- */

/* WHAT IT DOES: Set the texture scroll offsets from two float positions (in
 * texels, 1/32 steps). */
/* @t4-pass 0x8022D43C 1 2026-09-26 compiles 20 best 24 moved 1  (n64/tools/n64permute.py) */
/* @t4-pass 0x8022D43C 2 2026-09-26 compiles 21 best 24 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x8022D43C tgr BrTexScrollSet */
void BrTexScrollSet(float param_1,float param_2)
{
  int uVar1;
  int uVar2;
  
  uVar2 = BrFloatToInt(param_2 * 0.03125f);
  uVar1 = BrFloatToInt(0.03125f * param_1);
  func_8022D3D4(uVar1,uVar2);
}
