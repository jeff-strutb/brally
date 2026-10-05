/* sndcar.c -- a car's sounds each frame: the engine loop's pitch and level,
 * the one-shot knocks and the surface loop
 */
#include "tgr/common.h"
#include "tgr/car.h"

/* -- declarations -- */
typedef struct BrRaceSnd {      /* a sound channel (0x18 bytes) */
  TgrAddr p;  /* void * */
  int x4;
  unsigned long long pitch;     /* 0x08  32.32 playback ratio */
  int x10;
  unsigned int level;           /* 0x14  left << 16 | right */
} BrRaceSnd;
typedef struct BrViewRec { int x; int y; int w; int h; int car; } BrViewRec;
#include "tgr/track.h"
typedef struct BrSurfSnd {      /* a surface loop (0x18 bytes) */
  int start;
  int x4;
  int x8;
  int len;                      /* 0x0C */
  int loop;                     /* 0x10 */
  int x14;
} BrSurfSnd;
extern BrRaceSnd D_802A4920[6];         /* six sound channels, two per car */
extern int D_802A49B0;                  /* the engine channel's override rate */
extern BrViewRec D_8031B2C8[2];         /* the views and the car each follows */
extern BrCar D_8031B760[];
extern int D_8026FF08;                  /* human players */
extern int D_8026FF10;                  /* set: silence the channels */
extern int D_8028AB0C;                  /* views on screen */
extern int D_8028C800;                  /* the weather */
#define D_80025C60 BR_TRACKOBJS()          /* the track's objects (cartridge data) */
extern BrSurfSnd D_8028BC04[];          /* the surface loops */
extern short D_802A4BEC[2];             /* the rumble pulse per player: on, */
extern short D_802A4BF0[2];
extern short D_802A4BF4[2];
extern short D_802A4BF8[2];
extern short D_802A4BFC[2];
extern short D_802A4C00[2];
float BrSndDoppler(BrVec3 *pos, BrVec3 *prev, BrVec3 *lst, BrVec3 *lstPrev);
void BrSndPan(BrVec3 *pos, BrCarCam *lst, float *l, float *r, int *vol, int x);
void BrSfxVoicePlay(short v, int start, int len, int loop);
float BrVec3Length(BrVec3 *v);
/* -- end declarations -- */

/* WHAT IT DOES: One frame of a car's sounds. Silenced (or carless), its
 * engine channel is zeroed. The listener is the viewed car's camera with one
 * view on screen, else this car's own; a car or view in the close camera
 * plays nothing. The engine pitch folds the RPM into Hz times the car's
 * Doppler factor (cleared outside [0, 100000]) as a 32.32 ratio of 11000 Hz,
 * with the panned level pair; car 0 sets the 11000 Hz override where the
 * track asks. Three pending knocks take over the surface state with their own
 * priority and rumble pulse; otherwise the surface impact picks the loop and
 * rumble by the ground under wheel 1 (snow has its own), building over up to
 * 16 hits; with neither, the rolling loop, loud as the speed. Car 0 starts
 * the loop when it changes and writes its pitch and level; the listener is
 * kept for next frame's Doppler. PC twin: BrSndCarStep.
 * Source facts: the knock bytes read once into n; the state range tests as
 * the skip condition (0, <4, <8, <8, <13 in turn); an unused int first sizes
 * the 0x78 frame.
 * RESIDUE (838): the impact compare keeps n in v0 with a copy in v1 where
 * ours uses one register, and the temporaries rotate after it. */
/* @t4-pass 0x8022BCB4 1 2026-09-29 compiles 100 best 837 moved 1  (n64/tools/n64permute.py) */
/* @t4-pass 0x8022BCB4 2 2026-09-29 compiles 100 best 837 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x8022BCB4 */
/* @implements 0x8022BCB4 tgr BrSndCarStep */
void BrSndCarStep(BrCar *car)
{
  int u;
  float f;
  int k;
  BrCar *view;
  BrCarCam *lst;
  int remote;
  float r;
  float l;
  int vol;
  int prev;
  int ground;
  int n;
  BrVec3 *pos;
  BrRaceSnd *ch;
  float *p;
  view = &D_8031B760[D_8031B2C8[0].car];
  k = car->slot * 2;
  if (D_8026FF10 != 0 || car->link == 0) {
    D_802A4920[k].level = 0;
    D_802A4920[k].pitch = 0;
    return;
  }
  remote = 0;
  if (D_8028AB0C == 1) {
    lst = TGR_PTR(BrCarCam *, view->cam);
    if (car->xf48 != 0 || view->xf48 != 0) {
      remote = 1;
    }
  } else {
    lst = TGR_PTR(BrCarCam *, car->cam);
    if (car->xf48 != 0 || view->xf48 != 0 || D_8031B760[D_8031B2C8[1].car].xf48 != 0) {
      remote = 1;
    }
  }
  pos = (BrVec3 *)car->mtx0[3];
  car->xf44 = BrSndDoppler(pos, &car->posPrev, (BrVec3 *)lst->mtx[3], &car->posStart);
  BrSndPan(pos, lst, &r, &l, &vol, 0);
  if (car->xdf4 > 0.0f) {
    f = car->xdf4 * 0.5f * 0.0007142857f * 11000.0f;
  } else {
    f = car->xdf4 * -0.5f * 0.0007142857f * 11000.0f;
  }
  f = car->xf44 * f;
  if (f > 100000.0f) {
    f = 0.0f;
  } else if (f < 0.0f) {
    f = 0.0f;
  }
  if (car->slot == 0) {
    if (BES16(D_80025C60[car->x1fc0[0]].flags) & 0x10) {
      D_802A49B0 = 11000;
    } else {
      D_802A49B0 = 0;
    }
  }
  if (remote == 0) {
    D_802A4920[k].pitch = tgr_f2ull(f * 9.090909e-05f * 4294967296.0);
    f = vol;
    D_802A4920[k].level = tgr_f2u(((int)(f * r) << 16) + l * f);
  }
  ch = &D_802A4920[k];
  prev = car->xf38;
  if (ch[1].pitch == 0 || (car->sndImpact == 0 && prev >= 4 && prev < 8)) {
    car->xf38 = 0;
    car->xf3c = 0;
    car->xf40 = 0;
  }
  n = car->sndHitA;
  if (car->xf3c < n || (n != 0 && car->xf40 < 100)) {
    car->xf3c = n;
    car->xf38 = 1;
    car->xf40 = 100;
    if (car->slot < D_8026FF08) {
      D_802A4BFC[car->slot] = 0;
      D_802A4BF0[car->slot] = 0;
      D_802A4BF4[car->slot] = 5;
      D_802A4BF8[car->slot] = 1;
      D_802A4C00[car->slot] = 30;
      D_802A4BEC[car->slot] = 1;
    }
  }
  car->sndHitA = 0;
  n = car->sndHitB;
  if (car->xf3c < n || (n != 0 && car->xf40 < 90)) {
    car->xf3c = n;
    car->xf38 = 2;
    car->xf40 = 90;
    if (car->slot < D_8026FF08) {
      D_802A4BFC[car->slot] = 0;
      D_802A4BF0[car->slot] = 0;
      D_802A4BF4[car->slot] = 100;
      D_802A4BF8[car->slot] = 1;
      D_802A4C00[car->slot] = 30;
      D_802A4BEC[car->slot] = 1;
    }
  }
  car->sndHitB = 0;
  n = car->sndHitC;
  if (car->xf3c < n || (n != 0 && car->xf40 < 80)) {
    car->xf3c = n;
    car->xf38 = 3;
    car->xf40 = 80;
    if (car->slot < D_8026FF08) {
      D_802A4BFC[car->slot] = 0;
      D_802A4BF0[car->slot] = 0;
      D_802A4BF4[car->slot] = 3;
      D_802A4BF8[car->slot] = 2;
      D_802A4C00[car->slot] = 20;
      D_802A4BEC[car->slot] = 1;
    }
  }
  car->sndHitC = 0;
  if (car->xf38 != 0 && (car->xf38 < 4 || car->xf38 > 7) && (car->xf38 < 8 || car->xf38 > 12)) {
    goto level;
  }
  car->x34a = 0;
  ground = -1;
  if (car->wheels[1].x13c != 0) {
    ground = car->wheels[1].surface;
  }
  n = car->sndImpact;
  if (car->xf3c < n || (n != 0 && (car->xf40 < 40 || (n != 0 && car->xf40 == 40 && prev >= 4 && prev < 8)))) {
    car->xf3c = n;
    if (D_8028C800 == 3) {
      switch (ground) {
      case 0:
      case 1:
      case 2:
      case 3:
        car->xf38 = 7;
        car->xf3c = n >> 1;
        break;
      case 4:
        car->xf38 = 7;
        car->xf3c = 0;
        break;
      }
    } else {
      switch (ground) {
      case 0:
      case 3:
        if (car->slot < D_8026FF08 && D_802A4BEC[car->slot] == 0) {
          D_802A4BFC[car->slot] = 0;
          D_802A4BF0[car->slot] = 0;
          D_802A4BF4[car->slot] = 1;
          D_802A4BF8[car->slot] = 9;
          D_802A4C00[car->slot] = 10;
          D_802A4BEC[car->slot] = 1;
        }
        car->xf38 = 7;
        break;
      case 1:
        if (car->slot < D_8026FF08 && D_802A4BEC[car->slot] == 0) {
          D_802A4BFC[car->slot] = 0;
          D_802A4BF0[car->slot] = 0;
          D_802A4BF4[car->slot] = 1;
          D_802A4BF8[car->slot] = 14;
          D_802A4C00[car->slot] = 15;
          D_802A4BEC[car->slot] = 1;
        }
        car->xf38 = 5;
        break;
      case 2:
        if (car->slot < D_8026FF08 && D_802A4BEC[car->slot] == 0) {
          D_802A4BFC[car->slot] = 0;
          D_802A4BF0[car->slot] = 0;
          D_802A4BF4[car->slot] = 1;
          D_802A4BF8[car->slot] = 12;
          D_802A4C00[car->slot] = 13;
          D_802A4BEC[car->slot] = 1;
        }
        car->xf38 = 4;
        break;
      case 4:
        car->xf38 = 12;
        break;
      }
      car->xf3c = car->sndImpact >> 1;
    }
    car->xf40 = 40;
    if (car->sndHits++ > 16) {
      car->sndHits = 16;
    }
    car->xf3c *= car->sndHits;
    car->xf3c /= 12;
  } else {
    car->sndHits = 0;
  }
  if (car->xf38 != 0 && (car->xf38 < 8 || car->xf38 > 12)) {
    goto level;
  }
  if (D_8028C800 == 3) {
    switch (ground) {
    case 0:
    case 1:
    case 2:
    case 3:
      car->xf38 = 11;
      break;
    case 4:
      car->xf38 = 10;
      break;
    default:
      car->xf38 = 0;
      break;
    }
  } else {
    switch (ground) {
    case 0:
    case 3:
      car->xf38 = 10;
      break;
    case 1:
      car->xf38 = 8;
      break;
    case 2:
      car->xf38 = 9;
      break;
    case 4:
      car->xf38 = 12;
      break;
    default:
      car->xf38 = 0;
      break;
    }
  }
  car->xf40 = 1;
  car->xf3c = BrVec3Length(&car->velfd8) * 64.0f / 27.0f;
  if (car->xf3c > 64) {
    car->xf3c = 64;
  }
level:
  if (car->xf38 == 0) {
    if (ch == &D_802A4920[0]) {
      ch[1].level = 0;
      ch[1].pitch = 0;
    }
  } else {
    f = car->xf44 * 11000.0f;
    if (f > 20000.0f) {
      f = 0.0f;
    } else if (f < 0.0f) {
      f = 0.0f;
    }
    if (car->xf38 != prev && k == 0) {
      BrSfxVoicePlay(k + 1, D_8028BC04[car->xf38].start, D_8028BC04[car->xf38].len,
                     D_8028BC04[car->xf38].loop);
    }
    if (remote == 0 && ch == &D_802A4920[0]) {
      n = (car->xf3c * vol) >> 7;
      ch[1].pitch = tgr_f2ull(f * 9.090909e-05f * 4294967296.0);
      f = n;
      ch[1].level = tgr_f2u(((int)(f * r) << 16) + l * f);
    }
  }
  if (1 == D_8028AB0C) {
    p = TGR_PTR(BrCarCam *, view->cam)->mtx[3];
    car->posStart.x = p[0];
    car->posStart.y = p[1];
    car->posStart.z = p[2];
  } else {
    p = TGR_PTR(BrCarCam *, car->cam)->mtx[3];
    car->posStart.x = p[0];
    car->posStart.y = p[1];
    car->posStart.z = p[2];
  }
}
