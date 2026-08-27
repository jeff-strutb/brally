/* 0x00401000 HandleCommand: ID_APP_EXIT (0xE141) posts quit. cdecl. */
#ifdef BR_MATCHING_BUILD
/* @implements 0x00401000 bossrally.exe HandleCommand */

#include <windows.h>

#define ID_APP_EXIT 0xE141

LRESULT HandleCommand(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    if (wParam != ID_APP_EXIT)
        return DefWindowProcA(hwnd, WM_COMMAND, wParam, lParam);
    PostQuitMessage(0);
    return 0;
}

#endif
