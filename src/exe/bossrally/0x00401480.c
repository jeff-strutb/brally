/* 0x00401480 InitMedia */
#ifdef BR_MATCHING_BUILD
/* @implements 0x00401480 bossrally.exe InitMedia */

#include <windows.h>

extern HANDLE gGraphEvent;
extern void *gGraph;
void SetMediaState(int s);

int InitMedia(void)
{
    SetMediaState(0);
    gGraphEvent = 0;
    gGraph = 0;
    return 1;
}

#endif
