/* br_race.c -- menus.
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

extern int DAT_10ac5a48;
extern int DAT_10ac5a4c;

/* WHAT IT DOES: set the HUD race-position icon from the current standings and AI difficulty. */
/* @implements 0x10038980 glide BrRacePosIconSet */

int BrRacePosIconSet(int param_1)

{
  if (0 < DAT_10ac5a48) {
    switch(DAT_10ac5a48) {
    case 1:
      *(short *)(param_1 + 0x1e20c) = 0x73;
      break;
    case 2:
      *(short *)(param_1 + 0x1e20c) = 0x72;
      break;
    case 3:
      *(short *)(param_1 + 0x1e20c) = 0x71;
      break;
    case 4:
      *(short *)(param_1 + 0x1e20c) = 0x70;
      break;
    case 5:
      *(short *)(param_1 + 0x1e20c) = 0x6f;
      break;
    default:
      *(short *)(param_1 + 0x1e20c) = 0xffff;
    }
  }
  if (DAT_10ac5a48 == 0) {
    switch(DAT_10ac5a4c & 0xff) {
    case 1:
      *(short *)(param_1 + 0x1e20c) = 0x47;
      return 1;
    case 2:
      *(short *)(param_1 + 0x1e20c) = 0x49;
      return 1;
    case 3:
      *(short *)(param_1 + 0x1e20c) = 0x4b;
      return 1;
    case 4:
    case 5:
    case 6:
      *(short *)(param_1 + 0x1e20c) = 0x4d;
      return 1;
    default:
      *(short *)(param_1 + 0x1e20c) = 0xffff;
    }
  }
  return 1;
}

#endif /* BR_MATCHING_BUILD */
