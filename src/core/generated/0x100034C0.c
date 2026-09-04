/* Auto-generated from Ghidra decompilation — 0x100034C0 */
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
extern char s_CHK_FRead____trying_to_read__u_b_1007b168[];
int BrFChkFRead();



/* WHAT IT DOES: read from a file and abort the whole game if the read comes
 * up short, printing how many bytes it wanted to the debugger. The engine
 * treats a short read as corrupt game data, so there is no recoverable case
 * and no return value to test. */
/* @implements 0x100034C0 glide BrChkFRead */
int BrChkFRead(int param_1,int param_2,int param_3,int param_4)

{
  int iVar1;
  char local_400 [1024];
  
  iVar1 = BrFChkFRead(param_1,param_2,param_3,param_4);
  if (iVar1 == 0) {
    sprintf(local_400,s_CHK_FRead____trying_to_read__u_b_1007b168,param_3 * param_2);
    OutputDebugStringA(local_400);
                    
    exit(1);
  }
  return param_1;
}


#endif /* BR_MATCHING_BUILD */
