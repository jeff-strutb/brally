/* music.c -- the module music player
 */
#include "tgr/common.h"

/* -- declarations -- */
void func_80256720(int param_1,char *param_2);
void func_802571AC(void);
void osSyncPrintf();
extern int D_802A4798;
extern int D_802A49C0;
extern int D_80378F98;
extern int D_802A4920;
/* -- end declarations -- */

/* WHAT IT DOES: Start a piece of music: builds its instrument entries,
 * starts the module player and sets every channel to its starting volume. */
/* @implements 0x80257964 tgr BrMusicStart */
void BrMusicStart(int param_1,int param_2)
{
  int iVar1;
  int *puVar2;
  int *puVar3;
  
  osSyncPrintf("Creating I entries\n");
  func_80256720(param_1,param_2);
  osSyncPrintf("Starting Mod\n");
  func_802571AC();
  D_80378F98 = 1;
  if (0 < D_802A49C0) {
    iVar1 = D_802A49C0 * 6;
    puVar2 = &D_802A4798;
    do {
      puVar3 = puVar2 + 6;
      puVar2[5] = 0x20;
      puVar2 = puVar3;
    } while (puVar3 < &D_802A4798 + iVar1);
  }
}

/* WHAT IT DOES: Keep the music channels' samples looping: when a channel
 * runs past its sample's end it jumps back by the loop length, or stops if
 * the sample does not loop. */
/* @implements 0x80257C44 tgr BrMusicLoopSamples */
void BrMusicLoopSamples(void)
{
  int iVar1;
  int iVar2;
  unsigned int *puVar3;
  unsigned int uVar4;
  
  puVar3 = &D_802A4920;
  iVar2 = 0;
  do {
    if ((puVar3[2] == 0) && (puVar3[3] == 0)) {
      uVar4 = puVar3[8];
    }
    else {
      iVar1 = iVar2 * 0xc;
      if (*puVar3 < (unsigned int)(*(int *)(iVar1 + -0x7fc870ac) + *(int *)(iVar1 + -0x7fc870b0))) {
        uVar4 = puVar3[8];
      }
      else {
        if (*(int *)(iVar1 + -0x7fc870a8) == 0) {
          puVar3[2] = 0;
          puVar3[3] = 0;
        }
        else {
          *puVar3 = *puVar3 - *(int *)(iVar1 + -0x7fc870a8);
        }
        uVar4 = puVar3[8];
      }
    }
    if ((uVar4 != 0) || (puVar3[9] != 0)) {
      iVar1 = iVar2 * 0xc;
      if ((unsigned int)(*(int *)(iVar1 + -0x7fc870a0) + *(int *)(iVar1 + -0x7fc870a4)) <= puVar3[6]) {
        if (*(int *)(iVar1 + -0x7fc8709c) == 0) {
          puVar3[8] = 0;
          puVar3[9] = 0;
        }
        else {
          puVar3[6] = puVar3[6] - *(int *)(iVar1 + -0x7fc8709c);
        }
      }
    }
    iVar2 = iVar2 + 2;
    puVar3 = puVar3 + 0xc;
  } while (iVar2 != 6);
}
