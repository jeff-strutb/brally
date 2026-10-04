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

#define _CRTIMP __declspec(dllimport)
#include <stdlib.h>

extern unsigned int DAT_10697a58;
extern int DAT_106b7aa0;

/* WHAT IT DOES: search the live texture table for a record whose texel
 * source and palette source match the probe. If either the candidate or the
 * probe has mode != 1, that slot is a hit; only when both modes are 1 does
 * it also demand the eight render-state bytes agree. Returns the index, or
 * -1 if nothing matches (including when the table is empty). */
/* @t4-pass 0x10027A70 1 2026-09-07 probes 86 bytes 228 insns 63 regions 2 rows 9 census yes  (tools/crank.py) */
/* @t4-pass 0x10027A70 2 2026-09-07 probes 87 bytes 228 insns 63 regions 2 rows 9 census yes  (tools/crank.py) */
/* @t4-pass 0x10027A70 3 2026-09-13 probes 10 bytes 235 insns 68 regions 1 rows 0 census no  (hand, fn.py variants: const one, no q, int n, int i, one-first, ++i, increment order, n>i, unsigned char compares, single-expression p init; all 235/68/0+0 except n>i and p-init 1+1) */
/* @t4-pass 0x10027A70 4 2026-09-13 probes 10 bytes 235 insns 68 regions 1 rows 0 census yes  (slot census: one slot, the pReq read; fn.py variants around it: const q, byte-stride walker, merged one-tests, declaration order, +0x50 byte init, q-before-n, merged head test, uncast return, q inside the loop, p[0]; all 235/68/0+0 except the +0x50 init 1+1) */
/* @implements 0x10027A70 glide FUN_10027a70 */
int FUN_10027a70(int *pReq)
{
  unsigned int n;
  unsigned int i;
  int *q;
  int *p;
  int one;

  /* The loop is written with its entry test made explicit (`i = 0; if
   * (i < n)`) and the table pointer formed inside it: VC5 then loads the
   * pointer and adds 0x50 in the loop preheader, after the pushes and the
   * entry test, as the original does.  Formed before a plain `for`, the
   * load is hoisted above the pushes. */
  n = DAT_10697a58;
  q = pReq;
  one = 1;
  i = 0;
  if (i < n) {
  p = (int *)DAT_106b7aa0;
  p += 0x14;
  for (; i < n; i++, p += 0xad) {
    if (p[-1] == q[0x12]) {
      if (*p == q[0x13]) {
        if (p[0x86] != one) {
          return (int)i;
        }
        if (q[0x99] != one) {
          return (int)i;
        }
        if (((char *)p)[0x244] == ((char *)q)[0x290] &&
            ((char *)p)[0x245] == ((char *)q)[0x291] &&
            ((char *)p)[0x246] == ((char *)q)[0x292] &&
            ((char *)p)[0x247] == ((char *)q)[0x293] &&
            ((char *)p)[0x248] == ((char *)q)[0x294] &&
            ((char *)p)[0x249] == ((char *)q)[0x295] &&
            ((char *)p)[0x24a] == ((char *)q)[0x296] &&
            ((char *)p)[0x24b] == ((char *)q)[0x297]) {
          return (int)i;
        }
      }
    }
  }
  }
  return -1;
}

