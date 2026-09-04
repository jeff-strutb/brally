/* FindFirstSection: SetSubstituteDir-shaped cursor pointing at first '['.
 * i = 0 while pini is still live, then list = pini->list. That interference
 * colors pini/list into edx and i into ecx (`mov edx,[edx]; xor ecx,ecx`).
 * `for (i = 0; i < list->n; i++)` starts i after pini dies and colors the
 * other way (4 diffs). */
#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: position a cursor on the first section of a settings file. */
/* @implements 0x00401560 setvideo.exe FindFirstSection */

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
void *CHK_AllocateMemory(unsigned int, char *);


Section *FindFirstSection(INI *pini)
{
    Section *p;
    int i;
    ObjList *list;

    p = (Section *)CHK_AllocateMemory(8, "SetSubstituteDir():pins");
    p->pini = pini;
    p->index = -1;
    if (pini != 0) {
        i = 0;
        list = pini->list;
        for (; i < list->n; i++) {
            if (list->rgsz[i][0] == '[') {
                p->index = i;
                break;
            }
        }
    }
    return p;
}


#endif /* BR_MATCHING_BUILD */
