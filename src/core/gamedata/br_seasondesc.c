/* br_seasondesc.c -- gamedata: the season-description file pair.
 *
 * Filed out of the address batch slice6_73.c, whose include preamble this
 * file carries verbatim.  RESPONSIBILITY: gamedata.  0x10055A40 writes the
 * whole season-description table out to one file and 0x10055AF0 reads one
 * numbered season's description back in, so they are the game's own data
 * files rather than any part of the menu screens they sat beside in the
 * original packet.
 *
 * See slice6_73.h for the scope and the conflicts.
 */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <string.h>
#include <stdio.h>
#include <stddef.h>

#include "slice6_73.h"

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
#include <windows.h>
int FUN_100378c0();
extern char DAT_1007b600[];
extern int g_brPhaseAA2904;
extern char s_seasondesc_dat_100acb4c[];

/* WHAT IT DOES: write every season description out to one file, 100 bytes
 * per entry, aborting through the error reporter if the file cannot be
 * opened or a write comes up short. */
/* @implements 0x10055A40 glide FUN_10055a40 */
/* auto-filed from ghidra --refine; transforms: as-is */

int __stdcall FUN_10055a40(int _pad_0)
{
  char cVar1;
  FILE *_File;
  unsigned int sVar2;
  unsigned int uVar3;
  unsigned int uVar4;
  int iVar5;
  char *pcVar6;
  char *pcVar7;
  char acStack_64 [100];

  _File = fopen(s_seasondesc_dat_100acb4c, DAT_1007b600);
  if (_File == (FILE *)0x0) {
    FUN_100378c0(7);
  }
  iVar5 = 0;
  for (;;) {
    strcpy(acStack_64, (char *)(*(int *)(g_brPhaseAA2904 + 0xc0) + 4 + iVar5));
    sVar2 = fwrite(acStack_64,1,100,_File);
    if ((int)sVar2 < 100) {
      FUN_100378c0(7);
    }
    iVar5 = iVar5 + 0x104;
    if (iVar5 < 0x6590) {
      continue;
    }
    fclose(_File);
    return 1;
  }
}


int FUN_100378c0();
extern char DAT_1007b0e0[];
extern char DAT_100acaf8[];
extern int g_brPhaseAA2904;
extern char s_RallySeason_100acb00[];

/* WHAT IT DOES: load one numbered season's description from its own file,
 * upper-case it, and store it in the season table. A missing file is
 * silently ignored, so an absent season simply keeps its previous text. */
/* @implements 0x10055AF0 glide FUN_10055af0 */
/* auto-filed from ghidra --refine; transforms: stringops */

int __stdcall FUN_10055af0(short param_1)
{
  char cVar1;
  FILE *_File;
  unsigned int sVar2;
  char *pcVar3;
  unsigned int uVar4;
  unsigned int uVar5;
  int iVar6;
  char *pcVar7;
  char *pcVar8;
  char acStack_20c [4];
  char acStack_208 [260];
  char acStack_104 [260];

  strcpy(acStack_208, s_RallySeason_100acb00);
  _itoa((int)param_1,acStack_20c,10);
  pcVar3 = acStack_208; strcat(acStack_208, acStack_20c);
  pcVar3 = acStack_208; strcat(acStack_208, DAT_100acaf8);
  _File = fopen(acStack_208,DAT_1007b0e0);
  if (_File != (FILE *)0x0) {
    memset(acStack_104, 0, 260);
    sVar2 = fread(acStack_104,1,0x80,_File);
    if ((int)sVar2 < 0x80) {
      FUN_100378c0(7);
    }
    pcVar3 = _strupr(acStack_104);
    strcpy((char *)(*(int *)(g_brPhaseAA2904 + 0xc0) + 4 + param_1 * 0x104), pcVar3);
    fclose(_File);
  }
  return 1;
}

#endif /* BR_MATCHING_BUILD */
