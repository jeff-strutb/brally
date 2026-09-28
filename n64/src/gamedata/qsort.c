/* qsort.c -- the game's qsort: element swaps by width and a selection sort
 * for short runs
 */
#include "tgr/common.h"

/* -- declarations -- */
/* -- end declarations -- */

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
