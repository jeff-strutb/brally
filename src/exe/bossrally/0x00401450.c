/* 0x00401450 IsStopped: gMediaState == 1 */
#ifdef BR_MATCHING_BUILD
/* @implements 0x00401450 bossrally.exe IsStopped */

#include <windows.h>

extern int gMediaState;

int IsStopped(void)
{
    return gMediaState == 1;
}

#endif
