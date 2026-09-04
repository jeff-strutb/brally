/* br_model.c -- drawing.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import
 * table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdint.h>

#ifdef BR_MATCHING_BUILD

int FUN_1006e1d0();
extern int g_AC300;

/* WHAT IT DOES: apply texture slots from one model record to another, including optional extra slots when enabled. */
/* @implements 0x1005F220 glide BrModelSlotApply */

int BrModelSlotApply(int param_1,int param_2)

{
  int iVar1;
  int iVar2;
  
  iVar2 = 0;
  iVar1 = *(int *)(*(int *)(param_1 + 0x29c4) + 0x7c);
  if (0 < iVar1) {
    do {
      FUN_1006e1d0(*(int *)(*(int *)(param_1 + 0x29c4) + 4 + iVar2 * 4),
                   *(int *)(*(int *)(param_2 + 0x78) + iVar2 * 4));
      iVar2 = iVar2 + 1;
    } while (iVar2 < iVar1);
  }
  iVar2 = *(int *)(param_1 + 0x29c4);
  if ((*(int *)(*(int *)(iVar2 + 0x8014) + 4 + (unsigned int)*(unsigned char *)(iVar2 + 0x811b) * 0x24) != 0) &&
     (g_AC300 == 0)) {
    if (*(int *)(iVar2 + 0x84) != 0) {
      FUN_1006e1d0(*(int *)(iVar2 + 0x84),*(int *)(*(int *)(param_2 + 0x78) + iVar1 * 4));
    }
    iVar2 = *(int *)(*(int *)(param_1 + 0x29c4) + 0x88);
    if (iVar2 != 0) {
      FUN_1006e1d0(iVar2,*(int *)(*(int *)(param_2 + 0x78) + 4 + iVar1 * 4));
    }
    iVar2 = *(int *)(*(int *)(param_1 + 0x29c4) + 0x8c);
    if (iVar2 != 0) {
      FUN_1006e1d0(iVar2,*(int *)(*(int *)(param_2 + 0x78) + 8 + iVar1 * 4));
    }
    iVar2 = *(int *)(*(int *)(param_1 + 0x29c4) + 0x90);
    if (iVar2 != 0) {
      FUN_1006e1d0(iVar2,*(int *)(*(int *)(param_2 + 0x78) + 0xc + iVar1 * 4));
    }
  }
  return;
}

#endif /* BR_MATCHING_BUILD */
