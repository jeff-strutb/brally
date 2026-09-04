/* DlgProcComboB: chipset combo. Same as DlgProcComboA but FillComboB.
 * IDOK: merged EndDialog after if (idx >= 0) keeps ok live so EndDialog
 * IAT is ebp and prologue keeps 4 callee-saved. */
#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: handle the dialog built around the second drop-down. */
/* @implements 0x00402260 setvideo.exe DlgProcComboB */

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

typedef struct Sel {
    int saved;
    int index;
    int method;
} Sel;
int ComboGetItemData(HWND);
int FillComboB(HWND);


int __stdcall DlgProcComboB(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    Sel *sel;
    int idx;
    int ok;

    switch (msg) {
    case WM_INITDIALOG:
        SetWindowLongA(hWnd, 8, lParam);
        if (GetDlgItem(hWnd, 0x3e9) != 0) {
            if (FillComboB(hWnd) == 0)
                return 0;
        }
        return 1;
    case WM_COMMAND:
        switch (wParam & 0xffff) {
        case IDOK:
            sel = (Sel *)GetWindowLongA(hWnd, 8);
            if (sel == 0)
                EndDialog(hWnd, 0);
            ok = 0;
            idx = ComboGetItemData(hWnd);
            if (idx >= 0) {
                ok = 1;
                sel->index = idx;
            } else
                sel->index = sel->saved;
            EndDialog(hWnd, ok);
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
