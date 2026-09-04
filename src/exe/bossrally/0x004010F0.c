/* 0x004010F0 CoUninitialize IAT thunk: jmp [IAT]. 0-arg dllimport tail. */
#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: shut COM down again. A thunk so the atexit list can call it. */
/* @implements 0x004010F0 bossrally.exe CoUninitialize_thunk */

#include <windows.h>
#include <objbase.h>

void CoUninitialize_thunk(void)
{
    CoUninitialize();
}

#endif
