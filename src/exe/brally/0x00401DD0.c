/* Auto-generated from disassembly — 0x00401DD0
 * User _matherr stub (pushed to __setusermatherr). 3 bytes: xor eax,eax; ret. */
#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: the maths error hook the CRT calls; it does nothing and
 * reports the error unhandled. Present because the CRT requires one. */
/* @implements 0x00401DD0 brally.exe _matherr */

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


int _matherr(void *e)
{
    return 0;
}


#endif /* BR_MATCHING_BUILD */
