/* WinMain: GetInstallDir, load BossRally.vdb, parse an existing
 * BossRally.ini, run the Display Wizard dialogs, write BossRally.ini back.
 * ONE original function — the map splits at 0x402822 / 294F / 2AC0 / 2BCE
 * are mid-body block boundaries, not separate C functions.
 *
 * Idioms proven here (see docs/VC5-IDIOMS.md):
 *  - nested `plus = strstr(); gPlusD = plus != 0` is xor/setne; a bare
 *    `gPlusD = strstr(...) != 0` is neg/sbb/neg.
 *  - `char buf[0x400]` gives `sub esp,0x528` (the frame is `sub esp` THEN
 *    four pushes, so locals are read from post-push esp).
 *  - Card= walk is `for (i=0; i<n; i++)`; the inlined strcmp clobbers esi so
 *    orig reloads `card` at the continue point via `jmp +4`. do-while peels
 *    the body (+104 B).
 *  - Dialog templates: if/else on the DialogBoxParamA ASSIGNMENT, compare the
 *    result after. A ternary in the argument is neg/sbb; an `if (…==0) return`
 *    inside each arm duplicates the call.
 *  - THE RADIO lParam IS THE LOOP-CARRIED RESULT. `method` is seeded from
 *    gSel.method once, then reassigned by each radio DialogBoxParamA and
 *    passed back in as lParam next time round; every `goto radio` re-enters
 *    AFTER the seed. That is what puts it in ebx (`mov ebx,eax;
 *    lea eax,[ebx+1]`) — with a fresh `gSel.method` argument it is `inc eax`
 *    and 644 bytes of register cascade follow.
 *  - The vendor/chipset arms compare `gSel.method`, NOT the just-taken
 *    `vsave.method` copy: reading the copy lets VC5 keep the member in ebx
 *    and spill the loop variable instead (all three Sel fields must go to
 *    stack slots 0x18/0x1c/0x20).
 *  - The write blocks are `if (result != 0) { …; FreeINI(gINI); return 0; }`
 *    followed by a second `FreeINI(gINI); return 0;` — NOT an early-return
 *    guard. The guard form makes the exit the fall-through (`jne write`);
 *    the original branches away to it (`je <outlined stub>`) and cross-jumps
 *    the exits so that case 1's block is the merge master. */
#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: the whole Display Wizard. Works out where the game is
 * installed, loads the video-device database, pre-selects whatever card
 * the existing BossRally.ini names, then loops over the wizard dialogs
 * (OK/Cancel -> method radio -> symptoms | vendor combo | chipset combo,
 * with Back returning to the previous screen) and writes the chosen
 * card and its D3D settings back into BossRally.ini. */
/* @implements 0x00402480 setvideo.exe WinMain */

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
typedef struct CHKFile {
    FILE *fp;
    char *name;
} CHKFile;

void GetInstallDir(void);
int CHK_FileExists(char *);
INI *ReadINI(char *);
int CountSections(INI *);
CHKFile *CHK_FReadOpen(char *);
char *CHK_FGets(char *, int, CHKFile *);
void CHK_FClose(CHKFile *);
CHKFile *CHK_FWriteOpen(char *, char *);
void CHK_FPutS(char *, CHKFile *);
Section *SetSubstituteDir(INI *, char *);
char *GetIniValue(Section *, char *);
Section *FindFirstSection(INI *);
Section *FindNextSection(Section *);
char *GetObj(Section *);
void CHK_FreeMemory(void *);
void FreeINI(INI *);
char *GetSectionNameByIndex(int, INI *);
Section *FollowUse(INI *, char *);
INI *BindSection(Section *);
char *NextObj(INI *);

int __stdcall DlgProcOKCancel(HWND, UINT, WPARAM, LPARAM);
int __stdcall DlgProc(HWND, UINT, WPARAM, LPARAM);
int __stdcall DlgProcRadio(HWND, UINT, WPARAM, LPARAM);
int __stdcall DlgProcComboA(HWND, UINT, WPARAM, LPARAM);
int __stdcall DlgProcComboB(HWND, UINT, WPARAM, LPARAM);

extern char gInstallDir[];
extern char gIniPath[];
extern INI *gINI;
extern int gSectionCount;
extern Sel gSel;
extern int gPlusD;
extern int gD3DAlphaCompare;
extern int gD3DDrawCarShadow;
extern int gD3DInvSrcAlpha;
extern int gD3DClearZBuffer;


int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPSTR lpCmdLine, int nShow)
{
    HWND hwnd;
    char line[256];
    char buf[0x400];
    int i;
    INI *pini2;
    Sel vsave;
    Sel csave;
    char *card;
    Section *psec;
    CHKFile *fp;
    Section *pwalk;
    char *name;
    char *s;
    INI *bound;
    int result;
    int method;

    hwnd = GetDesktopWindow();
    GetInstallDir();
    strcpy(gIniPath, gInstallDir);
    strcat(gIniPath, "BossRally.ini");
    if (lpCmdLine != 0) {
        if (strlen(lpCmdLine) != 0) {
            char *plus;
            plus = strstr(lpCmdLine, "+d");
            gPlusD = plus != 0;
        }
    }
    if (CHK_FileExists("BossRally.vdb") == 0) {
        sprintf(buf, "Error: file %s is missing.", "BossRally.vdb");
        MessageBoxA(hwnd, buf, "Boss Rally Display Wizard", 0);
        return 0;
    }
    gINI = ReadINI("BossRally.vdb");
    gSectionCount = CountSections(gINI);
    gSel.index = -1;
    gSel.saved = -1;
    gSel.method = 0;
    if (CHK_FileExists(gIniPath) != 0) {
        fp = CHK_FReadOpen(gIniPath);
        while (CHK_FGets(line, 0x100, fp) != 0) {
            if (strncmp(line, "D3DDrawCarShadow=", 0x11) == 0)
                gD3DDrawCarShadow = atoi(line + 0x11);
            else if (strncmp(line, "D3DAlphaCompare=", 0x10) == 0)
                gD3DAlphaCompare = atoi(line + 0x10);
            else if (strncmp(line, "D3DClearZBuffer=", 0x10) == 0)
                gD3DClearZBuffer = atoi(line + 0x10);
            else if (strncmp(line, "D3DInvSrcAlpha=", 0xf) == 0)
                gD3DInvSrcAlpha = atoi(line + 0xf);
        }
        CHK_FClose(fp);
    }
    if (CHK_FileExists(gIniPath) != 0) {
        pini2 = ReadINI(gIniPath);
        psec = SetSubstituteDir(pini2, "[Video]");
        card = GetIniValue(psec, "Card");
        if (card != 0) {
            pwalk = FindFirstSection(gINI);
            for (i = 0; i < gSectionCount; i++) {
                name = GetObj(pwalk);
                if (strcmp(name, card) == 0) {
                    gSel.index = i;
                    gSel.saved = i;
                    if (strncmp(name, "[c:", 3) == 0)
                        gSel.method = 3;
                    else if (strncmp(name, "[v:", 3) == 0)
                        gSel.method = 2;
                    break;
                }
                pwalk = FindNextSection(pwalk);
            }
        }
        CHK_FreeMemory(psec);
        FreeINI(pini2);
    }

okcancel:
    if (gPlusD != 0)
        result = DialogBoxParamA(hInstance, MAKEINTRESOURCE(0x67),
                                 hwnd, DlgProcOKCancel, 0);
    else
        result = DialogBoxParamA(hInstance, MAKEINTRESOURCE(0x6c),
                                 hwnd, DlgProcOKCancel, 0);
    if (result == 0)
        return 0;
    method = gSel.method;
radio:
    if (gPlusD != 0)
        method = DialogBoxParamA(hInstance, MAKEINTRESOURCE(0x68),
                                 hwnd, DlgProcRadio, method);
    else
        method = DialogBoxParamA(hInstance, MAKEINTRESOURCE(0x6b),
                                 hwnd, DlgProcRadio, method);
    switch (method + 1) {
    case 0:
        goto okcancel;
    case 1:
        FreeINI(gINI);
        return 0;
    case 2:
        result = DialogBoxParamA(hInstance, MAKEINTRESOURCE(0x6a),
                                 hwnd, DlgProc, 0);
        if (result == -1)
            goto radio;
        if (result != 0) {
            fp = CHK_FWriteOpen(gIniPath, "wt");
            CHK_FPutS("[Video]", fp);
            CHK_FPutS("\n", fp);
            CHK_FPutS("Card", fp);
            CHK_FPutS("=", fp);
            CHK_FPutS("[Set via symptoms (use Direct3D)]", fp);
            CHK_FPutS("\n", fp);
            CHK_FPutS("Driver=D3D\n", fp);
            sprintf(buf, "D3DAlphaCompare=%d\n", gD3DAlphaCompare);
            CHK_FPutS(buf, fp);
            CHK_FPutS("D3DAlwaysSquareTextures=0\n", fp);
            sprintf(buf, "D3DClearZBuffer=%d\n", gD3DClearZBuffer);
            CHK_FPutS(buf, fp);
            sprintf(buf, "D3DDrawCarShadow=%d\n", gD3DDrawCarShadow);
            CHK_FPutS(buf, fp);
            CHK_FPutS("D3DWaitCanFlip=0\n", fp);
            CHK_FPutS("D3DWaitFlipDone=0\n", fp);
            sprintf(buf, "D3DInvSrcAlpha=%d\n", gD3DInvSrcAlpha);
            CHK_FPutS(buf, fp);
            CHK_FClose(fp);
            FreeINI(gINI);
            return 0;
        }
        FreeINI(gINI);
        return 0;
    case 3:
        vsave = gSel;
        if (gSel.method != 2) {
            gSel.method = 0;
            gSel.index = -1;
            gSel.saved = -1;
        }
        result = DialogBoxParamA(hInstance, MAKEINTRESOURCE(0x66),
                                 hwnd, DlgProcComboA, (LPARAM)&gSel);
        if (result == -1) {
            gSel = vsave;
            goto radio;
        }
        if (result != 0) {
            name = GetSectionNameByIndex(gSel.index, gINI);
            fp = CHK_FWriteOpen(gIniPath, "wt");
            CHK_FPutS("[Video]", fp);
            CHK_FPutS("\n", fp);
            CHK_FPutS("Card", fp);
            CHK_FPutS("=", fp);
            CHK_FPutS(name, fp);
            CHK_FPutS("\n", fp);
            pwalk = FollowUse(gINI, name);
            bound = BindSection(pwalk);
            if (bound != 0) {
                for (s = NextObj(bound); s != 0; s = NextObj(bound)) {
                    CHK_FPutS(s, fp);
                    CHK_FPutS("\n", fp);
                }
            }
            CHK_FreeMemory(pwalk);
            CHK_FClose(fp);
            FreeINI(gINI);
            return 0;
        }
        FreeINI(gINI);
        return 0;
    case 4:
        csave = gSel;
        if (gSel.method != 3) {
            gSel.method = 0;
            gSel.index = -1;
            gSel.saved = -1;
        }
        result = DialogBoxParamA(hInstance, MAKEINTRESOURCE(0x69),
                                 hwnd, DlgProcComboB, (LPARAM)&gSel);
        if (result == -1) {
            gSel = csave;
            goto radio;
        }
        if (result != 0) {
            name = GetSectionNameByIndex(gSel.index, gINI);
            fp = CHK_FWriteOpen(gIniPath, "wt");
            CHK_FPutS("[Video]", fp);
            CHK_FPutS("\n", fp);
            CHK_FPutS("Card", fp);
            CHK_FPutS("=", fp);
            CHK_FPutS(name, fp);
            CHK_FPutS("\n", fp);
            pwalk = FollowUse(gINI, name);
            bound = BindSection(pwalk);
            if (bound != 0) {
                for (s = NextObj(bound); s != 0; s = NextObj(bound)) {
                    CHK_FPutS(s, fp);
                    CHK_FPutS("\n", fp);
                }
            }
            CHK_FreeMemory(pwalk);
            CHK_FClose(fp);
            FreeINI(gINI);
            return 0;
        }
        FreeINI(gINI);
        return 0;
    }
    return 0;
}


#endif /* BR_MATCHING_BUILD */
