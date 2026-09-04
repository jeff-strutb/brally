/* Auto-generated from disassembly — 0x00401000
 * FreeObjList: free each rgsz[i], then rgsz, then rgi. n==0 returns
 * without freeing the arrays. */
#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: free a parsed object list -- each string first, then the two
 * arrays. An empty list frees nothing, not even the arrays. */
/* @implements 0x00401000 setvideo.exe FreeObjList */

/* SetVideo.exe is /ML (static CRT): CRT calls are E8, not FF 15. */
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
typedef struct ObjList {
    int n;
    int *rgi;
    char **rgsz;
} ObjList;


void FreeObjList(ObjList *p)
{
    int i;

    if (p->n == 0)
        return;
    for (i = 0; i < p->n; i++)
        free(p->rgsz[i]);
    free(p->rgsz);
    free(p->rgi);
}


#endif /* BR_MATCHING_BUILD */
