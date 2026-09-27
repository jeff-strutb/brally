/* options.c -- the options screen
 */
#include "tgr/common.h"
#include "tgr/pad.h"

/* -- declarations -- */
int BrCpakCheck(int param_1,char param_2);
void BrTextHighlightOff(void);
void BrTextAlignCentre(void);
void BrTextSetFont(int param_1);
void BrTextPrint(int param_1,int param_2,int param_3);
extern int D_803163A0;
extern int D_803163A4;
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

/* WHAT IT DOES: Draw the options screen's help line (for the two volume
 * rows) and run a pending Controller Pak check (0x803163A0, else
 * 0x803163A4): once the check succeeds its flag is cleared; until then
 * player 1's fresh presses and first stick axis are swallowed each frame. */
/* @implements 0x8021196C tgr BrOptionsDrawHelp */
void BrOptionsDrawHelp(int row)
{
  if (row < 2) {
    BrTextSetFont(10);
    BrTextHighlightOff();
    BrTextAlignCentre();
    BrTextPrint("%wwPush up or down to adjust", 160, 90);
  }
  if (D_803163A0 != 0) {
    if (BrCpakCheck(2, 0) != 0) {
      D_803163A0 = 0;
    } else {
      D_8036A8E0[0].pressed = 0;
      D_8036A8E0[0].axis[0] = 0.0f;
    }
  } else if (D_803163A4 != 0) {
    if (BrCpakCheck(3, 0) != 0) {
      D_803163A4 = 0;
    } else {
      D_8036A8E0[0].pressed = 0;
      D_8036A8E0[0].axis[0] = 0.0f;
    }
  }
}

