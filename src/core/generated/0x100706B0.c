/* Auto-generated from Ghidra decompilation — 0x100706B0 */
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
extern int *DAT_118eeeec;
typedef int (__stdcall *CC_std_1)(int);



/* @implements 0x100706B0 glide BrDiAcquire */
int BrDiAcquire(void)

{
  int iVar1;
  
  if (DAT_118eeeec != (int *)0x0) {
    iVar1 = (*(CC_std_1 *)(*(int *)(DAT_118eeeec) + 28))(DAT_118eeeec);
    return (iVar1 >= 0);
  }
  return false;
}


#endif /* BR_MATCHING_BUILD */
