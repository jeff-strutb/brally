/* br_sndrace.c -- audio.
 *
 * The per-race audio bring-up: the one call that puts the sound bank into the
 * state a race needs, and that also brings force feedback up on the way past.
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

#ifdef BR_MATCHING_BUILD

int FUN_1006c290();
extern int DAT_100b2f04;
extern int DAT_100b32b0;
extern int DAT_100b32bc;
extern int DAT_100b32c0;
extern int DAT_10af3bb0;
extern int DAT_10b71338;
extern int DAT_10b713e0;
extern int DAT_10b71488;
extern int DAT_10b71530;
extern int DAT_10b71534;
extern int g_BrCtrlCfg;
int BrFfbInit();
int BrSfxSrcPlaySilent();
int BrSndBankClear();
int BrSndBankSetCar();

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
  
  if ((DAT_10b71530 == 1) || (DAT_10b71530 == 2)) {
    v = BrFfbInit();
    DAT_10b71530 = v;
    switch (v) {
    case 1:
      DAT_10b71534 = (int)&DAT_10b71338;
      break;
    case 2:
      DAT_10b71534 = (int)&DAT_10b713e0;
      break;
    case 3:
      DAT_10b71534 = (int)&DAT_10b71488;
      break;
    default:
      DAT_10b71534 = (int)&g_BrCtrlCfg;
      break;
    }
  }
  iVar2 = 0;
  piVar1 = &DAT_100b32b0;
  do {
    *piVar1 = iVar2;
    piVar1 = piVar1 + 6;
    iVar2 = iVar2 + 1;
  } while ((int)piVar1 < 0x100b3508);
  BrSndBankClear();
  iVar2 = 0;
  if (DAT_100b2f04 > 0) {
    puVar3 = &DAT_10af3bb0;
    do {
      BrSndBankSetCar(iVar2,*puVar3);
      iVar2 = iVar2 + 1;
      puVar3 = puVar3 + 0xada;
    } while (iVar2 < DAT_100b2f04);
  }
  FUN_1006c290(1);
  BrSfxSrcPlaySilent(0,DAT_100b32b0,DAT_100b32bc,DAT_100b32c0);
  if (DAT_100b2f04 > 1) {
    BrSfxSrcPlaySilent(2,DAT_100b32b0,DAT_100b32bc,DAT_100b32c0);
  }
  if (DAT_100b2f04 > 2) {
    BrSfxSrcPlaySilent(4,DAT_100b32b0,DAT_100b32bc,DAT_100b32c0);
  }
  return;
}

#endif /* BR_MATCHING_BUILD */
