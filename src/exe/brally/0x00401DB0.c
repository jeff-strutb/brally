/* Auto-generated from disassembly — 0x00401DB0
 * _setdefaultprecision: E8 to the local _controlfp IAT thunk (not FF 15). */
#ifdef BR_MATCHING_BUILD
/* @implements 0x00401DB0 brally.exe _setdefaultprecision */

#define _CRTIMP __declspec(dllimport)
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef true
#define true 1
#define false 0
#endif

typedef int (*funcptr)();

/* Forward declarations for unknown functions/globals */
unsigned int _controlfp_thunk(unsigned int, unsigned int);


void _setdefaultprecision(void)
{
    _controlfp_thunk(0x10000, 0x30000);
}


#endif /* BR_MATCHING_BUILD */
