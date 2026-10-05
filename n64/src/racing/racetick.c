/* racetick.c -- the race: one frame of it, from the first-frame setup
 * (players, cars, ghosts, the track's special objects) through the start
 * lights, every view drawn, the pause menu, to the fade that hands over to
 * the next game mode.
 *
 * BrRaceTick was compiled without IDO's global optimiser (it is over the
 * -Olimit threshold), so its locals live in the frame and its `register`
 * variables in s0-s6 and f20-f24: its declarations reproduce that frame slot
 * for slot, which the whole-image run needs (the frame is live across every
 * retrace).  Paths no box script reaches yet: two players, the instant
 * replay, the time-attack ghost, the airplane, the rear-view mirror and the
 * season end.
 */
/* n64-cflags: -O2 */
#include "tgr/common.h"
#include "tgr/car.h"
#include "tgr/pad.h"
#include "tgr/menu.h"
#include "tgr/season.h"
#include "tgr/vec.h"

/* -- declarations -- */
typedef struct BrCarEnt {       /* a race entity (0x78 bytes): the cars, then the rest */
  char pad00[0x60];
  BrCar *car;                   /* 0x60  0 for an entity that is not a car */
  int x64;
  unsigned int flags;           /* 0x68  bit 0: held at the start, bit 1: finished */
  char pad6c[0x78 - 0x6C];
} BrCarEnt;
extern BrCarEnt D_803239A0[];

typedef struct BrViewRect {     /* a player's view (0x14 bytes) */
  int x; int y; int w; int h;
  int car;                      /* 0x10  the car it follows */
} BrViewRect;
extern BrViewRect D_8031B2C8[2];

typedef struct BrRaceLight {    /* one step of the start-light script */
  int state;
  float secs;
} BrRaceLight;
extern BrRaceLight D_802707D0[];
extern float D_80270794[];      /* countdown beep times (the state-2 timer runs down) */

typedef struct BrSpecial {      /* one of the track's special objects (12 bytes) */
  int obj;                      /* 0x00  its track object */
  int arg;                      /* 0x04  a count, a path, or an angle's bits */
  unsigned char kind;           /* 0x08  0-2 spinner (z, x, y axis), 3 airplane, 4/5 its
                                 *       left/right path, 6 its trigger, 7 waterfall */
  char pad09[3];
} BrSpecial;

typedef struct BrTrackObj {     /* a track object (0x54 bytes) */
  BrVec3 x; float wx;           /* 0x00  its matrix, rows */
  BrVec3 y; float wy;           /* 0x10 */
  BrVec3 z; float wz;           /* 0x20 */
  BrVec3 pos; float wpos;       /* 0x30 */
  char pad40[0x4C - 0x40];
  unsigned short flags;         /* 0x4C */
  char pad4e[0x54 - 0x4E];
} BrTrackObj;

typedef struct BrTrackHdr {     /* the loaded track's header */
  char pad00[0x60];
  BrTrackObj *objs;             /* 0x60  its objects */
  char pad64[0x164 - 0x64];
  BrSpecial specials[16];       /* 0x164 */
  int nSpecials;                /* 0x224 */
} BrTrackHdr;
extern BrTrackHdr D_80025C00;

typedef struct BrTimeLimit {    /* time allowed per difficulty and car class (0x1C bytes) */
  float secs;
  char pad04[0x18];
} BrTimeLimit;
typedef struct BrTrackTimes {
  char pad00[0x44];
  BrTimeLimit limit[4][3];      /* 0x44  [class][difficulty] */
} BrTrackTimes;
extern BrTrackTimes *D_80271D1C[];  /* per track (the same records as D_80270854) */

typedef struct BrTip {          /* a demo caption (0x20 bytes) */
  char *title;
  float secs;
  char *lines[6];
} BrTip;
extern BrTip D_8026FFB4[];

typedef struct BrRaceSnd {      /* 0x18 bytes, six of them */
  void *p;
  int x4;
  long long x8;
  int x10;
  int x14;
} BrRaceSnd;
extern BrRaceSnd D_802A4920[6];
extern int D_802A49B0;
extern char D_802A4A08[];

typedef struct { char raw[0xdf88]; } BrCarModelBuf;
extern BrCarModelBuf D_803C8000[];

extern int D_802707C8;          /* the race has been set up */
extern int D_8028A884;          /* setting up */
extern int D_8026FF08;          /* human players */
extern int D_8026FF10;          /* paused */
extern int D_8026FF14;          /* the pause menu's row */
extern int D_8026FF18;          /* the game mode: 0 championship, 1 arcade, 2 time attack,
                                 * 4 attract demo, 5 season end */
extern int D_8026FF1C;          /* which demo */
extern int D_8027078C;
extern int D_80270788;          /* this race is the instant replay */
extern int D_80270784;          /* bytes in the saved time-attack ghost */
extern int D_80270790;
extern int D_802707A8;          /* a new time-attack record */
extern int D_802707C4;          /* waterfalls */
extern int D_802707CC;          /* the race is fading out */
extern int D_8028B304;
extern int D_802A4A30;          /* flips every frame */
extern int D_8028B7F0;          /* entries in D_803239A0 */
extern int D_8028B7F4;          /* cars in the race */
extern int D_8028B7F8;
extern int D_8028B940;          /* the track */
extern int D_8028C800;
extern int D_80315E3C;          /* the best recording's length */
extern int D_80315E40;          /* the player whose recording is the best */
extern unsigned char *D_80315E48[2];    /* the recordings the replay plays */
extern int D_80315E58[2];       /* and their lengths */
extern unsigned char D_80307F00[];      /* the saved time-attack ghost: track, kind, ... */
extern int D_8028A8AC;          /* the track is mirrored */
extern int D_8026FF54;
extern char D_802F7F00[];
extern int D_8026FF58;
extern int D_8026FF5C;
float D_8026FF60 = 0.0f;
typedef struct BrAirplane {     /* the airplane some tracks fly over the course */
  int obj;                      /* 0x00  its track object, 0 for none */
  int trigger;                  /* 0x04  the trigger that sends it */
  int lap;                      /* 0x08  on this lap */
  int sent;                     /* 0x0C  it has been sent */
  int point;                    /* 0x10  the path point it is past */
  int count;                    /* 0x14  path points */
  float scale;                  /* 0x18 */
  int left;                     /* 0x1C  its path, left and right edges (BrVec3 *) */
  int right;                    /* 0x20 */
  float dist;                   /* 0x24  distance along the current path segment */
  float segLen;                 /* 0x28  the segment's length */
  BrVec3 prev;                  /* 0x2C  the previous, current and next segment directions */
  BrVec3 dir;                   /* 0x38 */
  BrVec3 next;                  /* 0x44 */
} BrAirplane;
BrAirplane D_8026FF64 = { 0 };
#define PLANE (&D_8026FF64)
typedef struct BrWaterfall {    /* a waterfall: its track object */
  int obj;
} BrWaterfall;
extern BrWaterfall D_80315DF8[];
extern int D_8031B2D8;
extern int D_8031B2EC;
extern int D_8028AB0C;          /* views on screen */
extern int D_80315E78;          /* the start-light state */
extern float D_802F7EF0;        /* its timer */
extern int D_802F7EF4;          /* countdown beeps played */
extern int D_802F7EF8;          /* the step of the light script */
extern int D_8028B300;
extern int D_8028AA80;
extern int D_8028AA84;
extern int D_8028AA8C;
extern int D_8028AA54;          /* the rear-view mirror size, 0 for none */
extern int D_8028AA5C;
extern int D_8028AA6C;          /* the HUD is on */
extern int D_8028AA98;
extern int D_8028AAB0;          /* screen width */
extern int D_8028AAB4;          /* screen height */
extern float D_8028AAC8;        /* far plane */
extern float D_8028AAD8;        /* seconds this frame */
extern int D_8028AAEC;          /* the view being drawn */
extern BrCar *D_8028AAF0;       /* its car */
extern BrCarCam *D_8028AAF4;    /* its camera */
extern int D_8028AAF8;
extern int D_8028AAFC;
extern int D_8028AB00;
extern int D_8028AB04;
extern int D_8031B248;
extern int D_8031B288;
extern int D_8028AB10;
extern int D_8028C328;
extern int D_80315E38;          /* frames to clear the whole screen */
extern int D_80315E74;          /* views last frame */
extern float D_80315E68;        /* the demo caption's clock */
extern int D_80315E6C;          /* the demo caption shown */
extern int D_80315E70;          /* the demo was skipped */
extern float D_8031AB10[4][4];
extern char D_80315DD8[];
extern int D_802723D0;          /* music volume */
extern int D_802723D4;          /* effects volume */
extern unsigned char D_802724F4;
extern short D_802A4C04;
extern char D_000AD400[];       /* the three demo recordings in ROM */
extern char D_000AD740[];
extern char D_000ADB00[];
extern char D_001CB410[];
extern char D_001D9CF0[];

void osViBlack(int black);
void osSyncPrintf(char *fmt, ...);
void *memcpy(void *dst, void *src, unsigned int n);
int sprintf(char *buf, char *fmt, ...);
void guRotateF(float mf[4][4], float a, float x, float y, float z);
void guMtxCatF(float m[4][4], float n[4][4], float r[4][4]);
void BrRaceSetKind(int kind);
void BrMusicLoadTrack(void);
void BrRaceDrawLayers(void);
void func_8020046C(void);
void BrDemoRaceStartC(void);
void BrRaceCueLayout(void);
void BrRaceCueRewind(void);
void BrRaceCueDraw(void);
void BrRaceResultSave(void);
void func_802060DC(void);
void BrSeasonPickRace(void);
void func_8020686C(void);
void BrTimeFormat(char *psz, float t);
void BrResultsRun(void);
void BrMainMenu(void);
void BrMusicVolumeUp(void);
void BrMusicVolumeDown(void);
void BrSfxVolumeUp(void);
void BrSfxVolumeDown(void);
void BrOptionsScreen(void);
void func_80211D70(void);
void BrRumbleProbe(void);
void BrStub80217C8C(void);
void BrZBufferClear(void);
void func_80217E20(int x, int y, int w, int h);
void BrScreenClear(int r, int g, int b);
void BrGfxFillRect(int x, int y, int w, int h, int r, int g, int b);
void func_802182A8(void);
void func_80218D5C(void);
void BrFrameBeginLayout0(void);
void BrScissorSet(int x, int y, int w, int h);
void BrViewportSet(int x, int y, int w, int h, int scissor);
void BrViewportFull(int x, int y, int w, int h, int scissor);
void func_80219F6C(int x, int y, int w, int h);
void func_8021A0F8(int x, int y, int w, int h);
void BrScreenFlush2Layout1(void);
void func_8021A5B8(void);
void BrFrameEnd(void);
void func_8021B0C4(BrCarCam *cam, float fov, float far, float w, float h);
void BrCameraSet(float m[4][4], float fov, float far, float w, float h);
void BrScreenCameraSet(float w, float h);
void BrRaceClockReset(void);
void BrClockTick(void);
void BrFrameSetHook(int hook);
void BrModeSet(int mode);
int BrRomReadSize(int rom);
void BrRomUnpack(unsigned char *dst, char *rom, int s);
void BrEntPaintTexture(int model, unsigned int r, unsigned int g, int b);
void BrAnimSetOnce(int anim);
int BrModelReadRaw(char *dst, char *rom, char *romEnd);
void func_8021DE5C(int track);
int func_8021F380(int a, int b, float *at, int *c, int *d, int *e, int *f, int *g, int *h);
void BrEntLoadRecord(BrCar *car, int slot, int kind);
void BrScreenFlashClear(void);
void BrScreenFlashDraw(void);
void BrFadeTo(float level, float seconds);
void BrSfxFadeTo(float level, float seconds);
void BrMusicFadeTo(float level, float seconds);
int BrFadeIsOut(void);
int BrFadeAtTarget(void);
void BrFadeBarsDraw(void);
void BrScreenDim(float level);
void BrFadeStep(void);
void BrVec3Cross(BrVec3 *pOut, BrVec3 *pA, BrVec3 *pB);
void BrVec3ScaleBy(BrVec3 *pV, float s);
void BrVec3Normalise(BrVec3 *pV);
void BrVec3Sub(BrVec3 *pOut, BrVec3 *pA, BrVec3 *pB);
void BrVec3Lerp(BrVec3 *pOut, BrVec3 *pA, BrVec3 *pB, float t);
void BrVec3Midpoint(BrVec3 *pOut, BrVec3 *pA, BrVec3 *pB);
float BrVec3Dist(BrVec3 *pA, BrVec3 *pB);
float BrVec3Length(BrVec3 *pV);
void BrMat4ProjectPoint(float out[3], float v[3], float m[4][4]);
void BrRandSeed(int seed);
void BrClearList(void);
void BrCarPickKind(BrCar *car);
void BrCarCamStep(int ent);
void BrCtlAi(int ctl);
void func_80228A6C(BrCar *car);
void func_802291F8(BrCarEnt *e);
void func_80229550(void);
void func_80229700(BrCar *car);
void func_8022AF90(BrCar *car);
void BrVarLoadAll(char *buf);
void BrVarSaveSmall(void);
void BrVarLoadSmall(void);
void BrSfxSrcBeep(void);
void BrSfxSrcBeep2(void);
void BrSndNearestInvalidate(void);
void BrSndNearestReset(void);
void func_8022B534(void);
void BrSndNearestOfferTrack(int f8C, void *pPos, void *pListener);
void BrSndNearestOfferDefault(int f8C, void *pPos, void *pListener);
void BrStub8022BA98(int arg0);
void func_8022BAA0(void);
void BrCarCamClearCut(int car);
void func_8022BCB4(BrCar *car);
void func_8022C9FC(BrCarEnt *e);
void BrCarEntTick(BrCarEnt *e);
void func_8022D49C(void);
void BrPerfMark(int bar, int r, int g, int b, int a);
void func_8022D97C(void);
void BrTextHighlightOff(void);
void BrTextAlignCentre(void);
void BrTextAlignLeft(void);
void BrTextAlignRight(void);
void BrTextSetColours(int a, int b, int c, int d, int e, int f);
void BrTextSetFont(int font);
void BrTextPrint(char *s, int x, int y);
void func_8022F968(void);
void BrCarVisibility(BrCar *car);
void func_80230554(BrCar *car, int a);
void func_80232ED4(BrCar *car);
void BrVtxPoolsReset(void);
void func_80235BAC(int pass);
void BrStub8023870C(void);
void BrHudPositionDraw(void);
void func_80238F30(void);
void func_8023A1C8(void);
void func_8023A784(void);
void BrParticleReset(void);
void BrParticleFrame(void);
void func_8023D714(void);
void func_8023DBC0(void);
void BrPadConsume(BrPadRec *pad, unsigned int bits);
void BrPadStickToButtons(BrPadRec *pad);
void BrTriCacheReset(void);
/* -- end declarations -- */

#define PAD(i) ((BrPadRec *)D_8031B760[i].pad)
#define OBJ(n) ((BrTrackObj *)((char *)D_80025C00.objs + (n) * sizeof(BrTrackObj)))
#define LEFT ((BrVec3 *)PLANE->left)
#define RIGHT ((BrVec3 *)PLANE->right)

/* WHAT IT DOES: One frame of the race (the game mode function while a race
 * runs).  The first call sets the race up: how many cars and entities, the
 * players' recordings for the instant replay and time attack's ghost (or,
 * for the attract demo and replay, the recording to play back), each car's
 * setup, control and model, the track's special objects (spinners, the
 * airplane and its path, waterfalls) and the start-light script.  Every
 * frame then steps the start lights and the race state (countdown beeps,
 * the finish, time attack's new record and TIME UP), ticks and moves every
 * entity, flies the airplane, draws each view (and the rear-view mirror)
 * with the HUD or the replay captions, runs the pause menu, and once the
 * screen has faded out picks the next game mode: the replay, results, the
 * next race or the menus.
 */
/* @t3 0x8020082C */
/* @t4-pass 0x8020082C 1 2026-09-28 compiles 16 best 5248 moved 28  (n64/tools/n64permute.py) */
/* @t4-pass 0x8020082C 2 2026-09-28 compiles 31 best 5247 moved 1  (n64/tools/n64permute.py) */
/* @t4-pass 0x8020082C 3 2026-09-28 compiles 31 best 5245 moved 2  (n64/tools/n64permute.py) */
/* @t4-pass 0x8020082C 4 2026-09-28 compiles 31 best 5245 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x8020082C tgr BrRaceTick */
void BrRaceTick(void)
{
  /* declared in the order of the original's frame (first = highest slot);
   * register variables take a slot too, and get s0, s1, ... in this order */
  register int j;
  register int i;
  BrCar *car0;
  float v[3];
  register float vx;
  register float vy;
  register float vz;
  BrCar *p0;
  BrCar *p1;
  BrCar *p2;
  BrCar *p3;
  BrCar *p4;
  BrCar *p5;
  int paused;
  BrCar *car;
  short s;
  int running;
  int k50;
  int k54;
  int k58;
  BrVec3 a;
  BrVec3 b;
  BrVec3 c;
  register float *d1;
  register float *s1;
  register float *d2;
  register float *s2;
  float f;
  int mx;
  int my;
  int mw;
  int mh;
  BrCarCam *saved;
  register int x;
  int y;
  int y0;
  int kb4;
  int kb8;
  int kbc;

  if (D_802707C8 == 0) {
    D_8028A884 = 1;
    osViBlack(1);
    BrFrameBeginLayout0();
    BrFrameEnd();
    osViBlack(1);
    BrFrameBeginLayout0();
    BrFrameEnd();
    BrRandSeed(123);
    D_8027078C = 0;
    if (D_8026FF18 != 1) {
      D_8028B304 = 3;
    }
    D_802A4A30 = 0;
    switch (D_8026FF18) {
    case 0:
      D_8028B7F0 = 20;
      D_8028B7F4 = 3;
    players:
      if (D_80270788 != 0) {
        BrVarLoadAll(D_803C8000[3 - D_80315E40].raw);
        for (i = 0; i < 6; i++) {
          D_802A4920[i].x14 = 0;
          D_802A4920[i].x10 = 0;
        }
        for (i = 0; i < D_8026FF08; i++) {
          PAD(i)->ghost = D_80315E48[i];
          PAD(i)->ghostLen = D_80315E58[i];
          PAD(i)->ghostPos = 0;
        }
      } else {
        for (i = 0; i < D_8026FF08; i++) {
          PAD(i)->ghost = 0;
          PAD(i)->rec[0] = 0;
          PAD(i)->rec[1] = 0;
        }
      }
      D_8027078C = !D_80270788;
      break;
    case 1:
      D_8028B7F0 = 2;
      D_8028B7F4 = 2;
      goto players;
    case 5:
      D_8026FF08 = 1;
      D_8028B7F0 = 1;
      D_8028B7F4 = 1;
      D_8027078C = 0;
      D_80270788 = 0;
      D_8031B760[0].xe58 = 1;
      D_8031B760[0].cam = &D_8031B760[0].cams[1];
      D_8028B7F8 = 0;
      break;
    case 4:
      D_8028C800 = 1;
      D_8026FF08 = 1;
      D_8028B7F0 = 1;
      D_8028B7F4 = 1;
      D_8027078C = 0;
      D_8031B760[0].xe58 = 0;
      PAD(0)->ghost = (unsigned char *)0x80197000;
      switch (D_8026FF1C) {
      case 0:
        PAD(0)->ghostLen = BrRomReadSize((int)D_000AD400);
        BrRomUnpack(PAD(0)->ghost, D_000AD400, 0);
        break;
      case 1:
        PAD(0)->ghostLen = BrRomReadSize((int)D_000AD740);
        BrRomUnpack(PAD(0)->ghost, D_000AD740, 0);
        break;
      case 2:
        BrRaceCueLayout();
        PAD(0)->ghostLen = BrRomReadSize((int)D_000ADB00);
        BrRomUnpack(PAD(0)->ghost, D_000ADB00, 0);
        break;
      }
      PAD(0)->ghostPos = 8;
      PAD(0)->rec[0] = 0;
      PAD(0)->rec[1] = 0;
      car0 = &D_8031B760[0];
      car0->colour[0] = 0xFF;
      car0->colour[1] = 0xFF;
      car0->colour[2] = 0xFF;
      D_8028B940 = PAD(0)->ghost[0];
      D_8031B760[0].kind = D_8031B760[0].x2058 = PAD(0)->ghost[1];
      D_8031B760[0].xe68 = PAD(0)->ghost[2];
      D_8031B760[0].xe6c = PAD(0)->ghost[3];
      D_8031B760[0].xe60 = PAD(0)->ghost[4];
      D_8031B760[0].xe64 = PAD(0)->ghost[5];
      PAD(0)->x25 = PAD(0)->ghost[6];
      break;
    case 2:
      if (D_8028B940 == 3 || D_8028B940 == 8) {
        D_8028C800 = 1;
      } else {
        D_8028C800 = 0;
      }
      D_8028B7F4 = D_8028B7F0 = D_8026FF08 + 1;
      D_8027078C = 0;
      D_802707A8 = 0;
      PAD(D_8026FF08)->ghost = (unsigned char *)0x80197000;
      osSyncPrintf("savedRecordingLength = %d\n", D_80270784);
      memcpy(PAD(D_8026FF08)->ghost, D_80307F00, D_80270784);
      D_8031B760[D_8026FF08].kind = D_8031B760[D_8026FF08].x2058 = PAD(D_8026FF08)->ghost[1];
      if (PAD(D_8026FF08)->ghost[0] != D_8028B940) {
        osSyncPrintf("WRONG TRACK (%d!=%d)\n", PAD(D_8026FF08)->ghost[0], D_8028B940);
        PAD(D_8026FF08)->ghost = 0;
      } else {
        osSyncPrintf("TRACK=%d\n", PAD(D_8026FF08)->ghost[0]);
        PAD(D_8026FF08)->ghostPos = 8;
        osSyncPrintf("PLAYLIMIT=%d\n", D_80270784);
        PAD(D_8026FF08)->ghostLen = D_80270784;
      }
      for (i = 0; i < D_8026FF08; i++) {
        PAD(i)->rec[i] = (unsigned char *)(0x801A6000 + i * 0xF000);
        PAD(i)->rec[i][0] = D_8028B940;
        PAD(i)->rec[i][1] = D_8031B760[i].kind;
        PAD(i)->rec[i][2] = D_8031B760[i].season->xd4;
        PAD(i)->rec[i][3] = D_8031B760[i].season->xd8;
        PAD(i)->rec[i][4] = D_8031B760[i].season->xdc;
        PAD(i)->rec[i][5] = D_8031B760[i].season->xe0;
        PAD(i)->rec[i][6] = D_8031B760[i].season->xe4;
        PAD(i)->recLen[i] = 8;
        PAD(i)->recKeep[i] = 0xDE5C;
      }
      break;
    default:
      D_8028B7F0 = D_8026FF08;
      D_8028B7F4 = D_8026FF08;
      D_8027078C = 0;
      break;
    }
    if (D_8026FF18 == 5) {
      D_8028A8AC = 0;
    } else {
      D_8028A8AC = !!(((MenuItem *)D_80271D1C[D_8028B940])->flags & 0x10);
    }
    if (D_8026FF18 != 4 && D_80270788 == 0) {
      BrRumbleProbe();
      BrMusicLoadTrack();
    }
    func_8022BAA0();
    func_8021DE5C(D_8026FF18 == 5 ? 10 : D_8028B940);
    if (D_80270788 == 0) {
      PLANE->sent = 0;
      PLANE->trigger = 0;
      PLANE->obj = 0;
      PLANE->right = 0;
      PLANE->left = 0;
      PLANE->dist = 0.0f;
      PLANE->segLen = 0.0f;
      PLANE->point = 0;
      PLANE->count = 0;
      osSyncPrintf("specials: %d\n", D_80025C00.nSpecials);
      D_802707C4 = 0;
      for (i = 0; i < D_80025C00.nSpecials; i++) {
        switch (D_80025C00.specials[i].kind) {
        case 4:
          PLANE->count = D_80025C00.specials[i].arg;
          osSyncPrintf("airplanePathLeft = %08x\n",
                       PLANE->left = D_80025C00.specials[i].obj);
          break;
        case 5:
          PLANE->right = D_80025C00.specials[i].obj;
          osSyncPrintf("airplanePathRight = %08x\n", PLANE->right);
          break;
        case 3:
          PLANE->obj = D_80025C00.specials[i].obj;
          osSyncPrintf("airplane = %d\n", PLANE->obj);
          break;
        case 6:
          PLANE->trigger = D_80025C00.specials[i].obj;
          osSyncPrintf("airplanetrigger = %d\n", PLANE->trigger);
          break;
        case 7:
          D_80315DF8[D_802707C4].obj = D_80025C00.specials[i].obj;
          D_802707C4++;
          osSyncPrintf("waterfall = %d\n", D_80025C00.specials[i].obj);
          break;
        }
      }
      if (PLANE->trigger == 0 || PLANE->left == 0 || PLANE->right == 0) {
        PLANE->obj = 0;
      }
      if (PLANE->obj != 0) {
        vx = 1.0f;
        vy = 0.0f;
        vz = 0.0f;
        (&v)[0][0] = vx;
        (&v)[0][1] = vy;
        (&v)[0][2] = vz;
        BrMat4ProjectPoint(v, v, (float (*)[4])OBJ(PLANE->obj));
        PLANE->scale = BrVec3Length((BrVec3 *)v);
      }
      PLANE->lap = D_8028B304 - 1;
    }
    if (D_80270788 != 0) {
      D_8028AB0C = 1;
    } else {
      D_8028AB0C = D_8026FF08;
    }
    if (D_80270788 != 0) {
      D_8031B2C8[0].car = D_80315E40;
    } else {
      D_8031B2C8[0].car = 0;
    }
    D_8031B2C8[1].car = !D_8031B2C8[0].car;
    if (D_8026FF18 == 4) {
      D_8031B760[0].cam = &D_8031B760[0].cams[1];
      D_8028B7F8 = 180;
    } else {
      for (i = 0; i < D_8026FF08; i++) {
        D_8031B760[i].xe6c = D_8031B760[i].season->xd8;
        D_8031B760[i].xe64 = D_8031B760[i].season->xe0;
        D_8031B760[i].xe60 = D_8031B760[i].season->xdc;
        D_8031B760[i].xe68 = D_8031B760[i].season->xd4;
        PAD(i)->x25 = D_8031B760[i].season->xe4;
      }
      if (D_8026FF18 == 2 && PAD(1)->ghost != 0) {
        D_8031B760[1].xe68 = PAD(1)->ghost[2];
        D_8031B760[1].xe6c = PAD(1)->ghost[3];
        D_8031B760[1].xe60 = PAD(1)->ghost[4];
        D_8031B760[1].xe64 = PAD(1)->ghost[5];
        PAD(1)->x25 = PAD(1)->ghost[6];
      } else {
        for (; i < D_8028B7F4; i++) {
          D_8031B760[i].xe6c = 1;
          D_8031B760[i].xe64 = 1;
          D_8031B760[i].xe60 = 2;
          D_8031B760[i].xe68 = 0;
          PAD(i)->x25 = 0;
        }
      }
    }
    if (D_80270788 == 0) {
      D_80270790 = 0;
      for (i = 0; i < D_8028B7F4; i++) {
        D_8031B760[i].x2064 = 1.0f;
        if (i < D_8026FF08) {
          p0 = &D_8031B760[i]; p0->xed8 = (int)BrCarCamStep;
          p1 = &D_8031B760[i]; p1->colour[3] = 0;
        } else if (D_8026FF18 == 2) {
          p2 = &D_8031B760[i]; p2->xed8 = (int)BrCarCamStep;
          p3 = &D_8031B760[i]; p3->colour[3] = 2;
          D_8031B760[i].x2064 = 0.375f;
        } else {
          BrCarPickKind(&D_8031B760[i]);
          p4 = &D_8031B760[i]; p4->xed8 = (int)BrCtlAi;
          p5 = &D_8031B760[i]; p5->colour[3] = 0;
        }
        if (D_8031B760[i].xe58 == 0) {
          BrEntLoadRecord(&D_8031B760[i], i, D_8031B760[i].kind);
        }
        func_80228A6C(&D_8031B760[i]);
        D_8031B760[i].xf4c = 0;
        D_8031B760[i].msgA = 0;
        D_8031B760[i].msgB = 0;
      }
    } else {
      for (i = D_8026FF08; i < D_8028B7F4; i++) {
        BrEntPaintTexture((int)D_8031B760[i].model, D_8031B760[i].colour[0] >> 3,
                          D_8031B760[i].colour[1] >> 3, D_8031B760[i].colour[2] >> 3);
      }
    }
    if (D_80270788 != 0) {
      D_802F7EF8 = 4;
    } else if (D_8026FF18 == 5) {
      D_802F7EF8 = 4;
    } else {
      D_802F7EF8 = 0;
    }
    D_80315E78 = D_802707D0[D_802F7EF8].state;
    D_802F7EF0 = D_802707D0[D_802F7EF8].secs;
    if (D_80270788 == 0) {
      D_8028B300 = 0;
      for (i = 0; i < D_8028B7F0; i++) {
        func_802291F8(&D_803239A0[i]);
      }
    }
    BrRaceSetKind(D_8028C800);
    if (D_8028AA80 == 0 && (D_8028B940 == 0 || D_8028B940 == 5)) {
      PLANE->obj = 0;
    }
    BrFadeTo(1.0f, 0.2f);
    BrSfxFadeTo(1.0f, 0.2f);
    BrMusicFadeTo(1.0f, 0.2f);
    D_80315E38 = 2;
    D_802707C8 = 1;
    D_8026FF54 = (int)D_802F7F00;
    BrModelReadRaw(D_802F7F00, D_001CB410, D_001D9CF0);
    BrAnimSetOnce(D_8026FF54);
    BrParticleReset();
    BrTriCacheReset();
    D_8026FF10 = 0;
    D_802707CC = 0;
    D_80315E74 = -1;
    D_80315E68 = 0.0f;
    D_80315E6C = 0;
    D_8028AB10 = 1;
    if (D_80270788 == 0) {
      BrRaceClockReset();
      BrSndNearestReset();
    }
    D_8028A884 = 0;
  }

  /* ---- every frame ---- */
  D_802A4C04 = 0;
  if (D_80315E78 == 4) {
    for (i = 0; i < D_8026FF08; i++) {
      func_8022AF90(&D_8031B760[i]);
    }
  }
  if (D_8026FF18 != 4) {
    D_802A4A30 ^= 1;
  }
  /* the replay ends when the recordings run out */
  if (D_80270788 != 0 && D_80315E78 == 4) {
    if (D_8026FF18 == 1) {
      if (D_8031B2C8[0].car == D_80315E40) {
        if (PAD(D_80315E40)->ghostPos == PAD(D_80315E40)->ghostLen) {
          if (D_8026FF08 == 1) {
          replay_over:
            if (D_802707CC == 0) {
              D_802707CC = 1;
              D_8027078C = 0;
              BrSfxFadeTo(0.0f, 0.2f);
              BrFadeTo(0.0f, 0.2f);
            }
          } else if (PAD(!D_80315E40)->ghostPos == PAD(D_80315E40)->ghostLen + 240) {
            D_8031B2C8[1].car = D_80315E40;
            D_8031B2C8[0].car = !D_8031B2C8[1].car;
            goto other_view;
          }
        }
      } else {
      other_view:
        if (PAD(!D_80315E40)->ghostPos == PAD(!D_80315E40)->ghostLen) {
          goto replay_over;
        }
      }
    } else if (PAD(0)->ghostPos == PAD(0)->ghostLen) {
      if (D_802707CC == 0) {
        D_802707CC = 1;
        D_8027078C = 0;
        BrSfxFadeTo(0.0f, 0.2f);
        BrFadeTo(0.0f, 0.2f);
      }
    }
  }
  BrClockTick();
  D_8026FF58 = 0;
  paused = D_8026FF10;
  D_8026FF5C = 0;

  /* the start lights and the race state */
  if (D_80315E78 < 3) {
    D_8026FF5C = 1;
    for (i = 0; i < D_8028B7F0; i++) {
      D_803239A0[i].flags |= 1;
      car = D_803239A0[i].car;
      if (car != 0 && car->slot < D_8026FF08) {
        if (D_8026FF18 == 1) {
          s = D_8028C800 - 1;
          if (s > 2 || s < 0) {
            s = 0;
          }
          car->xfa4 = D_80271D1C[D_8028B940]->limit[car->xe34][s].secs;
        }
        car->msgATime = 1.0f;
        switch (D_80315E78) {
        case 0:
          D_8026FF58 = -1;
          D_802F7EF4 = 0;
          break;
        case 1:
          break;
        case 2:
          D_8026FF58 = 1;
          if (D_80270794[D_802F7EF4] > D_802F7EF0) {
            D_802F7EF4++;
            if (D_802F7EF4 == 4) {
              BrSfxSrcBeep2();
            } else {
              BrSfxSrcBeep();
            }
          }
          break;
        }
      }
    }
    D_8026FF60 = 0.0f;
  timer:
    if (D_8026FF10 == 0 && (D_802F7EF0 -= D_8028AAD8) < 0.0f) {
    advance:
      D_802F7EF8++;
      D_80315E78 = D_802707D0[D_802F7EF8].state;
      D_802F7EF0 = D_802707D0[D_802F7EF8].secs;
    }
  } else if (D_80315E78 == 3) {
    D_8026FF58 = 1;
    D_8026FF5C = 1;
    for (i = 0; i < D_8028B7F0; i++) {
      D_803239A0[i].flags &= ~1;
    }
    D_8026FF60 = (D_802707D0[D_802F7EF8].secs - D_802F7EF0) / D_802707D0[D_802F7EF8].secs;
    D_8026FF60 *= 15.0f * D_8026FF60;
    goto timer;
  } else if (D_80315E78 == 4) {
    running = 1;
    if (((D_8026FF18 == 4
          && ((D_8026FF1C == 2 && D_8031B760[0].lapTime > 140.0f)
              || (D_8026FF1C != 2 && D_8031B760[0].lapTime > 39.6f)))
         || (D_8026FF18 == 5 && D_8031B760[0].lapTime > 16.0f))
        && BrFadeIsOut() == 0) {
      BrFadeTo(0.0f, 0.2f);
      BrSfxFadeTo(0.0f, 0.2f);
      D_802707CC = 1;
      D_80270788 = 0;
      D_8027078C = 0;
      running = 0;
    }
    for (i = 0; i < D_8026FF08; i++) {
      if ((D_803239A0[i].flags & 2) != 0) {
        if (D_8026FF18 == 2 && PAD(i)->rec[i] != 0) {
          osSyncPrintf("# Format SB BitSize 24 Rows %d Columns 1\n", PAD(i)->recLen[i] >> 1);
          for (k50 = 0; k50 < PAD(i)->recLen[i]; k50 += 2) {
            osSyncPrintf("%.02x %.02x\n", PAD(i)->rec[i][k50], PAD(i)->rec[i][k50 + 1]);
          }
          if (i == 0
              && (PAD(i)->recLen[i] < D_80270784 || D_80307F00[0] != D_8028B940
                  || D_80270784 < 9)) {
            osSyncPrintf("%d: %d<%d=%d || %d!=%d=%d || %d<=%d=%d\n", i, PAD(i)->recLen[i],
                         D_80270784, PAD(i)->recLen[i] < D_80270784, D_80307F00[0], D_8028B940,
                         D_80307F00[0] != D_8028B940, D_80270784, 8, D_80270784 < 9);
            D_80270784 = PAD(i)->recLen[i];
            memcpy(D_80307F00, PAD(i)->rec[i], D_80270784);
            D_8031B760[i].msgA = (int)"NEW RECORD!";
            D_8031B760[i].msgATime = 1.0f;
            BrTimeFormat(D_8031B760[i].xfc0, D_8031B760[i].lapTime);
            D_8031B760[i].msgB = (int)D_8031B760[i].xfc0;
            D_8031B760[i].msgBTime = 1.0f;
            D_802707A8 = 1;
          }
          PAD(i)->rec[i] = 0;
        } else if (D_8026FF18 == 1) {
          for (k54 = 0; k54 < D_8026FF08; k54++) {
            PAD(i)->recKeep[k54] = PAD(i)->recLen[k54];
          }
        }
      } else {
        running = 0;
      }
    }
    if (running == 0) {
      if (D_8026FF18 == 1 && D_8026FF08 == 1 && D_8031B760[0].xfa4 == 0.0) {
        D_8027078C = 0;
        D_8031B760[0].msgA = D_8031B760[1].msgA = (int)"%ryTIME UP!";
        D_8031B760[1].msgATime = 2.0f;
        D_8031B760[0].msgATime = D_8031B760[1].msgATime;
      } else {
        goto draw;
      }
    }
    goto advance;
  } else if (D_80315E78 == 5) {
    if ((D_8026FF18 == 2 || D_8026FF18 == 1 || D_8026FF18 == 0) && D_80270788 == 0) {
      BrRaceResultSave();
    }
    goto advance;
  } else if (D_80315E78 == 6) {
    goto timer;
  } else if (D_80315E78 == 7 && D_802707CC == 0) {
    BrSfxFadeTo(0.0f, 0.2f);
    if (D_8027078C == 0) {
      BrMusicFadeTo(0.0f, 0.2f);
    }
    BrFadeTo(0.0f, 0.2f);
    D_802707CC = 1;
  }

draw:
  BrPerfMark(0, 0, 0, 200, 0xFF);
  BrClearList();
  for (i = 0; i < D_8028B7F4; i++) {
    BrCarCamClearCut((int)&D_8031B760[i]);
  }
  for (i = 0; i < D_8028B7F0; i++) {
    func_8022C9FC(&D_803239A0[i]);
  }
  for (i = 0; i < D_8028B7F0; i++) {
    BrCarEntTick(&D_803239A0[i]);
  }
  if (D_8026FF18 == 0) {
    for (i = 0; i < D_8026FF08; i++) {
      func_80229700(&D_8031B760[i]);
    }
  }
  func_80229550();
  if (D_8026FF10 == 0) {
    BrPerfMark(0, 0x80, 0x80, 0, 0xFF);
    BrParticleFrame();
    BrPerfMark(0, 0, 0xFF, 0xFF, 0xFF);
    func_8023A1C8();
    /* the airplane goes when a player passes its trigger on the right lap */
    if (PLANE->obj != 0 && PLANE->sent == 0
        && ((D_8028AA8C == 0 && D_8028AA84 == 0 && D_8028AA80 == 0)
            || (D_8028B940 != 2 && D_8028B940 != 7))) {
      for (i = 0; i < D_8026FF08; i++) {
        for (k58 = 0; k58 < D_8031B760[i].x2000; k58++) {
          if (D_8031B760[i].laps == PLANE->lap && D_8031B760[i].x1fc0[k58] == PLANE->trigger) {
            PLANE->sent = 1;
            break;
          }
        }
      }
    }
    /* the track's moving objects */
    for (i = 0; i < D_80025C00.nSpecials; i++) {
      switch (D_80025C00.specials[i].kind) {
      case 0:
        guRotateF(D_8031AB10, *(float *)&D_80025C00.specials[i].arg, 0.0f, 0.0f, 1.0f);
        goto spin;
      case 1:
        guRotateF(D_8031AB10, *(float *)&D_80025C00.specials[i].arg, 1.0f, 0.0f, 0.0f);
        goto spin;
      case 2:
        guRotateF(D_8031AB10, *(float *)&D_80025C00.specials[i].arg, 0.0f, 1.0f, 0.0f);
      spin:
        guMtxCatF(D_8031AB10, (float (*)[4])OBJ((D_80025C00.specials + i)->obj),
                  (float (*)[4])OBJ((D_80025C00.specials + i)->obj));
        OBJ((D_80025C00.specials + i)->obj)->flags &= 0xDFFF;
        break;
      case 3:
        if (PLANE->obj == 0 || PLANE->sent == 0) {
          break;
        }
        switch (D_8028B940) {
        case 1:
        case 6:
          PLANE->dist += D_8028AAD8 * 18.0f;
          break;
        default:
          PLANE->dist += D_8028AAD8 * 50.0f;
          break;
        }
        while (PLANE->dist > PLANE->segLen) {
          PLANE->dist -= PLANE->segLen;
          PLANE->point++;
          if (PLANE->point >= PLANE->count) {
            PLANE->obj = 0;
            break;
          }
          BrVec3Midpoint(&a, &LEFT[PLANE->point - 1], &RIGHT[PLANE->point - 1]);
          BrVec3Midpoint(&b, &LEFT[PLANE->point], &RIGHT[PLANE->point]);
          PLANE->segLen = BrVec3Dist(&a, &b);
          BrVec3Sub(&PLANE->dir, &b, &a);
          BrVec3Normalise(&PLANE->dir);
          if (PLANE->point < 2) {
            d1 = (float *)&PLANE->prev;
            s1 = (float *)&PLANE->dir;
            d1[0] = s1[0];
            d1[1] = s1[1];
            d1[2] = s1[2];
          } else {
            BrVec3Midpoint(&c, &LEFT[PLANE->point - 2], &RIGHT[PLANE->point - 2]);
            BrVec3Sub(&PLANE->prev, &a, &c);
            BrVec3Normalise(&PLANE->prev);
          }
          if (PLANE->point + 1 == PLANE->count) {
            d2 = (float *)&PLANE->next;
            s2 = (float *)&PLANE->dir;
            d2[0] = s2[0];
            d2[1] = s2[1];
            d2[2] = s2[2];
          } else {
            BrVec3Midpoint(&c, &LEFT[PLANE->point + 1], &RIGHT[PLANE->point + 1]);
            BrVec3Sub(&PLANE->next, &c, &b);
            BrVec3Normalise(&PLANE->next);
          }
        }
        if (PLANE->obj != 0) {
          f = PLANE->dist / PLANE->segLen;
          BrVec3Lerp(&a, &LEFT[PLANE->point], &LEFT[PLANE->point - 1], f);
          BrVec3Lerp(&b, &RIGHT[PLANE->point], &RIGHT[PLANE->point - 1], f);
          BrVec3Midpoint(&OBJ(PLANE->obj)->pos, &a, &b);
          if (f > 0.5f) {
            BrVec3Lerp(&OBJ(PLANE->obj)->x, &PLANE->next, &PLANE->dir, f - 0.5f);
          } else {
            BrVec3Lerp(&OBJ(PLANE->obj)->x, &PLANE->dir, &PLANE->prev, f + 0.5f);
          }
          BrVec3Normalise(&OBJ(PLANE->obj)->x);
          BrVec3Sub(&OBJ(PLANE->obj)->z, &a, &b);
          BrVec3Cross(&OBJ(PLANE->obj)->y, &OBJ(PLANE->obj)->x, &OBJ(PLANE->obj)->z);
          BrVec3Normalise(&OBJ(PLANE->obj)->y);
          BrVec3Cross(&OBJ(PLANE->obj)->z, &OBJ(PLANE->obj)->y, &OBJ(PLANE->obj)->x);
          BrVec3Normalise(&OBJ(PLANE->obj)->z);
          BrVec3ScaleBy(&OBJ(PLANE->obj)->x, PLANE->scale);
          BrVec3ScaleBy(&OBJ(PLANE->obj)->z, -PLANE->scale);
          BrVec3ScaleBy(&OBJ(PLANE->obj)->y, PLANE->scale);
        }
        break;
      }
    }
  }
  for (i = 0; i < D_8028B7F4; i++) {
    func_8022BCB4(&D_8031B760[i]);
  }
  BrSndNearestInvalidate();
  for (j = 0; j < D_8028AB0C; j++) {
    D_8028AAF0 = &D_8031B760[D_8031B2C8[j].car];
    D_8028AAF4 = D_8028AAF0->cam;
    if (PLANE->obj != 0 && PLANE->sent != 0) {
      BrSndNearestOfferTrack(PLANE->obj, &OBJ(PLANE->obj)->pos, D_8028AAF4);
    }
    for (i = 0; D_8028AAF4 != 0 && i < D_802707C4; i++) {
      BrSndNearestOfferDefault(PLANE->obj, &OBJ((D_80315DF8 + i)->obj)->pos, D_8028AAF4);
    }
    BrStub8022BA98((int)D_8028AAF4);
  }
  func_8022B534();
  BrFadeStep();
  BrFrameBeginLayout0();
  BrPerfMark(0, 0, 0x82, 0, 0xFF);
  BrVtxPoolsReset();
  BrZBufferClear();
  if (D_80315E38 != 0 || D_8028AA5C != 0) {
    BrScreenClear(0, 0, 0);
    if (D_80315E38 != 0) {
      D_80315E38--;
    }
  } else if (D_8031B2C8[0].w < 304) {
    BrGfxFillRect(D_8031B2C8[0].x + D_8031B2C8[0].w, 8, 304 - D_8031B2C8[0].w, 224, 0, 0, 0);
  }
  if (D_8028AB0C != D_80315E74) {
    D_80315E38 = 2;
    D_80315E74 = D_8028AB0C;
  }

  /* each view */
  for (j = 0; j < D_8028AB0C; j++) {
    BrPerfMark(0, 0, 0, 0, 0xFF);
    D_8028AAF0 = &D_8031B760[D_8031B2C8[j].car];
    D_8028AAF4 = D_8028AAF0->cam;
    D_8028AAEC = j;
    func_8021F380(0, 0, D_8028AAF4->mtx[3], &D_8031B248, &D_8028AB00, &D_8031B288, &D_8028AB04,
                  &D_8028AAF8, &D_8028AAFC);
    BrViewportSet(D_8031B2C8[j].x, D_8031B2C8[j].y, D_8031B2C8[j].w, D_8031B2C8[j].h, 1);
    if (D_8028AB0C >= 2) {
      func_8021B0C4(D_8028AAF4, D_8028AAF4->fov * 0.7f, D_8028AAC8 * 0.65f,
                    (float)D_8031B2C8[j].w, (float)D_8031B2C8[j].h);
      BrCameraSet(D_8028AAF4->mtx, D_8028AAF4->fov * 0.7f, D_8028AAC8 * 0.65f,
                  (float)D_8031B2C8[j].w, (float)D_8031B2C8[j].h);
    } else {
      func_8021B0C4(D_8028AAF4, D_8028AAF4->fov, D_8028AAC8, (float)D_8031B2C8[j].w,
                    (float)D_8031B2C8[j].h);
      BrCameraSet(D_8028AAF4->mtx, D_8028AAF4->fov, D_8028AAC8, (float)D_8031B2C8[j].w,
                  (float)D_8031B2C8[j].h);
    }
    func_802182A8();
    func_80218D5C();
    BrPerfMark(0, 0, 0x82, 0, 0xFF);
    func_8022F968();
    BrScreenFlashClear();
    func_8022D49C();
    if (D_8028AA80 != 0) {
      D_8028C328 = 2;
    } else {
      D_8028C328 = 1;
    }
    for (i = 0; i < D_8028B7F4; i++) {
      BrCarVisibility(&D_8031B760[i]);
    }
    func_80235BAC(0);
    if (D_8028AA84 == 0 || D_8028B940 == 2 || D_8028B940 == 7) {
      func_802182A8();
      func_80218D5C();
      BrPerfMark(0, 0x80, 0x80, 0x80, 0xFF);
      for (i = 0; i < D_8028B7F0; i++) {
        if (D_803239A0[i].car != 0 && D_803239A0[i].car->colour[3] != 2) {
          func_80230554(D_803239A0[i].car, 0);
        }
      }
      BrPerfMark(0, 0x80, 0x80, 0, 0xFF);
      func_8023D714();
    }
    BrPerfMark(0, 0x40, 0x40, 0x40, 0xFF);
    func_8020046C();
    func_80235BAC(1);
    if (D_8028AA84 != 0 && D_8028B940 != 2 && D_8028B940 != 7) {
      func_802182A8();
      func_80218D5C();
      BrPerfMark(0, 0x80, 0x80, 0x80, 0xFF);
      for (i = 0; i < D_8028B7F0; i++) {
        if (D_803239A0[i].car != 0 && D_803239A0[i].car->colour[3] != 2) {
          func_80230554(D_803239A0[i].car, 0);
        }
      }
      BrPerfMark(0, 0x80, 0x80, 0, 0xFF);
      func_8023D714();
    }
    BrPerfMark(0, 0x80, 0x80, 0x80, 0xFF);
    for (i = 0; i < D_8028B7F0; i++) {
      if (D_803239A0[i].car != 0) {
        func_80232ED4(D_803239A0[i].car);
      }
    }
    BrPerfMark(0, 0x80, 0x80, 0x80, 0xFF);
    for (i = 0; i < D_8028B7F0; i++) {
      if (D_803239A0[i].car != 0 && D_803239A0[i].car->colour[3] == 2) {
        func_80230554(D_803239A0[i].car, 0);
      }
    }
    BrPerfMark(0, 0x80, 0x80, 0, 0xFF);
    func_8023DBC0();
    BrPerfMark(0, 0, 0xFF, 0xFF, 0xFF);
    func_8023A784();
    BrPerfMark(0, 0, 0x82, 0, 0xFF);
    BrScreenFlashDraw();
    BrScreenCameraSet((float)D_8031B2C8[j].w, (float)D_8031B2C8[j].h);
    BrViewportFull(D_8031B2C8[j].x, D_8031B2C8[j].y, D_8031B2C8[j].w, D_8031B2C8[j].h, 1);
    /* the rear-view mirror, in the bumper view of a single player */
    if (&D_8028AAF0->cams[2] == D_8028AAF4 && D_8028AB0C == 1 && D_8028AA54 != 0) {
      if (D_8028AA54 == 1) {
        mw = D_8031B2C8[j].w / 4;
      } else if (D_8028AA54 == 2) {
        mw = D_8031B2C8[j].w * 5 / 16;
      } else {
        mw = D_8031B2C8[j].w * 3 / 8 + 2;
      }
      mh = mw >> 2;
      mx = D_8031B2C8[j].x + ((D_8031B2C8[j].w - mw) >> 1);
      my = D_8031B2C8[j].y + D_8031B2C8[j].h / 16;
      saved = D_8028AAF4;
      D_8028AAF4 = D_8028AAF0->cam = &D_8028AAF0->cam4;
      BrViewportSet(mx, my, -mw, mh, 1);
      func_80217E20(mx, my, mw, mh);
      func_8021B0C4(D_8028AAF4, D_8028AAF4->fov, D_8028AAC8 * 0.2f, (float)mw, (float)mh);
      BrCameraSet(D_8028AAF4->mtx, D_8028AAF4->fov, D_8028AAC8 * 0.3f, (float)mw, (float)mh);
      func_802182A8();
      func_80218D5C();
      BrPerfMark(0, 0, 0x82, 0, 0xFF);
      func_8022F968();
      BrScreenFlashClear();
      func_8022D49C();
      BrPerfMark(0, 0, 0x82, 0, 0xFF);
      for (i = 0; i < D_8028B7F4; i++) {
        BrCarVisibility(&D_8031B760[i]);
      }
      func_80235BAC(0);
      if (D_8028AA84 == 0 || D_8028B940 == 2 || D_8028B940 == 7) {
        func_802182A8();
        func_80218D5C();
        BrPerfMark(0, 0x80, 0x80, 0x80, 0xFF);
        for (i = 0; i < D_8028B7F0; i++) {
          if (D_803239A0[i].car != 0 && D_803239A0[i].car->colour[3] != 2) {
            func_80230554(D_803239A0[i].car, 0);
          }
        }
        BrPerfMark(0, 0x80, 0x80, 0, 0xFF);
        func_8023D714();
      }
      func_80235BAC(1);
      if (D_8028AA84 != 0 && D_8028B940 != 2 && D_8028B940 != 7) {
        func_802182A8();
        func_80218D5C();
        BrPerfMark(0, 0x80, 0x80, 0x80, 0xFF);
        for (i = 0; i < D_8028B7F0; i++) {
          if (D_803239A0[i].car != 0 && D_803239A0[i].car->colour[3] != 2) {
            func_80230554(D_803239A0[i].car, 0);
          }
        }
      }
      BrPerfMark(0, 0x80, 0x80, 0x80, 0xFF);
      for (i = 0; i < D_8028B7F0; i++) {
        if (D_803239A0[i].car != 0 && D_803239A0[i].car->colour[3] == 2) {
          func_80230554(D_803239A0[i].car, 0);
        }
      }
      BrScreenFlashDraw();
      BrPerfMark(0, 0, 0x82, 0, 0xFF);
      D_8028AAF0->cam = saved;
      D_8028AAF4 = D_8028AAF0->cam;
      BrViewportSet(D_8031B2C8[j].x, D_8031B2C8[j].y, D_8031B2C8[j].w, D_8031B2C8[j].h, 1);
      func_8021A0F8(mx, my, mw, mh);
    }
    if (D_8026FF18 == 5) {
      BrTextHighlightOff();
      BrTextAlignCentre();
      BrTextSetFont(40);
      if (D_8031B760[0].season->round != 0) {
        BrTextPrint("%rySEASON WINNER!", 160, 210);
      } else {
        BrTextPrint("%ryCHAMPION!", 160, 210);
      }
    } else if (D_8028AA6C != 0 && D_8026FF18 != 4) {
      if (D_80270788 != 0) {
        BrTextHighlightOff();
        BrTextAlignLeft();
        BrTextSetFont(15);
        BrTextPrint("%wwINSTANT REPLAY!", 28, 32);
        BrTextPrint("%wwA to restart", 28, D_8028AAB4 - 24);
        BrTextAlignRight();
        BrTextPrint("%wwSTART to exit", D_8028AAB0 - 28, D_8028AAB4 - 24);
      } else {
        if (&D_8028AAF0->cams[3] != D_8028AAF4) {
          BrPerfMark(0, 0xFF, 0, 0, 0xFF);
          func_80238F30();
          BrPerfMark(0, 0, 0x82, 0, 0xFF);
          if (&D_8028AAF0->cams[2] != D_8028AAF4) {
            BrStub8023870C();
          }
        } else if (D_8028AAF0->link->flags & 2) {
          BrHudPositionDraw();
        }
      }
    }
    func_80219F6C(D_8031B2C8[j].x, D_8031B2C8[j].y, D_8031B2C8[j].w, D_8031B2C8[j].h);
  }
  BrStub80217C8C();
  BrScissorSet(0, 0, D_8028AAB0, D_8028AAB4);

  /* input: skip the demo, pause, or the pause menu */
  if (D_8026FF10 != 0) {
    for (i = 0; i < D_8026FF08; i++) {
      BrPadStickToButtons(PAD(i));
      if (PAD(i)->pressed & 0x20) {
        BrPadConsume(PAD(i), 0x20);
        BrSfxFadeTo(1.0f, 0.2f);
        BrMusicFadeTo(1.0f, 0.2f);
        D_80315E38 = 2;
        paused = 0;
      }
      if (PAD(i)->pressed & 0xC010) {
        if (D_8026FF14 != 0 || (PAD(i)->pressed & 0x10) == 0) {
          BrPadConsume(PAD(i), 0xC010);
        }
        switch (D_8026FF14) {
        case 0:
          BrSfxFadeTo(1.0f, 0.2f);
          BrMusicFadeTo(1.0f, 0.2f);
          D_80315E38 = 2;
          paused = 0;
          break;
        case 1:
          BrFadeTo(0.0f, 0.2f);
          D_802707CC = 0;
          D_8027078C = 0;
          D_802707C8 = 0;
          break;
        case 4:
          BrFadeTo(0.0f, 0.2f);
          BrSfxFadeTo(0.0f, 0.2f);
          BrMusicFadeTo(0.0f, 0.2f);
          D_8027078C = 0;
          D_802707CC = 1;
          break;
        }
      }
      if (PAD(i)->pressed & 8) {
        BrPadConsume(PAD(i), 5);
        PAD(i)->pressed &= ~5;
        BrPadConsume(PAD(i), 8);
        D_8026FF14 = (D_8026FF14 + 4) % 5;
      }
      if (PAD(i)->pressed & 2) {
        BrPadConsume(PAD(i), 5);
        PAD(i)->pressed &= ~5;
        BrPadConsume(PAD(i), 2);
        D_8026FF14 = (D_8026FF14 + 1) % 5;
      }
      if (PAD(i)->pressed & 4) {
        if (D_8026FF14 != 0) {
          BrPadConsume(PAD(i), 4);
        }
        switch (D_8026FF14) {
        case 2:
          BrMusicVolumeDown();
          break;
        case 3:
          BrSfxVolumeDown();
          break;
        }
      }
      if (PAD(i)->pressed & 1) {
        if (D_8026FF14 != 0) {
          BrPadConsume(PAD(i), 1);
        }
        switch (D_8026FF14) {
        case 2:
          BrMusicVolumeUp();
          break;
        case 3:
          BrSfxVolumeUp();
          break;
        }
      }
    }
    {
      x = D_8031B2C8[0].x + (D_8031B2C8[0].w >> 1);
      BrScreenDim(0.33f);
      BrTextHighlightOff();
      BrTextAlignCentre();
      BrTextSetFont(40);
      BrTextPrint("%ryPAUSED", x, D_8028AAB4 * 5 / 16);
      y = D_8028AAB4 * 5 / 11;
      BrTextSetFont(20);
      BrTextPrint(D_8026FF14 == 0 ? "%y1Continue" : "%ryContinue", x, y);
      y += 20;
      BrTextPrint(D_8026FF14 == 1 ? "%y1Restart Race" : "%ryRestart Race", x, y);
      y += 20;
      sprintf(D_80315DD8, "%sBGM Volume: %d", D_8026FF14 == 2 ? "%y1" : "%ry", D_802723D0);
      BrTextPrint(D_80315DD8, x, y);
      y += 20;
      sprintf(D_80315DD8, "%sSFX Volume: %d", D_8026FF14 == 3 ? "%y1" : "%ry", D_802723D4);
      BrTextPrint(D_80315DD8, x, y);
      y += 20;
      BrTextPrint(D_8026FF14 == 4 ? "%y1Exit to Main Menu" : "%ryExit to Main Menu", x, y);
      y += 20;
    }
  } else {
    if (D_8026FF18 == 4 || D_8026FF18 == 5) {
      if (BrFadeIsOut() == 0) {
        for (i = 0; i < 2; i++) {
          if (D_8036A8E0[i].pressed & 0xC010) {
            BrPadConsume(&D_8036A8E0[i], 0xC010);
            D_80315E70 = 1;
            D_802707CC = 1;
            D_8027078C = 0;
            BrSfxFadeTo(0.0f, 0.2f);
            BrFadeTo(0.0f, 0.2f);
          }
        }
      }
    } else {
      for (i = 0; i < D_8026FF08; i++) {
        if (PAD(i)->pressed & 0x4000) {
          if (D_80270788 != 0) {
            BrFadeTo(0.0f, 0.2f);
            D_8027078C = 0;
            D_802707CC = 1;
          } else {
            paused = 1;
            D_8026FF14 = 0;
          }
          BrSfxFadeTo(0.0f, 0.2f);
          BrMusicFadeTo(0.0f, 0.2f);
          BrPadConsume(PAD(0), 0x4000);
          BrPadConsume(PAD(1), 0x4000);
          break;
        }
        if (PAD(i)->pressed & 0x10) {
          if (D_80270788 != 0) {
            BrFadeTo(0.0f, 0.2f);
            D_8027078C = 1;
            D_802707CC = 1;
          }
          BrPadConsume(PAD(0), 0x10);
          BrPadConsume(PAD(1), 0x10);
          break;
        }
      }
    }
  }

  /* the attract demo's captions */
  if (D_8026FF18 == 4 && D_8026FF1C == 2) {
    BrRaceCueRewind();
    BrRaceCueDraw();
  } else if (D_8026FF18 == 4 && D_8026FF1C == 1 && D_8026FFB4[D_80315E6C].title != 0) {
    if (D_80315E68 > 0.5f) {
      BrTextHighlightOff();
      BrTextAlignCentre();
      BrTextSetColours(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);
      for (i = 0; i < 5; i++) {
        if (D_8026FFB4[D_80315E6C].lines[i] == 0) {
          break;
        }
      }
      y0 = 132 - i * 40 / 4;
      if (D_8026FFB4[D_80315E6C].title[0] == 0) {
        y0 -= 5;
      }
      while (--i >= 0) {
        BrTextSetFont(20);
        BrTextPrint(D_8026FFB4[D_80315E6C].lines[i], 160, i * 40 / 2 + y0);
      }
      BrTextSetFont(15);
      BrTextPrint(D_8026FFB4[D_80315E6C].title, 160, y0 - 20);
    }
    D_80315E68 += D_8028AAD8;
    if (D_8026FFB4[D_80315E6C].secs < D_80315E68) {
      D_80315E68 = 0.0f;
      D_80315E6C++;
    }
  }
  BrFadeBarsDraw();
  if (1 < D_8028AA98) {
    BrViewportSet(0, 0, D_8028AAB0, D_8028AAB4, 1);
    func_8022D97C();
  }
  BrPerfMark(0, 0xFF, 0xE0, 0, 0xFF);
  func_8021A5B8();
  BrPerfMark(0, 0, 0x82, 0, 0xFF);
  BrFrameSetHook((int)BrRaceDrawLayers);
  BrFrameEnd();

  /* faded out: on to the next thing */
  if (BrFadeIsOut() != 0 && BrFadeAtTarget() != 0) {
    for (i = 0; i < 4; i++) {
      for (kb4 = 0; kb4 < D_8026FF08; kb4++) {
        PAD(i)->recKeep[kb4] = PAD(i)->recLen[kb4];
      }
    }
    D_802A49B0 = 0;
    for (kb8 = 0; kb8 < 6; kb8++) {
      D_802A4920[kb8].x14 = 0;
      D_802A4920[kb8].x10 = 0;
      D_802A4920[kb8].x8 = 0;
      D_802A4920[kb8].p = D_802A4A08;
    }
    D_8028AB10 = 0;
    if (D_802707CC != 0) {
      if (D_8027078C != 0) {
        /* the instant replay plays the best lap's recordings */
        if (D_80270788 == 0) {
          D_80315E3C = PAD(0)->recLen[0];
          D_80315E40 = 0;
          for (i = 1; i < D_8026FF08; i++) {
            if (D_80315E3C > PAD(i)->recLen[i]) {
              D_80315E3C = PAD(i)->recLen[i];
              D_80315E40 = i;
            }
          }
          for (i = 0; i < D_8026FF08; i++) {
            D_80315E48[i] = PAD(i)->rec[D_80315E40];
            D_80315E58[i] = PAD(i)->recLen[D_80315E40];
          }
          for (i = 0; i < D_8026FF08; i++) {
            for (kbc = 0; kbc < D_8026FF08; kbc++) {
              osSyncPrintf("veh[%d]->recIndex[%d] = %d\n", i, kbc, PAD(i)->recLen);
              PAD(i)->rec[kbc] = 0;
            }
          }
          osSyncPrintf("bestNumber = %d, bestIndex = %d\n", D_80315E40, D_80315E3C);
        }
        D_80270788 = 1;
      } else {
        BrScreenFlush2Layout1();
        D_8028A8AC = 0;
        if (D_8026FF18 == 5) {
          if (D_8031B760[0].season->round != 0) {
          next_race:
            D_8026FF18 = 0;
            BrSeasonPickRace();
            D_802724F4 = 1;
            BrModeSet((int)func_80211D70);
          } else {
            BrDemoRaceStartC();
          }
        } else if (D_8026FF18 == 4 && D_8026FF1C == 2) {
          goto next_race;
        } else if (D_80270790 != 0 && (D_8026FF18 == 2 || D_8026FF18 == 1 || D_8026FF18 == 0)) {
          func_802060DC();
          BrModeSet((int)BrResultsRun);
        } else {
          if (D_8026FF18 == 4 && D_8026FF1C == 1) {
            BrModeSet((int)BrOptionsScreen);
          } else if (D_8026FF18 == 4 && D_8026FF1C == 0 && D_80315E70 == 0) {
            BrModeSet((int)func_8020686C);
          } else {
            BrModeSet((int)BrMainMenu);
          }
        }
        D_80270788 = 0;
      }
      D_802707C8 = 0;
    }
    D_802A4C04 = 1;
    for (i = 0; i < 4; i++) {
      PAD(i)->ghost = 0;
    }
  }
  if (D_8026FF10 != paused) {
    if (paused != 0) {
      BrVarSaveSmall();
    } else {
      BrVarLoadSmall();
    }
    D_8026FF10 = paused;
  }
}
