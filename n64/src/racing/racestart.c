/* racestart.c -- starting a race
 */
#include "tgr/common.h"

/* -- declarations -- */
extern int D_8028AAD0;
extern int D_8028AAD4;
extern float D_8028AAD8;
extern int D_8028AADC;
unsigned long long D_8028ADF8 = 0;
extern int D_8028AE00;
/* -- end declarations -- */

/* WHAT IT DOES: Reset the race clocks and frame timers to zero when a race
 * starts. */
/* @implements 0x8021C46C tgr BrRaceClockReset */
void BrRaceClockReset(void)
{
  D_8028ADF8 = 0;
  D_8028AE00 = 0;
  D_8028AAD0 = D_8028AAD4 = 0;
  D_8028AADC = 0;
  D_8028AAD8 = 0.0f;
}


/* WHAT IT DOES: Does nothing. An empty function at the end of the race-clock
 * object: the game-mode object after it starts on the next 16-byte line, and
 * its main loop's padding places that start at 0x8021C6B0. */
/* @implements 0x8021C6A8 tgr BrStub8021C6A8 */
void BrStub8021C6A8(void)
{
}
