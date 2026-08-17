/* br_bits.c -- see br_bits.h. */
#include "br_bits.h"

/* 0x10035FA0 -- note it reads pending once and writes both fields, so a bit
 * present in pending and already set in latched stays set (OR, not XOR). */
/* WHAT IT DOES: moves the chosen bits from "waiting" to "taken" in a two-
 * word latch, leaving the rest waiting. A bit that was already taken stays
 * taken, because the merge is an OR and not a flip. */
/* @implements 0x10035FA0 d3d BrBitLatchTake */
void BrBitLatchTake(BrBitLatch *pLatch, uint32_t mask)
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
    int i;

    for (i = 0; i < 3; i++) {
        unsigned char a = p[i * 4 + 0];
        unsigned char b = p[i * 4 + 1];
        p[i * 4 + 0] = p[i * 4 + 3];
        p[i * 4 + 3] = a;
        p[i * 4 + 1] = p[i * 4 + 2];
        p[i * 4 + 2] = b;
    }
}

void *BrHandleLookup(void *const *apTable, uint32_t handle)
{
    if (handle < BR_HANDLE_MIN || handle > BR_HANDLE_MAX)
        return 0;
    return apTable[handle];
}
