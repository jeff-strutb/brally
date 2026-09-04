/* Auto-generated from Ghidra decompilation — 0x100061B0 */
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
extern int DAT_1021ce00;
extern int DAT_10226a58;



/* WHAT IT DOES: read one entry from a shared network table under the table's
 * own mutex. */
/* @implements 0x100061B0 glide BrNetGetA102212D0 */
int BrNetGetA102212D0(int param_1)

{
  int uVar1;
  
  WaitForSingleObject(DAT_10226a58,0xffffffff);
  uVar1 = (&DAT_1021ce00)[param_1];
  ReleaseMutex(DAT_10226a58);
  return uVar1;
}


#endif /* BR_MATCHING_BUILD */
