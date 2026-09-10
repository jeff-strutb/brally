/* br_rank.c -- racing.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import
 * table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdint.h>
#include <stdlib.h>

#ifdef BR_MATCHING_BUILD


/* 0x10066620 */
/* WHAT IT DOES: the "who is ahead" test used when the game sorts the field
 * into race order -- it compares two drivers' progress figures and says which
 * comes first. If either figure is not a real number the answer it gives is
 * "less than" rather than "equal", which is the original's behaviour and not
 * a tidy-up opportunity. */
/* @implements 0x10066620 d3d BrRankCmpKey */
/* @implements 0x1005F690 glide BrRankCmpKey */
int BrRankCmpKey(const void *pA, const void *pB)
{
    /* Orig is `fld [ecx]; fcomp [edx]` twice -- no float locals. */
    if (*(const float *)pA > *(const float *)pB)
        return 1;
    if (!(*(const float *)pA >= *(const float *)pB))
        return -1;
    return 0;
}

extern volatile int DAT_10226a48;
extern int DAT_100b2f00;
extern int DAT_100b2f04;
extern int DAT_10af0858;
extern char DAT_10af084c;
extern int DAT_10af2200;
int BrNetGetA102212D0(int param_1);

/* WHAT IT DOES: recompute every driver's race placement. In a network game
 * (0x10226A48 set) each remote entry's rank is simply fetched from the shared
 * table. Locally it collects one {progress key, slot index} pair per live
 * driver slot (flag bit 2 skips a slot; the key comes from the car record at
 * +0xFF4, or from the slot's own field when no car is attached), sorts the
 * pairs with BrRankCmpKey, then walks the sorted order writing rank =
 * count-1-position into the car (+0xFF8) or back into the empty slot. */
/* RESIDUE (colouring only): size- and insn-exact 259/259 B, 92/92 insns,
 * regnorm rows 0+0, 1 masked region.  Loop 1's four registers sit one
 * cyclic shift off (orig key-cursor/eax, key/ecx, slot/edx, i/edi; recomp
 * edi/eax/ecx/edx): the derived key cursor is allocated FIRST in the
 * original and LAST here.  DEAD 2026-09-09: twin explicit cursors (fold to
 * one + cached bl,2), fully indexed pair stores (same fold), char-vs-int
 * cursor type split (same), cursor-idx + indexed-key (multiset exact,
 * rotation), function order swap to address order (no movement),
 * cursor-key + indexed-idx (kept: positional DIFFS 22 -> 16).  Gate 0+A
 * PASS; parked for Gate B's counted ledger.
 * @t4-pass 0x1005F580 1 2026-09-09 probes 6 bytes 259 insns 92 regions 1 rows 0 census no
 * @t4-pass 0x1005F580 2 2026-09-10 probes 12 bytes 259 insns 92 regions 1 rows 0 census no
 * @t4-pass 0x1005F580 3 2026-09-10 probes 11 bytes 259 insns 92 regions 1 rows 0 census yes
 * Pass 2 attacked the cyclic shift where the 2026-09-09 list had not: the
 * four locals' DECLARATION order (key cursor first, last, and after the
 * array), the two cursors' init order, the key as a ternary, the index store
 * moved below the key store, compound increments, a pre-advanced cursor
 * written through [-2], the key read hoisted above the flag test, and a
 * separate named key temp.  Pass 3 permuted the tail loop and the network
 * loop (hoisted slot pointer, hoisted rank expression, two re-associations
 * of `n - i - 1`, swapped increments, indexed vs dereferenced cursor, `!= 0`
 * vs `> 0` on the qsort guard, and the call result stored directly).  Every
 * one of the 23 compiles left the rotation exactly where it was; the only
 * movers made it worse (pre-advanced cursor 1+1, hoisted rank 0+2).
 * `corpus.py find --from 0x1005F580 --at 0x5c --len 12` is a MISS -- loop 1's
 * construct is not proven anywhere in the solved tree, so there is no
 * spelling to copy.  That MISS is pass 3's census. */
/* @t3 0x1005F580 2026-09-10 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 259/259 insns 92/92 rows 0+0 regions 1 oracle UNCLASSIFIED
 * @t3-effort passes 2 zero-movement 2 3
 * The residue is loop 1's four-register cyclic shift and nothing else: the
 * derived key cursor is allocated FIRST in the original and LAST here, which
 * rotates key-cursor/key/slot/i one place.  Size, instruction count and the
 * register-blind multiset are all exact (259/259 B, 92/92 insns, 0+0, one
 * masked region).  The dossier and the full 29-compile dead list are in the
 * RESIDUE block above; the corpus is a MISS on loop 1's 12-instruction run,
 * so no proven spelling exists to copy.
 * Do not reopen before the end-grind (CLAUDE.md rule 12). */
/* @implements 0x1005F580 glide BrRankAssign */

void BrRankAssign(void)

{
  int uVar1;
  int *piVar2;
  int iVar3;
  int *piVar4;
  int _NumOfElements;
  int *puVar5;
  int iVar7;
  int local_a0 [40];

  if (DAT_10226a48 != 0) {
    iVar7 = 0;
    if (0 < DAT_100b2f04) {
      puVar5 = &DAT_10af2200;
      do {
        uVar1 = BrNetGetA102212D0(puVar5[-0x3ad]);
        *puVar5 = uVar1;
        iVar7 = iVar7 + 1;
        puVar5 = puVar5 + 0xada;
      } while (iVar7 < DAT_100b2f04);
      return;
    }
  }
  else {
    _NumOfElements = 0;
    iVar7 = 0;
    if (0 < DAT_100b2f00) {
      piVar2 = local_a0;
      piVar4 = &DAT_10af0858;
      do {
        if ((*(unsigned char *)(piVar4 + 2) & 2) == 0) {
          iVar3 = *piVar4;
          local_a0[_NumOfElements * 2 + 1] = iVar7;
          if (iVar3 != 0) {
            iVar3 = *(int *)(iVar3 + 0xff4);
          }
          else {
            iVar3 = piVar4[-4];
          }
          *piVar2 = iVar3;
          _NumOfElements = _NumOfElements + 1;
          piVar2 = piVar2 + 2;
        }
        iVar7 = iVar7 + 1;
        piVar4 = piVar4 + 0x20;
      } while (iVar7 < DAT_100b2f00);
    }
    if (_NumOfElements != 0) {
      qsort(local_a0,_NumOfElements,8,BrRankCmpKey);
    }
    iVar7 = 0;
    if (0 < _NumOfElements) {
      piVar2 = local_a0 + 1;
      do {
        if ((&DAT_10af0858)[*piVar2 * 0x20] != 0) {
          *(int *)((&DAT_10af0858)[*piVar2 * 0x20] + 0xff8) = DAT_100b2f00 - iVar7 - 1;
        }
        else {
          *(int *)(&DAT_10af084c + *piVar2 * 0x80) = DAT_100b2f00 - iVar7 - 1;
        }
        iVar7 = iVar7 + 1;
        piVar2 = piVar2 + 2;
      } while (iVar7 < _NumOfElements);
    }
  }
  return;
}

#endif /* BR_MATCHING_BUILD */
