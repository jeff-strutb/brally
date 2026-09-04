/* Auto-generated from Ghidra decompilation — 0x100358C0 */
#ifdef BR_MATCHING_BUILD

/* The original binary is /MD: CRT calls resolve through the import table. */
#define _CRTIMP __declspec(dllimport)
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <mmsystem.h>

#ifndef true
#define true 1
#define false 0
#endif
#ifndef NAN
unsigned long _ghidra_nan_bits = 0x7FC00000;
#define NAN (*(float*)&_ghidra_nan_bits)
#endif

typedef int (*funcptr)();

/* Forward declarations for unknown functions/globals */
int FUN_100356b0();
extern int DAT_10ac306c;
extern int DAT_10ac408c;
extern int g_brP680584;



/* WHAT IT DOES: start the once-a-second Windows timer that drives
 * multiplayer housekeeping, and flag that it is running. Always reports
 * success. */
/* @implements 0x100358C0 glide BrTimerStart1003C230 */
int BrTimerStart1003C230(void)

{
  FUN_100356b0();
  DAT_10ac306c = SetTimer(g_brP680584,1,1000,(TIMERPROC)0x0);
  DAT_10ac408c = 1;
  return 1;
}


#endif /* BR_MATCHING_BUILD */
