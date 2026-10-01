/* br_idle.c -- gamedata.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */
/* The original is /MD: CRT calls go through the import
 * table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "slice1_05.h"   /* br_globals: its objects */
#include <stdint.h>


/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* BrPodNop: prototype in br_funcs.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* BrStubTrue: prototype in br_funcs.h */

/* WHAT IT DOES: never-returning loop over the 0x106EC6A8 / 0x106ED5D0 blocks, advancing the
 * 16-entry ring index at 0x106ED700 each pass. */
/* @implements 0x1002DE04 glide BrIdleLoop_1002DE04 */

void BrIdleLoop_1002DE04(void)

{
  BrPodNop();
  BrPodNop();
  for (;;) {
    BrStubTrue();
    BrStubTrue();
    DAT_106ed700 = DAT_106ed700 + 1 & 0xf;
  }
}

