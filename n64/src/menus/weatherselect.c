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
/* @t4-pass 0x80210F80 1 2026-09-26 compiles 17 best 17 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80210F80 2 2026-09-26 compiles 16 best 17 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80210F80 3 2026-09-26 compiles 13 best 17 moved 0  (n64/tools/n64permute.py) */
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
