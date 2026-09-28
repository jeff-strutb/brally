/* rank.c -- ordering the cars into race positions
 */
#include "tgr/common.h"

/* -- declarations -- */
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
