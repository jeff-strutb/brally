/* 0x00401460 HasGraph: gMediaState != 0 */
#ifdef BR_MATCHING_BUILD
/* @implements 0x00401460 bossrally.exe HasGraph */

#include <windows.h>

extern int gMediaState;

int HasGraph(void)
{
    return gMediaState != 0;
}

#endif
