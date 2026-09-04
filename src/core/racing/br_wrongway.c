/* br_wrongway.c -- racing.
 *
 * The wrong-way check: a car that has come to rest facing back down the
 * track gets the warning put on screen. Filed out of slice6_76.c's
 * Ghidra-matched section.
 */
#include <stddef.h>
#include <stdint.h>

#ifdef BR_MATCHING_BUILD
extern int DAT_100bcbe8;
extern double _DAT_10077c40;
extern int BrG_6C7CB8;
int BrStrGet(int);
float BrVec3Dot(int, int);

/* WHAT IT DOES: check whether a car has come to rest facing the wrong way
 * and, if so, put the 'wrong way' warning on screen and count it. Only
 * applies while the car is below a speed threshold and not already flagged. */
/* @implements 0x1006EB00 glide FUN_1006eb00 */
/* auto-filed from ghidra --refine; transforms: as-is */

void __fastcall FUN_1006eb00(int param_1)

{
  int iVar1;
  int iVar2;
  
  if (BrG_6C7CB8 != 0) {
    if ((*(int *)(param_1 + 0xfa8) < DAT_100bcbe8) && (*(int *)(param_1 + 0xf7c) == 0) &&
        ((double)BrVec3Dot(param_1 + 0xf94, param_1) < _DAT_10077c40)) {
      iVar1 = BrStrGet(0xf3);
      iVar2 = *(int *)(param_1 + 0x29b8) + 1;
      *(int *)(param_1 + 0x29b8) = iVar2;
      if ((iVar2 > 0x1f) && ((iVar2 & 0x10) == 0x10)) {
        if (*(int *)(param_1 + 0xffc) != 0) {
          return;
        }
        *(int *)(param_1 + 0xffc) = iVar1;
        *(int *)(param_1 + 0x1004) = 0;
        *(int *)(param_1 + 0x1000) = 0x3e800000;
        return;
      }
      if (*(int *)(param_1 + 0xffc) != iVar1) {
        return;
      }
      *(int *)(param_1 + 0x1004) = 0;
      *(int *)(param_1 + 0xffc) = 0;
      return;
    }
    *(int *)(param_1 + 0x29b8) = 0;
  }
  return;
}

#endif /* BR_MATCHING_BUILD */
