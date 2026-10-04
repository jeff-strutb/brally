/* br_x100746.c -- geometry.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */
/* The original is /MD: CRT calls go through the import
 * table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include <stdint.h>
#include <string.h>
#include "br_cartypes.h"



/* ================================================================== */
/* Misc                                                                */
/* ================================================================== */

/* 0x100746E0 */
/* WHAT IT DOES: copies seven values into a block, with a deliberate shuffle:
 * the last argument lands in the first written slot and the rest shift down
 * behind it, and the block's very first slot is left untouched. What the
 * block is for is not established. */
/* @implements 0x100746E0 d3d BrX100746E0 */
void BrX100746E0(struct BrRbForce *pDst,
                 unsigned int a2, unsigned int a3, unsigned int a4,
                 unsigned int a5, unsigned int a6, unsigned int a7,
                 unsigned int a8)
{
    /* the block is a BrRbForce: a8 is its kind, a2..a7 the two vectors'
     * float bits; pNext (the first slot) is left to the caller */
    pDst->kind = (int32_t)a8;
    memcpy(&pDst->f.x, &a2, 4);
    memcpy(&pDst->f.y, &a3, 4);
    memcpy(&pDst->f.z, &a4, 4);
    memcpy(&pDst->r.x, &a5, 4);
    memcpy(&pDst->r.y, &a6, 4);
    memcpy(&pDst->r.z, &a7, 4);
}

