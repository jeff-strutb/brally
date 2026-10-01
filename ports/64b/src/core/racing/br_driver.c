/* br_driver.c -- racing.
 *
 * Per-driver bookkeeping filed out of the address batches: the copy of a
 * car's position record, and the release of a driver's loaded assets.
 * Every function carries its original address.
 */
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include <stdlib.h>

/* -- Ghidra-matched functions --------------------------- */
/* FUN_1006f840: prototype in br_funcs.h */

/* WHAT IT DOES: copy a car position record and its trailing 3-vector. */
/* @implements 0x10062610 glide BrRacePosCopy */

int BrRacePosCopy(float *param_1,float *param_2)

{
  FUN_1006f840(param_2,param_1);
  *(int *)(((float *)((char *)(param_1) + (0x10)))) = *(int *)(((float *)((char *)(param_2) + (0x30))));
  *(int *)(((float *)((char *)(param_1) + (0x14)))) = *(int *)(((float *)((char *)(param_2) + (0x34))));
  *(int *)(((float *)((char *)(param_1) + (0x18)))) = *(int *)(((float *)((char *)(param_2) + (0x38))));
  return;
}

/* WHAT IT DOES: free every entry of the driver's pointer array at +0x78 (count +0x7C),
 * then the array itself, and zero both fields. thiscall via BR_THISCALL1 (__fastcall). */
/* @implements 0x1005F530 glide BrDriverAssetsFree */

void __fastcall BrDriverAssetsFree(BrDriver * param_1)

{
  int iVar1;
  
  iVar1 = 0;
  if (0 < *(int *)(param_1 + 0x7c)) {
    do {
      free(*(void **)(*(int *)(param_1 + 0x78) + iVar1 * 4));
      *(int *)(*(int *)(param_1 + 0x78) + iVar1 * 4) = 0;
      iVar1 = iVar1 + 1;
    } while (iVar1 < *(int *)(param_1 + 0x7c));
  }
  free(*(void **)(param_1 + 0x78));
  *(int *)(param_1 + 0x78) = 0;
  *(int *)(param_1 + 0x7c) = 0;
  return;
}

