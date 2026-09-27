/* trackdraw.c -- drawing the track: the grid cells in view and the objects
 * standing in them
 */
#include "tgr/common.h"

/* -- declarations -- */
typedef struct BrDrawSortItem { /* sorted before drawing */
  short x0;
  short key;                    /* 0x02 */
} BrDrawSortItem;
/* -- end declarations -- */

/* WHAT IT DOES: The qsort comparison the track drawer sorts with: by the
 * 16-bit key at +2, ascending. */
/* @implements 0x80234FC0 tgr BrDrawSortCmp */
int BrDrawSortCmp(BrDrawSortItem *a, BrDrawSortItem *b)
{
  if (a->key > b->key) {
    return 1;
  }
  if (a->key < b->key) {
    return -1;
  }
  return 0;
}
