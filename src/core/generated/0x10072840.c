/* Auto-generated from Ghidra decompilation — 0x10072840 */
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
extern int *DAT_118eef04;
extern int *DAT_118eef14;
extern int DAT_118eef18;
typedef int (__stdcall *CC_std_1)(int);



/* @implements 0x10072840 glide BrExt_10079550 */
void BrExt_10079550(void)

{
  DAT_118eef18 = DAT_118eef18 + -1;
  if (DAT_118eef18 < 0) {
    DAT_118eef18 = 0;
    return;
  }
  if (DAT_118eef18 == 0) {
    if (DAT_118eef14 != (int *)0x0) {
      (*(CC_std_1 *)(*(int *)(DAT_118eef14) + 8))(DAT_118eef14);
      DAT_118eef14 = (int *)0x0;
    }
    if (DAT_118eef04 != (int *)0x0) {
      (*(CC_std_1 *)(*(int *)(DAT_118eef04) + 8))(DAT_118eef04);
      DAT_118eef04 = (int *)0x0;
    }
    if (DAT_118eeeec != (int *)0x0) {
      (*(CC_std_1 *)(*(int *)(DAT_118eeeec) + 32))(DAT_118eeeec);
      (*(CC_std_1 *)(*(int *)(DAT_118eeeec) + 8))(DAT_118eeeec);
      DAT_118eeeec = (int *)0x0;
    }
  }
  return;
}


#endif /* BR_MATCHING_BUILD */
