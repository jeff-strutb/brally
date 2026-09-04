/* DlgProc: symptoms dialog. switch(msg)/switch(LOWORD); last command
 * case 0x3EA (Back) EndDialog(-1) falls through to return 0. */
#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: the settings tool's main dialog procedure -- routes every
 * message for the window the player actually sees. */
/* @implements 0x00401C70 setvideo.exe DlgProc */

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

extern int gD3DAlphaCompare;
extern int gD3DDrawCarShadow;
extern int gD3DInvSrcAlpha;
extern int gD3DClearZBuffer;


int __stdcall DlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_INITDIALOG:
        SetWindowLongA(hWnd, 8, lParam);
        if (gD3DAlphaCompare == 0)
            CheckDlgButton(hWnd, 0x3ee, 1);
        else
            CheckDlgButton(hWnd, 0x3ee, 0);
        if (gD3DDrawCarShadow == 0)
            CheckDlgButton(hWnd, 0x3ef, 1);
        else
            CheckDlgButton(hWnd, 0x3ef, 0);
        if (gD3DInvSrcAlpha != 0)
            CheckDlgButton(hWnd, 0x3f0, 1);
        else
            CheckDlgButton(hWnd, 0x3f0, 0);
        if (gD3DClearZBuffer != 0)
            CheckDlgButton(hWnd, 0x3f1, 1);
        else
            CheckDlgButton(hWnd, 0x3f1, 0);
        return 1;
    case WM_COMMAND:
        switch (wParam & 0xffff) {
        case IDOK:
            gD3DAlphaCompare = IsDlgButtonChecked(hWnd, 0x3ee) == 0;
            gD3DDrawCarShadow = IsDlgButtonChecked(hWnd, 0x3ef) == 0;
            gD3DInvSrcAlpha = IsDlgButtonChecked(hWnd, 0x3f0) != 0;
            gD3DClearZBuffer = IsDlgButtonChecked(hWnd, 0x3f1) != 0;
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
