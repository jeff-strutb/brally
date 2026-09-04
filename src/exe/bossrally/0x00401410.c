/* 0x00401410 CanRun: state is Stopped(1) or Paused(2) */
#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: report whether a playback graph exists and is in a state
 * that can be started. */
/* @implements 0x00401410 bossrally.exe CanRun */

#include <windows.h>

extern int gMediaState;

int CanRun(void)
{
    int s;

    s = gMediaState;
    if (s == 1 || s == 2)
        return 1;
    return 0;
}

#endif
