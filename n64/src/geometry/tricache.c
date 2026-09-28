/* tricache.c -- the collision triangle cache: four slots, each holding the
 * triangle planes of one 32x32 grid cell, recycled least-recently used.
 */
#include "tgr/common.h"

/* -- declarations -- */
extern unsigned short D_8037EA80[4];   /* the cell each slot holds (0 = none) */
extern int D_8037EA90[4];              /* each slot's last-use stamp */
/* -- end declarations -- */

/* WHAT IT DOES: Drop the cache slot holding the grid cell under (x, y), if
 * any: its cell and use stamp go back to zero.  Returns 1 when a slot was
 * dropped, else 0. */
/* @implements 0x8025F0A4 tgr BrTriCacheDrop */
int BrTriCacheDrop(float x, float y)
{
  short cell;
  int i;

  cell = ((int)y / 32) * 64 + (int)x / 32;
  for (i = 0; i < 4; i++) {
    if (cell == D_8037EA80[i]) {
      D_8037EA90[i] = 0;
      D_8037EA80[i] = 0;
      return 1;
    }
  }
  return 0;
}
