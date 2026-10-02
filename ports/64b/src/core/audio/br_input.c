/* br_input.c -- audio.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */
/* The original is /MD: CRT calls go through the import
 * table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include <stdint.h>


/* FUN_10037720: prototype in br_funcs.h */


/* WHAT IT DOES: return 1 if live input or demo playback is active. */
/* @implements 0x10037780 glide BrInputOrPlaybackActive */

int BrInputOrPlaybackActive(void)

{
  int iVar1;
  
  iVar1 = BrFn1005FFD0();
  if (iVar1 < 0) {
    iVar1 = BrInputAnyActive();
    if (iVar1 == 0) {
      return 0;
    }
  }
  return 1;
}

