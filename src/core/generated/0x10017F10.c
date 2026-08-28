/* Auto-generated from Ghidra decompilation — 0x10017F10 */
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
extern funcptr DAT_104b161c;
extern int DAT_104b1688;



/* @implements 0x10017F10 glide BrFadeRelease */
int BrFadeRelease(void)

{
  DAT_104b1688 = DAT_104b1688 + -1;
  if (DAT_104b1688 == 0) {
    (*DAT_104b161c)();
  }
  return 1;
}


#endif /* BR_MATCHING_BUILD */
