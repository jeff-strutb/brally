/* rmcontrols.c (port): the controller screen as Top Gear Rally Remastered has
 * it.  The game's showed the five N64 layouts to pick from; the races here are
 * run by Boss Rally's engine, driven through the app's controls
 * (ports/remastered/platform/controls.h), so the screen sets those instead,
 * for either player, in the carousel the options screen uses:
 *
 *   Player       up, down or A: whose controls the rows below show and set
 *   an action    (Accelerate, Brake, Reverse, Steer Left, Steer Right, Gear Up,
 *                Gear Down, Change View) its key and its pad button under the
 *                label; A (the foot of the screen's Select), then the next key
 *                or pad button pressed, is the new one (a key for the keyboard,
 *                a button for that player's pad); Escape leaves it as it was
 *   Reset        A: the player's controls as they came
 *
 * B goes back to the options screen.  Each row turns the controller the
 * options screen's own row shows.  While a press is awaited the menu sees no
 * presses (they are taken in the help line's hook, before its input). */
#include "tgr/common.h"
#include "tgr/menu.h"
#include "tgr/pad.h"
#include "tgr/romimage.h"

/* -- declarations -- */
void BrTextHighlightOff(void);
void BrTextAlignCentre(void);
void BrTextSetFont(int param_1);
void BrTextPrint(char *s, int x, int y);
void BrFrontMenuEnter(MenuItem **items, int *count);
int BrMenu(char *title, int n, MenuItem **items, int *sel, int (*ok)(int), int, int, int, int, int, int);
void BrModeSet(void (*fn)(void));
void BrOptionsScreen(void);
extern MenuItem *D_80272468[];          /* the options screen's rows: [3] its controller row */
extern int D_80271FA0;                  /* -1, or only that player chooses */
extern int D_80271FA4;
extern int D_80271FA8;                  /* the player who chose */
extern void (*D_80271FC8)(int);         /* the selected row's extras */
extern void *D_80271FB0;                /* the menu panel */
extern int D_80271FB4;
extern int D_80271FB8;
extern int D_80271FBC;
extern int D_80271FC0;
extern BrRomImage D_8027205C;
/* the app's (ports/remastered/platform/controls.h) */
enum { RM_ACT_COUNT = 8, RM_BIND_KEY = 0, RM_BIND_PAD = 1 };
const char *rm_action_name(int act);
int  rm_bind(int player, int act, int kind);
void rm_bind_set(int player, int act, int kind, int code);
void rm_bind_reset(int player);
typedef struct host_pad { float x, y, z, rx, ry; unsigned buttons; int pov; } host_pad;   /* the host's (host.h) */
int host_pad_read(host_pad *p);
int host_pad_read_n(int i, host_pad *p);
const char *rm_bind_label(int kind, int code, char *buf, int n);
const unsigned char *tgr_input_dik(void);   /* si.c: the scan codes held */
/* -- end declarations -- */

#define ROWS (RM_ACT_COUNT + 2)         /* the player, the actions, reset */

static MenuItem s_row[ROWS];
static MenuItem *s_items[ROWS + 1];
static char s_label[ROWS][32];
static int s_count, s_sel, s_set, s_player;
static int s_wait = -1;                 /* the action awaiting a press, -1 none */
static unsigned char s_was[256];        /* the keys held when the wait began */
static unsigned s_padwas;               /* and the player's pad's buttons */

static unsigned pad_buttons(int player)
{
  host_pad p;
  int has = player == 0 ? host_pad_read(&p) : host_pad_read_n(player, &p);
  return has ? p.buttons : 0;
}

static void labels(void)
{
  int a;
  sprintf(s_label[0], "Player %d", s_player + 1);
  for (a = 0; a < RM_ACT_COUNT; a++) {
    sprintf(s_label[1 + a], "%s", rm_action_name(a));
  }
  sprintf(s_label[ROWS - 1], "Reset Player %d", s_player + 1);
  for (a = 0; a < ROWS; a++) {
    s_row[a].label = s_label[a];
  }
}

/* the selected row's lines, and the awaited press */
static void help(int row)
{
  char buf[96], k[40], p[40];
  int i;
  const unsigned char *dik = tgr_input_dik();

  BrTextSetFont(10);
  BrTextHighlightOff();
  BrTextAlignCentre();
  if (s_wait >= 0) {
    unsigned b = pad_buttons(s_player);
    int got = 0;
    for (i = 0; i < 4; i++) {           /* the menu sees nothing meanwhile */
      D_8036A8E0[i].pressed = 0;
      D_8036A8E0[i].axis[0] = 0.0f;
      D_8036A8E0[i].axis[1] = 0.0f;
    }
    if (dik[0x01] && !s_was[0x01]) {    /* Escape: as it was */
      s_wait = -1;
      return;
    }
    for (i = 2; i < 256 && !got; i++) {
      if (dik[i] && !s_was[i]) {
        rm_bind_set(s_player, s_wait, RM_BIND_KEY, i);
        got = 1;
      }
    }
    for (i = 0; i < 16 && !got; i++) {
      if ((b >> i) & 1 && !((s_padwas >> i) & 1)) {
        rm_bind_set(s_player, s_wait, RM_BIND_PAD, i);
        got = 1;
      }
    }
    for (i = 0; i < 256; i++) {         /* (a key let go may be pressed again) */
      s_was[i] = s_was[i] && dik[i];
    }
    s_padwas &= b;
    if (got) {
      s_wait = -1;
      return;
    }
    BrTextPrint("%wwPress a key or button (Escape: as it was)", 160, 90);
    return;
  }
  if (row == 0) {
    BrTextPrint("%wwPush up or down to choose", 160, 90);
  } else if (row <= RM_ACT_COUNT) {
    rm_bind_label(RM_BIND_KEY, rm_bind(s_player, row - 1, RM_BIND_KEY), k, sizeof k);
    rm_bind_label(RM_BIND_PAD, rm_bind(s_player, row - 1, RM_BIND_PAD), p, sizeof p);
    sprintf(buf, "%%wwKey: %s    Pad: %s", k, p);
    BrTextPrint(buf, 160, 90);
  } else {
    BrTextPrint("%wwPress A to restore the defaults", 160, 90);
  }
}

/* the screen, each frame (options.c, port: its controller row) */
void rm_controls_screen(void)
{
  int i;

  if (!s_set) {
    MenuItem *ctl = D_80272468[3];      /* (the options screen's controller row: its icon) */
    for (i = 0; i < ROWS; i++) {
      s_row[i].flags = i == 0 ? 0x03 : 0x02;   /* the player: up, down, A; the rest A at once */
      s_row[i].iconRomStart = ctl->iconRomStart;
      s_row[i].iconRomEnd = ctl->iconRomEnd;
      s_items[i] = &s_row[i];
    }
    s_items[ROWS] = 0;
    s_player = D_80271FA0 == 1 ? 1 : 0;
    D_80271FA0 = -1;                    /* either player may set either's */
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
    s_wait = -1;
    s_set = 1;
  }
  labels();
  switch (BrMenu("CONTROLS", s_count, s_items, &s_sel, 0, 8, 0, 0, 0xa0, 0x80, 0x20)) {
  case 1:
  case 3:
  case 4:
    if (s_wait >= 0) {
      break;
    }
    if (s_sel == 0) {
      s_player ^= 1;
    } else if (s_sel <= RM_ACT_COUNT) {
      const unsigned char *dik = tgr_input_dik();
      s_wait = s_sel - 1;
      memcpy(s_was, dik, sizeof s_was);
      s_padwas = pad_buttons(s_player);
    } else {
      rm_bind_reset(s_player);
    }
    break;
  case 2:
    BrModeSet(BrOptionsScreen);
    s_set = 0;
    break;
  }
}
