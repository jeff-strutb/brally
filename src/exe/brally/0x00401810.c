/* Auto-generated from disassembly — 0x00401810
 * WinMain: registry dir, optional SetVideo.exe, ReadINI [Video]/Driver,
 * LoadLibrary the renderer DLL, call RallyMain cdecl with the WinMain args.
 * Idiom: "BRD3D.dll" / "BRGlide.dll" must be extern char[] so strcpy uses
 * generic rep movs (a literal becomes a dword-move burst). */
#ifdef BR_MATCHING_BUILD
/* @implements 0x00401810 brally.exe WinMain */

#define _CRTIMP __declspec(dllimport)
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <process.h>

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
void GetInstallDir(void);
int CHK_FileExists(char *);
void AddSpawnArg(char *);
INI *ReadINI(char *);
Section *SetSubstituteDir(INI *, char *);
char *GetIniValue(Section *, char *);
void CHK_FreeMemory(void *);
void FreeINI(INI *);
int LoadRallyMain(char *);
int UnloadRallyMain(void);
extern char gInstallDir[];
extern char gIniPath[];
extern char *gArgv[];
extern int (__cdecl *gRallyMain)(HINSTANCE, HINSTANCE, LPSTR, int);
extern char s_BRD3D[];
extern char s_BRGlide[];


int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPSTR lpCmd, int nShow)
{
    HWND hwnd;
    char dllname[0x400];
    char err[0x400];
    INI *pini;
    Section *psec;
    char *driver;
    int r;

    hwnd = GetDesktopWindow();
    GetInstallDir();
    strcpy(gIniPath, gInstallDir);
    strcat(gIniPath, "BossRally.ini");
    if (CHK_FileExists(gIniPath) == 0) {
        AddSpawnArg("SetVideo.exe");
        if (_spawnv(0, "SetVideo.exe", gArgv) != 0) {
            MessageBoxA(hwnd, "User canceled SetVideo.exe", "Boss Rally", 0);
            return 0;
        }
    }
    dllname[0] = 0;
    pini = ReadINI(gIniPath);
    psec = SetSubstituteDir(pini, "[Video]");
    driver = GetIniValue(psec, "Driver");
    if (driver != 0) {
        if (strcmp(driver, "D3D") == 0)
            strcpy(dllname, s_BRD3D);
        else if (strcmp(driver, "Glide") == 0)
            strcpy(dllname, s_BRGlide);
        else
            strcpy(dllname, driver);
    }
    CHK_FreeMemory(psec);
    FreeINI(pini);
    if (dllname[0] != 0) {
        if (LoadRallyMain(dllname) == 0) {
            sprintf(err, "Error: failed to load %s.", dllname);
            MessageBoxA(hwnd, err, "Boss Rally", 0x10);
            exit(1);
        }
        r = gRallyMain(hInstance, hPrev, lpCmd, nShow);
        UnloadRallyMain();
        return r;
    }
    return 0;
}


#endif /* BR_MATCHING_BUILD */
