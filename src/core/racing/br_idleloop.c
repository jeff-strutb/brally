/* br_idleloop.c -- racing.
 *
 * The two never-returning service loops the scene pool setup starts, one per
 * pool. Filed out of slice2_19.c's Ghidra-matched section; the declarations
 * are copied rather than moved, because functions left behind in the slice
 * use the same symbols.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import
 * table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdint.h>

#ifdef BR_MATCHING_BUILD

extern int DAT_106e7738;
extern int DAT_106e79d0;
extern int DAT_106ea388;
extern int DAT_106ea410;
extern int DAT_106ea430;
extern int DAT_106ecb48;
extern int DAT_106ed650;
int BrPodNop();
int BrStubTrue();

/* WHAT IT DOES: never-returning service loop: two (compiled-out) trace calls, then forever:
 * stub-true on the 0x106ED650 block, a trace, stub-true on the 0x106EA430 block. */
/* @implements 0x1002DD30 glide BrIdleLoop_1002DD30 */

void BrIdleLoop_1002DD30(void)

{
  BrPodNop(&DAT_106ed650,&DAT_106e79d0,1);
  BrPodNop(4,&DAT_106ed650,DAT_106e7738);
  for (;;) {
    BrStubTrue(&DAT_106ed650,0,1);
    BrPodNop(1,0,0,0,0xff);
    BrStubTrue(&DAT_106ea430,DAT_106e7738,1);
  }
}

/* WHAT IT DOES: same shape as BrIdleLoop_1002DD30 over the 0x106ECB48 / 0x106EA410 blocks. */
/* @implements 0x1002DD9A glide BrIdleLoop_1002DD9A */

void BrIdleLoop_1002DD9A(void)

{
  BrPodNop(&DAT_106ecb48,&DAT_106ea388,1);
  BrPodNop(9,&DAT_106ecb48,DAT_106e7738);
  for (;;) {
    BrStubTrue(&DAT_106ecb48,0,1);
    BrPodNop(2,0,0,0,0xff);
    BrStubTrue(&DAT_106ea410,DAT_106e7738,1);
  }
}

#endif /* BR_MATCHING_BUILD */
