/* Auto-generated from Ghidra decompilation — 0x100356B0 */
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
int FUN_10035be0();
int FUN_10036300();
int FUN_10036b20();
extern int DAT_1027332c;
extern int DAT_10ac306c;
extern int DAT_10ac408c;
extern int DAT_10ac4094;
extern int g_brAA287C;
extern int *g_brP277B40;
extern int g_brP680584;
extern int g_brPAA29D4;
extern char s_Could_not_select_service_provide_100aa544[];
int BrComCreateInstance();
typedef int (__stdcall *CC_std_3)(int, int, int);





/* @implements 0x100356B0 glide FUN_100356b0 */
void FUN_100356b0(void)

{
  int iVar1;
  int local_408 [2];
  char local_400 [1024];
  
  local_408[0] = 0;
  local_408[1] = 0;
  KillTimer(g_brP680584,DAT_10ac306c);
  FUN_10035be0();
  iVar1 = FUN_10036b20(local_408,local_408 + 1);
  if (local_408[0] != 0) {
    iVar1 = BrComCreateInstance(&g_brP277B40);
    DAT_10ac4094 = DAT_10ac4094 + 1;
    if ((((iVar1 >= 0)) && (g_brP277B40 != (int *)0x0)) &&
       (iVar1 = (*(CC_std_3 *)(*(int *)(g_brP277B40) + 152))(g_brP277B40,local_408[0],0), (iVar1 >= 0))) {
      if ((g_brAA287C != 2) && (g_brAA287C != 3)) {
        if ((g_brPAA29D4 != 0) && (iVar1 = FUN_10036300(g_brP277B40), iVar1 < 0))
        goto LAB_100357b5;
        DAT_10ac306c = SetTimer(g_brP680584,1,1000,(TIMERPROC)0x0);
        DAT_10ac408c = 1;
      }
      if (DAT_1027332c != (HANDLE)0x0) {
        return;
      }
      DAT_1027332c = CreateEventA((LPSECURITY_ATTRIBUTES)0x0,0,0,(LPCSTR)0x0);
      if (DAT_1027332c != (HANDLE)0x0) {
        return;
      }
      iVar1 = -0x7ff8fff2;
    }
  }
LAB_100357b5: ;
  if (iVar1 != -0x7788fee8) {
    sprintf(local_400,s_Could_not_select_service_provide_100aa544,iVar1);
  }
  return;
}


#endif /* BR_MATCHING_BUILD */
