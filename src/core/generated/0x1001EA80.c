/* Auto-generated from Ghidra decompilation — 0x1001EA80 */
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
extern float DAT_105ce2d0;
extern float DAT_105d17a4;
extern float DAT_105d17b4;
extern float _DAT_105cd9f0;
void __stdcall grConstantColorValue(int);





/* @implements 0x1001EA80 glide br_dl_prim */
int br_dl_prim(int param_1)

{
  DAT_105d17a4 = (float)(*(unsigned int *)(param_1 + 4) >> 0x18);
  DAT_105d17b4 = (float)(*(unsigned int *)(param_1 + 4) >> 0x10 & 0xff);
  DAT_105ce2d0 = (float)(*(unsigned int *)(param_1 + 4) >> 8 & 0xff);
  _DAT_105cd9f0 = (float)(*(unsigned int *)(param_1 + 4) & 0xff);
  grConstantColorValue(*(int *)(param_1 + 4));
  return param_1 + 8;
}


#endif /* BR_MATCHING_BUILD */
