/* Auto-generated from Ghidra decompilation — 0x10003760 */
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
extern char s_CHK_ReAllocateMemory____Out_of_m_1007b224[];



/* @implements 0x10003760 glide BrChkRealloc */
void * BrChkRealloc(void *param_1,unsigned int param_2,int param_3)

{
  void *pvVar1;
  char local_400 [1024];
  
  pvVar1 = realloc(param_1,param_2);
  if (param_2 == 0) {
    return (void *)0x0;
  }
  if (pvVar1 == (void *)0x0) {
    sprintf(local_400,s_CHK_ReAllocateMemory____Out_of_m_1007b224,param_3);
    OutputDebugStringA(local_400);
                    
    exit(1);
  }
  return pvVar1;
}


#endif /* BR_MATCHING_BUILD */
