/* br_rec.c -- drawing.
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
#include <stdint.h>

#ifdef BR_MATCHING_BUILD

extern short DAT_10396ef8;
extern short DAT_10396efc;
extern short DAT_10396f00;
extern int DAT_10396f04;

/* WHAT IT DOES: latch a record header -- three SHORT fields (+2, +4, +6)
 * into the 0x10396EF8 group and the payload pointer (+8) into 0x10396F04.
 * The three globals are 16-bit (auto-refined; int externs mis-encode). */
/* @implements 0x10010F80 glide BrRecHdrLatch_10010F80 */

void BrRecHdrLatch_10010F80(int param_1)

{
  DAT_10396f04 = param_1 + 8;
  DAT_10396efc = *(short *)(param_1 + 2);
  DAT_10396ef8 = *(short *)(param_1 + 4);
  DAT_10396f00 = *(short *)(param_1 + 6);
  return;
}

#endif /* BR_MATCHING_BUILD */
