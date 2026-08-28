/* Auto-generated from Ghidra decompilation — 0x10069930 */
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
int FUN_10001000();
extern int DAT_1007b600;
extern int DAT_100abdec;
extern int DAT_100abdf0;
extern int DAT_100abdf4;
extern int DAT_100abdfc;
extern int DAT_100b559c;
extern int DAT_10ac5d60;
extern int DAT_10af3cf0;
extern int DAT_117a6030;
extern int g_pBrMenuACED34;



/* @implements 0x10069930 glide BrMenuSub100709A0 */
char BrMenuSub100709A0(void)

{
  FILE *_File;
  unsigned int sVar1;
  int local_4;
  
  local_4 = FUN_10001000(0,0,0);
  local_4 = FUN_10001000(local_4,g_pBrMenuACED34,0x200);
  _File = fopen((char *)&DAT_117a6030,&DAT_1007b600);
  if (_File == (FILE *)0x0) {
    return 0;
  }
  sVar1 = fwrite(&DAT_100b559c,1,4,_File);
  if (sVar1 != 4) {
    fclose(_File);
    return 0;
  }
  sVar1 = fwrite(&local_4,1,4,_File);
  if (sVar1 != 4) {
    fclose(_File);
    return 0;
  }
  sVar1 = fwrite(g_pBrMenuACED34,1,0x200,_File);
  if (sVar1 != 0x200) {
    fclose(_File);
    return 0;
  }
  fwrite(&DAT_10ac5d60,4,1,_File);
  fwrite(&DAT_100abdec,4,1,_File);
  fwrite(&DAT_100abdf0,4,1,_File);
  fwrite(&DAT_100abdf4,4,1,_File);
  fwrite(&DAT_100abdfc,4,1,_File);
  sVar1 = fwrite(&DAT_10af3cf0,1,0x80,_File);
  if (sVar1 != 0x80) {
    fclose(_File);
    return 0;
  }
  fclose(_File);
  return 1;
}


#endif /* BR_MATCHING_BUILD */
