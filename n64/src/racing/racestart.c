/* racestart.c -- starting a race
 */
#include "tgr/common.h"

/* -- declarations -- */
extern int D_8028AAD0;
extern int D_8028AAD4;
extern float D_8028AAD8;
extern int D_8028AADC;
extern int D_8028ADF8;
extern int D_8028ADFC;
extern int D_8028AE00;
/* -- end declarations -- */

/* WHAT IT DOES: Reset the race clocks and frame timers to zero when a race
 * starts. */
/* @implements 0x8021C46C tgr BrRaceClockReset */
void BrRaceClockReset(void)
{
  D_8028ADFC = 0;
  D_8028ADF8 = 0;
  D_8028AE00 = 0;
  D_8028AAD4 = 0;
  D_8028AAD0 = 0;
  D_8028AADC = 0;
  D_8028AAD8 = 0;
}
