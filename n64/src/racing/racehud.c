/* racehud.c -- the race's frame hook: extra layers drawn over the 3-D view
 */
#include "tgr/common.h"
#include "tgr/car.h"
#include "tgr/gbi.h"

/* -- declarations -- */
void func_8020037C(void);
void func_8022D7E0(int param_1,unsigned int param_2,unsigned int param_3,int param_4,int param_5);
void func_8023BF60(void);
extern int D_8026FF10;
float func_80224404();
extern int D_8028B304;                /* laps in the race */
extern char D_8028B308[];             /* "%yyWRONG WAY" */
float func_8022576C(float param_1,float param_2);
void func_8022F5DC(unsigned char *s, int x, int y);
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
void BrHudTimeDraw(char *label, char *prefix, float t, int x, int y);
extern int D_8026FF08;
extern int D_8026FF18;
extern int D_8028AAEC;                /* the view being drawn */
extern BrCar *D_8028AAF0;             /* the car of the view being drawn */
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
void func_8022F4DC(void);
void func_8022F4EC(void);
void func_8022F514(void);
int func_8022F530();
int func_8022F720(unsigned char *param_1,int param_2);
extern BrCarCam *D_8028AAF4;           /* the camera being drawn from */
extern char D_802AA0F8[];
extern char D_802AA0FC[];
extern char D_802AA110[];
extern char D_802AA114[];
extern char D_802AA118[];
extern char D_802AA11C[];
extern char D_802AA120[];
typedef struct BrViewRect { int x; int y; int w; int h; int car; } BrViewRect;
extern BrViewRect D_8031B2C8[2];        /* the players' views */
void func_8022F504(void);
extern int D_80238EBC;
extern int D_80238F10;
extern int D_80025C70;
void BrScissorSet(int x0, int y0, int x1, int y1);
void func_80237980(void);
extern int D_802723D8;                 /* speed in mph (else kph) */
extern char D_80361C30[];              /* the speed text */
typedef struct BrHudPanel {     /* one per view, 0x14 bytes */
  short x0;
  unsigned short h;             /* 0x02  height of the view's top panel */
  char pad04[0x14 - 0x04];
} BrHudPanel;
extern BrHudPanel D_8028C7B4[2];
extern Gfx *D_8028A858;
extern int D_8028A850;
extern int D_8028A898;                 /* the texture filter mode */
void BrTexSizeBits(int n, int *shift, int *mask);
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
void BrWrongWayCheck(BrCar *car)
{
  if (D_80025C70 != 0) {
    if (car->laps < D_8028B304 && car->xf4c == 0 && func_80224404(&car->xf64) < 0.0) {
      car->wrongWay++;
      if (car->wrongWay >= 32 && (car->wrongWay & 0x10) == 0x10) {
        if (car->msgA == 0) {
          car->msgA = (int)D_8028B308;
          car->msgB = 0;
          car->msgATime = 0.25f;
        }
      } else if (car->msgA == (int)D_8028B308) {
        car->msgB = 0;
        car->msgA = 0;
      }
    } else {
      car->wrongWay = 0;
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
  iVar19 = (int)((fVar18 * 180.0f) / 3.1415927f);
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
 * its label.  No named locals: the ROM's frame holds only the buffer, and
 * the seconds and minutes are the one expression CSE'd. */
/* @t4-pass 0x80238714 1 2026-09-26 compiles 17 best 70 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80238714 2 2026-09-26 compiles 16 best 70 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80238714 3 2026-09-26 compiles 13 best 70 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x80238714 tgr BrHudTimeDraw */
void BrHudTimeDraw(char *label, char *prefix, float t, int x, int y)
{
  char buf[44];

  func_80260DD4(buf, "%s%d'%02d\"%02d", prefix,
                ((int)(t * 100.0f) / 100) / 60, ((int)(t * 100.0f) / 100) - ((int)(t * 100.0f) / 100) / 60 * 60, (int)(t * 100.0f) - ((int)(t * 100.0f) / 100) * 100);
  func_8022F5DC(buf, x, y + 15);
  func_8022F5DC(label, x, y);
}

/* WHAT IT DOES: Draw the race times panel by race mode: the total time
 * (single-player layout only), then the lap time, best lap or time left,
 * placed below the top of the player's view. */
/* @t4-pass 0x8023880C 1 2026-09-26 compiles 16 best 143 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8023880C 2 2026-09-26 compiles 17 best 143 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8023880C 3 2026-09-26 compiles 17 best 143 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x8023880C tgr BrHudTimesDraw */
void BrHudTimesDraw(void)
{
  int y;
  BrCar *car;                   /* declared, never used: its slot is in the frame */
  int dy;

  if (D_8028AB0C == 1) {
    dy = 30;
  } else {
    dy = 0;
  }
  y = D_8031B2C8[D_8028AAEC].y + 20;
  func_8022F4F8();
  func_8022F520();
  func_8022F5D0(15);
  switch (D_8026FF18) {
  case 0:
  case 2:
    if (D_8028AB0C == 1) {
      BrHudTimeDraw("%15TOTAL TIME", (char *)&D_802AA060, D_8028AAF0->lapTime, 296, y);
    }
    if (D_8028AAF0->laps >= D_8028B304) {
      BrHudTimeDraw("%15BEST LAP", (char *)&D_802AA070, D_8028AAF0->xf98, 296, y + dy);
    } else {
      BrHudTimeDraw("%15LAP TIME", (char *)&D_802AA080, D_8028AAF0->raceTime, 296, y + dy);
    }
    break;
  case 1:
    if (D_8028AB0C == 1) {
      BrHudTimeDraw("%15TOTAL TIME", (char *)&D_802AA094, D_8028AAF0->lapTime, 296, y);
    }
    if (D_8028AAF0->laps >= D_8028B304) {
      BrHudTimeDraw("%15BEST LAP", (char *)&D_802AA0A4, D_8028AAF0->xf98, 296, y + dy);
    } else if (D_8026FF08 == 1) {
      BrHudTimeDraw("%15TIME LEFT", (char *)&D_802AA0B8, D_8028AAF0->xfa4, 296, y + dy);
    } else {
      BrHudTimeDraw("%15LAP TIME", (char *)&D_802AA0C8, D_8028AAF0->raceTime, 296, y + dy);
    }
    break;
  case 3:
    if (D_8028AB0C == 1) {
      BrHudTimeDraw("%15BEST LAP", (char *)&D_802AA0D8, D_8028AAF0->xf98, 296, y);
    }
    BrHudTimeDraw("%15LAP TIME", (char *)&D_802AA0E8, D_8028AAF0->raceTime, 296, y + dy);
    break;
  }
}

/* WHAT IT DOES: Draw the lap counter and the race position.  Outside race
 * mode 3: unless the view is the car's third camera, the lap counter
 * (n/m, or FINISHED once done; shown finished only in the single-player
 * layout) at the top left; then the position number at the bottom left
 * with its st/nd/rd/th suffix after it, sized for the layout. */
/* @t4-pass 0x80238AB8 1 2026-09-26 compiles 17 best 194 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80238AB8 2 2026-09-26 compiles 17 best 194 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80238AB8 3 2026-09-26 compiles 17 best 194 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x80238AB8 tgr BrHudLapDraw */
void BrHudLapDraw(void)
{
  char buf[20];
  char *suffix;
  int x;
  int y;
  int adj;
  int w;

  if (D_8026FF18 != 3) {
    x = D_8031B2C8[0].x + 16;
    if (D_8028AAF4 != &D_8028AAF0->cams[2]) {
      if (D_8028AAF0->laps < D_8028B304 || D_8028AB0C == 1) {
        y = D_8031B2C8[D_8028AAEC].y + 5;
        if (D_8028AAF0->laps < D_8028B304) {
          func_80260DD4(buf, "%%y1%s%d/%d", D_8028AB0C == 2 ? D_802AA0F8 : D_802AA0FC,
                  D_8028AAF0->laps + 1, D_8028B304);
        } else {
          func_80260DD4(buf, "FINISHED");
        }
        y += 15;
        func_8022F4F8();
        func_8022F514();
        func_8022F5D0(15);
        func_8022F5DC(buf, x, y);
      }
    }
    y = D_8031B2C8[D_8028AAEC].y + D_8031B2C8[D_8028AAEC].h - 12;
    x -= 2;
    func_8022F4DC();
    func_8022F514();
    func_8022F530(0xff, 0xf0, 0x7d, 0xff, 0x78, 0);
    func_80260DD4(buf, D_802AA110, D_8028AAF0->xfac + 1);
    adj = 0;
    switch (D_8028AAF0->xfac) {
    case 0:
      suffix = D_802AA114;
      adj = -3;
      break;
    case 1:
      suffix = D_802AA118;
      adj = 1;
      break;
    case 2:
      suffix = D_802AA11C;
      break;
    default:
      suffix = D_802AA120;
      adj = 1;
      break;
    }
    if (D_8028AB0C == 1) {
      func_8022F5D0(40);
      w = func_8022F720(buf, 40);
      func_8022F5DC(buf, x - 1, y - 1);
      func_8022F5D0(20);
      func_8022F5DC(suffix, x + adj + w + 3, y - 15);
    } else {
      func_8022F5D0(26);
      w = func_8022F720(buf, 26);
      func_8022F5DC(buf, x, y);
      func_8022F5D0(13);
      func_8022F5DC(suffix, x + 3 + adj * 2 / 3 + w, y - 10);
    }
    func_8022F4EC();
  }
}

/* WHAT IT DOES: Draw the car's current message (the first, else the
 * second) centred in the view, a third of the way down, sized for one or
 * two players, unless the display is switched off. */
/* @t4-pass 0x80238DD4 1 2026-09-26 compiles 17 best 75 moved 2  (n64/tools/n64permute.py) */
/* @t4-pass 0x80238DD4 2 2026-09-26 compiles 17 best 76 moved 1  (n64/tools/n64permute.py) */
/* @t4-pass 0x80238DD4 3 2026-09-26 compiles 17 best 76 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x80238DD4 tgr BrHudPositionDraw */
void BrHudPositionDraw(void)
{
  int x;
  int y;
  int big;
  int small;

  if (D_8026FF10 == 0) {
    if (D_8028AB0C == 1) {
      big = 30;
      small = 20;
    } else {
      big = 20;
      small = 15;
    }
    x = D_8031B2C8[D_8028AAEC].x + D_8031B2C8[D_8028AAEC].w / 2;
    y = D_8031B2C8[D_8028AAEC].y + D_8031B2C8[D_8028AAEC].h / 3;
    func_8022F504();
    if (D_8028AAF0->msgA != 0) {
      func_8022F5D0(big);
      func_8022F5DC(D_8028AAF0->msgA, x, big / 4 + y);
    } else if (D_8028AAF0->msgB != 0) {
      func_8022F5D0(small);
      func_8022F5DC(D_8028AAF0->msgB, x, big * 3 / 16 + y);
    }
  }
}


/* WHAT IT DOES: The race HUD for the view being drawn: clip to the view,
 * the arrow and times panels (not from the car's third camera), the lap
 * counter and position and the car's message, then its speed (never
 * negative) in kph or mph at the bottom right -- smaller in the two-player
 * layout, with the units under it in the one-player one.  (From the third
 * camera in the one-player layout x keeps its first value, 296.) */
/* @implements 0x80238F30 tgr BrHudDraw */
void BrHudDraw(void)
{
  float speed;
  int x;
  int y;

  speed = D_8028AAF0->xfe4[0];
  BrScissorSet(8, D_8031B2C8[D_8028AAEC].y, 312, D_8031B2C8[D_8028AAEC].h);
  if (speed < 0.0) {
    speed = 0.0f;
  }
  if (D_8028AAF4 != &D_8028AAF0->cams[2]) {
    func_80237980();
    BrHudTimesDraw();
  }
  BrHudLapDraw();
  BrHudPositionDraw();
  func_8022F4F8();
  func_8022F520();
  if (D_802723D8 != 0) {
    func_80260DD4(D_80361C30, "%%yw%.0f", speed / 1.609344f);
  } else {
    func_80260DD4(D_80361C30, "%%yw%.0f", speed);
  }
  x = 296;
  y = D_8031B2C8[D_8028AAEC].y + D_8031B2C8[D_8028AAEC].h - 4;
  if (D_8028AB0C == 2) {
    if (D_8028AAF4 != &D_8028AAF0->cams[2]) {
      y = y - D_8028C7B4[D_8028AAEC].h * 3 / 4 - 1;
    }
    func_8022F5D0(15);
    func_8022F5DC((unsigned char *)D_80361C30, x, y);
  } else {
    if (D_8028AAF4 != &D_8028AAF0->cams[2]) {
      x = 266;
      y = y - D_8028C7B4[D_8028AAEC].h;
    }
    func_8022F5D0(20);
    if (D_802723D8 != 0) {
      func_8022F5DC((unsigned char *)D_80361C30, x, y - 3);
    } else {
      func_8022F5DC((unsigned char *)D_80361C30, x - 3, y - 3);
    }
    if (D_8028AAF4 != &D_8028AAF0->cams[2]) {
      func_8022F5D0(15);
      func_8022F514();
      if (D_802723D8 != 0) {
        func_8022F5DC((unsigned char *)"%wwmph", x, y - 3);
      } else {
        func_8022F5DC((unsigned char *)"%wwkph", x - 3, y - 3);
      }
    }
  }
}

/* WHAT IT DOES: Draw a tw by th 4-bit intensity texture as a w by h
 * rectangle at (x, y), upside down (t starts at the bottom row and steps
 * back), tinted red (primitive 255, 88, 88), in 320-wide coordinates
 * doubled on a hi-res screen: the texture is loaded as one block with its
 * wrap masks from BrTexSizeBits, and perspective correction is off for the
 * rectangle and back on after it.  A negative w is taken as its size.  The
 * tile size is written out as a block: the macro's one-line form stores
 * its second word before taking the packet pointer, the ROM after. */
/* @implements 0x80239220 tgr BrTex4bFlipDraw */
void BrTex4bFlipDraw(void *img, int tw, int th, int x, int y, int w, int h)
{
  int ms;
  int mt;
  int ss;
  int st;
  int spare[2];                 /* declared, never used: the frame holds it */

  if (w < 0) {
    w = -w;
  }
  BrTexSizeBits(tw, &ss, &ms);
  BrTexSizeBits(th, &st, &mt);
  gDPPipeSync(D_8028A858++);
  gDPSetCycleType(D_8028A858++, G_CYC_1CYCLE);
  gSPTexture(D_8028A858++, ss, st, 0, 0, 1);
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 12, 2, D_8028A898);
  gDPSetCombine(D_8028A858++, 0x309861, 0x5532ff7f);
  gDPSetRenderMode(D_8028A858++, 0xf0a4000, 0);
  gDPSetTextureImage(D_8028A858++, 4, G_IM_SIZ_16b, 1, img);
  gDPSetTile(D_8028A858++, 4, G_IM_SIZ_16b, 0, 0, 7, 0, 0, mt, 0, 0, ms, 0);
  gDPLoadSync(D_8028A858++);
  gDPLoadBlock(D_8028A858++, 7, 0, 0, ((tw * th + 3) >> 2) - 1,
               ((1 << 11) + (tw / 16 < 1 ? 1 : tw / 16) - 1) / (tw / 16 < 1 ? 1 : tw / 16));
  gDPPipeSync(D_8028A858++);
  gDPSetTile(D_8028A858++, 4, 0, ((tw >> 1) + 7) >> 3, 0, 0, 0, 0, mt, 0, 0, ms, 0);
  {
    Gfx *_g = (Gfx *)(D_8028A858++);

    _g->words.w0 = _SHIFTL(G_SETTILESIZE, 24, 8) | _SHIFTL(0, 12, 12) | _SHIFTL(0, 0, 12);
    _g->words.w1 = _SHIFTL(0, 24, 3) | _SHIFTL((tw - 1) << 2, 12, 12) | _SHIFTL((th - 1) << 2, 0, 12);
  }
  gDPSetTextureLUT(D_8028A858++, 0);
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 19, 1, 0);
  if (D_8028A850 != 0) {
    x <<= 1;
    y <<= 1;
    w <<= 1;
    h <<= 1;
  }
  gDPSetPrimColor(D_8028A858++, 0xff, 0xff, 0xff, 0x58, 0x58, 0xff);
  gDPSetEnvColor(D_8028A858++, 0, 0, 0, 0xff);
  {
    Gfx *_g = (Gfx *)(D_8028A858++);

    _g->words.w0 = (_SHIFTL(G_TEXRECT, 24, 8) | _SHIFTL((x + w) << 2, 12, 12) | _SHIFTL((y + h) << 2, 0, 12));
    _g->words.w1 = (_SHIFTL(0, 24, 3) | _SHIFTL(x << 2, 12, 12) | _SHIFTL(y << 2, 0, 12));
  }
  gImmp1(D_8028A858++, G_RDPHALF_1, (_SHIFTL(0, 16, 16) | _SHIFTL((th - 1) << 5, 0, 16)));
  gImmp1(D_8028A858++, G_RDPHALF_2, (_SHIFTL(((tw << 10) - (1 << 10)) / w, 16, 16) | _SHIFTL(((1 << 10) - (th << 10)) / h, 0, 16)));
  gDPPipeSync(D_8028A858++);
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 19, 1, 0x80000);
}
