/* Auto-generated from Ghidra decompilation — 0x100035E0 */
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
extern int DAT_1021c810;
extern char s_CHK_FClose____error_closing_file_1007b19c[];
extern char s_CHK_FClose__s__1007b1c4[];



/* @implements 0x100035E0 glide BrChkFClose */
void BrChkFClose(int *param_1)

{
  int iVar1;
  char local_400 [1024];
  
  if (DAT_1021c810 != 0) {
    sprintf(local_400,s_CHK_FClose__s__1007b1c4,param_1[1]);
    OutputDebugStringA(local_400);
  }
  iVar1 = fclose((FILE *)*param_1);
  if (iVar1 == -1) {
    sprintf(local_400,s_CHK_FClose____error_closing_file_1007b19c,param_1[1]);
    OutputDebugStringA(local_400);
                    
    exit(1);
  }
  free((void *)param_1[1]);
  free(param_1);
  return;
}


#endif /* BR_MATCHING_BUILD */
