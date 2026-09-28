/* ctlai.c -- the computer drivers
 */
#include "tgr/common.h"
#include "tgr/vec.h"
#include "tgr/car.h"

/* -- declarations -- */
void func_8022762C(int ctl);
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
typedef struct BrPathPt {       /* a point on a path segment (0x28 bytes) */
  BrVec3 pos;                   /* 0x00 */
  char pad0c[0x18 - 0xc];
  float dist;                   /* 0x18  distance along the track */
  char pad1c[0x28 - 0x1c];
} BrPathPt;
typedef struct BrPathSeg {      /* a path segment */
  struct BrPathSeg *next;       /* 0x00 */
  struct BrPathSeg *alt;        /* 0x04  taken when this one is closed */
  char pad08[0x14 - 0x8];
  unsigned short count;         /* 0x14  points */
  unsigned short flags;         /* 0x16  bit 0: closed */
  char pad18[0x4c - 0x18];
  BrPathPt pt[1];               /* 0x4C */
} BrPathSeg;
extern BrVec3 D_8031B750;               /* the point found by BrPathWalk */
extern BrPathSeg *D_8028B824;           /* and its segment */
extern int D_8028B828;                  /* and its point */
void BrVec3Lerp(BrVec3 *pOut, BrVec3 *pA, BrVec3 *pB, float t);
typedef struct BrGate {         /* a lap gate across the track (0x14 bytes) */
  float a[2];                   /* 0x00  its ends, x and y */
  float b[2];                   /* 0x08 */
  int x10;
} BrGate;
typedef struct BrTrackGates {   /* the gates in the loaded track's header */
  char pad00[0x98];
  BrGate gate[10];              /* 0x98 */
  int nGates;                   /* 0x160 */
} BrTrackGates;
extern BrTrackGates D_80025C00;
extern BrGate D_80025C98[];             /* the same gates, as their own symbol */
extern int D_8028B82C;                  /* gates crossed by BrPathGates */
extern int D_8028B830;                  /* times it crossed gate 0 */
int BrSegmentsOverlapXY(float *a, float *b, BrVec3 *c, BrVec3 *d);
typedef struct BrCarEnt {       /* a race entity (0x78 bytes): the cars, then the rest */
  BrVec3 pos;                   /* 0x00 */
  BrVec3 prev;                  /* 0x0C */
  char pad18[0x28 - 0x18];
  BrPathSeg *seg;               /* 0x28  where it is on the path */
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
  BrCar *car;                   /* 0x60  0 for an entity that is not a car */
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
extern BrPathSeg *D_80025C70;           /* the track's path */
void osSyncPrintf(const char *fmt, ...);
void BrPathGates(BrPathSeg *seg, float d);
/* -- end declarations -- */

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
    e->car = &D_8031B760[e->index];
    e->car->link = (struct BrCarLink *)e;
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
    e->seg = D_8028B824;
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
 * RESIDUE (74, 156/158 instructions): register allocation in the final
 * span.  The ROM keeps gate 0's address in s7 and 0x14 in fp through the
 * loop, then re-materialises gate 0 and holds the point and D_8031B750 in
 * s1/s7 across the interpolation; ours swaps s7/fp and spills the point.
 * Landed: gate 0 as its own symbol (D_80025C98), the gate by index k. */
/* @implements 0x80228E4C tgr BrPathGates */
void BrPathGates(BrPathSeg *seg, float d)
{
  int i;
  float len;
  int k;

  D_8028B82C = 0;
  D_8028B830 = 0;
  for (;;) {
    if (seg != 0 && (seg->flags & 1)) {
      do {
        seg = seg->alt;
      } while (seg != 0 && (seg->flags & 1));
    }
    if (seg == 0) {
      return;
    }
    for (i = 0; i < seg->count; i++) {
      len = seg->pt[i].dist - seg->pt[i + 1].dist;
      if (len < d) {
        d -= len;
        if (D_80025C00.nGates != 0) {
          k = (D_8028B82C + 1) % D_80025C00.nGates;
          if (BrSegmentsOverlapXY(D_80025C00.gate[k].b, D_80025C00.gate[k].a, &seg->pt[i].pos, &seg->pt[i + 1].pos)) {
            D_8028B82C++;
            if (&D_80025C00.gate[k] == D_80025C98) {
              D_8028B830++;
            }
          }
        }
      } else {
        BrVec3Lerp(&D_8031B750, &seg->pt[i + 1].pos, &seg->pt[i].pos, d / len);
        if (D_80025C00.nGates != 0) {
          k = (D_8028B82C + 1) % D_80025C00.nGates;
          if (BrSegmentsOverlapXY(D_80025C00.gate[k].b, D_80025C00.gate[k].a, &seg->pt[i].pos, &D_8031B750)) {
            D_8028B82C++;
            if (&D_80025C00.gate[k] == D_80025C98) {
              D_8028B830++;
            }
          }
        }
        D_8028B824 = seg;
        D_8028B828 = i;
        return;
      }
    }
    seg = seg->next;
  }
}

/* WHAT IT DOES: Drive one computer-controlled car for this frame: the
 * out-of-line entry to the AI driver's main body. */
/* @implements 0x802288B4 tgr BrCtlAi */
void BrCtlAi(int ctl)
{
    func_8022762C(ctl);
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
 * RESIDUE (83): the ROM loads 0.034 before 0.137 (so its literal pool
 * order differs too) and finishes the lane chain before the other small
 * loops; ours interleaves them.  Each small loop needs its own counter or
 * IDO leaves it rolled. */
/* @t4-pass 0x802288D4 1 2026-09-26 compiles 17 best 144 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x802288D4 2 2026-09-26 compiles 17 best 144 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x802288D4 3 2026-09-26 compiles 16 best 144 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x802288D4 tgr BrAiLaneSetup */
void BrAiLaneSetup(BrAiCar *a)
{
  int i;
  int j;
  int k;
  int m;
  int q;
  float x;
  BrAiNode *n;
  short (*p)[3];

  x = a->slot * 0.137f;
  for (j = 0; j < 4; j++) {
    a->lane[j] = x;
    x += 0.034f;
  }
  for (k = 0; k < 4; k++) {
    a->x1090[k] = 0;
    a->x1070[k] = 2;
  }
  for (m = 0; m < 4; m++) {
    a->x1020[m] = 0.15f * m;
  }
  for (q = 0; q < 4; q++) {
    a->lane[q] = 0.0f;
    a->x1080[q] = 0.0f;
  }
  for (i = 0, p = a->x19d0, n = a->node; i < 0x90; i++, p++, n++) {
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
/* @implements 0x802290C4 tgr BrPathWalk */
void BrPathWalk(BrPathSeg *seg, int i, float frac, float d)
{
  float len;
  BrPathPt *p;                    /* unused: with spare, the ROM's frame */
  float spare[2];

  for (;;) {
    if (seg != 0 && (seg->flags & 1)) {
      do {
        seg = seg->alt;
      } while (seg != 0 && (seg->flags & 1));
    }
    if (seg == 0) {
      return;
    }
    for (; i < seg->count; i++) {
      len = (seg->pt[i].dist - seg->pt[i + 1].dist) * frac;
      if (len < d) {
        frac = 1.0f;
        d -= len;
      } else {
        BrVec3Lerp(&D_8031B750, &seg->pt[i].pos, &seg->pt[i + 1].pos, frac);
        BrVec3Lerp(&D_8031B750, &seg->pt[i + 1].pos, &D_8031B750, d / len);
        D_8028B824 = seg;
        D_8028B828 = i;
        return;
      }
    }
    seg = seg->next;
    i = 0;
  }
}
