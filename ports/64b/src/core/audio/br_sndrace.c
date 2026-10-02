#include "slice3_41.h"   /* br_globals: its objects */
#include "slice3_42.h"   /* br_globals: its objects */
/* br_sndrace.c -- audio.
 *
 * The per-race audio bring-up: the one call that puts the sound bank into the
 * state a race needs, and that also brings force feedback up on the way past.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */
/* The original is /MD: CRT calls go through the import
 * table (FF 15). */
#define _CRTIMP __declspec(dllimport)


/* FUN_1006c290: prototype in br_funcs.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* BrFfbInit: prototype in br_funcs.h */
/* BrSfxSrcPlaySilent: prototype in br_funcs.h */
/* BrSndBankClear: prototype in br_funcs.h */
/* BrSndBankSetCar: prototype in br_funcs.h */

/* WHAT IT DOES: gets sound and force feedback ready for a race.
 *
 * Force feedback first, and only when the setting asks for it: the device is
 * brought up and WHAT CAME BACK IS WRITTEN STRAIGHT BACK INTO THE SETTING, so
 * a device that failed or came up as something lesser is recorded as such and
 * the game stops asking for it on every later race rather than retrying. The
 * recorded answer also selects which of three control-configuration blocks is
 * in force, with a fallback block for "no force feedback at all".
 *
 * Then the sound bank is rebuilt from scratch for this race: every per-car
 * sound-source row is renumbered in order, the bank is emptied, and each car
 * in the race registers its own sound set -- walked out of the car table a
 * long stride apart. The bank is then loaded and a silent source started for
 * each of the first three cars, so their engines are already running at zero
 * before the race is heard. Cars beyond the third get no silent start.
 *
 * The force-feedback half runs whether or not there is any sound to set up;
 * the two are only together because one call does both. */
/* @implements 0x10061310 glide FUN_10061310 */
/* auto-filed from ghidra --refine; transforms: as-is */

void FUN_10061310(void)

{
  int *piVar1;
  int iVar2;
  int *puVar3;
  int v;
  
  if (((*(int *)((char *)&(*(int *)&g_BrCtrlCfg) + 0x2A0)) /* BR_LP64_BYTE_VIEW */ == 1) || ((*(int *)((char *)&(*(int *)&g_BrCtrlCfg) + 0x2A0)) /* BR_LP64_BYTE_VIEW */ == 2)) {
    v = BrFfbInit();
    (*(int *)((char *)&(*(int *)&g_BrCtrlCfg) + 0x2A0)) /* BR_LP64_BYTE_VIEW */ = v;
    switch (v) {
    case 1:
      g_BrPadModeBytes = (int)&(*(int *)((char *)&(*(int *)&g_BrCtrlCfg) + 0xA8)) /* BR_LP64_BYTE_VIEW */;
      break;
    case 2:
      g_BrPadModeBytes = (int)&(*(int *)((char *)&(*(int *)&g_BrCtrlCfg) + 0x150)) /* BR_LP64_BYTE_VIEW */;
      break;
    case 3:
      g_BrPadModeBytes = (int)&(*(int *)((char *)&(*(int *)&g_BrCtrlCfg) + 0x1F8)) /* BR_LP64_BYTE_VIEW */;
      break;
    default:
      g_BrPadModeBytes = (int)&(*(int *)&g_BrCtrlCfg);
      break;
    }
  }
  iVar2 = 0;
  piVar1 = &(DAT_100b32b0[0]);
  do {
    *piVar1 = iVar2;
    piVar1 = piVar1 + 6;
    iVar2 = iVar2 + 1;
  } while (piVar1 < &DAT_100b32b0[0] + (0x100b3508 - 0x100b32b0) / 4);
  BrSndBankClear();
  iVar2 = 0;
  if ((*(int *)&g_BrCarCount) > 0) {
    puVar3 = &(*(int *)&g_aBrRaceCar[0].f29A8);
    do {
      BrSndBankSetCar(iVar2,*puVar3);
      iVar2 = iVar2 + 1;
      puVar3 = puVar3 + 0xada;
    } while (iVar2 < (*(int *)&g_BrCarCount));
  }
  BrSfxBankLoad(1);
  BrSfxSrcPlaySilent(0,(DAT_100b32b0[0]),(DAT_100b32bc[0]),(DAT_100b32c0[0]));
  if ((*(int *)&g_BrCarCount) > 1) {
    BrSfxSrcPlaySilent(2,(DAT_100b32b0[0]),(DAT_100b32bc[0]),(DAT_100b32c0[0]));
  }
  if ((*(int *)&g_BrCarCount) > 2) {
    BrSfxSrcPlaySilent(4,(DAT_100b32b0[0]),(DAT_100b32bc[0]),(DAT_100b32c0[0]));
  }
  return;
}

