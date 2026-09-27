/* mainmenu.c -- the title screen's main menu and the two one-player modes
 * it starts directly (one ROM object: 0x80211150-0x802114E0)
 */
#include "tgr/common.h"
#include "tgr/menu.h"

/* -- declarations -- */
void BrModeSet(void (*fn)(void));
void BrFrontMenuEnter(MenuItem **items, int *count);
int BrMenu(char *title, int n, MenuItem **items, int *sel, int, int, int, int, int, int, int);
void BrMusicStart(int mod, int dest);
unsigned int func_8021CD30(unsigned int dst, int rom, unsigned int *size);
void func_802578F4(void);
void BrSeasonPickRace(void);
int func_80209434();
void BrPlayerSelectScreen(void);
void func_8020D004(void);
void func_80211D70(void);
void func_8020686C(void);
void BrOptionsScreen(void);
extern int D_8026FF08;
extern int D_8026FF18;
extern int D_8028C800;
extern int D_802A49C0;
extern int D_80271FA0;
extern int D_80271FA4;
extern int D_80271FA8;
extern void *D_80271FB0;
extern int D_80271FB4;
extern int D_80271FB8;
extern int D_80271FBC;
extern int D_80271FC0;
extern float D_80271FC4;
extern int D_80272070;
extern int D_802722C0;
extern MenuItem *D_80272360[];
extern int D_80272380;
extern unsigned char D_802724F4;
extern int D_80316380;
extern char D_000EBC00[];
extern char D_802AC400[];
/* -- end declarations -- */

/* WHAT IT DOES: Begin a one-player Championship: set the race type, go to
 * track select, and remember that the main menu's Championship row was
 * chosen so the cursor returns there. */
/* @implements 0x80211150 tgr BrChampionshipStart */
void BrChampionshipStart(void)
{
  D_8026FF18 = 0;
  BrSeasonPickRace();
  D_8026FF08 = 1;
  BrModeSet(func_80209434);
  D_80272380 = 1;
}

/* WHAT IT DOES: Begin a one-player Time Attack: set the race type, go to
 * track select, and remember that the main menu's Time Attack row was
 * chosen so the cursor returns there. */
/* @implements 0x80211194 tgr BrTimeAttackStart */
void BrTimeAttackStart(void)
{
  D_8026FF18 = 2;
  D_8026FF08 = 1;
  D_8028C800 = 1;
  BrModeSet(func_80209434);
  D_80272380 = 2;
}

/* WHAT IT DOES: The title screen's main menu, run once per frame: on entry
 * set up the menu (and start the title music if it is not playing), put the
 * cursor back on the mode just left, run the menu and start the chosen
 * mode -- Championship, Arcade, Time Attack, Practice, Paint Shop,
 * Load/Save or Options; B goes back to the attract sequence. */
/* @implements 0x802111E0 tgr BrMainMenu */
void BrMainMenu(void)
{
  static int entered = 0;         /* 0x80272384: 1 once set up, -1 = returning from a sub-screen */
  static int sel;                 /* 0x80316384: the highlighted item */

  if (entered <= 0) {
    D_80271FA4 = -1;
    D_80271FA0 = -1;
    D_8026FF08 = 2;
    BrFrontMenuEnter(D_80272360, &D_80316380);
    if (D_802A49C0 < 8) {
      func_802578F4();
      func_8021CD30(0x80025C00, (int)D_000EBC00, 0);
      BrMusicStart(0x80025C00, (int)D_802AC400);
    }
    D_80271FB0 = &D_802722C0;
    D_80271FB4 = 25;
    D_80271FB8 = 14;
    D_80271FBC = 280;
    D_80271FC0 = 32;
    D_80271FC4 = 20.0f;
    switch (D_80272380) {
    case 1:
      sel = 0;
      break;
    case 2:
      sel = 2;
      break;
    }
    D_80272380 = 0;
    if (entered == 0) {
      sel = 0;
    }
    entered = 1;
  }
  switch (BrMenu("TOP GEAR RALLY", D_80316380, D_80272360, &sel, 0, 0, 0, 0, 0x40, 0x40, 0x40)) {
  case 1:
    D_80272070 = 0;
    switch (sel) {
    case 0:
      BrChampionshipStart();
      break;
    case 1:
      D_8026FF08 = 2;
      D_8026FF18 = 1;
      BrModeSet(BrPlayerSelectScreen);
      break;
    case 2:
      BrTimeAttackStart();
      break;
    case 3:
      D_8026FF08 = 2;
      D_8026FF18 = 3;
      BrModeSet(BrPlayerSelectScreen);
      break;
    case 4:
      D_80272070 = 1;
      D_8026FF08 = 1;
      D_80271FA0 = D_80271FA8;
      D_80271FA4 = 1 << D_80271FA0;
      BrModeSet(func_8020D004);
      break;
    case 5:
      D_802724F4 = 0;
      BrModeSet(func_80211D70);
      break;
    case 6:
      BrModeSet(BrOptionsScreen);
      break;
    }
    entered = -1;
    break;
  case 2:
  case 5:
    BrModeSet(func_8020686C);
    entered = 0;
    break;
  }
}
