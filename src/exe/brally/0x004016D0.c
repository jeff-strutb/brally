/* Auto-generated from disassembly — 0x004016D0
 * GetInstallDir: HKLM\SOFTWARE\SouthPeak Interactive\Boss Rally\Directory.
 * Fallback "c:\\"; append '\\' if the value has none. */
#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: look the game's install directory up in the registry,
 * falling back to the drive root if it is not there, and make sure it ends
 * in a backslash so paths can be appended. */
/* @implements 0x004016D0 brally.exe GetInstallDir */

#define _CRTIMP __declspec(dllimport)
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef true
#define true 1
#define false 0
#endif

typedef int (*funcptr)();

/* Forward declarations for unknown functions/globals */
extern char gInstallDir[];
extern char s_regkey[];
extern char s_Directory[];
extern char s_c_drive[];
extern char s_backslash[];


void GetInstallDir(void)
{
    HKEY h;
    DWORD size;
    LONG r;

    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, s_regkey, 0, KEY_READ, &h) == 0) {
        size = 0x104;
        r = RegQueryValueExA(h, s_Directory, 0, 0, (BYTE *)gInstallDir, &size);
        RegCloseKey(h);
        if (r == 0) {
            if (gInstallDir[strlen(gInstallDir) - 1] != '\\')
                strcat(gInstallDir, s_backslash);
            return;
        }
    }
    strcpy(gInstallDir, s_c_drive);
}


#endif /* BR_MATCHING_BUILD */
