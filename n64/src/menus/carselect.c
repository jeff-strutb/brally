/* carselect.c -- the car-select screen
 */
#include "tgr/common.h"

/* -- declarations -- */
int BrCarModelPresent(int param_1);
extern int D_8020C688;
extern int D_8020C68C;
extern int D_80272070;
extern char *D_8031C5BC;
extern char D_8028AE24;
/* -- end declarations -- */

/* WHAT IT DOES: Tell whether car n may be picked on the car-select screen:
 * cars 9 and up only while the flag at 0x80272070 is clear, the car's bit
 * must be set in the season record's unlock mask, and its model must be
 * present. */
/* @implements 0x8020C66C tgr BrCarSelectable */
int BrCarSelectable(int n)
{
  return (D_80272070 == 0 || n < 9) && (*(unsigned short *)(D_8031C5BC + 0xcc) & (1 << n)) &&
         BrCarModelPresent(n);
}

/* WHAT IT DOES: Tell whether car n has a model record loaded (its entry in
 * the car model table is non-zero). */
/* @implements 0x8021D6B8 tgr BrCarModelPresent */
int BrCarModelPresent(int param_1)
{
  return *(int *)(&D_8028AE24 + param_1 * 0x60) != 0;
}
