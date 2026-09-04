/* Auto-generated from Ghidra decompilation — 0x10006400 */
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
extern int DAT_1021c900;
extern int DAT_1021ce4c;



/* WHAT IT DOES: raise a shared network flag under its mutex. Paired with
 * BrNetClearF10220DD0. */
/* @implements 0x10006400 glide BrNetSetF10220DD0 */
void BrNetSetF10220DD0(void)

{
  WaitForSingleObject(DAT_1021ce4c,0xffffffff);
  DAT_1021c900 = 1;
  ReleaseMutex(DAT_1021ce4c);
  return;
}


#endif /* BR_MATCHING_BUILD */
