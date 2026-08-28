/* FollowUse: walk Use= aliases. do { use = GetIniValue(p,"Use");
 * if (use) { free p; p = SetSubstituteDir(pini, use); } } while (use);
 * for(;;) + break is jmp back-edge and duplicates GetIniValue (+16). */
#ifdef BR_MATCHING_BUILD
/* @implements 0x00402360 setvideo.exe FollowUse */

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
Section *SetSubstituteDir(INI *, char *);
char *GetIniValue(Section *, char *);
void CHK_FreeMemory(void *);


Section *FollowUse(INI *pini, char *name)
{
    Section *p;
    char *use;

    p = SetSubstituteDir(pini, name);
    do {
        use = GetIniValue(p, "Use");
        if (use != 0) {
            CHK_FreeMemory(p);
            p = SetSubstituteDir(pini, use);
        }
    } while (use != 0);
    return p;
}


#endif /* BR_MATCHING_BUILD */
