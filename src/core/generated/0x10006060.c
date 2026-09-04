/* Auto-generated from Ghidra decompilation — 0x10006060 */
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
extern char DAT_1021ce5c;



/* WHAT IT DOES: read a different field out of a network player slot, under
 * the same per-slot mutex. */
/* @implements 0x10006060 glide BrNetSlotGetF004 */
int BrNetSlotGetF004(int param_1)

{
  int uVar1;
  
  WaitForSingleObject((HANDLE)(&DAT_1021ce58)[param_1 * 0x25e],0xffffffff);
  uVar1 = *(int *)(&DAT_1021ce5c + param_1 * 0x978);
  ReleaseMutex((HANDLE)(&DAT_1021ce58)[param_1 * 0x25e]);
  return uVar1;
}


#endif /* BR_MATCHING_BUILD */
