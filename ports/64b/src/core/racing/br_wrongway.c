/* br_wrongway.c -- racing.
 *
 * The wrong-way check: a car that has come to rest facing back down the
 * track gets the warning put on screen. Filed out of slice6_76.c's
 * Ghidra-matched section.
 */
#include "br_race.h"   /* br_globals: its objects */
#include <stddef.h>
#include "slice3_41.h"
#include <stdint.h>

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* BrStrGet: prototype in br_funcs.h */
/* 64-bit core: declared once, by its definition's header */

/* WHAT IT DOES: check whether a car has come to rest facing the wrong way
 * and, if so, put the 'wrong way' warning on screen and count it. Only
 * applies while the car is below a speed threshold and not already flagged. */
/* @implements 0x1006EB00 glide FUN_1006eb00 */
/* auto-filed from ghidra --refine; transforms: as-is */

void __fastcall FUN_1006eb00(BrDriverCar *param_1)

{
  char *iVar1;
  int iVar2;
  
  if (g_pBrRaceLapRec != 0) {
    if ((param_1->lap < g_CBE8) && (param_1->fF7C == 0) &&
        ((double)BrVec3Dot((const BrVec3 *)&param_1->f0F94, param_1) < _DAT_10077c40)) {
      iVar1 = BrStrGet(0xf3);
      iVar2 = param_1->f29B8 + 1;
      param_1->f29B8 = iVar2;
      if ((iVar2 > 0x1f) && ((iVar2 & 0x10) == 0x10)) {
        if (param_1->pszBanner != 0) {
          return;
        }
        *(int *)&param_1->pszBanner /* BR_LP64_INT_STORED_IN_PTR */ = iVar1;
        param_1->psz1004 = 0;
        *(int *)&param_1->f1000 = 0x3e800000;
        return;
      }
      if (BR_LP64_PTR_AS_INT(param_1->pszBanner) != iVar1) {
        return;
      }
      param_1->psz1004 = 0;
      param_1->pszBanner = 0;
      return;
    }
    param_1->f29B8 = 0;
  }
  return;
}

