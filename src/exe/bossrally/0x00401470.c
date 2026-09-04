/* 0x00401470 SetMediaState */
#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: record the requested playback state -- playing, paused or
 * stopped -- for the rest of the player to act on. */
/* @implements 0x00401470 bossrally.exe SetMediaState */

#include <windows.h>

extern int gMediaState;

void SetMediaState(int s)
{
    gMediaState = s;
}

#endif
