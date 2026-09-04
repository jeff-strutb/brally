/* DlgProcRadio: method picker. Last command case 0x3EA falls through. */
#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: handle a dialog whose choices are radio buttons, tracking
 * which one is selected. */
/* @implements 0x00401DC0 setvideo.exe DlgProcRadio */

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

extern int gPlusD;


int __stdcall DlgProcRadio(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_INITDIALOG:
        SetWindowLongA(hWnd, 8, lParam);
        switch (lParam) {
        case 2:
            CheckRadioButton(hWnd, 0x3eb, 0x3ed, 0x3ec);
            return 1;
        case 3:
            CheckRadioButton(hWnd, 0x3eb, 0x3ed, 0x3ed);
            return 1;
        }
        if (gPlusD == 0)
            CheckRadioButton(hWnd, 0x3eb, 0x3ed, 0x3ec);
        else
            CheckRadioButton(hWnd, 0x3eb, 0x3ed, 0x3eb);
        return 1;
    case WM_COMMAND:
        switch (wParam & 0xffff) {
        case IDOK:
            if (IsDlgButtonChecked(hWnd, 0x3ed) != 0)
                EndDialog(hWnd, 3);
            else if (IsDlgButtonChecked(hWnd, 0x3ec) != 0)
                EndDialog(hWnd, 2);
            else
                EndDialog(hWnd, 1);
            return 0;
        case IDCANCEL:
            EndDialog(hWnd, 0);
            return 0;
        case 0x3ea:
            EndDialog(hWnd, -1);
        }
    }
    return 0;
}


#endif /* BR_MATCHING_BUILD */
