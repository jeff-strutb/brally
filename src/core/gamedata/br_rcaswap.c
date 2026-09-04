/* br_rcaswap.c -- gamedata: turning loaded N64-order data the right way round.
 *
 * The byte-order flips and the in-place pointer rebase that every .rca and
 * track blob goes through as it is read in. Filed out of slice2_16.c.
 *
 * See slice2_16.h for the per-function notes and gotchas.
 */
#ifdef BR_MATCHING_BUILD
/* The original binary is /MD: CRT calls resolve through the import table. */
#define _CRTIMP __declspec(dllimport)
#endif
#include "slice2_16.h"

/* DO NOT REMOVE: nothing here calls anything from <stdlib.h>, but the
 * intrinsic declarations it brings in are what put BrSwapU16x4's four byte
 * loads in the original's order (`mov cl,[eax+1]` before `mov ch,[eax]`).
 * Without this include that function comes out 7 bytes different, the loads
 * of the first pair swapped; measured 2026-09-03 while filing it out of
 * slice2_16.c, which had the include for its own reasons. */
#include <stdlib.h>

/* 0x1002BA20 -- fully unrolled in the original over offsets 0,2,4,6. */
/* WHAT IT DOES: flips the byte order of the four 16-bit values in one eight-
 * byte record of N64-ordered data. */
/* @implements 0x1002BA20 d3d BrSwapU16x4 */
/* @implements 0x10018A90 glide BrSwapU16x4 */
void BrSwapU16x4(void *pv)
{
    uint8_t *p = (uint8_t *)pv;

    /* MATCHING: the original inlines all four swaps and merges each pair of
     * byte stores into one 16-bit store built as `lo | (hi << 8)` -- the
     * `xor ecx,ecx / mov cl / mov ch / mov [p],cx` shape.  Calling
     * br16_swap_u16_at() four times will never reproduce that. */
    *(uint16_t *)(void *)(p + 0) = (uint16_t)(p[1] | (p[0] << 8));
    *(uint16_t *)(void *)(p + 2) = (uint16_t)(p[3] | (p[2] << 8));
    *(uint16_t *)(void *)(p + 4) = (uint16_t)(p[5] | (p[4] << 8));
    *(uint16_t *)(void *)(p + 6) = (uint16_t)(p[7] | (p[6] << 8));
}

/* 0x1002BA00 */
/* WHAT IT DOES: flips the byte order of a whole array of those four-value
 * records. */
/* @implements 0x1002BA00 d3d BrSwapU16x4Array */
/* @implements 0x10018A70 glide BrSwapU16x4Array */
void BrSwapU16x4Array(void *pv, int count)
{
    uint8_t *p;

    /* MATCHING: the original tests the count BEFORE it ever touches pv, so
     * the count register is allocated first (edi) and the cursor second
     * (esi).  Hoisting `p` above the guard swaps the two pushes.  The loop
     * is a do/while on the count itself -- `dec edi / jne` -- not a separate
     * induction variable. */
    if (count <= 0)
        return;
    p = (uint8_t *)pv;
    do {
        BrSwapU16x4(p);
        p += 8;
    } while (--count);
}

/* 0x1002BA60 */
/* WHAT IT DOES: flips the byte order of a whole array of 3D vectors read out
 * of an N64-ordered data file. */
/* @implements 0x1002BA60 d3d BrSwapVec3Array */
/* @implements 0x10018AD0 glide BrSwapVec3Array */
void BrSwapVec3Array(void *pv, int count)
{
    uint8_t *p;

    /* MATCHING: same shape as BrSwapU16x4Array -- count tested first, cursor
     * materialised inside the guard, do/while on the count. */
    if (count <= 0)
        return;
    p = (uint8_t *)pv;
    do {
        BrSwapVec3(p);
        p += 12;
    } while (--count);
}


/* 0x10018B60 */
/* The in-place N64->host pointer rebase the record fixup calls three times:
 * glide 0x100189E0, 43 bytes.  Zero stays zero; anything below the N64 load
 * base (SIGNED compare) becomes zero; the rest is rebased onto the host
 * image.  br_ai.c's static br_ai_reloc also claims this VA with a
 * by-value shape -- that claim is stale and should be retired when br_ai
 * is next touched. */
/* WHAT IT DOES: rewrite one pointer inside freshly loaded N64-format data so
 * it points at where the block actually landed in memory. A null stays null,
 * and anything pointing BELOW the block's own base is not a real pointer at
 * all and is zeroed rather than rebased into nonsense. The bases come from
 * BrSegSetBases. */
/* @implements 0x100189E0 glide BrSegPtrFixup */
void BrSegPtrFixup(uint32_t *p)
{
    if (*p == 0)
        return;
    if ((int32_t)*p < g_brSegN64Base) {
        *p = 0;
        return;
    }
    *p = (uint32_t)(g_brSegHostBase - g_brSegN64Base) + *p;
}
