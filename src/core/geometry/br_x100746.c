/* br_x100746.c -- geometry.
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

#ifdef BR_MATCHING_BUILD


/* ================================================================== */
/* Misc                                                                */
/* ================================================================== */

/* 0x100746E0 */
/* WHAT IT DOES: copies seven values into a block, with a deliberate shuffle:
 * the last argument lands in the first written slot and the rest shift down
 * behind it, and the block's very first slot is left untouched. What the
 * block is for is not established. */
/* @implements 0x100746E0 d3d BrX100746E0 */
void BrX100746E0(unsigned int *pDst,
                 unsigned int a2, unsigned int a3, unsigned int a4,
                 unsigned int a5, unsigned int a6, unsigned int a7,
                 unsigned int a8)
{
    pDst[1] = a8;   /* the LAST argument, into the FIRST written slot */
    pDst[2] = a2;
    pDst[3] = a3;
    pDst[4] = a4;
    pDst[5] = a5;
    pDst[6] = a6;
    pDst[7] = a7;
    /* pDst[0] is deliberately never written */
}

#endif /* BR_MATCHING_BUILD */
