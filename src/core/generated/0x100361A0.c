/* Auto-generated from Ghidra decompilation — 0x100361A0 */
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
extern int DAT_10ac5898;
extern int *g_brP277B40;
extern int g_brPA9D008;
extern int g_brPAA29E4;
int BrSlotsFindById();
int BrSlotsResetIfBZero();
int BrSub1003C9B0();
int BrSub1003D950();



typedef int (__stdcall *CC_std_5)(int, int, int, int, int);

/* WHAT IT DOES: join an existing multiplayer session: leaves any current
 * one, clears the local player table, and passes the request to DirectPlay.
 * Returns the DirectPlay result, or a generic failure code when no
 * DirectPlay object exists. */
/* @implements 0x100361A0 glide FUN_100361a0 */
int FUN_100361a0(int param_1,int param_2,int param_3,int param_4)

{
  int *puVar1;
  int uVar2;
  int uVar3;
  
  uVar3 = 0x80004005;
  if (g_brPAA29E4 != 0) {
    BrSub1003C9B0();
  }
  puVar1 = &DAT_10ac5898;
  do {
    *puVar1 = 0;
    puVar1 = puVar1 + 3;
  } while ((int)puVar1 < 0x10ac58f8);
  if (g_brP277B40 != (int *)0x0) {
    uVar3 = (*(CC_std_5 *)(*(int *)(g_brP277B40) + 48))(g_brP277B40,param_1,param_2,param_3,param_4);
  }
  BrSlotsResetIfBZero();
  uVar2 = BrSlotsFindById(*(int *)(g_brPA9D008 + 8));
  BrSub1003D950(g_brPA9D008,uVar2);
  return uVar3;
}


#endif /* BR_MATCHING_BUILD */
