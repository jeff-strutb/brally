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
typedef struct BrGate {         /* a timing gate across the track (0x14 bytes) */
  float postA[2];               /* its two posts, x/y */
  float postB[2];
  float award;                  /* seconds it would grant (printed only) */
} BrGate;
typedef struct BrRaceTrack {
  char pad00[0x70];
  BrTrackHdr *hdr;              /* 0x70 */
  char pad74[0x98 - 0x74];
  BrGate gates[10];             /* 0x98 */
  int nGates;                   /* 0x160 */
} BrRaceTrack;
extern BrRaceTrack D_80025C00;
typedef struct BrTrackAwards {
  char pad00[0x2C];
  float award[4][3][7];         /* 0x2C  [car class][difficulty][checkpoint]; [6] is the start time */
} BrTrackAwards;
extern BrTrackAwards *D_80271D1C[];     /* per track */
extern int D_8028B304;                  /* laps in the race */
extern int D_8028B300;                  /* cars finished */
extern int D_8028B940;                  /* the track */
extern int D_8028C800;                  /* the weather (difficulty row + 1) */
extern int D_80270788;                  /* replay */
extern char *D_8028BABC[];              /* the finishing-place messages */
int BrSegmentsOverlapXY(float *pA, float *pB, BrVec3 *pC, BrVec3 *pD);
int BrFloatToInt(float x);
void BrSfxSrcBeep(void);
int strlen(char *s);
void BrTimeFormat(char *buf, float t);
int sprintf(char *buf, char *fmt, ...);
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
/* @t3 0x80229700 */
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

/* WHAT IT DOES: One entity's gate and lap bookkeeping for a frame, with its
 * car's state copied in and back: crossing the gate behind it goes back one
 * (a lap back at the start line), crossing the next goes forward one; a new
 * furthest gate at the line completes a lap (best-lap and lap-record
 * messages, the lap clock banked, "LAP n" shown; the last lap finishes the
 * race with its place and a course record) and in arcade mode every gate
 * adds the checkpoint's time to each player and shows the time left or the
 * gap; re-crossing the line after backing over it grants the lap it had
 * already earned.  The PC twin is BrRaceGateStep.
 * RESIDUE (622): frame 0x70 vs 0x80 (the ROM's spill slots sit 0x10
 * higher), the ROM keeps &D_8026FF08 in a register through the lap block,
 * and the iNext test branches straight to the standings where ours goes
 * through a jump. */
/* @t4-pass 0x8022A0E0 1 2026-09-29 compiles 120 best 622 moved 9  (n64/tools/n64permute.py) */
/* @t4-pass 0x8022A0E0 2 2026-09-29 compiles 120 best 622 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x8022A0E0 tgr BrRaceGateStep */
void BrRaceGateStep(BrRaceEnt *drv)
{
  BrCar *car;
  BrCar *c;
  BrRaceEnt *ent;
  float tLap;
  char *msg;
  float ratio;
  float d;
  int iCur;
  int iNext;
  int i;
  short w;

  if (D_80025C00.nGates == 0) {
    return;
  }
  car = drv->car;
  if (car != 0) {
    drv->pos.x = car->mtx0[3][0];
    drv->pos.y = car->mtx0[3][1];
    drv->pos.z = car->mtx0[3][2];
    drv->posPrev.x = car->posPrev.x;
    drv->posPrev.y = car->posPrev.y;
    drv->posPrev.z = car->posPrev.z;
    drv->x48 = car->xf70;
    drv->x4c = car->xf74;
    drv->laps = car->laps;
    drv->x44 = car->xf7c;
    drv->raceTime = car->raceTime;
    drv->x34 = car->xf98;
    drv->progress = car->xfa8;
  }
  if (drv->x4c < 0) {
    iCur = (D_80025C00.nGates - (-1 - drv->x4c) % D_80025C00.nGates - 1) % D_80025C00.nGates;
  } else {
    iCur = drv->x4c % D_80025C00.nGates;
  }
  iNext = (iCur + 1) % D_80025C00.nGates;
  tLap = BrFloatToInt(drv->raceTime * 100.0f) / 100.0f;
  if (BrSegmentsOverlapXY(D_80025C00.gates[iCur].postB, D_80025C00.gates[iCur].postA, &drv->posPrev,
                          &drv->pos)) {
    osSyncPrintf("%d went backwards, from gate %d to gate %d\n", drv->x64, drv->x4c, drv->x4c - 1);
    if (iCur == 0) {
      drv->x44--;
    }
    osSyncPrintf("Hmm %d == %d %% %d && %d < 0 ? %d\n", iCur, drv->x48, D_80025C00.nGates, drv->x4c,
                 (drv->x4c + D_80025C00.nGates * 2) % D_80025C00.nGates == drv->x48 % D_80025C00.nGates &&
                     drv->x4c < 0);
    if (iCur == drv->x48 % D_80025C00.nGates && drv->x4c < 0) {
      drv->x4c += D_80025C00.nGates;
      drv->x44++;
      if (D_80025C00.hdr != 0) {
        drv->progress += D_80025C00.hdr->lapLen;
      }
      osSyncPrintf("lapping %d forward to -1\n", drv->x64);
    }
    drv->x4c--;
    goto tail;
  }
  if (!BrSegmentsOverlapXY(D_80025C00.gates[iNext].postB, D_80025C00.gates[iNext].postA, &drv->posPrev,
                           &drv->pos)) {
    goto tail;
  }
  osSyncPrintf("%d went forward, from gate %d to gate %d\n", drv->x64, drv->x4c, drv->x4c + 1);
  drv->x4c++;
  if (drv->x48 < drv->x4c) {
    drv->x48 = drv->x4c;
    osSyncPrintf("moved ahead one gate, getting %f seconds\n", D_80025C00.gates[iNext].award);
    if (iNext != 0) {
      goto positions;
    }
    if (drv->laps < D_8028B304) {
      msg = 0;
      if (drv->x34 == 0.0f || tLap < drv->x34) {
        drv->x34 = tLap;
        if (drv->x64 < D_8026FF08) {
          drv->car->xf9c = drv->laps;
          if (D_8031B760[0].season->x8c[D_8028B940] == 0.0f ||
              drv->x34 < D_8031B760[0].season->x8c[D_8028B940]) {
            D_8031B760[0].season->x8c[D_8028B940] = drv->x34;
            msg = "%ryNEW LAP RECORD!";
          } else if (drv->laps != 0) {
            msg = "%ryBEST LAP!";
          }
        }
      }
      if (car != 0) {
        osSyncPrintf("veh->lapTimeFinal[%d]=%f\n", drv->laps, tLap);
        car->lapTimes[drv->laps] = tLap;
      }
      drv->raceTime -= tLap;
      drv->laps++;
      drv->x44 = drv->laps;
      if (D_8026FF18 == 3) {
        if (drv->laps == 1) {
          osSyncPrintf("lapping %d back to 0 (a)\n", drv->x64);
          drv->laps--;
          drv->x44--;
          drv->x48 -= D_80025C00.nGates;
          drv->x4c -= D_80025C00.nGates;
          if (D_80025C00.hdr != 0) {
            drv->progress -= D_80025C00.hdr->lapLen;
          }
        }
      } else if (car != 0) {
        osSyncPrintf("******* LAP #%d ********\n", drv->laps);
        sprintf(car->xfc0, "%%ryLAP %d", drv->laps + 1);
        car->msgA = (int)car->xfc0;
        car->msgATime = 1.0f;
        if (msg != 0) {
          car->msgB = (int)msg;
          car->msgBTime = 1.0f;
        }
      }
    }
    if (drv->laps == D_8028B304) {
      drv->flags |= 2;
      if (car != 0) {
        car->x2064 = 0.9f;
        car->lapTime = car->lapTime - drv->raceTime;
        drv->raceTime = 0.0f;
        car->xfac = D_8028B300;
        car->msgA = (int)D_8028BABC[D_8028B300];
        car->msgATime = 5.0f;
        if (drv->x64 < D_8026FF08 && D_8028B304 == 3) {
          if (D_8031B760[0].season->xe8[D_8028B940] == 0.0f ||
              car->lapTime < D_8031B760[0].season->xe8[D_8028B940]) {
            D_8031B760[0].season->xe8[D_8028B940] = car->lapTime;
            car->msgB = (int)"%ryNEW COURSE RECORD!";
            car->msgATime = 1.5f;
            car->msgBTime = 3.5f;
          }
        }
      }
      D_8028B300++;
      goto tail;
    }
  positions:
    if (D_8026FF18 == 1 && drv->x64 < D_8026FF08 && 0.0f < drv->car->xfa4) {
      if (D_80270788 == 0) {
        BrSfxSrcBeep();
      }
      d = D_8031B760[0].xfa8 - D_8031B760[1].xfa8;
      if (d < 0.0f) {
        d = -d;
      }
      ratio = drv->car->xfe4[1] / (2.24f * drv->car->lapTime);
      if (ratio != 0.0f) {
        ratio = d / ratio;
      } else {
        ratio = 1000.0f;
      }
      for (i = 0, ent = D_803239A0; i < D_8026FF08; i++, ent++) {
        c = ent->car;
        c->msgA = (int)"%ryCheck Point!";
        c->msgATime = 0.4f;
        w = D_8028C800 - 1;
        if (w >= 3 || w < 0) {
          w = 0;
        }
        c->xfa4 = c->xfa4 + D_80271D1C[D_8028B940]->award[D_8031B760[0].xe34][w][iNext];
        if (D_8026FF08 == 1) {
          sprintf(c->xfc0, "%%ry");
          BrTimeFormat(c->xfc0 + 3, c->xfa4);
          sprintf(c->xfc0 + strlen(c->xfc0), " Left");
          c->msgB = (int)c->xfc0;
          c->msgBTime = 0.9f;
        } else if (c->lapTime != 0.0f) {
          sprintf(c->xfc0, "%%ry");
          BrTimeFormat(c->xfc0 + 3, ratio);
          if (c->xfac != 0) {
            if (120.0f < ratio) {
              c->msgB = (int)"%ryYou're way behind!";
            }
            sprintf(c->xfc0 + strlen(c->xfc0), " Behind");
            c->msgB = (int)c->xfc0;
          } else {
            sprintf(c->xfc0 + strlen(c->xfc0), " Ahead");
            c->msgB = (int)c->xfc0;
          }
          c->msgBTime = 0.9f;
        }
        c->xf70 = drv->x48;
      }
    }
    goto tail;
  }
  if (iNext != 0) {
    goto tail;
  }
  if (drv->laps < ++drv->x44) {
    msg = 0;
    if (drv->x34 == 0.0f || tLap < drv->x34) {
      drv->x34 = tLap;
      if (drv->x64 < D_8026FF08) {
        drv->car->xf9c = drv->laps;
        if (D_8031B760[0].season->x8c[D_8028B940] == 0.0f ||
            drv->x34 < D_8031B760[0].season->x8c[D_8028B940]) {
          D_8031B760[0].season->x8c[D_8028B940] = tLap;
          msg = "%ryNEW LAP RECORD!";
        } else if (drv->laps != 0) {
          msg = "%ryBEST LAP!";
        }
      }
    }
    if (car != 0) {
      osSyncPrintf("veh->lapTimeFinal[%d]=%f\n", drv->laps, tLap);
      car->lapTimes[drv->laps] = tLap;
    }
    drv->raceTime -= tLap;
    drv->laps = drv->x44;
    if (car != 0) {
      osSyncPrintf("******* LAP #%d ********\n", drv->laps);
      sprintf(car->xfc0, "%%ryLAP %d", drv->laps + 1);
      car->msgA = (int)car->xfc0;
      car->msgATime = 1.0f;
      if (msg != 0) {
        car->msgB = (int)msg;
        car->msgBTime = 1.0f;
      }
    }
  }
  osSyncPrintf("granting technically-earned lap %d/%d to %d\n", drv->x44, drv->laps, drv->x64);
  if (D_8026FF18 == 3 && drv->laps == 1) {
    osSyncPrintf("lapping %d back to 0 (b)\n", drv->x64);
    drv->laps--;
    drv->x44--;
    drv->x48 -= D_80025C00.nGates;
    drv->x4c -= D_80025C00.nGates;
    if (0 != D_80025C00.hdr) {
      drv->progress -= D_80025C00.hdr->lapLen;
    }
  }
tail:
  if (car != 0) {
    car->xf70 = drv->x48;
    car->xf74 = drv->x4c;
    car->laps = drv->laps;
    car->xf7c = drv->x44;
    car->raceTime = drv->raceTime;
    car->xf98 = drv->x34;
    car->xfa8 = drv->progress;
  }
}
