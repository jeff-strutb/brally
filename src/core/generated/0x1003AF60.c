/* Auto-generated from Ghidra decompilation — 0x1003AF60 */
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
extern char DAT_10396f08[];
extern char DAT_10ac4100[];
extern int DAT_10ac5d24;
extern int g_brAA28D8;
extern int g_i0AB3F4;



/* @implements 0x1003AF60 glide BrExt_10041A00 */
int BrExt_10041A00(int param_1)

{
  char cVar1;
  unsigned int uVar2;
  unsigned int uVar3;
  char *pcVar4;
  char *pcVar5;
  char *pcVar6;
  
  *(int *)(*(int *)(param_1 + 0x2ae8) + 0x70) = 0;
  *(unsigned int *)(DAT_10ac5d24 + 0x44c + g_i0AB3F4 * 0x438) =
       (unsigned int)(*(int *)(DAT_10ac5d24 + 0x44c + g_i0AB3F4 * 0x438) == 0);
  g_brAA28D8 = *(int *)(DAT_10ac5d24 + 0x44c + g_i0AB3F4 * 0x438);
  if (g_brAA28D8 != 0) {
    pcVar6 = (char *)(DAT_10ac5d24 + g_i0AB3F4 * 0x438 + 0x35);
    strcpy(DAT_10ac4100, pcVar6);
    strcpy(pcVar6, DAT_10396f08);
  }
  return 1;
}


#endif /* BR_MATCHING_BUILD */
