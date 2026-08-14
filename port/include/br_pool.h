/* br_pool.h -- per-frame slot allocator, decompiled from BRD3D.dll (0x10069490).
 *
 * Hands out fixed 64-byte slots from a per-frame bank. Each frame owns 257
 * slots: 256 usable plus a shared overflow slot that every request past the
 * limit returns. The original never fails and never returns NULL -- it just
 * keeps handing back the overflow slot, so late allocations in a frame
 * silently alias each other. Preserved, because callers were written against
 * that behaviour.
 *
 * The two base addresses in the original (0x10AF9BC0 and 0x10AFDBC0) differ by
 * exactly 0x4000 = 256 * 64, which is what shows the "overflow" path is simply
 * index 256 of the same per-frame bank rather than a separate buffer.
 */
#ifndef BR_POOL_H
#define BR_POOL_H

#include <stddef.h>
#include <stdint.h>

#define BR_POOL_SLOT_SIZE   64
#define BR_POOL_SLOTS_USED  256                    /* per frame */
#define BR_POOL_SLOTS_BANK  (BR_POOL_SLOTS_USED + 1)   /* 257: +overflow */

typedef struct BrPool {
    uint8_t  *pBase;      /* 0x10AF9BC0 */
    uint32_t  frame;      /* 0x106C65EC */
    uint32_t  count;      /* 0x10B01C40 -- slots taken this frame */
} BrPool;

/* Allocate one 64-byte slot. Never returns NULL; past 256 allocations in a
 * frame it returns the shared overflow slot. */
void *BrPoolAlloc(BrPool *pPool);

/* Slots consumed this frame, including requests that overflowed. */
uint32_t BrPoolCount(const BrPool *pPool);

#endif /* BR_POOL_H */
