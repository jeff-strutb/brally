/* 0x00401640 DeleteContents: Release graph, clear event/state. */
#ifdef BR_MATCHING_BUILD
/* @implements 0x00401640 bossrally.exe DeleteContents */

#include <windows.h>

typedef struct ComObj {
    struct {
        HRESULT (__stdcall *QueryInterface)(struct ComObj *, const IID *, void **);
        ULONG (__stdcall *AddRef)(struct ComObj *);
        ULONG (__stdcall *Release)(struct ComObj *);
    } *lpVtbl;
} ComObj;

extern ComObj *gGraph;
extern HANDLE gGraphEvent;
void SetMediaState(int s);

void DeleteContents(void)
{
    if (gGraph != 0) {
        gGraph->lpVtbl->Release(gGraph);
        gGraph = 0;
    }
    gGraphEvent = 0;
    SetMediaState(0);
}

#endif
