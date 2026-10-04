/* br_secondtick.c -- startup: spawn the once-a-second service thread.
 *
 * Filed out of the address batch slice6_76.c, whose local declarations for
 * the four globals and for the thread body are copied here.  The loop the
 * thread runs, 0x1006A5F0 BrSecondTickLoop, is filed under racing/ and stays
 * where it is; this TU only needs its address.
 */

#include <windows.h>

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* 0x1006A5F0, filed under racing/. */
/* BrSecondTickLoop: prototype in br_funcs.h */

/* WHAT IT DOES: start the once-a-second service: create its wake event and spawn
 * BrSecondTickLoop on its own thread, arming the first 1000 ms deadline. */
/* @implements 0x1006A5A0 glide BrSecondTickStart */

void BrSecondTickStart(void)

{
  g_hBrSndWake86 = CreateEventA((LPSECURITY_ATTRIBUTES)0x0,0,0,(LPCSTR)0x0);
  g_hBrSndThread86 = CreateThread((LPSECURITY_ATTRIBUTES)0x0,0,(LPTHREAD_START_ROUTINE)BrSecondTickLoop,
                              (LPVOID)0x0,0,(LPDWORD)&DAT_11849e64);
  DAT_11849ea8 = 1000;
  (*(int *)&g_fBrSndThread86) = 1;
  return;
}

