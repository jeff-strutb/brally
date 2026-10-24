/* keysmenu.c (port): Keys and Pad, a row of the options screen after the
 * controller row, and its screen: the keys and game pad buttons that are the
 * N64 pad's (platform/os/si.c, tgr_keys.h).  The cartridge has no such
 * screen: on the N64 the pad is the pad.  The controller row still opens the
 * game's own screen, its five layouts saying what each of the pad's buttons
 * does; this one says which key or button is each of the pad's.
 *
 *   an input   (A, B, Z, Start, L, R, the C buttons, the d-pad, the stick's
 *              four ways) its keys and pad buttons under the label; A (the
 *              foot of the screen's Select), then the next key or pad button
 *              pressed, is the new one; Escape leaves it as it was
 *   Reset      A: the defaults
 *
 * B goes back to the options screen.  Each row turns the controller the
 * options screen's own row shows.  In a scripted run there is no such row,
 * so the screens are the cartridge's (tools/suite.py). */
#include "tgr/common.h"
#include "tgr/menu.h"
#include "tgr/romimage.h"
#include "tgr_keys.h"

/* -- declarations -- */
void BrTextHighlightOff(void);
void BrTextAlignCentre(void);
void BrTextSetFont(int param_1);
void BrTextPrint(char *s, int x, int y);
void BrFrontMenuEnter(MenuItem **items, int *count);
int BrMenu(char *title, int n, MenuItem **items, int *sel, int (*ok)(int), int, int, int, int, int, int);
void BrModeSet(void (*fn)(void));
void BrOptionsScreen(void);
extern int D_802724E0;                  /* the options screen: -1 returning from a sub-screen */
extern int D_80271FA0;                  /* -1, or only that player chooses */
extern int D_80271FA4;
extern void (*D_80271FC8)(int);         /* the selected row's extras */
extern void *D_80271FB0;                /* the menu panel */
extern int D_80271FB4;
extern int D_80271FB8;
extern int D_80271FBC;
extern int D_80271FC0;
extern BrRomImage D_8027205C;
/* -- end declarations -- */

#define CTL_ROW 3                       /* the options screen's controller row */
#define ROWS (TGR_KEYS_COUNT + 1)       /* the inputs, reset */

static MenuItem s_opt;                  /* the options screen's row */
static MenuItem *s_opt_items[24];
static int s_opt_n;
static MenuItem s_row[ROWS];
static MenuItem *s_items[ROWS + 1];
static int s_count, s_sel, s_set;

/* the options screen's rows: the game's, Keys and Pad after its controller row
 * (NULL-terminated); the game's own in a scripted run */
MenuItem **tgr_keys_items(MenuItem **game)
{
  int i, n = 0;

  if (!tgr_keys_shown()) {
    return game;
  }
  s_opt.label = ("Keys and Pad");
  s_opt.flags = game[CTL_ROW]->flags;   /* a row like the controller's: A chooses it */
  s_opt.iconRomStart = game[CTL_ROW]->iconRomStart;
  s_opt.iconRomEnd = game[CTL_ROW]->iconRomEnd;
  for (i = 0; game[i] != 0 && n < 22; i++) {
    s_opt_items[n++] = game[i];
    if (i == CTL_ROW) {
      s_opt_items[n++] = &s_opt;
    }
  }
  s_opt_items[n] = 0;
  s_opt_n = n;
  return s_opt_items;
}

void tgr_keys_screen(void);

/* the options screen's answer on `row`: Keys and Pad taken here (-1 back:
 * nothing for the game's switch); else the row's number in the game's own */
int tgr_keys_row(int result, int row)
{
  if (!tgr_keys_shown() || row <= CTL_ROW) {
    return row;
  }
  if (row == CTL_ROW + 1 && result != 2) {   /* (B, 2, is the screen's: its switch goes back) */
    if (result == 1) {
      D_802724E0 = -1;
      BrModeSet(tgr_keys_screen);
    }
    return -1;
  }
  return row - 1;
}

static void labels(void)
{
  int i;

  for (i = 0; i < TGR_KEYS_COUNT; i++) {
    s_row[i].label = (tgr_keys_name(i));
  }
  s_row[ROWS - 1].label = ("Reset");
}

/* the selected row's line, and the awaited press */
static void help(int row)
{
  char buf[96], k[40], p[40];

  BrTextSetFont(10);
  BrTextHighlightOff();
  BrTextAlignCentre();
  if (tgr_keys_capturing() >= 0) {
    BrTextPrint("%wwPress a key or button (Escape: as it was)", 160, 90);
  } else if (row < TGR_KEYS_COUNT) {
    tgr_keys_labels(row, k, sizeof k, p, sizeof p);
    sprintf(buf, "%%wwKey: %s    Pad: %s", k, p);
    BrTextPrint(buf, 160, 90);
  } else {
    BrTextPrint("%wwPress A to restore the defaults", 160, 90);
  }
}

/* the screen, each frame */
void tgr_keys_screen(void)
{
  int i;

  if (!s_set) {
    MenuItem *ctl = s_opt_items[CTL_ROW];   /* (the controller row: its icon) */
    for (i = 0; i < ROWS; i++) {
      s_row[i].flags = 0x02;            /* A at once */
      s_row[i].iconRomStart = ctl->iconRomStart;
      s_row[i].iconRomEnd = ctl->iconRomEnd;
      s_items[i] = &s_row[i];
    }
    s_items[ROWS] = 0;
    D_80271FA0 = -1;
    D_80271FA4 = -1;
    labels();
    BrFrontMenuEnter(s_items, &s_count);
    D_80271FB0 = &D_8027205C;
    D_80271FB4 = 22;
    D_80271FB8 = 9;
    D_80271FBC = 280;
    D_80271FC0 = 42;
    D_80271FC8 = help;
    s_sel = 0;
    s_set = 1;
  }
  switch (BrMenu("KEYS AND PAD", s_count, s_items, &s_sel, 0, 8, 0, 0, 0xa0, 0x80, 0x20)) {
  case 1:
  case 3:
  case 4:
    if (tgr_keys_capturing() >= 0) {
      break;
    }
    if (s_sel < TGR_KEYS_COUNT) {
      tgr_keys_capture(s_sel);
    } else {
      tgr_keys_reset();
    }
    break;
  case 2:
    BrModeSet(BrOptionsScreen);
    s_set = 0;
    break;
  }
}
