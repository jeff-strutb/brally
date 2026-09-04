/* Auto-generated from Ghidra decompilation — 0x10055ED0 */
#ifdef BR_MATCHING_BUILD

/* The original binary is /MD: CRT calls resolve through the import table. */
#define _CRTIMP __declspec(dllimport)
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <io.h>
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

/* WHAT IT DOES: count how many files match a wildcard pattern, using the
 * CRT's find-first/find-next walk and stopping after 100 entries. Returns
 * -1 when nothing matches at all; otherwise the number of matches AFTER the
 * first one (the first hit is not counted), which is what the saved-game
 * browser uses as the index of the last slot. */
/* @implements 0x10055ED0 glide BrFileCountMatching */
int __stdcall BrFileCountMatching(const char *pszPattern)
{
  struct _finddata_t fd;
  long h;
  int n;
  int i;

  h = _findfirst(pszPattern, &fd);
  if (h == -1) {
    return -1;
  }
  n = 0;
  for (i = 1; i < 100; i++) {
    if (_findnext(h, &fd) != 0) break;
    n++;
  }
  _findclose(h);
  return n;
}


#endif /* BR_MATCHING_BUILD */
