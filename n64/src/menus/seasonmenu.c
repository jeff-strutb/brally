/* seasonmenu.c -- the season screen: round progression and its help line
 */
#include "tgr/common.h"
#include "tgr/car.h"

/* -- declarations -- */
extern int D_802723D0;
extern int D_8028B94C;
extern char * D_8031C5BC;
void BrSeasonValidate(void);
void BrFrontSetMenuFlag(int param_1);
void BrChampionshipStart(void);
void BrTimeAttackStart(void);
int BrCpakCheck(int param_1,char param_2);
void func_80220474(int param_1,int param_2,int param_3,int param_4);
int BrEntLoadModel(int param_1);
void BrTextHighlightOff(void);
void BrTextAlignCentre(void);
void BrTextSetFont(int param_1);
void BrTextPrint(int param_1,int param_2,int param_3);
int sprintf();
extern int D_8026FF18;
extern int D_80271D58;
extern int D_8028AE04;
extern int D_8028B940;
extern char D_8028B950;
extern unsigned char D_8028B954;
extern int D_802A7784;
extern int D_802A7788;
extern unsigned char D_80307F00;
extern int D_80315EE0;
extern int D_80315EE8;
extern int D_8036A8E0;
extern int D_8036A8F8;
extern float D_80271D5C;                /* the current race's blink clock */
extern float D_8028AAD8;                /* the frame time */
extern int D_8028AAB0;                  /* the screen width */
extern int D_8028AAB4;                  /* and height */
extern char D_80315F38[];               /* a text line */
extern char **D_80271D1C[];             /* the tracks (their names first) */
extern char **D_802722A4[];             /* the weathers (their names first) */
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
/* -- end declarations -- */

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
/* @implements 0x80208A58 tgr BrSeasonDrawHelp */
void BrSeasonDrawHelp(int param_1)
{
  int iVar1;
  char *puVar2;
  int iVar3;
  int iVar4;
  
  if (D_8026FF18 == 1) {
    BrTextSetFont(10);
    BrTextHighlightOff();
    BrTextAlignCentre();
    BrTextPrint("%wwPush up or down to adjust number of laps",0xa0,0x5a);
  }
  if ((D_8026FF18 == 0) && (param_1 < D_8028AE04)) {
    BrTextSetFont(10);
    BrTextHighlightOff();
    BrTextAlignCentre();
    iVar3 = *(int *)(&D_8028B950 + (unsigned int)*(unsigned char *)(((int)D_8031C5BC) + 4) * 0x1c) -
            (unsigned int)*(unsigned short *)(((int)D_8031C5BC) + (unsigned int)*(unsigned char *)(((int)D_8031C5BC) + 4) * 2 + 0x1e);
    if (iVar3 < 1) {
      sprintf((&D_80315EE8),"%%wwYou have enough points to advance to next season");
    }
    else {
      BrTextPrint("%wwFirst=9, Second=6, Third=5, Fourth=3, Fifth=2, Sixth=1",0xa0,100);
      if (iVar3 == 1) {
        puVar2 = &D_802A7784;
      }
      else {
        puVar2 = &D_802A7788;
      }
      sprintf((&D_80315EE8),"%%ww%d point%s needed to advance to next season",iVar3,puVar2);
    }
    BrTextPrint((&D_80315EE8),0xa0,0x5a);
  }
  if (D_80271D58 != 0) {
    iVar3 = BrCpakCheck(D_8026FF18 == 2,0);
    if (iVar3 == 0) {
      D_8036A8E0 = 0;
      D_8036A8F8 = 0;
    }
    else {
      D_80271D58 = 0;
      if (D_8026FF18 == 0) {
        BrSeasonValidate();
        BrChampionshipStart();
        iVar3 = -0x7fce4c90;
        iVar4 = 0;
        do {
          func_80220474(iVar3,iVar4,
                       (&D_8028B954)[(unsigned int)*(unsigned char *)(((int)D_8031C5BC) + 4) * 0x1c + iVar4],iVar4);
          iVar1 = BrEntLoadModel(iVar3);
          while (iVar1 != 0) {
            iVar1 = BrEntLoadModel(iVar3);
          }
          iVar4 = iVar4 + 1;
          iVar3 = iVar3 + 0x28;
        } while (iVar4 != 2);
        D_80315EE0 = 1;
      }
      else if (D_8026FF18 == 2) {
        BrTimeAttackStart();
        if ((1 << (D_80307F00 & 0x1f) & (unsigned int)*(unsigned short *)(((int)D_8031C5BC) + 0xce)) != 0) {
          D_8028B940 = (unsigned int)D_80307F00;
        }
      }
      BrFrontSetMenuFlag(D_8028B940);
    }
  }
}

/* WHAT IT DOES: Draw the season screen: the season's name, the round's
 * races (track and weather, mirrored on a mirror season; the one to run
 * next blinks yellow) with the player's race and lap records for each track,
 * and the cars on offer, with the two car views set up side by side.
 * RESIDUE (454): the ROM keeps the season state in s4 (state & 1 hoisted to
 * fp per loop), the round in s0 and the text buffer address in s2, spilling
 * the race and the round pointer (0x84, 0x70) in a 0xB0 frame; ours spills
 * the state and reloads the race count each pass.  The blink clock's address
 * is rematerialised per access in the ROM.  Part of the count is the TU's
 * later functions (their strings and .lit4 floats precede and follow ours). */
/* @implements 0x80208CF0 tgr BrSeasonDraw */
void BrSeasonDraw(void)
{
  int race;
  int round;
  unsigned int state;
  BrRound *r;
  int i;
  int k;
  int off;
  int y;
  float t;

  D_80271D5C = t = D_80271D5C + D_8028AAD8;
  while (t > 0.75f) {
    t -= 0.75f;
  }
  D_80271D5C = t;
  y = 95;
  race = D_8031B760[0].season->race;
  round = D_8031B760[0].season->round;
  state = D_8031B760[0].season->state;
  BrTextHighlightOff();
  BrTextAlignCentre();
  BrTextSetFont(20);
  r = &D_8028B944[round];
  sprintf(D_80315F38, "%%ry%s:", (char *)r->x4);
  BrTextPrint((int)D_80315F38, D_8028AAB0 / 2, D_8028AAB4 * 20 / 64);
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
    if (i == race && D_80271D5C < 0.4f) {
      BrTextSetColours(0xFF, 0xF0, 0x50, 0xFF, 0xF0, 0x50);
    } else {
      BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);
    }
    sprintf(D_80315F38, "Race %d", i + 1);
    BrTextPrint((int)D_80315F38, D_8028AAB0 * 4 / 32 - off, y);
    sprintf(D_80315F38, "%s/%s", *D_80271D1C[k], *D_802722A4[D_8028B944[round].races[i][1]]);
    BrTextPrint((int)D_80315F38, D_8028AAB0 * 8 / 32 - off + 4, y);
    BrTextAlignRight();
    if (D_8031B760[0].season->xe8[k] != 0) {
      BrTimeFormat(D_80315F38, D_8031B760[0].season->xe8[k]);
    } else {
      sprintf(D_80315F38, "--    ");
    }
    BrTextPrint((int)D_80315F38, D_8028AAB0 * 23 / 32 + off - 1, y);
    if (D_8031B760[0].season->x8c[k] != 0) {
      BrTimeFormat(D_80315F38, D_8031B760[0].season->x8c[k]);
    } else {
      sprintf(D_80315F38, "--    ");
    }
    BrTextPrint((int)D_80315F38, D_8028AAB0 * 28 / 32 + off, y);
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
