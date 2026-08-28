/* Auto-generated from Ghidra decompilation — 0x10033C90 */
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
extern int DAT_10ac0c38;
extern int DAT_10ac0c3c;
extern int DAT_10ac0c40;
extern int DAT_10ac0c44;
extern int DAT_10ac0c48;



/* @implements 0x10033C90 glide BrPfxSaveState */
void BrPfxSaveState(short *param_1)

{
  int iVar1;
  int *puVar2;
  int *puVar3;
  
  *param_1 = (short)DAT_10ac0c38;
  param_1[1] = DAT_10ac0c40;
  param_1[2] = (short)DAT_10ac0c3c;
  param_1[3] = (short)DAT_10ac0c44;
  memcpy((int *)(param_1 + 4), &DAT_10ac0c48, 8192);
  return;
}


#endif /* BR_MATCHING_BUILD */
