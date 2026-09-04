/* Auto-generated from Ghidra decompilation — 0x100061E0 */
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
extern char DAT_1021d3c8;
extern char DAT_10226628[];



/* WHAT IT DOES: copy a network player's name out of its slot into one SHARED
 * static buffer and return that buffer. Not re-entrant and not safe to hold:
 * the next caller overwrites it. That is the original's design. */
/* @implements 0x100061E0 glide BrNetSlotName */
int * BrNetSlotName(int param_1)

{
  char cVar1;
  unsigned int uVar2;
  unsigned int uVar3;
  char *pcVar4;
  char *pcVar5;
  
  WaitForSingleObject((HANDLE)(&DAT_1021ce58)[param_1 * 0x25e],0xffffffff);
  strcpy(DAT_10226628, &DAT_1021d3c8 + param_1 * 0x978);
  ReleaseMutex((HANDLE)(&DAT_1021ce58)[param_1 * 0x25e]);
  return DAT_10226628;
}


#endif /* BR_MATCHING_BUILD */
