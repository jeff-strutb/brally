/* Auto-generated from Ghidra decompilation — 0x1006E3F0 */
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
extern int g_br86HasPerf_S_S1437;
int BrSub10075020();
extern int g_br86HasPerf_S_S1437;



/* WHAT IT DOES: restart a stopwatch from now. Uses the high-resolution
 * performance counter when the machine has one and a coarser millisecond
 * clock when it does not, writing to a different pair of fields in each case
 * -- so the two halves of this record are alternatives, not both live. */
/* @implements 0x1006E3F0 glide br86_timer_restart */
void __fastcall br86_timer_restart(int *param_1)

{
  int uVar1;
  
  if (g_br86HasPerf_S_S1437 != 0) {
    QueryPerformanceCounter((LARGE_INTEGER *)(param_1 + 2));
    param_1[4] = *param_1;
    param_1[5] = param_1[1];
    return;
  }
  uVar1 = BrSub10075020();
  param_1[7] = uVar1;
  param_1[8] = param_1[6];
  return;
}


#endif /* BR_MATCHING_BUILD */
