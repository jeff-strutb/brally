/* 0x00401460 HasGraph: gMediaState != 0 */
/* WHAT IT DOES: report whether a DirectShow filter graph has been built at
 * all. */
/* @implements 0x00401460 bossrally.exe HasGraph */
/* @n64 0x80266390 located */

#include <windows.h>

extern int gMediaState;

int HasGraph(void)
{
    return gMediaState != 0;
}

