/* br_rec.c -- drawing.
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

/* WHAT IT DOES: latch a record header -- three SHORT fields (+2, +4, +6)
 * into the 0x10396EF8 group and the payload pointer (+8) into 0x10396F04.
 * The three globals are 16-bit (auto-refined; int externs mis-encode). */
/* @implements 0x10010F80 glide BrRecHdrLatch_10010F80 */

void BrRecHdrLatch_10010F80(unsigned char *param_1)

{
  DAT_10396f04 = (struct BrPaceNote *)(param_1 + 8);
  DAT_10396efc = *(short *)(param_1 + 2);
  DAT_10396ef8 = *(short *)(param_1 + 4);
  DAT_10396f00 = *(short *)(param_1 + 6);
  return;
}

