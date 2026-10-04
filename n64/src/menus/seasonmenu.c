/* seasonmenu.c -- the season screen: round progression and its help line
 */
#include "tgr/common.h"
#include "tgr/car.h"
#include "tgr/menu.h"
#include "tgr/pad.h"

/* -- declarations -- */
extern int D_802723D0;
extern int D_8028B94C;
extern char * D_8031C5BC;
void BrSeasonValidate(void);
void BrFrontSetMenuFlag(int param_1);
void BrChampionshipStart(void);
void BrTimeAttackStart(void);
int BrCpakCheck(int param_1,char param_2);
typedef struct BrStream { char pad00[0x28]; } BrStream;
int BrEntLoadModel(BrStream *s);
void BrTextHighlightOff(void);
void BrTextAlignCentre(void);
void BrTextSetFont(int param_1);
void BrTextPrint(int param_1,int param_2,int param_3);
int sprintf();
extern int D_8026FF18;
extern int D_8028AE04;
extern int D_8028B940;
extern char D_8028B950;
extern unsigned char D_8028B954;
extern unsigned char D_80307F00;
extern int D_80315EE0;
extern float D_8028AAD8;                /* the frame time */
extern int D_8028AAB0;                  /* the screen width */
extern int D_8028AAB4;                  /* and height */
extern MenuItem *D_80271D1C[];          /* the track menu's rows */
extern MenuItem *D_802722A4[];          /* the weather menu's rows */
extern int D_8028AA78;
extern int D_8028AA80;
extern int D_8028AA84;
extern int D_8028AA8C;
typedef struct BrSeasonView { int x; int y; int w; int h; int car; } BrSeasonView;
extern BrSeasonView D_8031B2C8[2];
void BrTextHighlightOff(void);
void BrTextAlignLeft(void);
void BrTextAlignRight(void);
void BrTextSetColours(int r1, int g1, int b1, int r2, int g2, int b2);
void BrTimeFormat(char *buf, float t);
void func_802182A8(void);
void BrFrameTintSetup(void);
void func_8020C6D0(int a, int b, int c, int x, int y, int w, int h, float d, int e, int f, int g, float k, float l);
void BrScissorSet(int x, int y, int w, int h);
extern int D_8028C328;
extern int D_8028C800;                  /* the chosen weather */
extern int D_8028B304;                  /* laps in the race */
extern int D_80270784;
extern int D_80270850;                  /* coming back from a race */
extern int D_80271FAC;                  /* the track row on entry */
extern void (*D_80271FC8)(int);         /* the menu panel's draw hook */
extern void (*D_80271FCC)(void);        /* the screen's draw hook */
extern BrStream D_8031B370[4];
void BrFrontMenuEnter(MenuItem **items, int *count);
void BrPaintShopMemInit(void);
void BrCarDefaultColour(BrCar *car);
void BrCarCamStep(void);
void BrEntLoadRecord(BrCar *car, int slot, int kind);
void BrVec3Scale(void *pOut, void *pV, float s);
void BrVec3Normalise(void *pV);
void BrVec3Cross(void *pOut, void *pA, void *pB);
void *memcpy(void *dst, void *src, unsigned int n);
int BrTrackSelectable(int n);
int BrMenu(char *title, int n, MenuItem **items, int *sel, int (*ok)(int), int, int, int, int, int, int);
void BrModeSet(void (*fn)(void));
void BrCarModelStream(BrStream *s, int slot, int car, int bufIdx);
void func_8020D004(void);
void BrWeatherScreen(void);
void BrMainMenu(void);
void BrPlayerSelectScreen(void);
/* -- end declarations -- */

static int loadPending = 0;     /* 0x80271D58: Load Season/Ghost Data was chosen; the help panel runs the load */

/* WHAT IT DOES: Make player 1's season record consistent: a negative state
 * starts it over; a round outside 0..5 goes back to round 0; a race outside
 * the round's race count goes back to race 0 and clears that round's
 * points. */
/* @implements 0x802089D0 tgr BrSeasonValidate */
void BrSeasonValidate(void)
{
  if (D_8031B760[0].season->state >= 0) {
    goto check;
  }
  D_8031B760[0].season->state = 0;
bad_round:
  D_8031B760[0].season->round = 0;
bad_race:
  D_8031B760[0].season->race = 0;
  D_8031B760[0].season->points[D_8031B760[0].season->round] = 0;
check:
  if (D_8031B760[0].season->round < 0 || D_8031B760[0].season->round >= 6) {
    goto bad_round;
  }
  if (D_8031B760[0].season->race < 0 || D_8031B760[0].season->race >= D_8028B944[D_8031B760[0].season->round].x8) {
    goto bad_race;
  }
}


/* WHAT IT DOES: Draw the help line at the foot of the season screen: the
 * lap-count hint in Arcade, and in a Championship how many points are still
 * needed to advance (or that the player already has enough), with the
 * points table. */
/* @t4-pass 0x80208A58 1 2026-09-26 compiles 17 best 84 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80208A58 2 2026-09-26 compiles 17 best 84 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80208A58 3 2026-09-26 compiles 17 best 84 moved 0  (n64/tools/n64permute.py) */
/* RESIDUE (5): in the time-attack track bit test the ROM loads the season
 * into t1 and the 1 into t0; ours swaps the two temporaries. */
/* @t3 0x80208A58 */
/* @implements 0x80208A58 tgr BrSeasonDrawHelp */
void BrSeasonDrawHelp(int row)
{
  static char text[80];       /* 0x80315EE8 */
  int i;
  int need;

  if (D_8026FF18 == 1) {
    BrTextSetFont(10);
    BrTextHighlightOff();
    BrTextAlignCentre();
    BrTextPrint((int)"%wwPush up or down to adjust number of laps", 0xa0, 0x5a);
  }
  if (D_8026FF18 == 0 && row < D_8028AE04) {
    BrTextSetFont(10);
    BrTextHighlightOff();
    BrTextAlignCentre();
    need = D_8028B944[D_8031B760[0].season->round].xc - D_8031B760[0].season->points[D_8031B760[0].season->round];
    if (need <= 0) {
      sprintf(text, "%%wwYou have enough points to advance to next season");
    } else {
      BrTextPrint((int)"%wwFirst=9, Second=6, Third=5, Fourth=3, Fifth=2, Sixth=1", 0xa0, 100);
      sprintf(text, "%%ww%d point%s needed to advance to next season", need, need == 1 ? "" : "s");
    }
    BrTextPrint((int)text, 0xa0, 0x5a);
  }
  if (loadPending != 0) {
    if (BrCpakCheck(D_8026FF18 == 2, 0)) {
      loadPending = 0;
      if (D_8026FF18 == 0) {
        BrSeasonValidate();
        BrChampionshipStart();
        for (i = 0; i < 2; i++) {
          BrCarModelStream(&D_8031B370[i], i, D_8028B944[D_8031B760[0].season->round].kinds[i], i);
          while (BrEntLoadModel(&D_8031B370[i])) {
          }
        }
        D_80315EE0 = 1;
      } else if (D_8026FF18 == 2) {
        BrTimeAttackStart();
        if (D_8031B760[0].season->xce & (1 << D_80307F00)) {
          D_8028B940 = D_80307F00;
        }
      }
      BrFrontSetMenuFlag(D_8028B940);
    } else {
      D_8036A8E0[0].pressed = 0;
      D_8036A8E0[0].axis[0] = 0.0f;
    }
  }
}

/* WHAT IT DOES: Draw the season screen: the season's name, the round's
 * races (track and weather, mirrored on a mirror season; the one to run
 * next blinks yellow) with the player's race and lap records for each track,
 * and the cars on offer, with the two car views set up side by side.
 * RESIDUE (440): register choice -- the ROM spills the race and the round
 * pointer (0x84, 0x70, frame 0xB0) and keeps the round in s0, the state in s4
 * (state & 1 in fp) and the text buffer in s2; ours spills the state and the
 * race count reloads each pass of the first loop.  Structure, calls and the
 * blink clock match. */
/* @t4-pass 0x80208CF0 1 2026-09-29 compiles 196 best 440 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80208CF0 2 2026-09-29 compiles 196 best 440 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x80208CF0 */
/* @implements 0x80208CF0 tgr BrSeasonDraw */
void BrSeasonDraw(void)
{
  static float blink = 0.0f;  /* 0x80271D5C: the next race's blink clock, seconds */
  static char text[80];       /* 0x80315F38 */
  int race;
  int round;
  unsigned int state;
  BrRound *r;
  int i;
  int k;
  int off;
  int y;

  y = 95;
  for (blink += D_8028AAD8; blink > 0.75f; blink -= 0.75f) {
  }
  race = D_8031B760[0].season->race;
  round = D_8031B760[0].season->round;
  state = D_8031B760[0].season->state;
  BrTextHighlightOff();
  BrTextAlignCentre();
  BrTextSetFont(20);
  r = &D_8028B944[round];
  sprintf(text, "%%ry%s:", (char *)r->x4);
  BrTextPrint((int)text, D_8028AAB0 / 2, D_8028AAB4 * 20 / 64);
  off = 0;
  for (i = 0; i < r->x8; i++) {
    if (D_8028B944[round].races[i][0] < 5) {
      if (state & 1) {
        off = D_8028AAB0 * 3 / 64;
        break;
      }
    } else if (!(state & 1)) {
      off = D_8028AAB0 * 3 / 64;
      break;
    }
  }
  BrTextSetFont(8);
  BrTextAlignRight();
  BrTextPrint((int)"Race Record", D_8028AAB0 * 23 / 32 + off, 0x56);
  BrTextPrint((int)"Lap Record", D_8028AAB0 * 28 / 32 + off + 2, 0x56);
  BrTextSetFont(10);
  for (i = 0; i < r->x8; i++) {
    k = D_8028B944[round].races[i][0];
    if (state & 1) {
      if (k < 5) {
        k += 5;
      } else {
        k -= 5;
      }
    }
    BrTextAlignLeft();
    if (i == race && blink < 0.4f) {
      BrTextSetColours(0xFF, 0xF0, 0x50, 0xFF, 0xF0, 0x50);
    } else {
      BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);
    }
    sprintf(text, "Race %d", i + 1);
    BrTextPrint((int)text, D_8028AAB0 * 4 / 32 - off, y);
    sprintf(text, "%s/%s", D_80271D1C[k]->label, D_802722A4[D_8028B944[round].races[i][1]]->label);
    BrTextPrint((int)text, D_8028AAB0 * 8 / 32 - off + 4, y);
    BrTextAlignRight();
    if (D_8031B760[0].season->xe8[k] != 0) {
      BrTimeFormat(text, D_8031B760[0].season->xe8[k]);
    } else {
      sprintf(text, "--    ");
    }
    BrTextPrint((int)text, D_8028AAB0 * 23 / 32 + off - 1, y);
    if (D_8031B760[0].season->x8c[k] != 0) {
      BrTimeFormat(text, D_8031B760[0].season->x8c[k]);
    } else {
      sprintf(text, "--    ");
    }
    BrTextPrint((int)text, D_8028AAB0 * 28 / 32 + off, y);
    y += 10;
  }
  BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);
  BrTextAlignCentre();
  BrTextPrint((int)"Cars For", D_8028AAB0 >> 1, 0x94);
  BrTextPrint((int)"This Season:", D_8028AAB0 >> 1, 0x9E);
  D_8028AA78 = 0;
  D_8028AA80 = 0;
  D_8028AA8C = 0;
  D_8028AA84 = 0;
  func_802182A8();
  BrFrameTintSetup();
  D_8031B2C8[0].car = 0;
  D_8031B2C8[1].car = 1;
  func_8020C6D0(0, 0, 0, 0, D_8028AAB4 * 7 / 16, D_8028AAB0 * 5 / 8, D_8028AAB4 * 5 / 8, 0.0f, 1, 0, 0, 0.0f, 1.5707964f);
  func_8020C6D0(0, 0, 1, D_8028AAB0 * 3 / 8, D_8028AAB4 * 7 / 16, D_8028AAB0 * 5 / 8, D_8028AAB4 * 5 / 8, 0.0f, 1, 1, 1, 0.0f, 0.0f);
  BrScissorSet(0, 0, D_8028AAB0, D_8028AAB4);
}

/* WHAT IT DOES: The track-select screen, run once per frame.  On entry it
 * lays out the track menu (a track row's mirror flag follows the mode),
 * sets up the two display cars and a fixed camera looking at them, starts
 * the cursor on the season's track (stepping past rows that may not be
 * chosen) and names the extra rows: the season's load and reset rows in a
 * Championship, the ghost car's otherwise.  Each frame the highlighted row
 * shows what it leads to (the race number and weather in a Championship, or
 * the lap count in Arcade), then the menu is run: A on a track goes on to
 * weather or car select, A on an extra row loads data or resets the season
 * (or the ghost car), B goes back, and up and down set the lap count in
 * Arcade. */
/* The TU's statics say how it was written: the menu state is function
 * static (a global would keep its address in a register), and the unused
 * 100-byte buffer is what the ROM frame holds above the two named slots.
 * The menu's result reuses the loop counter i (one declared local fewer
 * puts the hoisted BrCarCamStep spill at the ROM's 0x5C), and the label is
 * read through pp, so the row's item takes v1 as in the ROM. */
/* @implements 0x80209434 tgr BrTrackSelectScreen */
void BrTrackSelectScreen(void)
{
  static int entered = 0;     /* 0x80271D60: 1 once set up, -1 = returning from a sub-screen */
  static int count;           /* 0x80315F88: the menu's row count */
  static int sel;             /* 0x80315F8C: the highlighted row */
  static int season;          /* 0x80315F90: this is the Championship's track screen */
  static char text[80];       /* 0x80315F98: the highlighted row's label while it is shown */
  char buf[100];
  MenuItem **pp;
  MenuItem *item;
  const char *label;
  int idx;
  int i;

  if (entered <= 0) {
    BrFrontMenuEnter(D_80271D1C, &count);
    for (i = 0; i < D_8028AE04; i++) {
      if (D_8026FF18 == 1) {
        D_80271D1C[i]->flags |= 1;
      } else {
        D_80271D1C[i]->flags &= ~1;
      }
    }
    BrPaintShopMemInit();
    BrSeasonValidate();
    for (i = 0; i < 2; i++) {
      BrCarDefaultColour(&D_8031B760[i]);
      D_8031B760[i].xed8 = (int)BrCarCamStep;
      BrEntLoadRecord(&D_8031B760[i], i, D_8028B944[D_8031B760[0].season->round].kinds[i]);
      D_8031B760[i].x2000 = 0;
      D_8031B760[i].x2044 = 0;
      D_8031B760[i].mtx0[0][0] = 1.0f;
      D_8031B760[i].mtx0[0][1] = 0.0f;
      D_8031B760[i].mtx0[0][2] = 0.0f;
      D_8031B760[i].mtx0[1][0] = 0.0f;
      D_8031B760[i].mtx0[1][1] = 1.0f;
      D_8031B760[i].mtx0[1][2] = 0.0f;
      D_8031B760[i].mtx0[2][0] = 0.0f;
      D_8031B760[i].mtx0[2][1] = 0.0f;
      D_8031B760[i].mtx0[2][2] = 1.0f;
      D_8031B760[i].mtx0[3][0] = 0.0f;
      D_8031B760[i].mtx0[3][1] = 0.0f;
      D_8031B760[i].mtx0[3][2] = 0.254f;
    }
    D_8031B760[0].cams[3].mtx[3][0] = 10.0f;
    D_8031B760[0].cams[3].mtx[3][1] = 10.0f;
    D_8031B760[0].cams[3].mtx[3][2] = 5.0f;
    BrVec3Scale(D_8031B760[0].cams[3].mtx[0], D_8031B760[0].cams[3].mtx[3], -1.0f);
    BrVec3Normalise(D_8031B760[0].cams[3].mtx[0]);
    D_8031B760[0].cams[3].mtx[2][0] = 0.0f;
    D_8031B760[0].cams[3].mtx[2][1] = 0.0f;
    D_8031B760[0].cams[3].mtx[2][2] = 1.0f;
    BrVec3Cross(D_8031B760[0].cams[3].mtx[1], D_8031B760[0].cams[3].mtx[2], D_8031B760[0].cams[3].mtx[0]);
    BrVec3Cross(D_8031B760[0].cams[3].mtx[2], D_8031B760[0].cams[3].mtx[0], D_8031B760[0].cams[3].mtx[1]);
    memcpy(&D_8031B760[1].cams[3], &D_8031B760[0].cams[3], sizeof(BrCarCam));
    D_8028C328 = 0;
    sel = D_8028B940;
    while (!BrTrackSelectable(sel)) {
      sel = (sel + 1) % D_8028AE04;
    }
    D_80271FAC = sel;
    D_80271FC8 = BrSeasonDrawHelp;
    season = D_8026FF18 == 0;
    D_80315EE0 = season && D_80270850 == 0;
    D_80270850 = 0;
    if (D_8026FF18 == 0) {
      D_80271D1C[D_8028AE04]->label = "Load Season Data";
      D_80271D1C[D_8028AE04 + 1]->label = "Reset To First Round";
      D_80271D1C[D_8028AE04 + 2]->label = "Reset To First Season";
      D_80271D1C[D_8028AE04 + 3]->label = "Reset To First Year";
    } else {
      D_80271D1C[D_8028AE04]->label = "Load Ghost Car Data";
      D_80271D1C[D_8028AE04 + 2]->label = "Reset Ghost Car";
    }
    entered = 1;
  }
  if (D_8026FF18 == 0) {
    idx = D_8028B940;
    pp = &D_80271D1C[idx];
    item = *pp;
    label = (*pp)->label;
    if (D_80315EE0) {
      D_80271FCC = BrSeasonDraw;
      item->flags |= 2;
    } else {
      D_80271FCC = 0;
      sprintf(text, "Race %d: %s/%s", D_8031B760[0].season->race + 1, label,
              D_802722A4[D_8028C800]->label);
      (*pp)->label = text;
    }
    i = BrMenu((char *)D_8028B944[D_8031B760[0].season->round].x0, count, D_80271D1C, &sel,
               BrTrackSelectable, 0, 0, 0, 0, 0x78, 0x82);
    if (D_80315EE0) {
      (*pp)->flags &= ~2;
    }
  } else {
    if (D_8026FF18 == 1) {
      idx = D_80271FAC;
      pp = &D_80271D1C[idx];
      label = (*pp)->label;
      sprintf(text, "%s: %d laps", label, D_8028B304);
      (*pp)->label = text;
    }
    i = BrMenu("TRACK SELECT", count, D_80271D1C, &sel, BrTrackSelectable, 0, 0, 0, 0x80, 0, 0x80);
  }
  switch (i) {
  case 3:
    if (D_8026FF18 == 1 && D_8028B304 < 5) {
      D_8028B304++;
    }
    break;
  case 4:
    if (D_8026FF18 == 1 && D_8028B304 > 1) {
      D_8028B304--;
    }
    break;
  case 1:
    if (sel == D_8028AE04) {
      loadPending = 1;
    } else if (sel == D_8028AE04 + 1) {
      sel = sel;
      if (D_8026FF18 == 0) {
        goto reset_race;
      }
    } else if (sel == D_8028AE04 + 2) {
      sel = sel;
      if (D_8026FF18 == 0) {
        goto reset_round;
      }
      if (D_8026FF18 == 2) {
        D_80270784 = 8;
        BrTimeAttackStart();
        BrFrontSetMenuFlag(sel = D_8028B940);
      }
    } else if (sel == D_8028AE04 + 3) {
      sel = sel;
      if (D_8026FF18 == 0) {
        D_8031B760[0].season->state = 0;
      reset_round:
        D_8031B760[0].season->round = 0;
      reset_race:
        D_8031B760[0].season->race = 0;
        D_8031B760[0].season->points[D_8031B760[0].season->round] = 0;
        BrChampionshipStart();
        BrFrontSetMenuFlag(sel = D_8028B940);
        for (i = 0; i < 2; i++) {
          BrCarModelStream(&D_8031B370[i], i, D_8028B944[D_8031B760[0].season->round].kinds[i], i);
          while (BrEntLoadModel(&D_8031B370[i])) {
          }
        }
        D_80315EE0 = 1;
      }
    } else if (D_80315EE0) {
      sel = sel;
      D_80315EE0 = 0;
    } else {
      D_8028B940 = sel;
      sel = sel;
      if (D_8026FF18 == 2 || D_8026FF18 == 0) {
        BrModeSet(func_8020D004);
      } else {
        BrModeSet(BrWeatherScreen);
      }
      entered = -1;
    }
    break;
  case 2:
    if (D_8026FF18 == 2 || D_8026FF18 == 0) {
      BrModeSet(BrMainMenu);
    } else {
      BrModeSet(BrPlayerSelectScreen);
    }
    entered = 0;
    break;
  }
  if (D_8026FF18 == 0 || D_8026FF18 == 1) {
    D_80271D1C[idx]->label = label;
  }
}
