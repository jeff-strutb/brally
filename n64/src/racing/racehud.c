/* racehud.c -- the race's frame hook: extra layers drawn over the 3-D view
 */
#include "tgr/common.h"

/* -- declarations -- */
void func_8020037C(void);
void func_8022D7E0(int param_1,unsigned int param_2,unsigned int param_3,int param_4,int param_5);
void func_8023BF60(void);
extern int D_8026FF10;
float func_80224404();
extern int D_8028B304;
float func_8022576C(float param_1,float param_2);
extern float D_802A9EF4;
void func_8022F5DC(int param_1,int param_2,int param_3);
int func_80260DD4();
extern int D_8023876C;
extern int D_8023877C;
extern int D_80238784;
extern int D_80238798;
extern int D_802387A8;
extern int D_802387B0;
void func_8022F4F8(void);
void func_8022F520(void);
void func_8022F5D0(int param_1);
int func_80238714();
extern int D_8026FF08;
extern int D_8026FF18;
extern int D_8028AAEC;
extern int D_8028AAF0;
extern int D_8028AB0C;
extern int D_802AA060;
extern int D_802AA070;
extern int D_802AA080;
extern int D_802AA094;
extern int D_802AA0A4;
extern int D_802AA0B8;
extern int D_802AA0C8;
extern int D_802AA0D8;
extern int D_802AA0E8;
extern int D_8031B2CC;
void func_8022F4DC(void);
void func_8022F4EC(void);
void func_8022F514(void);
int func_8022F530();
int func_8022F720(unsigned char *param_1,int param_2);
extern int D_8028AAF4;
extern int D_802AA0F8;
extern int D_802AA0FC;
extern int D_802AA110;
extern int D_802AA114;
extern int D_802AA118;
extern int D_802AA11C;
extern int D_802AA120;
extern int D_8031B2C8;
extern int D_8031B2D4;
void func_8022F504(void);
extern int D_80238EBC;
extern int D_80238F10;
extern int D_8031B2D0;
/* -- end declarations -- */

/* WHAT IT DOES: The race's frame hook: unless the race has switched the
 * layers off, draws the extra model layer and the second overlay pass, each
 * after setting the fill colour it needs. */
/* @implements 0x802003E4 tgr BrRaceDrawLayers */
void BrRaceDrawLayers(void)
{
  if (D_8026FF10 == 0) {
    func_8022D7E0(0,0x80,0x80,0xf0,0xff);
    func_8020037C();
    func_8022D7E0(0,0,0,0xc0,0xff);
    func_8023BF60();
    func_8022D7E0(0,0,0x82,0,0xff);
  }
}

/* WHAT IT DOES: Does nothing. An empty function the retail build kept among
 * the race display code. */
/* @implements 0x8023870C tgr BrStub8023870C */
void BrStub8023870C(void)
{
}

/* WHAT IT DOES: Watch whether a car is driving the wrong way: after half a
 * second of facing backwards the WRONG WAY message starts flashing on that
 * player's screen, and it is taken down as soon as the car turns round. */
/* @t4-pass 0x8021F1F0 1 2026-09-26 compiles 17 best 62 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8021F1F0 2 2026-09-26 compiles 17 best 62 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8021F1F0 3 2026-09-26 compiles 17 best 62 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x8021F1F0 tgr BrWrongWayCheck */
void BrWrongWayCheck(int param_1)
{
  unsigned int uVar1;
  char *pcVar2;
  float fVar3;
  
  if ((*(int *)0x80025C70) != 0) {
    if (*(int *)(param_1 + 0xf78) < D_8028B304) {
      if (*(int *)(param_1 + 0xf4c) == 0) {
        fVar3 = (float)func_80224404(param_1 + 0xf64);
        if (fVar3 < 0.0f) {
          uVar1 = *(int *)(param_1 + 0x206c) + 1;
          *(unsigned int *)(param_1 + 0x206c) = uVar1;
          if ((int)uVar1 < 0x20) {
            pcVar2 = *(char **)(param_1 + 0xfb0);
          }
          else {
            if ((uVar1 & 0x10) == 0x10) {
              if (*(int *)(param_1 + 0xfb0) != 0) {
                return;
              }
              *(char **)(param_1 + 0xfb0) = "%yyWRONG WAY";
              *(int *)(param_1 + 0xfb8) = 0;
              *(int *)(param_1 + 0xfb4) = 0x3e800000;
              return;
            }
            pcVar2 = *(char **)(param_1 + 0xfb0);
          }
          if (pcVar2 == "%yyWRONG WAY") {
            *(int *)(param_1 + 0xfb8) = 0;
            *(int *)(param_1 + 0xfb0) = 0;
          }
        }
        else {
          *(int *)(param_1 + 0x206c) = 0;
        }
      }
      else {
        *(int *)(param_1 + 0x206c) = 0;
      }
    }
    else {
      *(int *)(param_1 + 0x206c) = 0;
    }
  }
}

/* WHAT IT DOES: Draw the direction arrow for the next turn: picks one of
 * the arrow shapes from the angle of the upcoming bend and draws it tinted
 * for that player. */
/* @t4-pass 0x80233880 1 2026-09-26 compiles 17 best 348 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80233880 2 2026-09-26 compiles 17 best 348 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80233880 3 2026-09-26 compiles 17 best 348 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x80233880 tgr BrHudArrowDraw */
void BrHudArrowDraw(int param_1,float *param_2,short param_3)
{
  short sVar1;
  short *psVar2;
  unsigned int uVar3;
  int iVar4;
  unsigned int *puVar5;
  short sVar7;
  int iVar6;
  unsigned int uVar8;
  int iVar9;
  int iVar10;
  int iVar11;
  int iVar12;
  int iVar13;
  int iVar14;
  int iVar15;
  unsigned int uVar16;
  int iVar17;
  float fVar18;
  int iVar19;
  
  fVar18 = (float)func_8022576C(*param_2,param_2[1]);
  iVar9 = 0x3fff;
  iVar10 = 0xff;
  iVar11 = 0x3fff;
  iVar12 = -0x3fff;
  iVar14 = 0;
  iVar19 = (int)((fVar18 * 180.0f) / D_802A9EF4);
  if ((iVar19 < 0x14) || (0x153 < iVar19)) {
    iVar19 = 0;
  }
  else {
    iVar9 = 0x3fff;
    if (iVar19 < 0x32) {
      iVar10 = 0x80;
      iVar11 = 0x3fff;
      iVar12 = 0x40;
      iVar19 = 1;
    }
    else {
      iVar9 = 0x3fff;
      if (iVar19 < 0x82) {
        iVar10 = -0x3fff;
        iVar11 = 0x3fff;
        iVar12 = 0x40;
        iVar19 = 2;
      }
      else {
        iVar9 = -0x80;
        if (iVar19 < 0xa0) {
          iVar10 = -0x3fff;
          iVar11 = 0x3fff;
          iVar12 = 0x40;
          iVar19 = 3;
        }
        else {
          iVar9 = -0xff;
          if (iVar19 < 200) {
            iVar10 = -0x3fff;
            iVar11 = 0x3fff;
            iVar12 = -0x3fff;
            iVar19 = 4;
          }
          else {
            iVar9 = -0x80;
            if (iVar19 < 0xe6) {
              iVar10 = -0x3fff;
              iVar11 = -0x40;
              iVar12 = -0x3fff;
              iVar19 = 5;
            }
            else {
              iVar9 = 0x3fff;
              if (iVar19 < 0x136) {
                iVar9 = 0x3fff;
                iVar10 = -0x3fff;
                iVar11 = -0x40;
                iVar12 = -0x3fff;
                iVar19 = 6;
              }
              else {
                iVar10 = 0x80;
                iVar11 = -0x40;
                iVar12 = -0x3fff;
                iVar19 = 7;
              }
            }
          }
        }
      }
    }
  }
  iVar19 = param_1 + iVar19 * 2;
  sVar7 = *(short *)(iVar19 + 0x207c);
  if (sVar7 < 0x100) {
    sVar1 = (short)((unsigned int)((int)param_3 << 0x12) >> 0x10);
    *(short *)(iVar19 + 0x207c) = sVar7 + sVar1;
    *(unsigned short *)(param_1 + 0x208c) = (*(short *)(param_1 + 0x208c) + 5U & 7) - 4;
    iVar19 = (int)*(short *)(param_1 + 0x208c);
    iVar17 = (int)sVar1;
    if (iVar19 < 0) {
      iVar19 = iVar19 + 1;
    }
    fVar18 = param_2[1];
    sVar7 = sVar1 >> 1;
    if (fVar18 <= 1.0f) {
      if (0.0f < fVar18) {
        iVar17 = (int)sVar7;
      }
      else {
        iVar17 = (int)-sVar7;
        if (fVar18 < -1.0f) {
          iVar17 = (int)-sVar1;
        }
      }
    }
    fVar18 = param_2[2];
    if (1.0f < fVar18) {
      iVar4 = (int)(sVar1 >> 2);
    }
    else if (0.0f < fVar18) {
      iVar4 = (int)(sVar1 >> 3);
    }
    else {
      iVar4 = (int)(short)-(sVar1 >> 3);
      if (fVar18 < -1.0f) {
        iVar4 = (int)(short)-(sVar1 >> 2);
      }
    }
    fVar18 = *param_2;
    iVar15 = (int)sVar1;
    if (fVar18 <= 1.25f) {
      if (0.0f < fVar18) {
        iVar15 = (int)sVar7;
      }
      else {
        iVar15 = (int)-sVar1;
        if (fVar18 < -1.25f) {
          iVar15 = (int)-sVar1;
          iVar4 = (iVar4 << 0x11) >> 0x10;
        }
      }
    }
    iVar13 = 0;
LAB_80233be0:
    do {
      if ((iVar13 != 9) &&
         (puVar5 = *(unsigned int **)(*(int *)(param_1 + 0x2078) + iVar14 * 0x28 + iVar13 * 4 + 0x18),
         puVar5 != (unsigned int *)0x0)) {
LAB_80233c10:
        do {
          uVar3 = *puVar5;
          while( 1 ) {
            if (uVar3 >> 0x18 != 4) break;
            uVar3 = uVar3 >> 10 & 0x3f;
            psVar2 = (short *)puVar5[1];
            puVar5 = puVar5 + 2;
            if (uVar3 == 0) goto LAB_80233c10;
            do {
              uVar3 = uVar3 - 1;
              sVar7 = *psVar2;
              uVar16 = (sVar7 + iVar19) * 0x10000 >> 0x10;
              if ((((iVar10 < (int)uVar16) && ((int)uVar16 < iVar9)) &&
                  (uVar8 = (psVar2[1] + iVar19) * 0x10000 >> 0x10, iVar12 < (int)uVar8)) &&
                 ((((int)uVar8 < iVar11 &&
                   (iVar6 = (psVar2[2] + iVar19) * 0x10000 >> 0x10, -0x30 < iVar6)) &&
                  (iVar6 < 0xe0)))) {
                if ((uVar8 & 0x80) == 0) {
                  *psVar2 = sVar7 + (short)((int)(iVar15 * ((uVar8 & 0xf) - 0xc)) >> 5);
                }
                else {
                  *psVar2 = sVar7 + (short)((int)(iVar15 * (4 - (uVar8 & 0xf))) >> 5);
                }
                if ((uVar16 & 0x80) == 0) {
                  psVar2[1] = psVar2[1] + (short)((int)(iVar17 * ((uVar16 & 0xf) - 0xc)) >> 5);
                }
                else {
                  psVar2[1] = psVar2[1] + (short)((int)(iVar17 * (4 - (uVar16 & 0xf))) >> 5);
                }
                uVar16 = (int)((uVar16 + uVar8) * 0x10000) >> 0x10;
                if ((uVar16 & 0x80) == 0) {
                  psVar2[2] = psVar2[2] + (short)((int)(iVar4 * ((uVar16 & 0xf) - 8)) >> 6);
                }
                else {
                  psVar2[2] = psVar2[2] + (short)((int)(iVar4 * (8 - (uVar16 & 0xf))) >> 6);
                }
              }
              psVar2 = psVar2 + 8;
            } while (uVar3 != 0);
            uVar3 = *puVar5;
          }
          if (uVar3 >> 0x18 == 0xb8) break;
          puVar5 = puVar5 + 2;
        } while( 1 );
      }
      iVar13 = iVar13 + 1;
    } while (iVar13 != 10);
    iVar14 = iVar14 + 1;
    if (iVar14 != 3) {
      iVar13 = 0;
      goto LAB_80233be0;
    }
  }
}

/* WHAT IT DOES: Draw a race time as minutes, seconds and hundredths under
 * its label. */
/* @t4-pass 0x80238714 1 2026-09-26 compiles 17 best 70 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80238714 2 2026-09-26 compiles 16 best 70 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80238714 3 2026-09-26 compiles 13 best 70 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x80238714 tgr BrHudTimeDraw */
void BrHudTimeDraw(int param_1,int param_2,float param_3,int param_4,int param_5
                 )
{
  int iVar1;
  char auStack_2c [44];
  
  iVar1 = (int)(param_3 * 100.0f) / 100;
  func_80260DD4(auStack_2c,"%s%d'%02d\"%02d",param_2,iVar1 / 0x3c,iVar1 % 0x3c,
               (int)(param_3 * 100.0f) % 100);
  func_8022F5DC(auStack_2c,param_4,param_5 + 0xf);
  func_8022F5DC(param_1,param_4,param_5);
}

/* WHAT IT DOES: Draw the race times panel: the total time and, per lap, the
 * lap times, placed for one or two players. */
/* @t4-pass 0x8023880C 1 2026-09-26 compiles 16 best 143 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8023880C 2 2026-09-26 compiles 17 best 143 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8023880C 3 2026-09-26 compiles 17 best 143 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x8023880C tgr BrHudTimesDraw */
void BrHudTimesDraw(void)
{
  int iVar1;
  int iVar2;
  
  if (D_8028AB0C == 1) {
    iVar1 = 0x1e;
  }
  else {
    iVar1 = 0;
  }
  iVar2 = *(int *)(&D_8031B2CC + D_8028AAEC * 0x14) + 0x14;
  func_8022F4F8();
  func_8022F520();
  func_8022F5D0(0xf);
  if (D_8026FF18 != 0) {
    if (D_8026FF18 == 1) {
      if (D_8028AB0C == 1) {
        func_80238714("%15TOTAL TIME",&D_802AA094,*(int *)(D_8028AAF0 + 4000),
                     0x128,iVar2);
      }
      if (D_8028B304 <= *(int *)(D_8028AAF0 + 0xf78)) {
        func_80238714("%15BEST LAP",&D_802AA0A4,*(int *)(D_8028AAF0 + 0xf98),
                     0x128,iVar2 + iVar1);
        return;
      }
      if (D_8026FF08 == 1) {
        func_80238714("%15TIME LEFT",&D_802AA0B8,*(int *)(D_8028AAF0 + 0xfa4),
                     0x128,iVar2 + iVar1);
        return;
      }
      func_80238714("%15LAP TIME",&D_802AA0C8,*(int *)(D_8028AAF0 + 0xf80),0x128,
                   iVar2 + iVar1);
      return;
    }
    if (D_8026FF18 != 2) {
      if (D_8026FF18 != 3) {
        return;
      }
      if (D_8028AB0C == 1) {
        func_80238714("%15BEST LAP",&D_802AA0D8,*(int *)(D_8028AAF0 + 0xf98),
                     0x128,iVar2);
      }
      func_80238714("%15LAP TIME",&D_802AA0E8,*(int *)(D_8028AAF0 + 0xf80),0x128,
                   iVar2 + iVar1);
      return;
    }
  }
  if (D_8028AB0C == 1) {
    func_80238714("%15TOTAL TIME",&D_802AA060,*(int *)(D_8028AAF0 + 4000),0x128,
                 iVar2);
  }
  if (*(int *)(D_8028AAF0 + 0xf78) < D_8028B304) {
    func_80238714("%15LAP TIME",&D_802AA080,*(int *)(D_8028AAF0 + 0xf80),0x128,
                 iVar2 + iVar1);
  }
  else {
    func_80238714("%15BEST LAP",&D_802AA070,*(int *)(D_8028AAF0 + 0xf98),0x128,
                 iVar2 + iVar1);
  }
}

/* WHAT IT DOES: Draw the lap counter (LAP n/m) for the player being shown,
 * while the race is on. */
/* @t4-pass 0x80238AB8 1 2026-09-26 compiles 17 best 194 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80238AB8 2 2026-09-26 compiles 17 best 194 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80238AB8 3 2026-09-26 compiles 17 best 194 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x80238AB8 tgr BrHudLapDraw */
void BrHudLapDraw(void)
{
  int iVar1;
  int iVar2;
  char *puVar3;
  int iVar4;
  int iVar5;
  int iVar6;
  char *local_18;
  char auStack_14 [20];
  
  iVar1 = D_8031B2C8;
  if (D_8026FF18 != 3) {
    iVar5 = D_8031B2C8 + 0x10;
    if (D_8028AAF4 != D_8028AAF0 + 0x1e78) {
      iVar2 = *(int *)(D_8028AAF0 + 0xf78);
      if ((iVar2 < D_8028B304) || (D_8028AB0C == 1)) {
        iVar4 = *(int *)(&D_8031B2CC + D_8028AAEC * 0x14);
        if (iVar2 < D_8028B304) {
          if (D_8028AB0C == 2) {
            puVar3 = &D_802AA0F8;
          }
          else {
            puVar3 = &D_802AA0FC;
          }
          func_80260DD4(auStack_14,"%%y1%s%d/%d",puVar3,iVar2 + 1,D_8028B304);
        }
        else {
          func_80260DD4(auStack_14,"FINISHED");
        }
        func_8022F4F8();
        func_8022F514();
        func_8022F5D0(0xf);
        func_8022F5DC(auStack_14,iVar5,iVar4 + 0x14);
      }
    }
    iVar4 = iVar1 + 0xe;
    iVar6 = *(int *)(&D_8031B2D4 + D_8028AAEC * 0x14) +
            *(int *)(&D_8031B2CC + D_8028AAEC * 0x14);
    func_8022F4DC();
    func_8022F514();
    func_8022F530(0xff,0xf0,0x7d,0xff,0x78,0);
    func_80260DD4(auStack_14,&D_802AA110,*(int *)(D_8028AAF0 + 0xfac) + 1);
    iVar5 = 0;
    iVar2 = *(int *)(D_8028AAF0 + 0xfac);
    if (iVar2 == 0) {
      local_18 = &D_802AA114;
      iVar5 = -3;
    }
    else if (iVar2 == 1) {
      local_18 = &D_802AA118;
      iVar5 = 1;
    }
    else if (iVar2 == 2) {
      local_18 = &D_802AA11C;
    }
    else {
      local_18 = &D_802AA120;
      iVar5 = 1;
    }
    if (D_8028AB0C == 1) {
      func_8022F5D0(0x28);
      iVar2 = func_8022F720(auStack_14,0x28);
      func_8022F5DC(auStack_14,iVar1 + 0xd,iVar6 + -0xd);
      func_8022F5D0(0x14);
      func_8022F5DC(local_18,iVar4 + iVar5 + iVar2 + 3,iVar6 + -0x1b);
    }
    else {
      func_8022F5D0(0x1a);
      iVar1 = func_8022F720(auStack_14,0x1a);
      func_8022F5DC(auStack_14,iVar4,iVar6 + -0xc);
      func_8022F5D0(0xd);
      func_8022F5DC(local_18,iVar4 + (iVar5 << 1) / 3 + iVar1 + 3,iVar6 + -0x16);
    }
    func_8022F4EC();
  }
}

/* WHAT IT DOES: Draw the player's race position unless the display is
 * switched off, placed for one or two players. */
/* @t4-pass 0x80238DD4 1 2026-09-26 compiles 17 best 75 moved 2  (n64/tools/n64permute.py) */
/* @t4-pass 0x80238DD4 2 2026-09-26 compiles 17 best 76 moved 1  (n64/tools/n64permute.py) */
/* @t4-pass 0x80238DD4 3 2026-09-26 compiles 17 best 76 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x80238DD4 tgr BrHudPositionDraw */
void BrHudPositionDraw(void)
{
  int uVar1;
  int iVar3;
  int iVar2;
  int iVar4;
  
  if (D_8026FF10 == 0) {
    if (D_8028AB0C == 1) {
      iVar2 = 0x1e;
      uVar1 = 0x14;
    }
    else {
      iVar2 = 0x14;
      uVar1 = 0xf;
    }
    iVar4 = D_8028AAEC * 0x14;
    iVar3 = *(int *)(&D_8031B2D0 + iVar4);
    if (iVar3 < 0) {
      iVar3 = iVar3 + 1;
    }
    iVar3 = (iVar3 >> 1) + *(int *)(&D_8031B2C8 + iVar4);
    iVar4 = *(int *)(&D_8031B2D4 + iVar4) / 3 + *(int *)(&D_8031B2CC + iVar4);
    func_8022F504();
    if (*(int *)(D_8028AAF0 + 0xfb0) == 0) {
      if (*(int *)(D_8028AAF0 + 0xfb8) != 0) {
        func_8022F5D0(uVar1);
        func_8022F5DC(*(int *)(D_8028AAF0 + 0xfb8),iVar3,(iVar2 * 3 >> 4) + iVar4);
      }
    }
    else {
      func_8022F5D0(iVar2);
      func_8022F5DC(*(int *)(0xfb0 + D_8028AAF0),iVar3,(iVar2 >> 2) + iVar4);
    }
  }
}
