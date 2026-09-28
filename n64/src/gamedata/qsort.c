/* qsort.c -- the game's qsort: element swaps by width and a selection sort
 * for short runs
 */
#include "tgr/common.h"

/* -- declarations -- */
void BrQsortSelect(char *lo, char *hi, int size, int (*cmp)(void *, void *), void (*swap)(void *, void *, int));
void BrQsortSwap8(char *a, char *b, int n);
void BrQsortSwap16(short *a, short *b, unsigned int n);
void BrQsortSwap32(int *a, int *b, unsigned int n);
/* -- end declarations -- */

/* WHAT IT DOES: Sort num elements of width bytes at base by cmp: the
 * classic non-recursive quicksort (median-of-middle pivot, the smaller
 * partition stacked, runs of 8 or fewer handed to the selection sort),
 * swapping a word, halfword or byte at a time as the width allows. */
/* @implements 0x80255E80 tgr BrQsort */
void BrQsort(void *base, unsigned int num, unsigned int width, int (*comp)(void *, void *))
{
  char *lo, *hi;
  char *mid;
  char *loguy, *higuy;
  unsigned int size;
  char *lostk[30], *histk[30];
  int stkptr;
  void (*swap)(void *, void *, int);

  if ((width & 3) == 0) {
    swap = (void (*)(void *, void *, int))BrQsortSwap32;
  } else if ((width & 1) == 0) {
    swap = (void (*)(void *, void *, int))BrQsortSwap16;
  } else {
    swap = (void (*)(void *, void *, int))BrQsortSwap8;
  }
  if (num < 2 || width == 0) {
    return;
  }
  stkptr = 0;
  lo = base;
  hi = (char *)base + width * (num - 1);

recurse:
  size = (hi - lo) / width + 1;
  if (size <= 8) {
    BrQsortSelect(lo, hi, width, comp, swap);
  } else {
    mid = lo + (size / 2) * width;
    swap(mid, lo, width);
    loguy = lo;
    higuy = hi + width;
    for (;;) {
      do {
        loguy += width;
      } while (loguy <= hi && comp(loguy, lo) <= 0);
      do {
        higuy -= width;
      } while (higuy > lo && comp(higuy, lo) >= 0);
      if (higuy < loguy) {
        break;
      }
      swap(loguy, higuy, width);
    }
    swap(lo, higuy, width);
    if (higuy - 1 - lo >= hi - loguy) {
      if (lo + width < higuy) {
        lostk[stkptr] = lo;
        histk[stkptr] = higuy - width;
        ++stkptr;
      }
      if (loguy < hi) {
        lo = loguy;
        goto recurse;
      }
    } else {
      if (loguy < hi) {
        lostk[stkptr] = loguy;
        histk[stkptr] = hi;
        ++stkptr;
      }
      if (lo + width < higuy) {
        hi = higuy - width;
        goto recurse;
      }
    }
  }
  --stkptr;
  if (stkptr >= 0) {
    lo = lostk[stkptr];
    hi = histk[stkptr];
    goto recurse;
  }
}

/* WHAT IT DOES: Selection-sort the elements from lo to hi (inclusive, each
 * size bytes): repeatedly find the largest of lo..hi by cmp and swap it to
 * hi, then shrink hi by one. */
/* @implements 0x802560DC tgr BrQsortSelect */
void BrQsortSelect(char *lo, char *hi, int size, int (*cmp)(void *, void *), void (*swap)(void *, void *, int))
{
  char *p;
  char *max;

  while (lo < hi) {
    max = lo;
    for (p = lo + size; p <= hi; p += size) {
      if (cmp(p, max) > 0) {
        max = p;
      }
    }
    swap(max, hi, size);
    hi -= size;
  }
}

/* WHAT IT DOES: Swap two elements of n bytes, a byte at a time. */
/* @implements 0x802561A8 tgr BrQsortSwap8 */
void BrQsortSwap8(char *a, char *b, int n)
{
  char t;

  if (a != b) {
    while (n--) {
      t = *a;
      *a++ = *b;
      *b++ = t;
    }
  }
}

/* WHAT IT DOES: Swap two elements of n bytes, a halfword at a time. */
/* @implements 0x802561E4 tgr BrQsortSwap16 */
void BrQsortSwap16(short *a, short *b, unsigned int n)
{
  short t;

  n >>= 1;
  if (a != b) {
    while (n--) {
      t = *a;
      *a++ = *b;
      *b++ = t;
    }
  }
}

/* WHAT IT DOES: Swap two elements of n bytes, a word at a time. */
/* @implements 0x80256228 tgr BrQsortSwap32 */
void BrQsortSwap32(int *a, int *b, unsigned int n)
{
  int t;

  n >>= 2;
  if (a != b) {
    while (n--) {
      t = *a;
      *a++ = *b;
      *b++ = t;
    }
  }
}
