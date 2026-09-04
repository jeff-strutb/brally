/* br_rank.c -- racing.
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


/* 0x10066620 */
/* WHAT IT DOES: the "who is ahead" test used when the game sorts the field
 * into race order -- it compares two drivers' progress figures and says which
 * comes first. If either figure is not a real number the answer it gives is
 * "less than" rather than "equal", which is the original's behaviour and not
 * a tidy-up opportunity. */
/* @implements 0x10066620 d3d BrRankCmpKey */
/* @implements 0x1005F690 glide BrRankCmpKey */
int BrRankCmpKey(const void *pA, const void *pB)
{
    /* Orig is `fld [ecx]; fcomp [edx]` twice -- no float locals. */
    if (*(const float *)pA > *(const float *)pB)
        return 1;
    if (!(*(const float *)pA >= *(const float *)pB))
        return -1;
    return 0;
}

#endif /* BR_MATCHING_BUILD */
