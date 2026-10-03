/* frontbuttons.c -- the button prompts along the bottom of front-end screens
 */
#include "tgr/common.h"
#include "tgr/gbi.h"
#include "tgr/car.h"
#include "tgr/menu.h"

/* -- declarations -- */
void BrTextHighlightOff(void);
void BrTextAlignLeft(void);
void BrTextSetFont(int param_1);
void BrTextPrint(char *s, int x, int y);
int func_8023DF9C();
typedef struct BrRomFile { int start; int end; void *data; } BrRomFile;
extern BrRomFile D_80271D70;           /* the A and B button images */
extern BrRomFile D_80271D84;
extern int D_8028AAB4;
extern int D_80271FD0;
typedef struct BrRaceSnd { void *p; int x4; long long x8; int x10; int x14; } BrRaceSnd;
extern BrRaceSnd D_802A4920[6];         /* six sound channels */
extern char D_802A4A08[];
extern BrRomFile D_80272048;            /* the menu background */
extern BrRomFile *D_80271FB0;           /* the screen's panel, and where it goes */
extern int D_80271FB4;
extern int D_80271FB8;
extern int D_80271FBC;
extern int D_80271FC0;
extern float D_80271FC4;                /* seconds idle before the screen gives up, 0 never */
extern void (*D_80271FC8)(int sel);     /* draws the selected row's extras */
extern void (*D_80271FCC)(void);        /* draws the whole middle instead */
extern int D_80271FD4;                  /* the screen has been set up */
extern int D_80271FA0;                  /* -1, or only the players in D_80271FA4 may choose */
extern int D_80271FA4;
extern int D_80271FA8;                  /* the player who chose */
extern int D_80271FAC;                  /* the row chosen, on the way out */
extern char D_80271F88[];               /* the menu lights */
extern char D_80271F90[];
extern int D_8028AAB0;                  /* screen width */
extern int D_8028AAB4;                  /* screen height */
extern float D_8028AAD8;                /* seconds this frame */
extern int D_8028AA98;
extern int D_8028A8A8;
extern Gfx *D_8028A858;
extern char D_001BF480[];               /* the move sound bank in ROM (a link-time address) */
extern char D_001BFEC0[];               /* and the choose sound bank */
extern int D_8026FF08;                  /* human players */
extern BrCar D_8031B760[];
extern float D_8031AB10[4][4];
void BrRomFileUnpack(BrRomFile *f, void *(*alloc)(int size));
void *BrIfaceMemAlloc(int size);
int BrRomReadSize(int rom);
void BrRomUnpack(unsigned char *dst, int rom, int s);
void BrFadeTo(float level, float seconds);
void BrSfxFadeTo(float level, float seconds);
void BrMusicFadeTo(float level, float seconds);
void BrFadeStep(void);
int BrFadeIsOut(void);
int BrFadeAtTarget(void);
void BrFadeBarsDraw(void);
short BrSfxFreeVoice(void);
void BrSfxVoiceStart(short v, unsigned char *start, int len, int loop);
void BrClockTick(void);
void BrFrameBeginLayout1(void);
void BrPerfMark(int bar, int r, int g, int b, int a);
void BrZBufferClear(void);
void BrScreenCameraSet(float w, float h);
void BrViewportSet(int x, int y, int w, int h, int scissor);
void BrTextAlignCentre(void);
int sprintf(char *buf, char *fmt, ...);
Mtx *BrMtxAlloc(void);
void guRotateF(float mf[4][4], float a, float x, float y, float z);
void guMtxF2L(float mf[4][4], Mtx *m);
void BrMenuRingDraw(void);
void BrMenuBackdropDraw(void);
void BrAnimUpdate(void *set);
void BrMenuIconDraw(void *icon, float x, float y, float z, float spin);
void BrPerfMeterDraw(void);
void BrFrameEnd(void);
void BrScreenFlush3Layout1(void);
void BrPadStickToButtons(unsigned int *pad);
void BrPadConsume(unsigned int *pad, unsigned int bits);
/* -- end declarations -- */

/* WHAT IT DOES: Draw the A and B button icons at the foot of the screen
 * with the prompts Select and Go Back. */
/* @implements 0x8020AAE0 tgr BrFrontPromptSelect */
void BrFrontPromptSelect(void)
{
  BrTextHighlightOff();
  BrTextAlignLeft();
  func_8023DF9C(&D_80271D70,100,0xd8,0xc,0xc,0,0,0,0xff,0,0,0,0xff,0);
  func_8023DF9C(&D_80271D84,0xa0,0xd8,0xc,0xc,0,0,0,0xff,0,0,0,0xff,0);
  BrTextSetFont(0xb);
  BrTextPrint("%wwSelect",0x73,(D_8028AAB4 * 0x13) / 0x14 + -3);
  BrTextPrint("%wwGo Back",0xaf,(D_8028AAB4 * 0x13) / 0x14 + -3);
}

/* WHAT IT DOES: Draw the A and B button icons at the foot of the screen
 * with the prompts Continue and Exit. */
/* @implements 0x8020AC18 tgr BrFrontPromptContinue */
void BrFrontPromptContinue(void)
{
  BrTextHighlightOff();
  BrTextAlignLeft();
  func_8023DF9C(&D_80271D70,0x66,0xd8,0xc,0xc,0,0,0,0xff,0,0,0,0xff,0);
  func_8023DF9C(&D_80271D84,0xb2,0xd8,0xc,0xc,0,0,0,0xff,0,0,0,0xff,0);
  BrTextSetFont(0xb);
  BrTextPrint("%wwContinue",0x76,(D_8028AAB4 * 0x13) / 0x14 + -3);
  BrTextPrint("%wwExit",0xc1,(D_8028AAB4 * 0x13) / 0x14 + -3);
}

/* WHAT IT DOES: Set the front-end flag that the season and track-select
 * screens pass to the menu drawer. */
/* @implements 0x8020AD50 tgr BrFrontSetMenuFlag */
void BrFrontSetMenuFlag(int param_1)
{
  D_80271FD0 = param_1;
}



/* WHAT IT DOES: One frame of the generic menu screen: a carousel of rows,
 * one shown at a time (its icon spinning, or its label), turned left and
 * right by the pads. The first frame loads the background, the button
 * prompts, the panel and the two sound banks and fades in. A row can be
 * forced from outside (BrFrontSetMenuFlag). ok, when given, says which rows
 * may be stopped on. Returns 0 while the screen runs; 1 when a row that
 * leaves at once is chosen (the row goes to *sel), 3 or 4 for the two side
 * buttons on a row that takes them; once the fade out after a choice (or B,
 * or the idle time running out) ends, 1 when going on and 2 when backing
 * out. r1..b2 tint the background.
 * Source facts: the screen state is function static (the ROM addresses
 * each afresh, and IDO keeps the selection in a register with stores back);
 * the sound banks' ROM addresses are link-time symbols; the equality tests
 * are against the integer 0 (a fresh 0.0 each, where the ordering tests
 * share f20); the turn is eased on the static itself.
 * The frame must be the ROM's 0x100: the first frame's zlib unpacks read
 * uninitialised stack at a fixed depth below this frame, so a deeper frame
 * builds different (still valid) inflate tables and A5 sees the difference.
 * Plain expressions for the pad word and row flags (not locals) and a
 * 92-byte buffer give 0x100.
 * RESIDUE: the named slots are not the ROM's (off 0xFC, result 0xF4, buf
 * 0x84, row 0x78) and the register choices follow. */
/* @t4-pass 0x8020AD5C 1 2026-10-03 compiles 115 best 985 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8020AD5C 2 2026-10-03 compiles 112 best 953 moved 32  (n64/tools/n64permute.py) */
/* @implements 0x8020AD5C tgr BrMenu */
int BrMenu(char *title, int n, MenuItem **items, int *sel, int (*ok)(int), int r1, int g1, int b1, int r2,
           int g2, int b2)
{
  static int D_80316220;                  /* the row before the last move */
  static float D_80316224;                /* the carousel's turn, -1..1, 0 at rest */
  static float D_80316228;                /* the icon's spin */
  static int D_8031622C;                  /* leaving forwards (not backed out) */
  static unsigned char *D_80316230;       /* the two sound banks: move, choose */
  static int D_80316234;
  static unsigned char *D_80316238;
  static int D_8031623C;
  static float D_80316240;                /* seconds since the last button */
  static int D_80316244;                  /* the selected row */
  char buf[92];
  int i;
  int row;
  int result;
  float off;
  short v;

  result = 0;
  if (D_80271FD4 == 0) {
    BrFadeTo(1.0f, 0.2f);
    BrMusicFadeTo(1.0f, 0.2f);
    BrSfxFadeTo(1.0f, 0.2f);
    D_80316244 = *sel;
    D_80316228 = 4.712389f;
    D_80316224 = 0.0f;
    BrRomFileUnpack(&D_80271D70, BrIfaceMemAlloc);
    BrRomFileUnpack(&D_80271D84, BrIfaceMemAlloc);
    BrRomFileUnpack(&D_80272048, BrIfaceMemAlloc);
    D_80316234 = BrRomReadSize((int)D_001BF480);
    D_80316230 = BrIfaceMemAlloc(D_80316234 + D_80316234);
    BrRomUnpack(D_80316230, (int)D_001BF480, 0);
    D_8031623C = BrRomReadSize((int)D_001BFEC0);
    D_80316238 = BrIfaceMemAlloc(D_8031623C + D_8031623C);
    BrRomUnpack(D_80316238, (int)D_001BFEC0, 0);
    for (i = 0; i != 0x100; i++) {
      D_80316230[D_80316234 + i] = 0;
      D_80316238[D_8031623C + i] = 0;
    }
    if (D_80271FB0 != 0) {
      BrRomFileUnpack(D_80271FB0, BrIfaceMemAlloc);
    }
    D_80316240 = 0.0f;
    D_80271FD0 = -1;
    D_80271FD4 = 1;
  }
  if (D_80271FD0 >= 0) {
    D_80316220 = D_80316244;
    D_80316244 = D_80271FD0;
    if (D_80271FD0 != D_80316220) {
      D_80316224 = -1.0f;
      v = BrSfxFreeVoice();
      if (v != -1) {
        BrSfxVoiceStart(v, D_80316230, D_80316234, 0);
      }
    }
    D_80271FD0 = -1;
  }
  BrClockTick();
  BrFadeStep();
  BrFrameBeginLayout1();
  BrPerfMark(0, 0, 0, 200, 0xFF);
  BrZBufferClear();
  func_8023DF9C(&D_80272048, 0, 0, 320, 240, r1, g1, b1, 0xFF, r2, g2, b2, 0xFF, 4);
  D_80316228 += D_8028AAD8 + D_8028AAD8;
  while (D_80316228 > 6.2831855f) {
    D_80316228 -= 6.2831855f;
  }
  if (D_80316224 < 0.0f) {
    D_80316224 += D_8028AAD8 + D_8028AAD8;
    if (D_80316224 > 0.0f) {
      D_80316224 = 0.0f;
    }
  } else if (D_80316224 > 0.0f) {
    D_80316224 -= D_8028AAD8 + D_8028AAD8;
    if (D_80316224 < 0.0f) {
      D_80316224 = 0.0f;
    }
  }
  if (D_80316224 < 0.0f) {
    off = D_80316224 * D_80316224 * -5.0f;
  } else {
    off = D_80316224 * D_80316224 * 5.0f;
  }
  BrScreenCameraSet(D_8028AAB0, D_8028AAB4);
  BrViewportSet(0, 0, D_8028AAB0, D_8028AAB4, 1);
  gSPNumLights(D_8028A858++, 1);
  gSPLight(D_8028A858++, D_80271F90, 1);
  gSPLight(D_8028A858++, D_80271F88, 2);
  gSPClipRatio(D_8028A858++, 6);
  if (D_80271FB0 != 0) {
    func_8023DF9C(D_80271FB0, D_80271FB4, D_80271FB8, D_80271FBC, D_80271FC0, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF,
                  0xFF, 2);
  }
  BrTextSetFont(30);
  BrTextAlignCentre();
  sprintf(buf, "%%ry%s", title);
  BrTextPrint(buf, D_8028AAB0 / 2, D_8028AAB4 / 6 - 2);
  if (D_80271FCC != 0) {
    D_80271FCC();
  } else {
    Mtx *mtx = BrMtxAlloc();

    guRotateF(D_8031AB10, 12.0f, 1.0f, 0.0f, 0.0f);
    guMtxF2L(D_8031AB10, mtx);
    gSPMatrix(D_8028A858++, mtx, G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_PUSH);
    BrMenuRingDraw();
    row = D_80316244;
    do {
      row = (row + n - 1) % n;
    } while (ok != 0 && ok(row) == 0);
    if (row != D_80316244) {
      BrMenuBackdropDraw();
    }
    if (items[D_80316244]->flags & 4) {
      if (D_80316224 > 0.75f || D_80316224 < -0.75f) {
        row = D_80316220;
      } else {
        row = D_80316244;
      }
      BrAnimUpdate(items[row]->icon);
      if (items[row]->flags & 0x10) {
        D_8028A8A8 = 1;
      }
      BrMenuIconDraw(items[row]->icon, D_8028AAB0 / 2, (D_8028AAB4 * 6) / 16, 0.0f,
                     -1.5707964f - D_80316224 * 6.2831855f);
      D_8028A8A8 = 0;
      if (D_80316224 == 0) {
        goto label;
      }
    } else if (off != 0) {
      if (items[D_80316244]->icon != 0) {
        BrAnimUpdate(items[D_80316244]->icon);
        if (items[D_80316244]->flags & 0x10) {
          D_8028A8A8 = 1;
        }
        BrMenuIconDraw(items[D_80316244]->icon, D_8028AAB0 / 2 - off * D_8028AAB0 / 6.0f,
                       (D_8028AAB4 * 6) / 16, 0.0f, D_80316228);
        D_8028A8A8 = 0;
      } else {
        sprintf(buf, "%%ww%s", items[D_80316244]->label);
        BrTextHighlightOff();
        BrTextAlignCentre();
        BrTextPrint(buf, D_8028AAB0 / 2 - (int)(off * D_8028AAB0 / 6.0f), (D_8028AAB4 * 9) / 16);
      }
      if (items[D_80316220]->icon != 0) {
        BrAnimUpdate(items[D_80316220]->icon);
        if (items[D_80316220]->flags & 0x10) {
          D_8028A8A8 = 1;
        }
        BrMenuIconDraw(items[D_80316220]->icon,
                       D_8028AAB0 / 2 - ((off < 0.0f ? 5.0f : -5.0f) + off) * D_8028AAB0 / 6.0f,
                       (D_8028AAB4 * 6) / 16, 0.0f, D_80316228);
        D_8028A8A8 = 0;
      } else {
        sprintf(buf, "%%ww%s", items[D_80316220]->label);
        BrTextHighlightOff();
        BrTextAlignCentre();
        BrTextPrint(buf, D_8028AAB0 / 2 - (int)(((off < 0.0f ? 5.0f : -5.0f) + off) * D_8028AAB0 / 6.0f),
                    (D_8028AAB4 * 9) / 16);
      }
    } else {
      BrTextHighlightOff();
      BrTextAlignCentre();
      if (items[D_80316244]->icon != 0) {
        BrAnimUpdate(items[D_80316244]->icon);
        if (items[D_80316244]->flags & 0x10) {
          D_8028A8A8 = 1;
        }
        BrMenuIconDraw(items[D_80316244]->icon, D_8028AAB0 / 2, (D_8028AAB4 * 6) / 16, 0.0f, D_80316228);
        D_8028A8A8 = 0;
      } else {
        sprintf(buf, "%%ww%s", items[D_80316244]->label);
        BrTextPrint(buf, D_8028AAB0 / 2, (D_8028AAB4 * 9) / 16);
      }
    label:
      BrTextSetFont(20);
      sprintf(buf, "%%ry%s", items[D_80316244]->label);
      BrTextPrint(buf, D_8028AAB0 / 2, (D_8028AAB4 * 20) / 64);
      if (D_80271FC8 != 0) {
        D_80271FC8(D_80316244);
      }
    }
    gSPPopMatrix(D_8028A858++, 0);
  }
  BrFrontPromptSelect();
  BrFadeBarsDraw();
  if (D_8028AA98 >= 2) {
    BrViewportSet(0, 0, D_8028AAB0, D_8028AAB4, 1);
    BrPerfMeterDraw();
  }
  BrFrameEnd();
  if (BrFadeIsOut() != 0) {
    if (BrFadeAtTarget() != 0) {
      *sel = D_80316244;
      D_80271FD4 = 0;
      BrScreenFlush3Layout1();
      for (i = 0; i < 6; i++) {
        D_802A4920[i].x14 = 0;
        D_802A4920[i].x10 = 0;
        D_802A4920[i].x8 = 0;
        D_802A4920[i].p = D_802A4A08;
      }
      if (D_8031622C != 0) {
        return 1;
      }
      return 2;
    }
  } else {
    for (i = 0; i < D_8026FF08; i++) {
      if (D_80271FA0 != -1 && ((1 << i) & D_80271FA4) == 0) {
        continue;
      }
      BrPadStickToButtons(D_8031B760[i].pad);
      if (*D_8031B760[i].pad != 0) {
        D_80316240 = 0.0f;
      }
      if (D_80316224 == 0) {
        if (*D_8031B760[i].pad & 4) {
          BrPadConsume(D_8031B760[i].pad, 10);
          *D_8031B760[i].pad &= ~10;
          D_80316220 = D_80316244;
          do {
            D_80316244 = (D_80316244 + n - 1) % n;
          } while (ok != 0 && ok(D_80316244) == 0);
          if (D_80316244 != D_80316220) {
            D_80316224 = 1.0f;
            goto move;
          }
        }
        if (*D_8031B760[i].pad & 1) {
          BrPadConsume(D_8031B760[i].pad, 10);
          *D_8031B760[i].pad &= ~10;
          D_80316220 = D_80316244;
          do {
            D_80316244 = (D_80316244 + 1) % n;
          } while (ok != 0 && ok(D_80316244) == 0);
          if (D_80316244 != D_80316220) {
            D_80316224 = -1.0f;
          move:
            v = BrSfxFreeVoice();
            if (v != -1) {
              BrSfxVoiceStart(v, D_80316230, D_80316234, 0);
            }
          }
        }
        if (items[D_80316244]->flags & 1) {
          if (*D_8031B760[i].pad & 8) {
            *sel = D_80316244;
            result = 3;
            BrPadConsume(D_8031B760[i].pad, 8);
            goto choose;
          }
          if (*D_8031B760[i].pad & 2) {
            *sel = D_80316244;
            result = 4;
            BrPadConsume(D_8031B760[i].pad, 2);
            goto choose;
          }
        }
        if (*D_8031B760[i].pad & 0x20) {
          BrPadConsume(D_8031B760[i].pad, 0x20);
          D_8031622C = 0;
          BrFadeTo(0.0f, 0.2f);
          BrSfxFadeTo(0.0f, 0.2f);
          goto choose;
        }
      }
      if (!(items[D_80316244]->flags & 8) && !(items[D_80316244]->flags & 0x40) && (*D_8031B760[i].pad & 0xC010)) {
        BrPadConsume(D_8031B760[i].pad, 0xC010);
        if (i == 0 || !(items[D_80316244]->flags & 0x20)) {
          D_80271FA8 = i;
          if ((items[D_80316244]->flags & 2) == 0) {
            D_8031622C = 1;
            BrSfxFadeTo(0.0f, 0.2f);
            BrFadeTo(0.0f, 0.2f);
          } else {
            *sel = D_80316244;
            result = 1;
          }
        choose:
          v = BrSfxFreeVoice();
          if (v != -1) {
            BrSfxVoiceStart(v, D_80316238, D_8031623C, 0);
          }
        }
      }
    }
  }
  if (D_80271FC4 != 0 && result == 0) {
    D_80316240 = D_8028AAD8 + D_80316240;
    if (D_80271FC4 <= D_80316240 && BrFadeIsOut() == 0) {
      D_8031622C = 0;
      BrFadeTo(0.0f, 0.2f);
      BrSfxFadeTo(0.0f, 0.2f);
    }
  }
  D_80271FAC = D_80316244;
  return result;
}
