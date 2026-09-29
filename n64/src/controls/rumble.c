/* rumble.c -- Rumble Pak output
 */
#include "tgr/common.h"

/* -- declarations -- */
int func_8020082C();
int BrModeIs(int param_1);
int func_80261F20();
int func_80262088(int param_1);
int func_80262370(int param_1,int *param_2,int param_3);
extern int D_802604FC;
extern int D_80260500;
extern int D_8026FF10;
extern int D_8026FF18;
extern int D_80272D48;
extern short D_802A4BE8;
extern int D_802A4BEC;
extern int D_802A4BF0;
extern short D_802A4BF4;
extern short D_802A4BF8;
extern int D_802A4BFC;
extern int D_802A4C00;
extern short D_802A4C04;
extern int D_802A4C08;
extern char D_8031B1E8;
extern int D_8031D7D4;
/* -- end declarations -- */

/* WHAT IT DOES: Run the Rumble Paks: re-detect them outside a race, and in
 * a race pulse each player's motor on and off at the rates the current
 * effect asks for, stopping it when the effect ends. */
/* @t4-pass 0x80260490 1 2026-09-26 compiles 17 best 173 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80260490 2 2026-09-26 compiles 17 best 173 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80260490 3 2026-09-26 compiles 17 best 173 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x80260490 tgr BrRumbleUpdate */
void BrRumbleUpdate(int arg0)
{
  short sVar1;
  int iVar2;
  int iVar3;
  unsigned short uVar4;
  unsigned short *puVar5;
  char *puVar6;
  int iVar7;
  short *psVar8;
  char *pcVar9;
  
  iVar2 = BrModeIs(func_8020082C);
  if ((((iVar2 == 0) || (D_8026FF18 == 5)) || (D_802A4C04 != 0)) || (D_8026FF10 != 0)) {
    iVar2 = 0;
    do {
      if (D_802A4BE8 != 0) {
        (&D_802A4BEC)[iVar2] = 0;
        iVar7 = iVar2 * 0x68 + -0x7fce5c08;
        (&D_8031B1E8)[iVar2] = 0;
        iVar3 = func_80262370(&D_80272D48,iVar7,iVar2);
        if (iVar3 == 0) {
          (&D_8031B1E8)[iVar2] = 1;
          func_80261F20(iVar7);
        }
      }
      iVar2 = iVar2 + 1;
    } while (iVar2 != 2);
  }
  if ((D_802A4BE8 != 0) && (D_802A4C04 == 0)) {
    pcVar9 = &D_8031B1E8;
    iVar2 = 0;
    if (D_8026FF10 == 0) {
      do {
        if (((*pcVar9 != '\0') && ((&D_802A4BEC)[iVar2] != 0)) &&
           (psVar8 = &D_802A4BFC + iVar2,
           *(int *)(*(int *)(&D_8031D7D4 + iVar2 * 0x2090) + 0x44) == 0)) {
          if (*psVar8 == 0) {
            puVar5 = &D_802A4BF0 + iVar2;
            if (*puVar5 == 0) {
              *psVar8 = (&D_802A4BF4)[iVar2];
              func_80262088(iVar2 * 0x68 + -0x7fce5c08);
              uVar4 = *puVar5;
            }
            else {
              *psVar8 = (&D_802A4BF8)[iVar2];
              func_80261F20();
              uVar4 = *puVar5;
            }
            *puVar5 = uVar4 ^ 1;
          }
          else {
            *psVar8 = *psVar8 + -1;
          }
          sVar1 = (&D_802A4C00)[iVar2];
          if (sVar1 == 0) {
            func_80261F20(iVar2 * 0x68 + -0x7fce5c08);
            (&D_802A4BF0)[iVar2] = 0;
            *psVar8 = 0;
            (&D_802A4BEC)[iVar2] = 0;
          }
          else {
            (&D_802A4C00)[iVar2] = sVar1 + -1;
          }
        }
        iVar2 = iVar2 + 1;
        pcVar9 = pcVar9 + 1;
      } while (iVar2 != 2);
    }
  }
  D_802A4C08 = D_802A4C08 + 1 & 0x3f;
  if (D_802A4C08 == 0) {
    puVar6 = &D_8031B1E8;
    iVar2 = 0;
    if (D_802A4BE8 != 0) {
      iVar3 = -0x7fce5c08;
      do {
        *puVar6 = 0;
        iVar7 = func_80262370(&D_80272D48,iVar3,iVar2);
        iVar2 = iVar2 + 1;
        if (iVar7 == 0) {
          *puVar6 = 1;
        }
        puVar6 = puVar6 + 1;
        iVar3 = iVar3 + 0x68;
      } while (iVar2 != 2);
    }
  }
}

/* WHAT IT DOES: Does nothing: an empty function the rumble code ends with,
 * called once from the game's startup. */
/* @implements 0x802607AC tgr BrStub802607AC */
void BrStub802607AC(void)
{
}
