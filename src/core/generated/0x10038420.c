/* Auto-generated from Ghidra decompilation — 0x10038420 */
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
extern int DAT_10ac5d00;
extern char DAT_10b71648[];
int Br85ItemApply();



/* WHAT IT DOES: commit what the player typed into a menu text field. One of
 * a family of near-identical callbacks: each applies the item, then copies
 * the typed string into ITS OWN destination global if it changed, and clears
 * an enable bit on a related page once the field is non-empty. This one
 * backs the setting at 0x10B71648. */
/* @implements 0x10038420 glide BrUiFn1003EEF0 */
int BrUiFn1003EEF0(int param_1)

{
  char cVar1;
  int iVar2;
  unsigned int uVar3;
  unsigned int uVar4;
  char *pcVar5;
  char *pcVar6;
  
  Br85ItemApply(param_1,0);
  pcVar5 = (char *)(param_1 + 0x2b65);
  if (strlen(pcVar5) != 0) {
    *(unsigned int *)(DAT_10ac5d00 + 0x1c) = *(unsigned int *)(DAT_10ac5d00 + 0x1c) & 0xffffffef;
  }
  iVar2 = _stricmp(DAT_10b71648,pcVar5);
  if (iVar2 != 0) {
    strcpy(DAT_10b71648, pcVar5);
  }
  return 1;
}


#endif /* BR_MATCHING_BUILD */
