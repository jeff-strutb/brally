/* Auto-generated from Ghidra decompilation — 0x100385F0 */
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
extern char DAT_10b71ac0[];
int Br85ItemApply();



/* WHAT IT DOES: text-field commit for the setting at 0x10B71AC0, otherwise
 * identical to Br85TextReadBack. */
/* @implements 0x100385F0 glide BrUiHook85_1003F0B0 */
int BrUiHook85_1003F0B0(int param_1)

{
  char cVar1;
  int iVar2;
  unsigned int uVar3;
  unsigned int uVar4;
  char *pcVar5;
  char *pcVar6;
  
  Br85ItemApply(param_1,0);
  iVar2 = _stricmp(DAT_10b71ac0,(char *)(param_1 + 0x2b65));
  if (iVar2 != 0) {
    strcpy(DAT_10b71ac0, (char *)(param_1 + 0x2b65));
  }
  return 1;
}


#endif /* BR_MATCHING_BUILD */
