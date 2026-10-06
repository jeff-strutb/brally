/* br_zeroregions.c -- game data: clearing the per-race variable regions.
 *
 * BrZeroRegions (glide 0x1005C450) zeroes every block in a NULL-terminated
 * list of address/size pairs; a zero-sized block is stepped over.
 *
 * Filed out of the address batch slice3_40.c, with that file's preamble.
 */

#include <string.h>

/* Header prototype is cdecl; the original is thiscall.  Rename the
 * prototype so the thiscall definition is not a C2373 redefinition. */
#define BrCarInitTables BrCarInitTables_cdecl_hdr
#define BrCarClear29C8  BrCarClear29C8_cdecl_hdr
#define BrZeroRegions   BrZeroRegions_cdecl_hdr
#define BrPathWalk      BrPathWalk_port_hdr   /* defined on the raw node */
#include "slice3_40.h"
#undef BrCarInitTables
#undef BrCarClear29C8
#undef BrZeroRegions
#undef BrPathWalk
void BrZeroRegions(void);

#include "br_match.h"    /* BR_THISCALL1 */

/* ------------------------------------------------------------------ */
/* Byte-offset accessors into the car record.                          */
/* BrCar's first member is a byte array, so &pCar->a0000 == pCar and    */
/* the struct is at least 4-aligned (it contains floats), which every   */
/* offset used below is a multiple of.                                  */
/* ------------------------------------------------------------------ */
#define CAR_BYTES(c)     ((uint8_t *)(void *)(c))
#define CAR_AT(c, off)   ((void *)(CAR_BYTES(c) + (off)))
#define CAR_U8(c, off)   (*(uint8_t  *)CAR_AT(c, off))
#define CAR_U16(c, off)  (*(uint16_t *)CAR_AT(c, off))
#define CAR_I32(c, off)  (*(int32_t  *)CAR_AT(c, off))
#define CAR_U32(c, off)  (*(uint32_t *)CAR_AT(c, off))
#define CAR_F32(c, off)  (*(float    *)CAR_AT(c, off))
#define CAR_PTR(c, off)  (*(void *   *)CAR_AT(c, off))

/* 0x100633E0 */
/* WHAT IT DOES: zeroes each block of memory in a list of address-and-size
 * pairs, stopping at the first entry with no address. A block of size zero
 * is stepped over rather than cleared. */
/* An INDEXED loop over the global array, not a cursor pointer: VC5 then
 * strength-reduces the index into the original's two cursor copies (one
 * reads ->p, the other ->size and the next entry) across the back edge.
 * @t4-pass 0x1005C450 1 2026-09-09 probes 11 bytes 58 insns 26 regions 2 rows 2 census yes  (hand, fn.py variants)
 * @t4-pass 0x1005C450 2 2026-09-09 probes 10 bytes 58 insns 26 regions 2 rows 2 census yes  (position sweep) */
/* @implements 0x100633E0 d3d BrZeroRegions */
extern BrZeroRegion DAT_100b2f08[];    /* list head, 0x100B2F08 */
void BrZeroRegions(void)
{
    int i;

    for (i = 0; DAT_100b2f08[i].p != NULL; i++) {
        uint8_t *pBeg = (uint8_t *)DAT_100b2f08[i].p;
        uint8_t *pEnd = pBeg + DAT_100b2f08[i].size;

        if (pBeg < pEnd)
            memset(pBeg, 0, (size_t)(pEnd - pBeg));
    }
}
