/* Auto-generated from disassembly — 0x00401650
 * GetObj: current rgsz of a section cursor. Two-level deref. */
#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: return the string list the section cursor is currently
 * pointing at. */
/* @implements 0x00401650 setvideo.exe GetObj */

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
typedef struct Section {
    INI *pini;
    int index;
} Section;


char *GetObj(Section *p)
{
    if (p != 0) {
        if (p->pini != 0) {
            if (p->index != -1)
                return p->pini->list->rgsz[p->index];
        }
    }
    return 0;
}


#endif /* BR_MATCHING_BUILD */
