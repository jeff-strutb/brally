/* Auto-generated from Ghidra decompilation — 0x100703D0 */
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
extern int DAT_10073ae0;
extern int DAT_10078718;
extern int DAT_118ee9cc;
extern int DAT_118ee9d0;
extern int DAT_118eebd0;
extern int DAT_118eebf0;
extern int DAT_118eebf8;
extern int *DAT_118eee88;
extern int DAT_118eee94;
extern int DAT_118eeef0;
extern int g_brP680584;
extern int *g_pBrDik18ABDD0;
int BrSub100770C0();
typedef int (__stdcall *CC_std_4)(int, int, int, int);
typedef int (__stdcall *CC_std_2)(int, int);
typedef int (__stdcall *CC_std_3)(int, int, int);
typedef int (__stdcall *CC_std_1)(int);



/* @implements 0x100703D0 glide FUN_100703d0 */
int FUN_100703d0(void)

{
  int iVar1;
  int *puVar2;
  
  DAT_118eeef0 = DAT_118eeef0 + 1;
  if (DAT_118eeef0 == 1) {
    DAT_118eebf0 = 1;
    DAT_118ee9cc = 0;
    puVar2 = &DAT_118ee9d0;
    for (iVar1 = 0x80; iVar1 != 0; iVar1 = iVar1 + -1) {
      *puVar2 = 0;
      puVar2 = puVar2 + 1;
    }
    puVar2 = &DAT_118eebf8;
    for (iVar1 = 0x88; iVar1 != 0; iVar1 = iVar1 + -1) {
      *puVar2 = 0;
      puVar2 = puVar2 + 1;
    }
    DAT_118eee94 = 0;
    DAT_118eebd0 = 1;
    BrSub100770C0();
    iVar1 = (*(CC_std_4 *)(*(int *)(DAT_118eee88) + 12))(DAT_118eee88,&DAT_10078718,&g_pBrDik18ABDD0,0);
    if (iVar1 < 0) {
      return 0;
    }
    iVar1 = (*(CC_std_2 *)(*(int *)(g_pBrDik18ABDD0) + 44))(g_pBrDik18ABDD0,&DAT_10073ae0);
    if (iVar1 < 0) {
      return 0;
    }
    iVar1 = (*(CC_std_3 *)(*(int *)(g_pBrDik18ABDD0) + 52))(g_pBrDik18ABDD0,g_brP680584,6);
    if (iVar1 < 0) {
      return 0;
    }
    if (g_pBrDik18ABDD0 != (int *)0x0) {
      (*(CC_std_1 *)(*(int *)(g_pBrDik18ABDD0) + 28))(g_pBrDik18ABDD0);
    }
  }
  return 1;
}


#endif /* BR_MATCHING_BUILD */
