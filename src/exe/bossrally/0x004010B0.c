/* 0x004010B0 InitCOM: copy "Player" into class name, CoInitialize(0). */
#ifdef BR_MATCHING_BUILD
/* @implements 0x004010B0 bossrally.exe InitCOM */

#include <windows.h>
#include <objbase.h>
#include <string.h>

extern char gClassName[];

int InitCOM(void)
{
    strcpy(gClassName, "Player");
    return CoInitialize(0) >= 0;
}

#endif
