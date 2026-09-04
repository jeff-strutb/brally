/* br_clear.c -- drawing.
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

extern int DAT_104ab504;

/* WHAT IT DOES: clear a one-shot flag (zeroes it if set, idempotent). */
/* @implements 0x10013F00 glide BrClearFlag_AB504 */

int BrClearFlag_AB504(void)

{
  if (DAT_104ab504 != 0) {
    DAT_104ab504 = 0;
  }
  return;
}

#endif /* BR_MATCHING_BUILD */
