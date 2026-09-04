/* FindNextSection: shared-fail `if (p != 0) { if (p->pini != 0)`;
 * i = p->index + 1 while pini is still live, then list = p->pini->list.
 * Indexed `list->rgsz[i][0]` (`while (i < n)`). Loading i after pini dies
 * colors pini into ecx (3 diffs). */
#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: advance a cursor to the next section, so the sections can be
 * enumerated. */
/* @implements 0x004015B0 setvideo.exe FindNextSection */

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


Section *FindNextSection(Section *p)
{
    int i;
    int n;
    ObjList *list;

    if (p != 0) {
        if (p->pini != 0) {
            i = p->index + 1;
            list = p->pini->list;
            n = list->n;
            while (i < n) {
                if (list->rgsz[i][0] == '[') {
                    p->index = i;
                    return p;
                }
                i++;
            }
            p->index = -1;
            return p;
        }
    }
    return 0;
}


#endif /* BR_MATCHING_BUILD */
