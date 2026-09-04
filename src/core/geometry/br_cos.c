/* br_cos.c -- geometry.
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


/* WHAT IT DOES: cosine of a float, returned on the x87 stack (inlined fcos). */
/* @implements 0x100023E0 glide BrCosF */

double BrCosF(float param_1)
{
  return cos((double)param_1);
}

#endif /* BR_MATCHING_BUILD */
