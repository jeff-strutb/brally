/* playerselect.c -- the 1P/2P select screen
 */
#include "tgr/common.h"

/* -- declarations -- */
void func_80219A78(int param_1,int param_2,int param_3,int param_4);
void func_80223DE0(float param_1);
void BrTextHighlightOff(void);
void BrTextAlignCentre(void);
void BrTextSetFont(int param_1);
void BrTextPrint(int param_1,int param_2,int param_3);
extern int D_8028AAB0;
extern int D_8028AAB4;
extern float D_802A8050;
extern char *D_802723B8;
/* -- end declarations -- */

/* WHAT IT DOES: Draw one row of the 1P/2P select screen; when the row needs
 * a second controller that is not present it also draws the notice asking
 * for two controllers to be plugged in. */
/* @implements 0x802114E0 tgr BrPlayerSelectDrawRow */
void BrPlayerSelectDrawRow(int param_1)
{
  int iVar1;
  int iVar2;
  int iVar3;
  int iVar4;
  
  if ((*(unsigned int *)((&D_802723B8)[param_1] + 4) & 8) != 0) {
    iVar1 = D_8028AAB0 * 9;
    if (iVar1 < 0) {
      iVar1 = iVar1 + 0x1f;
    }
    iVar2 = D_8028AAB4 * 0x1b;
    iVar3 = D_8028AAB0 * 0xe;
    iVar4 = D_8028AAB4 * 4;
    if (iVar2 < 0) {
      iVar2 = iVar2 + 0x3f;
    }
    if (iVar3 < 0) {
      iVar3 = iVar3 + 0x1f;
    }
    if (iVar4 < 0) {
      iVar4 = iVar4 + 0xf;
    }
    func_80219A78(iVar1 >> 5,iVar2 >> 6,iVar3 >> 5,iVar4 >> 4);
    func_80223DE0(D_802A8050);
    func_80219A78(0,0,D_8028AAB0,D_8028AAB4);
    BrTextHighlightOff();
    BrTextAlignCentre();
    BrTextSetFont(0xf);
    iVar2 = D_8028AAB4 * 10;
    iVar1 = D_8028AAB0;
    if (D_8028AAB0 < 0) {
      iVar1 = D_8028AAB0 + 1;
    }
    if (iVar2 < 0) {
      iVar2 = iVar2 + 0xf;
    }
    BrTextPrint("%wwYou Need To Have",iVar1 >> 1,(iVar2 >> 4) + -0x1e);
    iVar2 = D_8028AAB4 * 10;
    iVar1 = D_8028AAB0;
    if (D_8028AAB0 < 0) {
      iVar1 = D_8028AAB0 + 1;
    }
    if (iVar2 < 0) {
      iVar2 = iVar2 + 0xf;
    }
    BrTextPrint("%wwTwo Controllers",iVar1 >> 1,(iVar2 >> 4) + -0xf);
    iVar2 = D_8028AAB4 * 10;
    iVar1 = D_8028AAB0;
    if (D_8028AAB0 < 0) {
      iVar1 = D_8028AAB0 + 1;
    }
    if (iVar2 < 0) {
      iVar2 = iVar2 + 0xf;
    }
    BrTextPrint("%wwPlugged in",iVar1 >> 1,iVar2 >> 4);
  }
}
