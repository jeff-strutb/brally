/* Auto-generated from Ghidra decompilation — 0x10023900 */
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
extern float DAT_105d1760;



/* WHAT IT DOES: handle the display-list command that loads a matrix, by
 * copying its 64 bytes from wherever the list points into the renderer's
 * current-matrix slot. Returns the pointer to the next command. */
/* @implements 0x10023900 glide BrGbiMoveMemMatrix */
int BrGbiMoveMemMatrix(int param_1)

{
  int iVar1;
  int *puVar2;
  int *puVar3;
  
  memcpy(&DAT_105d1760, *(int **)(param_1 + 4), 64);
  return param_1 + 8;
}


#endif /* BR_MATCHING_BUILD */
