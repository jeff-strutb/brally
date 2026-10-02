/* br_reset.c -- racing.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */
/* The original is /MD: CRT calls go through the import
 * table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include <stdint.h>


/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: zero the race pad/step state block.  The 0x106ED588 store is
 * a LOAD of the just-zeroed 0x106EC768 (Ghidra folds it to 0; the /Od bytes
 * re-read it). */
/* @implements 0x1002E13B glide BrReset_1002E13B */

void BrReset_1002E13B(void)

{
  (*(int *)&DAT_106ec740) = 0;
  (*(int *)((char *)&(*(int *)&DAT_106ec740) + 0x4)) = 0;
  DAT_106e7294 = 0;
  DAT_106ec768 = 0;
  DAT_106ed588 = DAT_106ec768;
  DAT_106b7ac0 = 0;
  (*(int *)&g_brRaceFlyStep) = 0;
  return;
}

