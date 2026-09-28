/* br_pool.c -- per-frame slot allocator. See br_pool.h.
 *
 * Transcribed from 0x10069490. The original computes the byte offset as
 *   (count + frame * 257) << 6
 * using shift/add strength reduction (shl 8 then add, then shl 6); written
 * here as the multiply it represents.
 */
#include "br_pool.h"

void *BrPoolAlloc(BrPool *pPool)
{
    uint32_t index;

    if (pPool->count < BR_POOL_SLOTS_USED) {
        index = pPool->count + pPool->frame * BR_POOL_SLOTS_BANK;
        pPool->count++;
    } else {
        /* Overflow: still counted, but everyone shares slot 256. */
        pPool->count++;
        index = BR_POOL_SLOTS_USED + pPool->frame * BR_POOL_SLOTS_BANK;
    }
    return pPool->pBase + (size_t)index * BR_POOL_SLOT_SIZE;
}

uint32_t BrPoolCount(const BrPool *pPool)
{
    return pPool->count;
}
