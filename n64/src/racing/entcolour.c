/* entcolour.c -- car artwork records and their paint colours
 */
#include "tgr/common.h"

/* -- declarations -- */
void func_8021D140(int param_1,unsigned int param_2,unsigned int param_3,int param_4);
void func_80222D54(int param_1);
void func_8021D5E4(int param_1,int param_2,int param_3);
void func_802203F0(int param_1,int param_2);
void func_8021D070(unsigned int *param_1,unsigned int param_2,unsigned int param_3,int param_4);
void func_8021D098(unsigned int *param_1,int param_2,int param_3,int param_4);
void func_80220398();
unsigned int func_8021CD30();
void func_8021D32C(int param_1);
extern int D_8028AE20;
/* -- end declarations -- */

/* WHAT IT DOES: Repaint a car's artwork record in the car's own colour (its
 * red, green and blue reduced to the 5-bit texture format), then refresh
 * what depends on it. */
/* @implements 0x80220398 tgr BrEntRefreshColour */
void BrEntRefreshColour(int param_1)
{
  func_8021D140(*(int *)(param_1 + 0x2078),(int)(unsigned int)*(unsigned char *)(param_1 + 0x2060) >> 3,
               (int)(unsigned int)*(unsigned char *)(param_1 + 0x2061) >> 3,
               (int)(unsigned int)*(unsigned char *)(param_1 + 0x2062) >> 3);
  func_80222D54(param_1);
}

/* WHAT IT DOES: Load a car into its slot and attach that car's artwork
 * record to the entity, repainting it. */
/* @implements 0x80220438 tgr BrEntLoadRecord */
void BrEntLoadRecord(int param_1,int param_2,int param_3)
{
  func_8021D5E4(param_2,param_3,0);
  func_802203F0(param_1,param_2);
}

/* WHAT IT DOES: Tell whether an entity slot is unused (it has no owner
 * yet). */
/* @implements 0x80220534 tgr BrEntIsFree */
int BrEntIsFree(int param_1)
{
  return *(int *)(param_1 + 0x18) == 0;
}

/* WHAT IT DOES: Paint a car's colourable texture in the given colour: every
 * texel of the paint layer gets the 5-bit red, green and blue, keeping its
 * own alpha bit. */
/* @implements 0x8021D140 tgr BrEntPaintTexture */
void BrEntPaintTexture(int param_1,unsigned int param_2,unsigned int param_3,int param_4)
{
  int iVar1;
  int iVar2;
  int iVar3;
  short *puVar4;
  int iVar5;
  unsigned int uVar6;
  
  iVar1 = 0;
  iVar5 = param_1;
  do {
    iVar2 = *(int *)(param_1 + 0x14);
    iVar3 = (unsigned int)*(unsigned char *)(iVar5 + 0x110) * 0x24 + iVar2;
    puVar4 = *(short **)(iVar3 + 4);
    if (puVar4 == (short *)0x0) {
      uVar6 = (unsigned int)*(unsigned char *)(iVar5 + 0x111);
    }
    else if ((*(unsigned char *)(iVar3 + 0x20) & 0xf) == 1) {
      *puVar4 = puVar4[iVar1] & 1 | (short)(param_2 << 0xb) | (short)(param_3 << 6) |
                (short)(param_4 << 1);
      puVar4[1] = puVar4[iVar1] & 1 | (short)((param_2 & 0x1e) << 10) |
                  (short)((param_3 & 0x1e) << 5) | (short)param_4 & 0x1e;
      iVar2 = *(int *)(param_1 + 0x14);
      uVar6 = (unsigned int)*(unsigned char *)(iVar5 + 0x111);
    }
    else {
      uVar6 = (unsigned int)*(unsigned char *)(iVar5 + 0x111);
    }
    iVar2 = uVar6 * 0x24 + iVar2;
    puVar4 = *(short **)(iVar2 + 4);
    if ((puVar4 != (short *)0x0) && ((*(unsigned char *)(iVar2 + 0x20) & 0xf) == 1)) {
      *puVar4 = puVar4[iVar1 + 1] & 1 | (short)(param_2 << 0xb) | (short)(param_3 << 6) |
                (short)(param_4 << 1);
      puVar4[1] = puVar4[iVar1 + 1] & 1 | (short)((param_2 & 0x1e) << 10) |
                  (short)((param_3 & 0x1e) << 5) | (short)param_4 & 0x1e;
    }
    iVar1 = iVar1 + 2;
    iVar5 = iVar5 + 2;
  } while (iVar1 != 0xc);
}

/* WHAT IT DOES: Fix up a car model just loaded into its slot: every part's
 * address and display list is moved from the loading area to the slot's own
 * copy. */
/* @implements 0x8021D32C tgr BrEntRebaseModel */
void BrEntRebaseModel(int param_1)
{
  int iVar1;
  int iVar2;
  int iVar3;
  int iVar4;
  int iVar5;
  int local_1c;
  
  iVar4 = 0;
  iVar5 = param_1;
  local_1c = param_1;
  do {
    iVar2 = 0;
    iVar3 = iVar5 + 0x18;
    iVar1 = iVar5;
    do {
      func_8021D070(iVar3,0x803c8000,0x803d5f88,param_1);
      func_8021D098(*(int *)(iVar1 + 0x18),0x803c8000,0x803d5f88,param_1);
      iVar2 = iVar2 + 4;
      iVar1 = iVar1 + 4;
      iVar3 = iVar3 + 4;
    } while (iVar2 < 0x28);
    iVar2 = 0;
    iVar1 = local_1c;
    do {
      if (*(int *)(iVar1 + 0xbc) != 0) {
        func_8021D070(iVar1 + 0xbc,0x803c8000,0x803d5f88,param_1);
        func_8021D098(*(int *)(iVar1 + 0xbc),0x803c8000,0x803d5f88,param_1);
      }
      iVar2 = iVar2 + 4;
      iVar1 = iVar1 + 4;
    } while (iVar2 != 0xc);
    iVar4 = iVar4 + 1;
    local_1c = local_1c + 0xc;
    iVar5 = iVar5 + 0x28;
  } while (iVar4 < 3);
  func_8021D070(param_1 + 0x14,0x803c8000,0x803d5f88,param_1);
  func_8021D070(param_1 + 0x90,0x803c8000,0x803d5f88,param_1);
  func_8021D070(param_1 + 0x94,0x803c8000,0x803d5f88,param_1);
  iVar4 = 0;
  iVar5 = 0;
  if (0 < *(int *)(param_1 + 0x10)) {
    iVar1 = *(int *)(param_1 + 0x14);
    while( 1 ) {
      func_8021D070(iVar1 + iVar5,0x803c8000,0x803d5f88,param_1);
      func_8021D070(*(int *)(param_1 + 0x14) + iVar5 + 4,0x803c8000,0x803d5f88,param_1);
      iVar4 = iVar4 + 1;
      iVar5 = iVar5 + 0x24;
      if (*(int *)(param_1 + 0x10) <= iVar4) break;
      iVar1 = *(int *)(param_1 + 0x14);
    }
  }
  iVar4 = 0;
  func_8021D070(param_1 + 0x11c,0x803c8000,0x803d5f88,param_1);
  iVar5 = *(int *)(param_1 + 0x11c);
  while( 1 ) {
    func_8021D070(iVar5 + iVar4 * 4,0x803c8000,0x803d5f88,param_1);
    iVar4 = iVar4 + 1;
    if (iVar4 == 0xc) break;
    iVar5 = *(int *)(param_1 + 0x11c);
  }
}

/* WHAT IT DOES: Attach one of the car artwork records to a car and repaint
 * it in its own colour. */
/* @implements 0x802203F0 tgr BrEntSetRecord */
void BrEntSetRecord(int param_1,int param_2)
{
  *(int *)(param_1 + 0x2078) = param_2 * 0xdf88 + -0x7fc38000;
  func_80220398();
}

/* WHAT IT DOES: Load a car's model into its slot when the slot has an
 * owner, remember which model is there, and fix up the model's addresses. */
/* @implements 0x80220544 tgr BrEntLoadModel */
int BrEntLoadModel(int param_1)
{
  int iVar1;
  
  iVar1 = 0;
  if (*(int *)(param_1 + 0x18) != 0) {
    func_8021CD30(*(int *)(param_1 + 0x20) * 0xdf88 + -0x7fc38000,
                 *(int *)(&D_8028AE20 + *(int *)(param_1 + 0x24) * 0x60));
    iVar1 = *(int *)(param_1 + 0x18);
    if (iVar1 == 0) {
      *(int *)(*(int *)(param_1 + 0x20) * 4 + -0x7fce4dc8) = *(int *)(param_1 + 0x24);
      func_8021D32C(*(int *)(param_1 + 0x20) * 0xdf88 + -0x7fc38000);
      iVar1 = *(int *)(param_1 + 0x18);
    }
  }
  return iVar1;
}
