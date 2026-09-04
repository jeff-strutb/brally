/* Auto-generated from disassembly — 0x004017E0
 * UnloadRallyMain: FreeLibrary + clear HMODULE; return 1 if already null. */
#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: unload the renderer DLL and forget the handle. Reports
 * success if there was nothing loaded. */
/* @implements 0x004017E0 brally.exe UnloadRallyMain */

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
extern HMODULE gModule;


int UnloadRallyMain(void)
{
    HMODULE h;

    h = gModule;
    if (h != 0) {
        FreeLibrary(h);
        gModule = 0;
    } else {
        return 1;
    }
}


#endif /* BR_MATCHING_BUILD */
