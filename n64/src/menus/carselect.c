/* carselect.c -- the car-select screen
 */
#include "tgr/common.h"

/* -- declarations -- */
int BrCarModelPresent();
extern int D_8020C688;
extern int D_8020C68C;
extern int D_80272070;
extern int D_8031C5BC;
extern int D_8028AE24;
/* -- end declarations -- */

/* WHAT IT DOES: Tell whether car n may be picked on the car-select screen:
 * it must be unlocked in the season record, the bonus cars (9 and up) only
 * when bonus cars are on, and its model must be present. */
/* @implements 0x8020C66C tgr BrCarSelectable */
int BrCarSelectable(unsigned int param_1)
{
  int iVar1;
  int bVar2;
  
  if (((D_80272070 == 0) || (bVar2 = 0, (int)param_1 < 9)) &&
     (bVar2 = 0, ((unsigned int)*(unsigned short *)(D_8031C5BC + 0xcc) & 1 << (param_1 & 0x1f)) != 0)) {
    iVar1 = BrCarModelPresent();
    bVar2 = iVar1 != 0;
  }
  return bVar2;
}

/* WHAT IT DOES: Tell whether car n has a model record loaded (its entry in
 * the car model table is non-zero). */
/* @implements 0x8021D6B8 tgr BrCarModelPresent */
int BrCarModelPresent(int param_1)
{
  return *(int *)(&D_8028AE24 + param_1 * 0x60) != 0;
}
