/* Auto-generated from disassembly — 0x00401720
 * FreeINI: FreeObjList(p->list); free(p). */
#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: release a parsed settings file -- its object list and then
 * the cursor itself. */
/* @implements 0x00401720 setvideo.exe FreeINI */

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
typedef struct INI {
    ObjList *list;
    int index;
} INI;
void FreeObjList(ObjList *);


void FreeINI(INI *p)
{
    FreeObjList(p->list);
    free(p);
}


#endif /* BR_MATCHING_BUILD */
