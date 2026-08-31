/* Auto-generated from Ghidra decompilation — 0x10071EB0 */
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
extern int *DAT_118eeee8;
extern int DAT_118eeef0;
typedef int (__stdcall *CC_std_1)(int);



/* @implements 0x10071EB0 glide BrDiKeyboardShutdown */
void BrDiKeyboardShutdown(void)

{
  DAT_118eeef0 = DAT_118eeef0 + -1;
  if (DAT_118eeef0 < 0) {
    DAT_118eeef0 = 0;
    return;
  }
  if ((DAT_118eeef0 == 0) && (DAT_118eeee8 != (int *)0x0)) {
    (*(CC_std_1 *)(*(int *)(DAT_118eeee8) + 32))(DAT_118eeee8);
    (*(CC_std_1 *)(*(int *)(DAT_118eeee8) + 8))(DAT_118eeee8);
    DAT_118eeee8 = (int *)0x0;
  }
  return;
}


#endif /* BR_MATCHING_BUILD */
