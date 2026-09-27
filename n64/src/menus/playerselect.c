/* playerselect.c -- the 1P/2P select screen
 */
#include "tgr/common.h"
#include "tgr/menu.h"
#include "tgr/pad.h"

/* -- declarations -- */
void func_80219A78(int param_1,int param_2,int param_3,int param_4);
void BrScreenDim(float param_1);
void BrTextHighlightOff(void);
void BrTextAlignCentre(void);
void BrTextSetFont(int param_1);
void BrTextPrint(char *s, int x, int y);
extern int D_8028AAB0;
extern int D_8028AAB4;
extern MenuItem *D_802723B8[];
extern int D_8026FF08;
extern void (*D_80271FC8)(int row);
void BrFrontMenuEnter(MenuItem **items, int *count);
int BrMenu(char *title, int n, MenuItem **items, int *sel, int (*ok)(int), int, int, int, int, int, int);
void BrModeSet(void (*fn)(void));
void BrMainMenu(void);
void func_80209434(void);
/* -- end declarations -- */

/* WHAT IT DOES: Draw one row of the 1P/2P select screen; when the row needs
 * a second controller that is not present it also draws the notice asking
 * for two controllers to be plugged in. */
/* @implements 0x802114E0 tgr BrPlayerSelectDrawRow */
void BrPlayerSelectDrawRow(int row)
{
  if (D_802723B8[row]->flags & 8) {
    func_80219A78(D_8028AAB0 * 9 / 32, D_8028AAB4 * 27 / 64, D_8028AAB0 * 14 / 32, D_8028AAB4 * 4 / 16);
    BrScreenDim(0.4f);
    func_80219A78(0, 0, D_8028AAB0, D_8028AAB4);
    BrTextHighlightOff();
    BrTextAlignCentre();
    BrTextSetFont(15);
    BrTextPrint("%wwYou Need To Have", D_8028AAB0 / 2, D_8028AAB4 * 10 / 16 - 30);
    BrTextPrint("%wwTwo Controllers", D_8028AAB0 / 2, D_8028AAB4 * 10 / 16 - 15);
    BrTextPrint("%wwPlugged in", D_8028AAB0 / 2, D_8028AAB4 * 10 / 16);
  }
}

/* WHAT IT DOES: The 1P/2P select screen, run once per frame: on entry lay
 * out the two rows and hook the row drawer; flag the two-player row as
 * needing a second controller while either port is empty; on a choice set
 * the player count and go on to the next screen, or back to the main menu. */
/* @implements 0x802116E0 tgr BrPlayerSelectScreen */
void BrPlayerSelectScreen(void)
{
  static int entered = 0;     /* 0x802723C4: 1 once set up, -1 = left for another screen */
  static int count;           /* 0x80316390: the menu's row count */
  static int sel;             /* 0x80316394: the highlighted row */

  if (entered <= 0) {
    BrFrontMenuEnter(D_802723B8, &count);
    D_80271FC8 = BrPlayerSelectDrawRow;
    if (entered == 0) {
      sel = 0;
    }
    entered = 1;
  }
  if (D_8036A8E0[0].absent != 0 || D_8036A8E0[1].absent != 0) {
    D_802723B8[1]->flags |= 8;
  } else {
    D_802723B8[1]->flags &= ~8;
  }
  switch (BrMenu("1P/2P SELECT", count, D_802723B8, &sel, 0, 0, 0, 0, 0x3a, 0x86, 0)) {
  case 1:
    D_8026FF08 = sel + 1;
    BrModeSet(func_80209434);
    entered = -1;
    break;
  case 2:
    BrModeSet(BrMainMenu);
    entered = -1;
    break;
  }
}
