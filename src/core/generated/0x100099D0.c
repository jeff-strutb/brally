/* Auto-generated from Ghidra decompilation — 0x100099D0 */
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
extern int *g_brPA9D008;
typedef int (__stdcall *CC_std_2)(int, int);



/* WHAT IT DOES: release the COM interface a holder object is keeping alive,
 * if it is still holding one, and forget it. Returns the reference count the
 * release reported, or zero if there was nothing to release. */
/* @implements 0x100099D0 glide BrComHolderRelease */
int BrComHolderRelease(void)

{
  int *piVar1;
  int uVar2;
  
  uVar2 = 0;
  if (((g_brPA9D008 != (int *)0x0) && (piVar1 = (int *)*g_brPA9D008, piVar1 != (int *)0x0)) &&
     (g_brPA9D008[2] != 0)) {
    uVar2 = (*(CC_std_2 *)(*(int *)(piVar1) + 36))(piVar1,g_brPA9D008[2]);
    g_brPA9D008[2] = 0;
  }
  return uVar2;
}


#endif /* BR_MATCHING_BUILD */
