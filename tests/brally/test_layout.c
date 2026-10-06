/* test_layout.c -- compile-time assertions on every struct layout the headers
 * claim. Plain C99, so these fail the BUILD, not a test run.
 *
 * This file exists because the project makes dozens of layout claims recovered
 * from index arithmetic in the original ("records are 0x2B68 bytes", "the POD
 * entry is 76"). Those claims were previously only in comments, where nothing
 * checked them. The first version of this file immediately caught one.
 *
 * IMPORTANT distinction, and the reason the POD assertion is written the way it
 * is: an ON-DISK record size and an IN-MEMORY struct size are different claims.
 * BrPodEntry is 76 bytes on disk but 80 in memory, because the port adds a NUL
 * terminator to the name for safety. That is fine ONLY because br_pod.c decodes
 * field-by-field; it would be a serious bug if anything ever fread() a
 * directory directly into an array of these.
 */
#include "br_slots.h"
#include "br_pod.h"
#include "br_vec.h"
#include "br_vecd.h"
#include "br_mat.h"
#include "br_span.h"
#include "br_pool.h"
#include <stdio.h>

/* C99 compile-time assertion: false -> char[-1] -> hard error. */
#define BR_ASSERT(name, cond) typedef char BR_ASSERT_##name[(cond) ? 1 : -1]

/* --- exact-size types: these MUST match the original ------------------- */
BR_ASSERT(BrVec3_is_12, sizeof(BrVec3) == 12);
BR_ASSERT(BrVec3d_is_24, sizeof(BrVec3d) == 24);
BR_ASSERT(BrMat4_is_64, sizeof(BrMat4) == 64);
BR_ASSERT(BrSlot_is_12, sizeof(BrSlot) == 12);

/* --- on-disk vs in-memory: asserted separately, on purpose ------------- */
#define BR_POD_ONDISK_ENTRY 76
BR_ASSERT(PodEntry_covers_ondisk, sizeof(BrPodEntry) >= BR_POD_ONDISK_ENTRY);
/* Deliberately differs: the port adds a NUL terminator. If this ever
 * becomes equal, re-check that nothing overlays the on-disk record. */
BR_ASSERT(PodEntry_differs_from_ondisk, sizeof(BrPodEntry) != BR_POD_ONDISK_ENTRY);

/* --- constants the original's arithmetic pins -------------------------- */
BR_ASSERT(BR_SPAN_ROWS_is_64, BR_SPAN_ROWS == 64);
BR_ASSERT(BR_POOL_SLOT_SIZE_is_64, BR_POOL_SLOT_SIZE == 64);
BR_ASSERT(BR_POOL_SLOTS_BANK_is_257, BR_POOL_SLOTS_BANK == 257);

int main(void)
{
    printf("  BrVec3 %zu  BrVec3d %zu  BrMat4 %zu  BrSlot %zu  BrPodEntry %zu (on disk %d)\n",
           sizeof(BrVec3), sizeof(BrVec3d), sizeof(BrMat4), sizeof(BrSlot),
           sizeof(BrPodEntry), BR_POD_ONDISK_ENTRY);
    /* status LAST: the harness reads the final line. */
    printf("layout: all compile-time assertions held -- 0 failures\n");
    return 0;
}
