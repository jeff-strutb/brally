/* FillComboA: enumerate VDB sections whose name starts with "[v:", strip
 * the "[v:" prefix and trailing byte, CB_ADDSTRING / CB_SETITEMDATA(i) /
 * CB_SETCURSEL if Sel.index == i. Combo ctl 0x3E9. */
#ifdef BR_MATCHING_BUILD
/* @implements 0x00401F00 setvideo.exe FillComboA */

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
typedef struct Sel {
    int saved;
    int index;
    int method;
} Sel;
extern INI *gINI;
extern int gSectionCount;
Section *FindFirstSection(INI *);
Section *FindNextSection(Section *);
char *GetObj(Section *);
void CHK_FreeMemory(void *);


int FillComboA(HWND hWnd)
{
    Sel *sel;
    Section *psec;
    int i;
    char *name;
    int n;
    char buf[80];
    int item;

    sel = (Sel *)GetWindowLongA(hWnd, 8);
    if (sel == 0)
        return 0;
    psec = FindFirstSection(gINI);
    i = 0;
    if (gSectionCount > 0) {
        do {
            name = GetObj(psec);
            if (strncmp(name, "[v:", 3) == 0) {
                n = strlen(name + 1) - 1;
                if (n >= 0x4f)
                    n = 0x4f;
                strncpy(buf, name + 3, n);
                buf[n] = 0;
                item = SendDlgItemMessageA(hWnd, 0x3e9, CB_ADDSTRING, 0, (LPARAM)buf);
                SendDlgItemMessageA(hWnd, 0x3e9, CB_SETITEMDATA, item, i);
                if (sel->index == i)
                    SendDlgItemMessageA(hWnd, 0x3e9, CB_SETCURSEL, item, 0);
            }
            psec = FindNextSection(psec);
            i++;
        } while (i < gSectionCount);
    }
    CHK_FreeMemory(psec);
    return 1;
}


#endif /* BR_MATCHING_BUILD */
