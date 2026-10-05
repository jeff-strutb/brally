/* weatherselect.c -- the weather-select screen
 */
#include "tgr/common.h"
#include "tgr/menu.h"

/* -- declarations -- */
extern int D_8026FF08;
extern int D_8028B940;
int BrWeatherSelectable(int always);
void BrFrontMenuEnter(MenuItem **items, int *count);
int BrMenu(char *title, int n, MenuItem **items, int *sel, int (*ok)(int), int, int, int, int, int, int);
void BrModeSet(void (*fn)(void));
void func_8020D004(void);
void func_80209434(void);
extern MenuItem *D_802722A4[];
extern int D_80316370;
extern int D_8028C800;
extern int D_8028AE04;
/* -- end declarations -- */

/* WHAT IT DOES: Tell whether a weather row may be chosen: always when the
 * caller allows it, otherwise only for one player on tracks other than the
 * two that force their own weather. */
/* @implements 0x80210F80 tgr BrWeatherSelectable */
int BrWeatherSelectable(int always)
{
  return always || (D_8028B940 != 3 && D_8028B940 != 8 && D_8026FF08 == 1);
}

/* WHAT IT DOES: The weather-select screen, run once per frame: on entry lay
 * out the menu, start from the last weather chosen (or keep the cursor when
 * coming back) and step past rows that may not be chosen; A keeps the
 * weather and goes on to car select, B goes back to track select. */
/* @implements 0x80210FC8 tgr BrWeatherScreen */
void BrWeatherScreen(void)
{
  static int entered = 0;     /* 0x802722BC: 1 once set up, -1 = returning from a sub-screen */
  static int sel;             /* 0x80316374: the highlighted weather */

  if (entered <= 0) {
    BrFrontMenuEnter(D_802722A4, &D_80316370);
    if (entered == 0) {
      sel = D_8028C800;
    }
    while (!BrWeatherSelectable(sel)) {
      sel = (sel + 1) % D_8028AE04;
    }
    entered = 1;
  }
  switch (BrMenu("WEATHER SELECT", D_80316370, D_802722A4, &sel, BrWeatherSelectable, 0, 0, 0, 0x20, 0, 0xa0)) {
  case 1:
    D_8028C800 = sel;
    BrModeSet(func_8020D004);
    entered = -1;
    break;
  case 2:
    BrModeSet(func_80209434);
    entered = -1;
    break;
  }
}
