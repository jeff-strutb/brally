/* options.c -- the options screen
 */
#include "tgr/common.h"
#include "tgr/romimage.h"
#include "tgr/pad.h"
#include "tgr/menu.h"
#include "tgr/car.h"

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
#if TGR_REGION_EUR || TGR_REGION_JPN
void BrOptionsEepromSave(void);
extern unsigned char *D_80272500;      /* the paint shop's data buffer */
extern TgrMsgQueue D_80272D48;          /* the SI message queue */
extern short D_802A4BE8;               /* the SI is free for the pak code */
#endif
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
MenuItem **tgr_keys_items(MenuItem **game);   /* port: Keys and Pad after the controller row (keysmenu.c) */
int tgr_keys_row(int result, int row);
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
 * rows; Japan has none) and run a pending Controller Pak check (0x803163A0, else
 * 0x803163A4): once the check succeeds its flag is cleared; until then
 * player 1's fresh presses and first stick axis are swallowed each frame. */
/* @implements 0x8021196C tgr BrOptionsDrawHelp */
void BrOptionsDrawHelp(int row)
{
#if !TGR_REGION_JPN
  if (row < 2) {
    BrTextSetFont(10);
    BrTextHighlightOff();
    BrTextAlignCentre();
    BrTextPrint("%wwPush up or down to adjust", 160, 90);
  }
#endif
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


#if TGR_REGION_JPN || TGR_REGION_EUR
extern int BrOptionsChanged;            /* a setting changed: save them on the way out */
#define OPTIONS_CHANGED() (BrOptionsChanged = 1)
#else
#define OPTIONS_CHANGED() ((void)0)
#endif

/* WHAT IT DOES: The options screen, run once per frame: on entry set up
 * the menu (items, layout, help line; keep the selection when returning
 * from a sub-screen), refresh the volume and units labels, run the menu and
 * act on it -- left/right adjust the volumes or flip the units, A opens the
 * controller setup, the pak save/load checks or the credits, B returns to
 * the main menu.  Europe and Japan drop the pak rows, note any change and
 * save the settings to the EEPROM on the way out (Japan with its own
 * layout and unit names). */
/* @implements 0x80211A3C tgr BrOptionsScreen */
void BrOptionsScreen(void)
{
  extern int D_802724E0;         /* 0x802724E0: 1 once set up, -1 = returning from a sub-screen */
  extern int D_803163AC;                 /* 0x803163AC: the highlighted item */

  if (D_802724E0 <= 0) {
    D_80271FA4 = -1;
    D_80271FA0 = -1;
    BrFrontMenuEnter(tgr_keys_items(D_80272468), &D_803163A8);   /* port: with Keys and Pad */
    BrPaintShopMemInit();
    D_80271FC8 = BrOptionsDrawHelp;
    D_80271FB0 = &D_8027205C;
#if TGR_REGION_JPN
    D_80271FB4 = 25;
    D_80271FB8 = 14;
    D_80271FBC = 280;
    D_80271FC0 = 32;
#else
    D_80271FB4 = 22;
    D_80271FB8 = 9;
    D_80271FBC = 280;
    D_80271FC0 = 42;
#endif
    if (D_802724E0 == 0) {
#if TGR_REGION_JPN || TGR_REGION_EUR
      BrOptionsChanged = 0;
#endif
      D_803163AC = 0;
    }
    D_802724E0 = 1;
  }
  sprintf(D_803163B0, "BGM Volume: %d", D_802723D0[0]);
  sprintf(D_803163C8, "SFX Volume: %d", D_802723D0[1]);
  D_802723DC[0].label = tgr_addr32(D_803163B0);
  D_802723DC[1].label = tgr_addr32(D_803163C8);
  if (D_802723D0[2] != 0) {
#if TGR_REGION_JPN
    D_802723DC[2].label = tgr_addr32("Units: MILE/H");
#else
    D_802723DC[2].label = tgr_addr32("Units: mph");
#endif
  } else {
#if TGR_REGION_JPN
    D_802723DC[2].label = tgr_addr32("Units: KM/H");
#else
    D_802723DC[2].label = tgr_addr32("Units: kph");
#endif
  }
  {
  int res = BrMenu("OPTIONS", D_803163A8, tgr_keys_items(D_80272468), &D_803163AC, 0, 0, 0, 0, 0x80, 0x40, 0x20);
  int keep = D_803163AC;               /* port: the game's switch sees its own row numbers */
  D_803163AC = tgr_keys_row(res, keep);
  switch (D_803163AC < 0 ? 0 : res) {
  case 4:
    switch (D_803163AC) {
    case 0:
      BrMusicVolumeDown();
      OPTIONS_CHANGED();
      break;
    case 1:
      BrSfxVolumeDown();
      OPTIONS_CHANGED();
      break;
    case 2:
      D_802723D0[2] = !D_802723D0[2];
      OPTIONS_CHANGED();
      break;
    }
    break;
  case 3:
    switch (D_803163AC) {
    case 0:
      BrMusicVolumeUp();
      OPTIONS_CHANGED();
      break;
    case 1:
      BrSfxVolumeUp();
      OPTIONS_CHANGED();
      break;
    case 2:
      D_802723D0[2] = !D_802723D0[2];
      OPTIONS_CHANGED();
      break;
    }
    break;
  case 1:
    switch (D_803163AC) {
    case 2:
      D_802723D0[2] = !D_802723D0[2];
      OPTIONS_CHANGED();
      break;
    case 3:
      D_802724E0 = -1;
      D_80271FA0 = D_80271FA8;
      D_80271FA4 = 1 << D_80271FA0;
      BrModeSet(BrCtrlConfigScreen);
      break;
#if TGR_REGION_JPN || TGR_REGION_EUR
    case 4:
      D_802724E0 = -1;
      BrDemoRaceStartB();
      break;
#else
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
#endif
    }
    break;
  case 2:
    BrModeSet(BrMainMenu);
    D_802724E0 = 0;
#if TGR_REGION_JPN || TGR_REGION_EUR
    if (BrOptionsChanged) {
      BrOptionsEepromSave();
    }
#endif
    break;
  }
  D_803163AC = keep;
  }
}

#if TGR_REGION_EUR || TGR_REGION_JPN
/* WHAT IT DOES: Load the saved settings from the EEPROM: both players'
 * season records (player 2's five per-season words kept apart) and the
 * three option values, read as one block into the paint shop's buffer and
 * checked against its additive checksum (seeded 0xBFED812).  Answers 1 when
 * it loaded; the SI is marked busy while it works. */
/* @implements eur:0x80211CA0 tgr BrOptionsEepromLoad */
/* @implements jpn:0x80211B60 tgr BrOptionsEepromLoad */
int BrOptionsEepromLoad(void)
{
  int i;
  int sum;
  unsigned char *data;
  int *p2;
  int u0;                       /* unused: the frame has it */
  int n;
  int stored;

  data = D_80272500 + 4;
  p2 = (int *)(data + 0x128);
  n = (data - D_80272500 + 0x14f) & ~7;
  D_802A4BE8 = 0;
  if (tgr_eeprom_probe(&D_80272D48) == 0) {
    goto fail;
  }
  if (tgr_eeprom_long_read(&D_80272D48, 0, D_80272500, n) != 0) {
    goto fail;
  }
  memcpy(&stored, D_80272500, 4);
  sum = 0xBFED812;
  for (i = 4; i < n; i++) {
    sum += D_80272500[i];
  }
  if (sum != stored) {
    goto fail;
  }
  memcpy(TGR_PTR(BrSeason *, D_8031B760[0].season), data, 0x128);   /* port: the settings as the port keeps them */
  memcpy(TGR_PTR(BrSeason *, D_8031B760[1].season), data, 0x128);
  TGR_PTR(BrSeason *, D_8031B760[1].season)->xd4 = p2[0];
  TGR_PTR(BrSeason *, D_8031B760[1].season)->xd8 = p2[1];
  TGR_PTR(BrSeason *, D_8031B760[1].season)->xdc = p2[2];
  TGR_PTR(BrSeason *, D_8031B760[1].season)->xe0 = p2[3];
  TGR_PTR(BrSeason *, D_8031B760[1].season)->xe4 = p2[4];
  memcpy(D_802723D0, data + 0x13c, 0xc);
  D_802A4BE8 = 1;
  return 1;
fail:
  D_802A4BE8 = 1;
  return 0;
}
#endif

#if TGR_REGION_JPN || TGR_REGION_EUR
/* WHAT IT DOES: Save the settings to the EEPROM: read the block back, and
 * when its checksum is bad first lay both players' season records out in
 * it afresh; then refresh player 1's five per-season words, the three
 * option values and player 2's five words, recompute the checksum and write
 * the block.  Nothing is written without an EEPROM or when the read fails;
 * the SI is marked busy while it works.  Europe also reports a bad and the
 * new checksum on the debug output. */
/* @implements jpn:0x80211D38 tgr BrOptionsEepromSave */
/* @implements eur:0x80211E78 tgr BrOptionsEepromSave */
void BrOptionsEepromSave(void)
{
  int i;
  unsigned char *data;
  int n;
  int check;
  int *p2;
  int u0;                       /* unused: the frame has it */
  int sum;

  data = D_80272500 + 4;
  p2 = (int *)(data + 0x128);
  n = (data - D_80272500 + 0x14f) & ~7;
  D_802A4BE8 = 0;
  if (tgr_eeprom_probe(&D_80272D48) != 0) {
    if (tgr_eeprom_long_read(&D_80272D48, 0, D_80272500, n) == 0) {
      memcpy(&sum, D_80272500, 4);
      check = 0xBFED812;
      for (i = 4; i < n; i++) {
        check += D_80272500[i];
      }
      if (check != sum) {
#if TGR_REGION_EUR
        tgr_print("checksum fail (%08x vs %08x/%02x%02x%02x%02x) (%d)\n", check, sum,
                     D_80272500[0], D_80272500[1], D_80272500[2], D_80272500[3], n);
#endif
        memcpy(data, TGR_PTR(BrSeason *, D_8031B760[0].season), 0x128);
        p2[0] = TGR_PTR(BrSeason *, D_8031B760[1].season)->xd4;
        p2[1] = TGR_PTR(BrSeason *, D_8031B760[1].season)->xd8;
        p2[2] = TGR_PTR(BrSeason *, D_8031B760[1].season)->xdc;
        p2[3] = TGR_PTR(BrSeason *, D_8031B760[1].season)->xe0;
        p2[4] = TGR_PTR(BrSeason *, D_8031B760[1].season)->xe4;
        memcpy(data + 0x13c, D_802723D0, 0xc);
      }
      ((BrSeason *)data)->xd4 = TGR_PTR(BrSeason *, D_8031B760[0].season)->xd4;
      ((BrSeason *)data)->xd8 = TGR_PTR(BrSeason *, D_8031B760[0].season)->xd8;
      ((BrSeason *)data)->xdc = TGR_PTR(BrSeason *, D_8031B760[0].season)->xdc;
      ((BrSeason *)data)->xe0 = TGR_PTR(BrSeason *, D_8031B760[0].season)->xe0;
      ((BrSeason *)data)->xe4 = TGR_PTR(BrSeason *, D_8031B760[0].season)->xe4;
      memcpy(data + 0x13c, D_802723D0, 0xc);
      p2[0] = TGR_PTR(BrSeason *, D_8031B760[1].season)->xd4;
      p2[1] = TGR_PTR(BrSeason *, D_8031B760[1].season)->xd8;
      p2[2] = TGR_PTR(BrSeason *, D_8031B760[1].season)->xdc;
      p2[3] = TGR_PTR(BrSeason *, D_8031B760[1].season)->xe0;
      p2[4] = TGR_PTR(BrSeason *, D_8031B760[1].season)->xe4;
      sum = 0xBFED812;
      for (i = 4; i < n; i++) {
        sum += D_80272500[i];
      }
      memcpy(D_80272500, &sum, 4);
#if TGR_REGION_EUR
      tgr_print("checksum is (%08x/%02x%02x%02x%02x) (%d)\n", sum,
                   D_80272500[0], D_80272500[1], D_80272500[2], D_80272500[3], n);
#endif
      tgr_eeprom_long_write(&D_80272D48, 0, D_80272500, n);
    }
  }
  D_802A4BE8 = 1;
}
#endif

