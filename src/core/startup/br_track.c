/* br_track.c -- startup.
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

extern void BrSegPtrFixup(uint32_t *p);
int BrTrackFixupSegRec();

/* WHAT IT DOES: walk the list of dword segment pointers at +0x78, byte-swap
 * and rebase each, then fix up the record it points at. */
/* @implements 0x10031910 glide BrTrackFixupSegList */

int BrTrackFixupSegList(int param_1)

{
  char uVar1;
  int *puVar3;
  int iVar4;

  puVar3 = *(int **)(param_1 + 0x78);
  iVar4 = 0;
  if (0 < *(int *)(param_1 + 0x7c)) {
    do {
      uVar1 = *(char *)((int)puVar3 + 3);
      *(char *)((int)puVar3 + 3) = *(char *)puVar3;
      *(char *)puVar3 = uVar1;
      uVar1 = *(char *)((int)puVar3 + 2);
      *(char *)((int)puVar3 + 2) = *(char *)((int)puVar3 + 1);
      *(char *)((int)puVar3 + 1) = uVar1;
      BrSegPtrFixup(puVar3);
      BrTrackFixupSegRec(*puVar3);
      puVar3 = puVar3 + 1;
      iVar4 = iVar4 + 1;
    } while (iVar4 < *(int *)(param_1 + 0x7c));
  }
  return;
}



/* WHAT IT DOES: byte-swap a 0x28-byte record: three Vec3s then one dword. */
/* @implements 0x10031A40 glide BrTrackSwapRec28 */

int BrTrackSwapRec28(int param_1)

{
  char uVar1;

  BrSwapVec3(param_1);
  BrSwapVec3(param_1 + 0xc);
  BrSwapVec3(param_1 + 0x18);
  uVar1 = *(char *)(param_1 + 0x27);
  *(char *)(param_1 + 0x27) = *(char *)(param_1 + 0x24);
  *(char *)(param_1 + 0x24) = uVar1;
  uVar1 = *(char *)(param_1 + 0x26);
  *(char *)(param_1 + 0x26) = *(char *)(param_1 + 0x25);
  *(char *)(param_1 + 0x25) = uVar1;
  return;
}


/* WHAT IT DOES: walk an array of Vec3s in a track struct and byte-swap each one. */
/* @implements 0x10031A80 glide BrTrackSwapAllVec3 */

int BrTrackSwapAllVec3(int param_1)

{
  int iVar1;
  int iVar2;
  
  iVar1 = *(int *)(param_1 + 0x84);
  iVar2 = 0;
  if (0 < *(int *)(param_1 + 0x88)) {
    do {
      BrSwapVec3(iVar1);
      iVar1 = iVar1 + 0xc;
      iVar2 = iVar2 + 1;
    } while (iVar2 < *(int *)(param_1 + 0x88));
  }
  return;
}

#endif /* BR_MATCHING_BUILD */
