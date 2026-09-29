/* rank.c -- ordering the cars into race positions
 */
#include "tgr/common.h"
#include "tgr/car.h"

/* -- declarations -- */
typedef struct BrRaceEnt {      /* a race entity (0x78 bytes): the cars, then the rest */
  BrVec3 pos;                   /* 0x00  (an AI slot: its car's position when saved) */
  BrVec3 posPrev;               /* 0x0C */
  BrVec3 vel;                   /* 0x18 */
  float x24;                    /* 0x24  the car's 0xFE4 */
  int seg;                      /* 0x28  its route segment and point */
  int pt;                       /* 0x2C */
  float raceTime;               /* 0x30 */
  float x34;                    /* 0x34 */
  float lapTime;                /* 0x38 */
  float x3c;                    /* 0x3C */
  int laps;                     /* 0x40 */
  int x44;                      /* 0x44 */
  int x48;                      /* 0x48 */
  int x4c;                      /* 0x4C */
  float progress;               /* 0x50  for an entity that is not a car */
  int rank;                     /* 0x54  its race position, likewise */
  char pad58[4];
  unsigned char colour[3];      /* 0x5C  the body colour of the slot's car */
  char pad5f;
  BrCar *car;                   /* 0x60  0 for an entity that is not a car */
  int x64;                      /* 0x64  the car slot it stands for */
  unsigned int flags;           /* 0x68  bit 0: held at the start, bit 1: finished */
  char pad6c[0x74 - 0x6C];
  int group;                    /* 0x74  which half of the field (0 or 1) */
} BrRaceEnt;
typedef struct BrRankEntry {    /* one row of the ranking table */
  float key;
  int ent;
} BrRankEntry;
void BrQsort(void *base, unsigned int num, unsigned int width, int (*comp)(void *, void *));
extern int D_8028B7F0;                  /* entity count */
extern BrRaceEnt D_803239A0[];
struct BrCarWheel {              /* a wheel's rigid body */
  char pad000[0x19C];
  int x19c;                     /* 0x19C */
  unsigned char x1a0;           /* 0x1A0 */
  char pad1a1[0x1B4 - 0x1A1];
  int x1b4;                     /* 0x1B4 */
};
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
extern BrTrackHdr *D_80025C70;
extern int D_8026FF08;                  /* players */
extern int D_8026FF18;                  /* game mode */
int osSyncPrintf(char *fmt, ...);
float BrVec3Dist(BrVec3 *pA, BrVec3 *pB);
void BrVec3Sub(BrVec3 *pOut, BrVec3 *pA, BrVec3 *pB);
void BrVec3Direction(float *pOut, BrVec3 *pA, BrVec3 *pB);
void BrVec3Midpoint(float *pOut, BrVec3 *pA, BrVec3 *pB);
void BrVec3Cross(float *pOut, float *pA, float *pB);
void BrVec3Normalise(float *pV);
void BrCarPhysInit(BrCar *car);
void BrAiLaneSetup(BrCar *car);
void BrCarSetPos(BrCar *car, float x, float y, float z);
void BrEntPaintTexture(char *model, int r, int g, int b);
void BrCarPlaceAt(BrCar *car, float m[4][4]);
void BrCarSetVel(BrCar *car, float x, float y, float z);
/* -- end declarations -- */

/* WHAT IT DOES: qsort comparator for the ranking table: entries are keyed
 * by a leading float, larger first sorts later (1 when a's key is larger,
 * -1 when smaller, 0 when equal).  The PC twin is BrRankCmpKey. */
/* @implements 0x80229510 tgr BrRankCmpKey */
int BrRankCmpKey(void *pA, void *pB)
{
  if (*(float *)pA > *(float *)pB) {
    return 1;
  }
  if (*(float *)pA < *(float *)pB) {
    return -1;
  }
  return 0;
}

/* WHAT IT DOES: Work out the race positions: every entity not yet finished
 * is keyed by its progress (the car's at 0xFA8, or the entity's own at 0x50
 * for one that is not a car), the keys are sorted ascending with
 * BrRankCmpKey, and each gets position count-1-k in its car's 0xFAC or its
 * own 0x54 (more progress, lower number).  The locals are declared tab, i,
 * n: IDO gives them their stack slots top-down in that order. */
/* @implements 0x80229550 tgr BrRankUpdate */
void BrRankUpdate(void)
{
  BrRankEntry tab[20];
  int i;
  int n;

  n = 0;
  for (i = 0; i < D_8028B7F0; i++) {
    if (!(D_803239A0[i].flags & 2)) {
      tab[n].ent = i;
      if (D_803239A0[i].car != 0) {
        tab[n++].key = D_803239A0[i].car->xfa8;
      } else {
        tab[n++].key = D_803239A0[i].progress;
      }
    }
  }
  if (n != 0) {
    BrQsort(tab, n, 8, BrRankCmpKey);
  }
  for (i = 0; i < n; i++) {
    if (D_803239A0[tab[i].ent].car != 0) {
      D_803239A0[tab[i].ent].car->xfac = D_8028B7F0 - i - 1;
    } else {
      D_803239A0[tab[i].ent].rank = D_8028B7F0 - i - 1;
    }
  }
}

/* WHAT IT DOES: Hand the AI car slots round so the cars near this car are
 * the ones simulated.  Every entity is keyed by its lap-wrapped distance
 * along the track from this car, squared, plus its straight-line distance
 * (a car of another player far behind, or a finished car stopped in the
 * pits, keyed out of the way), this car's own entry is swapped to the front
 * and the rest sorted.  Then for each half of the field: the first AI entity
 * in key order that has a finished, stopped car, or any later one whose car
 * is more than 80000 away and not already moving slots, gives its car up --
 * the car's position, route point, clocks and lap state are saved into the
 * entity -- and a half with no car at all gets its slot opened.  The freed
 * cars then go, nearest first, to the unfinished entities without one: the
 * saved state is restored, the car set down 0.1 above its saved position,
 * repainted in the entity's colour, turned along its route and set moving
 * at 50 (or held, at the start).  The car's own list of entities by key is
 * written as it goes.  The PC twin is BrLapSaveRestore.
 * Source facts: the scoring loop indexes list[] and D_803239A0[] (IDO
 * strength-reduces both); the route point is int arithmetic (seg + pt * 40,
 * which keeps 40 in fp for the multu and the 0x340 store); the second
 * x2064 = 1.0 is a double literal, which gives it its own register (f26 vs
 * f30) as in the ROM; the slot tests read ent->car and the save takes it.
 * RESIDUE (443): the ROM frame is 0x18 smaller (fewer spill temps; every
 * named slot is 0x18 lower), &free[nFree] is formed in both save paths
 * before the jump, and the restore loop's pointer set-up is ordered
 * differently. */
/* @t4-pass 0x80229700 1 2026-09-29 compiles 198 best 447 moved 15  (n64/tools/n64permute.py) */
/* @t4-pass 0x80229700 2 2026-09-29 compiles 198 best 443 moved 4  (n64/tools/n64permute.py) */
/* @implements 0x80229700 tgr BrCarSlotSwap */
void BrCarSlotSwap(BrCar *me)
{
  BrRankEntry list[20];
  int u0[4];
  float d;
  BrCar *free[7];
  int u1[2];
  int u2[6];
  int group;
  BrVec3 tA;
  BrVec3 tB;
  BrRaceEnt *ent;
  BrCar *car;
  BrRankEntry *e;
  float lap;
  float t;
  int i;
  int seen;
  int nFree;
  int open;
  int r;

  lap = D_80025C70->lapLen;
  for (i = 0; i < D_8028B7F0; i++) {
    car = D_803239A0[i].car;
    if (car != 0) {
      if (car != me && car->slot < D_8026FF08) {
        list[i].key = 1e10f;
      } else if (D_8026FF18 == 0 && car->slot >= D_8026FF08 && (car->link->flags & 2) &&
                 car->colour[3] == 2 && car->x2064 == 0.0f) {
        list[i].key = 1e9f;
      } else {
        d = (me->xfa8 - car->xfa8) + (float)(car->xf7c - me->xf7c) * lap;
        if (lap * 0.5f < d) {
          d -= lap;
        } else if (d < lap * -0.5f) {
          d += lap;
        }
        list[i].key = BrVec3Dist((BrVec3 *)me->mtx0[3], (BrVec3 *)car->mtx0[3]) + d * d;
      }
    } else if (D_8026FF18 == 0 && D_803239A0[i].x64 >= D_8026FF08 && (D_803239A0[i].flags & 2)) {
      list[i].key = 1e9f;
    } else {
      d = (float)(D_803239A0[i].x44 - me->xf7c) * lap + (me->xfa8 - D_803239A0[i].progress);
      if (lap * 0.5f < d) {
        d -= lap;
      } else if (d < lap * -0.5f) {
        d += lap;
      }
      list[i].key = BrVec3Dist((BrVec3 *)me->mtx0[3], &D_803239A0[i].pos) + d * d;
    }
    list[i].ent = i;
  }
  if (D_8028B7F0 > 1) {
    t = list[me->slot].key;
    list[me->slot].key = list[0].key;
    list[0].key = t;
    list[0].ent = me->slot;
    list[me->slot].ent = 0;
    BrQsort(&list[1], D_8028B7F0 - 1, 8, BrRankCmpKey);
  }
  group = 0;
  do {
    open = 1;
    seen = 0;
    nFree = 0;
    for (i = 0, e = list; i < D_8028B7F0; i++, e++) {
      ent = &D_803239A0[e->ent];
      if (ent->group != group || ent->x64 < D_8026FF08) {
        continue;
      }
      if (seen < 1) {
        seen++;
        if (0 == ent->car) {
          continue;
        }
        open = 0;
        if (!(ent->flags & 2) || ent->car->colour[3] != 2 || ent->car->x2064 != 0.0f) {
          continue;
        }
      } else {
        if (ent->car == 0) {
          continue;
        }
        open = 0;
        if (ent->car->xed4 != 0 || !(80000.0 < e->key)) {
          continue;
        }
      }
      car = ent->car;
      osSyncPrintf("enemy %d loses slot %d (dist=%f)\n", ent->x64, car->slot, e->key);
      ent->pos.x = car->mtx0[3][0];
      ent->pos.y = car->mtx0[3][1];
      ent->pos.z = car->mtx0[3][2];
      ent->vel.x = car->velfd8.x;
      ent->vel.y = car->velfd8.y;
      ent->vel.z = car->velfd8.z;
      ent->seg = car->xf5c;
      ent->x24 = car->xfe4[0];
      ent->pt = car->xf60;
      ent->x44 = car->xf7c;
      ent->raceTime = car->raceTime;
      ent->x34 = car->xf98;
      ent->lapTime = car->lapTime;
      ent->x3c = car->xfa4;
      ent->laps = car->laps;
      ent->x4c = car->xf74;
      ent->x48 = car->xf70;
      osSyncPrintf("saving lap (%d/%d) and gate (%d/%d)\n", ent->laps, ent->x44, ent->x48, ent->x4c);
      ent->progress = car->xfa8;
      ent->rank = car->xfac;
      free[nFree++] = ent->car;
      ent->car = 0;
      car->xed4 = 60;
      car->link = 0;
    }
    i = 0;
    if (open) {
      osSyncPrintf("init slot %d to being open\n", group + D_8026FF08);
      free[nFree++] = &D_8031B760[D_8026FF08 + group];
    }
    for (e = &list[i]; nFree != 0 && i < D_8028B7F0; i++, e++) {
      ent = &D_803239A0[e->ent];
      if (ent->group == group && ent->x64 >= D_8026FF08 && !(ent->flags & 2) && ent->car == 0) {
        car = free[--nFree];
        ent->car = car;
        osSyncPrintf("enemy %d acquires slot %d\n", ent->x64, car->slot);
        car->xfe4[0] = ent->x24;
        car->xf5c = ent->seg;
        car->xf60 = ent->pt;
        car->raceTime = ent->raceTime;
        car->xf98 = ent->x34;
        car->lapTime = ent->lapTime;
        car->xfa4 = ent->x3c;
        car->laps = ent->laps;
        car->xf70 = ent->x48;
        car->xf74 = ent->x4c;
        car->xf7c = ent->x44;
        osSyncPrintf("restoring lap (%d/%d) and gate (%d/%d)\n", car->laps, car->xf7c, car->xf70, car->xf74);
        car->xfac = ent->rank;
        car->link = (BrCarLink *)ent;
        car->xfa8 = ent->progress;
        car->colour[3] = 0;
        car->x2064 = 1.0f;
        car->xf48 = 1;
        BrCarPhysInit(car);
        BrAiLaneSetup(car);
        car->posPrev.x = ent->posPrev.x;
        car->posPrev.y = ent->posPrev.y;
        car->posPrev.z = ent->posPrev.z;
        BrCarSetPos(car, ent->pos.x, ent->pos.y, 0.1f + ent->pos.z);
        car->colour[3] = 0;
        car->x2064 = 1.0;
        car->colour[0] = ent->colour[0];
        car->colour[1] = ent->colour[1];
        car->colour[2] = ent->colour[2];
        BrEntPaintTexture(car->model, ent->colour[0] >> 3, ent->colour[1] >> 3, ent->colour[2] >> 3);
        r = car->xf5c + car->xf60 * 40;
        BrVec3Sub(&tA, (BrVec3 *)(r + 0x40), (BrVec3 *)(r + 0x4c));
        r = car->xf5c + car->xf60 * 40;
        BrVec3Sub(&tB, (BrVec3 *)(r + 0x68), (BrVec3 *)(r + 0x74));
        r = car->xf5c + car->xf60 * 40;
        BrVec3Direction(car->mtx0[0], (BrVec3 *)(r + 0x4c), (BrVec3 *)(r + 0x74));
        BrVec3Midpoint(car->mtx0[1], &tA, &tB);
        BrVec3Cross(car->mtx0[2], car->mtx0[0], car->mtx0[1]);
        BrVec3Normalise(car->mtx0[2]);
        BrVec3Cross(car->mtx0[1], car->mtx0[2], car->mtx0[0]);
        BrVec3Normalise(car->mtx0[1]);
        car->xf64 = car->mtx0[0][0];
        car->xf68 = car->mtx0[0][1];
        car->xf6c = car->mtx0[0][2];
        BrCarPlaceAt(car, car->mtx0);
        if (car->link->flags & 1) {
          BrCarSetVel(car, 0.0f, 0.0f, 0.0f);
        } else {
          BrCarSetVel(car, car->mtx0[0][0] * 50.0f, car->mtx0[0][1] * 50.0f, 50.0f * car->mtx0[0][2]);
        }
        car->wheel[0]->x19c = 0;
        car->wheel[0]->x1b4 = 0;
        car->wheel[0]->x1a0 = 2;
        car->wheel[1]->x19c = 0;
        car->wheel[1]->x1b4 = 0;
        car->wheel[1]->x1a0 = 2;
        car->wheel[3]->x19c = 0;
        car->wheel[3]->x1b4 = 0;
        car->wheel[3]->x1a0 = 2;
        car->wheel[2]->x19c = 0;
        car->wheel[2]->x1b4 = 0;
        car->wheel[2]->x1a0 = 2;
        car->xe70[0] = 0;
        car->xe70[1] = 0;
        car->xe70[2] = 0;
        car->xe70[3] = -180;
        car->x340 = 40;
        car->xdf0 = 0.0f;
      }
      ((BrRaceEnt **)me->pade80)[i] = &D_803239A0[e->ent];
    }
  } while (++group != 2);
}
