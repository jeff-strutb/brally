/* br_reset.c -- racing.
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

extern int DAT_106ec740;
extern int DAT_106ec744;
extern int DAT_106e7294;
extern int DAT_106ec768;
extern int DAT_106ed588;
extern int DAT_106b7ac0;
extern int DAT_106e9d8c;

/* WHAT IT DOES: zero the race pad/step state block.  The 0x106ED588 store is
 * a LOAD of the just-zeroed 0x106EC768 (Ghidra folds it to 0; the /Od bytes
 * re-read it). */
/* @implements 0x1002E13B glide BrReset_1002E13B */

void BrReset_1002E13B(void)

{
  DAT_106ec740 = 0;
  DAT_106ec744 = 0;
  DAT_106e7294 = 0;
  DAT_106ec768 = 0;
  DAT_106ed588 = DAT_106ec768;
  DAT_106b7ac0 = 0;
  DAT_106e9d8c = 0;
  return;
}

#endif /* BR_MATCHING_BUILD */
