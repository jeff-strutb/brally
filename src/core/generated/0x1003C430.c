/* Auto-generated from Ghidra decompilation — 0x1003C430 */
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
int FUN_100368a0();
int FUN_100387f0();
extern int DAT_100abaa8[];
extern int DAT_100abc78;
extern int DAT_100abde8;
extern char DAT_10396f08[];
extern char DAT_10ac4db0[];
extern int DAT_10ac5c54;
extern int DAT_10ac6730;
extern int DAT_10ac6734;
extern char *PTR_PTR_100bcab0[];
extern int g_Br0B380C;
extern int g_brP277B40;
extern int g_brP680584;
extern int g_brPA9D008;
int BrStrGet(int id);



/* WHAT IT DOES: step the menu highlight to the next selectable entry,
 * wrapping round at the end of the page and skipping anything not currently
 * selectable. The extra three entries when a mode flag is set are the rows
 * that only exist in that mode. */
/* @implements 0x1003C430 glide FUN_1003c430 */
int FUN_1003c430(void)

{
  int iVar2;
  char *pcVar4;
  int iVar6;
  int k;
  
  if (DAT_10ac6734 != 0) {
    DAT_100abde8 = DAT_100abde8 + 1;
    k = (DAT_10ac5c54 ? 3 : 0) + 0xb;
    if (DAT_100abde8 > k) {
      DAT_100abde8 = 0;
    }
    iVar6 = DAT_100abde8;
    iVar2 = FUN_100387f0(DAT_100abde8);
    while (iVar2 == 0) {
      DAT_100abde8 = DAT_100abde8 + 1;
      k = (DAT_10ac5c54 ? 3 : 0) + 0xb;
      if (DAT_100abde8 > k) {
        DAT_100abde8 = 0;
      }
      else if (DAT_100abde8 == iVar6) break;
      iVar2 = FUN_100387f0(DAT_100abde8);
    }
  }
  else if (DAT_10ac6730 != 0) {
    DAT_100abde8 = DAT_100abde8 - 1;
    if (DAT_100abde8 < 0) {
      DAT_100abde8 = (DAT_10ac5c54 ? 3 : 0) + 0xb;
    }
    iVar6 = DAT_100abde8;
    iVar2 = FUN_100387f0(DAT_100abde8);
    while (iVar2 == 0) {
      DAT_100abde8 = DAT_100abde8 - 1;
      if (DAT_100abde8 < 0) {
        DAT_100abde8 = (DAT_10ac5c54 ? 3 : 0) + 0xb;
      }
      else if (DAT_100abde8 == iVar6) break;
      iVar2 = FUN_100387f0(DAT_100abde8);
    }
  }
  g_Br0B380C = (&DAT_100abc78)[DAT_100abde8];
  if (g_brP277B40 != 0) {
    sprintf(DAT_10ac4db0, (char *)BrStrGet(0xb8), (char *)BrStrGet(DAT_100abaa8[g_Br0B380C]));
    if ((PTR_PTR_100bcab0[g_Br0B380C][4] & 0x10) != 0) {
      strcat(DAT_10ac4db0, (char *)BrStrGet(0xb0));
    }
    FUN_100368a0(g_brP680584, g_brPA9D008, 1);
    strcpy(DAT_10ac4db0, DAT_10396f08);
  }
  return 1;
}


#endif /* BR_MATCHING_BUILD */
