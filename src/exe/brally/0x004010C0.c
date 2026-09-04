/* Auto-generated from disassembly — 0x004010C0
 * CHK_AllocateMemory. size==0 returns without allocating (eax still 0). */
#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: allocate memory, treating a zero-byte request as 'nothing to
 * do' and returning null rather than calling the allocator. */
/* @implements 0x004010C0 brally.exe CHK_AllocateMemory */

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


void *CHK_AllocateMemory(unsigned int size, char *what)
{
    char buf[0x400];
    void *p;

    if (size == 0)
        return (void *)size;
    p = malloc(size);
    if (p == 0) {
        sprintf(buf, "CHK_AllocateMemory(): Out of memory: couldn't allocate %s\n", what);
        OutputDebugStringA(buf);
        exit(1);
    }
    return p;
}


#endif /* BR_MATCHING_BUILD */
