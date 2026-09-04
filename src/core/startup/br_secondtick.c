/* br_secondtick.c -- startup: spawn the once-a-second service thread.
 *
 * Filed out of the address batch slice6_76.c, whose local declarations for
 * the four globals and for the thread body are copied here.  The loop the
 * thread runs, 0x1006A5F0 BrSecondTickLoop, is filed under racing/ and stays
 * where it is; this TU only needs its address.
 */
#ifdef BR_MATCHING_BUILD

#include <windows.h>

extern int          DAT_11849e60;
extern int          DAT_11849e64;
extern unsigned int DAT_11849ea8;
extern int          DAT_1184c078;
extern int          DAT_1184c07c;

/* 0x1006A5F0, filed under racing/. */
extern void BrSecondTickLoop(void);

/* WHAT IT DOES: start the once-a-second service: create its wake event and spawn
 * BrSecondTickLoop on its own thread, arming the first 1000 ms deadline. */
/* @implements 0x1006A5A0 glide BrSecondTickStart */

void BrSecondTickStart(void)

{
  DAT_11849e60 = (int)CreateEventA((LPSECURITY_ATTRIBUTES)0x0,0,0,(LPCSTR)0x0);
  DAT_1184c07c = (int)CreateThread((LPSECURITY_ATTRIBUTES)0x0,0,(LPTHREAD_START_ROUTINE)BrSecondTickLoop,
                              (LPVOID)0x0,0,(LPDWORD)&DAT_11849e64);
  DAT_11849ea8 = 1000;
  DAT_1184c078 = 1;
  return;
}

#endif /* BR_MATCHING_BUILD */
