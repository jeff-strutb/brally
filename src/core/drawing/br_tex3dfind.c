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
/* @t4-pass 0x10027A70 1 2026-09-07 probes 86 bytes 228 insns 63 regions 2 rows 9 census yes  (tools/crank.py) */
/* @t4-pass 0x10027A70 2 2026-09-07 probes 87 bytes 228 insns 63 regions 2 rows 9 census yes  (tools/crank.py) */
/* @t4-pass 0x10027A70 3 2026-09-13 probes 10 bytes 235 insns 68 regions 1 rows 0 census no  (hand, fn.py variants: const one, no q, int n, int i, one-first, ++i, increment order, n>i, unsigned char compares, single-expression p init; all 235/68/0+0 except n>i and p-init 1+1) */
/* @t4-pass 0x10027A70 4 2026-09-13 probes 10 bytes 235 insns 68 regions 1 rows 0 census yes  (slot census: one slot, the pReq read; fn.py variants around it: const q, byte-stride walker, merged one-tests, declaration order, +0x50 byte init, q-before-n, merged head test, uncast return, q inside the loop, p[0]; all 235/68/0+0 except the +0x50 init 1+1) */
/* @t3 0x10027A70 2026-09-13 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 235/235 insns 68/68 rows 0+0 regions 1 oracle UNCLASSIFIED
 * @t3-effort passes 4 zero-movement 3 4
 * residue is scheduling only: the loop preheader's global load and its
 * +0x50 bias sit above the pushes and the entry test, where the original
 * has them after the test (register-blind multiset identical, 25 positional
 * bytes in one region).  Dossier and dead list are in the comment inside
 * the function; ledger lines above.  Do not reopen before the end-grind
 * (CLAUDE.md rule 12). */
/* @implements 0x10027A70 glide FUN_10027a70 */
int FUN_10027a70(int *pReq)
{
  unsigned int n;
  unsigned int i;
  int *q;
  int *p;
  int one;

  /* A `for` over i, not a guarded do-while with an in-loop `return -1`:
   * the entry test is shrink-wrapped with its OWN duplicated epilogue
   * (`pop; pop; pop; or eax,-1; pop; ret` at the tail) while the loop's
   * fall-through `return -1` keeps a second, `or`-first epilogue, and the
   * three found-returns share the third.  The old shape cross-jumped the
   * in-loop exit into the guard's tail (2+7 rows); this one is register-
   * blind exact (0+0).  RESIDUE (25 positional bytes, scheduling): VC5
   * hoists `p`'s global load and `add ecx,0x50` above the pushes and the
   * entry test, where the original loads them in the loop preheader; an
   * i-indexed `p` inside the loop LICMs them into the preheader but moves
   * the IV anchor to the byte-compare block (+0x294), and the for-init
   * form is the same as statements before the loop.  2026-09-13. */
  n = DAT_10697a58;
  q = pReq;
  p = (int *)DAT_106b7aa0;
  p += 0x14;
  one = 1;
  for (i = 0; i < n; i++, p += 0xad) {
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
  return -1;
}

#endif /* BR_MATCHING_BUILD */
