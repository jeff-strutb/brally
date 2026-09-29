/* br_uihook85.c -- menus: the "85" family of front-end control hooks.
 *
 * The small hook functions the 0x10038xxx block of the original installs into
 * the front-end control table (slice8_85.h lists the table): list and edit
 * actions that step an option, read an edited string back into its record,
 * or apply the highlighted item -- 0x10038420..0x100387C0.  Matching build
 * only; the port versions of the ones it needs live in br_uictlhook.c.
 *
 * Every function carries its original address.
 */
/* The original is /MD: CRT calls go through the import table (FF 15). */
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

/* ------------------------------------------------------------------ */
/* 0x10038420                                                         */
/* ------------------------------------------------------------------ */

typedef int (*funcptr)();

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
  int iVar2;
  char *pcVar5;
  
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

/* ------------------------------------------------------------------ */
/* 0x10038490                                                         */
/* ------------------------------------------------------------------ */

/* WHAT IT DOES: the flag-only member of that text-field family: it clears
 * the enable bit on its page when the field is non-empty and copies nothing.
 * Always reports success. */
/* @implements 0x10038490 glide BrUiFn1003EF60 */
int BrUiFn1003EF60(int param_1)

{
  
  if (strlen((char *)(param_1 + 0x2b65)) != 0) {
    *(unsigned int *)(DAT_10ac5d00 + 0x1c) = *(unsigned int *)(DAT_10ac5d00 + 0x1c) & 0xffffffef;
  }
  return 1;
}

/* ------------------------------------------------------------------ */
/* 0x10038550                                                         */
/* ------------------------------------------------------------------ */

extern int DAT_10ac5d40;

/* WHAT IT DOES: same flag-only text-field callback as BrUiFn1003EF60, acting
 * on a different page's enable bit (0x10AC5D40). */
/* @implements 0x10038550 glide BrUiFn1003F020 */
int BrUiFn1003F020(int param_1)

{
  
  if (strlen((char *)(param_1 + 0x2b65)) != 0) {
    *(unsigned int *)(DAT_10ac5d40 + 0x1c) = *(unsigned int *)(DAT_10ac5d40 + 0x1c) & 0xffffffef;
  }
  return 1;
}

/* ------------------------------------------------------------------ */
/* 0x10038580                                                         */
/* ------------------------------------------------------------------ */

extern char DAT_10b71aa0[];

/* WHAT IT DOES: text-field commit for the setting at 0x10B71AA0 -- applies
 * the item and copies the typed string over if it differs, case-
 * insensitively. No page flag. */
/* @implements 0x10038580 glide Br85TextReadBack */
int Br85TextReadBack(int param_1)

{
  int iVar2;
  
  Br85ItemApply(param_1,0);
  iVar2 = _stricmp(DAT_10b71aa0,(char *)(param_1 + 0x2b65));
  if (iVar2 != 0) {
    strcpy(DAT_10b71aa0, (char *)(param_1 + 0x2b65));
  }
  return 1;
}

/* ------------------------------------------------------------------ */
/* 0x100385F0                                                         */
/* ------------------------------------------------------------------ */

extern char DAT_10b71ac0[];

/* WHAT IT DOES: text-field commit for the setting at 0x10B71AC0, otherwise
 * identical to Br85TextReadBack. */
/* @implements 0x100385F0 glide BrUiHook85_1003F0B0 */
int BrUiHook85_1003F0B0(int param_1)

{
  int iVar2;
  
  Br85ItemApply(param_1,0);
  iVar2 = _stricmp(DAT_10b71ac0,(char *)(param_1 + 0x2b65));
  if (iVar2 != 0) {
    strcpy(DAT_10b71ac0, (char *)(param_1 + 0x2b65));
  }
  return 1;
}

/* ------------------------------------------------------------------ */
/* 0x10038650                                                         */
/* ------------------------------------------------------------------ */

extern char DAT_10ac4db0[];

/* WHAT IT DOES: text-field commit for the setting at 0x10AC4DB0, otherwise
 * identical to Br85TextReadBack. */
/* @implements 0x10038650 glide BrUiFn1003F110 */
int BrUiFn1003F110(int param_1)

{
  int iVar2;
  
  Br85ItemApply(param_1,0);
  iVar2 = _stricmp(DAT_10ac4db0,(char *)(param_1 + 0x2b65));
  if (iVar2 != 0) {
    strcpy(DAT_10ac4db0, (char *)(param_1 + 0x2b65));
  }
  return 1;
}

/* ------------------------------------------------------------------ */
/* 0x100386B0                                                         */
/* ------------------------------------------------------------------ */

/* 0x100386B0 (d3d 0x1003F170) -- hand the edited name off and clear both
 * buffers.
 *
 * WHAT IT DOES: copy the +0x2B5C item's label into the shared name buffer,
 * pass it to the commit helper along with the current slot, then blank the
 * shared buffer and the item label with the empty string. Returns 1.
 *
 * The port body in slice2_23.c reaches every global through a
 * BrUiGlobals* it takes as a second parameter; the original is cdecl with
 * ONE argument and direct global addresses, which is the whole 129-diff
 * gap (six extra `mov r,[r+disp]`, an extra push and an extra call). Same
 * split as the 0x10038650 sibling next door.
 *
 * All three copies are the inline strcpy form, and the original interleaves
 * the helper's argument pushes into the first copy's expansion -- that
 * falls out of writing the call as the next statement, since the arguments
 * are plain loads.
 */

extern char g_szBrName4DB0[];       /* 0x10AC4DB0 */
extern int  g_brSlot4098;           /* 0x10AC4098 */
extern int  g_brOwner5BC72C;        /* 0x105BC72C */
extern char g_szBrEmpty396F08[];    /* 0x10396F08 */

extern int BrFn1003D210_glide(int a, int b, int c);     /* 0x100368A0 */

/* @implements 0x100386B0 glide BrUiFn1003F170 */
int BrUiFn1003F170(int param_1)
{
    char *pText = (char *)(param_1 + 0x2b65);

    strcpy(g_szBrName4DB0, pText);

    BrFn1003D210_glide(g_brOwner5BC72C, g_brSlot4098, 0);

    strcpy(g_szBrName4DB0, g_szBrEmpty396F08);
    strcpy(pText, g_szBrEmpty396F08);

    return 1;
}

/* ------------------------------------------------------------------ */
/* 0x10038750                                                         */
/* ------------------------------------------------------------------ */

extern char DAT_10ac40a8[];
extern int DAT_10ac5d14;

/* WHAT IT DOES: text-field commit for the setting at 0x10AC40A8, and it also
 * clears the enable bit on the 0x10AC5D14 page when the field is non-empty. */
/* @implements 0x10038750 glide BrUiFn1003F210 */
int BrUiFn1003F210(int param_1)

{
  int iVar2;
  char *pcVar5;
  
  Br85ItemApply(param_1,0);
  pcVar5 = (char *)(param_1 + 0x2b65);
  if (strlen(pcVar5) != 0) {
    *(unsigned int *)(DAT_10ac5d14 + 0x1c) = *(unsigned int *)(DAT_10ac5d14 + 0x1c) & 0xffffffef;
  }
  iVar2 = _stricmp(DAT_10ac40a8,pcVar5);
  if (iVar2 != 0) {
    strcpy(DAT_10ac40a8, pcVar5);
  }
  return 1;
}

/* ------------------------------------------------------------------ */
/* 0x100387C0                                                         */
/* ------------------------------------------------------------------ */

/* WHAT IT DOES: the flag-only partner of BrUiFn1003F210 -- clears the same
 * 0x10AC5D14 page bit when the field is non-empty, and copies nothing. */
/* @implements 0x100387C0 glide BrUiFn1003F280 */
int BrUiFn1003F280(int param_1)

{
  
  if (strlen((char *)(param_1 + 0x2b65)) != 0) {
    *(unsigned int *)(DAT_10ac5d14 + 0x1c) = *(unsigned int *)(DAT_10ac5d14 + 0x1c) & 0xffffffef;
  }
  return 1;
}

