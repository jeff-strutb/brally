/* Matching TU for 0x10035400 — DirectPlay init (prefix of a map-split
 * function; 0x10035533 is the internal join, not a separate C function).
 * Inferred from orig bytes: two lstrcpyA via IAT-in-esi, /Oi memset of
 * 8+16 dwords, stride-0xe0 zero, nested GetModuleHandleA into
 * FUN_10009b00, FUN_10037260(FUN_1006d280(id), hr). */
#ifdef BR_MATCHING_BUILD

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

int FUN_10009b00(HMODULE, int *);
int FUN_10032320(int *);
int FUN_10035bb0(int *);
int FUN_1006d280(int);
int FUN_10037260(int, int);
int FUN_10036e50(int *);
int FUN_10036300(int *);
int __stdcall FUN_10035ac0(int, int, int, int, int, int);

extern int DAT_10ac3e80;
extern int DAT_10ac3f88;
extern int DAT_10396f08;
extern int DAT_10b71aa0;
extern int DAT_10b71ac0;
extern int DAT_10ac315c;
extern int DAT_10273328;
extern int DAT_10273334;
extern int DAT_10226a48;
extern int DAT_10ac5bdc;
extern int DAT_10ac4090;
extern int DAT_10ac4098;
extern int DAT_10077500;
extern int DAT_105bc72c;
extern int DAT_10ac3068;
extern int DAT_10ac5d2c;
extern int DAT_10ac5bf0;
extern int DAT_10ac306c;
extern int DAT_10ac408c;
extern char s_Could_not_create_DirectPlay_obje_100aa4d8[];

typedef int (__stdcall *CC_std_5)(int, int, int, int, int);

/* WHAT IT DOES: initialise the multiplayer subsystem from cold -- clears the
 * session name and password buffers, zeroes every entry in the player table,
 * and creates the DirectPlay object. Returns zero if DirectPlay is
 * unavailable, which is how the game discovers multiplayer cannot run. */
/* @implements 0x10035400 glide FUN_10035400 */
int FUN_10035400(void)
{
  int *puVar1;
  int iVar4;
  char local_400[1024];

  lstrcpyA((LPSTR)&DAT_10ac3e80, (LPCSTR)&DAT_10396f08);
  lstrcpyA((LPSTR)&DAT_10ac3f88, (LPCSTR)&DAT_10396f08);
  memset(&DAT_10b71aa0, 0, 32);
  memset(&DAT_10b71ac0, 0, 64);
  puVar1 = &DAT_10ac315c;
  do {
    *puVar1 = 0;
    puVar1 = puVar1 + 0x38;
  } while ((int)puVar1 < 0x10ac3f5c);

  iVar4 = FUN_10009b00(GetModuleHandleA((LPCSTR)0x0), &DAT_10273328);
  if (iVar4 < 0) {
    return 0;
  }
  iVar4 = FUN_10032320(&DAT_10273328);
  if (iVar4 == -0x7788fbd2) {
    DAT_10273328 = 0;
    iVar4 = FUN_10035bb0(&DAT_10273328);
    if (iVar4 < 0) {
      sprintf(local_400, s_Could_not_create_DirectPlay_obje_100aa4d8, iVar4);
      FUN_10037260(FUN_1006d280(0x12b), iVar4);
      return 0;
    }
  } else {
    if (iVar4 < 0) {
      FUN_10037260(FUN_1006d280(0x12a), iVar4);
      exit(1);
    }
    if (DAT_10273334 != 0) {
      DAT_10226a48 = 2;
      DAT_10ac5bdc = 1;
    } else {
      DAT_10226a48 = 1;
      DAT_10ac5bdc = 0;
    }
    DAT_10ac4090 = 1;
  }
  iVar4 = DAT_10ac4090;
  DAT_10ac4098 = (int)&DAT_10273328;
  if (iVar4 == 0) {
    (*(CC_std_5 *)(*(int *)DAT_10273328 + 0x8c))(
        DAT_10273328, &DAT_10077500, (int)FUN_10035ac0, DAT_105bc72c, 0);
    iVar4 = FUN_10036e50(&DAT_10ac3068);
    if (iVar4 < 0) {
      return 0;
    }
  } else {
    iVar4 = DAT_10ac5d2c;
    DAT_10ac5bf0 = 0;
    if (iVar4 != 0) {
      iVar4 = FUN_10036300(DAT_10273328);
      if (iVar4 < 0) {
        return 0;
      }
    }
    DAT_10ac306c = SetTimer((HWND)DAT_105bc72c, 1, 1000, (TIMERPROC)0x0);
    DAT_10ac408c = 1;
  }
  return 1;
}

#endif /* BR_MATCHING_BUILD */
