/* Auto-generated from Ghidra decompilation — 0x1005A080 */
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
int FUN_10003680(char *);
int FUN_1005a280(int, int, int, void *);
int FUN_1005a300(int, int, int, int);
extern int DAT_100ad7d0;
extern int DAT_100ad7d8[];
extern int DAT_10ac67c4;
extern int DAT_10ac67c8;
extern char s_Paint__s_100b2e4c[];
int BrBmpLoadRgba(char *);

/* WHAT IT DOES: load the paint-scheme bitmaps for one car -- walks that
 * car's thirty livery entries and, for any that names a file it has not
 * loaded yet, reads the bitmap in as raw pixels. Skips entries whose file is
 * missing. */
/* @implements 0x1005A080 glide FUN_1005a080 */
void FUN_1005a080(int param_1, int param_2)
{
  int nLeft;
  int nCopy;
  char buf[0x400];
  int *p;
  void *pMem;
  int *pSlot;

  nLeft = 0x1e;
  p = DAT_100ad7d8 + param_1 * 300;
  do {
    if (*p == 0 && p[9] != 0) {
      sprintf(buf, s_Paint__s_100b2e4c, p[9]);
      if (FUN_10003680(buf) != 0) {
        *p = BrBmpLoadRgba(buf);
        buf[8] = 100;
        if (FUN_10003680(buf) != 0) {
          pMem = (void *)BrBmpLoadRgba(buf);
          if (*p != 0 && pMem != 0) {
            FUN_1005a280(*p, DAT_10ac67c4, DAT_10ac67c8, pMem);
            free(pMem);
          }
        }
        if (*p != 0 && p[4] >= 0 && param_2 != 0 && DAT_100ad7d0 != 0) {
          nCopy = 1;
          pSlot = p + 1;
          do {
            pMem = malloc(DAT_10ac67c8 * DAT_10ac67c4 * 4);
            *pSlot = (int)pMem;
            if (pMem != 0) {
              memcpy(pMem, (void *)*p, DAT_10ac67c8 * DAT_10ac67c4 * 4);
              FUN_1005a300(nCopy, *pSlot, DAT_10ac67c4, DAT_10ac67c8);
            }
            pSlot = pSlot + 1;
            nCopy = nCopy + 1;
          } while (nCopy < 4);
        }
      }
    }
    p = p + 10;
    nLeft = nLeft - 1;
  } while (nLeft != 0);
}


#endif /* BR_MATCHING_BUILD */
