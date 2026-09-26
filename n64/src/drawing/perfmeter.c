/* perfmeter.c -- the performance meter
 */
#include "tgr/common.h"

/* -- declarations -- */
int func_802649F0(void);
extern int D_8028BDA0;
extern int D_8028BDAC;
/* -- end declarations -- */

/* WHAT IT DOES: Record a timing mark for the performance meter: the colour
 * for this bar segment and the CPU count since the frame began. */
/* @t4-pass 0x8022D7E0 1 2026-09-26 compiles 16 best 45 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8022D7E0 2 2026-09-26 compiles 16 best 45 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8022D7E0 3 2026-09-26 compiles 17 best 45 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x8022D7E0 tgr BrPerfMark */
void BrPerfMark(int param_1,unsigned int param_2,unsigned int param_3,int param_4,int param_5)
{
  int *piVar1;
  int iVar2;
  unsigned int uVar3;
  int iVar4;
  
  piVar1 = (int *)(D_8028BDA0 * 0xc + param_1 * 4 + -0x7fcae650);
  iVar2 = *piVar1;
  iVar4 = D_8028BDA0 * 0x1800 + param_1 * 0x800 + iVar2 * 8;
  *piVar1 = iVar2 + 1;
  uVar3 = (param_2 & 0xf8) << 8 | (param_3 & 0xf8) << 3 | param_4 >> 2 & 0x3eU | param_5 >> 7 & 1U;
  *(unsigned int *)(iVar4 + -0x7fcb1648) = uVar3 | uVar3 << 0x10;
  iVar2 = func_802649F0();
  *(int *)(iVar4 + -0x7fcb164c) = iVar2 - D_8028BDAC;
}
