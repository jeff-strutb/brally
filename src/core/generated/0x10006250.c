/* Auto-generated from Ghidra decompilation — 0x10006250 */
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



/* WHAT IT DOES: write a network player's name into its slot, under that
 * slot's mutex. The read side is BrNetSlotName. */
/* @implements 0x10006250 glide BrNetSlotSetName */
void BrNetSlotSetName(int param_1,char *param_2)

{
  char cVar1;
  unsigned int uVar2;
  unsigned int uVar3;
  char *pcVar4;
  char *pcVar5;
  
  WaitForSingleObject((HANDLE)(&DAT_1021ce58)[param_1 * 0x25e],0xffffffff);
  strcpy(&DAT_1021d3c8 + param_1 * 0x978, param_2);
  ReleaseMutex((HANDLE)(&DAT_1021ce58)[param_1 * 0x25e]);
  return;
}


#endif /* BR_MATCHING_BUILD */
