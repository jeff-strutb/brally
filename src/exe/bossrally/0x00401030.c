/* 0x00401030 WndProc (__stdcall ret 16) */
#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: the intro player's window procedure -- routes every Windows
 * message: paint, keys, close, and the private message DirectShow uses to
 * report playback events. */
/* @implements 0x00401030 bossrally.exe WndProc */

#include <windows.h>

LRESULT HandleCommand(HWND hwnd, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_CHAR:
        if ((BYTE)wParam == 0x1B) {
            PostQuitMessage(0);
            return 0;
        }
        return 0;
    case WM_SYSKEYDOWN:
        return 0;
    case WM_COMMAND:
        return HandleCommand(hwnd, wParam, lParam);
    default:
        return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
}

#endif
