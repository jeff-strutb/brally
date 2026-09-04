/* Auto-generated from Ghidra decompilation — 0x100378C0 */
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
typedef struct { int fatal; int strid; } BrErrEnt;
extern BrErrEnt DAT_100abe00[];
extern int g_brP680584;
int BrStrGet(int);



/* WHAT IT DOES: show one of the game's numbered error messages in a Windows
 * message box, looking the text up in the string table so it appears in the
 * player's language, and quitting the game afterwards if that error is
 * marked fatal. */
/* @implements 0x100378C0 glide FUN_100378c0 */
void FUN_100378c0(int param_1)

{
  int iVar1;

  if (param_1 <= 8) {
    iVar1 = BrStrGet(DAT_100abe00[param_1].strid);
    MessageBoxA((HWND)g_brP680584, (LPCSTR)(iVar1 + 1), (LPCSTR)BrStrGet(0xaa), 0);
    if (DAT_100abe00[param_1].fatal != 0) {
      exit(1);
    }
  }
  return;
}


#endif /* BR_MATCHING_BUILD */
