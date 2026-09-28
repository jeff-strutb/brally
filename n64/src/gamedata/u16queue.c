/* u16queue.c -- reading a table of 16-bit entries through a cursor
 */
#include "tgr/common.h"

/* -- declarations -- */
extern unsigned short *D_80025C20;
extern unsigned short *D_80025C24;     /* per 64x64 grid cell: first entry (the next cell's is the end) */
/* -- end declarations -- */

/* WHAT IT DOES: For a cell of the 64 by 64 grid, the range of entries it
 * owns: the count in the high half and the first entry in the low half;
 * outside the grid, 0. */
/* @implements 0x8021E998 tgr BrGridCellRange */
unsigned int BrGridCellRange(int x, int y)
{
  unsigned short i;
  int first;

  if (x < 0 || x >= 64 || y < 0 || y >= 64) {
    return 0;
  }
  i = (unsigned char)x + (unsigned char)y * 64;
  first = D_80025C24[i];
  return (D_80025C24[(unsigned short)(i + 1)] - first) << 16 | first;
}

/* WHAT IT DOES: Read the next entry of the queue table through a cursor
 * (position, entries left) and move it on by one; with nothing left answer
 * zero and leave the cursor alone.  Both cursor halves are rewritten from
 * one packed word, as the PC twin BrU16QueuePop does.
 * RESIDUE (9): the packed word lands in a2 (a register variable); the ROM
 * keeps it in a scratch register (t9) and every later temporary shifts by
 * one.  Local types, inline vs named, store order and 145 permuter compiles
 * leave 9. */
/* @implements 0x8021EA90 tgr BrU16QueuePop */
unsigned short BrU16QueuePop(unsigned short *q)
{
  unsigned short hi;
  unsigned short lo;
  unsigned int packed;

  hi = q[1];
  if (hi) {
    lo = q[0];
    packed = (lo + 1) | ((hi - 1) << 16);
    q[1] = packed >> 16;
    q[0] = packed;
    return D_80025C20[lo];
  }
  return 0;
}
