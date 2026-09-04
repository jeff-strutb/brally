/* br_input.c -- audio.
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

int FUN_10037720();


/* WHAT IT DOES: return 1 if live input or demo playback is active. */
/* @implements 0x10037780 glide BrInputOrPlaybackActive */

int BrInputOrPlaybackActive(void)

{
  int iVar1;
  
  iVar1 = BrFn1005FFD0();
  if (iVar1 < 0) {
    iVar1 = FUN_10037720();
    if (iVar1 == 0) {
      return 0;
    }
  }
  return 1;
}

#endif /* BR_MATCHING_BUILD */
