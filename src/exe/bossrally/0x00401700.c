/* 0x00401700 SetPlayerTitle: "Player" + " - " + file */
#ifdef BR_MATCHING_BUILD
/* @implements 0x00401700 bossrally.exe SetPlayerTitle */
/* @n64 0x80267470 located */

#include <windows.h>
#include <string.h>

void SetPlayerTitle(HWND hwnd, const char *file)
{
    char buf[0x214];

    strcpy(buf, "Player");
    strcat(buf, " - ");
    strcat(buf, file);
    SetWindowTextA(hwnd, buf);
}

#endif
