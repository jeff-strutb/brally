/* Auto-generated from Ghidra decompilation — 0x100418C0 */
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
int FUN_100776c0();
extern funcptr PTR_FUN_100776c0;



/* @implements 0x100418C0 glide BrUiPageCtor_10048470 */
int * __fastcall BrUiPageCtor_10048470(int *param_1)

{
  int iVar1;
  int *puVar2;
  
  param_1[4] = 0;
  *(short *)(param_1 + 5) = 0;
  param_1[0xce] = 0;
  param_1[0xcf] = 0;
  *param_1 = &PTR_FUN_100776c0;
  param_1[1] = 0;
  param_1[2] = 0;
  param_1[3] = 0;
  memset(param_1 + 6, 0, 800);
  param_1[0xd0] = 0;
  *(short *)(param_1 + 0xd1) = 0;
  *(short *)((int)param_1 + 0x346) = 0;
  return param_1;
}


#endif /* BR_MATCHING_BUILD */
