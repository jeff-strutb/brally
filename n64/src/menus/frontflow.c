/* frontflow.c -- moving between front-end screens and into races
 */
#include "tgr/common.h"

/* -- declarations -- */
void func_802111E0(void);
int func_80209434();
void BrModeSet(int param_1);
extern int D_8026FF08;
int func_8020082C();
extern int D_8026FF18;
extern int D_8026FF1C;
void func_80206304(void);
extern int D_80272380;
extern int D_8028C800;
int func_8020D004();
extern int D_80272074;
void func_8020C27C(int *param_1,int *param_2);
void BrIfaceMemReset(void);
extern float D_80271FC4;
extern int D_80271FC8;
extern int D_80271FCC;
/* -- end declarations -- */

/* WHAT IT DOES: Go back to the title screen's main menu (used by the paint
 * shop and the Controller Pak menu when they close). */
/* @implements 0x80200108 tgr BrFrontReturnToTitle */
void BrFrontReturnToTitle(void)
{
  func_802111E0();
}

/* WHAT IT DOES: Open the track-select screen for the given number of human
 * players. */
/* @implements 0x80200128 tgr BrTrackSelectOpen */
void BrTrackSelectOpen(int param_1)
{
  D_8026FF08 = param_1;
  BrModeSet(func_80209434);
}

/* WHAT IT DOES: Start the attract-mode demo race that replays the first of
 * the three recorded races stored in ROM. The credits sequence calls it
 * when it runs out. */
/* @implements 0x802005FC tgr BrDemoRaceStartA */
void BrDemoRaceStartA(void)
{
  D_8026FF18 = 4;
  D_8026FF1C = 0;
  BrModeSet(func_8020082C);
}

/* WHAT IT DOES: Start the attract-mode demo race that replays the third
 * recorded race stored in ROM. */
/* @implements 0x8020068C tgr BrDemoRaceStartC */
void BrDemoRaceStartC(void)
{
  D_8026FF18 = 4;
  D_8026FF1C = 2;
  BrModeSet(func_8020082C);
}

/* WHAT IT DOES: Begin a one-player Championship: set the race type, go to
 * track select, and remember that the main menu's Championship row was
 * chosen so the cursor returns there. */
/* @implements 0x80211150 tgr BrChampionshipStart */
void BrChampionshipStart(void)
{
  D_8026FF18 = 0;
  func_80206304();
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

/* WHAT IT DOES: Run the results screen for one frame, with the flag set
 * that tells it the race has just finished. */
/* @implements 0x80210F4C tgr BrResultsRun */
void BrResultsRun(void)
{
  D_80272074 = 1;
  func_8020D004();
  D_80272074 = 0;
}

/* WHAT IT DOES: Enter a front-end menu screen: empty the interface memory
 * pool, clear the menu's scroll state, and lay out the menu table given. */
/* @implements 0x8020C408 tgr BrFrontMenuEnter */
void BrFrontMenuEnter(int param_1,int param_2)
{
  BrIfaceMemReset();
  D_80271FC4 = 0;
  D_80271FC8 = 0;
  D_80271FCC = 0;
  func_8020C27C(param_1,param_2);
}
