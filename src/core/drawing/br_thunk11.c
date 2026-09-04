/* br_thunk11.c -- drawing.
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

int FUN_1006e590();

/* WHAT IT DOES: thunk — forwards to the shared no-op at 0x1006E590. */
/* @implements 0x10011D10 glide BrThunk11D10 */
/* @n64 0x802288B4 exact */

int BrThunk11D10(void)

{
  FUN_1006e590();
  return;
}

#endif /* BR_MATCHING_BUILD */
