/* br_bits.c -- see br_bits.h. */
#include "br_bits.h"
#include "br_match.h"

/* 0x10035FA0 -- note it reads pending once and writes both fields, so a bit
 * present in pending and already set in latched stays set (OR, not XOR). */
/* WHAT IT DOES: moves the chosen bits from "waiting" to "taken" in a two-
 * word latch, leaving the rest waiting. A bit that was already taken stays
 * taken, because the merge is an OR and not a flip. */
/* @implements 0x10035FA0 d3d BrBitLatchTake */
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
 * dl-then-dh -- same bag, TU-local schedule, do not grind. */
/* WHAT IT DOES: reverses the byte order of a run of 16-bit numbers in place.
 * Boss Rally's data files came from the N64 and store their numbers the other
 * way round from a PC, so they have to be turned around after loading. Asking
 * for nothing, or for a negative number of them, does nothing. */
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
