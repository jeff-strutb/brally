/* br_secondtick.c -- racing.
 *
 * The once-a-second service loop: it runs the five per-second steps and
 * sleeps out the rest of each second, for ever. Filed out of slice6_76.c's
 * Ghidra-matched section; its declarations are copied rather than moved,
 * because the starter that spawns it stays behind in the slice.
 */
#include <stddef.h>
#include <stdint.h>

#ifdef BR_MATCHING_BUILD
int FUN_1006a650();
int FUN_1006a7e0();
int FUN_1006aaf0();
int FUN_1006ab80();
int FUN_1006b0e0();
extern unsigned int DAT_11849ea8;
extern unsigned int DAT_1184c070;
extern int g_brP277B40;
int BrDelta_100713A0();
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
      FUN_1006a7e0();
      FUN_1006aaf0();
      FUN_1006ab80();
      FUN_1006b0e0(&g_brP277B40);
      DAT_11849ea8 = DAT_11849ea8 + 1000;
    }
    Sleep(DAT_11849ea8 - DAT_1184c070);
  } while( 1 );
}

#endif /* BR_MATCHING_BUILD */
