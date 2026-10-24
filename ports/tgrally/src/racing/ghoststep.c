/* ghoststep.c -- stepping a race entity: a car's fade and control call, or
 * a carless entity (the ghost) along its recorded route
 */
#include "tgr/common.h"
#include "tgr/mix.h"
#include "tgr/ent.h"
#include "tgr/car.h"

/* -- declarations -- */
#include "tgr/track.h"
typedef BrPathSeg BrRoute;          /* a route: a path segment (cartridge data) */

typedef BrMixVoice BrRaceSnd;
extern BrRaceSnd D_802A4920[6];         /* six sound channels */
extern int D_8026FF08;                  /* players */
extern int D_8026FF10;                  /* set: silence the channels and stop */
extern int D_8026FF18;                  /* game mode */
extern float D_8028AAD8;                /* the frame time */
extern float D_802A9D54;
extern float D_802A9D58;
#define D_80025C70 BEPTR(BrPathSeg *, D_80025C00.path)
extern BrCar D_8031B760[];
extern BrRoute *D_8028B824;             /* the path walk's route and point */
extern int D_8028B828;
extern BrVec3 D_8031B750;               /* and position */
void BrCtlAi(void *car);
float BrVec3Length(BrVec3 *v);
void BrPathWalk(BrRoute *route, int pt, float t, float step);
void BrVec3Sub(BrVec3 *pOut, BrVec3 *pA, BrVec3 *pB);
void BrVec3ScaleBy(BrVec3 *v, float s);
void BrRaceGateStep(BrRaceEnt *e);
#define VEC3_COPY(d, s)                                                 \
{                                                                       \
  float *_d = (float *)(d);                                             \
  float *_s = (float *)(s);                                             \
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
 * The position saves read both addresses through e->car, so the store
 * through one may change the other and each load is stored before the
 * next.  The held-at-start arm keeps its own car and control locals; the
 * path fraction is built up in place in a local of the carless arm. */
/* @implements 0x8022C9FC tgr BrGhostPlaybackStep */
void BrGhostPlaybackStep(BrRaceEnt *e)
{
  BrCar *car;
  unsigned int flags;
  void (*ctl)(BrCar *);

  if (e->car != 0 && ((BrCar *)(e->car))->xed8 == TGR_FA(BrCtlAi) && D_8026FF18 != 5) {
    *((unsigned int *)((((BrCar *)(e->car))->pad))) &= 0xF000000;
    ((float *)((unsigned int *)((((BrCar *)(e->car))->pad))))[8] = 0;
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
  if (flags & 1) {
    if (e->car != 0) {
      BrCar *car;
      void (*ctl)(BrCar *);

      *((unsigned int *)((((BrCar *)(e->car))->pad))) |= 0x40000;
      ((BrCar *)(e->car))->xe40 = 0;
      VEC3_COPY(&((BrCar *)(e->car))->posPrev, ((BrCar *)(e->car))->mtx0[3]);
      car = ((BrCar *)(e->car));
      ctl = ((void (*)(BrCar *))(car->xed8));
      if (ctl != 0) {
        ctl(car);
      }
      ((BrCar *)(e->car))->xe40 = 0;
    }
  } else if (flags & 2) {
    if (e->car != 0) {
      if (D_8026FF18 != 0 || e->x64 < D_8026FF08) {
        *((unsigned int *)((((BrCar *)(e->car))->pad))) = 0xC0000;
        ((signed char *)((unsigned int *)((((BrCar *)(e->car))->pad))))[0x24] = -0x7F;
        ((float *)((unsigned int *)((((BrCar *)(e->car))->pad))))[8] = -1.0f;
      }
      if (D_8026FF18 == 0 && e->x64 >= D_8026FF08) {
        ((BrCar *)(e->car))->colour[3] = 2;
        ((BrCar *)(e->car))->x2064 -= D_8028AAD8;
        if (((BrCar *)(e->car))->x2064 < 0.0f) {
          ((BrCar *)(e->car))->x2064 = 0.0f;
        }
      }
      VEC3_COPY(&((BrCar *)(e->car))->posPrev, ((BrCar *)(e->car))->mtx0[3]);
      car = ((BrCar *)(e->car));
      ctl = ((void (*)(BrCar *))(car->xed8));
      if (ctl != 0) {
        ctl(car);
      }
    }
  } else if (e->car != 0) {
    if (((BrCar *)(e->car))->colour[3] == 2) {
      ((BrCar *)(e->car))->x2064 += D_8028AAD8 * D_802A9D54;
      if (D_8026FF18 == 2 && e->x64 != 0) {
        if (((BrCar *)(e->car))->x2064 > 0.375f) {
          ((BrCar *)(e->car))->x2064 = 0.375f;
        }
      } else if (((BrCar *)(e->car))->x2064 >= 1.0f) {
        ((BrCar *)(e->car))->x2064 = 1.0f;
        ((BrCar *)(e->car))->colour[3] = 0;
      }
    }
    VEC3_COPY(&((BrCar *)(e->car))->posPrev, ((BrCar *)(e->car))->mtx0[3]);
    car = ((BrCar *)(e->car));
    ctl = ((void (*)(BrCar *))(car->xed8));
    if (ctl != 0) {
      ctl(car);
    }
    ((BrCar *)(e->car))->xfe4[1] += ((BrCar *)(e->car))->xfe4[0] * D_8028AAD8;
  } else {
    float t;

    e->posPrev.x = e->pos.x;
    e->posPrev.y = e->pos.y;
    e->posPrev.z = e->pos.z;
    t = BEF(D_80025C70->pt[0].dist) * (e->x44 + 1) - e->progress;
    t -= BEF(((BrRoute *)(e->route))->pt[e->pt + 1].dist);
    t /= BEF(((BrRoute *)(e->route))->pt[e->pt].dist) - BEF(((BrRoute *)(e->route))->pt[e->pt + 1].dist);
    if (D_8031B760[D_8026FF08 + e->group].link != 0) {
      BrPathWalk(((BrRoute *)(e->route)), e->pt, t, BrVec3Length(&D_8031B760[D_8026FF08 + e->group].velfd8) * D_8028AAD8);
      e->progress += BrVec3Length(&D_8031B760[D_8026FF08 + e->group].velfd8) * D_8028AAD8;
    } else {
      BrPathWalk(((BrRoute *)(e->route)), e->pt, t, 2.22f);
      e->progress = e->progress + D_802A9D58;
    }
    e->route = (D_8028B824);
    e->pt = D_8028B828;
    VEC3_COPY(&e->pos, &D_8031B750);
    BrVec3Sub(&e->vel, &e->pos, &e->posPrev);
    BrVec3ScaleBy(&e->vel, 1.0f / D_8028AAD8);
    BrRaceGateStep(e);
  }
}
