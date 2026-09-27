/* options.c -- the options screen
 */
#include "tgr/common.h"

/* -- declarations -- */
int BrCpakCheck(int param_1,char param_2);
void BrTextHighlightOff(void);
void BrTextAlignCentre(void);
void BrTextSetFont(int param_1);
void BrTextPrint(int param_1,int param_2,int param_3);
extern int D_803163A0;
extern int D_803163A4;
extern int D_8036A8E0;
extern int D_8036A8F8;
extern int D_802723D0[3];
extern int D_80272488[11];
extern int D_802724B4[11];
extern unsigned char D_802A49C4;
extern unsigned char D_802A49CC;
/* -- end declarations -- */

/* WHAT IT DOES: Turn the music up one notch on the options screen (to at
 * most the eleventh step) and set the mixer's music volume from the step
 * table (0..48). */
/* @implements 0x80211840 tgr BrMusicVolumeUp */
void BrMusicVolumeUp(void)
{
  if (D_802723D0[0] < 10) {
    D_802723D0[0]++;
  }
  D_802A49C4 = D_80272488[D_802723D0[0]];
}

/* WHAT IT DOES: Turn the music down one notch (to at least step zero, which
 * is silent) and set the mixer's music volume from the step table. */
/* @implements 0x8021187C tgr BrMusicVolumeDown */
void BrMusicVolumeDown(void)
{
  if (D_802723D0[0] > 0) {
    D_802723D0[0]--;
  }
  D_802A49C4 = D_80272488[D_802723D0[0]];
}

/* WHAT IT DOES: Set the mixer's music and sound-effect volumes from the saved
 * volume steps (used after the settings are loaded). */
/* @implements 0x802118B4 tgr BrVolumesApply */
void BrVolumesApply(void)
{
  D_802A49C4 = D_80272488[D_802723D0[0]];
  D_802A49CC = D_802724B4[D_802723D0[1]];
}

/* WHAT IT DOES: Turn the sound effects up one notch (to at most the eleventh
 * step) and set the mixer's effects volume from the step table (0..255). */
/* @implements 0x802118F8 tgr BrSfxVolumeUp */
void BrSfxVolumeUp(void)
{
  if (D_802723D0[1] < 10) {
    D_802723D0[1]++;
  }
  D_802A49CC = D_802724B4[D_802723D0[1]];
}

/* WHAT IT DOES: Turn the sound effects down one notch (to at least step
 * zero, which is silent) and set the mixer's effects volume from the step
 * table. */
/* @implements 0x80211934 tgr BrSfxVolumeDown */
void BrSfxVolumeDown(void)
{
  if (D_802723D0[1] > 0) {
    D_802723D0[1]--;
  }
  D_802A49CC = D_802724B4[D_802723D0[1]];
}

/* WHAT IT DOES: Draw the options screen's help line and keep the Controller
 * Pak state current: when the pak state has to be re-read it re-initialises
 * the pak and clears the save flags if none is found. */
/* @t4-pass 0x8021196C 1 2026-09-26 compiles 17 best 33 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8021196C 2 2026-09-26 compiles 16 best 33 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8021196C 3 2026-09-26 compiles 15 best 33 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x8021196C tgr BrOptionsDrawHelp */
void BrOptionsDrawHelp(int param_1)
{
  int iVar1;
  
  if (param_1 < 2) {
    BrTextSetFont(10);
    BrTextHighlightOff();
    BrTextAlignCentre();
    BrTextPrint("%wwPush up or down to adjust",0xa0,0x5a);
  }
  if (D_803163A0 == 0) {
    if (D_803163A4 != 0) {
      iVar1 = BrCpakCheck(3,0);
      if (iVar1 == 0) {
        D_8036A8E0 = 0;
        D_8036A8F8 = 0;
      }
      else {
        D_803163A4 = 0;
      }
    }
  }
  else {
    iVar1 = BrCpakCheck(2,0);
    if (iVar1 == 0) {
      D_8036A8E0 = 0;
      D_8036A8F8 = 0;
    }
    else {
      D_803163A0 = 0;
    }
  }
}
