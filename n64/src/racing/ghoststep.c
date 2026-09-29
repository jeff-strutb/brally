/* ghoststep.c -- stepping a race entity: a car's fade and control call, or
 * a carless entity (the ghost) along its recorded route
 */
#include "tgr/common.h"
#include "tgr/car.h"

/* -- declarations -- */
typedef struct BrRoutePt {      /* a route point (0x28 bytes) */
  BrVec3 left;
  BrVec3 pos;
  BrVec3 right;
  float dist;
} BrRoutePt;
typedef struct BrRoute {
  char pad00[0x40];
  BrRoutePt pt[1];              /* 0x40 */
} BrRoute;
typedef struct BrTrackHdr {
  char pad00[0x64];
  float lapLen;                 /* 0x64  a lap's length */
} BrTrackHdr;
typedef struct BrRaceEnt {      /* a race entity (0x78 bytes): the cars, then the rest */
  BrVec3 pos;                   /* 0x00 */
  BrVec3 posPrev;               /* 0x0C */
  BrVec3 vel;                   /* 0x18 */
  float x24;                    /* 0x24 */
  BrRoute *route;               /* 0x28  its route and point on it */
  int pt;                       /* 0x2C */
  float raceTime;               /* 0x30 */
  float x34;                    /* 0x34 */
  float lapTime;                /* 0x38 */
  float x3c;                    /* 0x3C */
  int laps;                     /* 0x40 */
  int x44;                      /* 0x44  laps completed on the route */
  int x48;                      /* 0x48 */
  int x4c;                      /* 0x4C */
  float progress;               /* 0x50  distance run */
  int rank;                     /* 0x54 */
  char pad58[4];
  unsigned char colour[3];      /* 0x5C */
  char pad5f;
  BrCar *car;                   /* 0x60  0 for an entity that is not a car */
  int x64;                      /* 0x64  the car slot it stands for */
  unsigned int flags;           /* 0x68  bit 0: held at the start, bit 1: finished */
  char pad6c[0x74 - 0x6C];
  int group;                    /* 0x74  which half of the field (0 or 1) */
} BrRaceEnt;
typedef struct BrRaceSnd {     /* a sound channel (0x18 bytes) */
  void *p;
  int x4;
  long long x8;
  int x10;
  int x14;
} BrRaceSnd;
extern BrRaceSnd D_802A4920[6];         /* six sound channels */
extern int D_8026FF08;                  /* players */
extern int D_8026FF10;                  /* set: silence the channels and stop */
extern int D_8026FF18;                  /* game mode */
extern float D_8028AAD8;                /* the frame time */
extern float D_802A9D54;
extern float D_802A9D58;
extern BrTrackHdr *D_80025C70;
extern BrCar D_8031B760[];
extern BrRoute *D_8028B824;             /* the path walk's route and point */
extern int D_8028B828;
extern BrVec3 D_8031B750;               /* and position */
void BrCtlAi(int ctl);
float BrVec3Length(BrVec3 *v);
void BrPathWalk(BrRoute *route, int pt, float t, float step);
void BrVec3Sub(BrVec3 *pOut, BrVec3 *pA, BrVec3 *pB);
void BrVec3ScaleBy(BrVec3 *v, float s);
void BrRaceGateStep(BrRaceEnt *e);
#define VEC3_COPY(d, s)                                                 \
{                                                                       \
  float *_s = (float *)(s);                                             \
  float *_d = (float *)(d);                                             \
                                                                        \
  _d[0] = _s[0];                                                        \
  _d[1] = _s[1];                                                        \
  _d[2] = _s[2];                                                        \
}
/* -- end declarations -- */

/* WHAT IT DOES: Step one race entity for the frame. An AI car outside mode
 * 5 has its pad cleared first; the silence flag clears five sound
 * channels and stops. A car held at the start keeps its pad pressed and
 * runs its control function; a finished car brakes (in a race it fades
 * out over a second), saves its position and runs its control; a racing car fades back in,
 * saves its position, runs its control and accumulates its distance. An
 * entity with no car walks its route: the fraction along the current
 * segment where its distance falls, advanced by the watched car's speed (or
 * a fixed step), then takes the walk's route, point and position, derives
 * its velocity and runs the timing gates.
 * RESIDUE (334, 32 bytes short): the ROM copies the car's position with
 * both addresses in registers (src = car + 0x30, dst = car + 0xF50) and each
 * load stored before the next, as if it could not tell the two apart; every
 * spelling tried here folds the offsets.  The frame is 0x60 against 0x58
 * here, with the path fraction spilled at 0x30. */
/* @t4-pass 0x8022C9FC 1 2026-09-29 compiles 99 best 334 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8022C9FC 2 2026-09-29 compiles 99 best 334 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x8022C9FC tgr BrGhostPlaybackStep */
void BrGhostPlaybackStep(BrRaceEnt *e)
{
  BrCar *car;
  unsigned int flags;
  float t;
  void (*ctl)(BrCar *);

  if (e->car != 0 && e->car->xed8 == (int)BrCtlAi && D_8026FF18 != 5) {
    *e->car->pad &= 0xF000000;
    ((float *)e->car->pad)[8] = 0.0f;
  }
  if (D_8026FF10 != 0) {
    D_802A4920[0].x8 = 0;
    D_802A4920[0].x14 = 0;
    D_802A4920[1].x8 = 0;
    D_802A4920[1].x14 = 0;
    D_802A4920[2].x8 = 0;
    D_802A4920[2].x14 = 0;
    D_802A4920[3].x8 = 0;
    D_802A4920[3].x14 = 0;
    D_802A4920[4].x8 = 0;
    D_802A4920[4].x14 = 0;
    return;
  }
  flags = e->flags;
  car = e->car;
  if (flags & 1) {
    if (car != 0) {
      *car->pad |= 0x40000;
      e->car->xe40 = 0;
      car = e->car;
      VEC3_COPY(&car->posPrev, car->mtx0[3]);
      ctl = (void (*)(BrCar *))e->car->xed8;
      if (ctl != 0) {
        ctl(e->car);
      }
      e->car->xe40 = 0;
    }
  } else if (flags & 2) {
    if (car != 0) {
      if (D_8026FF18 != 0 || e->x64 < D_8026FF08) {
        *car->pad = 0xC0000;
        ((signed char *)e->car->pad)[0x24] = -0x7F;
        ((float *)e->car->pad)[8] = -1.0f;
      }
      if (D_8026FF18 == 0 && e->x64 >= D_8026FF08) {
        e->car->colour[3] = 2;
        e->car->x2064 -= D_8028AAD8;
        if (e->car->x2064 < 0.0f) {
          e->car->x2064 = 0.0f;
        }
      }
      car = e->car;
      VEC3_COPY(&car->posPrev, car->mtx0[3]);
      ctl = (void (*)(BrCar *))e->car->xed8;
      if (ctl != 0) {
        ctl(e->car);
      }
    }
  } else if (car != 0) {
    if (car->colour[3] == 2) {
      car->x2064 += D_8028AAD8 * D_802A9D54;
      if (D_8026FF18 == 2 && e->x64 != 0) {
        if (e->car->x2064 > 0.375f) {
          e->car->x2064 = 0.375f;
        }
      } else if (e->car->x2064 >= 1.0f) {
        e->car->x2064 = 1.0f;
        e->car->colour[3] = 0;
      }
    }
    car = e->car;
    VEC3_COPY(&car->posPrev, car->mtx0[3]);
    ctl = (void (*)(BrCar *))e->car->xed8;
    if (ctl != 0) {
      ctl(e->car);
    }
    car = e->car;
    car->xfe4[1] = car->xfe4[1] + car->xfe4[0] * D_8028AAD8;
  } else {
    e->posPrev.x = e->pos.x;
    e->posPrev.y = e->pos.y;
    e->posPrev.z = e->pos.z;
    t = ((D_80025C70->lapLen * (e->x44 + 1) - e->progress) - e->route->pt[e->pt + 1].dist) /
        (e->route->pt[e->pt].dist - e->route->pt[e->pt + 1].dist);
    if (D_8031B760[D_8026FF08 + e->group].link != 0) {
      BrPathWalk(e->route, e->pt, t, BrVec3Length(&D_8031B760[D_8026FF08 + e->group].velfd8) * D_8028AAD8);
      e->progress = e->progress + BrVec3Length(&D_8031B760[D_8026FF08 + e->group].velfd8) * D_8028AAD8;
    } else {
      BrPathWalk(e->route, e->pt, t, 2.22f);
      e->progress = e->progress + D_802A9D58;
    }
    e->route = D_8028B824;
    e->pt = D_8028B828;
    VEC3_COPY(&e->pos, &D_8031B750);
    BrVec3Sub(&e->vel, &e->pos, &e->posPrev);
    BrVec3ScaleBy(&e->vel, 1.0f / D_8028AAD8);
    BrRaceGateStep(e);
  }
}
