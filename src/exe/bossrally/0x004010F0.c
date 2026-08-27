/* 0x004010F0 CoUninitialize IAT thunk: jmp [IAT]. 0-arg dllimport tail. */
#ifdef BR_MATCHING_BUILD
/* @implements 0x004010F0 bossrally.exe CoUninitialize_thunk */

#include <windows.h>
#include <objbase.h>

void CoUninitialize_thunk(void)
{
    CoUninitialize();
}

#endif
