/* 0x00401450 IsStopped: gMediaState == 1 */
/* WHAT IT DOES: report whether playback is stopped. */
/* @implements 0x00401450 bossrally.exe IsStopped */
/* @n64 0x8026B738 located */

#include <windows.h>

extern int gMediaState;

int IsStopped(void)
{
    return gMediaState == 1;
}

