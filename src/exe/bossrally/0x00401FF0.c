/* 0x00401FF0 _setdefaultprecision: E8 to local _controlfp (static CRT). */
#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: set the floating-point precision the program expects at
 * start-up. */
/* @implements 0x00401FF0 bossrally.exe _setdefaultprecision */

#include <windows.h>

unsigned int _controlfp(unsigned int, unsigned int);

void _setdefaultprecision(void)
{
    _controlfp(0x10000, 0x30000);
}

#endif
