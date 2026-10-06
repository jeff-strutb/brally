/* 0x00401990 GetGraphEvent: return the IMediaEvent handle. */
/* WHAT IT DOES: fetch the next event DirectShow has queued for the player. */
/* @implements 0x00401990 bossrally.exe GetGraphEvent */

#include <windows.h>

extern HANDLE gGraphEvent;

HANDLE GetGraphEvent(void)
{
    return gGraphEvent;
}

