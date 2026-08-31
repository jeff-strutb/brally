/* Auto-generated from Ghidra decompilation — 0x10004D80 */
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
extern int DAT_1021ce58;
extern int DAT_1021ce84;



/* @implements 0x10004D80 glide BrNetSlotGetF02C */
int BrNetSlotGetF02C(int param_1)

{
  int uVar1;
  
  WaitForSingleObject((HANDLE)(&DAT_1021ce58)[param_1 * 0x25e],0xffffffff);
  uVar1 = (&DAT_1021ce84)[param_1 * 0x25e];
  ReleaseMutex((HANDLE)(&DAT_1021ce58)[param_1 * 0x25e]);
  return uVar1;
}


#endif /* BR_MATCHING_BUILD */
