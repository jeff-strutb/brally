/* 0x00401100 RegisterWindowClass */
#ifdef BR_MATCHING_BUILD
/* @implements 0x00401100 bossrally.exe RegisterWindowClass */

#include <windows.h>

extern HINSTANCE gInstance;
extern char gClassName[];
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

int RegisterWindowClass(HINSTANCE hInst, HINSTANCE hPrev)
{
    WNDCLASS wc;

    gInstance = hInst;
    if (hPrev == 0) {
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WndProc;
        wc.cbClsExtra = 0;
        wc.cbWndExtra = 0;
        wc.hInstance = hInst;
        wc.hIcon = LoadIconA(hInst, MAKEINTRESOURCE(0x80));
        wc.hCursor = LoadCursorA(0, IDC_ARROW);
        wc.hbrBackground = GetStockObject(LTGRAY_BRUSH);
        wc.lpszMenuName = MAKEINTRESOURCE(0x80);
        wc.lpszClassName = gClassName;
        RegisterClassA(&wc);
    }
    return 1;
}

#endif
