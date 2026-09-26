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
/* @t4-pass 0x8021C46C 1 2026-09-26 compiles 59 best 15 moved 1  (n64/tools/n64permute.py) */
/* @t4-pass 0x8021C46C 2 2026-09-26 compiles 16 best 15 moved 1  (n64/tools/n64permute.py) */
/* @t4-pass 0x8021C46C 3 2026-09-26 compiles 15 best 15 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x8021C46C tgr BrRaceClockReset */
void BrRaceClockReset(void)
{
  D_8028ADFC = 0;
  D_8028ADF8 = 0;
  D_8028AAD4 = 0;
  D_8028AE00 = 0;
  D_8028AAD0 = 0;
  D_8028AADC = 0;
  D_8028AAD8 = 0;
}
