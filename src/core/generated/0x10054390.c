/* Auto-generated from Ghidra decompilation — 0x10054390 */
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
typedef int (__fastcall *VT1)(void *this);

/* Forward declarations for unknown functions/globals */
extern char DAT_100acb44[];
extern int DAT_10ac5bb4;
extern int DAT_10ac5de4;
extern int DAT_10ac5e50;
extern int DAT_10ac5ecc;
extern int DAT_10ac5ed0;
extern int DAT_10ac5ed4;
extern int DAT_10ac5ed8;
extern int DAT_10ac6050;
extern int g_brAA2854;
extern int g_brAA33E4;
extern int g_brPA9D008;
char FUN_10054360(int key);
int FUN_10037720(void);
int FUN_10037040(int a, int b);
int FUN_1006ba60(int a, int b);



/* WHAT IT DOES: pick which of several alternative actions a control
 * triggers, by testing the mode flags in order and dispatching the first one
 * that is set -- sending the matching network message and recording which
 * was chosen. A chain of mutually exclusive menu buttons sharing one
 * handler. */
/* @implements 0x10054390 glide FUN_10054390 */
char __fastcall FUN_10054390(int *param_1)

{
  char cVar1;
  
  if (DAT_10ac5bb4 != 0) {
    if (DAT_10ac5ecc != 0) {
      FUN_10037040(g_brPA9D008, 4);
      FUN_1006ba60(4, 0x200020);
      g_brAA2854 = 4;
    }
    else if (DAT_10ac5ed0 != 0) {
      FUN_10037040(g_brPA9D008, 5);
      FUN_1006ba60(5, 0x200020);
      g_brAA2854 = 5;
    }
    else if (DAT_10ac5ed4 != 0) {
      FUN_10037040(g_brPA9D008, 6);
      FUN_1006ba60(6, 0x200020);
      g_brAA2854 = 6;
    }
    else if (DAT_10ac5ed8 != 0) {
      FUN_10037040(g_brPA9D008, 7);
      FUN_1006ba60(7, 0x200020);
      g_brAA2854 = 7;
    }
  }
  if (DAT_10ac5de4 != 0) {
    g_brAA33E4 = 0;
    return (char)0xff;
  }
  if ((DAT_10ac5e50 != 0) || (DAT_10ac6050 != 0) ||
      (FUN_10037720() != 0 && DAT_10ac5bb4 == 0)) {
    if (strlen((char *)param_1 + 9) != 0) {
      g_brAA33E4 = 0;
      return 0;
    }
  }
  if (g_brAA33E4 != 0) {
    if (g_brAA33E4 == 8) {
      if (strlen((char *)param_1 + 9) != 0) {
        ((char *)param_1)[8 + strlen((char *)param_1 + 9)] = 0;
        g_brAA33E4 = 0;
        return 1;
      }
    }
    else {
      cVar1 = FUN_10054360(g_brAA33E4);
      if (cVar1 == 0) {
        return 1;
      }
      (*(VT1 *)(*param_1 + 4))(param_1);
      if (*(short *)((int)param_1 + 0x40a) < *(short *)((int)param_1 + 0x41c)) {
        sprintf((char *)param_1 + 9, DAT_100acb44, (char *)param_1 + 9, (int)cVar1);
      }
    }
  }
  g_brAA33E4 = 0;
  return 1;
}


#endif /* BR_MATCHING_BUILD */
