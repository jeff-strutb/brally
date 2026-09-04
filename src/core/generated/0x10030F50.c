/* Auto-generated from Ghidra decompilation — 0x10030F50 */
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
extern char s_File__s_missing_100aa318[];
int BrChkFClose();
int BrChkFRead();
int BrChkFReadOpen();
int BrChkFileExists();
int BrChkFileSize();
int BrLogPrint();



/* WHAT IT DOES: read a whole file into a caller-supplied buffer: warns to
 * the log if it is missing, opens it, reads either the requested number of
 * bytes or the entire file when a negative length is passed, and closes it.
 * Every step uses the abort-on-failure file helpers, so a real failure ends
 * the game rather than returning. */
/* @implements 0x10030F50 glide BrFileReadInto */
void BrFileReadInto(int param_1,int param_2,int param_3)

{
  int iVar1;
  int uVar2;
  char local_200 [512];
  
  iVar1 = BrChkFileExists(param_2);
  if (iVar1 == 0) {
    sprintf(local_200,s_File__s_missing_100aa318,param_2);
    BrLogPrint(local_200);
  }
  uVar2 = BrChkFReadOpen(param_2);
  if (param_3 < 0) {
    param_3 = BrChkFileSize(uVar2);
  }
  BrChkFRead(param_1,1,param_3,uVar2);
  BrChkFClose(uVar2);
  return;
}


#endif /* BR_MATCHING_BUILD */
