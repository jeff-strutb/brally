/* 0x00401470 SetMediaState */
#ifdef BR_MATCHING_BUILD
/* @implements 0x00401470 bossrally.exe SetMediaState */

#include <windows.h>

extern int gMediaState;

void SetMediaState(int s)
{
    gMediaState = s;
}

#endif
