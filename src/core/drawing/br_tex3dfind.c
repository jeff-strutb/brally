/* br_tex3dfind.c -- drawing: look up a live texture-table slot by key.
 *
 * 0x10027A70 walks the 0x2B4-stride table at DAT_106b7aa0. Neighbour
 * 0x10027710 already calls it as the dedup before a TMEM allocate.
 *
 * RESIDUE 7B /O2, FIRSTDIV +0x10. Prologue through the eight byte-compares
 * match. Back-edge is still `jae empty; jmp body` (merged n==0 epilogue)
 * vs orig `jb body; or eax,-1; 4 pops`. Tried: do-while, for(;;) with
 * in-loop return, break-then-return-i, i-n-1 exhaust, #pragma optimize
 * ("g",off), same TU as br_tex3d_append (regressed append). */
#ifdef BR_MATCHING_BUILD

#define _CRTIMP __declspec(dllimport)
#include <stdlib.h>

extern unsigned int DAT_10697a58;
extern int DAT_106b7aa0;

/* WHAT IT DOES: search the live texture table for a record whose texel
 * source and palette source match the probe. If either the candidate or the
 * probe has mode != 1, that slot is a hit; only when both modes are 1 does
 * it also demand the eight render-state bytes agree. Returns the index, or
 * -1 if nothing matches (including when the table is empty). */
/* @implements 0x10027A70 glide FUN_10027a70 */
int FUN_10027a70(int *pReq)
{
  unsigned int n;
  unsigned int i;
  int *q;
  int *p;
  int one;

  n = DAT_10697a58;
  i = 0;
  if (n <= 0) {
    return -1;
  }
  q = pReq;
  p = (int *)DAT_106b7aa0;
  p += 0x14;
  one = 1;
  for (;;) {
    if (p[-1] == q[0x12]) {
      if (*p == q[0x13]) {
        if (p[0x86] != one) {
          break;
        }
        if (q[0x99] != one) {
          break;
        }
        if (((char *)p)[0x244] == ((char *)q)[0x290] &&
            ((char *)p)[0x245] == ((char *)q)[0x291] &&
            ((char *)p)[0x246] == ((char *)q)[0x292] &&
            ((char *)p)[0x247] == ((char *)q)[0x293] &&
            ((char *)p)[0x248] == ((char *)q)[0x294] &&
            ((char *)p)[0x249] == ((char *)q)[0x295] &&
            ((char *)p)[0x24a] == ((char *)q)[0x296] &&
            ((char *)p)[0x24b] == ((char *)q)[0x297]) {
          break;
        }
      }
    }
    i = i + 1;
    p += 0xad;
    if (i >= n) {
      return -1;
    }
  }
  return (int)i;
}

#endif /* BR_MATCHING_BUILD */
