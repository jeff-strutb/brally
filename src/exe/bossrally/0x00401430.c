/* 0x00401430 IsPlayingOrPaused: state is Playing(3) or Paused(2) */
#ifdef BR_MATCHING_BUILD
/* @implements 0x00401430 bossrally.exe IsPlayingOrPaused */

#include <windows.h>

extern int gMediaState;

int IsPlayingOrPaused(void)
{
    int s;

    s = gMediaState;
    if (s == 3 || s == 2)
        return 1;
    return 0;
}

#endif
