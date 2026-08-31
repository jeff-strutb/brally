/* Auto-generated from Ghidra decompilation — 0x10006300 */
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
extern int DAT_1021d7cc;



/* @implements 0x10006300 glide BrNetSlotGetF974 */
int BrNetSlotGetF974(int param_1)

{
  int iVar1;
  
  WaitForSingleObject((HANDLE)(&DAT_1021ce58)[param_1 * 0x25e],0xffffffff);
  iVar1 = (&DAT_1021d7cc)[param_1 * 0x25e];
  ReleaseMutex((HANDLE)(&DAT_1021ce58)[param_1 * 0x25e]);
  if (iVar1 < 0) {
    iVar1 = 0;
  }
  return iVar1;
}


#endif /* BR_MATCHING_BUILD */
