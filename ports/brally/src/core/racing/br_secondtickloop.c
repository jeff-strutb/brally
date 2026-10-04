/* br_secondtick.c -- racing.
 *
 * The once-a-second service loop: it runs the five per-second steps and
 * sleeps out the rest of each second, for ever. Filed out of slice6_76.c's
 * Ghidra-matched section; its declarations are copied rather than moved,
 * because the starter that spawns it stays behind in the slice.
 */
#include "slice2_25.h"   /* br_globals: its objects */
#include <stddef.h>
#include <stdint.h>

/* FUN_1006a650: prototype in br_funcs.h */
/* FUN_1006a7e0: prototype in br_funcs.h */
/* BrNetPeerMsgReset: prototype in br_funcs.h */
/* FUN_1006ab80: prototype in br_funcs.h */
/* FUN_1006b0e0: prototype in br_funcs.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* BrDelta_100713A0: prototype in br_funcs.h */
#include <windows.h>

/* WHAT IT DOES: a 1 Hz service loop that never returns: read the elapsed-ms counter; if the
 * next second has not arrived, Sleep until it does, otherwise run the five once-a-second
 * steps and advance the deadline by 1000 ms. Both globals are unsigned (jb). */
/* @implements 0x1006A5F0 glide BrSecondTickLoop */

void BrSecondTickLoop(void)

{
  do {
    while( 1 ) {
      DAT_1184c070 = BrDelta_100713A0();
      if (DAT_1184c070 < DAT_11849ea8) break;
      FUN_1006a650();
      BrNetPeerRank();
      BrNetPeerMsgReset();
      BrNetPeerPump();
      BrNetPeerSendPass(&g_brP277B40);
      DAT_11849ea8 = DAT_11849ea8 + 1000;
    }
    Sleep(DAT_11849ea8 - DAT_1184c070);
  } while( 1 );
}

