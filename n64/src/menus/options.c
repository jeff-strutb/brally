/* options.c -- the options screen
 */
#include "tgr/common.h"

/* -- declarations -- */
int BrCpakCheck(int param_1,char param_2);
void BrTextHighlightOff(void);
void BrTextAlignCentre(void);
void BrTextSetFont(int param_1);
void BrTextPrint(int param_1,int param_2,int param_3);
extern int D_803163A0;
extern int D_803163A4;
extern int D_8036A8E0;
extern int D_8036A8F8;
/* -- end declarations -- */

/* WHAT IT DOES: Draw the options screen's help line and keep the Controller
 * Pak state current: when the pak state has to be re-read it re-initialises
 * the pak and clears the save flags if none is found. */
/* @implements 0x8021196C tgr BrOptionsDrawHelp */
void BrOptionsDrawHelp(int param_1)
{
  int iVar1;
  
  if (param_1 < 2) {
    BrTextSetFont(10);
    BrTextHighlightOff();
    BrTextAlignCentre();
    BrTextPrint("%wwPush up or down to adjust",0xa0,0x5a);
  }
  if (D_803163A0 == 0) {
    if (D_803163A4 != 0) {
      iVar1 = BrCpakCheck(3,0);
      if (iVar1 == 0) {
        D_8036A8E0 = 0;
        D_8036A8F8 = 0;
      }
      else {
        D_803163A4 = 0;
      }
    }
  }
  else {
    iVar1 = BrCpakCheck(2,0);
    if (iVar1 == 0) {
      D_8036A8E0 = 0;
      D_8036A8F8 = 0;
    }
    else {
      D_803163A0 = 0;
    }
  }
}
