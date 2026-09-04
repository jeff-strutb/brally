/* Auto-generated from Ghidra decompilation — 0x100357E0 */
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
int FUN_10035c50();
int FUN_100367c0();
extern int DAT_10226a48;
extern int g_brP277B40;
extern int g_brPA9D008;
extern char s_Could_not_host_session_because_o_100aa580[];
int BrNetMutexInit();
int BrSub10071550();



/* WHAT IT DOES: host a new multiplayer session -- builds the session
 * description, asks DirectPlay to create it, and on success marks the game
 * as hosting and starts the networking mutexes. On failure it formats a
 * message and returns without starting anything. */
/* @implements 0x100357E0 glide BrSub1003C150 */
void BrSub1003C150(void)

{
  int *puVar1;
  int iVar2;
  int local_4cc [51];
  char local_400 [1024];
  
  if (g_brP277B40 != 0) {
    memset(local_4cc, 0, 204);
    FUN_100367c0(local_4cc);
    iVar2 = FUN_10035c50(g_brP277B40,local_4cc,g_brPA9D008);
    if (iVar2 < 0) {
      sprintf(local_400,s_Could_not_host_session_because_o_100aa580,iVar2);
      return;
    }
    DAT_10226a48 = 2;
    BrSub10071550();
    BrNetMutexInit(1);
  }
  return;
}


#endif /* BR_MATCHING_BUILD */
