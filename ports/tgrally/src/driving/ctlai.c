/* ctlai.c -- the computer drivers
 */
#include "tgr/common.h"
#include "tgr/vec.h"
#include "tgr/car.h"

/* -- declarations -- */
void BrCtlAiBody(BrCar *car);
typedef struct BrAiNode {       /* 0x10 bytes */
  short x0[3];
  char pad6[6];
  unsigned char xc;
  unsigned char xd;
  unsigned char xe;
  unsigned char xf;
} BrAiNode;
typedef struct BrAiCar {        /* the AI's view of a car record */
  char pad000[0x140];
  int slot;                     /* 0x140 */
  char pad144[0x1020 - 0x144];
  float x1020[4];               /* 0x1020 */
  char pad1030[0x1060 - 0x1030];
  float lane[4];                /* 0x1060 */
  int x1070[4];                 /* 0x1070 */
  float x1080[4];               /* 0x1080 */
  int x1090[4];                 /* 0x1090 */
  char pad10a0[0x10d0 - 0x10a0];
  BrAiNode node[0x90];          /* 0x10D0 */
  short x19d0[0x90][3];         /* 0x19D0 */
  short x1d30[36];              /* 0x1D30 */
} BrAiCar;
#include "tgr/track.h"
extern BrVec3 D_8031B750;               /* the point found by BrPathWalk */
extern BrPathSeg *D_8028B824;           /* and its segment */
extern int D_8028B828;                  /* and its point */
void BrVec3Lerp(BrVec3 *pOut, BrVec3 *pA, BrVec3 *pB, float t);
#define D_80025C98 (D_80025C00.gate)     /* the same gates, as their own symbol */
extern int D_8028B82C;                  /* gates crossed by BrPathGates */
extern int D_8028B830;                  /* times it crossed gate 0 */
int BrSegmentsOverlapXY(float *a, float *b, BrVec3 *c, BrVec3 *d);
typedef struct BrCarEnt {       /* a race entity (0x78 bytes): the cars, then the rest */
  BrVec3 pos;                   /* 0x00 */
  BrVec3 prev;                  /* 0x0C */
  char pad18[0x28 - 0x18];
  TgrAddr seg;               /* BrPathSeg * -- 0x28  where it is on the path */
  int pt;                       /* 0x2C */
  float x30;
  float x34;
  float x38;
  float speed;                  /* 0x3C  from the track's time table */
  int laps;                     /* 0x40  times past gate 0 */
  int laps2;                    /* 0x44 */
  int gates;                    /* 0x48  gates passed */
  int gates2;                   /* 0x4C */
  float dist;                   /* 0x50  start distance along the path */
  int x54;
  char pad58[0x5C - 0x58];
  unsigned char rgb[3];         /* 0x5C */
  TgrAddr car;                   /* BrCar * -- 0x60  0 for an entity that is not a car */
  int index;                    /* 0x64 */
  unsigned int flags;           /* 0x68 */
  char pad6c[0x74 - 0x6C];
  int x74;                      /* 0x74  1 for the entities after the thirteen others */
} BrCarEnt;
extern BrCarEnt D_803239A0[];
typedef struct BrTimeLimit {    /* 0x1C bytes */
  float secs;
  char pad04[0x18];
} BrTimeLimit;
typedef struct BrTrackTimes {
  char pad00[0x44];
  BrTimeLimit limit[4][3];      /* 0x44  [class][difficulty] */
} BrTrackTimes;
extern BrTrackTimes *D_80271D1C[];
extern unsigned char D_8028B904[][3];
extern int D_8026FF08;
extern int D_8026FF18;                  /* the game mode */
extern int D_8028B7F0;                  /* entries in D_803239A0 */
extern int D_8028B7F4;                  /* cars in the race */
extern int D_8028B940;                  /* the chosen track */
extern int D_8028C800;                  /* the difficulty, 1-3 */
#define D_80025C70 BEPTR(BrPathSeg *, D_80025C00.path)   /* the track's path */
void BrPathGates(BrPathSeg *seg, float d);
void BrCarResetFrames(BrCar *car);
void BrEntSetRecord(BrCar *car, int slot);
void BrCarSetPos(BrCar *car, float x, float y, float z);
void BrCarSetHeading(BrCar *car, float h);
void BrCarSetVel(BrCar *car, float x, float y, float z);
void BrCarCamInit(BrCar *car);
void BrVec3Direction(BrVec3 *pOut, BrVec3 *pFrom, BrVec3 *pTo);
float cosf(float x);
float sinf(float x);
void BrAiLaneSetup(BrAiCar *);
void BrVec3Sub(BrVec3 *pOut, BrVec3 *pA, BrVec3 *pB);
void BrVec3Midpoint(BrVec3 *pOut, BrVec3 *pA, BrVec3 *pB);
float BrVec3Dot(BrVec3 *pA, BrVec3 *pB);
void BrVec3ScaleBy(BrVec3 *pV, float s);
float BrVec3Dist(BrVec3 *pA, BrVec3 *pB);
void BrVec3Scale(BrVec3 *pOut, BrVec3 *pV, float s);
void BrVec3MulAddTo(BrVec3 *pA, BrVec3 *pB, float s);
void BrVec3Normalise(BrVec3 *pV);
void BrVec3Cross(BrVec3 *pOut, BrVec3 *pA, BrVec3 *pB);
extern int D_8028B800;                  /* depth of the copied corridor edges */
extern int D_8028B804;                  /* deepest clear corridor so far */
extern int D_8028B808;                  /* steering hints */
extern int D_8028B80C;
extern int D_8028B810;                  /* the deepest corridor's point */
extern BrPathSeg *D_8028B814;           /* and segment */
extern int D_8028B818;
extern int D_8028B81C;
extern int D_8028B820;                  /* a level was blocked */
extern BrVec3 D_8031B550[8];            /* right edge pulled in, per depth (from depth 1) */
extern BrVec3 D_8031B5B0[8];            /* midpoint(right, centre) */
extern BrVec3 D_8031B610[8];            /* centre */
extern BrVec3 D_8031B670[8];            /* midpoint(left, centre) */
extern BrVec3 D_8031B6D0[8];            /* left edge pulled in */
extern BrVec3 D_8031B430[8];            /* copied-out right edges */
extern BrVec3 D_8031B490[8];            /* copied-out centres */
extern BrVec3 D_8031B4F0[8];            /* copied-out left edges */
extern BrVec3 D_8031B730;               /* aim point */
extern BrVec3 D_8031B740;               /* half-depth look-ahead point */
void BrAiInputClear(short *);
typedef struct BrMenuPick { char pad00[4]; unsigned char b4; unsigned char b5; } BrMenuPick;
#define D_8031C5BC TGR_PTR(BrMenuPick *, *TGR_PTR(TgrAddr *, 0x8031C5BC))   /* car 0\'s season (a member of D_8031B760) */          /* player one's season: its two option bytes */
extern float D_8028B9F0[][4][2];        /* the AI's pace, by options and entrant */
extern BrVec3 D_8028BAB0;               /* (0, 0, 1) */
extern float D_8028AAD8;                /* seconds this frame */
extern int D_8028B7F0;                  /* entries in D_803239A0 */
void BrPerfMark(int bar, int r, int g, int b, int a);
float BrVec3Length(BrVec3 *v);
void BrCarCamPlaceChase(BrCar *car);
unsigned int BrAiScanCorridor(BrCar *car, int depth, int mid, BrPathSeg *seg);
void BrCarLineFit(BrCar *car);
void BrCarPhysTick(BrCar *car);
void BrCarRespawn(BrCar *car);
/* -- end declarations -- */

/* WHAT IT DOES: Put a car on the starting grid for a race: its record;
 * two columns 3 apart, rows 8 back from the start line by slot (one centred
 * spot in the modes without a grid); facing along the line, stopped, camera
 * set; with a path, the clocks cleared, the time limit for the difficulty
 * and its class, and the path's first direction; lap state cleared (players'
 * cars flagged), the rest of the per-race state zeroed, AI lanes and input
 * reset. */
/* @implements 0x80228A6C tgr BrCarGridStart */
void BrCarGridStart(BrCar *car)
{
  float row;
  float col;
  float c1;
  float s1;
  float c2;
  short k;

  BrCarResetFrames(car);
  if (D_8026FF18 == 2 || D_8026FF18 == 4 || (D_8026FF18 == 3 && D_8026FF08 == 1) || D_8026FF18 == 0) {
    BrEntSetRecord(car, car->slot);
    row = 0.0f;
    col = 0.5f;
  } else {
    BrEntSetRecord(car, car->slot);
    row = car->slot >> 1;
    col = (car->slot & 1) ^ 1;
  }
  c1 = cosf(BEF(D_80025C00.heading));
  s1 = sinf(BEF(D_80025C00.heading));
  c2 = cosf(BEF(D_80025C00.heading) + 1.5707964f);
  BrCarSetPos(car, BEF(D_80025C00.start.x) - c2 * 3.0f * (col - 0.5f) - c1 * 8.0f * (row + 1.0f),
              BEF(D_80025C00.start.y) - sinf(BEF(D_80025C00.heading) + 1.5707964f) * 3.0f * (col - 0.5f) -
                  s1 * 8.0f * (row + 1.0f),
              BEF(D_80025C00.start.z) + 0.1f);
  car->xfa8 = (row + 0.5f) * -8.0f;
  BrCarSetHeading(car, BEF(D_80025C00.heading));
  car->posPrev.x = car->mtx0[3][0];
  car->posStart.x = car->mtx0[3][0];
  car->posPrev.y = car->mtx0[3][1];
  car->posStart.y = car->mtx0[3][1];
  car->posPrev.z = car->mtx0[3][2];
  car->posStart.z = car->mtx0[3][2];
  BrCarCamInit(car);
  car->xe70[0] = 0;
  car->xe70[1] = 0;
  car->xe70[2] = 0;
  car->xe70[3] = -180;
  BrCarSetVel(car, 0.0f, 0.0f, 0.0f);
  if (BES32(D_80025C00.path) != 0) {
    car->raceTime = 0.0f;
    car->lapTime = 0.0f;
    car->xf98 = 0.0f;
    k = D_8028C800 - 1;
    if (k > 2 || k < 0) {
      k = 0;
    }
    car->xfa4 = D_80271D1C[D_8028B940]->limit[car->xe34][k].secs;
    car->xf5c = BE32(D_80025C00.path);
    car->xf60 = 0;
    BrVec3Direction((BrVec3 *)&car->xf64, BRV(&BEPTR(BrPathSeg *, D_80025C00.path)->pt[0].pos), BRV(&BEPTR(BrPathSeg *, D_80025C00.path)->pt[1].pos));
  } else {
    car->xf68 = 0.0f;
    car->xf6c = 0.0f;
    car->xf64 = 1.0f;
  }
  car->xfac = D_8028B7F0 - car->slot - 1;
  car->laps = 0;
  car->xf9c = 0;
  car->xf70 = 0;
  if (car->slot < D_8026FF08 || D_8026FF18 == 1 || D_8026FF18 == 2 || D_8026FF18 == 4) {
    car->xf7c = -1;
    car->xf74 = -1;
  } else {
    car->xf7c = 0;
    car->xf74 = 0;
  }
  car->x2000 = 0;
  car->x2044 = 0;
  car->heading = 0.0f;
  car->xfe4[1] = 0.0f;
  car->xfe4[0] = 0.0f;
  car->xfe4[2] = 0.0f;
  car->xfe4[3] = 0.0f;
  car->xfe4[4] = 0.0f;
  car->xfe4[5] = 0.0f;
  car->xfe4[6] = 0.0f;
  car->xfe4[7] = 0.0f;
  car->xfe4[8] = 0.0f;
  car->xfe4[9] = 0.0f;
  car->xfe4[10] = 0.0f;
  car->xfd4 = 0;
  car->xed4 = 0;
  BrAiLaneSetup(car);
  BrAiInputClear(car);
  car->xf48 = 0;
  car->xf38 = 0;
  car->xf3c = 0;
  car->xf40 = 0;
  car->xf44 = 1.0f;
}

/* WHAT IT DOES: Set up one race entity: its index and colour; the cars
 * in the race get their car record (and the car links back); the others are
 * placed along the path (by index, spaced 520 or 550 depending on the
 * track, the entities after the thirteenth from -7760 in steps of 600) with
 * their segment, point and gates found by BrPathGates, stopped, and given
 * the track's time for the difficulty and the lead car's class.  The mode
 * switch is written one case per line: with each case over two lines IDO
 * schedules the default's load out of the branch delay slot. */
/* @implements 0x802291F8 tgr BrCarEntInit */
void BrCarEntInit(BrCarEnt *e)
{
  int n;
  short k;

  switch (D_8026FF18) {
  default: n = D_8028B7F4; break;
  case 1: n = 2; break;
  case 0: n = D_8026FF08; break;
  }
  e->index = e - D_803239A0;
  e->rgb[0] = D_8028B904[e->index][0];
  e->rgb[1] = D_8028B904[e->index][1];
  e->rgb[2] = D_8028B904[e->index][2];
  e->flags = 0;
  if (e->index < D_8026FF08 + 13) {
    e->x74 = 0;
  } else {
    e->x74 = 1;
  }
  if (e->index < n) {
    e->car = tgr_addr32(&D_8031B760[e->index]);
    TGR_PTR(BrCar *, e->car)->link = tgr_addr32((struct BrCarLink *)e);
  } else {
    e->car = 0;
    if (e->x74 == 1) {
      e->dist = (e->index - D_8026FF08) * 600 - 7760;
    } else if (D_8028B940 == 2 || D_8028B940 == 7) {
      e->dist = e->index * 520;
    } else {
      e->dist = e->index * 550;
    }
    BrPathGates(D_80025C70, e->dist);
    e->pos.x = D_8031B750.x;
    e->pos.y = D_8031B750.y;
    e->pos.z = D_8031B750.z;
    e->seg = tgr_addr32(D_8028B824);
    e->pt = D_8028B828;
    e->gates = e->gates2 = D_8028B82C;
    e->laps = e->laps2 = D_8028B830;
    osSyncPrintf("Initial lap.gate for %d(%d): %d.%d (dist=%f)\n", e->index, e->x74, e->laps, e->gates, e->dist);
    e->prev.x = e->pos.x;
    e->x30 = 0.0f;
    e->x38 = 0.0f;
    e->x34 = 0.0f;
    e->prev.y = e->pos.y;
    e->prev.z = e->pos.z;
    k = D_8028C800 - 1;
    if (k > 2 || k < 0) {
      k = 0;
    }
    e->speed = D_80271D1C[D_8028B940]->limit[D_8031B760[0].xe34][k].secs;
    e->x54 = D_8028B7F0 - e->index - 1;
  }
}

/* WHAT IT DOES: Walk d along a path from the start of a segment (closed
 * segments bypassed through their alternates), counting the lap gates each
 * span crosses (and how many times gate 0); where d runs out, interpolate
 * the point into D_8031B750, count the part-span's gate too, and keep the
 * segment and point.
 * Each arm declares its own gate index k, so the frame keeps no homes
 * (0x48).  The span test compares against gate 0 as its own symbol
 * (D_80025C98), held in s7; the interpolated part-span tests k == 0, so
 * that address is re-materialised there and s7 holds D_8031B750 across the
 * interpolation, as in the ROM. */
/* @implements 0x80228E4C tgr BrPathGates */
void BrPathGates(BrPathSeg *seg, float d)
{
  int i;
  float len;

  D_8028B82C = 0;
  D_8028B830 = 0;
  for (;;) {
    if (seg != 0 && (BES16(seg->flags) & 1)) {
      do {
        seg = BEPTR(BrPathSeg *, seg->alt);
      } while (seg != 0 && (BES16(seg->flags) & 1));
    }
    if (seg == 0) {
      return;
    }
    for (i = 0; i < BES16(seg->count); i++) {
      len = BEF(seg->pt[i].dist) - BEF(seg->pt[i + 1].dist);
      if (len < d) {
        d -= len;
        if (BES32(D_80025C00.nGates) != 0) {
          int k = (D_8028B82C + 1) % BES32(D_80025C00.nGates);
          if (BrSegmentsOverlapXY(BRF(D_80025C00.gate[k].b), BRF(D_80025C00.gate[k].a), BRV(&seg->pt[i].pos), BRV(&seg->pt[i + 1].pos))) {
            D_8028B82C++;
            if (D_80025C98 == &D_80025C00.gate[k]) {
              D_8028B830++;
            }
          }
        }
      } else {
        BrVec3Lerp(&D_8031B750, BRV(&seg->pt[i + 1].pos), BRV(&seg->pt[i].pos), d / len);
        if (BES32(D_80025C00.nGates) != 0) {
          int k = (D_8028B82C + 1) % BES32(D_80025C00.nGates);
          if (BrSegmentsOverlapXY(BRF(D_80025C00.gate[k].b), BRF(D_80025C00.gate[k].a), BRV(&seg->pt[i].pos), &D_8031B750)) {
            D_8028B82C++;
            if (k == 0) {
              D_8028B830++;
            }
          }
        }
        D_8028B824 = seg;
        D_8028B828 = i;
        return;
      }
    }
    seg = BEPTR(BrPathSeg *, seg->next);
  }
}

/* WHAT IT DOES: The AI's corridor lookahead, eight path points deep.  Each
 * call handles one point and recurses to the next: both edges pulled 0.2
 * toward each other, the centre kept, the two midpoints built; the line from
 * the car to the middle of the point must clear every level already visited,
 * else that branch ends.  The deepest clear corridor is remembered with a
 * left/right steering hint and its edges copied out.  Returns non-zero once
 * a corridor has been banked.  Depth 0 clears the state; no segment means
 * the path's start.  Past a segment's last point it recurses into each
 * following segment.  Ported from the PC twin (BrAiScanCorridor).
 * RESIDUE (255, size 1140 vs 1144): register allocation.  The ROM keeps
 * car, depth and mid in their argument home slots and reloads them at each
 * use (seg in s3, ret in s4, frame 0x80); ours gives car and mid s-registers
 * (frame 0x78).  Structure and call order match. */
/* @t4-pass 0x80226D9C 1 2026-09-29 compiles 13 best 253 moved 2  (n64/tools/n64permute.py) */
/* @t4-pass 0x80226D9C 2 2026-09-29 compiles 13 best 253 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x80226D9C */
/* @implements 0x80226D9C tgr BrAiScanCorridor */
unsigned int BrAiScanCorridor(BrCar *car, int depth, int mid, BrPathSeg *seg)
{
  unsigned int ret;
  int next;
  BrVec3 midPt;
  int level;
  BrVec3 *pA;
  BrVec3 *pB;
  BrVec3 *pC;

  ret = 0;
  if (seg == 0) {
    seg = D_80025C70;
    mid = 0;
  }
  if (depth == 0) {
    D_8028B804 = 0;
    D_8028B818 = 0;
    D_8028B81C = 0;
    D_8028B820 = 0;
  } else {
    if (depth > 8 || (BES16(seg->flags) & 1)) {
      return 0;
    }
    BrVec3Lerp(&D_8031B6D0[depth - 1], BRV(&seg->pt[mid].left), BRV(&seg->pt[mid].right), 0.2f);
    pC = &D_8031B610[depth - 1];
    pC->x = BEF(seg->pt[mid].pos.x);
    pC->y = BEF(seg->pt[mid].pos.y);
    pC->z = BEF(seg->pt[mid].pos.z);
    BrVec3Lerp(&D_8031B550[depth - 1], BRV(&seg->pt[mid].right), BRV(&seg->pt[mid].left), 0.2f);
    BrVec3Midpoint(&D_8031B5B0[depth - 1], &D_8031B550[depth - 1], pC);
    BrVec3Midpoint(&D_8031B670[depth - 1], &D_8031B6D0[depth - 1], pC);
    midPt.x = (BEF(seg->pt[mid].right.x) + BEF(seg->pt[mid].left.x)) * 0.5f;
    midPt.y = (BEF(seg->pt[mid].right.y) + BEF(seg->pt[mid].left.y)) * 0.5f;
    pA = &D_8031B550[1];
    pB = &D_8031B6D0[1];
    for (level = 1; level < depth - 1; level++, pA++, pB++) {
      if (BrSegmentsOverlapXY((float *)car->mtx0[3], (float *)&midPt, pA, pB) == 0) {
        D_8028B820 = 1;
        goto tail;
      }
    }
    if (depth > D_8028B804) {
      D_8028B804 = depth;
      D_8028B814 = seg;
      D_8028B810 = mid;
      if (depth > 2) {
        if (BrSegmentsOverlapXY((float *)car->mtx0[3], (float *)&D_8031B610[2], &D_8031B5B0[1], &D_8031B550[1]) != 0) {
          D_8028B808 = 1;
          D_8028B80C = 0;
        } else if (BrSegmentsOverlapXY((float *)car->mtx0[3], (float *)&D_8031B610[2], &D_8031B670[1], &D_8031B6D0[1]) != 0) {
          D_8028B80C = 1;
          D_8028B808 = 0;
        } else {
          D_8028B80C = 0;
          D_8028B808 = 0;
        }
      } else {
        D_8028B80C = 0;
        D_8028B808 = 0;
      }
      D_8031B740.x = D_8031B610[depth >> 1].x;
      D_8031B740.y = D_8031B610[depth >> 1].y;
      D_8031B740.z = D_8031B610[depth >> 1].z;
      D_8031B730.x = pC->x;
      D_8031B730.y = pC->y;
      D_8031B730.z = pC->z;
      memcpy(D_8031B430, D_8031B550, depth * sizeof(BrVec3));
      memcpy(D_8031B490, D_8031B610, depth * sizeof(BrVec3));
      memcpy(D_8031B4F0, D_8031B6D0, depth * sizeof(BrVec3));
      ret = 1;
      D_8028B800 = depth;
    }
  }
tail:
  if (++mid == BES16(seg->count)) {
    seg = BEPTR(BrPathSeg *, seg->next);
    if (seg == 0) {
      seg = D_80025C70;
    }
    if (seg != 0) {
      next = depth + 1;
      do {
        ret |= BrAiScanCorridor(car, next, 0, seg);
        seg = BEPTR(BrPathSeg *, seg->alt);
      } while (seg != 0);
    }
  } else {
    ret |= BrAiScanCorridor(car, depth + 1, mid, seg);
  }
  return ret;
}

/* WHAT IT DOES: Find where the car sits against the racing line: the line
 * between the two path points either side of it is a Hermite curve through
 * their midpoints, with tangents square to the neighbouring spans and scaled
 * by the span length; the parameter comes from the car's projections on the
 * two tangents.  Leaves the curve point, its direction, a side and up axis,
 * and the car's signed and absolute distance across the line. */
/* @implements 0x80227214 tgr BrCarLineFit */
void BrCarLineFit(BrCar *car)
{
  float dist;
  float len;
  float half;
  BrVec3 a;
  BrVec3 b;
  BrPathSeg *seg1;
  BrPathSeg *seg2;
  int i1;
  int i2;
  BrVec3 m1;
  BrVec3 m2;
  BrVec3 p;
  BrVec3 d1;
  BrVec3 d2;
  float e1;
  float e2;
  float t;
  float t2;
  float t3;

  len = BEF((TGR_PTR(BrPathSeg *, car->xf5c))->pt[car->xf60].dist) - BEF((TGR_PTR(BrPathSeg *, car->xf5c))->pt[car->xf60 + 1].dist);
  BrVec3Sub(&a, BRV(&(TGR_PTR(BrPathSeg *, car->xf5c))->pt[car->xf60].left), BRV(&(TGR_PTR(BrPathSeg *, car->xf5c))->pt[car->xf60].pos));
  BrVec3Sub(&b, BRV(&(TGR_PTR(BrPathSeg *, car->xf5c))->pt[car->xf60 + 1].left), BRV(&(TGR_PTR(BrPathSeg *, car->xf5c))->pt[car->xf60 + 1].pos));
  BrVec3Midpoint(&b, &a, &b);
  a.x = -b.y;
  a.y = b.x;
  a.z = 0.0f;
  BrVec3Midpoint(&b, BRV(&(TGR_PTR(BrPathSeg *, car->xf5c))->pt[car->xf60].pos), BRV(&(TGR_PTR(BrPathSeg *, car->xf5c))->pt[car->xf60 + 1].pos));
  BrVec3Sub(&b, (BrVec3 *)car->mtx0[3], &b);
  if (BrVec3Dot(&a, &b) < 0.0f) {
    seg1 = TGR_PTR(BrPathSeg *, car->xf5c);
    i1 = car->xf60;
    seg2 = seg1;
    if (i1 + 1 == BES16(seg1->count)) {
      seg2 = BEPTR(struct BrPathSeg *, seg1->next);
      if (BES16(seg2->flags) & 1) {
        do {
          seg2 = BEPTR(struct BrPathSeg *, seg2->alt);
        } while (BES16(seg2->flags) & 1);
      }
      i2 = 0;
    } else {
      i2 = i1 + 1;
    }
  } else {
    seg1 = TGR_PTR(BrPathSeg *, car->xf5c);
    i1 = car->xf60 - 1;
    seg2 = TGR_PTR(BrPathSeg *, car->xf5c);
    i2 = car->xf60;
  }
  BrVec3Direction(&a, BRV(&seg1->pt[i1].pos), BRV(&seg1->pt[i1 + 1].pos));
  BrVec3Direction(&b, BRV(&seg2->pt[i2].pos), BRV(&seg2->pt[i2 + 1].pos));
  BrVec3ScaleBy(&a, len * 0.5f);
  BrVec3ScaleBy(&b, len * 0.5f);
  BrVec3Midpoint(&m1, BRV(&seg1->pt[i1].pos), BRV(&seg1->pt[i1 + 1].pos));
  BrVec3Midpoint(&m2, BRV(&seg2->pt[i2].pos), BRV(&seg2->pt[i2 + 1].pos));
  p.x = car->mtx0[3][0];
  p.y = car->mtx0[3][1];
  p.z = 0.0f;
  BrVec3Sub(&d1, &p, &m1);
  BrVec3Sub(&d2, &p, &m2);
  e1 = BrVec3Dot(&d1, &a);
  e2 = -BrVec3Dot(&d2, &b);
  dist = BrVec3Dist(&m1, &m2);
  t = dist * e1 / (e1 + e2) / dist;
  t2 = t * t;
  t3 = t2 * t;
  BrVec3Scale(&car->linePos, &m1, t3 + t3 - 3.0f * t2 + 1.0f);
  BrVec3MulAddTo(&car->linePos, &m2, -2.0f * t3 + 3.0f * t2);
  BrVec3MulAddTo(&car->linePos, &a, t3 - (t2 + t2) + t);
  BrVec3MulAddTo(&car->linePos, &b, t3 - t2);
  BrVec3Lerp(&car->lineDir, &b, &a, t);
  BrVec3Normalise(&car->lineDir);
  BrVec3Cross(&car->lineSide, &car->lineUp, &car->lineDir);
  BrVec3Normalise(&car->lineSide);
  BrVec3Cross(&car->lineUp, &car->lineDir, &car->lineSide);
  BrVec3Sub(&car->lineRel, (BrVec3 *)car->mtx0[3], &car->linePos);
  car->lineOff = BrVec3Dot(&car->lineSide, &car->lineRel);
  if (car->lineOff < 0.0f) {
    car->lineOffAbs = -car->lineOff;
  } else {
    car->lineOffAbs = car->lineOff;
  }
}

/* WHAT IT DOES: Drive one computer-controlled car for this frame. Walk the
 * car's path cursor ahead by a speed-scaled look-ahead, ease the aim point
 * toward the corridor scan's suggestion and toward that waypoint, and build
 * the path frame there; turn the car's offset from the line and its heading
 * into a steering request (a side bias, a magnitude and a response curve);
 * a throttle ladder over the scan's corridor edges decides whether to lift
 * or brake; counters flip the car into reverse when it is stuck for 30
 * frames and back; the nearest rival ahead pushes the line offset aside;
 * and the body's velocity is reshaped along the path frame and scaled by
 * the pace table or the weather. Its spin is clamped to unit length, then
 * the car is ticked and respawned if need be. PC twin: BrCtlAiBody.
 * Source facts: uopt merges float constants by their spelling, so which
 * constants share a register follows how each is written -- the walk tests
 * t < 0, the steering bias is .2f against the response curve's 0.2f, and
 * two of the magnitudes (0.1, 0.5) are double literals.  The walk resets i after the
 * closed-segment skip (uopt then steps a pointer); the second sign ladder
 * reuses i; the frame is the declaration order. */
/* @implements 0x8022762C tgr BrCtlAiBody */
void BrCtlAiBody(BrCar *car)
{
  BrVec3 aimDir;
  float lat;
  int i;
  float velFwd;
  float t;
  BrPathSeg *seg;
  float offset;
  BrVec3 target;
  float mag;
  float limit;
  int level;
  float tq;
  float speed;
  int sFwd;
  float f;
  BrVec3 dead;
  BrVec3 upCrossAim;
  int best_i;
  BrVec3 up;
  float best;
  float add;
  float lap;
  float heading;
  float absOffset;
  BrVec3 edge;
  float halfWidth;
  float d;
  float diff;
  float k;
  int sVel;
  float len;
  int unusedE8[3];
  float scale;
  BrVec3 vB;
  BrVec3 vA;
  BrVec3 vN;
  float q;
  BrCar *other;
  int unused98[8];
  float rel[3];
  short w;

  if (!(TGR_PTR(struct BrCarLink *, car->link)->flags & 1)) {
    if (*TGR_PTR(unsigned int *, car->pad) & 0x2000000) {
      car->xf48 = 1;
      BrCarCamPlaceChase(car);
    }
    BrPerfMark(0, 0xFF, 0xFF, 0xFF, 0xFF);
    seg = TGR_PTR(BrPathSeg *, car->xf5c);
    i = car->xf60;
    t = BrVec3Length(&car->velfd8) * 3.0f + 20.0f;
    {
      BrVec3 *p = BRV(&(TGR_PTR(BrPathSeg *, car->xf5c))->pt[car->xf60].pos);   /* cartridge data */

      target.x = p->x;
      target.y = p->y;
      target.z = p->z;
    }
    if (t > 80.0f) {
      t = 80.0f;
    }
    for (;;) {
      t -= BEF(seg->pt[i].dist) - BEF(seg->pt[i + 1].dist);
      i++;
      if (i == BES16(seg->count)) {
        seg = BEPTR(BrPathSeg *, seg->next);
        while (BES16(seg->flags) & 1) {
          seg = BEPTR(BrPathSeg *, seg->alt);
        }
        i = 0;
      }
      if (t < 0) {
        break;
      }
    }
    target.x = BEF(seg->pt[i].pos.x);
    target.y = BEF(seg->pt[i].pos.y);
    target.z = BEF(seg->pt[i].pos.z);
    if (BrAiScanCorridor(car, 0, car->xf60, TGR_PTR(BrPathSeg *, car->xf5c)) != 0) {
      BrVec3Midpoint(&D_8031B730, &D_8031B730, &D_8031B740);
      if (car->aim.x != 0.0f || car->aim.y != 0.0f || car->aim.z != 0.0f) {
        BrVec3Lerp(&car->aim, &car->aim, &D_8031B730, 0.5f + D_8028AAD8);
      } else {
        car->aim.x = D_8031B730.x;
        car->aim.y = D_8031B730.y;
        car->aim.z = D_8031B730.z;
      }
    }
    BrVec3Lerp(&car->aim, &car->aim, &target, 0.4f);
    velFwd = BrVec3Dot(&car->velfd8, (BrVec3 *)car->mtx0[0]);
    BrVec3Direction(&aimDir, (BrVec3 *)car->mtx0[3], &car->aim);
    up = D_8028BAB0;
    BrVec3Cross(&upCrossAim, &up, &aimDir);
    BrVec3Cross(&dead, &aimDir, &upCrossAim);
    lat = BrVec3Dot((BrVec3 *)car->mtx0[1], &aimDir);
    car->x1ddc = BrVec3Dot((BrVec3 *)car->mtx0[0], &aimDir);
    *TGR_PTR(unsigned int *, car->pad) |= 0x10000;

    BrVec3Direction(&car->lineDir, BRV(&(TGR_PTR(BrPathSeg *, car->xf5c))->pt[car->xf60].pos),
                    BRV(&(TGR_PTR(BrPathSeg *, car->xf5c))->pt[car->xf60 + 1].pos));
    BrVec3Sub(&car->lineSide, BRV(&(TGR_PTR(BrPathSeg *, car->xf5c))->pt[car->xf60].left),
              BRV(&(TGR_PTR(BrPathSeg *, car->xf5c))->pt[car->xf60].right));
    BrVec3Cross(&car->lineUp, &car->lineDir, &car->lineSide);
    BrVec3Normalise(&car->lineUp);
    BrVec3Cross(&car->lineSide, &car->lineUp, &car->lineDir);
    BrVec3Normalise(&car->lineSide);
    BrVec3Sub(&car->lineRel, (BrVec3 *)car->mtx0[3], BRV(&(TGR_PTR(BrPathSeg *, car->xf5c))->pt[car->xf60].pos));
    BrVec3MulAddTo(&car->lineRel, &car->velfd8, 0.4f);
    offset = BrVec3Dot(&car->lineSide, &car->lineRel);
    mag = 0.0f;
    heading = BrVec3Dot((BrVec3 *)car->mtx0[0], &car->lineSide);
    D_8028B80C = 0;
    D_8028B808 = 0;
    if (offset < 0.0f) {
      absOffset = -offset;
    } else {
      absOffset = offset;
    }
    BrVec3Sub(&edge, BRV(&(TGR_PTR(BrPathSeg *, car->xf5c))->pt[car->xf60].left), BRV(&(TGR_PTR(BrPathSeg *, car->xf5c))->pt[car->xf60].pos));
    halfWidth = BrVec3Dot(&car->lineSide, &edge);
    if (halfWidth > 5.0f) {
      limit = halfWidth - 3.0f;
    } else {
      limit = halfWidth * 0.4f;
    }
    if (absOffset > limit) {
      if (offset > 0.0f) {
        if (heading > -0.05) {
          D_8028B80C = 0;
          D_8028B808 = 1;
          mag = heading * -0.2f * ((absOffset - limit) / absOffset) + 0.03f;
        } else if (heading < -0.15) {
          D_8028B80C = 1;
          D_8028B808 = 0;
          mag = heading * -0.3f * ((absOffset - limit) / absOffset) + 0.1f;
        }
      } else if (heading < 0.05) {
        D_8028B80C = 1;
        D_8028B808 = 0;
        mag = heading * .2f * ((absOffset - limit) / absOffset) + 0.03f;
      } else if (heading > 0.15) {
        D_8028B80C = 0;
        D_8028B808 = 1;
        mag = heading * 0.3f * ((absOffset - limit) / absOffset) + 0.1f;
      }
    } else if (BrVec3Length(&car->velfd8) > 10.0f) {
      f = BrVec3Dot(&car->velfd8, &car->lineSide);
      if (f > 0.1f) {
        sVel = 1;
      } else if (f < -0.1f) {
        sVel = -1;
      } else {
        sVel = 0;
      }
      f = BrVec3Dot((BrVec3 *)car->wheelMtx[0][0], &upCrossAim);
      if (f > 0.1f) {
        i = 1;
      } else if (f < -0.1f) {
        i = -1;
      } else {
        i = 0;
      }
      f = BrVec3Dot((BrVec3 *)car->mtx0[0], &upCrossAim);
      if (f > 0.1f) {
        sFwd = 1;
      } else if (f < -0.1f) {
        sFwd = -1;
      } else {
        sFwd = 0;
      }
      if (sVel != 0 || sFwd != 0) {
        if (sVel != 0 && sVel + sFwd == 0) {
          if (sVel == 1 && i == 1) {
            D_8028B808 = 0;
            D_8028B80C = 1;
          } else if (sVel == -1 && i == -1) {
            D_8028B808 = 1;
            D_8028B80C = 0;
          }
          mag = 0.1;
        } else if (sVel != 0 && sVel == sFwd) {
          if (sVel == 1 && i == 1) {
            D_8028B808 = 1;
            D_8028B80C = 0;
          } else if (sVel == -1 && i == -1) {
            D_8028B808 = 0;
            D_8028B80C = 1;
          }
          mag = 0.4f;
        } else if (i != 0 || sFwd != 0) {
          if (i == 0 && sVel == 0) {
            *TGR_PTR(unsigned int *, car->pad) &= ~0x10000;
          } else if (i == sFwd) {
            if (i == 1) {
              D_8028B808 = 1;
              D_8028B80C = 0;
            } else if (i == -1) {
              D_8028B808 = 0;
              D_8028B80C = 1;
            }
            mag = 0.5;
            *TGR_PTR(unsigned int *, car->pad) &= ~0x10000;
          } else if (i == 0 || i + sVel != 0) {
            *TGR_PTR(unsigned int *, car->pad) &= ~0x10000;
          }
        }
      }
    }
    scale = 1.0f;
    for (level = 0; level < D_8028B804; level++) {
      BrVec3Sub(&vA, &D_8031B430[level], &D_8031B490[level]);
      BrVec3Sub(&vB, &D_8031B490[level + 1], &D_8031B490[level]);
      BrVec3Cross(&vN, &vB, &vA);
      BrVec3Normalise(&vN);
      q = BrVec3Dot(&car->velfd8, &vN);
      tq = BrVec3Dot(&car->velfd8, &vA) * 0.03f;
      if (q < tq) {
        q = tq;
      } else if (tq < -q) {
        q = -tq;
      }
      if (q > (level + 1) * 6.0f) {
        scale = 2.0f;
        *TGR_PTR(unsigned int *, car->pad) &= ~0x10000;
        *TGR_PTR(unsigned int *, car->pad) |= 0x40000;
        break;
      }
      if (q > (level + 1) * 4.5f) {
        scale = 2.0f;
        *TGR_PTR(unsigned int *, car->pad) &= ~0x10000;
        break;
      }
      if (q > (level + 1) * 3.0f) {
        scale = 1.3f;
        break;
      }
    }
    if (BrVec3Length(&car->velfd8) > 10.0f) {
      lat = lat * scale;
      if (lat > 1.0f) {
        *TGR_PTR(unsigned int *, car->pad) &= ~0x10000;
        lat = 1.0f;
      } else if (lat < -1.0f) {
        lat = -1.0f;
        *TGR_PTR(unsigned int *, car->pad) &= ~0x10000;
      }
    }
    if (mag < 0.01f) {
      mag = 0.01f;
    }
    if (lat < 0.0f) {
      if (D_8028B80C != 0) {
        if (lat < -(0.2f / mag)) {
          lat -= mag * lat;
        } else {
          lat = lat + 0.2f;
        }
      } else if (D_8028B808 != 0) {
        if (lat < -(0.2f / mag)) {
          lat += mag * lat;
        } else {
          lat = lat - 0.2f;
        }
      }
      lat = 1.0f + lat;
      lat = lat * lat * lat * lat - 1.0f;
    } else {
      if (D_8028B80C != 0) {
        if (0.2f / mag < lat) {
          lat += mag * lat;
        } else {
          lat = lat + 0.2f;
        }
      } else if (D_8028B808 != 0) {
        if (0.2f / mag < lat) {
          lat = lat - mag * lat;
        } else {
          lat = lat - 0.2f;
        }
      }
      lat = 1.0f - lat;
      lat = 1.0f - lat * lat * lat * lat;
    }
    speed = BrVec3Length(&car->velfd8);
    if (car->x1ddc >= 0.0f) {
      if (car->xe70[1] != 0) {
        car->xe70[1]--;
        goto reverse;
      }
      if (velFwd < -1.0f) {
        *TGR_PTR(unsigned int *, car->pad) |= 0x40000;
      } else {
      forward:
        ((float *)TGR_PTR(unsigned int *, car->pad))[8] = -lat;
        if (++car->xe70[3] > 30 && speed < 1.0f) {
          car->xe70[1] = 60;
          car->xe70[2] = 0;
          car->xe70[3] = 0;
        }
      }
    } else {
      if (car->xe70[0] != 0) {
        car->xe70[0]--;
        goto forward;
      }
      if (velFwd > 1.0f) {
        *TGR_PTR(unsigned int *, car->pad) |= 0x40000;
      } else {
      reverse:
        *TGR_PTR(unsigned int *, car->pad) |= 0x10000;
        if (lat < 0.0f) {
          ((float *)TGR_PTR(unsigned int *, car->pad))[8] = -1.0f;
        } else {
          ((float *)TGR_PTR(unsigned int *, car->pad))[8] = 1.0f;
        }
        *TGR_PTR(unsigned int *, car->pad) |= 0x20000;
        if (car->xe70[2] > 150) {
          if (car->xe70[2] > 270) {
            car->xe70[2] = 30;
          } else {
            ((float *)TGR_PTR(unsigned int *, car->pad))[8] *= -1.0;
          }
        }
        if (++car->xe70[2] > 30 && speed < 1.0f) {
          car->xe70[0] = 60;
          car->xe70[2] = 0;
          car->xe70[3] = 0;
        }
      }
    }
    BrCarLineFit(car);
    best_i = -1;
    best = 90.0f;
    add = 0.0f;
    for (level = 0; level < D_8028B7F0; level++) {
      other = TGR_PTR(BrCar *, D_803239A0[level].car);
      if (other != 0 && other != car) {
        lap = BEF(BEPTR(BrPathSeg *, D_80025C00.path)->pt[0].dist);
        d = other->xfa8 - car->xfa8;
        while (d > lap) {
          d -= lap;
        }
        while (d < -lap) {
          d += lap;
        }
        if (d > 0.0f && d < best) {
          diff = other->lineOff - offset;
          best = d;
          if (diff < 3.0f) {
            add = (1.0f - d * 0.011111111f) * -10.0f;
            if (add < -5.0f) {
              add = -5.0f;
            }
            best_i = level;
          } else if (diff > -3.0f) {
            add = (1.0f - d * 0.011111111f) * 10.0f;
            if (add > 5.0f) {
              add = 5.0f;
            }
            best_i = level;
          }
        }
      }
    }
    if (best_i != -1) {
      offset = offset + add;
      if (offset < 0.0f) {
        car->lineOffAbs = -offset;
      } else {
        car->lineOffAbs = offset;
      }
    }
    if (velFwd > 3.0f) {
      if (car->lineOffAbs < halfWidth + 1.0f) {
        k = (velFwd - 3.0f) * 0.015625f;
        if (k < 0.0f) {
          k = k * -0.25f;
        }
        if (k > 0.4f) {
          k = 0.4f;
        }
        rel[0] = BrVec3Dot(&car->st.vel, &car->lineDir);
        rel[1] = BrVec3Dot(&car->st.vel, &car->lineSide);
        rel[2] = BrVec3Dot(&car->st.vel, &car->lineUp);
        rel[1] = -(offset * k);
        BrVec3Scale(&car->st.vel, &car->lineDir, rel[0]);
        BrVec3MulAddTo(&car->st.vel, &car->lineSide, rel[1]);
        BrVec3MulAddTo(&car->st.vel, &car->lineUp, rel[2]);
      }
      if (D_8026FF18 == 0) {
        BrVec3ScaleBy(&car->st.vel, D_8028B9F0[D_8031C5BC->b4][D_8031C5BC->b5][TGR_PTR(struct BrCarLink *, car->link)->x74]);
      } else if (D_8026FF18 == 1) {
        w = D_8028C800 - 1;
        if (w > 2 || w < 0) {
          w = 0;
        }
        if (w == 2) {
          BrVec3ScaleBy(&car->st.vel, 0.99f);
        } else {
          BrVec3ScaleBy(&car->st.vel, 0.999f);
        }
      }
    }
  }
  BrPerfMark(0, 0, 0x82, 0, 0xFF);
  len = BrVec3Length(&car->st.angVel);
  if (len > 1.0f && car->wheels[0].x13c != 0) {
    BrVec3ScaleBy(&car->st.angVel, 1.0f / len);
  }
  BrCarPhysTick(car);
  BrCarRespawn(car);
}

/* WHAT IT DOES: Drive one computer-controlled car for this frame: the
 * out-of-line entry to the AI driver's main body. */
/* @implements 0x802288B4 tgr BrCtlAi */
void BrCtlAi(BrCar *car)
{
    BrCtlAiBody(car);
}

/* WHAT IT DOES: Does nothing with its argument. An empty function the
 * retail build kept among the AI driver code. */
/* @implements 0x80228E44 tgr BrStub80228E44 */
void BrStub80228E44(int arg0)
{
}

/* WHAT IT DOES: Set up a computer driver's racing lanes: four lane offsets
 * stepping 0.034 from slot * 0.137 (then cleared again with their
 * partners), the lane kinds to 2, the targets to i * 0.15, the 144 path
 * nodes emptied and 36 flags set to 2.
 * Each lane is the previous one plus 0.034, written out, so each value is
 * its own temporary (f0, f2, f12, f10) as in the ROM; the other small
 * arrays share one counter loop, and the node loop steps the row pointer
 * before the counter. */
/* @implements 0x802288D4 tgr BrAiLaneSetup */
void BrAiLaneSetup(BrAiCar *a)
{
  int i;
  int k;
  BrAiNode *n;
  short (*p)[3];

  a->lane[0] = a->slot * 0.137f;
  a->lane[1] = a->lane[0] + 0.034f;
  a->lane[2] = a->lane[1] + 0.034f;
  a->lane[3] = a->lane[2] + 0.034f;
  for (k = 0; k < 4; k++) {
    a->x1070[k] = 2;
    a->x1090[k] = 0;
    a->lane[k] = 0.0f;
    a->x1020[k] = 0.15f * k;
    a->x1080[k] = 0.0f;
  }
  for (p = a->x19d0, n = a->node, i = 0; i < 0x90; p++, i++, n++) {
    (*p)[0] = (*p)[1] = (*p)[2] = 0;
    n->x0[0] = 0;
    n->x0[1] = 0;
    n->x0[2] = 0;
    n->xc = 0;
    if (i & 1) {
      n->xd = 0;
    } else {
      n->xd = 0;
    }
    n->xe = 0;
    n->xf = 0xff;
  }
  for (i = 0; i < 36; i++) {
    a->x1d30[i] = 2;
  }
}

/* WHAT IT DOES: Clear a computer driver's steering and pedal outputs. */
/* @implements 0x80228A3C tgr BrAiInputClear */
void BrAiInputClear(short *car)
{
  int i;

  for (i = 0; i < 8; i++) {
    car[0x103e + i] = 0;
  }
  car[0x1046] = 0;
}


/* WHAT IT DOES: Walk a distance d along the track path from point i of a
 * segment, part-way (frac) through its first span: closed segments are
 * bypassed through their alternates, and where the distance runs out the
 * position is interpolated into D_8031B750, with the segment and point
 * kept.
 * RESIDUE (77): the same instructions, but our IDO hoists the 1.0 for frac
 * to the entry (lui/mtc1 once, mov.s in the loop) where the ROM builds it
 * inside the loop; everything after shifts by one slot.  Literal
 * spellings and goto/while loop shapes leave it. */
/* @t4-pass 0x802290C4 1 2026-10-03 compiles 119 best 77 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x802290C4 2 2026-10-03 compiles 119 best 77 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x802290C4 */
/* @implements 0x802290C4 tgr BrPathWalk */
void BrPathWalk(BrPathSeg *seg, int i, float frac, float d)
{
  float len;
  BrPathPt *p;                    /* unused: with spare, the ROM's frame */
  float spare[2];

  for (;;) {
    if (seg != 0 && (BES16(seg->flags) & 1)) {
      do {
        seg = BEPTR(BrPathSeg *, seg->alt);
      } while (seg != 0 && (BES16(seg->flags) & 1));
    }
    if (seg == 0) {
      return;
    }
    for (; i < BES16(seg->count); i++) {
      len = (BEF(seg->pt[i].dist) - BEF(seg->pt[i + 1].dist)) * frac;
      if (len < d) {
        frac = 1.0f;
        d -= len;
      } else {
        BrVec3Lerp(&D_8031B750, BRV(&seg->pt[i].pos), BRV(&seg->pt[i + 1].pos), frac);
        BrVec3Lerp(&D_8031B750, BRV(&seg->pt[i + 1].pos), &D_8031B750, d / len);
        D_8028B824 = seg;
        D_8028B828 = i;
        return;
      }
    }
    seg = BEPTR(BrPathSeg *, seg->next);
    i = 0;
  }
}
