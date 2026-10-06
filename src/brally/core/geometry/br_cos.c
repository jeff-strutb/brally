/* br_cos.c -- geometry.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */
/* The original is /MD: CRT calls go through the import
 * table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include <stdint.h>



/* WHAT IT DOES: cosine of a float, returned on the x87 stack (inlined fcos). */
/* @implements 0x100023E0 glide BrCosF */

double BrCosF(float param_1)
{
  return cos((double)param_1);
}

