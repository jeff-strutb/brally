/* 0x00401990 GetGraphEvent: return the IMediaEvent handle. */
#ifdef BR_MATCHING_BUILD
/* @implements 0x00401990 bossrally.exe GetGraphEvent */

#include <windows.h>

extern HANDLE gGraphEvent;

HANDLE GetGraphEvent(void)
{
    return gGraphEvent;
}

#endif
