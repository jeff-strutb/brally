/* carselect.c -- the car-select screen
 */
#include "tgr/common.h"
#include "tgr/gbi.h"

/* -- declarations -- */
int BrCarModelPresent(int param_1);
extern int D_8020C688;
extern int D_8020C68C;
extern int D_80272070;
extern char *D_8031C5BC;
extern char D_8028AE24;
extern Gfx *D_8028A858;
int sprintf(char *buf, char *fmt, ...);
/* -- end declarations -- */

/* WHAT IT DOES: Draw one of the car-select screen's stat bars at (x, y),
 * w by h, in fill mode: a dark frame, the empty bar inset by 3 pixels, and
 * the filled part as the given fraction of its width. */
/* @t4-pass 0x8020C460 1 2026-10-03 compiles 114 best 121 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8020C460 2 2026-10-03 compiles 114 best 121 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x8020C460 */
/* @implements 0x8020C460 tgr BrCarStatBarDraw */
void BrCarStatBarDraw(int x, int y, int w, int h, float frac)
{
  gDPPipeSync(D_8028A858++);
  gDPSetCycleType(D_8028A858++, G_CYC_FILL);
  gDPSetRenderMode(D_8028A858++, 0, 0);
  gDPSetCombine(D_8028A858++, 0xffffff, 0xfffdf6fb);
  gDPSetFillColor(D_8028A858++, 1);
  gDPFillRectangle(D_8028A858++, x, y, x + w, y + h);
  x += 3;
  gDPPipeSync(D_8028A858++);
  gDPSetFillColor(D_8028A858++, 0x1c1);
  gDPFillRectangle(D_8028A858++, x, y + 3, x + w - 6, y + h - 3);
  gDPPipeSync(D_8028A858++);
  gDPSetFillColor(D_8028A858++, 0x781);
  gDPFillRectangle(D_8028A858++, x, y + 3, x + (int)((w - 6) * frac), y + h - 3);
}

/* WHAT IT DOES: Tell whether car n may be picked on the car-select screen:
 * cars 9 and up only while the flag at 0x80272070 is clear, the car's bit
 * must be set in the season record's unlock mask, and its model must be
 * present. */
/* @implements 0x8020C66C tgr BrCarSelectable */
int BrCarSelectable(int n)
{
  return (D_80272070 == 0 || n < 9) && (*(unsigned short *)(D_8031C5BC + 0xcc) & (1 << n)) &&
         BrCarModelPresent(n);
}

/* -- declarations: BrCarViewDraw -- */
#include "tgr/car.h"
typedef struct BrModel BrModel;
typedef struct BrStream { char pad00[0x28]; } BrStream;
extern BrCar *D_8028AAF0;               /* the car being drawn */
extern BrCarCam *D_8028AAF4;            /* the camera being drawn from */
extern float D_8028AAC0;                /* the lens scale */
extern float D_8028AAC8;
extern int D_80272074;
extern BrStream D_8031B370[4];
extern float D_8031AB10[4][4];
extern float D_8031AB50[4][4];
extern char D_802721F0[];               /* the car-select lights: the ambient, then four lights */
float cosf(float x);
float sinf(float x);
void BrVec3Cross(BrVec3 *pOut, BrVec3 *pA, BrVec3 *pB);
float BrAtan2(float x, float y);
void BrCameraSet(BrCarCam *cam, float fov, float far, float w, float h);
void BrViewportSet(int x, int y, int w, int h, int scissor);
void BrGridSpanExtend(int x, int y);
void BrCarPlaceWheels(int n);
int BrEntLoadModel(BrStream *s);
int BrEntIsFree(BrStream *s);
void BrEntSetRecord(BrCar *car, int kind);
void BrMenuBackdropBegin(BrCar *car);
void BrMenu3DBegin(BrCar *car);
void BrMenuCarDraw(BrCar *car);
void BrCarVisibility(BrCar *car);
void func_80230554(BrCar *car, int a);
void *BrVpAlloc(void);
Mtx *BrMtxAlloc(void);
void guLookAtReflectF(float mf[4][4], void *l, float xEye, float yEye, float zEye, float xAt, float yAt,
                      float zAt, float xUp, float yUp, float zUp);
void guScaleF(float mf[4][4], float x, float y, float z);
void guRotateF(float mf[4][4], float a, float x, float y, float z);
void guMtxCatF(float m[4][4], float n[4][4], float r[4][4]);
void guTranslateF(float mf[4][4], float x, float y, float z);
void BrModelDraw(BrModel *model, float m[4][4]);
/* -- end declarations -- */

/* WHAT IT DOES: Draw a car-select view: point the player's car and camera
 * at the view (turned by spin radians), set the viewport, and slide the
 * shown car sideways by slide squared times k.  The car kindA is drawn
 * there -- its own model once streamed in, or the model iconA under fixed
 * lights when given -- and while the view is sliding the car kindB (or
 * iconB) comes in from the other side, k further along.  The unused
 * array and the order of la among the locals are what the ROM frame holds
 * (0xE0, the two look-at pointers at 0xCC and 0xA4). */
/* @implements 0x8020C6D0 tgr BrCarViewDraw */
void BrCarViewDraw(BrModel *iconA, BrModel *iconB, int player, int x, int y, int w, int h, float slide,
                   int noBegin, int kindA, int kindB, float k, float spin)
{
  BrStream *sA;
  BrStream *sB;
  float neg;
  float *p;
  void *la;
  float deg;

  D_8028AAF0 = &D_8031B760[player];
  D_8028AAF0->mtx0[0][0] = cosf(spin);
  D_8028AAF0->mtx0[0][1] = sinf(spin);
  D_8028AAF0->mtx0[0][2] = 0.0f;
  BrVec3Cross((BrVec3 *)D_8028AAF0->mtx0[1], (BrVec3 *)D_8028AAF0->mtx0[2], (BrVec3 *)D_8028AAF0->mtx0[0]);
  D_8028AAF4 = D_8028AAF0->cam = &D_8028AAF0->cams[3];
  D_8028AAF0->heading = BrAtan2(D_8028AAF4->mtx[0][0], D_8028AAF4->mtx[0][1]);
  D_8028AAF0->heading = BrAtan2(D_8028AAF4->mtx[0][0], D_8028AAF4->mtx[0][1]);
  BrCameraSet(D_8028AAF4, D_8028AAC0, D_8028AAC8 * 0.2f, w, h);
  BrViewportSet(x, y, w, h, 1);
  BrGridSpanExtend(0, 0);
  if (slide < 0.0) {
    slide = slide * slide * -k;
  } else {
    slide = slide * slide * k;
  }
  p = D_8028AAF0->mtx0[3];
  p[0] = slide;
  p[1] = neg = -slide;
  p[2] = 0.254f;
  if (iconA == 0) {
    BrCarPlaceWheels(player);
  }
  sA = &D_8031B370[kindA];
  BrEntLoadModel(sA);
  BrMenuBackdropBegin(D_8028AAF0);
  if (noBegin == 0 && D_80272074 == 0) {
    BrMenu3DBegin(D_8028AAF0);
  }
  if (iconA) {
    la = BrVpAlloc();
    guLookAtReflectF(D_8031AB50, la, 11.0f, 11.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    gSPLookAtX(D_8028A858++, la);
    gSPLookAtY(D_8028A858++, (char *)la + 16);
    gSPNumLights(D_8028A858++, 4);
    gSPLight(D_8028A858++, D_802721F0 + 8, 1);
    gSPLight(D_8028A858++, D_802721F0 + 24, 2);
    gSPLight(D_8028A858++, D_802721F0 + 40, 3);
    gSPLight(D_8028A858++, D_802721F0 + 56, 4);
    gSPLight(D_8028A858++, D_802721F0, 5);
    BrMtxAlloc();
    guScaleF(D_8031AB10, 1.0f / 768, 1.0f / 768, 1.0f / 768);
    guRotateF(D_8031AB50, spin * 57.295776f, 0.0f, 0.0f, 1.0f);
    guMtxCatF(D_8031AB10, D_8031AB50, D_8031AB10);
    guTranslateF(D_8031AB50, slide, neg, 0);
    guMtxCatF(D_8031AB10, D_8031AB50, D_8031AB10);
    BrModelDraw(iconA, D_8031AB10);
  } else if (BrEntIsFree(sA)) {
    BrEntSetRecord(D_8028AAF0, kindA);
    BrMenuCarDraw(D_8028AAF0);
    BrCarVisibility(D_8028AAF0);
    func_80230554(D_8028AAF0, 0);
  }
  sB = &D_8031B370[kindB];
  BrEntLoadModel(sB);
  if (slide != 0.0f) {
    if (iconB) {
      void *la;
      int u0[9];

      deg = spin * 57.295776f;
      la = BrVpAlloc();
      guLookAtReflectF(D_8031AB50, la, 0.0f, -1.0f, 15.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
      gSPLookAtX(D_8028A858++, la);
      gSPLookAtY(D_8028A858++, (char *)la + 16);
      gSPNumLights(D_8028A858++, 4);
      gSPLight(D_8028A858++, D_802721F0 + 8, 1);
      gSPLight(D_8028A858++, D_802721F0 + 24, 2);
      gSPLight(D_8028A858++, D_802721F0 + 40, 3);
      gSPLight(D_8028A858++, D_802721F0 + 56, 4);
      gSPLight(D_8028A858++, D_802721F0, 5);
      BrMtxAlloc();
      guScaleF(D_8031AB10, 1.0f / 768, 1.0f / 768, 1.0f / 768);
      guRotateF(D_8031AB50, deg, 0, 0, 1.0f);
      guMtxCatF(D_8031AB10, D_8031AB50, D_8031AB10);
      if (0.0f < slide) {
        guTranslateF(D_8031AB50, slide - k, -(slide - k), 0);
      } else {
        guTranslateF(D_8031AB50, slide + k, -(slide + k), 0);
      }
      guMtxCatF(D_8031AB10, D_8031AB50, D_8031AB10);
      BrModelDraw(iconB, D_8031AB10);
    } else {
      if (BrEntIsFree(sB) || !BrEntLoadModel(sB)) {
        BrEntSetRecord(D_8028AAF0, kindB);
        if (0.0f < slide) {
          p = D_8028AAF0->mtx0[3];
          p[0] = slide - k;
          p[1] = -(slide - k);
          p[2] = 0.254f;
        } else {
          p = D_8028AAF0->mtx0[3];
          p[0] = slide + k;
          p[1] = -(slide + k);
          p[2] = 0.254f;
        }
        BrCarPlaceWheels(player);
        BrMenuCarDraw(D_8028AAF0);
        BrCarVisibility(D_8028AAF0);
        func_80230554(D_8028AAF0, 0);
        if (BrEntIsFree(sA)) {
          BrEntSetRecord(D_8028AAF0, kindA);
        }
      }
    }
  }
}

/* WHAT IT DOES: Format a time in seconds as minutes'seconds"hundredths.
 * Same arithmetic as the PC twin BrTimeFormat (br_timefmt.c). */
/* @implements 0x8020CF44 tgr BrTimeFormat */
void BrTimeFormat(char *psz, float t)
{
  int total = (int)(t * 100.0f);
  int whole = total / 100;
  int minutes;

  total -= whole * 100;
  minutes = whole / 60;
  whole -= minutes * 60;
  sprintf(psz, "%d'%02d\"%02d", minutes, whole, total);
}

/* WHAT IT DOES: Tell whether car n has a model record loaded (its entry in
 * the car model table is non-zero). */
/* @implements 0x8021D6B8 tgr BrCarModelPresent */
int BrCarModelPresent(int param_1)
{
  return *(int *)(&D_8028AE24 + param_1 * 0x60) != 0;
}

/* -- declarations: BrCarSelect -- */
#include "tgr/car.h"
#include "tgr/pad.h"
#include "tgr/menu.h"
#include "tgr/season.h"
typedef struct BrRomFile { int start; int end; void *data; } BrRomFile;
typedef struct { char raw[0xdf88]; } BrCarModelBuf;
extern BrCarModelBuf D_803C8000[];  /* each car's model buffer */
extern BrStream D_8031B370[4];  /* the two players' model streams, two apiece */
extern MenuItem *D_802720B4[];  /* the setup choices: handling, transmission, */
extern int D_802720C4;          /* tyres, suspension, decals, and their counts */
extern MenuItem *D_802720F0[];
extern int D_802720FC;
extern MenuItem *D_8027213C[];
extern int D_8027214C;
extern MenuItem *D_8027218C[];
extern int D_8027219C;
extern MenuItem *D_802721DC[];
extern int D_802721EC;
extern BrRomFile D_80272048;    /* the background */
extern BrRomFile D_8027205C;    /* the title bar */
extern BrRomFile D_80271D70;
extern BrRomFile D_80271D84;
extern char *D_80272030[];      /* engine layouts */
extern char *D_8027203C[];      /* transmission names */
extern char *D_80271FE0[];      /* places: 1st, 2nd, ... */
extern unsigned char D_802707AC[];  /* championship points per place */
extern BrTrack *D_80271D1C[];   /* the tracks */
extern MenuItem *D_802722A4[];  /* the weathers */
extern int D_80272238;          /* the screen has been set up */
extern int D_80272074;          /* the screen shows the race results */
extern int D_80272070;          /* the screen is the paint shop's car select */
extern int D_80316250;
extern int D_80316258[2];       /* each player's model slot (0-3; slot ^ 2 is the other) */
extern float D_80316260[2];     /* each player's car sliding in (-1..1, 0 at rest) */
extern float D_80316268[2];     /* each player's turntable angle */
extern MenuItem **D_80316270[2];  /* each player's current setup list */
extern int D_80316278[2];       /* the choice in it */
extern int D_80316280[2];       /* the previous choice */
extern int D_80316288[2];       /* choices in the list */
extern char *D_80316290[2];     /* the list's title */
extern int D_80316298;          /* going ahead (not back) */
extern unsigned char *D_8031629C;  /* the two sound banks for the screen */
extern int D_803162A0;
extern unsigned char *D_803162A4;
extern int D_803162A8;
extern int D_803162AC;          /* frames the title is still drawn */
extern int D_803162B0[2];       /* each player's step: 0 car, 1-4 setup, 5 decals, 6 ready */
extern int D_803162B8;          /* loading decals from the pak */
extern int D_803162BC;          /* for this player */
extern int D_803162C0;          /* the load's status */
extern unsigned char D_803162C4;  /* the load has finished */
extern int D_803162FC;          /* the season's round, race, state, the track and */
extern int D_80316300;          /* the difficulty when the results were entered */
extern int D_80316304;
extern int D_80316308;
extern int D_8031630C;
extern int D_80316310;          /* the season results have been kept */
extern int D_80316314;          /* the results page */
extern char D_80316318[];
extern BrCarModelRec D_8028AE0C[];  /* car n's record (0x60 bytes) */
extern int D_8028AE08;          /* cars */
extern int D_8028AAD4;
extern float D_8028AAD8;        /* seconds this frame */
extern int D_8028AAB0;          /* screen width */
extern int D_8028AAB4;          /* screen height */
extern int D_8028AA78;
extern int D_8028AA80;
extern int D_8028AA84;
extern int D_8028AA8C;
extern int D_8028AA98;
extern int D_8028C328;
extern int D_8028C800;
extern int D_8028B304;          /* laps */
extern int D_8028B7F4;          /* cars in the race */
extern int D_8028B940;          /* the track */
extern int D_8026FF08;          /* human players */
extern int D_8026FF18;          /* the game mode */
extern int D_802707C0;          /* lines of season news */
extern char *D_80315DB0[];      /* the lines */
extern int D_802707A8;          /* a new time-attack record */
extern int D_80270850;
extern unsigned char D_802724F4;
extern int D_80271FA0;          /* the paint shop's player */
extern BrRound D_8028B944[];
typedef struct BrRaceSnd { void *p; int x4; int x8; int xc; int x10; int x14; } BrRaceSnd;
extern BrRaceSnd D_802A4920[6];  /* six sound channels */
extern char D_802A4A08[];
extern int D_8031B2D8;          /* the cars the two views follow */
extern int D_8031B2EC;
void BrCarDefaultColour(BrCar *car);
void BrCarCamStep(int ent);
void BrCtlAi(int ctl);
void BrCarPickKind(BrCar *car);
int BrCarSelectable(int n);
void BrEntLoadRecord(BrCar *car, int slot, int kind);
void BrVec3Scale(BrVec3 *pOut, BrVec3 *pV, float s);
void BrVec3Normalise(BrVec3 *pV);
void BrVec3Cross(BrVec3 *pOut, BrVec3 *pA, BrVec3 *pB);
void *memcpy(void *dst, void *src, unsigned int n);
void BrIfaceMemReset(void);
void BrMenuLoadItems(MenuItem **items, int *count);
void BrRomFileUnpack(BrRomFile *f, void *(*alloc)(int size));
void *BrIfaceMemAlloc(int size);
int BrRomReadSize(int rom);
void BrRomUnpack(void *dst, int rom, void *s);
void BrDecalMemInit(void);
void BrClockTick(void);
void BrFadeStep(void);
void BrFrameBeginLayout1(void);
void func_802182A8(void);
void func_80218D5C(void);
void BrPerfMark(int bar, int r, int g, int b, int a);
void BrZBufferClear(void);
void func_8023DF9C(BrRomFile *f, int x, int y, int w, int h, int a, int b, int c, int d,
                   int r, int g, int bl, int e, int f2);
int BrFadeAtTarget(void);
int BrFadeIsOut(void);
int BrFadeIsIn(void);
void BrScissorSet(int x, int y, int w, int h);
void func_8020C6D0(void *iconA, void *iconB, int player, int a, int y, int w, int h, float slide,
                   int ready, int slot, int other, float dist, float angle);
void BrScreenCameraSet(float w, float h);
void BrViewportSet(int x, int y, int w, int h, int scissor);
void BrTextHighlightOff(void);
void BrTextAlignCentre(void);
void BrTextAlignLeft(void);
void BrTextAlignRight(void);
void BrTextSetColours(int a, int b, int c, int d, int e, int f);
void BrTextSetFont(int font);
void BrTextPrint(char *s, int x, int y);
void BrCarStatBarDraw(int x, int y, int w, int h, float frac);
void BrFrontPromptSelect(void);
void BrFrontPromptContinue(void);
void BrFadeBarsDraw(void);
void func_8022D97C(void);
int BrDecalPakTransfer(BrCarModelBuf *m, unsigned char port, char op, char fromMenu,
                       unsigned char *done);
void func_80244D84(int status, char op, char fromMenu);
void BrCarColourFromModel(BrCar *car, void *m);
void BrFrameEnd(void);
void BrPadStickToButtons(BrPadRec *pad);
void BrPadConsume(BrPadRec *pad, unsigned int bits);
short BrSfxFreeVoice(void);
void BrSfxVoiceStart(short v, unsigned char *start, int len, int loop);
void BrCarModelStream(BrStream *s, int slot, int car, int bufIdx);
void BrEntPaintTexture(int model, unsigned int r, unsigned int g, int b);
int BrEntLoadModel(BrStream *s);
void func_802063A4(void);
void BrSfxFadeTo(float level, float seconds);
void BrMusicFadeTo(float level, float seconds);
void BrFadeTo(float level, float seconds);
void BrScreenFlush3Layout1(void);
void BrScreenFlush2Layout0(void);
void BrSeasonPickRace(void);
void BrModeSet(int mode);
void func_80209434(void);
void func_80210FC8(void);
void BrMainMenu(void);
void BrRaceTick(void);
void func_80243260(void);
void BrLoadSaveScreen(void);
/* -- end declarations -- */

#define CAR(i) (&D_8031B760[i])
#define STREAM(slot) (&D_8031B370[slot])

/* one player's setup list: the choices, their count, the title */
#define SETUP(p, list, count, title) \
  { D_80316270[p] = (list); D_80316288[p] = (count); D_80316290[p] = (title); }

/* WHAT IT DOES: One frame of the car-select screen, which also shows the
 * race results (D_80272074) and is the paint shop's car select
 * (D_80272070).  The first frame sets it up: fades, the season's state kept
 * for the results, each player's car (its default colour, camera control,
 * a selectable kind and its model record), the turntable camera, the setup
 * lists, the artwork and the two sound banks.  Every frame turns the
 * turntables and slides the cars, draws each player's car and setup choice
 * with its title, stats or rumble hint -- or the results pages: the race's
 * place and lap times, the season news, the season's races and points --
 * runs the decal load from the Controller Pak, then reads each pad: pick a
 * car (left/right, loading its model and playing a sound), tint it with the
 * C buttons, step through the setup choices (A on, B back, START to be
 * ready), and when everyone is ready fades out to the race, the paint shop,
 * the menus, the season screens or the Controller Pak save. */
/* @t4-pass 0x8020D004 1 2026-10-03 compiles 120 best 4007 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8020D004 2 2026-10-03 compiles 121 best 4007 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x8020D004 tgr BrCarSelect */
void BrCarSelect(void)
{
  char buf[256];
  char time[36];
  int p;
  int n;
  int i;
  int k;
  int y;
  BrCar *car;
  BrPadRec *pad;
  unsigned int round;
  unsigned int race;
  unsigned int tr;
  unsigned int pl;
  int x;
  int total;
  int a;
  int back;
  float spin;
  float dt;
  char *mirror;

  if (D_80272238 == 0) {
    D_80316250 = D_8028AAD4;
    BrFadeTo(1.0f, 0.2f);
    BrSfxFadeTo(1.0f, 0.2f);
    BrMusicFadeTo(1.0f, 0.2f);
    D_803162FC = CAR(0)->season->round;
    D_80316300 = CAR(0)->season->race;
    D_80316304 = CAR(0)->season->state;
    D_8031630C = D_8028C800;
    D_80316310 = 0;
    D_8028C328 = 0;
    D_80316268[0] = 0.0f;
    D_80316268[1] = 1.0f;
    D_80316308 = D_8028B940;
    for (p = 0; p < D_8026FF08; p++) {
      if (D_80272074 == 0) {
        BrCarDefaultColour(CAR(p));
        CAR(p)->xed8 = (int)BrCarCamStep;
      }
      D_80316258[p] = p;
      D_803162B0[p] = 0;
      CAR(p)->x2000 = 0;
      *(int *)((char *)CAR(p) + 0x2044) = 0;
      D_80316260[p] = 0.0f;
      D_803162B8 = 0;
    }
    if (D_80272074 == 0) {
      for (; p < 4; p++) {
        BrCarDefaultColour(CAR(p));
        CAR(p)->xed8 = (int)BrCtlAi;
      }
    }
    D_8028B7F4 = 2;
    if (D_80272074 == 0) {
      for (p = 0; p < D_8026FF08; p++) {
        BrCarPickKind(CAR(p));
        n = CAR(p)->kind;
        CAR(p)->x2058 = n;
        while (BrCarSelectable(n) == 0) {
          n = (n + 1) % D_8028AE08;
        }
        CAR(p)->kind = n;
        CAR(p)->x2058 = n;
        BrEntLoadRecord(CAR(p), p, n);
      }
    }
    /* the two players' turntable frames and the camera that looks at them */
    CAR(0)->mtx0[3][2] = 0.254f;
    CAR(1)->mtx0[3][2] = 0.254f;
    CAR(0)->mtx0[0][0] = 1.0f;
    CAR(0)->mtx0[1][1] = 1.0f;
    CAR(0)->mtx0[2][2] = 1.0f;
    CAR(1)->mtx0[0][0] = 1.0f;
    CAR(1)->mtx0[1][1] = 1.0f;
    CAR(1)->mtx0[2][2] = 1.0f;
    if (D_8026FF08 == 2 && D_80272074 == 0) {
      CAR(0)->cams[3].mtx[3][0] = 15.0f;
      CAR(0)->cams[3].mtx[3][1] = 15.0f;
      CAR(0)->cams[3].mtx[3][2] = 7.5f;
    } else {
      CAR(0)->cams[3].mtx[3][0] = 10.0f;
      CAR(0)->cams[3].mtx[3][1] = 10.0f;
      CAR(0)->cams[3].mtx[3][2] = 5.0f;
    }
    CAR(0)->mtx0[0][1] = 0.0f;
    CAR(0)->mtx0[0][2] = 0.0f;
    CAR(0)->mtx0[1][0] = 0.0f;
    CAR(0)->mtx0[1][2] = 0.0f;
    CAR(0)->mtx0[2][0] = 0.0f;
    CAR(0)->mtx0[2][1] = 0.0f;
    CAR(0)->mtx0[3][0] = 0.0f;
    CAR(0)->mtx0[3][1] = 0.0f;
    CAR(1)->mtx0[0][1] = 0.0f;
    CAR(1)->mtx0[0][2] = 0.0f;
    CAR(1)->mtx0[1][0] = 0.0f;
    CAR(1)->mtx0[1][2] = 0.0f;
    CAR(1)->mtx0[2][0] = 0.0f;
    CAR(1)->mtx0[2][1] = 0.0f;
    CAR(1)->mtx0[3][0] = 0.0f;
    CAR(1)->mtx0[3][1] = 0.0f;
    BrVec3Scale((BrVec3 *)CAR(0)->cams[3].mtx[0], (BrVec3 *)CAR(0)->cams[3].mtx[3], -1.0f);
    BrVec3Normalise((BrVec3 *)CAR(0)->cams[3].mtx[0]);
    CAR(0)->cams[3].mtx[2][2] = 1.0f;
    CAR(0)->cams[3].mtx[2][0] = 0.0f;
    CAR(0)->cams[3].mtx[2][1] = 0.0f;
    BrVec3Cross((BrVec3 *)CAR(0)->cams[3].mtx[1], (BrVec3 *)CAR(0)->cams[3].mtx[2],
                (BrVec3 *)CAR(0)->cams[3].mtx[0]);
    BrVec3Cross((BrVec3 *)CAR(0)->cams[3].mtx[2], (BrVec3 *)CAR(0)->cams[3].mtx[0],
                (BrVec3 *)CAR(0)->cams[3].mtx[1]);
    memcpy(&CAR(1)->cams[3], &CAR(0)->cams[3], 0x44);
    BrIfaceMemReset();
    BrMenuLoadItems(D_802720B4, &D_802720C4);
    BrMenuLoadItems(D_802720F0, &D_802720FC);
    BrMenuLoadItems(D_8027213C, &D_8027214C);
    BrMenuLoadItems(D_8027218C, &D_8027219C);
    BrMenuLoadItems(D_802721DC, &D_802721EC);
    BrRomFileUnpack(&D_80272048, BrIfaceMemAlloc);
    BrRomFileUnpack(&D_80271D70, BrIfaceMemAlloc);
    BrRomFileUnpack(&D_80271D84, BrIfaceMemAlloc);
    BrRomFileUnpack(&D_8027205C, BrIfaceMemAlloc);
    D_803162A0 = BrRomReadSize(0x1BF480);
    D_8031629C = BrIfaceMemAlloc(D_803162A0 + 0x100);
    BrRomUnpack(D_8031629C, 0x1BF480, 0);
    D_803162A8 = BrRomReadSize(0x1BFEC0);
    D_803162A4 = BrIfaceMemAlloc(D_803162A8 + 0x100);
    BrRomUnpack(D_803162A4, 0x1BFEC0, 0);
    for (i = 0; i != 0x100; i++) {
      D_8031629C[D_803162A0 + i] = 0;
      D_803162A4[D_803162A8 + i] = 0;
    }
    BrDecalMemInit();
    D_803162AC = 2;
    D_80272238 = 1;
    D_80316270[1] = D_802720B4;
    D_80316270[0] = D_802720B4;
    D_80316278[1] = 0;
    D_80316278[0] = 0;
    D_80316314 = 0;
  }

  /* ---- every frame: the turntables ---- */
  BrClockTick();
  BrFadeStep();
  BrFrameBeginLayout1();
  D_8028AA78 = 0;
  D_8028AA80 = 0;
  D_8028AA8C = 0;
  D_8028AA84 = 0;
  func_802182A8();
  func_80218D5C();
  BrPerfMark(0, 0, 0, 200, 0xFF);
  D_8031B2D8 = 0;
  D_8031B2EC = 1;
  D_803162AC = 2;
  BrZBufferClear();
  if (D_80272074 == 0) {
    func_8023DF9C(&D_80272048, 0, 0, 320, 240, 0, 0, 0, 0xFF, 0x50, 0, 0, 0xFF, 4);
  } else {
    func_8023DF9C(&D_80272048, 0, 0, 320, 240, 0, 0, 0, 0xFF, 0, 0x82, 0x8C, 0xFF, 4);
  }
  if (D_803162AC == 0 && BrFadeAtTarget() != 0) {
    BrScissorSet(0, 0, D_8028AAB0, D_8028AAB4);
  }
  spin = 6.2831855f;
  dt = D_8028AAD8;
  D_80316268[0] += D_8028AAD8;
  D_80316268[1] += 6.2831855f - D_8028AAD8;
  for (p = 0; p < D_8026FF08; p++) {
    while (spin < D_80316268[p]) {
      D_80316268[p] -= spin;
    }
  }
  for (p = 0; p < D_8026FF08; p++) {
    if (D_80316260[p] < 0.0f) {
      D_80316260[p] = D_80316260[p] + (dt + dt);
      if (0.0f < D_80316260[p]) {
        D_80316260[p] = 0.0f;
      }
    } else if (0.0f < D_80316260[p]) {
      D_80316260[p] = D_80316260[p] - (dt + dt);
      if (D_80316260[p] < 0.0f) {
        D_80316260[p] = 0.0f;
      }
    }
  }
  /* which model slot each player shows */
  for (p = 0; p < D_8026FF08; p++) {
    if (D_803162B0[p] == 6) {
      if (D_80272070 == 0 && D_80272074 == 0) {
        D_80316258[p] = (CAR(p)->xe58 << 1) ^ p ^ 2;
      }
    } else if (D_803162B0[p] == 5) {
      if (D_80316278[p] == 0) {
        D_80316258[p] = p ^ 2;
      } else if (D_80316278[p] == 2) {
        D_80316258[p] = p;
      } else if (D_80316278[p] == 1) {
        if (D_80316280[p] == 0) {
          D_80316258[p] = p;
        } else {
          D_80316258[p] = p ^ 2;
        }
      }
    }
  }
  D_8028AA84 = 0;
  D_8028AA78 = 0;
  D_8028AA8C = 0;
  D_8028AA80 = 0;
#define ICON(p, idx) ((D_803162B0[p] == 0 || D_803162B0[p] == 6) ? 0 : D_80316270[p][D_80316278[idx]]->icon)
#define ICON2(p, idx) ((D_803162B0[p] == 0 || D_803162B0[p] == 6) ? 0 : D_80316270[p][D_80316280[idx]]->icon)
  if (D_80272074 == 0) {
    if (D_8026FF08 == 1) {
      func_8020C6D0(ICON(0, 0), ICON2(0, 0), 0, 0, 0x2D, D_8028AAB0, D_8028AAB4, D_80316260[0],
                    D_803162B0[0] == 6, D_80316258[0], D_80316258[0] ^ 2, 8.0f, D_80316268[0]);
    } else {
      func_8020C6D0(ICON(0, 0), ICON2(0, 0), 0, 0, 0, D_8028AAB0, D_8028AAB4 - 24, D_80316260[0],
                    D_803162B0[0] == 6, D_80316258[0], D_80316258[0] ^ 2, 12.0f, D_80316268[0]);
      func_8020C6D0(ICON(1, 1), ICON2(1, 1), 1, 0, 0x47, D_8028AAB0, D_8028AAB4, D_80316260[1],
                    D_803162B0[1] == 6, D_80316258[1], D_80316258[1] ^ 2, 12.0f, D_80316268[1]);
    }
  } else {
    func_8020C6D0(ICON(0, 0), ICON2(0, 0), D_80316314 == 1, 0, 0x3B, D_8028AAB0, D_8028AAB4,
                  D_80316260[0], D_803162B0[0] == 6, D_80316258[D_80316314 == 1],
                  D_80316258[D_80316314 == 1] ^ 2, 12.0f, D_80316268[0]);
  }
  BrScreenCameraSet((float)D_8028AAB0, (float)D_8028AAB4);
  BrViewportSet(0, 0, D_8028AAB0, D_8028AAB4, 1);

  /* ---- the title ---- */
  if (D_803162AC != 0 || BrFadeAtTarget() == 0) {
    func_8023DF9C(&D_8027205C, 0x16, 9, 0x118, 0x2A, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 2);
    BrTextHighlightOff();
    BrTextAlignCentre();
    BrTextSetFont(30);
    if (D_80272074 == 0) {
      if (D_80272070 == 0) {
        BrTextPrint("%ryCar Select", D_8028AAB0 / 2, D_8028AAB4 / 6 - 2);
      } else {
        BrTextPrint("%ryPaint car select", D_8028AAB0 / 2, D_8028AAB4 / 6 - 2);
      }
    } else if (D_80316314 == 0 || D_80316314 == 1) {
      if (D_80316308 < 5) {
        mirror = "";
        tr = D_80316308;
      } else {
        mirror = "M-";
        tr = D_80316308 - 5;
      }
      sprintf(buf, "%%ry%s%s/%s", mirror, D_80271D1C[tr]->item.label,
              D_802722A4[D_8031630C]->label);
      BrTextPrint(buf, D_8028AAB0 / 2, D_8028AAB4 / 6 - 2);
    } else {
      sprintf(buf, "%%ry%s", *(char **)&D_8028B944[D_803162FC]);
      BrTextPrint(buf, D_8028AAB0 / 2, D_8028AAB4 / 6 - 2);
    }
    if (BrFadeAtTarget() != 0) {
      D_803162AC--;
    }
  }

  /* ---- each player's panel, or the results ---- */
  race = D_80316300;
  round = D_803162FC;
  if (D_80272074 == 0) {
    if (D_8026FF08 == 2) {
      BrTextSetFont(17);
      if (D_803162B0[0] == 0) {
        sprintf(buf, "%%ry%%i%s", *(char **)&D_8028AE0C[CAR(0)->x2058]);
      } else if (D_803162B0[0] == 6) {
        sprintf(buf, "%%ryREADY!");
      } else {
        sprintf(buf, "%%ry%s: %%i%s", D_80316290[0], D_80316270[0][D_80316278[0]]->label);
      }
      BrTextPrint(buf, D_8028AAB0 / 2, D_8028AAB4 / 4);
      if (D_803162B0[1] == 0) {
        sprintf(buf, "%%ry%%i%s", *(char **)&D_8028AE0C[CAR(1)->x2058]);
      } else if (D_803162B0[1] == 6) {
        sprintf(buf, "%%ryREADY!");
      } else {
        sprintf(buf, "%%ry%s: %%i%s", D_80316290[1], D_80316270[1][D_80316278[1]]->label);
      }
      BrTextPrint(buf, D_8028AAB0 / 2, D_8028AAB4 * 3 / 5);
      BrTextSetFont(8);
      BrTextAlignLeft();
      if (D_803162B0[0] == 5 && D_80316278[0] != 1 && D_80316260[0] == 0.0f) {
        BrTextPrint("%wwIf you wish to use", D_8028AAB0 * 11 / 16, D_8028AAB4 / 4 + 19);
        BrTextPrint("%wwthe Rumble Pak, you", D_8028AAB0 * 11 / 16, D_8028AAB4 / 4 + 28);
        BrTextPrint("%wwshould ensure that it", D_8028AAB0 * 11 / 16, D_8028AAB4 / 4 + 37);
        BrTextPrint("%wwis plugged in before", D_8028AAB0 * 11 / 16, D_8028AAB4 / 4 + 45);
        BrTextPrint("%wwpressing A.", D_8028AAB0 * 11 / 16, D_8028AAB4 / 4 + 54);
      }
      if (D_803162B0[1] == 5 && D_80316278[1] != 1 && D_80316260[1] == 0.0f) {
        BrTextPrint("%wwIf you wish to use", D_8028AAB0 * 11 / 16, D_8028AAB4 * 3 / 5 + 19);
        BrTextPrint("%wwthe Rumble Pak, you", D_8028AAB0 * 11 / 16, D_8028AAB4 * 3 / 5 + 28);
        BrTextPrint("%wwshould ensure that it", D_8028AAB0 * 11 / 16, D_8028AAB4 * 3 / 5 + 37);
        BrTextPrint("%wwis plugged in before", D_8028AAB0 * 11 / 16, D_8028AAB4 * 3 / 5 + 45);
        BrTextPrint("%wwpressing A.", D_8028AAB0 * 11 / 16, D_8028AAB4 * 3 / 5 + 54);
      }
      BrTextAlignRight();
      if (D_803162B0[0] == 0) {
        car = CAR(0);
        BrTextAlignRight();
        BrTextPrint("%wwAcceleration", D_8028AAB0 / 4 - 5, D_8028AAB4 / 4 + 2);
        BrTextPrint("%wwMax Speed", D_8028AAB0 / 4 - 5, D_8028AAB4 / 4 - 6);
        BrTextAlignCentre();
        BrTextPrint(D_80272030[*(int *)((char *)&D_8028AE0C[car->x2058] + 0x40)],
                    D_8028AAB0 * 3 / 4, D_8028AAB4 / 4 + 2);
        sprintf(buf, "%%ww%d-Speed %s", *(int *)((char *)&D_8028AE0C[car->x2058] + 0x54),
                D_8027203C[*(int *)((char *)&D_8028AE0C[car->x2058] + 0x3C)]);
        BrTextPrint(buf, D_8028AAB0 * 3 / 4, D_8028AAB4 / 4 - 6);
        BrCarStatBarDraw(D_8028AAB0 / 2, D_8028AAB4 / 2 - 23, 80, 11,
                         *(float *)((char *)&D_8028AE0C[car->x2058] + 0x44) / 500.0);
        BrCarStatBarDraw(D_8028AAB0 / 2, D_8028AAB4 / 2 - 6, 80, 11,
                         *(float *)((char *)&D_8028AE0C[car->x2058] + 0x48) / 10.0);
      }
      if (D_803162B0[1] == 0) {
        car = CAR(1);
        BrTextAlignRight();
        BrTextPrint("%wwAcceleration", D_8028AAB0 / 4 - 5, D_8028AAB4 * 3 / 5 + 2);
        BrTextPrint("%wwMax Speed", D_8028AAB0 / 4 - 5, D_8028AAB4 * 3 / 5 - 6);
        BrTextAlignCentre();
        BrTextPrint(D_80272030[*(int *)((char *)&D_8028AE0C[car->x2058] + 0x40)],
                    D_8028AAB0 * 3 / 4, D_8028AAB4 * 3 / 5 + 2);
        sprintf(buf, "%%ww%d-Speed %s", *(int *)((char *)&D_8028AE0C[car->x2058] + 0x54),
                D_8027203C[*(int *)((char *)&D_8028AE0C[car->x2058] + 0x3C)]);
        BrTextPrint(buf, D_8028AAB0 * 3 / 4, D_8028AAB4 * 3 / 5 - 6);
        BrCarStatBarDraw(D_8028AAB0 / 2, D_8028AAB4 * 6 / 5 - 23, 80, 11,
                         *(float *)((char *)&D_8028AE0C[car->x2058] + 0x44) / 500.0);
        BrCarStatBarDraw(D_8028AAB0 / 2, D_8028AAB4 * 6 / 5 - 6, 80, 11,
                         *(float *)((char *)&D_8028AE0C[car->x2058] + 0x48) / 10.0);
      }
    } else {
      BrTextSetFont(20);
      if (D_80272070 == 0) {
        if (D_803162B0[0] == 0) {
          sprintf(buf, "%%ry%%i%s", *(char **)&D_8028AE0C[CAR(0)->x2058]);
        } else if (D_803162B0[0] == 6) {
          sprintf(buf, "%%ryLET'S GO!");
        } else {
          sprintf(buf, "%%ry%s: %%i%s", D_80316290[0], D_80316270[0][D_80316278[0]]->label);
        }
        BrTextPrint(buf, D_8028AAB0 / 2, D_8028AAB4 * 20 / 64);
        BrTextSetFont(8);
        BrTextAlignCentre();
        if (D_803162B0[0] == 5 && D_80316278[0] != 1 && D_80316260[0] == 0.0f) {
          BrTextPrint("%wwIf you wish to use the Rumble Pak, you should", D_8028AAB0 / 2,
                      D_8028AAB4 * 3 / 8 - 6);
          BrTextPrint("%wwensure that it is plugged in before pressing A.", D_8028AAB0 / 2,
                      D_8028AAB4 * 3 / 8 + 2);
        }
        if (D_803162B0[0] == 0) {
          car = CAR(0);
          BrTextAlignRight();
          BrTextSetFont(8);
          BrTextPrint("%wwMax Speed", D_8028AAB0 * 3 / 8, D_8028AAB4 * 3 / 8 - 6);
          BrTextPrint("%wwAcceleration", D_8028AAB0 * 3 / 8, D_8028AAB4 * 3 / 8 + 2);
          BrTextAlignCentre();
          BrTextPrint(D_80272030[*(int *)((char *)&D_8028AE0C[car->x2058] + 0x40)],
                      D_8028AAB0 * 11 / 16, D_8028AAB4 * 3 / 8 - 6);
          sprintf(buf, "%%ww%d-Speed %s", *(int *)((char *)&D_8028AE0C[car->x2058] + 0x54),
                  D_8027203C[*(int *)((char *)&D_8028AE0C[car->x2058] + 0x3C)]);
          BrTextPrint(buf, D_8028AAB0 * 11 / 16, D_8028AAB4 * 3 / 8 + 2);
          BrCarStatBarDraw(D_8028AAB0 * 3 / 4 + 10, D_8028AAB4 * 3 / 4 - 23, 80, 11,
                           *(float *)((char *)&D_8028AE0C[car->x2058] + 0x44) / 500.0);
          BrCarStatBarDraw(D_8028AAB0 * 3 / 4 + 10, D_8028AAB4 * 3 / 4 - 6, 80, 11,
                           *(float *)((char *)&D_8028AE0C[car->x2058] + 0x48) / 10.0);
        }
      } else {
        sprintf(buf, "%%ry%%i%s", *(char **)&D_8028AE0C[CAR(0)->x2058]);
        BrTextPrint(buf, D_8028AAB0 / 2, D_8028AAB4 * 22 / 64);
      }
    }
  } else {
    y = 0x4F;
    i = 0;
    BrTextHighlightOff();
    BrTextAlignCentre();
    BrTextSetFont(20);
    if (D_80316314 == 0 || D_80316314 == 1) {
      /* a player's race: the place, then each lap and the race time */
      pl = D_80316314 != 0;
      if (D_8026FF08 < 2) {
        if (D_8026FF18 == 0) {
          car = CAR(pl);
          k = *(unsigned char *)((char *)CAR(pl)->season + round * 4 + race + 6);
          sprintf(D_80316318, "%%ry%s Place - %d Point%s", D_80271FE0[k], D_802707AC[k],
                  D_802707AC[k] == 1 ? "" : "s");
        } else {
          car = CAR(pl);
          if (car->laps < D_8028B304) {
            sprintf(D_80316318, "%%ryDid not finish");
          } else {
            sprintf(D_80316318, "%%ry%s Place", D_80271FE0[car->xfac]);
          }
        }
      } else {
        car = CAR(pl);
        if (car->laps < D_8028B304) {
          sprintf(D_80316318, "%%ryP%d: Did not finish", pl + 1);
        } else {
          sprintf(D_80316318, "%%ryP%d: %s Place", pl + 1, D_80271FE0[car->xfac]);
        }
      }
      BrTextPrint(D_80316318, D_8028AAB0 / 2, D_8028AAB4 * 19 / 64 - 4);
      BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);
      BrTextSetFont(10);
      for (i = 0; i <= D_8028B304; i++) {
        BrTextAlignLeft();
        if (i == D_8028B304) {
          y += 2;
          if (car->lapTime == CAR(0)->season->xe8[D_80316308]) {
            BrTextPrint("  %yyRECORD TIME!", D_8028AAB0 * 11 / 16, y);
          }
          sprintf(D_80316318, "Race time:");
          BrTimeFormat(time, car->lapTime);
        } else {
          sprintf(D_80316318, "Lap %d time:", i + 1);
          if (i < car->laps) {
            BrTimeFormat(time, car->lapTimes[i]);
          } else {
            sprintf(time, "--    ");
          }
        }
        BrTextPrint(D_80316318, D_8028AAB0 * 5 / 16, y);
        if (i < car->laps && i == car->xf9c) {
          if (car->xf98 == CAR(0)->season->x8c[D_80316308]) {
            BrTextPrint("  %yyRECORD LAP!", D_8028AAB0 * 11 / 16, y);
          } else {
            BrTextPrint("  %yyBEST LAP!", D_8028AAB0 * 11 / 16, y);
          }
        }
        BrTextAlignRight();
        BrTextPrint(time, D_8028AAB0 * 11 / 16, y);
        y += 10;
      }
    } else if (D_80316314 == 3) {
      /* the season news */
      BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);
      BrTextSetFont(12);
      y = 0x62 - D_802707C0 * 120 / 20;
      BrTextAlignCentre();
      for (i = 0; i < D_802707C0; i++) {
        BrTextPrint(D_80315DB0[i], D_8028AAB0 / 2, y);
        y += 12;
      }
    } else {
      /* the season so far: each race, its time, place and points */
      BrTextPrint("%rySeason Results", D_8028AAB0 / 2, D_8028AAB4 * 19 / 64 - 4);
      BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);
      BrTextSetFont(10);
      x = 0;
      for (i = 0; i < D_8028B944[round].x8; i++) {
        if (D_8028B944[round].races[i][0] < 5) {
          if (D_80316304 & 1) {
            x = D_8028AAB0 / 16;
            break;
          }
        } else if ((D_80316304 & 1) == 0) {
          x = D_8028AAB0 / 16;
          break;
        }
      }
      total = 0;
      for (i = 0; i < D_8028B944[round].x8; i++) {
        tr = D_8028B944[round].races[i][0];
        if (D_80316304 & 1) {
          if (tr < 5) {
            tr += 5;
          } else {
            tr -= 5;
          }
        }
        BrTextAlignLeft();
        sprintf(D_80316318, "%s/%s", D_80271D1C[tr]->item.label,
                D_802722A4[D_8028B944[round].races[i][1]]->label);
        BrTextPrint(D_80316318, D_8028AAB0 * 3 / 16 - x, y);
        BrTextAlignCentre();
        if ((int)race < i) {
          sprintf(D_80316318, "--");
        } else {
          BrTimeFormat(D_80316318, ((float *)CAR(0)->season)[round * 4 + i + 0xB]);
        }
        BrTextPrint(D_80316318, D_8028AAB0 * 17 / 32 + x, y);
        BrTextPrint((int)race < i ? "--"
                                  : D_80271FE0[*(unsigned char *)((char *)CAR(0)->season + i
                                                                 + round * 4 + 6)],
                    D_8028AAB0 * 21 / 32 + x, y);
        BrTextAlignRight();
        if (i <= (int)race) {
          k = D_802707AC[*(unsigned char *)((char *)CAR(0)->season + i + round * 4 + 6)];
          sprintf(D_80316318, "%d PTS", k);
          total += k;
        }
        BrTextPrint(D_80316318, D_8028AAB0 * 13 / 16 + x, y);
        y += 10;
      }
      sprintf(D_80316318, "TOTAL:    %d PTS ", total);
      BrTextPrint(D_80316318, D_8028AAB0 * 13 / 16 + x, y + 2);
    }
  }
  if (D_803162AC != 0 || BrFadeIsOut() != 0 || BrFadeIsIn() != 0) {
    if (D_80272074 == 0) {
      BrFrontPromptSelect();
    } else {
      BrFrontPromptContinue();
    }
  }
  BrFadeBarsDraw();
  if (1 < D_8028AA98) {
    BrViewportSet(0, 0, D_8028AAB0, D_8028AAB4, 1);
    func_8022D97C();
  }

  /* ---- the decal load from the Controller Pak ---- */
  if (D_803162B8 != 0) {
    if (D_803162C0 == 0) {
      if (D_803162C4 == 0) {
        D_803162C0 = BrDecalPakTransfer(&D_803C8000[D_803162BC], D_803162BC, 9, 1, &D_803162C4);
      }
    } else if (D_803162C0 == 999) {
      D_803162C4 = 1;
    } else {
      func_80244D84(D_803162C0, 9, 1);
      if (((BrPadRec *)CAR(D_803162BC)->pad)->pressed & 0x30) {
        BrPadConsume((BrPadRec *)CAR(D_803162BC)->pad, 0x30);
        D_803162C4 = 1;
      }
    }
    if (D_803162C4 != 0) {
      if (D_803162C0 == 0) {
        D_80316288[D_803162BC] = D_802721EC;
        D_80316280[D_803162BC] = D_80316278[D_803162BC];
        D_80316278[D_803162BC] = (D_80316278[D_803162BC] + 1) % D_802721EC;
        D_80316260[D_803162BC] = 1.0f;
        BrCarColourFromModel(CAR(D_803162BC), &D_803C8000[D_803162BC]);
      } else {
        D_80316288[D_803162BC] = D_802721EC - 1;
        D_803162C0 = 0;
      }
    }
  }
  BrFrameEnd();

  /* ---- the pads ---- */
  if (BrFadeIsOut() == 0) {
    if (D_803162B8 == 0) {
      if (D_80272070 != 0) {
        /* the paint shop's player drives the screen */
        ((BrPadRec *)CAR(0)->pad)->pressed = ((BrPadRec *)CAR(D_80271FA0)->pad)->pressed;
        ((BrPadRec *)CAR(0)->pad)->axis[0] = ((BrPadRec *)CAR(D_80271FA0)->pad)->axis[0];
        ((BrPadRec *)CAR(0)->pad)->axis[1] = ((BrPadRec *)CAR(D_80271FA0)->pad)->axis[1];
      }
      for (p = 0; p < D_8026FF08; p++) {
        car = CAR(p);
        back = 1;
        if (D_80272074 == 0) {
          BrPadStickToButtons((BrPadRec *)car->pad);
        } else {
          ((BrPadRec *)car->pad)->pressed &= ~0xF;
        }
        pad = (BrPadRec *)car->pad;
        if (D_803162B0[p] == 6) {
          if (pad->pressed & 0x20) {
            D_803162B0[p] = 5;
            BrPadConsume(pad, 0x20);
          }
          continue;
        }
        if (D_80316260[p] != 0.0f) {
          continue;
        }
        /* the car (step 0): left and right pick the next selectable car */
        if (D_803162B0[p] == 0) {
          if (pad->pressed & 4) {
            n = car->x2058;
            do {
              car->x2058 = (car->x2058 + D_8028AE08 - 1) % D_8028AE08;
            } while (BrCarSelectable(car->x2058) == 0);
            if (n != car->x2058) {
              D_80316260[p] = 1.0f;
              goto newcar;
            }
            pad = (BrPadRec *)car->pad;
          }
          if (pad->pressed & 1) {
            n = car->x2058;
            do {
              car->x2058 = (car->x2058 + 1) % D_8028AE08;
            } while (BrCarSelectable(car->x2058) == 0);
            if (n != car->x2058) {
              D_80316260[p] = -1.0f;
            newcar:
              k = BrSfxFreeVoice();
              if (k != -1) {
                BrSfxVoiceStart((short)k, D_8031629C, D_803162A0, 0);
              }
              D_80316258[p] ^= 2;
              BrCarModelStream(STREAM(D_80316258[p]), D_80316258[p], car->x2058, p);
            }
          }
        }
        /* the colour, on the car and on the decals */
        if (D_803162B0[p] == 0 || D_803162B0[p] == 5) {
          if (((BrPadRec *)car->pad)->pressed & 0x400) {
            BrPadConsume((BrPadRec *)car->pad, 0x400);
            if (car->season->unlocked & 0x8000) {
              car->x2068 ^= 1;
            }
          }
          a = ((BrPadRec *)car->pad)->pressed;
          if (a & 8) {
            if (a & 0x800) {
              if (car->colour[0] < 0xFC) {
                car->colour[0] += 4;
              } else {
                car->colour[0] = 0xFF;
              }
              a = ((BrPadRec *)car->pad)->pressed;
            }
            if (a & 0x100) {
              if (car->colour[1] < 0xFC) {
                car->colour[1] += 4;
              } else {
                car->colour[1] = 0xFF;
              }
              a = ((BrPadRec *)car->pad)->pressed;
            }
            if (a & 0x200) {
              if (car->colour[2] < 0xFC) {
                car->colour[2] += 4;
              } else {
                car->colour[2] = 0xFF;
              }
            }
            BrEntPaintTexture((int)car->model, car->colour[0] >> 3, car->colour[1] >> 3,
                              car->colour[2] >> 3);
            a = ((BrPadRec *)car->pad)->pressed;
          }
          if (a & 2) {
            if (a & 0x800) {
              if (car->colour[0] < 4) {
                car->colour[0] = 0;
              } else {
                car->colour[0] -= 4;
              }
              a = ((BrPadRec *)car->pad)->pressed;
            }
            if (a & 0x100) {
              if (car->colour[1] < 4) {
                car->colour[1] = 0;
              } else {
                car->colour[1] -= 4;
              }
              a = ((BrPadRec *)car->pad)->pressed;
            }
            if (a & 0x200) {
              if (car->colour[2] < 4) {
                car->colour[2] = 0;
              } else {
                car->colour[2] -= 4;
              }
            }
            BrEntPaintTexture((int)car->model, car->colour[0] >> 3, car->colour[1] >> 3,
                              car->colour[2] >> 3);
          }
        }
        /* the setup choice (steps 1-5): left and right */
        pad = (BrPadRec *)car->pad;
        if (D_803162B0[p] != 0) {
          if (pad->pressed & 4) {
            D_80316280[p] = D_80316278[p];
            D_80316278[p] = (D_80316278[p] + D_80316288[p] - 1) % D_80316288[p];
            D_80316260[p] = 1.0f;
            goto tick;
          }
          if (pad->pressed & 1) {
            D_80316280[p] = D_80316278[p];
            D_80316278[p] = (D_80316278[p] + 1) % D_80316288[p];
            D_80316260[p] = -1.0f;
          tick:
            k = BrSfxFreeVoice();
            if (k != -1) {
              BrSfxVoiceStart((short)k, D_8031629C, D_803162A0, 0);
            }
            pad = (BrPadRec *)car->pad;
          }
        }
        if (pad->pressed & 0x20) {
          /* back */
          BrPadConsume(pad, 0x20);
          n = D_803162B0[p];
          if (n == 0) {
            D_80316298 = 0;
            BrSfxFadeTo(0.0f, 0.2f);
            BrFadeTo(0.0f, 0.2f);
            back = 1;
          } else {
            D_803162B0[p] = n - 1;
            switch (n) {
            case 2:
              D_80316278[p] = car->season->xd4;
              SETUP(p, D_802720B4, D_802720C4, "Handling");
              break;
            case 3:
              D_80316278[p] = car->season->xd8;
              SETUP(p, D_802720F0, D_802720FC, "Transmission");
              break;
            case 4:
              D_80316278[p] = car->season->xdc;
              SETUP(p, D_8027213C, D_8027214C, "Tires");
              break;
            case 5:
              D_80316278[p] = car->season->xe0;
              SETUP(p, D_8027218C, D_8027219C, "Suspension");
              break;
            case 6:
              D_80316278[p] = 0;
              D_80316290[p] = "Decals";
              break;
            }
            D_80316280[p] = D_80316278[p];
          }
        } else if (pad->pressed & 0xC010) {
          /* on to the next step */
          BrPadConsume(pad, 0xC010);
          switch (D_803162B0[p]) {
          case 0:
            car->kind = car->x2058;
            D_80316278[p] = car->season->xd4;
            SETUP(p, D_802720B4, D_802720C4, "Handling");
            break;
          case 1:
            car->season->xd4 = D_80316278[p];
            D_80316278[p] = car->season->xd8;
            SETUP(p, D_802720F0, D_802720FC, "Transmission");
            break;
          case 2:
            car->season->xd8 = D_80316278[p];
            D_80316278[p] = car->season->xdc;
            SETUP(p, D_8027213C, D_8027214C, "Tires");
            break;
          case 3:
            car->season->xdc = D_80316278[p];
            D_80316278[p] = car->season->xe0;
            SETUP(p, D_8027218C, D_8027219C, "Suspension");
            break;
          case 4:
            car->season->xe0 = D_80316278[p];
            D_80316278[p] = 0;
            D_80316270[p] = D_802721DC;
            if (car->kind < 9) {
              D_80316288[p] = D_802721EC - 1;
            } else {
              D_80316288[p] = D_802721EC - 2;
            }
            D_80316290[p] = "Decals";
            D_80316258[p] = p ^ 2;
            k = BrSfxFreeVoice();
            if (k != -1) {
              BrSfxVoiceStart((short)k, D_803162A4, D_803162A8, 0);
            }
            back = 0;
            BrCarModelStream(STREAM(D_80316258[p]), D_80316258[p], car->kind, p);
            while (BrEntLoadModel(STREAM(D_80316258[p])) != 0) {
            }
            BrCarModelStream(STREAM(D_80316258[p] ^ 2), D_80316258[p] ^ 2, car->kind, p);
            break;
          case 5:
            if (D_80316278[p] == 1) {
              D_803162B8 = 1;
              D_803162C4 = 0;
              D_803162C0 = 0;
              D_803162BC = p;
              goto sound;
            }
            car->xe58 = D_80316278[p] == 2;
            break;
          }
          D_80316280[p] = D_80316278[p];
          pad = (BrPadRec *)car->pad;
          if ((pad->pressed & 0x4000) == 0 && D_80272070 == 0 && D_80272074 == 0) {
            D_803162B0[p]++;
          } else {
            /* START: straight to ready (or the next results page) */
            D_80316288[p] = D_802721EC - 1;
            D_80316278[p] = 0;
            D_80316280[p] = 0;
            n = D_80316314;
            if (D_80272074 == 0) {
              D_803162B0[p] = 6;
            } else {
              k = D_80316314 + 1;
              if (k == 1 && D_8026FF08 != 2) {
                k = D_80316314 + 2;
              }
              if (k == 2 && D_8026FF18 != 0) {
                k = 4;
              }
              D_80316314 = k;
              if (k == 3) {
                func_802063A4();
                D_80316310 = 1;
                if (D_802707C0 == 0) {
                  D_80316314++;
                }
              }
              if (D_80316314 == 4) {
                D_803162B0[1] = 6;
                D_803162B0[0] = 6;
                D_80316314 = n;
              }
            }
          }
          for (i = 0; i < D_8026FF08; i++) {
            if (D_803162B0[i] != 6) {
              goto sound;
            }
          }
          D_80316298 = 1;
          BrSfxFadeTo(0.0f, 0.2f);
          if (D_80272070 == 0) {
            BrMusicFadeTo(0.0f, 0.2f);
          }
          BrFadeTo(0.0f, 0.2f);
        } else {
          continue;
        }
      sound:
        if (back) {
          k = BrSfxFreeVoice();
          if (k != -1) {
            BrSfxVoiceStart((short)k, D_803162A4, D_803162A8, 0);
          }
        }
      }
    } else if (D_803162C4 != 0) {
      D_803162C4 = 0;
      D_803162B8 = 0;
    }
  } else if (BrFadeAtTarget() != 0) {
    /* faded out: on to the next screen */
    D_80272238 = 0;
    BrScreenFlush3Layout1();
    for (i = 0; i < 6; i++) {
      D_802A4920[i].x14 = 0;
      D_802A4920[i].x10 = 0;
      D_802A4920[i].x8 = 0;
      D_802A4920[i].xc = 0;
      D_802A4920[i].p = D_802A4A08;
    }
    if (D_80316298 == 0) {
      if (D_80272074 == 0) {
        if (D_80272070 == 0) {
          if (D_8026FF18 == 0 || D_8026FF18 == 2) {
            D_80270850 = 1;
            BrModeSet((int)func_80209434);
          } else {
            BrModeSet((int)func_80210FC8);
          }
        } else {
          BrModeSet((int)BrMainMenu);
        }
      } else {
        if (D_8026FF18 == 0 && D_80316310 == 0) {
          func_802063A4();
        }
        BrModeSet((int)BrMainMenu);
      }
    } else if (D_80272074 == 0) {
      if (D_80272070 == 0) {
        BrScreenFlush2Layout0();
        BrModeSet((int)BrRaceTick);
      } else {
        BrModeSet((int)func_80243260);
      }
    } else if (D_8026FF18 == 0) {
      D_8026FF08 = 1;
      D_80270850 = CAR(0)->season->race != 0;
      if (D_803162FC == CAR(0)->season->round) {
        BrSeasonPickRace();
        D_802724F4 = 1;
        BrModeSet((int)BrLoadSaveScreen);
      } else {
        BrScreenFlush2Layout0();
        D_8026FF18 = 5;
        BrModeSet((int)BrRaceTick);
      }
    } else if (D_8026FF18 == 2 && D_802707A8 != 0) {
      D_802724F4 = 2;
      BrModeSet((int)BrLoadSaveScreen);
    } else {
      BrModeSet((int)BrMainMenu);
    }
  }
}
