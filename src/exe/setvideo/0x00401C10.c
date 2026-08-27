/* DlgProcOKCancel: switch(msg) sub 0x110 / dec, inner switch(LOWORD)
 * IDOK / IDCANCEL. Last inner case (IDCANCEL) EndDialog then falls
 * through to `return 0` — an explicit `return 0` there outlines it
 * with `je` + a 5-byte xor-ret (53 diffs). */
#ifdef BR_MATCHING_BUILD
/* @implements 0x00401C10 setvideo.exe DlgProcOKCancel */

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


int __stdcall DlgProcOKCancel(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_INITDIALOG:
        SetWindowLongA(hWnd, 8, lParam);
        return 1;
    case WM_COMMAND:
        switch (wParam & 0xffff) {
        case IDOK:
            EndDialog(hWnd, 1);
            return 0;
        case IDCANCEL:
            EndDialog(hWnd, 0);
        }
    }
    return 0;
}


#endif /* BR_MATCHING_BUILD */
