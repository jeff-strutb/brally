/* br_zeroregions.c -- game data: clearing the per-race variable regions.
 *
 * BrZeroRegions (glide 0x1005C450) zeroes every block in a NULL-terminated
 * list of address/size pairs; a zero-sized block is stepped over.
 *
 * Filed out of the address batch slice3_40.c, with that file's preamble.
 */

#include <string.h>

#ifdef BR_MATCHING_BUILD
/* Header prototype is cdecl; the original is thiscall.  Rename the
 * prototype so the thiscall definition is not a C2373 redefinition. */
#define BrCarInitTables BrCarInitTables_cdecl_hdr
#define BrCarClear29C8  BrCarClear29C8_cdecl_hdr
#define BrZeroRegions   BrZeroRegions_cdecl_hdr
#define BrPathWalk      BrPathWalk_port_hdr   /* defined on the raw node */
#endif
#include "slice3_40.h"
#ifdef BR_MATCHING_BUILD
#undef BrCarInitTables
#undef BrCarClear29C8
#undef BrZeroRegions
#undef BrPathWalk
void BrZeroRegions(void);
#endif

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
/* Residue: two `mov R,R` copies of the loop cursor the original keeps live
 * across the back edge (reads ->p via one register, ->size/next via the
 * other) -- the VARIABLE-IDENTITY-IS-INERT live-range class.  DEAD
 * 2026-09-09: guard on the global directly (moves the address materialise
 * below the test, FIRSTDIV +0x5 -> +0x8, bytes unchanged); do-while; a
 * lookahead pNext local read before/after ++; a second cursor local q in
 * four shapes (z7 reaches 28/28 insns but misplaces both copies); a size
 * local; every slot in the TU (10).  Corpus MISS on the loop tail at +0x2d.
 * @t4-pass 0x1005C450 1 2026-09-09 probes 11 bytes 58 insns 26 regions 2 rows 2 census yes  (hand, fn.py variants)
 * @t4-pass 0x1005C450 2 2026-09-09 probes 10 bytes 58 insns 26 regions 2 rows 2 census yes  (position sweep) */
/* @t3 0x1005C450 2026-09-09 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 58/62 insns 26/28 rows 2+0 regions 2 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 1 2
 * residue is allocation/scheduling: 2+0 classified rows, 2 masked regions, 4 B short;
 * every row pairs under t3.py's canonical classes.  Effort: 2 counted
 * @t4-pass passes (ledger lines above, zero movement on passes 1 and 2);
 * crank candidates and scores in build/match/crank.log, dead probes in the
 * comment block above.  Do not reopen before the end-grind (CLAUDE.md rule 12). */
/* @implements 0x100633E0 d3d BrZeroRegions */
#ifdef BR_MATCHING_BUILD
extern BrZeroRegion DAT_100b2f08[];    /* list head, 0x100B2F08 */
void BrZeroRegions(void)
{
    BrZeroRegion *pList = DAT_100b2f08;

    if (pList->p == NULL)
        return;
    for (;;) {
        uint8_t *pBeg = (uint8_t *)pList->p;
        uint8_t *pEnd = pBeg + pList->size;

        if (pBeg < pEnd)
            memset(pBeg, 0, (size_t)(pEnd - pBeg));
        ++pList;
        if (pList->p == NULL)
            break;
    }
}
#else
void BrZeroRegions(BrZeroRegion *pList)
{
    if (pList == NULL || pList->p == NULL) {
        return;
    }
    for (;;) {
        uint8_t *pBeg = (uint8_t *)pList->p;
        uint8_t *pEnd = pBeg + pList->size;

        /* the original's guard is an UNSIGNED `jae`, i.e. skip when the
         * end pointer did not advance; size 0 is the only reachable way */
        if (pBeg < pEnd) {
            memset(pBeg, 0, (size_t)(pEnd - pBeg));
        }
        ++pList;
        if (pList->p == NULL) {
            break;
        }
    }
}
#endif
