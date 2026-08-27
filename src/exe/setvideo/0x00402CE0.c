/* GetSectionNameByIndex: FindFirst, FindNext idx times, GetObj, free.
 * Called as f(index, pini) — pini is the second arg. */
#ifdef BR_MATCHING_BUILD
/* @implements 0x00402CE0 setvideo.exe GetSectionNameByIndex */

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
char *GetObj(Section *);
void CHK_FreeMemory(void *);


char *GetSectionNameByIndex(int idx, INI *pini)
{
    Section *p;
    char *s;

    p = FindFirstSection(pini);
    if (idx > 0) {
        do {
            p = FindNextSection(p);
            idx--;
        } while (idx != 0);
    }
    s = GetObj(p);
    CHK_FreeMemory(p);
    return s;
}


#endif /* BR_MATCHING_BUILD */
