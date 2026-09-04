/* Auto-generated from disassembly — 0x004017B0
 * LoadRallyMain: LoadLibraryA + GetProcAddress("RallyMain"), setne return. */
#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: load the chosen renderer DLL and find its RallyMain entry
 * point. Reports whether both succeeded -- this is the moment the launcher
 * commits to Glide or Direct3D. */
/* @implements 0x004017B0 brally.exe LoadRallyMain */

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
extern FARPROC gRallyMain;


int LoadRallyMain(char *dll)
{
    HMODULE h;

    h = LoadLibraryA(dll);
    gModule = h;
    if (h == 0)
        return 0;
    gRallyMain = GetProcAddress(h, "RallyMain");
    return gRallyMain != 0;
}


#endif /* BR_MATCHING_BUILD */
