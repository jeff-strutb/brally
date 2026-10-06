/* br_rank.c -- racing.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 *
 * BrRankAssign 0x1005F580 -- the Gate B ledger from its T3 days.
 * @t4-pass 0x1005F580 1 2026-09-09 probes 6 bytes 259 insns 92 regions 1 rows 0 census no
 * @t4-pass 0x1005F580 2 2026-09-10 probes 12 bytes 259 insns 92 regions 1 rows 0 census no
 * @t4-pass 0x1005F580 3 2026-09-10 probes 11 bytes 259 insns 92 regions 1 rows 0 census yes
 */
/* The original is /MD: CRT calls go through the import
 * table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include <stdint.h>
#include <stdlib.h>



/* 0x10066620 */
/* WHAT IT DOES: the "who is ahead" test used when the game sorts the field
 * into race order -- it compares two drivers' progress figures and says which
 * comes first. If either figure is not a real number the answer it gives is
 * "less than" rather than "equal", which is the original's behaviour and not
 * a tidy-up opportunity. */
/* @implements 0x10066620 d3d BrRankCmpKey */
/* @implements 0x1005F690 glide BrRankCmpKey */
int BrRankCmpKey(const void *pA, const void *pB)
{
    /* Orig is `fld [ecx]; fcomp [edx]` twice -- no float locals. */
    if (*(const float *)pA > *(const float *)pB)
        return 1;
    if (!(*(const float *)pA >= *(const float *)pB))
        return -1;
    return 0;
}

extern volatile int DAT_10226a48;
extern int DAT_100b2f00;
extern int DAT_100b2f04;
/* 0x10AF0848 -- the driver slots, 0x80 bytes each. */
typedef struct BrRankSlot {
    int           key;            /* +0x00 progress key when no car */
    int           rank;           /* +0x04 */
    int           f08[2];
    int           pCar;           /* +0x10 car record, or 0 */
    int           f14;
    unsigned char flags;          /* +0x18 bit 2: not ranked */
    unsigned char pad[0x80 - 0x19];
} BrRankSlot;
typedef char br_rank_assert_slot[(sizeof(BrRankSlot) == 0x80) ? 1 : -1];
extern BrRankSlot DAT_10af0848[];

/* One sort record: the key BrRankCmpKey compares, then the slot index. */
typedef struct BrRankPair {
    int key;
    int idx;
} BrRankPair;
extern int DAT_10af2200;
int BrNetGetA102212D0(int param_1);

/* WHAT IT DOES: recompute every driver's race placement. In a network game
 * (0x10226A48 set) each remote entry's rank is simply fetched from the shared
 * table. Locally it collects one {progress key, slot index} pair per live
 * driver slot (flag bit 2 skips a slot; the key comes from the car record at
 * +0xFF4, or from the slot's own field when no car is attached), sorts the
 * pairs with BrRankCmpKey, then walks the sorted order writing rank =
 * count-1-position into the car (+0xFF8) or back into the empty slot. */
/* The pair index is stored first and the key in each arm as pairs[n++]:
 * one store after the join allocated the key cursor last, a cyclic shift
 * of the collection loop's four registers against the original. */
/* @t4-pass 0x1005F580 4 2026-09-10 probes 250 bytes 259 insns 92 regions 1 rows 0 census yes  (tools/brally/crank.py) */
/* @implements 0x1005F580 glide BrRankAssign */

void BrRankAssign(void)
{
  int i, n;
  BrRankPair pairs[20];

  if (DAT_10226a48 != 0) {
    for (i = 0; i < DAT_100b2f04; i++)
      (&DAT_10af2200)[i * 0xada] = BrNetGetA102212D0((&DAT_10af2200)[i * 0xada - 0x3ad]);
    return;
  }
  n = 0;
  for (i = 0; i < DAT_100b2f00; i++) {
    if (!(DAT_10af0848[i].flags & 2)) {
      pairs[n].idx = i;
      if (DAT_10af0848[i].pCar != 0)
        pairs[n++].key = *(int *)(DAT_10af0848[i].pCar + 0xff4);
      else
        pairs[n++].key = DAT_10af0848[i].key;
    }
  }
  if (n != 0)
    qsort(pairs, n, 8, BrRankCmpKey);
  for (i = 0; i < n; i++) {
    if (DAT_10af0848[pairs[i].idx].pCar != 0)
      *(int *)(DAT_10af0848[pairs[i].idx].pCar + 0xff8) = DAT_100b2f00 - i - 1;
    else
      DAT_10af0848[pairs[i].idx].rank = DAT_100b2f00 - i - 1;
  }
}

