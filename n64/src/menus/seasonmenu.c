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
