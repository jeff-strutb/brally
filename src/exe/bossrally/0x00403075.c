/* 0x00403075 _matherr: xor eax,eax; ret. Byte-identical to BRally 0x401DD0. */
#ifdef BR_MATCHING_BUILD
/* @implements 0x00403075 bossrally.exe _matherr */

#include <windows.h>

int _matherr(void *e)
{
    return 0;
}

#endif
