/* br_d.c -- net.
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

int FUN_10037130();
extern int g_brPA9D008;

/* WHAT IT DOES: send DPlay message 0x60000006 to the local player recorded at +8 of the
 * network object, if one is live. */
/* @implements 0x10037180 glide BrDPlayMsg6SendSelf */

void BrDPlayMsg6SendSelf(void)

{
  if ((g_brPA9D008 != 0) && (*(int *)(g_brPA9D008 + 8) != 0)) {
    FUN_10037130(g_brPA9D008,*(int *)(g_brPA9D008 + 8));
  }
  return;
}

#endif /* BR_MATCHING_BUILD */
