/* Auto-generated from disassembly — 0x00401DF0
 * IAT thunk: jmp [_except_handler3]. */
#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: jump to the CRT's exception handler through the import
 * table. A linker-generated thunk, not launcher code. */
/* @implements 0x00401DF0 brally.exe thunk_except_handler3 */

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
void _CRTIMP __cdecl _except_handler3(void);


void thunk_except_handler3(void)
{
    _except_handler3();
}


#endif /* BR_MATCHING_BUILD */
