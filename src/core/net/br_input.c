/* br_input.c -- net.
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

extern int DAT_10ac5bb4;
extern int DAT_10ac5e50;
extern int DAT_10ac6050;
extern int DAT_10ac610c;
extern int DAT_10ac6114;
extern int * DAT_10ac6730;
extern int DAT_10ac6734;
extern int DAT_10ac6738;
extern int DAT_10ac673c;

/* WHAT IT DOES: return 1 if any input source (joystick, keyboard, or net) is active. */
/* @implements 0x10037720 glide BrInputAnyActive */

int BrInputAnyActive(void)

{
  if (((((DAT_10ac6730 == 0) && (DAT_10ac6734 == 0)) && (DAT_10ac6738 == 0)) && (DAT_10ac673c == 0))
     && ((DAT_10ac5bb4 != 0 ||
         (((DAT_10ac5e50 == 0 && (DAT_10ac6050 == 0)) &&
          ((DAT_10ac610c == 0 && (DAT_10ac6114 == 0)))))))) {
    return 0;
  }
  return 1;
}

#endif /* BR_MATCHING_BUILD */
