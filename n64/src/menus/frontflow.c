/* frontflow.c -- moving between front-end screens and into races
 */
#include "tgr/common.h"
#include "tgr/menu.h"
#include "tgr/car.h"

/* -- declarations -- */
void BrMainMenu(void);
int func_80209434();
void BrModeSet(int param_1);
extern int D_8026FF08;
int func_8020082C();
extern int D_8026FF18;
extern int D_8026FF1C;
extern int D_8026FF20;
void BrSeasonPickRace(void);
extern int D_80272380;
extern int D_8028C800;
int func_8020D004();
extern int D_80272074;
void BrMenuLoadItems(MenuItem **items, int *count);
void osViBlack(int black);
void BrFrameBeginLayout0(void);
void BrFrameBeginLayout1(void);
void func_8021AA08(void);
int BrRomReadSize(int rom);
unsigned int BrIfaceMemAlloc(int size);
void BrModelLoad(void *dst, int rom);
extern int D_8028A850;
extern int D_8028A884;
void BrIfaceMemReset(void);
extern float D_80271FC4;
extern int D_80271FC8;
extern int D_80271FCC;
extern int D_8028B940;
/* -- end declarations -- */

/* WHAT IT DOES: Go back to the title screen's main menu (used by the paint
 * shop and the Controller Pak menu when they close). */
/* @implements 0x80200108 tgr BrFrontReturnToTitle */
void BrFrontReturnToTitle(void)
{
  BrMainMenu();
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

/* WHAT IT DOES: Start the attract-mode demo race that replays the second
 * recorded race stored in ROM -- or the third when D_8026FF20 is set.
 * The ROM tests the flag set first. */
/* @implements 0x80200634 tgr BrDemoRaceStartB */
void BrDemoRaceStartB(void)
{
  D_8026FF18 = 4;
  if (D_8026FF20 != 0) {
    D_8026FF1C = 2;
  } else {
    D_8026FF1C = 1;
  }
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

/* WHAT IT DOES: Run the results screen for one frame, with the flag set
 * that tells it the race has just finished. */
/* @implements 0x80210F4C tgr BrResultsRun */
void BrResultsRun(void)
{
  D_80272074 = 1;
  func_8020D004();
  D_80272074 = 0;
}

/* WHAT IT DOES: Set up the next Championship race from player 1's season:
 * the track and weather of this race of this round (in a mirrored season
 * the track is swapped for its mirror, five rows away). */
/* @implements 0x80206304 tgr BrSeasonPickRace */
void BrSeasonPickRace(void)
{
  D_8028B940 = D_8028B944[D_8031B760[0].season->round].races[D_8031B760[0].season->race][0];
  if (D_8031B760[0].season->state & 1) {
    if (D_8028B940 < 5) {
      D_8028B940 += 5;
    } else {
      D_8028B940 -= 5;
    }
  }
  D_8028C800 = D_8028B944[D_8031B760[0].season->round].races[D_8031B760[0].season->race][1];
}

/* WHAT IT DOES: Load a menu screen's items: blank the screen for two frames
 * (in the current resolution's layout), then count the NULL-terminated item
 * list and give every item with an icon its decompressed icon, sharing the
 * copy of an earlier item that uses the same ROM image. */
/* @implements 0x8020C27C tgr BrMenuLoadItems */
void BrMenuLoadItems(MenuItem **items, int *count)
{
  int i;

  D_8028A884 = 1;
  if (D_8028A850) {
    osViBlack(1);
    BrFrameBeginLayout1();
    func_8021AA08();
    osViBlack(1);
    BrFrameBeginLayout1();
    func_8021AA08();
  } else {
    osViBlack(1);
    BrFrameBeginLayout0();
    func_8021AA08();
    osViBlack(1);
    BrFrameBeginLayout0();
    func_8021AA08();
  }
  D_8028A884 = 0;
  for (*count = 0; items[*count] != 0; (*count)++) {
    if (items[*count]->iconRomStart != 0) {
      for (i = 0; i < *count; i++) {
        if (items[*count]->iconRomStart == items[i]->iconRomStart) {
          items[*count]->icon = items[i]->icon;
          break;
        }
      }
      if (i == *count) {
        items[*count]->icon = (void *)BrIfaceMemAlloc(BrRomReadSize(items[*count]->iconRomStart));
        BrModelLoad(items[*count]->icon, items[*count]->iconRomStart);
      }
    }
  }
}

/* WHAT IT DOES: Enter a front-end menu screen: empty the interface memory
 * pool, clear the menu's scroll state, and lay out the menu table given. */
/* @implements 0x8020C408 tgr BrFrontMenuEnter */
void BrFrontMenuEnter(MenuItem **items, int *count)
{
  BrIfaceMemReset();
  D_80271FC4 = 0;
  D_80271FC8 = 0;
  D_80271FCC = 0;
  BrMenuLoadItems(items, count);
}
