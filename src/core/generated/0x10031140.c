/* Auto-generated from Ghidra decompilation — 0x10031140 */
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
int FUN_10031030();
extern char DAT_100aa338[];
extern char DAT_100aa340[];
extern char s_tracks__100b74c0[];
extern int PTR_s_desert_trk_100b78c0;



/* WHAT IT DOES: load the handling data for one track, by taking that track's
 * file name from the track table, swapping its extension for the handling
 * one, and passing the result to the loader. */
/* @implements 0x10031140 glide BrTrackLoadHandling */
void BrTrackLoadHandling(int param_1)

{
  char cVar1;
  char *pcVar2;
  unsigned int uVar3;
  unsigned int uVar4;
  char *pcVar5;
  char *pcVar6;
  char local_400 [1024];
  
  sprintf(local_400,DAT_100aa340,s_tracks__100b74c0,(&PTR_s_desert_trk_100b78c0)[param_1]);
  pcVar2 = strrchr(local_400,0x2e);
  strcpy(pcVar2, DAT_100aa338);
  FUN_10031030(local_400);
  return;
}


#endif /* BR_MATCHING_BUILD */
