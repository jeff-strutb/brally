/* br_copy8.c -- gamedata.
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
/* 4. .rca byte-swap and fixup helpers                                */
/* ================================================================== */

/* 0x1002B930 */
/* WHAT IT DOES: copies thirty-two bytes from one place to another,
 * destination first. Used where the game moves a fixed-size record around. */
/* @implements 0x1002B930 d3d BrCopy8Words */
void BrCopy8Words(void *pDst, const void *pSrc)
{
    memcpy(pDst, pSrc, 8 * sizeof(uint32_t));
}

#endif /* BR_MATCHING_BUILD */
