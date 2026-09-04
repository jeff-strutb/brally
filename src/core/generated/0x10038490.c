/* Auto-generated from Ghidra decompilation — 0x10038490 */
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
extern int DAT_10ac5d00;



/* WHAT IT DOES: the flag-only member of that text-field family: it clears
 * the enable bit on its page when the field is non-empty and copies nothing.
 * Always reports success. */
/* @implements 0x10038490 glide BrUiFn1003EF60 */
int BrUiFn1003EF60(int param_1)

{
  char cVar1;
  int iVar2;
  char *pcVar3;
  
  if (strlen((char *)(param_1 + 0x2b65)) != 0) {
    *(unsigned int *)(DAT_10ac5d00 + 0x1c) = *(unsigned int *)(DAT_10ac5d00 + 0x1c) & 0xffffffef;
  }
  return 1;
}


#endif /* BR_MATCHING_BUILD */
