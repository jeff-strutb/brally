/* weatherselect.c -- the weather-select screen
 */
#include "tgr/common.h"

/* -- declarations -- */
extern int D_8026FF08;
extern int D_8028B940;
/* -- end declarations -- */

/* WHAT IT DOES: Tell whether a weather row may be chosen: always when the
 * caller allows it, otherwise only for one player on tracks other than the
 * two that force their own weather. */
/* @implements 0x80210F80 tgr BrWeatherSelectable */
int BrWeatherSelectable(int param_1)
{
  int bVar1;
  
  bVar1 = param_1 != 0;
  if (!bVar1) {
    bVar1 = 0;
    if ((D_8028B940 != 3) && (bVar1 = 0, D_8028B940 != 8)) {
      bVar1 = D_8026FF08 == 1;
    }
  }
  return bVar1;
}
