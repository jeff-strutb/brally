/* seasonmenu.c -- the season screen's lap-count setting
 */
#include "tgr/common.h"

/* -- declarations -- */
extern int D_802723D0;
extern unsigned char D_802A49C4;
extern int D_8028B94C;
extern char * D_8031C5BC;
extern int D_802723D4;
extern int D_802724B4;
extern unsigned char D_802A49CC;
void BrSeasonAdvanceRound(void);
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
extern int D_8028B950;
extern unsigned char D_8028B954;
extern int D_802A7784;
extern int D_802A7788;
extern unsigned char D_80307F00;
extern int D_80315EE0;
extern int D_80315EE8;
extern int D_8036A8E0;
extern int D_8036A8F8;
/* -- end declarations -- */

/* WHAT IT DOES: Raise the race length one step (to at most the eleventh
 * entry of the lap-count table) and set the number of laps from the table. */
/* @implements 0x80211840 tgr BrLapsMore */
void BrLapsMore(void)
{
  if (D_802723D0 < 10) {
    D_802723D0 = D_802723D0 + 1;
  }
  D_802A49C4 = (char)*(int *)(D_802723D0 * 4 + -0x7fd8db78);
}

/* WHAT IT DOES: Lower the race length one step (to at least the first entry
 * of the lap-count table) and set the number of laps from the table. */
/* @implements 0x8021187C tgr BrLapsFewer */
void BrLapsFewer(void)
{
  if (0 < D_802723D0) {
    D_802723D0 = D_802723D0 + -1;
  }
  D_802A49C4 = (char)*(int *)(D_802723D0 * 4 + -0x7fd8db78);
}

/* WHAT IT DOES: Move the season record on to its next round: past the last
 * race of a season it starts the next season from round one, and the points
 * for the new round start at zero. */
/* @implements 0x802089D0 tgr BrSeasonAdvanceRound */
void BrSeasonAdvanceRound(void)
{
  unsigned char bVar1;
  
  if (*D_8031C5BC < 0) {
    *D_8031C5BC = 0;
    goto LAB_802089f0;
  }
  bVar1 = *(unsigned char *)(D_8031C5BC + 1);
  do {
    if (bVar1 < 6) {
      if ((int)(unsigned int)*(unsigned char *)((int)D_8031C5BC + 5) <
          *(int *)(&D_8028B94C + (unsigned int)bVar1 * 0x1c)) {
        return;
      }
      *(char *)((int)D_8031C5BC + 5) = 0;
    }
    else {
LAB_802089f0:
      *(char *)(D_8031C5BC + 1) = 0;
      *(char *)((int)D_8031C5BC + 5) = 0;
    }
    *(short *)((int)D_8031C5BC + (unsigned int)*(unsigned char *)(D_8031C5BC + 1) * 2 + 0x1e) = 0;
    bVar1 = *(unsigned char *)(D_8031C5BC + 1);
  } while( 1 );
}

/* WHAT IT DOES: Set the number of laps and the difficulty from the current
 * lap-count and difficulty table entries. */
/* @implements 0x802118B4 tgr BrLapsApply */
void BrLapsApply(void)
{
  D_802A49C4 = (char)*(int *)(D_802723D0 * 4 + -0x7fd8db78);
  D_802A49CC = (char)(&D_802724B4)[D_802723D4];
}

/* WHAT IT DOES: Draw the help line at the foot of the season screen: the
 * lap-count hint in Arcade, and in a Championship how many points are still
 * needed to advance (or that the player already has enough), with the
 * points table. */
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
        BrSeasonAdvanceRound();
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
