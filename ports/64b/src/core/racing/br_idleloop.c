/* br_idleloop.c -- racing.
 *
 * The two never-returning service loops the scene pool setup starts, one per
 * pool. Filed out of slice2_19.c's Ghidra-matched section; the declarations
 * are copied rather than moved, because functions left behind in the slice
 * use the same symbols.
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
/* BrPodNop: prototype in br_funcs.h */
/* BrStubTrue: prototype in br_funcs.h */

/* WHAT IT DOES: never-returning service loop: two (compiled-out) trace calls, then forever:
 * stub-true on the 0x106ED650 block, a trace, stub-true on the 0x106EA430 block. */
/* @implements 0x1002DD30 glide BrIdleLoop_1002DD30 */

void BrIdleLoop_1002DD30(void)

{
  BrPodNop();
  BrPodNop();
  for (;;) {
    BrStubTrue();
    BrPodNop();
    BrStubTrue();
  }
}

/* WHAT IT DOES: same shape as BrIdleLoop_1002DD30 over the 0x106ECB48 / 0x106EA410 blocks. */
/* @implements 0x1002DD9A glide BrIdleLoop_1002DD9A */

void BrIdleLoop_1002DD9A(void)

{
  BrPodNop();
  BrPodNop();
  for (;;) {
    BrStubTrue();
    BrPodNop();
    BrStubTrue();
  }
}

