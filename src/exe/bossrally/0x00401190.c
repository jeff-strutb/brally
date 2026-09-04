/* 0x00401190 CreatePlayerWindow: "Player" + " - Untitled", tiny owner window. */
#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: create the window the video plays inside. */
/* @implements 0x00401190 bossrally.exe CreatePlayerWindow */

#include <windows.h>
#include <string.h>

extern char gClassName[];
extern HINSTANCE gInstance;
extern HWND gHwnd;

int CreatePlayerWindow(int nCmdShow)
{
    char szTitle[32];

    strcpy(szTitle, "Player");
    strcat(szTitle, " - Untitled");
    gHwnd = CreateWindowExA(0, gClassName, szTitle, 0x00CE0000,
        (int)0x80000000, (int)0x80000000, 0, 0x41,
        0, 0, gInstance, 0);
    UpdateWindow(gHwnd);
    return 1;
}

#endif
