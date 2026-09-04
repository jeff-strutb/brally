/* br_cdcheck.c -- settings.
 *
 * Recognising the Boss Rally CD: the per-drive-letter test the disc hunt
 * runs for each letter in turn.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import
 * table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdio.h>
#include <string.h>

#ifdef BR_MATCHING_BUILD
#include <windows.h>

extern char DAT_100acacc[];
extern char DAT_100acad0[];
extern char s_Boss_Rally_1007b384[];
_CRTIMP int __cdecl _chdrive(int);
_CRTIMP int __cdecl _chdir(const char *);

/* WHAT IT DOES: check whether one drive letter is the Boss Rally CD -- it
 * must be a CD-ROM, hold the expected directory, and carry the expected
 * volume label. The per-drive test the CD hunt calls for each letter in
 * turn. */
/* @implements 0x100377A0 glide FUN_100377a0 */
/* auto-filed from ghidra --refine; transforms: as-is */

int FUN_100377a0(int param_1)

{
  char path[260];
  char vol[260];
  
  sprintf(path, DAT_100acad0, param_1 + 0x40);
  if (_chdrive(param_1) == 0) {
    if (GetDriveTypeA((LPCSTR)0) == 5) {
      if (_chdir(DAT_100acacc) == 0) {
        if (GetVolumeInformationA(path, vol, 0x104, 0, 0, 0, 0, 0) != 0) {
          if (strcmp(vol, s_Boss_Rally_1007b384) == 0) {
            return 1;
          }
        }
      }
    }
  }
  return 0;
}

#endif /* BR_MATCHING_BUILD */
