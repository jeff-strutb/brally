/* br_timer.c -- startup: the game's timer services.
 *
 * Filed out of the address batches; each section keeps the declarations the
 * batch it came from made locally, so the compiler's view of each body is
 * unchanged.
 */
#ifdef BR_MATCHING_BUILD

#include <windows.h>

/* ---- from slice1_09.c ---------------------------------------------- */

extern int g_br18AB118_S_S1499;

/* WHAT IT DOES: return the current timer subsystem state. */
/* @implements 0x1006E350 glide BrGetTimerState */

int BrGetTimerState(void)

{
  return g_br18AB118_S_S1499;
}

#endif /* BR_MATCHING_BUILD */
