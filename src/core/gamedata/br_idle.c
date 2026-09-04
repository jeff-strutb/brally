/* br_idle.c -- gamedata.
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

extern int DAT_106e7738;
extern int DAT_106e9a30;
extern int DAT_106ec6a8;
extern int DAT_106ed700;
int BrPodNop();
extern int DAT_106ed5d0;
int BrStubTrue();

/* WHAT IT DOES: never-returning loop over the 0x106EC6A8 / 0x106ED5D0 blocks, advancing the
 * 16-entry ring index at 0x106ED700 each pass. */
/* @implements 0x1002DE04 glide BrIdleLoop_1002DE04 */

void BrIdleLoop_1002DE04(void)

{
  BrPodNop(&DAT_106ec6a8,&DAT_106e9a30,1);
  BrPodNop(&DAT_106ec6a8,DAT_106e7738,1);
  for (;;) {
    BrStubTrue(&DAT_106ec6a8,0,1);
    BrStubTrue(&DAT_106ed5d0,DAT_106e7738,1);
    DAT_106ed700 = DAT_106ed700 + 1 & 0xf;
  }
}

#endif /* BR_MATCHING_BUILD */
