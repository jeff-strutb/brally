/* rank.c -- ordering the cars into race positions
 */
#include "tgr/common.h"
#include "tgr/car.h"

/* -- declarations -- */
typedef struct BrRaceEnt {      /* a race entity (0x78 bytes): the cars, then the rest */
  char pad00[0x50];
  float progress;               /* 0x50  for an entity that is not a car */
  int rank;                     /* 0x54  its race position, likewise */
  char pad58[8];
  BrCar *car;                   /* 0x60  0 for an entity that is not a car */
  int x64;
  unsigned int flags;           /* 0x68  bit 1: finished */
  char pad6c[0x78 - 0x6C];
} BrRaceEnt;
typedef struct BrRankEntry {    /* one row of the ranking table */
  float key;
  int ent;
} BrRankEntry;
void BrQsort(void *base, unsigned int num, unsigned int width, int (*comp)(void *, void *));
extern int D_8028B7F0;                  /* entity count */
extern BrRaceEnt D_803239A0[];
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
