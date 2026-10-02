/* br_race.c -- menus.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */
/* The original is /MD: CRT calls go through the import
 * table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "slice2_25.h"   /* br_globals: its objects */
#include "br_ui.h"
#include <stdint.h>


/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: set the HUD race-position icon from the current standings and AI difficulty. */
/* @implements 0x10038980 glide BrRacePosIconSet */

int BrRacePosIconSet(BrUiCtl_ *param_1)

{
  if (0 < (DAT_10ac5a48[0])) {
    switch((DAT_10ac5a48[0])) {
    case 1:
      param_1->w1E20C = 0x73;
      break;
    case 2:
      param_1->w1E20C = 0x72;
      break;
    case 3:
      param_1->w1E20C = 0x71;
      break;
    case 4:
      param_1->w1E20C = 0x70;
      break;
    case 5:
      param_1->w1E20C = 0x6f;
      break;
    default:
      param_1->w1E20C = 0xffff;
    }
  }
  if ((DAT_10ac5a48[0]) == 0) {
    switch((*(int *)&g_aBrAA26F4) & 0xff) {
    case 1:
      param_1->w1E20C = 0x47;
      return 1;
    case 2:
      param_1->w1E20C = 0x49;
      return 1;
    case 3:
      param_1->w1E20C = 0x4b;
      return 1;
    case 4:
    case 5:
    case 6:
      param_1->w1E20C = 0x4d;
      return 1;
    default:
      param_1->w1E20C = 0xffff;
    }
  }
  return 1;
}

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: look up the race-position HUD icon from a table, returning -2 if the phase has not changed. */
/* @implements 0x10038C60 glide BrRaceIconLookup */

int BrRaceIconLookup(BrUiCtl_ *param_1)

{
  if ((g_brPAA29B8 == DAT_10ac5cbc) && (DAT_10ac5c40 == 0)) {
    return 0xfffffffe;
  }
  param_1->w1E20C = *(short *)((char *)k_AC5A0 + 8 + g_brSel0ABDF4 * 4)   /* 0x100ABD48 */;
  return 1;
}

