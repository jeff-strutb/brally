/* Auto-generated from Ghidra decompilation — 0x100355F0 */
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
int FUN_10035be0();
extern int DAT_10226a48;
extern int DAT_10ac306c;
extern int DAT_10ac408c;
extern int DAT_10ac5d30;
extern int g_brAA287C;
extern int g_brAA2884;
extern int g_brAA2888;
extern int g_brP680584;
int BrSndThreadStop();
int BrSub100586A0();



/* WHAT IT DOES: shut a multiplayer session down: stops the periodic timer,
 * stops the sound thread if it was started, tears the session down, and
 * clears the flags that say a session is live. Leaves the local car record
 * alone in the two modes where it is still needed. */
/* @implements 0x100355F0 glide BrExt_1003BF60 */
void BrExt_1003BF60(void)

{
  BrSub100586A0();
  KillTimer(g_brP680584,DAT_10ac306c);
  if (g_brAA2884 != 0) {
    BrSndThreadStop();
  }
  FUN_10035be0();
  if (((g_brAA287C != 2) && (g_brAA287C != 3)) && (DAT_10ac5d30 != 0)) {
    *(char *)(DAT_10ac5d30 + 0x2b64) = 0;
    *(unsigned int *)(DAT_10ac5d30 + 0x1c) = *(unsigned int *)(DAT_10ac5d30 + 0x1c) & 0xffffffef;
  }
  DAT_10ac408c = 0;
  g_brAA2884 = 0;
  DAT_10226a48 = 0;
  g_brAA2888 = 0;
  return;
}


#endif /* BR_MATCHING_BUILD */
