/* credits.c -- the credits sequence
 */
#include "tgr/common.h"
#include "tgr/gbi.h"
#include "tgr/pad.h"

/* -- declarations -- */
void func_8021D84C(int param_1);
extern int D_80270810;                  /* the sequence has been set up */
extern int D_8027081C;                  /* it is fading out */
extern char D_80270820[];               /* its lights */
extern char D_80270828[];
extern char D_80271F88[];               /* the title card's lights */
extern char D_80271F90[];
extern int D_80271D98;                  /* BrModelDraw: the opaque surface mode, once */
typedef struct BrRaceSnd { void *p; int x4; unsigned long long pitch; int x10; unsigned int level; } BrRaceSnd;
extern BrRaceSnd D_802A4920[6];         /* six sound channels */
extern Gfx *D_8028A858;
extern int D_8028AAB0;                  /* screen width */
extern int D_8028AAB4;                  /* screen height */
extern float D_8028AAD8;                /* seconds this frame */
extern int D_8028AA98;
extern int D_8028A868;                  /* the camera: set, near and far */
extern float D_8028A86C;
extern float D_8028A870;
extern float D_8031AB10[4][4];
extern char D_001B5440[];               /* the sound banks (link-time ROM addresses) */
extern char D_001BF3C0[];
extern char D_001DA030[];               /* the models */
extern char D_001DBA40[];
extern char D_001DCD70[];
extern char D_001E09F0[];
extern char D_001E1CA0[];
extern char D_001E2B40[];
extern char D_001E3DB0[];
extern char D_802ABBE0[];               /* the build date */
void BrFadeTo(float level, float seconds);
void BrSfxFadeTo(float level, float seconds);
void BrMusicFadeTo(float level, float seconds);
void BrFadeStep(void);
int BrFadeIsIn(void);
int BrFadeIsOut(void);
int BrFadeAtTarget(void);
void BrFadeBarsDraw(void);
void BrIfaceMemReset(void);
void *BrIfaceMemAlloc(int size);
int BrRomReadSize(int rom);
void BrRomUnpack(unsigned char *dst, int rom, int s);
int BrModelLoad(int p, int rom);
void BrAnimSetLoop(int set);
short BrSfxFreeVoice(void);
void BrSfxVoiceStart(short v, unsigned char *start, int len, int loop);
void BrClockTick(void);
void BrFrameBeginLayout1(void);
void BrZBufferClear(void);
void BrScreenClear(int r, int g, int b);
void BrPerfMark(int bar, int r, int g, int b, int a);
void BrScreenCameraSet(float w, float h);
void BrViewportSet(int x, int y, int w, int h, int scissor);
void BrScissorSet(int x, int y, int w, int h);
Mtx *BrMtxAlloc(void);
void guTranslateF(float mf[4][4], float x, float y, float z);
void guRotateF(float mf[4][4], float a, float x, float y, float z);
void guMtxF2L(float mf[4][4], Mtx *m);
void BrMenuIconDrawScaled(int icon, float x, float y, float z, float spin, float scale);
void BrTextSetColours(int r1, int g1, int b1, int r2, int g2, int b2);
void BrTextHighlightOff(void);
void BrTextSetFont(int font);
void BrTextAlignCentre(void);
void BrTextPrint(char *s, int x, int y);
float sqrtf(float x);
void BrPerfMeterDraw(void);
void BrFrameSetHook(int hook);
void BrFrameEnd(void);
void BrScreenFlush3Layout1(void);
void BrModeSet(int mode);
void BrMainMenu(void);
void BrDemoRaceStartA(void);
void BrPadConsume(BrPadRec *pad, unsigned int bits);
void BrCreditsDrawCars(void);
/* -- end declarations -- */

static int D_80315E80;                  /* the three publisher logos, */
static int D_80315E84;
static int D_80315E88;
static int D_80315E8C;                  /* then the four title-card models */
static int D_80315E90;
static int D_80315E94;
static int D_80315E98;

/* WHAT IT DOES: Draw the three car models the credits sequence shows. */
/* @implements 0x80206830 tgr BrCreditsDrawCars */
void BrCreditsDrawCars(void)
{
  func_8021D84C(D_80315E88);
  func_8021D84C(D_80315E8C);
  func_8021D84C(D_80315E90);
}

/* WHAT IT DOES: One frame of the boot sequence before the main menu. The
 * first frame loads the three publisher logos, the four title-card models
 * and the two sound banks and fades in. Each logo in turn (at 0, 4 and 8
 * seconds) spins in from the left with a whoosh and a hit, holds, then
 * slides off right with a fading whoosh; the second and third have a
 * caption (Produced by, Developed by) fading in and out. From 12 seconds the
 * title card's four models fly in under their own lights and matrices,
 * and from 17.2 the legal text fades in (the build date too, with the four
 * C buttons held). A or Start skips ahead to the legal text, and a second
 * press (or 24.2 seconds) fades out; the faded-out frame silences the
 * channels and goes to the main menu after a press, else to the attract
 * race.
 * Source facts: the logo and model pointers are file statics (BrCreditsDrawCars
 * animates three of them), the clock, sound banks and flags function statics
 * (the ROM addresses each afresh and keeps the clock in f16 with stores
 * back); ROM file offsets are link-time symbols.
 * RESIDUE (1461, same size): the ROM frame is 0xB0 against 0xA0 here, and each model load reloads
 * the stored pointer where the ROM passes v0 on. */
/* @implements 0x8020686C tgr BrIntroScreen */
void BrIntroScreen(void)
{
  static float D_80315E9C;                /* seconds into the sequence */
  static int D_80315EA0;                  /* the two sound banks: whoosh, */
  static unsigned char *D_80315EA4;
  static int D_80315EA8;                  /* and hit */
  static unsigned char *D_80315EAC;
  static int D_80315EB0;                  /* per logo: its exit voice, whoosh played, hit played */
  static int D_80315EB4;
  static int D_80315EB8;
  static int D_80315EBC;
  static int D_80315EC0;
  static int D_80315EC4;
  static int D_80315EC8;
  static int D_80315ECC;
  static int D_80315ED0;
  static int D_80315ED4;                  /* a button was pressed: go to the main menu */
  Mtx *mtx;
  float z;
  int a;
  int v;
  int i;
  BrPadRec *pad;

  if (D_80270810 == 0) {
    BrFadeTo(1.0f, 0.2f);
    BrMusicFadeTo(1.0f, 0.2f);
    BrSfxFadeTo(1.0f, 0.2f);
    BrIfaceMemReset();
    D_80315E88 = (int)BrIfaceMemAlloc(BrRomReadSize((int)D_001DCD70));
    BrModelLoad(D_80315E88, (int)D_001DCD70);
    D_80315E84 = (int)BrIfaceMemAlloc(BrRomReadSize((int)D_001DBA40));
    BrModelLoad(D_80315E84, (int)D_001DBA40);
    D_80315E80 = (int)BrIfaceMemAlloc(BrRomReadSize((int)D_001DA030));
    BrModelLoad(D_80315E80, (int)D_001DA030);
    D_80315E8C = (int)BrIfaceMemAlloc(BrRomReadSize((int)D_001E09F0));
    BrModelLoad(D_80315E8C, (int)D_001E09F0);
    BrAnimSetLoop(D_80315E8C);
    D_80315E90 = (int)BrIfaceMemAlloc(BrRomReadSize((int)D_001E3DB0));
    BrModelLoad(D_80315E90, (int)D_001E3DB0);
    BrAnimSetLoop(D_80315E90);
    D_80315E94 = (int)BrIfaceMemAlloc(BrRomReadSize((int)D_001E1CA0));
    BrModelLoad(D_80315E94, (int)D_001E1CA0);
    BrAnimSetLoop(D_80315E94);
    D_80315E98 = (int)BrIfaceMemAlloc(BrRomReadSize((int)D_001E2B40));
    BrModelLoad(D_80315E98, (int)D_001E2B40);
    BrAnimSetLoop(D_80315E98);
    D_80315EA0 = BrRomReadSize((int)D_001B5440);
    D_80315EA4 = BrIfaceMemAlloc(D_80315EA0 + 0x100);
    BrRomUnpack(D_80315EA4, (int)D_001B5440, 0);
    D_80315EA8 = BrRomReadSize((int)D_001BF3C0);
    D_80315EAC = BrIfaceMemAlloc(D_80315EA8 + 0x100);
    BrRomUnpack(D_80315EAC, (int)D_001BF3C0, 0);
    for (i = 0; i != 0x100; i++) {
      D_80315EA4[D_80315EA0 + i] = 0;
      D_80315EAC[D_80315EA8 + i] = 0;
    }
    D_80315EB8 = 0;
    D_80315EB4 = 0;
    D_80315EC4 = 0;
    D_80315EC0 = 0;
    D_80315ED0 = 0;
    D_80315ECC = 0;
    D_80315ED4 = 0;
    D_80270810 = 1;
    D_80315E9C = 0.0f;
  }
  BrClockTick();
  BrFadeStep();
  BrFrameBeginLayout1();
  BrZBufferClear();
  BrScreenClear(0, 0, 0);
  BrPerfMark(0, 0, 0, 200, 0xFF);
  if (BrFadeIsIn() == 0 || BrFadeAtTarget() != 0) {
    D_80315E9C += D_8028AAD8;
  }
  D_8028A868 = 1;
  D_8028A86C = 10.0f;
  D_8028A870 = 5000.0f;
  BrScreenCameraSet(D_8028AAB0, D_8028AAB4);
  BrViewportSet(0, 0, D_8028AAB0, D_8028AAB4, 1);
  gSPNumLights(D_8028A858++, 1);
  gSPLight(D_8028A858++, D_80270828, 1);
  gSPLight(D_8028A858++, D_80270820, 2);
  gSPClipRatio(D_8028A858++, 6);
  gSPClipRatio(D_8028A858++, 1);
  mtx = BrMtxAlloc();
  guTranslateF(D_8031AB10, D_8028AAB0 / 2, D_8028AAB4 / 2, 100.0f);
  guMtxF2L(D_8031AB10, mtx);
  gSPMatrix(D_8028A858++, mtx, G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_PUSH);

  if (D_80315E9C >= 0.0f) {
    if (D_80315E9C < 2.0f) {
      if (D_80315E9C > 0.35f && D_80315EB4 == 0) {
        short v1 = BrSfxFreeVoice();

        if (v1 != -1) {
          BrSfxVoiceStart(v1, D_80315EA4, D_80315EA0, 0);
          D_802A4920[v1].level = 0x200020;
          D_802A4920[v1].pitch = 0x100000000;
        }
        D_80315EB4 = 1;
      }
      if (D_80315E9C > 0.5f && D_80315EB8 == 0) {
        short v2 = BrSfxFreeVoice();

        if (v2 != -1) {
          BrSfxVoiceStart(v2, D_80315EAC, D_80315EA8, 0);
          D_802A4920[v2].level = 0x200020;
          D_802A4920[v2].pitch = 0x100000000;
        }
        D_80315EB8 = 1;
      }
      BrMenuIconDrawScaled(D_80315E80, -(2.0f - D_80315E9C) * 32.0f, 0.0f, -50.0f - (2.0f - D_80315E9C) * 750.0f,
                           D_80315E9C * 1.5707964f * D_80315E9C * D_80315E9C * D_80315E9C * 0.0625f + -3.1415927f,
                           0.03125f);
    } else if (D_80315E9C < 6.0f) {
      if (D_80315E9C > 3.5f) {
        z = (D_80315E9C - 3.5f) * (D_80315E9C - 3.5f) * 256.0f;
      } else {
        z = 0.0f;
      }
      BrMenuIconDrawScaled(D_80315E80, z, 0.0f, -50.0f, -1.5707964f, 0.03125f);
      if (D_80315E9C > 3.5f) {
        if (D_80315EB4 < 2) {
          D_80315EB0 = BrSfxFreeVoice();
          if (D_80315EB0 != -1) {
            BrSfxVoiceStart(D_80315EB0, D_80315EA4, D_80315EA0, 0);
            D_802A4920[D_80315EB0].level = 0x140014;
            D_802A4920[D_80315EB0].pitch = 0x100000000;
          }
          D_80315EB4 = 2;
        } else {
          v = (D_80315E9C - 3.5f) * 32.0f;
          if (v > 20) {
            v = 20;
          }
          D_802A4920[D_80315EB0].level = ((20 - v) << 16) + v + 20;
        }
      }
    }
  }

  if (D_80315E9C >= 4.0f) {
    if (D_80315E9C < 6.0f) {
      a = 200 - (int)((6.0f - D_80315E9C) * 255.0 * 4.0);
    } else {
      a = 200;
      if (D_80315E9C > 7.5f) {
        a = 200 - (int)((D_80315E9C - 7.5f) * 255.0 * 4.0);
      }
    }
    if (a > 0) {
      BrTextSetColours(a, a, a, a, a, a);
      BrTextHighlightOff();
      BrTextSetFont(10);
      BrTextAlignCentre();
      BrTextPrint("Produced by", 160, 70);
    }
    if (D_80315E9C < 6.0f) {
      if (D_80315E9C > 4.35f && D_80315EC0 == 0) {
        short v3 = BrSfxFreeVoice();

        if (v3 != -1) {
          BrSfxVoiceStart(v3, D_80315EA4, D_80315EA0, 0);
          D_802A4920[v3].level = 0x200020;
          D_802A4920[v3].pitch = 0x100000000;
        }
        D_80315EC0 = 1;
      }
      if (D_80315E9C > 4.5f && D_80315EC4 == 0) {
        short v4 = BrSfxFreeVoice();

        if (v4 != -1) {
          BrSfxVoiceStart(v4, D_80315EAC, D_80315EA8, 0);
          D_802A4920[v4].level = 0x200020;
          D_802A4920[v4].pitch = 0x100000000;
        }
        D_80315EC4 = 1;
      }
      BrMenuIconDrawScaled(D_80315E84, -(6.0f - D_80315E9C) * 32.0f, 0.0f, -50.0f - (6.0f - D_80315E9C) * 750.0f,
                           (D_80315E9C - 4.0f) * 1.5707964f * (D_80315E9C - 4.0f) * (D_80315E9C - 4.0f) *
                                   (D_80315E9C - 4.0f) * 0.0625f +
                               -3.1415927f,
                           0.03125f);
    } else if (D_80315E9C < 10.0f) {
      if (D_80315E9C > 7.5f) {
        z = (D_80315E9C - 7.5f) * (D_80315E9C - 7.5f) * 256.0f;
      } else {
        z = 0.0f;
      }
      BrMenuIconDrawScaled(D_80315E84, z, 0.0f, -50.0f, -1.5707964f, 0.03125f);
      if (D_80315E9C > 7.5f) {
        if (D_80315EC0 < 2) {
          D_80315EBC = BrSfxFreeVoice();
          if (D_80315EBC != -1) {
            BrSfxVoiceStart(D_80315EBC, D_80315EA4, D_80315EA0, 0);
            D_802A4920[D_80315EBC].level = 0x140014;
            D_802A4920[D_80315EBC].pitch = 0x100000000;
          }
          D_80315EC0 = 2;
        } else {
          v = (D_80315E9C - 7.5f) * 32.0f;
          if (v > 20) {
            v = 20;
          }
          D_802A4920[D_80315EBC].level = ((20 - v) << 16) + v + 20;
        }
      }
    }
  }

  if (D_80315E9C >= 8.0f) {
    if (D_80315E9C < 10.0f) {
      a = 200 - (int)((10.0f - D_80315E9C) * 255.0 * 4.0);
    } else {
      a = 200;
      if (D_80315E9C > 11.5f) {
        a = 200 - (int)((D_80315E9C - 11.5f) * 255.0 * 4.0);
      }
    }
    if (a > 0) {
      BrTextSetColours(a, a, a, a, a, a);
      BrTextHighlightOff();
      BrTextSetFont(10);
      BrTextAlignCentre();
      BrTextPrint("Developed by", 160, 60);
    }
    if (D_80315E9C < 10.0f) {
      if (D_80315E9C > 8.35f && D_80315ECC == 0) {
        short v5 = BrSfxFreeVoice();

        if (v5 != -1) {
          BrSfxVoiceStart(v5, D_80315EA4, D_80315EA0, 0);
          D_802A4920[v5].level = 0x200020;
          D_802A4920[v5].pitch = 0x100000000;
        }
        D_80315ECC = 1;
      }
      if (D_80315E9C > 8.5f && D_80315ED0 == 0) {
        short v6 = BrSfxFreeVoice();

        if (v6 != -1) {
          BrSfxVoiceStart(v6, D_80315EAC, D_80315EA8, 0);
          D_802A4920[v6].level = 0x200020;
          D_802A4920[v6].pitch = 0x100000000;
        }
        D_80315ED0 = 1;
      }
      BrMenuIconDrawScaled(D_80315E88, -(10.0f - D_80315E9C) * 32.0f, 0.0f, -50.0f - (10.0f - D_80315E9C) * 750.0f,
                           (D_80315E9C - 8.0f) * 1.5707964f * (D_80315E9C - 8.0f) * (D_80315E9C - 8.0f) *
                                   (D_80315E9C - 8.0f) * 0.0625f +
                               -3.1415927f,
                           0.03125f);
    } else if (D_80315E9C < 13.0f) {
      if (D_80315E9C > 11.5f) {
        z = (D_80315E9C - 11.5f) * (D_80315E9C - 11.5f) * 256.0f;
      } else {
        z = 0.0f;
      }
      BrMenuIconDrawScaled(D_80315E88, z, 0.0f, -50.0f, -1.5707964f, 0.03125f);
      if (D_80315E9C > 11.5f) {
        if (D_80315ECC < 2) {
          D_80315EC8 = BrSfxFreeVoice();
          if (D_80315EC8 != -1) {
            BrSfxVoiceStart(D_80315EC8, D_80315EA4, D_80315EA0, 0);
            D_802A4920[D_80315EC8].level = 0x140014;
            D_802A4920[D_80315EC8].pitch = 0x100000000;
          }
          D_80315ECC = 2;
        } else {
          v = (D_80315E9C - 11.5f) * 32.0f;
          if (v > 20) {
            v = 20;
          }
          D_802A4920[D_80315EC8].level = ((20 - v) << 16) + v + 20;
        }
      }
    }
  }

  if (D_80315E9C > 12.0f) {
    gSPNumLights(D_8028A858++, 1);
    gSPLight(D_8028A858++, D_80271F90, 1);
    gSPLight(D_8028A858++, D_80271F88, 2);
    mtx = BrMtxAlloc();
    guRotateF(D_8031AB10, 7.0f, 0.0f, 0.0f, 1.0f);
    guMtxF2L(D_8031AB10, mtx);
    gSPMatrix(D_8028A858++, mtx, G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_PUSH);
    mtx = BrMtxAlloc();
    guRotateF(D_8031AB10, -7.0f, 1.0f, 0.0f, 0.0f);
    guMtxF2L(D_8031AB10, mtx);
    gSPMatrix(D_8028A858++, mtx, G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_NOPUSH);
    D_80271D98 = 1;
    if (D_80315E9C > 15.0f) {
      BrScissorSet(40, 100, 250, 50);
    }
    if (D_80315E9C < 15.0f) {
      z = (sqrtf(3.0f) - sqrtf(D_80315E9C - 12.0f)) * 300.0f;
    } else {
      z = -0.0f;
    }
    BrMenuIconDrawScaled(D_80315E8C, 0.0f, 0.0f, z, sqrtf(D_80315E9C - 12.0f) * 0.01f + -1.5707964f, 0.05f);
    gSPPopMatrix(D_8028A858++, 0);
    BrScissorSet(0, 0, 320, 240);
  }
  if (D_80315E9C > 13.0f) {
    D_80271D98 = 1;
    if (D_80315E9C < 15.0f) {
      z = (sqrtf(2.0f) - sqrtf(D_80315E9C - 13.0f)) * 500.0f;
    } else {
      z = 0.0f;
    }
    BrMenuIconDrawScaled(D_80315E90, 0.0f, 0.0f, z, -1.5707964f, 0.083333336f);
  }
  if (D_80315E9C > 15.2f) {
    if (D_80315E9C < 16.2f) {
      z = (sqrtf(1.000001f) - sqrtf(D_80315E9C - 15.2f)) * 500.0f;
    } else {
      z = 0.0f;
    }
    BrMenuIconDrawScaled(D_80315E98, 0.0f, 0.0f, z, -1.5707964f, 0.083333336f);
  }
  if (D_80315E9C > 15.0f) {
    if (D_80315E9C < 16.0f) {
      z = (sqrtf(1.0f) - sqrtf(D_80315E9C - 15.0f)) * 500.0f;
    } else {
      z = 0.0f;
    }
    BrMenuIconDrawScaled(D_80315E94, 0.0f, 0.0f, z, -1.5707964f, 0.083333336f);
  }
  if (D_80315E9C > 17.2f) {
    a = (D_80315E9C - 17.2f) * 255.0 * 4.0;
    if (a > 200) {
      a = 200;
    }
    BrTextSetColours(a, a, a, a, a, a);
    BrTextHighlightOff();
    BrTextSetFont(10);
    BrTextAlignCentre();
    if ((D_8036A8E0[0].pressed & 0xF00) == 0xF00) {
      BrTextPrint(D_802ABBE0, 160, 40);
    }
    BrTextSetFont(7);
    BrTextPrint("O 1997 Boss Game Studios, Inc.  All Rights Reserved.", 160, 190);
    BrTextPrint("Top Gear Rally is a trademark of Kemco.", 160, 197);
    BrTextPrint("Distributed by Midway Home Entertainment under license.", 160, 204);
    BrTextPrint("Midway is a trademark of Midway Games Inc.  Used by permission.", 160, 211);
    BrTextPrint("LICENSED BY NINTENDO", 160, 224);
    BrTextSetFont(3);
    BrTextPrint("c", 78, 190);
  }
  gSPPopMatrix(D_8028A858++, 0);
  BrFadeBarsDraw();
  if (D_8028AA98 >= 2) {
    BrViewportSet(0, 0, D_8028AAB0, D_8028AAB4, 1);
    BrPerfMeterDraw();
  }
  BrFrameSetHook((int)BrCreditsDrawCars);
  BrFrameEnd();
  if (BrFadeIsOut() != 0) {
    if (BrFadeAtTarget() != 0) {
      D_80270810 = 0;
      for (i = 0; i < 5; i++) {
        D_802A4920[i].pitch = 0;
        D_802A4920[i].level = 0;
      }
      BrScreenFlush3Layout1();
      if (D_80315ED4 != 0) {
        BrModeSet((int)BrMainMenu);
      } else {
        BrDemoRaceStartA();
      }
    }
    return;
  }
  if (D_80315ED4 != 0 && D_80315E9C > 19.2f) {
    goto fade;
  }
  for (pad = D_8036A8E0; pad != &D_8036A8E0[2]; pad++) {
    if (pad->pressed & 0xC010) {
      BrPadConsume(pad, 0xC010);
      D_80315ED4 = 1;
      if (D_8027081C != 0) {
        goto fade;
      }
      if (D_80315E9C < 17.2f) {
        D_80315E9C = 17.2f;
      } else if (D_80315E9C > 19.2f) {
        goto fade;
      }
    }
  }
  if (D_80315E9C >= 24.2f) {
  fade:
    D_8027081C = 1;
    BrFadeTo(0.0f, 0.2f);
    BrSfxFadeTo(0.0f, 0.2f);
  }
}
