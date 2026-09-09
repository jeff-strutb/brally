/* br_bits.c -- see br_bits.h. */
#include "br_bits.h"
#include "br_match.h"

/* 0x10035FA0 -- note it reads pending once and writes both fields, so a bit
 * present in pending and already set in latched stays set (OR, not XOR). */
/* WHAT IT DOES: moves the chosen bits from "waiting" to "taken" in a two-
 * word latch, leaving the rest waiting. A bit that was already taken stays
 * taken, because the merge is an OR and not a flip. */
/* @implements 0x10035FA0 d3d BrBitLatchTake */
/* @n64 0x80255910 located */
/* register allocation wall: orig loads EAX=mask before EDX=pending, compiler
 * reverses the load order. Not fixable without inline asm. */
#ifdef BR_MATCHING_BUILD
void __fastcall BrBitLatchTake(BrBitLatch *pLatch, void *_dummy, uint32_t mask)
#else
void BR_THISCALL BrBitLatchTake(BrBitLatch *pLatch, uint32_t mask)
#endif
{
    uint32_t pending = pLatch->pending;

    pLatch->latched |= (mask & pending);
    pLatch->pending  = (~mask) & pending;
}

/* 0x100383C0 -- unrolled swap of three u32s. */
/* WHAT IT DOES: turns a 3D vector the right way round: three numbers, each
 * with its bytes reversed. Boss Rally's data came from the N64 and stores
 * its numbers the other way round from a PC. */
/* @implements 0x100383C0 d3d BrSwapVec3 */
void BrSwapVec3(void *pv)
{
    unsigned char *p = (unsigned char *)pv;
    unsigned char t;

    /* temp holds HIGH on both pairs of each dword (p[3]/p[0] then p[2]/p[1]). */
    t = p[3];  p[3]  = p[0];  p[0]  = t;
    t = p[2];  p[2]  = p[1];  p[1]  = t;
    t = p[7];  p[7]  = p[4];  p[4]  = t;
    t = p[6];  p[6]  = p[5];  p[5]  = t;
    t = p[11]; p[11] = p[8];  p[8]  = t;
    t = p[10]; p[10] = p[9];  p[9]  = t;
}

/* 0x10018A50 (glide) == 0x1002B9E0 (d3d), 29 bytes, byte-identical.
 *
 * ONE BODY, AND IT LIVES HERE because both of the modules that need it are
 * leaves that must not depend on each other: br_track.c had it as
 * `swap_u16_run` and slice2_16.c as `BrSwapU16Array`, transcribed
 * independently under the two builds' addresses.  Neither was wrong, which is
 * the point -- they would have drifted, as 0x10022120's two copies did.
 *
 * THE COUNT IS SIGNED and the guard is `test ecx,ecx / jle`, so a negative
 * count is a no-op rather than a run of four billion.  br_track.c's copy took
 * an unsigned count and would have looped forever on one; nothing passed one,
 * so nothing showed it.
 *
 * The loop itself is `dec ecx / jne`, entered only after the guard, and the
 * source pointer is loaded ONCE before the loop label at 0x10018A5C -- the
 * jump target is the `xor edx,edx`, not the `mov eax,[esp+4]` above it.
 * Word-compose `lo=p[1]; hi=p[0]; *(u16*)p = lo|(hi<<8)` is the orig shape
 * (xor edx; mov dl/dh; mov [eax],dx). Remaining 4B is dh-then-dl vs
 * dl-then-dh -- same bag, TU-local schedule, do not grind.  DEAD probes
 * 2026-09-03, all still 4: writing the compose as `(hi << 8) | lo`
 * (VC5 canonicalises `|` operand order), swapping the two local
 * assignments to hi-then-lo, dropping the locals for one direct
 * `(p[0] << 8) | p[1]` expression, and a two-lane union written low lane
 * first (that one costs a stack slot: 42 B, 5+3).  VC5 always fills the
 * HIGH half of the word register first for this compose.
 *
 * DEAD probes 2026-09-07, all still 4 (register-blind multiset 0+0 -- the two
 * byte-loads are the SAME bag, only their order flips): char-typed locals
 * (`unsigned char lo, hi`) emit dh-first identically; moving the whole
 * function to the END of the TU (after BrHandleLookup) moved nothing.  Corpus
 * find --from 0x10018A50 --at 0xe --len 8 is a MISS: the low-then-high word
 * compose (mov dl,[r+1]; mov dh,[r]) is not proven anywhere in the solved
 * tree, so there is no spelling to copy -- this needs a SOURCE fact, not
 * another permutation.
 * @t4-pass 0x10018A50 1 2026-09-07 probes 2 bytes 29 insns 12 regions 1 rows 4 census yes */
/* WHAT IT DOES: reverses the byte order of a run of 16-bit numbers in place.
 * Boss Rally's data files came from the N64 and store their numbers the other
 * way round from a PC, so they have to be turned around after loading. Asking
 * for nothing, or for a negative number of them, does nothing. */
/* @t4-pass 0x10018A50 2 2026-09-07 probes 25 bytes 29 insns 12 regions 1 rows 0 census yes  (tools/crank.py) */
/* @t4-pass 0x10018A50 3 2026-09-07 probes 39 bytes 29 insns 12 regions 1 rows 0 census yes  (tools/crank.py) */
/* @t3 0x10018A50 2026-09-09 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 29/29 insns 12/12 rows 0+0 regions 1 oracle UNCLASSIFIED
 * @t3-effort passes 2 zero-movement 2 3
 * residue is register colouring only: identical register-blind instruction
 * multiset (rows 0+0), 1 masked region;
 * every row pairs under t3.py's canonical classes.  Effort: 2 counted
 * @t4-pass passes (ledger lines above, zero movement on passes 2 and 3);
 * crank candidates and scores in build/match/crank.log, dead probes in the
 * comment block above.  Do not reopen before the end-grind (CLAUDE.md rule 12). */
/* @implements 0x10018A50 glide BrSwapU16Array */
void BrSwapU16Array(void *pv, int count)
{
    unsigned char *p;

    if (count <= 0)
        return;
    p = (unsigned char *)pv;
    do {
        unsigned short lo, hi;
        lo = p[1];
        hi = p[0];
        *(unsigned short *)p = (unsigned short)(lo | (hi << 8));
        p += 2;
    } while (--count);
}

void *BrHandleLookup(void *const *apTable, uint32_t handle)
{
    if (handle < BR_HANDLE_MIN || handle > BR_HANDLE_MAX)
        return 0;
    return apTable[handle];
}
