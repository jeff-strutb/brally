/* br_sub69.c -- racing.
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

extern int DAT_117a5f28;
int FUN_10069a80();

/* WHAT IT DOES: forward a parameter to FUN_10069A80 with a fixed first argument. */
/* @implements 0x10069DC0 glide BrSub69DC0 */

int BrSub69DC0(int param_1)

{
  FUN_10069a80(&DAT_117a5f28,param_1);
  return;
}

#endif /* BR_MATCHING_BUILD */
