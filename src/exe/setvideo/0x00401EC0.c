/* Auto-generated from disassembly — 0x00401EC0
 * ComboGetCurText: CB_GETCURSEL then CB_GETLBTEXT on item 0x3e9. */
#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: read the text of whatever is currently selected in a drop-
 * down. */
/* @implements 0x00401EC0 setvideo.exe ComboGetCurText */

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


int ComboGetCurText(HWND hWnd)
{
    int sel;

    sel = SendDlgItemMessageA(hWnd, 0x3e9, CB_GETCURSEL, 0, 0);
    if (sel != -1)
        return SendDlgItemMessageA(hWnd, 0x3e9, CB_GETITEMDATA, sel, 0);
    else
        return -1;
}


#endif /* BR_MATCHING_BUILD */
