/* 0x00401B70 CRT_empty: 1 byte ret. Byte-identical to BRally 0x401DE0. */
#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: the user start-up hook the CRT calls; deliberately empty. */
/* @implements 0x00401B70 bossrally.exe CRT_empty */

#include <windows.h>

void CRT_empty(void)
{
}

#endif
