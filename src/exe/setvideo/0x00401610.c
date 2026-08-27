/* Auto-generated from disassembly — 0x00401610
 * CountSections: FindFirst + FindNext until index == -1. */
#ifdef BR_MATCHING_BUILD
/* @implements 0x00401610 setvideo.exe CountSections */

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
Section *FindFirstSection(INI *);
Section *FindNextSection(Section *);
void CHK_FreeMemory(void *);


int CountSections(INI *pini)
{
    int n;
    Section *p;

    n = 0;
    p = FindFirstSection(pini);
    if (p->index != -1) {
        do {
            n++;
            p = FindNextSection(p);
        } while (p->index != -1);
    }
    CHK_FreeMemory(p);
    return n;
}


#endif /* BR_MATCHING_BUILD */
