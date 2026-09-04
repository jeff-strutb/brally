/* Auto-generated from Ghidra decompilation — 0x100064D0 */
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
extern int DAT_1021c81c;
extern int DAT_10226a30;
extern int DAT_10226a44;
int BrTicks30FromMs();



/* WHAT IT DOES: check whether a network timeout has expired and, if so, set
 * the flag that tells the rest of the game to give up waiting. Called from
 * the polling loop. */
/* @implements 0x100064D0 glide BrNetCheckDeadline */
void BrNetCheckDeadline(void)

{
  unsigned int uVar1;
  
  WaitForSingleObject(DAT_1021c81c,0xffffffff);
  uVar1 = BrTicks30FromMs();
  if (uVar1 >= DAT_10226a30) {
    DAT_10226a44 = 1;
  }
  ReleaseMutex(DAT_1021c81c);
  return;
}


#endif /* BR_MATCHING_BUILD */
