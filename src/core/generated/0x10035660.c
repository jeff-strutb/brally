/* Auto-generated from Ghidra decompilation — 0x10035660 */
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
int FUN_10005f50();
extern int *DAT_10ac3068;
extern int DAT_10ac4094;
extern char s_DirectPlay_interface_final_insta_100aa514[];
int BrGlobalFreeAll();



typedef int (__stdcall *CC_std_1)(int);

/* @implements 0x10035660 glide FUN_10035660 */
void FUN_10035660(void)

{
  char local_104 [260];
  
  FUN_10005f50(1);
  BrGlobalFreeAll();
  sprintf(local_104,s_DirectPlay_interface_final_insta_100aa514,DAT_10ac4094);
  OutputDebugStringA(local_104);
  if (DAT_10ac3068 != (int *)0x0) {
    (*(CC_std_1 *)(*(int *)(DAT_10ac3068) + 8))(DAT_10ac3068);
  }
  return;
}


#endif /* BR_MATCHING_BUILD */
