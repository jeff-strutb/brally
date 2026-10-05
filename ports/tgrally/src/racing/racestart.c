/* racestart.c -- starting a race
 */
#include "tgr/common.h"

/* -- declarations -- */
extern int D_8028AAD0;
extern int D_8028AAD4;
extern float D_8028AAD8;
extern int D_8028AADC;
extern unsigned long long D_8028ADF8;
extern unsigned int D_8028AE00;
extern int D_8028AB10;
extern unsigned long long osClockRate;
void BrPadPollAll(void);
void BrStub80254870(void);
void BrStub80254F2C(void);
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


/* WHAT IT DOES: Advance the game clock once per frame: from the CPU
 * counter (or exactly 1/30 s in fixed-step mode), keep the millisecond time
 * and the frame's length in seconds, and poll the controllers. */
/* @implements 0x8021C4B4 tgr BrClockTick */
void BrClockTick(void)
{
  unsigned int now;
  unsigned int d;

  if (D_8028AB10 != 0) {
    D_8028ADF8 += 33333333ULL * osClockRate / 1000000000ULL;
    D_8028AE00 = osGetCount();
  } else if (D_8028ADF8 != 0) {
    now = osGetCount();
    D_8028ADF8 += now - D_8028AE00;
    D_8028AE00 = now;
  } else {
    D_8028ADF8 = D_8028AE00 = osGetCount();
  }
  D_8028AAD0 = D_8028AAD4;
  D_8028AAD4 = D_8028ADF8 * 1000000 / osClockRate / 1000;
  BrPadPollAll();
  BrStub80254870();
  BrStub80254F2C();
  d = D_8028AAD4 - D_8028AAD0;
  D_8028AAD8 = d / 1000.0f;
  D_8028AADC = d;
}

/* WHAT IT DOES: Does nothing. An empty function at the end of the race-clock
 * object: the game-mode object after it starts on the next 16-byte line, and
 * its main loop's padding places that start at 0x8021C6B0. */
/* @implements 0x8021C6A8 tgr BrStub8021C6A8 */
void BrStub8021C6A8(void)
{
}
