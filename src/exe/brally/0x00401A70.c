/* Auto-generated from disassembly — 0x00401A70
 * AddSpawnArg: strcpy into gArgBuf[gArgOff], append to gArgv, NUL-terminate.
 * Idiom: dest is &gArgBuf[gArgOff] (scan s, then load gArgOff). gArgOff +=
 * strlen(s)+1 is spelled BEFORE gArgv[gArgc]=0 so the scasb-zero is hoisted
 * into the strcpy tail and reused for the NULL store. */
#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: append one argument to the list the launcher will pass on
 * when it spawns another program, keeping the list null-terminated. */
/* @implements 0x00401A70 brally.exe AddSpawnArg */

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
extern int gArgc;
extern int gArgOff;
extern char gArgBuf[];
extern char *gArgv[];


void AddSpawnArg(char *s)
{
    strcpy(&gArgBuf[gArgOff], s);
    gArgv[gArgc] = &gArgBuf[gArgOff];
    gArgc++;
    gArgOff += strlen(s) + 1;
    gArgv[gArgc] = 0;
}


#endif /* BR_MATCHING_BUILD */
