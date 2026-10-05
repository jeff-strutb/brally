/* options.c -- the options screen
 */
#include "tgr/common.h"
#include "tgr/romimage.h"
#include "tgr/pad.h"
#include "tgr/menu.h"

/* -- declarations -- */
int BrCpakCheck(int param_1,char param_2);
void BrTextHighlightOff(void);
void BrTextAlignCentre(void);
void BrTextSetFont(int param_1);
void BrTextPrint(char *s, int x, int y);
extern int D_803163A0;
extern int D_803163A4;
extern int D_802723D0[3];
extern int D_80272488[11];
extern int D_802724B4[11];
extern unsigned char D_802A49C4;
extern unsigned char D_802A49CC;
void BrFrontMenuEnter(MenuItem **items, int *count);
void BrPaintShopMemInit(void);
int BrMenu(void *, int, void *, void *, void *, int, int, int, int, int, int);
void BrModeSet(void (*fn)(void));
void BrCtrlConfigScreen(void);
void BrMainMenu(void);
void BrDemoRaceStartB(void);
extern int D_80271FA0;
extern int D_80271FA4;
extern int D_80271FA8;
extern void (*D_80271FC8)(int);
extern void *D_80271FB0;
extern int D_80271FB4;
extern int D_80271FB8;
extern int D_80271FBC;
extern int D_80271FC0;
extern BrRomImage D_8027205C;
extern MenuItem *D_80272468[];
extern int D_803163A8;
extern char D_803163B0[24];
extern char D_803163C8[24];
extern MenuItem D_802723DC[];
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


/* WHAT IT DOES: The options screen, run once per frame: on entry set up
 * the menu (items, layout, help line; keep the selection when returning
 * from a sub-screen), refresh the volume and units labels, run the menu and
 * act on it -- left/right adjust the volumes or flip the units, A opens the
 * controller setup, the pak save/load checks or the credits, B returns to
 * the main menu. */
/* @implements 0x80211A3C tgr BrOptionsScreen */
void BrOptionsScreen(void)
{
  extern int D_802724E0;         /* 0x802724E0: 1 once set up, -1 = returning from a sub-screen */
  extern int D_803163AC;                 /* 0x803163AC: the highlighted item */

  if (D_802724E0 <= 0) {
    D_80271FA4 = -1;
    D_80271FA0 = -1;
    BrFrontMenuEnter(D_80272468, &D_803163A8);
    BrPaintShopMemInit();
    D_80271FC8 = BrOptionsDrawHelp;
    D_80271FB0 = &D_8027205C;
    D_80271FB4 = 22;
    D_80271FB8 = 9;
    D_80271FBC = 280;
    D_80271FC0 = 42;
    if (D_802724E0 == 0) {
      D_803163AC = 0;
    }
    D_802724E0 = 1;
  }
  sprintf(D_803163B0, "BGM Volume: %d", D_802723D0[0]);
  sprintf(D_803163C8, "SFX Volume: %d", D_802723D0[1]);
  D_802723DC[0].label = tgr_addr32(D_803163B0);
  D_802723DC[1].label = tgr_addr32(D_803163C8);
  if (D_802723D0[2] != 0) {
    D_802723DC[2].label = tgr_addr32("Units: mph");
  } else {
    D_802723DC[2].label = tgr_addr32("Units: kph");
  }
  switch (BrMenu("OPTIONS", D_803163A8, D_80272468, &D_803163AC, 0, 0, 0, 0, 0x80, 0x40, 0x20)) {
  case 4:
    switch (D_803163AC) {
    case 0:
      BrMusicVolumeDown();
      break;
    case 1:
      BrSfxVolumeDown();
      break;
    case 2:
      D_802723D0[2] = !D_802723D0[2];
      break;
    }
    break;
  case 3:
    switch (D_803163AC) {
    case 0:
      BrMusicVolumeUp();
      break;
    case 1:
      BrSfxVolumeUp();
      break;
    case 2:
      D_802723D0[2] = !D_802723D0[2];
      break;
    }
    break;
  case 1:
    switch (D_803163AC) {
    case 2:
      D_802723D0[2] = !D_802723D0[2];
      break;
    case 3:
      D_802724E0 = -1;
      D_80271FA0 = D_80271FA8;
      D_80271FA4 = 1 << D_80271FA0;
      BrModeSet(BrCtrlConfigScreen);
      break;
    case 4:
      D_803163A4 = 1;
      break;
    case 5:
      D_803163A0 = 1;
      break;
    case 6:
      D_802724E0 = -1;
      BrDemoRaceStartB();
      break;
    }
    break;
  case 2:
    BrModeSet(BrMainMenu);
    D_802724E0 = 0;
    break;
  }
}

