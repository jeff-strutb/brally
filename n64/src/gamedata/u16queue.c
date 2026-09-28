/* u16queue.c -- reading a table of 16-bit entries through a cursor
 */
#include "tgr/common.h"

/* -- declarations -- */
extern unsigned short *D_80025C20;
extern unsigned short *D_80025C24;     /* per 64x64 grid cell: first entry (the next cell's is the end) */
extern unsigned short *D_80025C68;     /* the second queue table */
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
 * RESIDUE (4): the packed word and its high half swap t9/t0 -- the ROM
 * numbers the OR before the shift, ours after; the store order that fixes
 * the registers breaks the stores.  Local types, declaration order, named
 * vs CSE'd vs embedded packing, the PC twin's 0xFFFF spelling and 786
 * permuter compiles leave 4. */
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
    q[0] = packed & 0xffff;
    return D_80025C20[lo];
  }
  return 0;
}

/* WHAT IT DOES: BrU16QueuePop over the second queue table (D_80025C68):
 * read the next entry through a cursor (position, entries left) and move it
 * on by one; nothing left answers zero.
 * RESIDUE (4): as BrU16QueuePop. */
/* @implements 0x8021EADC tgr BrU16QueuePopB */
unsigned short BrU16QueuePopB(unsigned short *q)
{
  unsigned short hi;
  unsigned short lo;
  unsigned int packed;

  hi = q[1];
  if (hi) {
    lo = q[0];
    packed = (lo + 1) | ((hi - 1) << 16);
    q[1] = packed >> 16;
    q[0] = packed & 0xffff;
    return D_80025C68[lo];
  }
  return 0;
}
