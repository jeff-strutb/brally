/* 0x00401590 SetFullScreen */
#ifdef BR_MATCHING_BUILD
/* @implements 0x00401590 bossrally.exe SetFullScreen */

#include <windows.h>
#include <objbase.h>

typedef struct ComObj ComObj;
typedef struct ComVtbl {
    HRESULT (__stdcall *QueryInterface)(ComObj *, const IID *, void **);
    ULONG (__stdcall *AddRef)(ComObj *);
    ULONG (__stdcall *Release)(ComObj *);
    void *slot[40];
} ComVtbl;
struct ComObj { ComVtbl *lpVtbl; };

extern ComObj *gGraph;
extern IID IID_IVideoWindow;

typedef HRESULT (__stdcall *GetLong_t)(ComObj *, long *);
typedef HRESULT (__stdcall *PutLong_t)(ComObj *, long);

int SetFullScreen(void)
{
    ComObj *pVW;
    HRESULT hr;
    long mode;

    hr = gGraph->lpVtbl->QueryInterface(gGraph, &IID_IVideoWindow, (void **)&pVW);
    if (hr < 0)
        return 0;
    hr = ((GetLong_t)pVW->lpVtbl->slot[32])(pVW, &mode);
    if (hr < 0) {
        pVW->lpVtbl->Release(pVW);
        return 0;
    }
    if (mode != -1) {
        hr = ((PutLong_t)pVW->lpVtbl->slot[41])(pVW, -1);
        if (hr < 0) {
            pVW->lpVtbl->Release(pVW);
            return 0;
        }
    }
    hr = ((PutLong_t)pVW->lpVtbl->slot[33])(pVW, -1);
    if (hr < 0) {
        pVW->lpVtbl->Release(pVW);
        return 0;
    }
    pVW->lpVtbl->Release(pVW);
    return 1;
}

#endif
