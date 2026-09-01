/* Auto-generated from Ghidra decompilation — 0x1006C6A0 */
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
int FUN_1006c460();
extern int DAT_1184c2a8;
extern int DAT_1184c2b0;
extern int *DAT_1184c344;
extern int BrSndG18290FC;
extern int *BrSndPDS;
typedef int (__stdcall *CC_std_1)(int);



/* @implements 0x1006C6A0 glide FUN_1006c6a0 */
int FUN_1006c6a0(void)

{
  HGLOBAL pvVar1;
  
  BrSndG18290FC = BrSndG18290FC + -1;
  if (BrSndG18290FC != 0) {
    return 1;
  }
  FUN_1006c460();
  if (DAT_1184c344 != (int *)0x0) {
    (*(CC_std_1 *)(*(int *)(DAT_1184c344) + 72))(DAT_1184c344);
    (*(CC_std_1 *)(*(int *)(DAT_1184c344) + 8))(DAT_1184c344);
    DAT_1184c344 = (int *)0x0;
  }
  if (BrSndPDS != (int *)0x0) {
    (*(CC_std_1 *)(*(int *)(BrSndPDS) + 8))(BrSndPDS);
    BrSndPDS = (int *)0x0;
  }
  if (DAT_1184c2b0 != (LPCVOID)0x0) {
    GlobalUnlock(GlobalHandle(DAT_1184c2b0));
    GlobalFree(GlobalHandle(DAT_1184c2b0));
    DAT_1184c2b0 = (LPCVOID)0x0;
  }
  if (DAT_1184c2a8 != (LPCVOID)0x0) {
    GlobalUnlock(GlobalHandle(DAT_1184c2a8));
    GlobalFree(GlobalHandle(DAT_1184c2a8));
    DAT_1184c2a8 = (LPCVOID)0x0;
  }
  return 1;
}


#endif /* BR_MATCHING_BUILD */
