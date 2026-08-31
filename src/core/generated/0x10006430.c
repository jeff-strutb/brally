/* Auto-generated from Ghidra decompilation — 0x10006430 */
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



/* @implements 0x10006430 glide BrNetClearF10220DD0 */
void BrNetClearF10220DD0(void)

{
  WaitForSingleObject(DAT_1021ce4c,0xffffffff);
  DAT_1021c900 = 0;
  ReleaseMutex(DAT_1021ce4c);
  return;
}


#endif /* BR_MATCHING_BUILD */
