/* br_scenepools.c -- gamedata: wiring up a scene's model and animation pools.
 *
 * The one-shot setup that points the fixed table pointers at the loaded data
 * block and starts the two service loops that feed them. Filed out of
 * slice2_19.c's Ghidra-matched section; the declaration block it needs is
 * copied rather than moved, because functions left behind in the slice use
 * the same symbols.
 *
 * See slice2_19.h for the recovered layouts and the gotchas.
 */
#include "slice1_05.h"   /* br_globals: its objects */
#include <windows.h>

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* BrPodNop: prototype in br_funcs.h */
/* BrStubFalse: prototype in br_funcs.h */
/* BrStubTrue: prototype in br_funcs.h */

/* The two service loops this hands to the pool starter; they live in
 * src/core/racing/br_idleloop.c. */
/* BrIdleLoop_1002DD30: prototype in br_funcs.h */
/* BrIdleLoop_1002DD9A: prototype in br_funcs.h */

/* WHAT IT DOES: set up the model and animation pools for a scene from the
 * loaded data block, wiring the fixed table pointers before anything reads
 * them. */
/* @implements 0x1002DEC3 glide FUN_1002dec3 */
/* auto-filed from ghidra --refine; transforms: as-is */

void FUN_1002dec3(void)

{
  struct {
    int result;
    char buf[24];
    unsigned int flags;
    int idx;
    char tmp[4];
  } local_28;

  DAT_106e79d4 = &DAT_106b80a8;
  BrPodNop();
  BrPodNop();
  BrPodNop();
  BrPodNop();
  BrPodNop();
  BrPodNop();
  BrPodNop();
  BrPodNop();
  BrPodNop();
  BrPodNop();
  BrPodNop();
  _DAT_106ec770 = 0x47371b00;
  BrPodNop();
  BrPodNop();
  BrStubTrue();
  BrPodNop();
  BrPodNop();
  for (local_28.idx = 0; local_28.idx < 4; local_28.idx = local_28.idx + 1) {
    (&DAT_106e7730)[local_28.idx] = 0;
    if (((int)(local_28.flags & 0xff) >> local_28.idx & 1) != 0) {
      if (((&DAT_106e79bb)[local_28.idx * 4] & 8) == 0) {
        if ((*(unsigned short *)(&DAT_106e79b8 + local_28.idx * 4) & 4) != 0) {
          if (((&DAT_106e79ba)[local_28.idx * 4] & 1) != 0) {
            local_28.result = BrStubFalse();
            if (local_28.result != 0) {
              if (local_28.result > 9) {
                if (local_28.result > 0xb) goto next;
                if (BrStubFalse() == 0) {
                  (&DAT_106e7730)[local_28.idx] = 1;
                  BrStubFalse();
                }
              }
            }
          }
        }
      }
    }
next:
    ;
  }
  DAT_106ed6e0 = CreateMutexA((LPSECURITY_ATTRIBUTES)0x0,0,(LPCSTR)0x0);
  return;
}
