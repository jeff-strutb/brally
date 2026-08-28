/* Auto-generated from Ghidra decompilation — 0x10038580 */
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
extern char DAT_10b71aa0[];
int Br85ItemApply();



/* @implements 0x10038580 glide Br85TextReadBack */
int Br85TextReadBack(int param_1)

{
  char cVar1;
  int iVar2;
  unsigned int uVar3;
  unsigned int uVar4;
  char *pcVar5;
  char *pcVar6;
  
  Br85ItemApply(param_1,0);
  iVar2 = _stricmp(DAT_10b71aa0,(char *)(param_1 + 0x2b65));
  if (iVar2 != 0) {
    strcpy(DAT_10b71aa0, (char *)(param_1 + 0x2b65));
  }
  return 1;
}


#endif /* BR_MATCHING_BUILD */
